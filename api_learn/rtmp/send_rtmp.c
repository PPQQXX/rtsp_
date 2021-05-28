#include <stdio.h>
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"

#define RTMP_ADDR "rtmp://127.0.0.1:1935/live/1234"

void fix_packet_pts(AVPacket *packet, AVRational timebase, int frame_index) {
    // 两帧之间的持续时间
    // r_frame_rate = (AVRational) {den , num}, 与time_base结构相反
    AVRational in_frame_rate = {
        timebase.num,
        timebase.den,
    };
    int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_frame_rate); //in_stream->r_frame_rate

    packet->pts = (double)(frame_index * calc_duration) / (double)(av_q2d(timebase)*AV_TIME_BASE);
    packet->dts = packet->pts;
    packet->duration = (double)calc_duration / (double)(av_q2d(timebase)*AV_TIME_BASE);
}



void send_rtmp(const char *file) {
    // 输入
    AVFormatContext *ifmt_ctx = NULL;
    if (avformat_open_input(&ifmt_ctx, file, 0, 0) < 0) {
        printf("failed to open input file\n");
        goto _Error;
    }

    if (avformat_find_stream_info(ifmt_ctx, 0) < 0) {
        printf("failed to find stream info\n");
        goto _Error;
    }

    int video_idx = -1; 
    video_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_idx < 0) {
        printf("failed to find stream_index\n");
        goto _Error;
    }
    av_dump_format(ifmt_ctx, 0, file, 0);
        
    
    // 输出
    AVFormatContext *ofmt_ctx = NULL;
    if (avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", RTMP_ADDR) < 0) {
        printf("failed to alloc output context\n");
        goto _Error;
    }
    int out_stm_idx = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) { 
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);

        out_stream->codecpar->codec_tag = 0;
        //if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        //    out_stream->codecpar->flags |= CODEC_FLAG_GLOBAL_HEADER;
        //}
    }
    av_dump_format(ofmt_ctx, 0, RTMP_ADDR, 1);

    // open output url
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, RTMP_ADDR, AVIO_FLAG_WRITE) < 0) {
            printf("failed to open output url:%s\n", RTMP_ADDR);
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
    while (1) {
        if (av_read_frame(ifmt_ctx, packet) < 0) break;
        
        // raw h264, doesn't contain pts
        // 我们实际推流的是flv文件，所以fix_packet_pts不会执行。
        if (packet->pts == AV_NOPTS_VALUE) {
            printf("fix packet pts\n");
            fix_packet_pts(packet, ifmt_ctx->streams[video_idx]->time_base, frame_index); 
        }
        // important delay
        if (packet->stream_index == video_idx) {
            AVRational timebase = ifmt_ctx->streams[video_idx]->time_base;
            AVRational timebase_q = {1, AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(packet->pts, timebase, timebase_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time) 
                av_usleep(pts_time - now_time);
        }
        
        // rescale time base
        av_packet_rescale_ts(packet, ifmt_ctx->streams[packet->stream_index]->time_base, \
                ofmt_ctx->streams[packet->stream_index]->time_base);
        packet->pos = -1;
        if (packet->stream_index == video_idx) {
            printf("send %6d video frames to output url\n", frame_index);
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
    send_rtmp(argv[1]); 
    return 0;
}


