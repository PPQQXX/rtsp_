#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct wav_head {
    char fccID[4];          // 规范标识符，RIFF
    unsigned int dwSize;   
    char fccType[4];        // 格式，WAVE
} head_t;

typedef struct wav_fmt {
    char fccID[4];
    unsigned int dwSize;
    unsigned short wFormatTag;      // 编码格式
    unsigned short wChannels;       // 通道数
    unsigned int dwSamplesPerSec;   // 采样率
    unsigned int dwAvgBytesPerSec;  // 每秒码率
    unsigned short wBlockAgain;     // 对齐占位数据, 2-单声道，4-立体
    unsigned short uiBitsPerSample; // 采样精度
} fmt_t;

typedef struct wav_data {
    char fccID[4];
    unsigned int dwSize;
} data_t;

/* WAV音频格式文件结构
 * WAVE_HEAD
 * WAVE_FMT
 * WAVE_DATA
 * 原PCM数据
 */

// WAV文件头：head + fmt + data = 12 + 24 + 8 = 44 Bytes 
void pcm16le_to_wav(const char *file, int channels, int sample_rate, const char *wavfile) {
    if (channels == 0 || sample_rate == 0) {
        channels = 2; sample_rate = 44100;  // default
    }
    int bits = 16;

    head_t pcm_head;
    fmt_t pcm_fmt;
    data_t pcm_data;


    FILE *fp, *fpout;
    fp = fopen(file, "r");
    fpout = fopen(wavfile, "w+");
    
    // HEAD
    memcpy(pcm_head.fccID, "RIFF", 4);
    memcpy(pcm_head.fccType, "WAVE", 4);

    // FMT
    fseek(fpout, sizeof(head_t), SEEK_SET);
    pcm_fmt.dwSamplesPerSec = sample_rate;
    pcm_fmt.dwAvgBytesPerSec = sample_rate * 4; // 每次采样 = 2字节*2 
    pcm_fmt.uiBitsPerSample = bits;
    memcpy(pcm_fmt.fccID, "fmt ", 4);
    pcm_fmt.dwSize = 16;
    pcm_fmt.wBlockAgain = 4;        // 2-单声道 
    pcm_fmt.wChannels = channels;   // 声道数
    pcm_fmt.wFormatTag = 1;     // 标签，1表示PCM编码

    // 1. 填充head_msg
    fwrite(&pcm_fmt, sizeof(fmt_t), 1, fpout);
    
    // DATA
    fseek(fpout, sizeof(fmt_t), SEEK_CUR); // 偏移
    memcpy(pcm_data.fccID, "data", 4);
    pcm_data.dwSize = 0;

    
    char sample[4];
    while (!feof(fp)) {
        fread(sample, 4, 1, fp);
        pcm_data.dwSize += 4;
        fwrite(sample, 4, 1, fpout);
    }
    pcm_head.dwSize = 44 + pcm_data.dwSize; // wav文件头44字节，加上原音频数据
    
    // 2. 填充head_msg和data_msg
    rewind(fpout);
    fwrite(&pcm_head, sizeof(head_t), 1, fpout);
    fseek(fpout, sizeof(fmt_t), SEEK_CUR);
    fwrite(&pcm_data, sizeof(data_t), 1, fpout);

    fclose(fp);
    fclose(fpout);
}

int main(int argc, char const* argv[])
{
    pcm16le_to_wav("samples/test.pcm", 2, 44100, "output_wav.wav");    
    return 0;
}
