#ifndef __RTP_TCP_H
#define __RTP_TCP_H

#include <stdint.h>

#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE_H264 96
#define RTP_PAYLOAD_TYPE_AAC  97

#define RTP_HEADER_SIZE 12
#define RTP_MAX_PTK_SIZE 1400
#define RTP_PACKET_SIZE (4+RTP_HEADER_SIZE+2+RTP_MAX_PTK_SIZE) 

#define RTP_FRAG_TYPE_START     0 
#define RTP_FRAG_TYPE_COMMON    1 
#define RTP_FRAG_TYPE_END       2 

struct rtp_header {
    // Byte 0，位域
    uint8_t csrclen:4;
    uint8_t extension:1;
    uint8_t padding:1;
    uint8_t version:2;

    uint8_t payloadtype:7;
    uint8_t marker:1;
    uint16_t seq;
    
    uint32_t timestamp;
    uint32_t ssrc;
};

struct rtp_packet {
    char interleaved[4]; // rtsp/rtp/rtcp使用同一个socket通道，添加一个Interleaved层进行区分
    struct rtp_header header;   // 12 Bytes
    uint8_t payload[0];         // 柔性数组
};

// 传h264时marker为0，传aac时marker为1
void rtp_header_init(struct rtp_header *head, uint8_t payloadtype, \
            uint8_t marker, uint16_t seq, uint32_t timestamp, uint32_t ssrc);

/*
 * @params:
 *      sockfd 原先发送rtsp的socket
 *      packetsize = 4 + rtp_headsize + datalen
 * @return: send_bytes
 */

int rtp_send_packet(int sockfd, uint8_t rtpchannel, struct rtp_packet *packet, int datalen);

#endif
