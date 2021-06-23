#ifndef __RTP_H264_H
#define __RTP_H264_H

#include "protocol.h"
/* 
 * @func : RTP打包传输H264码流
 * @param: 
 *      sockfd --- UDP socket
 *      client --- client->ip, client->rtp_port
 *      path   --- H264 file 
 */
int rtp_play_h264(int sockfd, client_t *client, const char *path);


/*
 * 模块测试例程：
 * Linux下直接调用test(argc, argv);
 * Windows下创建rtp_test.sdp文件，用VLC打开。
 */

/* rtp_test.sdp文件:
 *
 * m=video 2018 RTP/AVP 96
 * a=rtpmap:96 H264/90000
 * a=framerate:25
 * c=IN IP4 127.0.0.1
 */
int rtp_h264_test(const char *file);


#endif
