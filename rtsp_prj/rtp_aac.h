#ifndef __RTP_AAC_H
#define __RTP_AAC_H

#include "protocol.h"
/* 
 * @func : RTP打包传输AAC码流
 * @param: 
 *      sockfd --- UDP socket
 *      client --- client->ip, client->rtp_port
 *      path   --- AAC_FILE 
 */
int rtp_play_aac(int sockfd, client_t *client);


/*
 * 模块测试例程：
 * Linux下直接调用test(argc, argv);
 * Windows下创建rtp_test.sdp文件，用VLC打开。
 */

/* rtp_test.sdp文件:
 *
 * m=audio 2018 RTP/AVP 97
 * a=rtpmap:97 mpeg4-generic/44100/2
 * a=fmtp:97 SizeLength=13;
 * c=IN IP4 127.0.0.1
 */
int rtp_aac_test(void);


#endif
