#include <stdio.h>
#include "packet_queue.h"
#include "stream.h"
#include "audio_video_thread.h"

/* 没有起作用...
#define HANDLE_EVENT(type) do { \
    if (type == STOP_EVENT) continue;\
    else if (type == QUIT_EVENT) break;\
} while(0)
*/


/**
 * 1. packet_queue.h        提供环形缓冲区的数据管理机制，不涉及数据块的创建和销毁
 * 2. stream.h              提供码流相关的结构，包含StreamState,MediaState结构，解封装线程
 * 3. audio_video_thread.h  提供音频和视频码流的解码播放线程，包含AudioState,VideoState结构
 * 4. main.c                提供播放控制线程，定时刷新事件线程
 */


/**
 * 问题1：播放控制，需要在所有线程中添加代码，要进行松耦合优化
 * 问题2：尽力统一音频码流和视频的操作，消除if-else(策略模式)。
 * 其它：。。。
 */

int thread_quit = 0;
int thread_stop = 0;

// 发生按键操作时，会将播放主线程暂停或退出
int refresh_thread(void *p) {
    SDL_Event event;
    while (!thread_quit) {
        if (!thread_stop) {
            event.type = REFRESH_EVENT;
        } else {
            event.type = STOP_EVENT;
        }
        SDL_PushEvent(&event);
        SDL_Delay(5);  
    }
    event.type = QUIT_EVENT;
    SDL_PushEvent(&event);
    printf("refresh_thread end\n");
    return 0;
}

int playback_control_thread(void *p) {
    struct MediaState *media = (struct MediaState *)p;

    media->audio_state->audio_tid = SDL_CreateThread(audio_thread, NULL, media);
    media->video_state->video_tid = SDL_CreateThread(video_thread, NULL, media);
    
    SDL_Thread *refresh_tid = SDL_CreateThread(refresh_thread, NULL, NULL);
    while (1) {
        if (media->event.type == SDL_KEYDOWN) {
            // 空格键暂停，ESC键退出
            if (media->event.key.keysym.sym == SDLK_SPACE) {
                thread_stop = !thread_stop;
            } else if (media->event.key.keysym.sym == SDLK_ESCAPE) {
                thread_quit = 1;
            }
        } else if (media->event.type == SDL_QUIT || media->event.type == QUIT_EVENT) {
            break;
        }
 
        // 等待刷新，刷新间隔越小，实时性越好
        SDL_WaitEvent(&media->event);
    }
    SDL_WaitThread(refresh_tid, NULL);
    
    SDL_WaitThread(media->audio_state->audio_tid, NULL);
    SDL_WaitThread(media->video_state->video_tid, NULL);
    printf("playback_control_thread end\n");
}

int main(int argc, char const* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER)) {
        printf("failed to init SDL\n");
        return -1;
    }

    struct MediaState *media;
    if (MediaState_Init(&media, argv[1]) < 0) {
        printf("failed to create media state\n");
        return -1;
    }
    
    media->demuxer_tid = SDL_CreateThread(demuxer_thread, NULL, media);
    media->playback_control_tid = SDL_CreateThread(playback_control_thread, NULL, media); 
  
    // 等待线程结束
    SDL_WaitThread(media->playback_control_tid, NULL);
    SDL_WaitThread(media->demuxer_tid, NULL);
    
    MediaState_Destroy(media);
    
    SDL_Quit();
    return 0;
}

