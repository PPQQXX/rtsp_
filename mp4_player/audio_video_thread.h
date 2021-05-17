#ifndef __AUDIO_VIDEO_THREAD_H
#define __AUDIO_VIDEO_THREAD_H

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "SDL2/SDL.h"
#include "stream.h"

struct AudioState {
    struct StreamState *stream_state;
    SDL_AudioSpec spec;
    int pcmbuf_size;
    uint8_t **pcmbuffer;
    struct SwrContext *swr_ctx;
    
    unsigned int audio_len;
    unsigned char *audio_chunk;
    unsigned char *audio_pos;
    SDL_Thread *audio_tid;
};

struct VideoState {
    struct StreamState *stream_state;
    AVBSFContext *bsf_ctx;  
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Rect rect;
    SDL_Thread *video_tid;
};


int audio_thread(void *p);
int video_thread(void *p);
int decodec_packet_to_frame(AVCodecContext *cod_ctx, AVPacket *packet, AVFrame *frame);

char *adts_header_gen(int len);
int AudioState_Init(struct AudioState **audio, AVFormatContext *fmt_ctx);
void AudioState_Destroy(struct AudioState *audio);

int VideoState_Init(struct VideoState **video, AVFormatContext *fmt_ctx);
void VideoState_Destroy(struct VideoState *video);




#endif
