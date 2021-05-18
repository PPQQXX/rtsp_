#include <string.h>
#include "stream.h"

int demuxer_state = 0;
int demuxer_thread(void *p) {
    struct MediaState *media = (struct MediaState *)p;
    struct VideoState *video = media->video_state;
    struct AudioState *audio = media->audio_state;

    // 读取一帧编码数据
    while (1) {
        HANDLE_EVENT(media->event.type);
        
        AVPacket *packet = av_packet_alloc();
        if (av_read_frame(media->fmt_ctx, packet) != 0) break;

        /*************** 处理码流 *******************/
        if (packet->stream_index == video->stream_state->stream_index) {
            // 过滤器处理视频码流
            if (av_bsf_send_packet(video->bsf_ctx, packet) != 0) {
                printf("failed to send packet to bitstream filter\n");
                break;
            }
            if (av_bsf_receive_packet(video->bsf_ctx, packet) != 0) {
                printf("failed to receive packet from bitstream filter\n");
                break;
            }
            // 将码流数据放到队列中管理
            enqueue(video->stream_state->que, packet);
        } else if (packet->stream_index == audio->stream_state->stream_index) {
            // 添加ADTS头
            AVPacket *tmp = av_packet_alloc();
            tmp->size = packet->size + 7;;
            tmp->data = malloc(tmp->size);
            memcpy(tmp->data, adts_header_gen(tmp->size), 7);
            memcpy(tmp->data + 7, packet->data, packet->size);
            
            av_packet_free(&packet);
            enqueue(audio->stream_state->que, tmp);
        } else {
            av_packet_unref(packet);
            continue; 
        }
    }
    // 解封装已结束
    demuxer_state = 1;
    printf("demuxer_thread end\n");
}



int StreamState_Init(struct StreamState **stream, int stream_type, AVFormatContext *fmt_ctx) {
    // 找到对应码流
    int index = av_find_best_stream(fmt_ctx, stream_type, -1, -1, NULL, 0);
    if (index < 0) return -1;

    // 初始化解码器上下文
    struct StreamState *me = malloc(sizeof(*me));
    me->stream_index = index; 
    me->cod_ctx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(me->cod_ctx, fmt_ctx->streams[index]->codecpar);
    
    // 找到并打开解码器
    me->cod = avcodec_find_decoder(me->cod_ctx->codec_id);
    if (me->cod == NULL) return -1;
    if (avcodec_open2(me->cod_ctx, me->cod, NULL) < 0) return -1;
    //printf("codec name: %s\n", me->cod->name);
    
    // 获取帧率，计算播放延时
    me->fps = av_q2d(fmt_ctx->streams[me->stream_index]->avg_frame_rate);
   
    // 码流队列
    me->que = create_queue_buffer();
    if (me->que == NULL) {
        printf("failed to create queue\n");
        return -1;
    }

    *stream = me;
    printf("Init StreamState Succes\n");
    return 0;
}

int MediaState_Init(struct MediaState **media, const char *url) {
    // 初始化格式上下文
    struct MediaState *me = malloc(sizeof(*me));
    me->fmt_ctx = avformat_alloc_context();
    if (avformat_open_input(&me->fmt_ctx, url, NULL, NULL) < 0) {
        printf("failed to open input file\n");
        return -1;
    }
    // 打印输入流信息
    avformat_find_stream_info(me->fmt_ctx, NULL);
    av_dump_format(me->fmt_ctx, 0, url, 0);
    
    // 初始化视频播放相关的结构
    if (VideoState_Init(&me->video_state, me->fmt_ctx) < 0) {
        printf("failed to init video state\n");
        return -1;
    } 
    // 初始化音频播放相关的结构    
    if (AudioState_Init(&me->audio_state, me->fmt_ctx) < 0) {
        printf("failed to init video state\n");
        return -1;
    } 
        
    *media = me;
    printf("Init MediaState Succes\n");
    return 0;    
}


void StreamState_Destroy(struct StreamState *stream) {
    if (stream->cod_ctx) {
        avcodec_close(stream->cod_ctx);
        avcodec_free_context(&stream->cod_ctx);
    }
    if (stream->que) destroy_queue_buffer(stream->que);
    free(stream);
}

void MediaState_Destroy(struct MediaState *media) {
    if (media->fmt_ctx) {
        avformat_close_input(&media->fmt_ctx); 
        avformat_free_context(media->fmt_ctx);
    }
    if (media->video_state) VideoState_Destroy(media->video_state);
    if (media->audio_state) AudioState_Destroy(media->audio_state);
    free(media);
}

