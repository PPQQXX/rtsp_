#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sock.h"
#include "rtp.h"
#include "rtp_aac.h"

struct adts_header {     
    char profile[8];    
    int channel; 
    int freq;       
    int length; // 一帧aac长度，包括adts头和原始流 
};

static int parser_adts(struct adts_header *adts, uint8_t header[]) {
    if ((header[0] != 0xFF) && ((header[1] & 0xF0) != 0xF0)) {
        return -1;  // syncword（12bits）不是0XFFF
    }
    memset(adts, 0, sizeof(struct adts_header));

    // profile 17:18位, 取第3字节高2位
    int pro = ((unsigned int)header[2] & 0xC0) >> 6; 
    switch (pro) {
        case 0: strcpy(adts->profile, "Main");break;
        case 1: strcpy(adts->profile, "LC");break;
        case 2: strcpy(adts->profile, "SSR");break;
        default: strcpy(adts->profile, "Unknown");break;
    };
   
    // frequency_idx 19:23位，取第3字节中间4位
    int freq_index = ((unsigned int)header[2] & 0x3C) >> 2;
    switch (freq_index) {
        //...
        case 3: adts->freq = 48000; break;
        case 4: adts->freq = 44100; break;
        default: adts->freq = 0; break;
    }

    adts->channel = ((unsigned int)header[2] & 0x01) << 2;
    adts->channel += ((unsigned int)header[3] & 0xC0) >> 6;

    // aac_frame_length 30:42（13位） header[3]后2位+header[4]+header[5]前3位
    int size = ((unsigned int)header[5] & 0XE0) >> 5; // low  3bits
    size += header[4] << 3;             // mid  8bits
    size += (header[3] & 0x03) << 11;   // high 2bits 
    adts->length = size;
    return 0;
}



static int rtp_send_acc_frame(int sockfd, client_t *client, uint8_t *data, int datalen, struct rtp_packet *packet) {
    packet->payload[0] = 0x00;
    packet->payload[1] = 0x10;
    packet->payload[2] = (datalen & 0x1fe0) >> 5;
    packet->payload[3] = (datalen & 0x1f) << 3; 

    memcpy(packet->payload+4, data, datalen);
    int ret = rtp_send_packet(sockfd, client->ip, client->rtp_port, \
            packet, RTP_HEADER_SIZE+4+datalen);
    if (ret < 0) return -1;
    
    packet->header.seq++;
    packet->header.timestamp += 1024;
    return 0;
}


int rtp_play_aac(int sockfd, client_t *client) {
    struct rtp_packet *packet = (struct rtp_packet *)malloc(RTP_PACKET_SIZE);
    rtp_header_init(&packet->header, RTP_PAYLOAD_TYPE_AAC, 1, 0, 0, 0x32411);

    FILE *fp = fopen(AAC_FILE, "r");
    if (fp == NULL) {
        return -1;
    }

    uint8_t header[7];
    struct adts_header adts;
    uint8_t *data = malloc(1024); // aac数据一般是几百字节
    int ret = 0;
    while (1) {
        ret = fread(header, 1, 7, fp);
        if (ret <= 0) {
            break;
        }
        if (parser_adts(&adts, header) < 0) {
            printf("failed parser_adts\n");
            break;
        }
        
        fread(data, 1, adts.length-7, fp);
        rtp_send_acc_frame(sockfd, client, data, adts.length-7, packet);    
        
        printf("packet seq:%d adts freq:%d length: %d\n",\
                packet->header.seq, adts.freq, adts.length);
        float fps = adts.freq / 1024.0;
        usleep(1000*1000 / (int)fps);
    }

    free(packet);
    free(data);
    fclose(fp);
    return 0;
}

int rtp_aac_test(void)
{
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        printf("failed to init socket\n");
        return -1;
    }
    
    client_t client;
    client.ip = strdup(RTP_CLIENT_IP);
    client.rtp_port = RTP_CLIENT_PORT;
    
    rtp_play_aac(sockfd, &client);

    close_socket(sockfd);
    return 0;
}
