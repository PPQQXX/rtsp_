#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

// 分离文件中的视频码流和音频码流，不转码
// 支持MPEG-TS文件
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

    while (av_read_frame(fmt_ctx, packet) == 0) {

        if (packet->stream_index == audio_index) {
            fwrite(packet->data, 1, packet->size, faudio);
        }
        else if (packet->stream_index == video_index) {
            fwrite(packet->data, 1, packet->size, fvideo);
        }
        av_packet_unref(packet);
    }

_Error:
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
    if (faudio)  fclose(faudio);
    if (fvideo)  fclose(fvideo);
    if (packet)  av_packet_free(&packet);
}


int main(int argc, char const* argv[])
{
    demuxer(argv[1]);
    return 0;
}
