#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AUDIO_TAG 0x08
#define VIDEO_TAG 0x09
#define SCRIPT_TAG 0x12

typedef struct {
    uint8_t signature[3];   // 'f' 'l' 'v'
    uint8_t version;        // 1
    uint8_t flags;          // 00000 1 0 1，该字节第0位和第2位表示视频、音频存在
    uint8_t offset[4];    // flv body起始位置，即flv header大小
} flv_header_t;

typedef struct {
    uint8_t tag_type;       // 0x08 音频， 0x09视频， 0x12 script
    uint8_t data_size[3];   // tag data大小，24位
    uint8_t time_stamp[3];   // 该tag的时间戳
    uint8_t ts_extended;    // 时间戳的扩展位
    uint8_t reserved[3];    // 总是为0
} tag_header_t;

int calc_u16b(uint8_t data[2]) {
    return (data[0] << 8) | data[1];
}
int calc_u24b(uint8_t data[3]) {
    return (data[0] << 16) | (data[1] << 8) | data[2];
}
int calc_u32b(uint8_t data[4]) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

unsigned int get_filelen(FILE *fp) {
    unsigned int save = ftell(fp);
    fseek(fp, 0, SEEK_END);
    unsigned int res = ftell(fp);
    fseek(fp, save, SEEK_SET);
    return res;
}

double get_double_data(uint8_t data[]) {
    double d = 0;
    unsigned char *dp = (unsigned char *)&d;
    for (int i = 0; i < 8; i++) {
        dp[i] = data[8-1 - i];
    }
    return d;
}

void process_script_tag(uint8_t *data, int n) {
    int idx = 0;
    while (1) {
        if (n - idx <= 3) // 解析完毕，只剩最后的结束标识00 00 09 
            break;

        uint8_t amf_type = data[idx]; idx += 1;
        switch (amf_type) {
            case 0x02: 
            {
                printf("AMF1:\n");
                int len = data[idx] << 16  | data[idx+1]; idx += 2; 
                char str[len+1];
                memcpy(str, &data[idx], len); idx += len;
                str[len] = '\0';
                printf("type:%d size:%d content:%s\n", amf_type, len, str);
            } break;
            case 0x08:
            {
                printf("AMF2:\n");
                int array_count = calc_u32b(&data[idx]); idx += 4;
                printf("type:%d arrary_count: %d\n", amf_type, array_count);
                for (int i = 0; i < array_count; i++) {
                    int key_size = calc_u16b(&data[idx]); idx += 2;
                    char key_str[key_size+1];
                    memcpy(key_str, &data[idx], key_size); idx += key_size;
                    key_str[key_size] = '\0';
                    printf("%s: ", key_str);

                    int valuetype = data[idx]; idx += 1;
                    switch (valuetype) {
                        case 0:
                            printf("%lf\n", get_double_data(&data[idx]));
                            idx += 8;
                            break;
                        case 1:
                            printf("%d\n", data[idx]);
                            idx += 1;
                            break;
                        case 2: 
                        { 
                            int size = calc_u16b(&data[idx]);
                            idx += 2;
                            char str[size+1];
                            memcpy(str, &data[idx], size); str[size] = '\0';
                            printf("%s\n", str);
                            idx += size;
                        } break;
                        default: break;
                    }
                }

            } break;
            default:
                break;
        }
    }
}

// 音频Tag_data: 音频参数(1字节) + 音频数据
// 格式(4bit) 频率(2bit) 位深(1bit) 布局(1bit)
void process_audio_tag(uint8_t param) {
    char fmt_str[64];
    int fmt_type = param >> 4;
    switch (fmt_type) {
        case 0:strcpy(fmt_str,"Linear PCM, platform endian");break;
        case 1:strcpy(fmt_str,"ADPCM");break;
        case 2:strcpy(fmt_str,"MP3");break;
        case 3:strcpy(fmt_str,"Linear PCM, little endian");break;
        case 4:strcpy(fmt_str,"Nellymoser 16-kHz mono");break;
        case 5:strcpy(fmt_str,"Nellymoser 8-kHz mono");break;
        case 6:strcpy(fmt_str,"Nellymoser");break;
        case 7:strcpy(fmt_str,"G.711 A-law logarithmic PCM");break;
        case 8:strcpy(fmt_str,"G.711 mu-law logarithmic PCM");break;
        case 9:strcpy(fmt_str,"reserved");break;
        case 10:strcpy(fmt_str,"AAC");break;
        case 11:strcpy(fmt_str,"Speex");break;
        case 14:strcpy(fmt_str,"MP3 8-Khz");break;
        case 15:strcpy(fmt_str,"Device-specific sound");break;
        default:strcpy(fmt_str,"UNKNOWN");break;
    }
    char rate_str[16];
    int rate_type = (param & 0x0C) >> 2;
    switch (rate_type) {
        case 0:strcpy(rate_str,"5.5-kHz");break;
        case 1:strcpy(rate_str,"1-kHz");break;
        case 2:strcpy(rate_str,"22-kHz");break;
        case 3:strcpy(rate_str,"44-kHz");break;
        default:strcpy(rate_str,"UNKNOWN");break;
    }
    char bit_str[8];
    int bit_type = (param & 0x02) >> 1;
    switch (bit_type) {
        case 0:strcpy(bit_str,"8Bit");break;
        case 1:strcpy(bit_str,"16Bit");break;
        default:strcpy(bit_str,"UNKNOWN");break;
    }
    char layout_str[8];
    int layout_type = param & 1;
    switch (layout_type) {
        case 0: strcpy(layout_str, "Mono");break;
        case 1: strcpy(layout_str, "Stereo");break;
        default: strcpy(layout_str, "UNKNOWN");break;
    }
    printf(" | %s | %s | %s | %s\n", fmt_str, rate_str, bit_str, layout_str);
}


void process_video_tag(uint8_t param) {
    char frame_str[64];
    int frame_type = param >> 4;
    switch (frame_type) {
        case 1:strcpy(frame_str,"key frame  ");break;
        case 2:strcpy(frame_str,"inter frame");break;
        case 3:strcpy(frame_str,"disposable inter frame");break;
        case 4:strcpy(frame_str,"generated keyframe");break;
        case 5:strcpy(frame_str,"video info/command frame");break;
        default:strcpy(frame_str,"UNKNOWN");break;
    }
    char code_str[64];
    int code_type = param & 0x0f;
    switch (code_type) {
        case 1:strcpy(code_str,"JPEG (currently unused)");break;
        case 2:strcpy(code_str,"Sorenson H.263");break;
        case 3:strcpy(code_str,"Screen video");break;
        case 4:strcpy(code_str,"On2 VP6");break;
        case 5:strcpy(code_str,"On2 VP6 with alpha channel");break;
        case 6:strcpy(code_str,"Screen video version 2");break;
        case 7:strcpy(code_str,"AVC");break;
        default:strcpy(code_str,"UNKNOWN");break;
    }
    printf(" | %s | %s\n", frame_str, code_str);
}

void flv_parser(const char *file) {
    FILE *fp = fopen(file, "r");
    
    // 1. analyses flv header
    rewind(fp);
    flv_header_t header;
    fread(&header, sizeof(header), 1, fp);
    // 如果offset直接声明为uint32_t，会出现字节对齐问题！！！
    // printf("cur offset of file:%ld\n", ftell(fp));

    printf("signature:%c%c%c version:%d\n", header.signature[0], \
            header.signature[1], header.signature[2], header.version);
    printf("audio:%d video:%d\n", header.flags & 1, (header.flags>>2) & 1);
    printf("header size:%d\n", calc_u32b(header.offset));
    

    // 2. analyses flv body
    unsigned int filelen = get_filelen(fp);
    uint32_t prev_tagsize = 0;
    tag_header_t tag;
    // while (!feof(fp)) { // fseek跳不到文件尾
    while (1) 
    {
        // 1. previous tag size
        fread(&prev_tagsize, sizeof(prev_tagsize), 1, fp);
        // 2. analyses tag header
        fread(&tag, sizeof(tag), 1, fp); 
        char str[10];
        switch (tag.tag_type) {
            case AUDIO_TAG: sprintf(str, "AUDIO");break;
            case VIDEO_TAG: sprintf(str, "VIDEO");break;
            case SCRIPT_TAG: sprintf(str, "SCRIPT");break;
            default: sprintf(str, "UNKOWN");break;
        }

        int data_size = calc_u24b(tag.data_size);
        int time_stamp = calc_u24b(tag.time_stamp); 
        if (tag.ts_extended) { // 扩展位为高位
            time_stamp += tag.ts_extended << 24; 
        }
    
        // 3.process tag data
        if (tag.tag_type == SCRIPT_TAG) {
            printf("\nSCRIPT_TAG:\n");
            printf("script_tag data size: %d\n", data_size);
            
            uint8_t *tag_data = malloc(sizeof(uint8_t) * data_size);
            fread(tag_data, 1, data_size, fp);
            process_script_tag(tag_data, data_size);
            
            free(tag_data);
            printf("\nTAG TYPE | DATA_SIZE | TIME_STAMP\n");
        } else {
            // tag_data: 音视频参数(1字节) + 具体数据
            printf("%8s | %6d | %6d", str, data_size, time_stamp);
            uint8_t param;
            fread(&param, 1, sizeof(param), fp);
            if (tag.tag_type == AUDIO_TAG) {
                process_audio_tag(param);            
            } else if (tag.tag_type == VIDEO_TAG) {
                process_video_tag(param); 
            }
            
            if (ftell(fp) + data_size - 1 < filelen)
                fseek(fp, data_size - 1, SEEK_CUR); // 跳过实际数据
            else 
                return ;
        }
    }

    fclose(fp);
}


int main(int argc, char const* argv[])
{
    flv_parser("../testvideo/short.flv");
    return 0;
}
