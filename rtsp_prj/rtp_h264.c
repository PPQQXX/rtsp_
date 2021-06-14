#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sock.h"
#include "rtp.h"

// 将H264打包为RTP包，vlc打开xx.sdp进行播放
// RTP = header + payload

/* rtp_h264.sdp文件:
 *
 * m=video 2018 RTP/AVP 96 
 * a=rtpmap:96 H264/90000
 * a=framerate:25
 * c=IN IP4 127.0.0.1
 */

// 问题：时而花屏


// 目标主机IP和端口
#define CLIENT_IP   "192.168.1.116"
#define CLIENT_PORT 2018

static uint8_t *filebuf;
static int filesize;

int startcode_len(unsigned char *buf) {
    if (buf[0] == 0 && buf[1] == 0) {
        if (buf[2] == 1) return 3;
        else if(buf[3] == 1) return 4;
    }    
    return -1;
}

int get_h264_nalu(FILE *fp, uint8_t *nalu, int *code_len) {
    static int last_pos = 0, codelen = 0; // last_pos表示起始码位置
    // left表示当前搜索位置，right表示当前文件偏移
    static int left = 0, right = 0;  
    static int count = 10*1024;    // 每次读10K字节
   
    if (left == filesize) // 搜索完毕 
        return 0;
    if (codelen == 0) {   // 第一次读 
        fread(filebuf, 1, count, fp);
        right += count;
    }

    codelen = startcode_len(&filebuf[last_pos]);
    if (codelen != 3 && codelen != 4) {
        printf("failed to find startcode\n");
        return -1;
    } else {
        left += codelen;  // 跳过起始码，继续搜索
    }

    int found = 0;             
    // 动态搜索, 找到下一个起始码
    while (!found) {
        if (left < right) {
            if (left + 4 >= filesize) { // 防止在函数内数组越界
                left = filesize;
                break;  // 最后一个NALU
            } 
            if (startcode_len(&filebuf[left]) < 0) left++;
            else found = 1;
        } else {
            int len = fread(&filebuf[right], 1, count, fp);
            if (len == count)
                right += count;
            else
                right = filesize;
        }
    }
    
    int nalulen = left - last_pos - codelen;
    memcpy(nalu, &filebuf[last_pos+codelen], nalulen);
    last_pos = left;
    
    *code_len = codelen;
    return nalulen;
}

// 分片位置
#define FRAG_TYPE_START     0 
#define FRAG_TYPE_COMMON    1 
#define FRAG_TYPE_END       2 
static void load_fragment_msg(struct rtp_packet *packet, uint8_t nalu_hdr, int frag_type) {
    packet->payload[0] = (nalu_hdr & 0x60) | 28;  // FU-A类型码28，FU-B类型码29
    packet->payload[1] = nalu_hdr & 0x1F;
    packet->header.marker = 0;  // 最后一个分片置1
    switch (frag_type) {
        case FRAG_TYPE_START:  packet->payload[1] |= 0x80; break;
        case FRAG_TYPE_END:    
            { packet->header.marker = 1; packet->payload[1] |= 0x40;} break;
        default: break;
    }
}

int rtp_send_h264_nalu(int sockfd, uint8_t *nalu, int nalulen, struct rtp_packet *packet) {
    int send_bytes = 0;
    uint8_t nalu_hdr = nalu[0];
    
    if (nalulen <= RTP_MAX_PTK_SIZE) {
        // rtp_packet = [12字节rtp头, 一个完整的nalu]
        memcpy(packet->payload, nalu, nalulen);
        int ret = rtp_send_packet(sockfd, CLIENT_IP, CLIENT_PORT, packet, nalulen+RTP_HEADER_SIZE);
        if (ret < 0)
            return -1;
        packet->header.marker = 1;
        packet->header.seq++;
        send_bytes += ret;
    } else {
        nalulen -= 1;  // payload不包括nalu头部 
        int ptk_cnt = nalulen / RTP_MAX_PTK_SIZE;  // 完整RTP包的个数
        int remain_szie = nalulen % RTP_MAX_PTK_SIZE;
        int ret, nalu_pos = 1; // 跳过nalu头部
        
        // rtp_packet = [12字节rtp头, 2字节片段信息, 1400字节NALU数据片段]
        // 片段信息
        // 第一个字节：|F|NRI|Type| = 1:2:5 bit   // 前3bit与nalu_hdr相同，Type为28
        // 第二个字节：|S|E|R|Type| = 1:1:1:5 bit // 起始，结束标志，NALU类型
        for (int i = 0; i < ptk_cnt; i++) {
            
            if (i == 0) {
                load_fragment_msg(packet, nalu_hdr, FRAG_TYPE_START);
            } else if (remain_szie == 0 && i == ptk_cnt - 1) {
                load_fragment_msg(packet, nalu_hdr, FRAG_TYPE_END);
            } else {
                load_fragment_msg(packet, nalu_hdr, FRAG_TYPE_COMMON);
            }

            memcpy(packet->payload+2, &nalu[nalu_pos], RTP_MAX_PTK_SIZE);
            ret = rtp_send_packet(sockfd, CLIENT_IP, CLIENT_PORT, packet, RTP_HEADER_SIZE+2+RTP_MAX_PTK_SIZE);
            if (ret < 0) return -1;
            
            packet->header.seq++;
            send_bytes += ret;
            nalu_pos += RTP_MAX_PTK_SIZE;
        }

        // 发送剩余数据，最后一个RTP包分片
        if (remain_szie > 0) {
            load_fragment_msg(packet, nalu_hdr, FRAG_TYPE_END);
            memcpy(packet->payload+2, &nalu[nalu_pos], remain_szie+2);
            ret = rtp_send_packet(sockfd, CLIENT_IP, CLIENT_PORT, packet, remain_szie+2);
            if (ret < 0) return -1;
            packet->header.seq++;
            send_bytes += ret;
        }
    }

    return send_bytes;
}

int main(int argc, char const* argv[])
{
    if (argc != 2) {
        printf("usage: ./a.out h264file\n");
        return -1;
    }
    int fps = 25;   // 固定帧率

    // 打开文件，创建udp套接字
    FILE *fp = fopen(argv[1], "r");
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        printf("failed to init socket\n");
        return -1;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    filebuf = malloc(filesize);
    if (filebuf == NULL) return -1;
    rewind(fp);
    printf("h264 file size: %d\n", filesize);

    struct rtp_packet *packet = (struct rtp_packet *)malloc(RTP_PACKET_SIZE);
    rtp_header_init(&packet->header, RTP_PAYLOAD_TYPE_H264, 0, 0, 0x88923423);
    printf("version:%d payloadtype:%d\n", packet->header.version, packet->header.payloadtype);

    uint8_t *nalu = malloc(1024*1024);

    int index = 0, read_finish = 0;
    int codelen;
    while (!read_finish) {
        int nalulen = get_h264_nalu(fp, nalu, &codelen);
        if (nalulen < 0) {
            printf("failed to read frame\n");
            break;
        } else if (nalulen == 0) {
            read_finish = 1;
        }
    
        // 起始码为00 00 00 01时表示NALU是一帧的开始
        if (codelen == 4) {
            index++;
            usleep(1000*1000/fps);
        } else {
            usleep(1000);
        }

        rtp_send_h264_nalu(sockfd, nalu, nalulen, packet);
        if ((nalu[0] & 0x1f) != 7 && (nalu[0] & 0x1f) != 8)   // SPS、PPS不需要加时间戳
            packet->header.timestamp += 90000/fps;
    }
    
    free(packet);
    free(filebuf);
    free(nalu);
    close_socket(sockfd);
    return 0;
}
