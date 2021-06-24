
`使用方式：打开测试集合test_set.c和配置文件protocol.h`
编译：gcc test_set.c rtsp.c rtp_h264.c rtp_aac.c rtp.c sock.c 

<br>
所有模块的测试情况：
```
1）直接用RTP传aac时没有问题，即rtp_aac正常。
2）直接用RTP传h264时没有问题，即rtp_h264正常。
3）使用RTSP传输H264正常。
4）使用RTSP传输AAC异常：
/*
 * 使用RTSP传输AAC时有问题，rtp发送完所有封包后，
 * vlc上过好一会，才播放出一小部分音频，然后就没了。
 */
```
