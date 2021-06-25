#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "packet_queue.h"
#include "protocol.h"

typedef struct {
    uint8_t *buf[4];
    int linesize[4];
    int size;
} cnv_t;

typedef struct {
    AVFormatContext *fmt_ctx;   // 输入设备
    AVCodecContext *enc_ctx;    // 编码器上下文
    AVPacket *camera_packet;    // 从设备中读取图像
    AVFrame *yuvframe;          // 原始数据帧
    int frame_index;            // 对应帧中的pts
    void *convert_ctx;          // 转换上下文
    cnv_t cnv_src, cnv_dest;    // 转换前后
} AVInputDev;

struct myqueue *video_que;
void get_rawframe(AVInputDev *input);
int encodec_frame_to_packet(AVCodecContext *cod_ctx, AVFrame *frame, AVPacket *packet);
int VideoInput_Init(AVInputDev *input, int w, int h); 
void VideoInput_Destroy(AVInputDev *input); 

void *capture_video_thread(void *p) {
    // 采集输入初始化
    AVInputDev *input = malloc(sizeof(AVInputDev));
    VideoInput_Init(input, CAMERA_W, CAMERA_H);

    video_que = create_queue_buffer();
    queue_add_user(video_que, WRITER_ROLE);
    printf("start capture\n");
    while (1) {
        // 添加录制时间限制, 如果需要推流，误差较大
        if (av_compare_ts(input->yuvframe->pts, input->enc_ctx->time_base, \
                    MEDIA_DURATION, (AVRational){1, 1}) >= 0) {
            printf("capture %d s video finish.\n", MEDIA_DURATION);
            break;
        }

        get_rawframe(input);

        AVPacket *packet = av_packet_alloc();
        int ret = encodec_frame_to_packet(input->enc_ctx, input->yuvframe, packet);
        if (ret == EAGAIN) continue;
        else if (ret == EINVAL) break;
            
        //av_packet_rescale_ts(packet, input->enc_ctx->time_base, media.video_st->time_base);
        enqueue(video_que, packet);            
    }

    queue_del_user(video_que, WRITER_ROLE);
    destroy_queue_buffer(video_que);
    VideoInput_Destroy(input);
    pthread_exit(NULL);
}


int encodec_frame_to_packet(AVCodecContext *cod_ctx, AVFrame *frame, AVPacket *packet) {
    int send_ret = avcodec_send_frame(cod_ctx, frame);
    if (send_ret == AVERROR_EOF || send_ret == AVERROR(EAGAIN)) {
        return EAGAIN;
    } else if (send_ret < 0) {
        printf("failed to send frame to decoder\n");
        return EINVAL;
    }

    int receive_ret = avcodec_receive_packet(cod_ctx, packet);
    if (receive_ret == AVERROR_EOF || receive_ret == AVERROR(EAGAIN)) {
        return EAGAIN;
    } else if (receive_ret < 0) {
        printf("failed to receive frame frome decoder\n");
        return EINVAL;
    }
    return 0;
}

void get_rawframe(AVInputDev *input) {
    AVPacket *camera_packet = input->camera_packet;
    // 摄像头获取图像数据
    av_read_frame(input->fmt_ctx, camera_packet);  
    memcpy(input->cnv_src.buf[0], camera_packet->data, camera_packet->size);
    // 图像格式转化
    sws_scale((struct SwsContext *)input->convert_ctx,\
            (const uint8_t **)input->cnv_src.buf, input->cnv_src.linesize, \
            0, input->enc_ctx->height, input->cnv_dest.buf, input->cnv_dest.linesize);
    av_packet_unref(camera_packet); // 清理数据

    input->yuvframe->data[0] = input->cnv_dest.buf[0];
    input->yuvframe->data[1] = input->cnv_dest.buf[1];
    input->yuvframe->data[2] = input->cnv_dest.buf[2];
    input->frame_index++;
    input->yuvframe->pts = input->frame_index;
}


void convert_init(AVInputDev *input, int w, int h) {
    // 图像格式转化
    struct SwsContext *sws_ctx;
    sws_ctx = sws_getContext(w, h, CAMERA_FMT, \
            w, h, ENCODE_FMT, 0, NULL, NULL, NULL);

    // YUY2的存储格式是packed的，[Y0,U0,Y1,V0]
    // packed格式的data[]数组中只有一维, align为1时，跨度s = width
    cnv_t *yuy2 = &input->cnv_src;
    yuy2->size = av_image_alloc(yuy2->buf, yuy2->linesize, w, h, CAMERA_FMT, 1);
    

    // YUV420P的存储格式是planar的，[YYYY UU VV]
    // planar格式的data[]数组中有3维，linesize[3] = {s, s/2, s/2}
    cnv_t *iyuv = &input->cnv_dest;
    iyuv->size = av_image_alloc(iyuv->buf, iyuv->linesize, w, h, ENCODE_FMT, 1);

    input->convert_ctx = sws_ctx;
}

int VideoInput_Init(AVInputDev *input, int w, int h) {
    avdevice_register_all();
    AVInputFormat *in_fmt = av_find_input_format("v4l2");
    if (in_fmt == NULL) {
        printf("can't find_input_format\n");
        return -1;
    }

    // 设置摄像头的分辨率
    AVDictionary *option = NULL;
    char video_size[10];
    sprintf(video_size, "%dx%d", w, h);
    av_dict_set(&option, "video_size", video_size, 0);
    
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, CAMERA_DEV, in_fmt, &option) < 0) {
        printf("can't open_input_file\n");
        return -1;
    }
    av_dump_format(fmt_ctx, 0, CAMERA_DEV, 0);
   
    // 初始化编码器
    AVCodec *cod = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (cod == NULL) {
        printf("failed to find encoder\n");
        return -1;
    }

    AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
    cod_ctx->pix_fmt = ENCODE_FMT;
    cod_ctx->width = w;
    cod_ctx->height = h;
    cod_ctx->time_base.num = 1;
    cod_ctx->time_base.den = CAMERA_FPS;
    cod_ctx->bit_rate = 400000;
    cod_ctx->qmin = 10;
    cod_ctx->qmax = 51;
    cod_ctx->max_b_frames = 0;
    cod_ctx->thread_count = 4;
    cod_ctx->gop_size = 8;

    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        printf("failed to open encoder\n");
        return -1;
    }

    // 格式转换初始化
    convert_init(input, w, h);

    // 初始化frame，存放原始数据
    AVFrame *frame = av_frame_alloc();
    frame->width = w;
    frame->height = h;
    frame->format = ENCODE_FMT;
    av_frame_get_buffer(frame, 0);
    input->camera_packet = av_packet_alloc();
    input->fmt_ctx = fmt_ctx;
    input->enc_ctx = cod_ctx;
    input->yuvframe = frame; 
    input->frame_index = 0;
    return 0;
}


void VideoInput_Destroy(AVInputDev *input) {
    if (input->fmt_ctx) avformat_close_input(&input->fmt_ctx);
    if (input->camera_packet) av_packet_free(&input->camera_packet);
    if (input->yuvframe) av_frame_free(&input->yuvframe);
    if (input->convert_ctx) sws_freeContext(input->convert_ctx);
    if (input->cnv_src.buf) av_freep(input->cnv_src.buf);
    if (input->cnv_dest.buf) av_freep(input->cnv_dest.buf);
    free(input);
}


