#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int display_header_msg(char header[], int cnt, int *length) {
    if (header[0] != 0xff && (header[1] & 0xF0) != 0xf0) {
        return -1; // syncword（12bits）不是0XFFF
    }
    // profile 17:18位, 取第3字节高2位
    char profile_str[8];
    int pro = ((int)header[2] & 0xC0) >> 6; 
    switch (pro) {
        case 0: sprintf(profile_str, "Main");break;
        case 1: sprintf(profile_str, "LC");break;
        case 2: sprintf(profile_str, "SSR");break;
        default: sprintf(profile_str, "Unknown");break;
    };
    
    // frequency_idx 19:23位，取第3字节中间4位
    char freq_str[8];
    int freq = (header[2] & 0x3C) >> 2;
    switch (freq) {
        //...
        case 3: sprintf(freq_str, "48000KHz");break;
        case 4: sprintf(freq_str, "44100KHz");break;
        //...
        default: sprintf(freq_str, "unknown");break;
    }

    // aac_frame_length 30:42（13位） header[3]后2位+header[4]+header[5]前3位
    int size = (header[5] & 0XE0) >> 5; // low  3bits
    size += header[4] << 3;             // mid  8bits
    size += (header[3] & 0x03) << 11;   // high 2bits 

    *length = size;
    printf("|%5d| %8s| %10s| %5d|\n", cnt, profile_str, freq_str, size);
    return 0;
}

void aac_parser_adts(const char *aacfile) {
    FILE *fp = fopen(aacfile, "r");
    char adts[7];
    
    printf("|------- ADTS Frame Message -------|\n");
    printf("| NUM | Profile | Frequency | Size |\n");
    printf("|-----|---------|-----------|------|\n");
    
    int cnt = 0, len = 0;
    fseek(fp, 0, SEEK_END);
    int file_end = ftell(fp); 
    rewind(fp); // 相当于fseek(fp, 0, SEEK_SET);
    
    while (!feof(fp)) {
        fread(adts, 1, 7, fp);
        if (display_header_msg(adts, cnt, &len) < 0) {
            printf("header bad\n");
            break;
        } 
        cnt++;
        //printf("seek_cur:%d\n", ftell(fp)); //当前offset
        if (ftell(fp) + len - 7 > file_end) break;

        fseek(fp, len-7, SEEK_CUR); //向后移动n个字节
    }
    fclose(fp);
}

int main(int argc, char const* argv[])
{
    aac_parser_adts("output.aac");   
    return 0;
}
