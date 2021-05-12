#include <stdio.h>
#include <sys/time.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"

#define CAMERA_DEV "/dev/video0"
#define CAMERA_FMT AV_PIX_FMT_YUYV422
#define ENCODE_FMT AV_PIX_FMT_YUV420P


float get_diff_time(struct timeval *tim, int update) {
    float dt = 0.0;
    struct timeval now;
    gettimeofday(&now, NULL);
    dt = (float)(now.tv_sec - tim->tv_sec);
    dt += (float)(now.tv_usec - tim->tv_usec)*1e-6;

    if (update == 1) {
        tim->tv_sec = now.tv_sec;
        tim->tv_usec = now.tv_usec;
    }
    return dt;
}

void get_video(const char *out_file, int w, int h, int nb_frames) {
    // 初始化格式上下文
    AVInputFormat *in_fmt = av_find_input_format("video4linux2");
    if (in_fmt == NULL) {
        printf("can't find_input_format\n");
        return ;
    }

    // 设置摄像头的分辨率
    AVDictionary *option = NULL;
    char video_size[10];
    sprintf(video_size, "%dx%d", w, h);
    av_dict_set(&option, "video_size", video_size, 0);
    
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, CAMERA_DEV, in_fmt, &option) < 0) {
        printf("can't open_input_file\n");
        return ;
    }
    // printf device info
    av_dump_format(fmt_ctx, 0, CAMERA_DEV, 0);


    // 初始化编码器
    AVCodec *cod = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (cod == NULL) {
        printf("failed to find encoder\n");
        return ;
    }

    AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
    cod_ctx->pix_fmt = ENCODE_FMT;
    cod_ctx->width = w;
    cod_ctx->height = h;
    cod_ctx->time_base.num = 1;
    cod_ctx->time_base.den = 30;
    cod_ctx->bit_rate = 400000;
    cod_ctx->qmin = 10;
    cod_ctx->qmax = 51;
    cod_ctx->max_b_frames = 0;
    cod_ctx->thread_count = 4;
    cod_ctx->gop_size = 15;

    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        printf("failed to open encoder\n");
        return ;
    }

    struct SwsContext *sws_ctx;
    // 图像格式转换：CAMERA_FMT --> ENCODE_FMT
    sws_ctx = sws_getContext(w, h, CAMERA_FMT, \
            w, h, ENCODE_FMT, 0, NULL, NULL, NULL);

    // YUY2的存储格式是packed的，[Y0,U0,Y1,V0]
    // packed格式的data[]数组中只有一维, align为1时，跨度s = width
    uint8_t *yuy2buf[4];
    int yuy2_linesize[4];
    int yuy2_size = av_image_alloc(yuy2buf, yuy2_linesize, w, h, CAMERA_FMT, 1);

    // YUV420P的存储格式是planar的，[YYYY UU VV]
    // planar格式的data[]数组中有3维，linesize[3] = {s, s/2, s/2}
    uint8_t *yuv420pbuf[4];
    int yuv420p_linesize[4];
    int yuv420p_size = av_image_alloc(yuv420pbuf, yuv420p_linesize, w, h, ENCODE_FMT, 1);
    
    FILE *fp = fopen(out_file, "w+");
    // 初始化packet，存放编码数据
    AVPacket *camera_packet = av_packet_alloc();
    AVPacket *packet = av_packet_alloc();
    
    // 初始化frame，存放原始数据
    AVFrame *frame = av_frame_alloc();
    frame->width = w;
    frame->height = h;
    frame->format = ENCODE_FMT;
    av_frame_get_buffer(frame, 0);



    int y_size = w*h;
    struct timeval timer;
    float tim_start = get_diff_time(&timer, 1);
    for (int i = 0; i < nb_frames; i++) {
        // 摄像头获取图像数据
        av_read_frame(fmt_ctx, camera_packet);  
        memcpy(yuy2buf[0], camera_packet->data, camera_packet->size);
        // 图像格式转化
        sws_scale(sws_ctx, (const uint8_t **)yuy2buf, yuy2_linesize, \
                0, h, yuv420pbuf, yuv420p_linesize);
        av_packet_unref(camera_packet); // 清理数据

        frame->data[0] = yuv420pbuf[0];
        frame->data[1] = yuv420pbuf[1];
        frame->data[2] = yuv420pbuf[2];
        frame->pts = i;
            
        // 图像数据发送到编码器
        int send_result = avcodec_send_frame(cod_ctx, frame);
        if (send_result < 0) {
            printf("send failed:%d.\n", send_result);
        }
        while (send_result >= 0) {
            // 编码器对图像数据进行编码
            int receive_result = avcodec_receive_packet(cod_ctx, packet);
            if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF)
                break;
            fwrite(packet->data, 1, packet->size, fp);
            // printf("get %d frames\n", i);
            av_packet_unref(packet);
        }
    }
    printf("recording time:%f\n", get_diff_time(&timer, 0));

    fclose(fp);
    av_packet_free(&camera_packet);
    av_packet_free(&packet);
    av_frame_free(&frame);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    av_freep(yuy2buf);
    av_freep(yuv420pbuf);
}

int main(int argc, char const* argv[])
{
    avdevice_register_all();
    get_video("output.h264", 640, 480, 500);
    return 0;
}
