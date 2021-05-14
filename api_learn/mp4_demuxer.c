#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

// 分离文件中的视频码流和音频码流，不转码
// 支持MPEG-TS文件, 不支持MP4文件，因为mp4中的码流缺少起始码或adts头
// 对mp4文件分离出来的裸流进行处理

char *adts_header_gen(int len) {
    static char header[7];

    int profile = 2;    // AAC LC
    int freqidx = 4;    // 3 - 48k, 4 - 44.1k 
    int chncfg  = 2;    // 声道数量 

    header[0] = 0xFF;
    header[1] = 0xF1;
    header[2] = ((profile-1) << 6) | (freqidx << 2) | (chncfg >> 2);
    header[3] = ((chncfg & 3) << 6)| (len >> 11);
    header[4] = (len & 0x7FF) >> 3;
    header[5] = ((len & 0x7) << 5) | 0x1F;
    header[6] = 0xFC;
    return header;
}


void demuxer(const char *url) {
    // 初始化格式上下文
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == NULL) {
        printf("failed to alloc format context\n");
        goto _Error;
    }

    // 打开输入流
    if (avformat_open_input(&fmt_ctx, url, NULL, NULL) < 0) {
        printf("failed to open input url\n");
        goto _Error;
    }

    // 读取媒体文件信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        printf("failed to find stream\n");
        goto _Error;
    }
    av_dump_format(fmt_ctx, 0, url, 0);


    // 寻找音频流和视频流下标
    int video_index = -1, audio_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
        } else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
        }
    }
    if (video_index < 0 || audio_index < 0) {
        printf("failed to find stream index\n");
        goto _Error;
    }

    // 由打印的视频文件信息，保存不同的码流文件
    char audiofile[128], videofile[128];
    printf("type the name of output audiofile:");
    scanf("%s", audiofile);
    printf("\ntype the name of output videofile:");
    scanf("%s", videofile);
    FILE *faudio = fopen(audiofile, "w+");
    FILE *fvideo = fopen(videofile, "w+");
    AVPacket *packet = av_packet_alloc();

    // 初始化一个过滤器，对mp4的h264码流（没有起始码）进行处理
    const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
    if (bsf == NULL) {
        printf("failed to find stream filter\n");
        goto _Error;
    }
    AVBSFContext *bsf_ctx;
    av_bsf_alloc(bsf, &bsf_ctx);
    avcodec_parameters_copy(bsf_ctx->par_in, fmt_ctx->streams[video_index]->codecpar);
    av_bsf_init(bsf_ctx);


    // 接收mp4中的码流，并进行处理
    while (av_read_frame(fmt_ctx, packet) == 0) {

        if (packet->stream_index == audio_index) {
            // packet->size是adts中数据块的长度
            fwrite(adts_header_gen(packet->size+7), 1, 7, faudio);
            fwrite(packet->data, 1, packet->size, faudio);
        }
        else if (packet->stream_index == video_index) {
            if (av_bsf_send_packet(bsf_ctx, packet) == 0) {
                while (av_bsf_receive_packet(bsf_ctx, packet) == 0) {
                    fwrite(packet->data, 1, packet->size, fvideo);
                }
            }
        }
        av_packet_unref(packet);
    }

_Error:
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
    if (faudio)  fclose(faudio);
    if (fvideo)  fclose(fvideo);
    if (bsf_ctx) av_bsf_free(&bsf_ctx);
    if (packet)  av_packet_free(&packet);
}


int main(int argc, char const* argv[])
{
    demuxer(argv[1]);
    return 0;
}
