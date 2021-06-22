#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "time.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "sock.h"
#include "protocol.h"
#include "rtsp.h"

#define BUF_MAX_SIZE    (1024*1024)

static int handleCmd_OPTIONS(char* result, int cseq);
static int handleCmd_DESCRIBE(char* result, int cseq, char* url);
static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort);
static int handleCmd_PLAY(char* result, int cseq);

static char* getLineFromBuf(char* buf, char* line)
{
    while(*buf != '\n') {
        *line = *buf;
        line++;
        buf++;
    }
    *line = '\n';    ++line;
    *line = '\0';

    ++buf;
    return buf; 
}
static void do_client(int clientSockfd, const char* clientIP, int clientPort,
        int serverRtpSockfd, int serverRtcpSockfd)
{
    char method[40], url[100], version[40];
    int clientRtpPort, clientRtcpPort;
    char *bufPtr;
    char* rbuf = malloc(BUF_MAX_SIZE);
    char* sbuf = malloc(BUF_MAX_SIZE);
    char line[400];
    
    int cseq = 0;
    while(1)
    {
        int recvLen;

        recvLen = recv(clientSockfd, rbuf, BUF_MAX_SIZE, 0);
        if(recvLen <= 0)
            goto out;

        rbuf[recvLen] = '\0';
        printf("---------------C->S--------------\n");
        printf("%s", rbuf);

        /* 解析方法 */
        bufPtr = getLineFromBuf(rbuf, line);
        if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3)
        {
            printf("parse err\n");
            goto out;
        }

        /* 解析序列号 */
        bufPtr = getLineFromBuf(bufPtr, line);
        if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1)
        {
            printf("parse err\n");
            goto out;
        }

        /* 如果是SETUP，那么就再解析client_port */
        if(!strcmp(method, "SETUP"))
        {
            while(1)
            {
                bufPtr = getLineFromBuf(bufPtr, line);
                if(!strncmp(line, "Transport:", strlen("Transport:")))
                {
                    sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
                            &clientRtpPort, &clientRtcpPort);
                    break;
                }
            }
        }

        if(!strcmp(method, "OPTIONS"))
        {
            if(handleCmd_OPTIONS(sbuf, cseq))
            {
                printf("failed to handle options\n");
                goto out;
            }
        }
        else if(!strcmp(method, "DESCRIBE"))
        {
            if(handleCmd_DESCRIBE(sbuf, cseq, url))
            {
                printf("failed to handle describe\n");
                goto out;
            }
        }
        else if(!strcmp(method, "SETUP"))
        {
            if(handleCmd_SETUP(sbuf, cseq, clientRtpPort))
            {
                printf("failed to handle setup\n");
                goto out;
            }
        }
        else if(!strcmp(method, "PLAY"))
        {
            if(handleCmd_PLAY(sbuf, cseq))
            {
                printf("failed to handle play\n");
                goto out;
            }
        }
        else
        {
            goto out;
        }

        printf("---------------S->C--------------\n");
        printf("%s", sbuf);
        send(clientSockfd, sbuf, strlen(sbuf), 0);
    }
out:
    close_socket(clientSockfd);
    free(rbuf);
    free(sbuf);
}

int rtsp_talk_with_client(int server_sockfd) {
    // 创建SERVER_RTP和SERVER_RTCP端口的套接字
    int rtp_fd = create_udp_socket();
    int rtcp_fd = create_udp_socket();
    if (rtp_fd < 0 || rtcp_fd < 0) {
        printf("failed to create_udp_socket\n");
        return -1;
    }
    if (bind_socket_addr(rtp_fd, "0.0.0.0", SERVER_RTP_PORT) < 0 || \
            bind_socket_addr(rtcp_fd, "0.0.0.0", SERVER_RTCP_PORT) < 0) {
        printf("failed to bind addr\n");
        return -1;
    }

    int client_sockfd, client_port;
    char client_ip[40];
    while(1) {
        client_sockfd = accept_client(server_sockfd, client_ip, &client_port);
        if (client_sockfd < 0) {
            printf("failed to accept client\n");
            return -1;
        }
        printf("accept client: %s:%d\r\n", client_ip, client_port);

        do_client(client_sockfd, client_ip, client_port, rtp_fd, rtcp_fd);
        close_socket(client_sockfd); 
    }

    close_socket(rtp_fd);
    close_socket(rtcp_fd);
    return 0;
}


int rtsp_test(void)
{
    int server_sockfd = create_tcp_connect(SERVER_TCP_PORT);
    if (server_sockfd < 0) return -1;
    printf("rtsp://127.0.0.1:%d\n", SERVER_TCP_PORT);

    rtsp_talk_with_client(server_sockfd);

    close_socket(server_sockfd);
    return 0;
}



static int handleCmd_OPTIONS(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
            "\r\n",
            cseq);

    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{
    char sdp[500];
    char localIp[100];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
            "o=- 9%ld 1 IN IP4 %s\r\n"
            "t=0 0\r\n"
            "a=control:*\r\n"
            "m=video 0 RTP/AVP 96\r\n"
            "a=rtpmap:96 H264/90000\r\n"
            "a=control:track0\r\n",
            time(NULL), localIp);

    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
            "Content-Base: %s\r\n"
            "Content-type: application/sdp\r\n"
            "Content-length: %ld\r\n\r\n"
            "%s",
            cseq,
            url,
            strlen(sdp),
            sdp);

    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
            "Session: 66334873\r\n"
            "\r\n",
            cseq,
            clientRtpPort,
            clientRtpPort+1,
            SERVER_RTP_PORT,
            SERVER_RTCP_PORT);

    return 0;
}

static int handleCmd_PLAY(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "Range: npt=0.000-\r\n"
            "Session: 66334873; timeout=60\r\n\r\n",
            cseq);

    return 0;
}


