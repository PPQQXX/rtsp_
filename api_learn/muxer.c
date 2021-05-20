#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

struct AVState {
    AVFormatContext *fmt_ctx;   // 格式上下文
    AVStream *stream;   // 码流
    int type;           // 码流类型
    int stream_index;   // 码流下标
};


int init_fmt_ctx(AVFormatContext **fmt_ctx, const char *file) {
    AVFormatContext *me = avformat_alloc_context();
    if (me == NULL) {
        printf("avformat_alloc failed.\n");
        return -1;
    } 
    if (avformat_open_input(&me, file, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(me, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    //av_dump_format(me, 0, file, 0);
    *fmt_ctx = me;
    return 0;
}


int AVState_Init(struct AVState **state, int type, const char *file) {
    struct AVState *me = malloc(sizeof(*me));

    if (init_fmt_ctx(&me->fmt_ctx, file) < 0) {
        printf("failed to init audio_fmt_ctx\n");
        return -1;
    }

    int index = av_find_best_stream(me->fmt_ctx, type, -1, -1, NULL, 0);
    if (index < 0) {
        printf("failed to find stream_index\n");
        return -1;
    }
    me->stream_index = index; 
    
    me->type = type;
    me->stream = me->fmt_ctx->streams[me->stream_index];
    *state = me;
    return 0;
}

void AVState_Destroy(struct AVState *state) {
    if (state->fmt_ctx) {
        avformat_close_input(&state->fmt_ctx);
        avformat_free_context(state->fmt_ctx);
    }
    free(state);
}


void muxer(const char *mp4file, const char *h264file, const char *aacfile) {
    // 初始输入码流状态结构体
    struct AVState *audio, *video;
    if (AVState_Init(&audio, AVMEDIA_TYPE_AUDIO, aacfile) < 0) {
        goto _Error;
    }
    if (AVState_Init(&video, AVMEDIA_TYPE_VIDEO, h264file) < 0) {
        goto _Error;
    }

    // 初始化mp4的格式上下文
    AVFormatContext *fmt_ctx;
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, mp4file);
    if (fmt_ctx == NULL) {
        printf("failed to alloc output fmt_ctx\n");
        goto _Error;
    }

    // 设置输出流
    AVStream *out_stream_audio = avformat_new_stream(fmt_ctx, NULL);
    AVStream *out_stream_video = avformat_new_stream(fmt_ctx, NULL);
    avcodec_parameters_copy(out_stream_audio->codecpar, audio->stream->codecpar);
    avcodec_parameters_copy(out_stream_video->codecpar, video->stream->codecpar);
    int out_audio_index = out_stream_audio->index;
    int out_video_index = out_stream_video->index;
    printf("out_audio_index:%d out_video_index:%d\n", out_audio_index, out_video_index);
    av_dump_format(fmt_ctx, 0, mp4file, 1);

    
    // 打开输出文件io
    if (! (fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, mp4file, AVIO_FLAG_WRITE) < 0) {
            printf("failed to open output file\n");
            goto _Error;
        }
    }

    // 写文件头
    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        printf("failed to write header\n");
        goto _Error;
    }
    AVPacket *packet = av_packet_alloc();

    // 音频编码数据和视频编码数据合并到MP4文件
    int frame_index = 0;
    int64_t cur_video_pts = 0, cur_audio_pts = 0;
    while (1) {
        struct AVState *inputstate = NULL;  
        AVStream *out_stream = NULL;
        int out_index = -1;
        
        //比较时间戳，判断当前应该写什么帧
        if (av_compare_ts(cur_video_pts, video->stream->time_base, \
                    cur_audio_pts, audio->stream->time_base) < 0) {
            inputstate = video;
            out_index = out_video_index;
        } else {
            inputstate = audio;
            out_index = out_audio_index;
        }
        out_stream = fmt_ctx->streams[out_index];
        
        // 从输入流读取编码数据
        if (av_read_frame(inputstate->fmt_ctx, packet) < 0) {
            break;
        }
       
        // 如果该帧没有pts，需要补齐
        if (packet->pts == AV_NOPTS_VALUE) {
            AVRational timebase = inputstate->stream->time_base;
            // 两帧之间的持续时间
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(inputstate->stream->r_frame_rate);
            
            packet->pts = (double)(frame_index * calc_duration) / (double)(av_q2d(timebase)*AV_TIME_BASE);
            packet->dts = packet->pts;
            packet->duration = (double)calc_duration / (double)(av_q2d(timebase)*AV_TIME_BASE);
            frame_index++;
        } 

        // 记录pts
        if (out_index == out_video_index) cur_video_pts = packet->pts;
        else cur_audio_pts = packet->pts;

        // 更新PTS/DTS
        av_packet_rescale_ts(packet, inputstate->stream->time_base, out_stream->time_base);
        // av_packet_rescale_ts与下面三行有相同效果
        /*
        packet->pts = av_rescale_q_rnd(packet->pts, inputstate->stream->time_base, \
                out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        packet->dts = av_rescale_q_rnd(packet->dts, inputstate->stream->time_base, \
                out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        packet->duration = av_rescale_q(packet->dts, inputstate->stream->time_base, out_stream->time_base);
        */
        packet->pos = -1;
        packet->stream_index = out_index;

        // 写文件
        if (av_interleaved_write_frame(fmt_ctx, packet) < 0) {
            printf("failed to muxing packet\n");
            break;
        } 

        av_packet_unref(packet);
    }

    // 写文件尾
    av_write_trailer(fmt_ctx);

_Error:
    if (audio) AVState_Destroy(audio);
    if (video) AVState_Destroy(video);
    if (fmt_ctx->pb) avio_close(fmt_ctx->pb);
    if (fmt_ctx) avformat_free_context(fmt_ctx);
    if (packet)  av_packet_free(&packet);
}


int main(int argc, char const* argv[])
{
    muxer(argv[1], argv[2], argv[3]);
    return 0;
}
