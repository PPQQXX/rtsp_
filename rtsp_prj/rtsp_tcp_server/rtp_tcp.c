#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/types.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "rtp_tcp.h"

 
void rtp_header_init(struct rtp_header *head, uint8_t payloadtype, \
            uint8_t marker, uint16_t seq, uint32_t timestamp, uint32_t ssrc) {
    head->csrclen = 0;
    head->extension = 0;
    head->padding = 0;
    head->version = RTP_VERSION;
    head->payloadtype = payloadtype;
    head->marker = marker;
    head->seq = seq;
    head->timestamp = timestamp;
    head->ssrc = ssrc;
}

// RTP采用网络字节序（大端模式）, 所以多字节数据传输需要调整顺序
int rtp_send_packet(int sockfd, uint8_t rtpchannel, struct rtp_packet *packet, int datalen) {
    packet->interleaved[0] = '$';  // 与rtsp区分
    packet->interleaved[1] = rtpchannel;    // 区别rtp和rtcp。0-1
    // rtp包载荷大小
    packet->interleaved[2] = ((datalen+RTP_HEADER_SIZE) & 0xff00)>>8;
    packet->interleaved[3] = (datalen+RTP_HEADER_SIZE) & 0xff;

    packet->header.seq = htons(packet->header.seq); 
    packet->header.timestamp = htonl(packet->header.timestamp);
    packet->header.ssrc = htonl(packet->header.ssrc);
    
    int ret = send(sockfd, (void *)packet, 4+RTP_HEADER_SIZE+datalen, 0);

    packet->header.seq = ntohs(packet->header.seq); 
    packet->header.timestamp = ntohl(packet->header.timestamp);
    packet->header.ssrc = ntohl(packet->header.ssrc);
    return ret;
}



