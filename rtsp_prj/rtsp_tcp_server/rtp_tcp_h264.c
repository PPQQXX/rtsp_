#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sock.h"
#include "rtp_tcp.h"
#include "protocol.h"

// 问题：时而花屏(已解决, 原rtp_send_h264_nalu中memcpy有误)
typedef struct _FILE_MANAGER {
    char *filename;
    FILE *fp;
    uint8_t *filebuf;
    int filesize;
    int readpos;    // 读取位置
    int searchpos;  // 搜索位置
} file_t;

file_t *create_file_manger(const char *path, char *permission) {
    file_t *me = malloc(sizeof(*me));
    memset(me, 0, sizeof(*me));

    me->filename = strdup(path);
    me->fp = fopen(path, permission);
    struct stat st;
    stat(me->filename, &st);
    me->filesize = st.st_size;
    me->filebuf = (uint8_t *)malloc(me->filesize);
    me->readpos = 0;
    me->searchpos = 0;
    return (me->filename &&  me->fp && me->filesize && me->filebuf)? me:NULL;
}

void destroy_file_manager(file_t *file) {
    free(file->filename);
    free(file->filebuf);
    fclose(file->fp);
    free(file);
}

int file_read_finish(file_t *file) {
    return file->readpos == file->filesize;
}
int file_search_finish(file_t *file) {
    return file->searchpos == file->filesize;
}


int startcode_len(unsigned char *buf) {
    if (buf[0] == 0 && buf[1] == 0) {
        if (buf[2] == 1) return 3;
        else if(buf[3] == 1) return 4;
    }
    return -1;
}

// 返回起始码长度
int get_h264_nalu(file_t *file, uint8_t *nalu, int *nalulen) {
    static int count = 4*1024;    // 每次读4K字节
    int last_pos = file->searchpos;

    if (file->readpos == 0) {   // 第一次读
        fread(file->filebuf, 1, count, file->fp);
        file->readpos += count;
    }

    int codelen = startcode_len(&file->filebuf[last_pos]);
    if (codelen != 3 && codelen != 4) {
        printf("failed to find startcode\n");
        return -1;
    }

    int found = 0;
    file->searchpos += codelen;  // 跳过起始码，继续搜索
    // 动态搜索, 找到下一个起始码
    while (!found) {
        if (file->searchpos < file->readpos) {
            if (file->searchpos + 4 >= file->filesize) { // 防止在函数内数组越界
                file->searchpos = file->filesize;
                break;  // 最后一个NALU
            }
            if (startcode_len(&file->filebuf[file->searchpos]) < 0)
                file->searchpos++;
            else
                found = 1;
        } else {
            int len = fread(&file->filebuf[file->readpos], 1, count, file->fp);
            if (len == count)
                file->readpos += count;
            else
                file->readpos = file->filesize;
        }
    }

    *nalulen = file->searchpos - last_pos - codelen;
    memcpy(nalu, &file->filebuf[last_pos+codelen], *nalulen);

    return codelen;
}

/*
 * 2字节片段信息
 * 第一个字节：|F|NRI|Type| = 1:2:5 bit   // 前3bit与nalu_hdr相同，Type为28
 * 第二个字节：|S|E|R|Type| = 1:1:1:5 bit // 起始，结束标志，NALU类型
 */
static void load_fragment_msg(struct rtp_packet *packet, uint8_t nalu_hdr, int frag_type) {
    packet->payload[0] = (nalu_hdr & 0x60) | 28;  // FU-A类型码28，FU-B类型码29
    packet->payload[1] = nalu_hdr & 0x1F;
    switch (frag_type) {
        case RTP_FRAG_TYPE_START:  packet->payload[1] |= 0x80; break;
        case RTP_FRAG_TYPE_END:    packet->payload[1] |= 0x40; break;
        default: break;
    }
}

int rtp_send_fragment(int sockfd, uint8_t rtpchannel, \
        struct rtp_packet *packet, uint8_t *data, int datalen) {
    memcpy(packet->payload+2, data, datalen);
    int size = rtp_send_packet(sockfd, rtpchannel, packet, 2+datalen);
    if (size < 0) return -1;
    packet->header.seq++;
    return size;
}
int rtp_send_h264_nalu(int sockfd, uint8_t rtpchannel, struct rtp_packet *packet, uint8_t *nalu, int nalulen) {
    int send_bytes = 0;
    uint8_t nalu_hdr = nalu[0];

    if (nalulen <= RTP_MAX_PTK_SIZE) {
        // rtp_packet = [4字节prev, 12字节rtp头, 一个完整的nalu]
        memcpy(packet->payload, nalu, nalulen);
        int ret = rtp_send_packet(sockfd, rtpchannel, packet, nalulen);
        if (ret < 0) return -1;

        packet->header.seq++;
        send_bytes += ret;
    } else {
        // rtp_packet = [4字节prev, 12字节rtp头, 2字节片段信息, NALU数据片段]
        int t = nalulen - 1;  // payload不包括nalu头部
        int ptk_cnt = t / RTP_MAX_PTK_SIZE;  // 分片个数
        if (t % RTP_MAX_PTK_SIZE != 0) ptk_cnt += 1;

        int nalu_pos = 1; // 跳过nalu头部
        for (int i = 0; i < ptk_cnt - 1; i++) {
            if (i == 0) {
                load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_START);
            } else {
                load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_COMMON);
            }

            int ret = rtp_send_fragment(sockfd, rtpchannel, packet, &nalu[nalu_pos], RTP_MAX_PTK_SIZE);
            if (ret < 0) return -1;
            send_bytes += ret;
            nalu_pos += RTP_MAX_PTK_SIZE;
        }

        // 发送最后一个RTP包分片
        load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_END);
        int ret = rtp_send_fragment(sockfd, rtpchannel, packet, &nalu[nalu_pos], nalulen-nalu_pos);
        if (ret < 0) return -1;
        send_bytes += ret;
    }

    return send_bytes;
}

int rtp_play_h264(int sockfd, uint8_t rtpchannel, const char *path) {
    file_t *file = create_file_manger(path, "r");
    if (file == NULL) return -1;

    struct rtp_packet *packet = (struct rtp_packet *)malloc(RTP_PACKET_SIZE);
    rtp_header_init(&packet->header, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0x88923423);

    uint8_t *nalu = malloc(1024*1024);
    int nalulen, index = 0;
    printf("send start\n");
    while (!file_search_finish(file)) {
        int codelen = get_h264_nalu(file, nalu, &nalulen);
        if (codelen < 0) {
            break;
        } else if (codelen == 4) { // 该NALU是一帧的开始
            index++;
            //printf("read %4d rtp frame:%5d packet:%c\n", index, nalulen, packet->prev[0]);
            usleep(1000*1000/H264_FPS);
        }

        rtp_send_h264_nalu(sockfd, rtpchannel, packet, nalu, nalulen);
        if ((nalu[0] & 0x1f) != 7 && (nalu[0] & 0x1f) != 8)   // SPS、PPS不需要加时间戳
            packet->header.timestamp += 90000/H264_FPS;
    }
    printf("send finish\n");
    free(packet);
    free(nalu);
    destroy_file_manager(file);
    return 0;
}


