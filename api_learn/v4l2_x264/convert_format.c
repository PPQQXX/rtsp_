#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"

// 摄像头数据通过v4l2采集，然后进行图像格式转化以及拉伸缩放
#define dev "/dev/video0"


// 宽高由用户自定义，格式为使用最广泛的yuv420p
void get_one_frame(const char *picfile, int w, int h) {
    // 初始化格式上下文
    AVInputFormat *in_fmt = av_find_input_format("video4linux2");
    if (in_fmt == NULL) {
        printf("can't find_input_format\n");
        return ;
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, dev, in_fmt, NULL) < 0) {
        printf("can't open_input_file\n");
        return ;
    }
    // printf device info
    av_dump_format(fmt_ctx, 0, dev, 0);


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

    AVCodecContext *cod_ctx = fmt_ctx->streams[videoindex]->codec;
    //printf("camera width:%d height:%d format:%d\n", cod_ctx->width, cod_ctx->height, cod_ctx->pix_fmt);
    //if (cod_ctx->pix_fmt == AV_PIX_FMT_YUYV422)
    //    printf("camera pixelfmt:%d\n", cod_ctx->pix_fmt);

    struct SwsContext *sws_ctx;
    // 输出图像的分辨率和格式
    sws_ctx = sws_getContext(cod_ctx->width, cod_ctx->height, cod_ctx->pix_fmt, \
            w, h, AV_PIX_FMT_YUV420P, \         
            SWS_BILINEAR, NULL, NULL, NULL);    // 缩放算法， 输入/输出图像的滤波器信息，缩放算法的参数

    // 读取一帧数据
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(*packet));
    av_read_frame(fmt_ctx, packet);  
    printf("packet->data:%d\n", packet->size);

    // YUY2的存储格式是packed的，[Y0,U0,Y1,V0]
    // packed格式的data[]数组中只有一维, align为1时，跨度s = width
    char *yuy2buf[4];
    int yuy2_linesize[4];
    // 按照指定的分辨率和图像格式以及指定的对齐方式，分析图像内存，返回申请的内存空间大小
    int yuy2_size = av_image_alloc(yuy2buf, yuy2_linesize, cod_ctx->width, cod_ctx->height, cod_ctx->pix_fmt, 1);
    printf("yuy2_size:%d linesize: {%d}\n", yuy2_size, yuy2_linesize[0]);

    // YUV420P的存储格式是planar的，[YYYY UU VV]
    // planar格式的data[]数组中有3维，linesize[3] = {s, s/2, s/2}
    char *yuv420pbuf[4];
    int yuv420p_linesize[4];
    int yuv420p_size = av_image_alloc(yuv420pbuf, yuv420p_linesize, w, h, AV_PIX_FMT_YUV420P, 1);
    printf("yuv420p size:%d linesize: {%d %d %d}\n", yuv420p_size, yuv420p_linesize[0], yuv420p_linesize[1], yuv420p_linesize[2]);
    
    FILE *fp = fopen(picfile, "w+");
    
    memcpy(yuy2buf[0], packet->data, packet->size);
    sws_scale(sws_ctx, \
            (const uint8_t **)yuy2buf, yuy2_linesize, \
            0, cod_ctx->height, \      
            yuv420pbuf, yuv420p_linesize);
    //printf("sws_scale() ok\n");

    fwrite(yuv420pbuf[0], 1, yuv420p_size, fp);


    fclose(fp);
    av_free_packet(packet);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    av_freep(yuy2buf);
    av_freep(yuv420pbuf);
}

int main(int argc, char const* argv[])
{
    avdevice_register_all();
    get_one_frame("output.yuv", 640, 480);
    return 0;
}
