#ifndef __STREAM_H
#define __STREAM_H

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "packet_queue.h"


#define REFRESH_EVENT (SDL_USEREVENT + 0)
#define STOP_EVENT    (SDL_USEREVENT + 1)
#define QUIT_EVENT    (SDL_USEREVENT + 2)


struct StreamState {
    int stream_index;           // 码流下标
    AVCodecContext *cod_ctx;    // 解码器上下文
    AVCodec *cod;               // 解码器
    float fps;                  // 播放延时为1/fps
    struct myqueue *que;        // 可播放的码流队列
};  

// 创建两个环形队列，里面用来存放处理后可直接播放的h264码流数据和AAC码流
// 创建一个解封装线程，不断demuxer然后用过滤器处理码流，将数据入队
// 创建一个解码播放线程，从队列中取码流数据，然后解码播放，播放速度用事件驱动控制
#include "audio_video_thread.h"
#include "SDL2/SDL.h"
struct MediaState {
    AVFormatContext *fmt_ctx;
    struct VideoState *video_state;
    struct AudioState *audio_state;
    
    SDL_Thread *demuxer_tid;
    SDL_Thread *playback_control_tid;
    SDL_Event event;
};

extern int demuxer_state;
int demuxer_thread(void *p);

int StreamState_Init(struct StreamState **stream, int stream_type, AVFormatContext *fmt_ctx);
void StreamState_Destroy(struct StreamState *stream);

int MediaState_Init(struct MediaState **media, const char *url);
void MediaState_Destroy(struct MediaState *media);


#endif
