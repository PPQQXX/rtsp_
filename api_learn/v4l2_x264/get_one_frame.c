#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"

void get_one_frame(const char *dev, const char *picfile) {
    // 查找设备前要先调用avdevice_register_all
    AVInputFormat *in_fmt = in_fmt = av_find_input_format("video4linux2");
    if (in_fmt == NULL) {
        printf("can't find_input_format\n");
        return ;
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, dev, in_fmt, NULL) < 0) {
        printf("can't open_input_file\n");
        return ;
    }
    // printf device info
    av_dump_format(fmt_ctx, 0, dev, 0);

    AVPacket *packet = (AVPacket *)av_malloc(sizeof(*packet));
    av_read_frame(fmt_ctx, packet);  
    printf("data length: %d\n", packet->size);

    FILE *fp = fopen(picfile, "w+");
        
    fwrite(packet->data, 1, packet->size, fp);

    fclose(fp);
    av_free_packet(packet);
    avformat_close_input(&fmt_ctx);
}

int main(int argc, char const* argv[])
{
    avdevice_register_all();
    get_one_frame("/dev/video0", "output.yuv"); 
    return 0;
}
