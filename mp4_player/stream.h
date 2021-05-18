#ifndef __STREAM_H
#define __STREAM_H

/****************** 事件相关的宏 *************************/
#define REFRESH_EVENT (SDL_USEREVENT + 0)
#define STOP_EVENT    (SDL_USEREVENT + 1)
#define QUIT_EVENT    (SDL_USEREVENT + 2)

// 不要加do {..}while(0)，不然continue,break不起作用...
#define HANDLE_EVENT(type) { \
    if (type == STOP_EVENT) continue;\
    else if (type == QUIT_EVENT) break;\
}


/****************** 码流结构，解码相关 ***********************/
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "packet_queue.h"
struct StreamState {
    int stream_index;           // 码流下标
    AVCodecContext *cod_ctx;    // 解码器上下文
    AVCodec *cod;               // 解码器
    float fps;                  // 视频播放延时为1/fps
    struct myqueue *que;        // 可播放的码流队列
};  

int StreamState_Init(struct StreamState **stream, int stream_type, AVFormatContext *fmt_ctx);
void StreamState_Destroy(struct StreamState *stream);


/****************** 媒体信息，最顶层结构 **********************/
#include "audio_video_thread.h"
#include "SDL2/SDL.h"
struct MediaState {
    AVFormatContext *fmt_ctx;           // 输入流的格式上下文
    struct VideoState *video_state;     // 视频结构 
    struct AudioState *audio_state;     // 音频结构

    SDL_Thread *demuxer_tid;            // 解封装线程
    SDL_Thread *playback_control_tid;   // 播放控制线程
    SDL_Event event;                    // 按键事件，播放/暂停/退出
};

extern int demuxer_state;       // 1表示解封装完成，队列空时不需要等待数据
int demuxer_thread(void *p);
int MediaState_Init(struct MediaState **media, const char *url);
void MediaState_Destroy(struct MediaState *media);


#endif
