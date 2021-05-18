#include "audio_video_thread.h"

// 重采样后没有了剧烈的噪音
#define SUPPORT_AUDIO_SWR 1

// 解封装时，添加adts头，mp4a --》 AAC。
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

static void read_audio_data(void *udata, uint8_t *stream, int len) {
    struct AudioState *audio = (struct AudioState *)udata;
    SDL_memset(stream, 0, len);
    if (audio->audio_len == 0) return ;

    len = (len > audio->audio_len)? audio->audio_len:len;

    SDL_MixAudio(stream, audio->audio_pos, len, SDL_MIX_MAXVOLUME);
    audio->audio_pos += len;
    audio->audio_len -= len; 
}

static void SDL_play_pcm(struct AudioState *audio, AVFrame *frame) {
#if SUPPORT_AUDIO_SWR
    swr_convert(audio->swr_ctx, audio->pcmbuffer, audio->pcmbuf_size, \
            (const uint8_t **)frame->data, frame->nb_samples);
    audio->audio_chunk = audio->pcmbuffer[0]; 
#else
    audio->audio_chunk = (uint8_t *)frame->data[0]; 
#endif
    audio->audio_len = audio->pcmbuf_size;
    audio->audio_pos = audio->audio_chunk;
    while (audio->audio_len > 0) ;
}

static void SDL_draw_frame(struct VideoState *video, AVFrame *frame) {
    SDL_UpdateYUVTexture(video->texture, &video->rect, \
            frame->data[0], frame->linesize[0], \
            frame->data[1], frame->linesize[1], \
            frame->data[2], frame->linesize[2]);

    SDL_RenderClear(video->renderer);
    SDL_RenderCopy(video->renderer, video->texture, NULL, &video->rect);
    SDL_RenderPresent(video->renderer);
}

int audio_thread(void *p) {
    struct MediaState *media = (struct MediaState *)p;
    struct AudioState *audio = media->audio_state;
    AVFrame *frame = av_frame_alloc();
    //int display_us = 1*1000 / audio->stream_state->fps; 
    
    // 启动播放
    SDL_PauseAudio(0); 
    while (1) {
        HANDLE_EVENT(media->event.type);

        // 从码流队列中取数据, 解缓冲区无数据且封装已完成时退出
        AVPacket *packet =  dequeue(audio->stream_state->que, demuxer_state);
        if (packet == NULL) break;

        int ret = decodec_packet_to_frame(audio->stream_state->cod_ctx, packet, frame);
        if (ret == EAGAIN) continue;
        else if (ret == EINVAL) break;
        
        SDL_play_pcm(audio, frame);
        av_packet_free(&packet);  // 播放完一帧后清理
        av_frame_unref(frame);
        //SDL_Delay(display_us);
    }
    av_frame_free(&frame);
    printf("audio_thread end\n");
}

int video_thread(void *p) {
    struct MediaState *media = (struct MediaState *)p;
    struct VideoState *video = media->video_state;
    AVCodecContext *cod_ctx = video->stream_state->cod_ctx;
    AVFrame *frame = av_frame_alloc();
    
    int display_us = 1*1000 / video->stream_state->fps; 
    while (1) {
        HANDLE_EVENT(media->event.type);
    
        // 从码流队列中取数据, 解缓冲区无数据且封装已完成时退出
        AVPacket *packet =  dequeue(video->stream_state->que, demuxer_state);
        if (packet == NULL) break;

        int ret = decodec_packet_to_frame(video->stream_state->cod_ctx, packet, frame);
        if (ret == EAGAIN) continue;
        else if (ret == EINVAL) break;
        
        SDL_draw_frame(video, frame);
        av_packet_free(&packet);  // 播放完一帧后清理
        av_frame_unref(frame);
        SDL_Delay(display_us);
    }
    av_frame_free(&frame);
    printf("video_thread end\n");
}



// 解码API：码流数据 ---》 原始图像/采样数据
int decodec_packet_to_frame(AVCodecContext *cod_ctx, AVPacket *packet, AVFrame *frame) {
    int send_ret = avcodec_send_packet(cod_ctx, packet);
    if (send_ret == AVERROR_EOF || send_ret == AVERROR(EAGAIN)) {
        return EAGAIN;
    } else if (send_ret < 0) {
        printf("failed to send packet to decoder\n");
        return EINVAL;
    }

    int receive_ret = avcodec_receive_frame(cod_ctx, frame);
    if (receive_ret == AVERROR_EOF || receive_ret == AVERROR(EAGAIN)) {
        return EAGAIN;
    } else if (receive_ret < 0) {
        printf("failed to receive frame frome decoder\n");
        return EINVAL;
    }
    return 0;
}


int AudioState_Init(struct AudioState **audio, AVFormatContext *fmt_ctx) {
    struct AudioState *me = malloc(sizeof(*me));

    if (StreamState_Init(&me->stream_state, AVMEDIA_TYPE_AUDIO, fmt_ctx) < 0) {
        printf("failed to init video stream state\n");
        return -1;
    }
    
    AVCodecContext *cod_ctx = me->stream_state->cod_ctx;
    //AVCodecParameters *param = fmt_ctx->streams[me->stream_state->stream_index]->codecpar;
    me->spec.freq = cod_ctx->sample_rate;
    me->spec.channels = cod_ctx->channels;
    me->spec.format = AUDIO_S16SYS;
    me->spec.silence = 0;
    me->spec.samples = cod_ctx->frame_size;
    me->spec.callback = read_audio_data;
    me->spec.userdata = me;
    if (SDL_OpenAudio(&me->spec, NULL) < 0) {
        printf("failed to open audio\n");
        return -1;
    }
    
#if SUPPORT_AUDIO_SWR
    // 输出格式必须为AV_SAMPLE_FMT_S16
    me->swr_ctx = swr_alloc();
    me->swr_ctx = swr_alloc_set_opts(me->swr_ctx, cod_ctx->channel_layout, AV_SAMPLE_FMT_S16, me->spec.freq, \
            cod_ctx->channel_layout, cod_ctx->sample_fmt, cod_ctx->sample_rate, 0, NULL);
    swr_init(me->swr_ctx);
    me->pcmbuffer = (uint8_t **)av_calloc(1, sizeof(uint8_t *));
    me->pcmbuf_size = av_samples_alloc(me->pcmbuffer, NULL, \
            me->spec.channels, me->spec.samples, AV_SAMPLE_FMT_S16, 0); 
#else
    me->pcmbuf_size = me->spec.samples *me->spec.channels \
            * SDL_AUDIO_BITSIZE(me->spec.format) / 8;
#endif
    printf("pcmbuf_size:%d\n", me->pcmbuf_size);
    
    *audio = me;
    return 0;
}

int VideoState_Init(struct VideoState **video, AVFormatContext *fmt_ctx) {
    struct VideoState *me = malloc(sizeof(*me));

    // 初始化视频解码器
    if (StreamState_Init(&me->stream_state, AVMEDIA_TYPE_VIDEO, fmt_ctx) < 0) {
        printf("failed to init video stream state\n");
        return -1;
    }
    // 初始化过滤器
    const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
    if (bsf == NULL) {
        printf("failed to find bitstream filter\n");
        return -1;
    }
    av_bsf_alloc(bsf, &me->bsf_ctx);
    avcodec_parameters_copy(me->bsf_ctx->par_in, fmt_ctx->streams[me->stream_state->stream_index]->codecpar);
    av_bsf_init(me->bsf_ctx);

    // 创建窗口
    int screen_w = me->stream_state->cod_ctx->width;   
    int screen_h = me->stream_state->cod_ctx->height;
    me->window = SDL_CreateWindow("SDL2.0 Video Sample", \
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, \
            screen_w, screen_h, SDL_WINDOW_RESIZABLE);
    if (me->window == NULL) {
        printf("failed to create window.\n");
        return -1;
    }

    // 创建渲染器
    me->renderer = SDL_CreateRenderer(me->window, -1, 0);
    if (me->renderer == NULL) {
        printf("failed to create renderer\n");
        return -1;
    }

    // 创建纹理
    me->texture = SDL_CreateTexture(me->renderer, SDL_PIXELFORMAT_IYUV, \
            SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);
    if (me->texture == NULL) {
        printf("failed to create texture\n");
        return -1;
    }
    
    // 图像显示位置与大小
    me->rect.x = 0;
    me->rect.y = 0;
    me->rect.w = screen_w,
    me->rect.h = screen_h,

    *video = me;
    printf("Init VideoState Succes\n");
    return 0;
}

void AudioState_Destroy(struct AudioState *audio) {
    if (audio->stream_state) StreamState_Destroy(audio->stream_state);
    if (audio->swr_ctx) swr_free(&audio->swr_ctx);
    if (audio->pcmbuffer) free(audio->pcmbuffer);
    SDL_CloseAudio();
    free(audio);
}


void VideoState_Destroy(struct VideoState *video) {
    if (video->stream_state) StreamState_Destroy(video->stream_state);
    
    if (video->window) SDL_DestroyWindow(video->window);
    if (video->renderer) SDL_DestroyRenderer(video->renderer);
    if (video->texture) SDL_DestroyTexture(video->texture);
    
    if (video->bsf_ctx) av_bsf_free(&video->bsf_ctx);
    free(video);
}

