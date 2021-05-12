#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"


// 摄像头数据通过v4l2采集，然后使用x264进行编码
#define DEV "/dev/video0"
#define DEST_FMT AV_PIX_FMT_YUV420P

// 宽高由用户自定义，格式为使用最广泛的yuv420p
void capture_video(const char *out_file, int w, int h, int frame_num) {
    // 初始化格式上下文
    AVInputFormat *in_fmt = av_find_input_format("video4linux2");
    if (in_fmt == NULL) {
        printf("can't find_input_format\n");
        return ;
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, DEV, in_fmt, NULL) < 0) {
        printf("can't open_input_file\n");
        return ;
    }
    // printf device info
    av_dump_format(fmt_ctx, 0, DEV, 0);


    // 找到视频流的索引
    int videoindex = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i; break;
        }
    }
    if (videoindex == -1) {
        printf("can't find video stream index\n");
        avformat_close_input(&fmt_ctx);
        return ;
    }
    // 图像数据的编码上下文
    AVCodecContext *cod_ctx = fmt_ctx->streams[videoindex]->codec;


    /************************************************************/
    AVOutputFormat *out_fmt = av_guess_format(NULL, out_file, NULL);
    AVFormatContext *out_fmt_ctx = avformat_alloc_context();
    out_fmt_ctx->oformat = out_fmt;
    if (avio_open(&out_fmt_ctx->pb, out_file, AVIO_READ_WRITE) < 0) {
        printf("failed to open output file\n");
        return ;
    }

    // fmt_ctx->streams[videoindex]->codec有什么不同?
    AVStream *video_stream = avformat_new_stream(out_fmt_ctx, 0);
    if (video_stream == NULL) {
        printf("failed to create video stream\n");
        return ;
    }

    // 初始化编码器
    AVCodecContext *out_cod_ctx = video_stream->codec;
    out_cod_ctx->codec_id = out_fmt->video_codec;   // 编码器id
    out_cod_ctx->codec_type = AVMEDIA_TYPE_VIDEO;   // 编码类型
    out_cod_ctx->pix_fmt = AV_PIX_FMT_YUV420P;      // 图像格式
    out_cod_ctx->width = w;
    out_cod_ctx->height = h;
























    struct SwsContext *sws_ctx;
    // 输出图像的分辨率和格式
    sws_ctx = sws_getContext(cod_ctx->width, cod_ctx->height, cod_ctx->pix_fmt, \
            w, h, AV_PIX_FMT_YUV420P, \         
            SWS_BILINEAR, NULL, NULL, NULL);    // 缩放算法， 输入/输出图像的滤波器信息，缩放算法的参数

    // 读取一帧数据
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(*packet));
    av_read_frame(fmt_ctx, packet);  

    char *yuy2buf[4];
    int yuy2_linesize[4];
    // 按照指定的分辨率和图像格式以及指定的对齐方式，分析图像内存，返回申请的内存空间大小
    int yuy2_size = av_image_alloc(yuy2buf, yuy2_linesize, cod_ctx->width, cod_ctx->height, cod_ctx->pix_fmt, 1);

    char *yuv420pbuf[4];
    int yuv420p_linesize[4];
    int yuv420p_size = av_image_alloc(yuv420pbuf, yuv420p_linesize, w, h, AV_PIX_FMT_YUV420P, 1);
    
    memcpy(yuy2buf[0], packet->data, packet->size);
    sws_scale(sws_ctx, \
            (const uint8_t **)yuy2buf, yuy2_linesize, \
            0, cod_ctx->height, \      
            yuv420pbuf, yuv420p_linesize);

    fwrite(yuv420pbuf[0], 1, yuv420p_size, fp);










    av_free_packet(packet);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    av_freep(yuy2buf);
    av_freep(yuv420pbuf);
}

int main(int argc, char const* argv[])
{
    avdevice_register_all();
    get_one_frame("output.h264", 640, 480);
    return 0;
}
