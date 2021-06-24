#include <stdio.h>
#include "rtp_h264.h"
#include "rtp_aac.h"
#include "rtsp.h"
#include "protocol.h"

// 所有配置在在protocol.h中定义
int main(int argc, char const* argv[])
{
    int chose = 0;
    printf("There are some test set you can chose\n");
    printf("1 --- rtsp server test\n");
    printf("      you can configure rtsp for h264/aac/null in protocol.h\n");
    printf("2 --- rtp h264 to vlc client\n");
    printf("3 --- rtp aac to vlc client\n");
    printf("your chose:");
    scanf("%d", &chose);

    // 测试方法：vlc打开rtsp://192.168.174.3:1935/live
    if (chose == 1)
        rtsp_server_test(); 

    // 测试方法：vlc打开rtp_test.sdp
    else if (chose == 2) {
        rtp_h264_test(); 
    } else if (chose == 3) {
        rtp_aac_test(); 
    }

    return 0;
}
