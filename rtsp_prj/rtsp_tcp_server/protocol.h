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

// 在rtsp+rtp传输时，客户端的RTP端口不能指定，需要从SETUP请求中获取

// 用于RTP传输测试
#define SUPPORT_TEST_SET
#define RTP_CLIENT_IP       "192.168.102.215"
#define RTP_CLIENT_PORT     2018
// 不支持ubuntu到windows的多播？
//#define RTP_CLIENT_IP       "239.255.255.11"

// H264帧率
#define H264_FPS 25
#define H264_FILE "/home/zhou/ffmpeg/testvideo/001.h264"
#define AAC_FILE "/home/zhou/ffmpeg/testvideo/001.aac"

// 服务器端口，可以指定 
#define SERVER_TCP_PORT     1935


#endif
