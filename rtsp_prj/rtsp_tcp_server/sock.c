#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sock.h"

int create_tcp_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    int val = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(val));
    return sockfd;
}

int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return -1;

    int val = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val, sizeof(val));
    return sockfd;
}

void close_socket(int sockfd) {
    close(sockfd);
}


int bind_socket_addr(int sockfd, const char *ip, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
        return -1;
    }
    return 0;
}

int accept_client(int sockfd, char *ip, int *port) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, len);

    int clientfd = accept(sockfd, (struct sockaddr *)&addr, &len);
    if (clientfd < 0) return -1;

    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);
    return clientfd;
}


int create_tcp_connect(int port) {
    // 服务器tcp套接字
    int sockfd = create_tcp_socket();
    if (sockfd < 0) {
        printf("failed to create_tcp_socket\n");
        return -1;
    }

    // 套接字绑定127.0.0.1:1935
    if (bind_socket_addr(sockfd, "0.0.0.0", port) < 0) {
        printf("failed to bind addr\n");
        return -1;
    }
    // 连接请求个数最多为10
    if (listen(sockfd, 10) < 0) {
        printf("failed to listen\n");
        return -1;
    }
    return sockfd;
}




