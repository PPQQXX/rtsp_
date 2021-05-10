#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/*
 *  RTP data header: 12Bytes
 */
typedef struct {
    unsigned char cc:4;        /* CSRC count */
    unsigned char x:1;         /* header extension flag */
    unsigned char p:1;         /* padding flag */
    unsigned char version:2;   /* protocol version */
    unsigned char pt:7;        /* payload type */
    unsigned char m:1;         /* marker bit */
    unsigned short seq;      /* sequence number */
    uint32_t ts;              /* timestamp */
    uint32_t ssrc;            /* synchronization source */
    uint32_t csrc[0];         /* optional CSRC list */
} rtp_header_t;
// 网络传输 -》大端模式, 高地址放低字节
// 注意非单字节要用ntoh转换

/*
 * MPEGTS header: 4Bytes
 */
typedef struct {
    unsigned int  sync_byte;                        // 同步字节，恒为0x47
    unsigned int  ts_error_indicator: 1;            // 传输误码指示符
    unsigned int  payload_unit_start_indicator: 1;  // 有效单元载荷起始指示
    unsigned int  ts_priority: 1;                   // 优先传输
    unsigned int  PID: 13;                          // 包标识符，流类型
    unsigned int scrambling_control: 2;             // 传输控制标识
    unsigned int adaptation_field_exist: 2;         // 自适应区标识，1时表示有附加数据
    unsigned int continue_counter: 4;               // 连续计数器
} mpegts_header_t;

static struct sockaddr_in local_addr;
int udp_socket_init() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket()");
        return -1;
    }

    // sockfd绑定127.0.0.1:1998
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(1998);
    inet_pton(AF_INET, "0.0.0.0", &local_addr.sin_addr);
    if (bind(sockfd, (void *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind()");
        return -2;
    }
    return sockfd;
}

// 打印信息：cnt , payload, timestamp, seq_no, rtp size
void rtp_parser(void) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);

    int sockfd = udp_socket_init();
    if (sockfd < 0) return ;
    
    int cnt = 0;
    char recvbuf[10000];
    while (1) {
        int pktsize = recvfrom(sockfd, recvbuf, 10000, 0, (void *)&remote_addr, &len);
        if (pktsize < 0) continue;

        // 解析RTP头信息
        rtp_header_t rtpHead;
        int rtphead_size = sizeof(rtpHead);
        memcpy((void *)&rtpHead, recvbuf, rtphead_size);
        
        // 在结构体中pt要放在m前面
        char pt = rtpHead.pt;
        char payload_str[10];
        switch(pt) {
            case 18: sprintf(payload_str,"Audio");break;
            case 31: sprintf(payload_str,"H.261");break;
            case 32: sprintf(payload_str,"MPV");break;
            case 33: sprintf(payload_str,"MP2T");break;
            case 34: sprintf(payload_str,"H.263");break;
            case 96: sprintf(payload_str,"H.264");break;
            default: sprintf(payload_str,"other");break;
        }
        unsigned int timestamp = ntohl(rtpHead.ts);
        unsigned int seq_no = ntohs(rtpHead.seq);
        printf("[RTP packet] %5d| %6s| %10u| %5d| %5d|\n", cnt++, payload_str, timestamp, seq_no, pktsize);
    
        // 解析MPEG-TS头信息
        char *rtp_data = recvbuf + rtphead_size;
        int rtp_data_size = pktsize - rtphead_size;

        if (pt == 33) {
            // mpegts_header_t tsHead;
            // memcpy((void *)&tsHead, &rtp_data[i], sizeof(tsHead));
            // 逐一解析包头的每个字段...
            for (int i = 0; i < rtp_data_size; i += 188) {
                if (rtp_data[i] != 0x47) // 检验同步字节
                    break;
                printf("[MPEG-TS packet]\n");
            }
        }
    }

    close(sockfd);
}


int main(int argc, char const* argv[])
{
    rtp_parser();
    return 0;
}
