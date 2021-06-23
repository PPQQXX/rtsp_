#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/types.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "rtp.h"

 
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
int rtp_send_packet(int sockfd, const char *ip, int port, struct rtp_packet *packet, int packetsize) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(ip);

    packet->header.seq = htons(packet->header.seq); 
    packet->header.timestamp = htonl(packet->header.timestamp);
    packet->header.ssrc = htonl(packet->header.ssrc);
    
    int ret = sendto(sockfd, (const void *)packet, packetsize, 0, \
            (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    packet->header.seq = ntohs(packet->header.seq); 
    packet->header.timestamp = ntohl(packet->header.timestamp);
    packet->header.ssrc = ntohl(packet->header.ssrc);
    return ret;
}



