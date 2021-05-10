#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

void yuv_to_h264(const char *sourcefile, int w, int h, const char *destfile) {
    FILE *sfp = fopen(sourcefile, "r");
    if (sfp == NULL) {
        printf("sourcefile fopen() failed\n");
        return ;
    }

    FILE *dfp = fopen(destfile, "w+");
    if (dfp == NULL) {
        printf("destfile fopen() failed\n");
        return ;
    }

    // 1. 设置编码器
    AVCodec *cod = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (cod == NULL) {
        printf("failed to find encoder\n");
        return ;
    }
    printf("encoder: %s\n", cod->name);
 
    // 2. 初始化编码器上下文
    AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
    cod_ctx->pix_fmt = AV_PIX_FMT_YUV420P;      
    cod_ctx->width = w;
    cod_ctx->height = h;
    cod_ctx->time_base.num = 1;     
    cod_ctx->time_base.den = 25;    // 帧率越高视频越流畅
    // 视频码率计算方式：视频文件大小/视频时间
    cod_ctx->bit_rate = 400000;      // 视频码率
    //cod_ctx->gop_size = 50;        // 表示每250帧插入一个I帧，I帧越少，视频越小，大小设置要合理。
    //av_opt_set_int(cod_ctx->priv_data, "qp", 18, 0); // 设置CQP恒定质量
    //av_opt_set_int(cod_ctx->priv_data, "crf", 18, 0); // 设置恒定速率因子
    cod_ctx->qmin = 10;             // 量化参数，设置默认值。量化系数越小，视频越清晰。
    cod_ctx->qmax = 51;             
    cod_ctx->max_b_frames = 0;       // B帧最大值，0表示不需要B帧
    cod_ctx->thread_count = 4;       // 线程数量

    // 3. 编码器初始化
    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        printf("failed to open encoder\n");
        return ;
    }

    // 初始frame
    AVFrame *frame = av_frame_alloc();
    frame->width = cod_ctx->width;
    frame->height = cod_ctx->height;
    frame->format = cod_ctx->pix_fmt;
    av_frame_get_buffer(frame, 0);

    // 初始化packet
    AVPacket *packet = av_packet_alloc();

    int y_size = frame->width*frame->height;
    int yuv_size = frame->width*frame->height*3/2;
    char *yuv_data = malloc(sizeof(char) * yuv_size);

    // 循环编码YUV文件为H264
    for (int i = 0; i < 1000; i++) {
        if (feof(sfp)) {
            printf("file read finish\n");
            break;
        }
        fread(yuv_data, 1, yuv_size, sfp);

        frame->data[0] = yuv_data;
        frame->data[1] = yuv_data + y_size;
        frame->data[2] = yuv_data + y_size*5/4;
        frame->pts = i;

        // 生成的YUV数据发送到编码器
        int send_result = avcodec_send_frame(cod_ctx, frame);
        
        // 编码器对原始数据进行编码
        while (send_result >= 0) {
            int receive_result = avcodec_receive_packet(cod_ctx, packet);
            // 没有处理完成的packet数据
            if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) break;
            
            fwrite(packet->data, 1, packet->size, dfp);
            // printf("receive %d packet\n", i);
            
            // 释放packet中的数据
            av_packet_unref(packet);
        }

    }

    // 释放内存资源
    fclose(sfp);
    fclose(dfp);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&cod_ctx);
}

int main(int argc, char const* argv[])
{
    yuv_to_h264("testvideo/output.yuv", 640, 272, "output.h264");
    return 0;
}
