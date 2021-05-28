#include <stdio.h>
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"


// 拉流，保存为out.flv文件
#define RTMP_ADDR "rtmp://127.0.0.1:1935/live/1234"

void receive_rtmp(const char *out_file) {
    // 输入rtmp url
    AVFormatContext *ifmt_ctx = NULL;
    if (avformat_open_input(&ifmt_ctx, RTMP_ADDR, 0, 0) < 0) {
        printf("failed to open input file\n");
        goto _Error;
    }
    printf("open input\n");

    if (avformat_find_stream_info(ifmt_ctx, 0) < 0) {
        printf("failed to find stream info\n");
        goto _Error;
    }
    printf("find stream info\n");

    int video_idx = -1; 
    video_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        printf("failed to find stream_index\n");
        goto _Error;
    }
    av_dump_format(ifmt_ctx, 0, RTMP_ADDR, 0);
        
    
    // 输出 xxx.flv文件
    AVFormatContext *ofmt_ctx = NULL;
    if (avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_file) < 0) {
        printf("failed to alloc output context\n");
        goto _Error;
    }
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) { 
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);

        out_stream->codecpar->codec_tag = 0;
        //if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        //    out_stream->codecpar->flags |= CODEC_FLAG_GLOBAL_HEADER;
        //}
    }
    av_dump_format(ofmt_ctx, 0, out_file, 1);


    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_file, AVIO_FLAG_WRITE) < 0) {
            printf("failed to open out_file:%s\n", RTMP_ADDR);
            goto _Error;
        }
    }
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        printf("failed to write header\n");
        goto _Error;
    }


    int frame_index = 0;
    int64_t start_time = av_gettime();
    AVPacket *packet = av_packet_alloc();

    //ifmt_ctx->flags |= AVFMT_FLAG_NONBLOCK; //无效
    while (1) {
        if (av_read_frame(ifmt_ctx, packet) < 0) break;
        
        // rescale time base
        av_packet_rescale_ts(packet, ifmt_ctx->streams[packet->stream_index]->time_base, \
                ofmt_ctx->streams[packet->stream_index]->time_base);
        packet->pos = -1;
        if (packet->stream_index == video_idx) {
            printf("receive %6d video frames\n", frame_index);
            frame_index++;
        }

        if (av_interleaved_write_frame(ofmt_ctx, packet) < 0) {
            printf("failed to write frame to url\n");
            break;
        }

        av_packet_unref(packet);
    }
    
    av_write_trailer(ofmt_ctx);

_Error:
    if (ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) 
            avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }
    if (packet) av_packet_free(&packet);
}


int main(int argc, char const* argv[])
{
    receive_rtmp(argv[1]); 
    return 0;
}


