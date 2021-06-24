#ifndef __RTP_TCP_H264_H
#define __RTP_TCP_H264_H

#include <stdint.h>
#include "protocol.h"
/* 
 * @func : RTP打包传输H264码流
 * @param: 
 *      sockfd --- TCP socket 
 *      path   --- H264 file 
 */
int rtp_play_h264(int sockfd, uint8_t rtpchannel, const char *path);

#endif
