#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

#define sourcefile "testvideo/output.aac"

int init_fmt_ctx(AVFormatContext **fmt_ctx, const char *file) {
    *fmt_ctx = avformat_alloc_context();
    if (*fmt_ctx == NULL) {
        printf("avformat_alloc failed.\n");
        return -1;
    } 
    if (avformat_open_input(fmt_ctx, file, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }

    if (avformat_find_stream_info(*fmt_ctx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // 转存有效信息到stderr
    av_dump_format(*fmt_ctx, 0, file, 0);
    return 0;
}


int main(int argc, char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *cod_ctx = NULL;
    AVCodec *cod = NULL; 
    AVPacket packet;

    // 1. 打开相关流，初始化fmt_ctx
    if (init_fmt_ctx(&fmt_ctx, sourcefile) < 0) 
        goto _Error;

    // 2. 找到合适的编解码器，返回第一帧数据索引
    int stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &cod, -1);
    if (cod == NULL) {
        printf("Couldn't find codec.\n");
        goto _Error;
    }
    // printf("stream_index:%d\n", stream_index);

    // 3. 获取编码时的参数
    AVCodecParameters *param = fmt_ctx->streams[stream_index]->codecpar;
    // 4. 设置解码上下文，并打开解码器
    cod_ctx = avcodec_alloc_context3(cod);
    avcodec_parameters_to_context(cod_ctx, param);
    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        printf("Couldn't open codec.\n");
        goto _Error;
    }
   

    
    // 4. 重采样初始化和前后参数设置
    
    /*********** 设置输出相关参数 ******************/
    int out_channels = param->channels;
    int out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = param->sample_rate;
    int out_nb_samples = param->frame_size;
    uint64_t out_channel_layout = param->channel_layout;

    // 初始化重采样上下文
    struct SwrContext *convert_ctx = swr_alloc();
    convert_ctx = swr_alloc_set_opts(convert_ctx, \
            out_channel_layout, out_sample_fmt, out_sample_rate, \  
            param->channel_layout, param->format, param->sample_rate,\
            0, NULL);
    swr_init(convert_ctx);


    // 4. 创建packet，存储编码数据
    av_init_packet(&packet);
    // 5. 创建Frame, 存储解码后的采样数据
    AVFrame *frame = av_frame_alloc();
    frame->channels = out_channels;          // 通道数不变
    frame->format = out_sample_fmt;          // 采样位深16bit
    frame->nb_samples = out_nb_samples;      // 单通道采样个数
    av_frame_get_buffer(frame, 0);



    // 7. 每次读取一帧编码数据，然后进行转码
    FILE *fout = fopen("output.pcm", "w+");
    uint8_t **data = (uint8_t **)av_calloc(1, sizeof(*data));
    int data_size = av_samples_alloc(data, NULL, \
            out_channels, out_nb_samples, out_sample_fmt, 0);
    
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
        if (packet.stream_index != stream_index) continue;

        if (avcodec_send_packet(cod_ctx, &packet) < 0) {
            printf("Decode error.\n");
            goto _Error;
        }
        while (avcodec_receive_frame(cod_ctx, frame) >= 0) {
            swr_convert(convert_ctx, data, data_size, \
                    (const uint8_t **)frame->data, frame->nb_samples);
            fwrite(data[0], 1, data_size, fout);
        }
        av_packet_unref(&packet);
    }

_Error:
    // 8. 关闭文件，释放资源    
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        avformat_free_context(fmt_ctx);
    } 
    if (cod_ctx) {
        avcodec_close(cod_ctx);
        avcodec_free_context(&cod_ctx);
    }
    if (convert_ctx) {
        swr_free(&convert_ctx);
    }
    if (fout) { 
        fclose(fout);
    }
    return 0;
}
