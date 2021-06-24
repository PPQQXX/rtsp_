#ifndef __SOCK_H
#define __SOCK_H

int create_tcp_socket();
int create_udp_socket();
void close_socket(int sockfd);

int bind_socket_addr(int sockfd, const char *ip, int port);
int accept_client(int sockfd, char *ip, int *port);
int create_tcp_connect(int port);


#endif
