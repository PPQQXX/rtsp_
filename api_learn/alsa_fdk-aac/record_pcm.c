#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

// 通过Alsa框架进行录音，获取pcm数据

// record -l 获取声卡
// 一般 hw:0 或 default 表示默认声卡设备

#define AUDIO_RATE 44100
#define AUDIO_CHANNEL 2

void record_pcm(const char * out_file) {
    FILE *fp = fopen(out_file, "w+");

    // 1. pcm设备结构器
    snd_pcm_t *pcm_st;
    snd_pcm_open(&pcm_st, "default", SND_PCM_STREAM_CAPTURE, 0);

    // 2. 初始化pcm参数
    snd_pcm_hw_params_t *param;
    snd_pcm_hw_params_malloc(&param);
    snd_pcm_hw_params_any(pcm_st, param);
    
    // 设置左右声道数据的存储方式，交错方式存放
    // 先记录帧1的左声道数据和右声道数据，再开始帧2的记录
    snd_pcm_hw_params_set_access(pcm_st, param, SND_PCM_ACCESS_RW_INTERLEAVED);
    // 设置采样格式，声道数量，采样率
    snd_pcm_hw_params_set_format(pcm_st, param, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_st, param, AUDIO_CHANNEL);
    int sample_rate = AUDIO_RATE, dir;   
    snd_pcm_hw_params_set_rate_near(pcm_st, param, &sample_rate, &dir);
    
    // 设置最大的缓冲时间，buffer_time单位为us
    unsigned int buffer_time;
    snd_pcm_hw_params_get_buffer_time_max(param, &buffer_time, 0);
    printf("max buffer_time:%d\n", buffer_time);
    if (buffer_time > 500000) buffer_time = 500000;
    snd_pcm_hw_params_set_buffer_time_near(pcm_st, param, &buffer_time, 0);
    
    // 设置采样周期，采样一帧音频数据的时间
    // 44100Khz，若帧率42fps（采样周期23.8ms）,则1050次采样得到一帧数据。
    unsigned int period_time = 23800;
    snd_pcm_hw_params_set_period_time_near(pcm_st, param, &period_time, 0);

    // 3. 将设置初始化到pcm结构
    snd_pcm_hw_params(pcm_st, param);


    // 4. 初始化一个帧缓冲区
    snd_pcm_uframes_t frames;   // 每帧采样点数
    snd_pcm_hw_params_get_period_size(param, &frames, &dir);
    printf("每帧采样点数：%d\n", (int)frames);

    int size = frames * 2 * AUDIO_CHANNEL;  // 每个采样点2Bytes，size表示一帧大小
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) goto _Error;

    while (1) {
        // 5. 开始录音
        int ret = snd_pcm_readi(pcm_st, buffer, frames);
        if (ret == -EPIPE) {
            printf("overrun occurred\n");
            ret = snd_pcm_prepare(pcm_st);
            if (ret < 0) {
                printf("failed to recover form overrun\n");
                goto _Error;
            }
        }
        else if (ret < 0) {
            printf("error from read:%s\n", snd_strerror(ret));
            goto _Error;
        }
        else if (ret != (int)frames) {
            printf("short read, read %d samples\n", ret);
        }
        
        // 6. 保存录音
        fwrite(buffer, 1, size, fp);
    }

_Error:
    if (fp) {
        fclose(fp);
    }
    if (pcm_st) {
        snd_pcm_drain(pcm_st);
        snd_pcm_close(pcm_st);
    }
    if (buffer) {
        free(buffer);
    }
}


int main(int argc, char const* argv[])
{
    record_pcm("output.pcm");
    return 0;
}













