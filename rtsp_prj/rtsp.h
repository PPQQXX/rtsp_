#ifndef __RTSP_H
#define __RTSP_H


int rtsp_talk_with_client(int server_sockfd);

/*
 * 模块测试例子：与vlc交互过程
 * vlc中打开网络串流rtsp://192.168.174.3:1935/live
 */
int rtsp_test(void);

#endif
