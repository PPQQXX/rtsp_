#include <stdio.h>
#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

#define sourcefile "output.pcm"
#define outputfile "output.aac"

void flush_encoder(AVFormatContext *fmt_ctx, int stream_index) {
    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities & AV_CODEC_CAP_DELAY))
        return ;

    int got_frame = 0;
    AVPacket *packet = av_packet_alloc();
    while (1) {
        packet->data = NULL;
        packet->size = 0;
        av_init_packet(packet);
        int ret = avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec, \
                packet, NULL, &got_frame);
        if (ret < 0) break;
        if (got_frame == 0) break;
        ret = av_write_frame(fmt_ctx, packet);
        if (ret < 0) break;
    }
    av_packet_free(&packet);
}

int init_fmt_ctx(AVFormatContext **fmt_ctx, const char *file) {
    // 1. 推测输出文件格式
    AVOutputFormat *fmt = av_guess_format(NULL, file, NULL);
    if (fmt == NULL) {
        return -1;
    }
    
    // 2. 初始化输出文件的上下文，AVFormatContext
    *fmt_ctx = avformat_alloc_context();
    if (avformat_alloc_output_context2(fmt_ctx, fmt, \
                fmt->name, file) < 0) {
        return -1;
    }
    return 0;
}

int main(int argc, char const* argv[])
{
    AVFormatContext *ofmt_ctx;
    if (init_fmt_ctx(&ofmt_ctx, outputfile) < 0) {
        av_log(NULL, AV_LOG_ERROR, "fail to init format context\n");
        goto _Error;
    }

    // 3. 找到合适的编码器
    AVCodec *cod = avcodec_find_encoder_by_name("libfdk_aac");
    if (cod == NULL) {
        av_log(NULL, AV_LOG_ERROR, "fail to find codec\n");
        goto _Error;
    }
    
    // 4. 设置其编码选项
    AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
    cod_ctx->codec_type = AVMEDIA_TYPE_AUDIO;       // 音频编码
    cod_ctx->sample_fmt = AV_SAMPLE_FMT_S16;        
    cod_ctx->channel_layout = (cod_ctx->channels == 1)? \
                AV_CH_LAYOUT_MONO:AV_CH_LAYOUT_STEREO;
    cod_ctx->channels = av_get_channel_layout_nb_channels(cod_ctx->channel_layout);
    cod_ctx->sample_rate = 44100;                   // 采样率
    cod_ctx->profile = FF_PROFILE_AAC_HE_V2;        // 编码协议

    // 5. 设置输出流相关的编码参数
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, cod);
    avcodec_parameters_from_context(out_stream->codecpar, cod_ctx);
    if (out_stream == NULL) {
        av_log(NULL, AV_LOG_ERROR, "fail to create new stream.\n");
        goto _Error;
    }

    // 6. 打开编码器
    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "fail to open codec\n");
        goto _Error;
    }
    av_dump_format(ofmt_ctx, 0, outputfile, 1);

 
    // 7. 获取输出AAC文件的AVIOContext, 并写入aac文件头
    if (avio_open(&ofmt_ctx->pb, outputfile, AVIO_FLAG_WRITE) < 0) {    
        av_log(NULL, AV_LOG_ERROR, "fail to open outputfile\n");
        goto _Error;
    }
    
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "fail to write aac header\n");
        goto _Error;
    }

    // 8. 初始化AVFrame（存储原始数据）, 为frame中的data分配空间
    AVFrame *frame = av_frame_alloc();
    frame->channels = cod_ctx->channels;
    frame->format = cod_ctx->sample_fmt;
    frame->nb_samples = cod_ctx->frame_size;
    // 一帧原始数据大小
    int size = av_samples_get_buffer_size(NULL, \
            cod_ctx->channels, cod_ctx->frame_size, cod_ctx->sample_fmt, 1);
    uint8_t *pcmbuf = (uint8_t *)av_malloc(size);
    avcodec_fill_audio_frame(frame, frame->channels, frame->format, \
            (const uint8_t *)pcmbuf, size, 1);
    printf("pcmbuffer size:%d\n", size);

    // 9. 初始重采样上下文
    SwrContext *swr_ctx = swr_alloc();
    swr_alloc_set_opts(swr_ctx, cod_ctx->channel_layout, cod_ctx->sample_fmt, cod_ctx->sample_rate,\
            cod_ctx->channel_layout, cod_ctx->sample_fmt, 44100, 0, NULL);
    swr_init(swr_ctx);
        
    //uint8_t **data = (uint8_t **)av_calloc(cod_ctx->channels, sizeof(*data));
    //av_samples_alloc(data, NULL, cod_ctx->channels, cod_ctx->frame_size, cod_ctx->sample_fmt, 1); 
    uint8_t **data = (uint8_t **)malloc(cod_ctx->channels*sizeof(*data));
    // 单声道数据大小
    int data_size = cod_ctx->frame_size * av_get_bytes_per_sample(cod_ctx->sample_fmt);
    for (int i = 0; i < cod_ctx->channels; i++)
        data[i] = malloc(data_size);
    printf("datasize:%d\tchannels:%d\n", data_size, cod_ctx->channels);


    AVPacket *packet = av_packet_alloc();
    av_new_packet(packet, size);
    packet->data = NULL;
    packet->size = 0;
    

    FILE *fp = fopen(sourcefile, "r");
    if (fp == NULL) {
        printf("fail to open file\n");
        goto _Error;
    }

    int count = 1;
    frame->pts = 0;
    while (1) {
        size = frame->nb_samples * av_get_bytes_per_sample(frame->format) * frame->channels;
        int ret = fread(pcmbuf, 1, size, fp);
        if (ret < 0) {
            printf("fail to read raw data\n");
            goto _Error;
        } else if (ret == 0) {
            break;
        }

        // 对读取的采样数据进行重采样
        swr_convert(swr_ctx, data, cod_ctx->frame_size, frame->data, frame->nb_samples);
        memcpy(frame->data[0], data[0], data_size);
/*        
        if (data[1] == NULL)
            printf("data[1] is NULL\n");
        if (frame->data[1] == NULL)
            printf("frame->data[1] is NULL\n");
        memcpy(frame->data[1], data[1], data_size);
*/
        // 当前数据在AAC的位置
        frame->pts += data_size;

        if (avcodec_send_frame(cod_ctx, frame) < 0) {
            printf("fail to send frame\n");
            goto _Error;
        }

        // 获取编码数据
        if (avcodec_receive_packet(cod_ctx, packet) >= 0) {
            packet->stream_index = out_stream->index;
            av_log(NULL, AV_LOG_INFO, "write %d frame\n", count);
            av_write_frame(ofmt_ctx, packet);
        }
        count++;
        av_packet_unref(packet);
    }
    flush_encoder(ofmt_ctx, out_stream->index);

    av_write_trailer(ofmt_ctx);
    
_Error:
    if (fp) {
        fclose(fp);
    }
    if (ofmt_ctx) {
        if (ofmt_ctx->pb) {
            avio_close(ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
    }
    if (cod_ctx) {
        avcodec_close(cod_ctx);
        avcodec_free_context(&cod_ctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (packet) {
        av_packet_free(&packet);
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (pcmbuf) {
        av_free(pcmbuf);
    }

    return 0;
}
