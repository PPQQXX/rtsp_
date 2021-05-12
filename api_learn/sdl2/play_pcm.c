#include "SDL2/SDL.h"

#define PCM_FILE    "../output.pcm"

unsigned int audio_len = 0;
unsigned char *audio_chunk = NULL;
unsigned char *audio_pos = NULL;

void read_audio_data(void *udata, uint8_t *stream, int len) {
    SDL_memset(stream, 0, len);
    if (audio_len == 0) return ;

    len = (len > audio_len)? audio_len:len;

    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len; 
}


void play_pcm(const char *pcmfile) {
    FILE *fp = fopen(pcmfile, "r");
    if (fp == NULL) {
        printf("failed to open pcmfile\n");
        return ;
    }

    // 1. 初始xx子系统
    if (SDL_Init(SDL_INIT_AUDIO)) {
        printf("init audio subsystem failed\n");
        return ;
    }

    // 2. 初始化音频参数
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.channels = 2;                  
    spec.format = AUDIO_S16SYS;         // signed 16bit  
    spec.silence = 0;                   // 静音值 
    spec.samples = 2048;                // 采样次数
    spec.callback = read_audio_data;    // 回调函数

    // 3. 打开音频设备
    if (SDL_OpenAudio(&spec, NULL) < 0) {
        printf("failed to open audio\n");
        return ;
    }

    // 4. 创建数据缓冲区
    int buffer_size = spec.samples * spec.channels \
            * SDL_AUDIO_BITSIZE(spec.format) / 8;
    char pcm_buffer[buffer_size];

    // 5. 启动播放
    SDL_PauseAudio(0);
    
    while (1) {
        if (fread(pcm_buffer, 1, buffer_size, fp) != buffer_size) {
            break;
        }
        
        // 设置数据缓冲区
        audio_chunk = (uint8_t *)pcm_buffer;
        audio_len = buffer_size;
        audio_pos = audio_chunk;

        // 等待回调完成（声卡播放缓冲区数据）
        while (audio_len > 0) {
//            SDL_Delay(1);
        }
    }

    fclose(fp);
    SDL_Quit();
}

int main(int argc, char const* argv[])
{
    play_pcm(PCM_FILE);
    return 0;
}
