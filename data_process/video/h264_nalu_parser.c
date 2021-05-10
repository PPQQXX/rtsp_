#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    NALU_TYPE_SLICE = 1,    // 不分区，非IDR图像
    NALU_TYPE_DPA,          // 片分区A
    NALU_TYPE_DPB,          // 片分区B
    NALU_TYPE_DPC,          // 片分区C
    NALU_TYPE_IDR,          // IDR图像中的片
    NALU_TYPE_SEI,         // 补充增强信息单元
    NALU_TYPE_SPS,          // 序列参数集
    NALU_TYPE_PPS,          // 图像参数集
    NALU_TYPE_AUD,          // 分界符
    NALU_TYPE_EOSEQ,        // 序列结束符
    NALU_TYPE_EOSTREAM,     // 码流结束符
    NALU_TYPE_FILL          // 填充
} NaluType;

typedef enum {
    NALU_PRIORITY_DISPOSABLE = 0,
    NALU_PRIORITY_LOW = 1,
    NALU_PRIORITY_HIGH = 2,
    NALU_PRIORITY_HIGHEST = 3,
} NaluPriority;


typedef struct {
    int pos;        // NALU对应起始码，在文件中的位置
    int codelen;    // 起始码大小
    int header;     // (0) + (priority)2bits + (types)5bits
} Nalu_t;



void fill_typestr(char *type_str, int type) {
    switch(type) {
        case NALU_TYPE_SLICE:sprintf(type_str,"SLICE");break;
        case NALU_TYPE_DPA:sprintf(type_str,"DPA");break;
        case NALU_TYPE_DPB:sprintf(type_str,"DPB");break;
        case NALU_TYPE_DPC:sprintf(type_str,"DPC");break;
        case NALU_TYPE_IDR:sprintf(type_str,"IDR");break;
        case NALU_TYPE_SEI:sprintf(type_str,"SEI");break;
        case NALU_TYPE_SPS:sprintf(type_str,"SPS");break;
        case NALU_TYPE_PPS:sprintf(type_str,"PPS");break;
        case NALU_TYPE_AUD:sprintf(type_str,"AUD");break;
        case NALU_TYPE_EOSEQ:sprintf(type_str,"EOSEQ");break;
        case NALU_TYPE_EOSTREAM:sprintf(type_str,"EOSTREAM");break;
        case NALU_TYPE_FILL:sprintf(type_str,"FILL");break;
    }
}

void fill_priostr(char *priostr, int prio) {
    switch(prio) {
        case NALU_PRIORITY_DISPOSABLE:sprintf(priostr,"DISPOS");break;
        case NALU_PRIORITY_LOW:sprintf(priostr,"LOW");break;
        case NALU_PRIORITY_HIGH:sprintf(priostr,"HIGH");break;
        case NALU_PRIORITY_HIGHEST:sprintf(priostr,"HIGHEST");break;
    }
}

int startcode_len(unsigned char *buf) {
    if (buf[0] == 0 && buf[1] == 0) {
        if (buf[2] == 1) return 3;
        else if(buf[3] == 1) return 4;
    }    
    return -1;
}

void h264_analyse(const char *file) {
    FILE *fp = fopen(file, "r");
    unsigned char *buf = malloc(30000000); // 根据视频大小设
    Nalu_t *nalu = malloc(sizeof(*nalu));
    memset(nalu, 0, sizeof(Nalu_t));
    
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    rewind(fp);


    // 先打印第一个NALU信息 
    fread(buf, 1, 100, fp);
    nalu->pos = 0;
    nalu->codelen = startcode_len(&buf[nalu->pos]);

    printf("| NUM | CODE |   PRIO |   TYPE  | LEN |\n");

    int cnt = 0;                // nalu个数
    int left = 0, right = 100;  // left表示当前搜索位置，right表示当前文件偏移
    int found, len;             
    while (1) {
        int headidx = nalu->pos + nalu->codelen + 1;
        nalu->header = buf[headidx];

        int type = nalu->header & 0x1F;
        char type_str[10];
        fill_typestr(type_str, type);

        int prio = (nalu->header & 0x60)>>5;
        char prio_str[10];
        fill_priostr(prio_str, prio);


        // 找到下一个起始码
        found = 0; len = 0;
        left += nalu->codelen + 1;  // 跳过startcode和header
        while (!found) {
            if (left < right) {
                if (left + 4 >= filesize) // 防止在函数内数组越界
                    goto clean;
                len = startcode_len(&buf[left++]);
                if (len > 0) found = 1;
            } else {
                if (right + 100 < filesize) {
                    fread(&buf[right], 1, 100, fp);
                    right += 100;
                } else {    // 剩余的数据不到100B字节
                    fread(&buf[right], 1, filesize - right, fp);
                    right = filesize;
                }
            }
        }
        int nalulen = left - nalu->pos - nalu->codelen;  // NALU大小，不包括起始码

        printf("|%5d|%6d|%8s|%9s|%5d|\n", cnt, nalu->codelen, prio_str, type_str, nalulen);
        cnt++;
        nalu->codelen = len;
        nalu->pos = left - 1;
    }

clean:
    free(nalu);
    free(buf);
    fclose(fp);
}


int main(int argc, char const* argv[])
{
    h264_analyse("test.h264"); 
    return 0;
}
