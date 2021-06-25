#ifndef __PROTOCOL_H
#define __PROTOCOL_H

// 在rtsp+rtp传输时，客户端的RTP端口不能指定，需要从SETUP请求中获取
typedef struct {
    char *ip;
    int tcp_port;
    int rtp_port;
    int rtcp_port;
} client_t;

// 单独用于RTP_H264/AAC测试
#define RTP_CLIENT_IP       "192.168.102.215"
#define RTP_CLIENT_PORT     2018
// 不支持ubuntu到windows的多播？
//#define RTP_CLIENT_IP       "239.255.255.11"


// RTSP服务器测试
#define RTSP_USE_RTP_AAC        0  // rtsp传输aac
#define RTSP_USE_RTP_H264       0  // rtsp传输h264
#define RTSP_USE_RTP_H264_RT    1  // 实时监控
#define RTSP_USE_RTP_NULL       0  // rtsp与vlc交互，不传输流

// 测试文件的路径 
#define H264_FPS 25
#define H264_FILE "/home/zhou/ffmpeg/testvideo/001.h264"
#define AAC_FILE "/home/zhou/ffmpeg/testvideo/001.aac"

// capture_h264.c采集相关设置
#define MEDIA_DURATION 40
#define CAMERA_DEV "/dev/video0"
#define CAMERA_W    640
#define CAMERA_H    480
#define CAMERA_FPS  25
#define CAMERA_FMT AV_PIX_FMT_YUYV422
#define ENCODE_FMT AV_PIX_FMT_YUV420P


// 服务器端口，用户指定 
#define SERVER_TCP_PORT     1935
#define SERVER_RTP_PORT     55532
#define SERVER_RTCP_PORT    55533


#endif
