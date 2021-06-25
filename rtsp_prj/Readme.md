
`使用方式：打开测试集合test_set.c和配置文件protocol.h`
>编译：./myrun.sh 
>测试：运行./a.out，然后选择测试模块。

<br>
所有模块的测试情况：
```bash
1）直接用RTP传aac时没有问题，即rtp_aac正常。
2）直接用RTP传h264时没有问题，即rtp_h264正常。
3）使用RTSP传输H264正常。
4）使用RTSP传输AAC正常。
5）使用RTSP传输摄像头监控正常。
```
