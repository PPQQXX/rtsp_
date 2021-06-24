#ifndef __PROTOCOL_H
#define __PROTOCOL_H

// 相关协议的约定
typedef struct {
    char *ip;
    int tcp_port;
    int rtp_port;
    int rtcp_port;
} client_t;

// rtp over tcp, 使用rtsp的socket传输
// rtsp传输流的方式取决于客户端，vlc客户端要设置rtp over tcp

// H264帧率
#define H264_FPS 25
#define H264_FILE "/home/zhou/ffmpeg/testvideo/001.h264"
#define AAC_FILE "/home/zhou/ffmpeg/testvideo/001.aac"

// 服务器端口，可以指定 
#define SERVER_TCP_PORT     1935


#endif
