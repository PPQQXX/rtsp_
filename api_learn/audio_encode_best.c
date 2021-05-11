#include <stdio.h>
#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

// 只对pcm进行编码，不需要重采样
void audio_encode(const char *pcmfile, const char *aacfile) {
    FILE *sfp = fopen(pcmfile, "r");
    if (sfp == NULL) {
        printf("fail to open pcmile\n");
        goto _Error;
    }
    FILE *dfp = fopen(aacfile, "w+");
    if (dfp == NULL) {
        printf("failed to open aacfile\n");
        goto _Error;
    }
    
    // 1. 初始化pcm_frame，保存每帧pcm数据
    AVFrame *pcm_frame;
    pcm_frame = av_frame_alloc();
    pcm_frame->format = AV_SAMPLE_FMT_S16;
    pcm_frame->channel_layout = AV_CH_LAYOUT_STEREO;
    pcm_frame->sample_rate = 44100;
    pcm_frame->channels = 2;
    // nb_samples的值与具体的编码协议有关
    pcm_frame->nb_samples = 2048;    // 一帧中有多少个采样点
    av_frame_get_buffer(pcm_frame, 0);
    
    // 2. 找到合适的编码器
    AVCodec *cod = avcodec_find_encoder_by_name("libfdk_aac");
    if (cod == NULL) {
        av_log(NULL, AV_LOG_ERROR, "fail to find codec\n");
        goto _Error;
    }
    
    // 3. 设置其编码选项
    AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
    cod_ctx->profile = FF_PROFILE_AAC_HE_V2;        // 编码协议
    cod_ctx->codec_type = AVMEDIA_TYPE_AUDIO;       // 音频编码
    cod_ctx->sample_fmt = pcm_frame->format;        
    cod_ctx->channel_layout = pcm_frame->channel_layout;
    cod_ctx->channels = pcm_frame->channels;
    cod_ctx->sample_rate = pcm_frame->sample_rate;  // 采样率


    // 4. 打开编码器
    if (avcodec_open2(cod_ctx, cod, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "fail to open codec\n");
        goto _Error;
    }
    printf("frame_size:%d\n", cod_ctx->frame_size); 

    // 5. 初始化packet
    int count = 0;
    AVPacket *packet = av_packet_alloc();
    int frame_size = pcm_frame->nb_samples * \
            av_get_bytes_per_sample(pcm_frame->format) * pcm_frame->channels; 

    while (1) {
        // AV_SAMPLE_FMT_S16是packed格式的, 声道数据LRLRLRLRLR...
        // 所以pcm_frame->data[]是一维的
        int ret = fread(pcm_frame->data[0], 1, frame_size, sfp);
        if (ret < 0) {
            printf("fail to read raw data\n");
            goto _Error;
        } else if (ret == 0) {
            break;
        }
        pcm_frame->pts = count;

        // 原始数据发送到编码器
        if (avcodec_send_frame(cod_ctx, pcm_frame) < 0) {
            printf("fail to send frame\n");
            goto _Error;
        }

        // 获取编码数据
        if (avcodec_receive_packet(cod_ctx, packet) >= 0) {
            fwrite(packet->data, 1, packet->size, dfp);
            count++;
            av_packet_unref(packet);
            printf("receive %d frame\n", count);
        }
    }

_Error:
    if (sfp) {
        fclose(sfp);
    }
    if (dfp) {
        fclose(dfp);
    }
    if (cod_ctx) {
        avcodec_close(cod_ctx);
        avcodec_free_context(&cod_ctx);
    }
    if (pcm_frame) {
        av_frame_free(&pcm_frame);
    }
    if (packet) {
        av_packet_free(&packet);
    }
}


int main(int argc, char const* argv[])
{
    audio_encode("output.pcm", "output.aac");
    return 0;
}
