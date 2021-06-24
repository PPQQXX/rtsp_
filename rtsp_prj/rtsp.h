#ifndef __RTSP_H
#define __RTSP_H


int rtsp_talk_with_client(int server_sockfd);

/*
 * 模块测试：使用RTSP传输h264/aac，或单纯与vlc交互
 * vlc中打开网络串流rtsp://192.168.174.3:1935/live
 */
int rtsp_server_test(void);

#endif
