#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"

int flush_encoder(AVFormatContext *fmt_ctx, AVCodecContext *cod_ctx, AVFrame *frame, unsigned int stream_index) {
    int cap = fmt_ctx->streams[stream_index]->codec->codec->capabilities;
    if (! ( cap & AV_CODEC_CAP_DELAY)) { 
        return 0;
    }

    int ret, got_frame;
    AVPacket packet;

    while (1) {
        av_packet_unref(&packet);

        //ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, \
                &packet, NULL, &got_frame);
        if (avcodec_send_frame(cod_ctx, frame) < 0) {
            printf("failed to send frame\n");
            return -1;
        }
        int got_packet = avcodec_receive_packet(cod_ctx, &packet);
        if (got_packet == 0) {
            return 0;   
        }

        av_frame_free(NULL);
        printf("flush encoder: succeed to encode 1 frame! size:%d\n", packet.size);
        ret = av_write_frame(fmt_ctx, &packet);
        if (ret < 0) break;
    }
    return ret;
}

void yuv_to_h264(const char *sourcefile, int w, int h, const char *destfile) {
    // 初始化封装格式上下文
    AVOutputFormat *out_fmt = av_guess_format(NULL, destfile, NULL);
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    fmt_ctx->oformat = out_fmt;

    // 打开输出文件
    if (avio_open(&fmt_ctx->pb, destfile, AVIO_FLAG_WRITE) < 0) {
        printf("failed to open destfile\n");
        return ;
    }

    // 创建输出码流
    //AVStream *video_stream = avformat_new_stream(fmt_ctx, NULL);
    /*
    int video_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i; break;
        }
    }
    if (video_index < 0) {
        printf("failed to find video stream\n");
        return ;
    }
    */
    
    // 初始化编码器上下文
    //AVCodecContext *cod_ctx = video_stream->codec;
    AVCodecContext *cod_ctx = avcodec_alloc_context3(NULL);
    // avcodec_parameters_to_context(cod_ctx, fmt_ctx->streams[0]->codecpar);

    cod_ctx->codec_id = out_fmt->video_codec;   // 编码器id
    cod_ctx->codec_type = AVMEDIA_TYPE_VIDEO;   // 编码器类型
    cod_ctx->pix_fmt = AV_PIX_FMT_YUV420P;      // 图像格式
    cod_ctx->width = w;
    cod_ctx->height = h;
    cod_ctx->time_base.num = 1;     
    cod_ctx->time_base.den = 25;    // 帧率越高视频越流畅
    // 视频码率计算方式：视频文件大小/视频时间
    cod_ctx->bit_rate = 52224*1024;      // 视频码率
    // 画面组GOP
    cod_ctx->gop_size = 50;        // 表示每250帧插入一个I帧，I帧越少，视频越小，大小设置要合理。
    cod_ctx->qmin = 10;             // 量化参数，设置默认值。量化系数越小，视频越清晰。
    cod_ctx->qmax = 51;             
    cod_ctx->max_b_frames = 0;       // B帧最大值，0表示不需要B帧

    AVStream *video_stream = avformat_new_stream(fmt_ctx, NULL);
    if (video_stream == NULL) {
        printf("failed to create new stream\n");
        return ;
    }
    avcodec_parameters_from_context(video_stream->codecpar, cod_ctx);



    // 打开编码器
    AVCodec *cod = avcodec_find_encoder(cod_ctx->codec_id);
    if (cod == NULL) {
        printf("failed to find encoder\n");
        return ;
    }
    printf("encoder: %s\n", cod->name);

    AVDictionary *param = 0; 
    // h264编码器需要设置一些参数
    if (cod_ctx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }
    if (avcodec_open2(cod_ctx, cod, &param) < 0) {
        printf("failed to open encoder\n");
        return ;
    }

    


    // 写头部信息
    int flag = avformat_write_header(fmt_ctx, NULL);

    // 循环编码YUV文件为H264
    // 1. 开辟缓冲区
    int bufsize = av_image_get_buffer_size(cod_ctx->pix_fmt, cod_ctx->width, cod_ctx->height, 1);    
    uint8_t *outbuf = (uint8_t *)av_malloc(bufsize);

    FILE *fp = fopen(sourcefile, "r");
    if (fp == NULL) {
        printf("sourcefile isn't exist\n");
        return ;
    }

    // 2. 内存空间填充
    AVFrame *frame = av_frame_alloc();
    av_image_fill_arrays(frame->data, frame->linesize, \
            outbuf, cod_ctx->pix_fmt, cod_ctx->width, cod_ctx->height, 1);

    // 3. 开辟packet
    AVPacket *packet = (AVPacket *)av_malloc(bufsize);
    
    
    // 4. 循环编码
    int y_size = cod_ctx->width * cod_ctx->height;  // Y分量大小
    int yuv_size = y_size * 3 / 2; 
    int count = 0, ret;
    while (1) {
        int len = fread(outbuf, 1, yuv_size, fp);
        if (len == 0){
            printf("sourcefile read finish\n");
            break;
        }

        frame->data[0] = outbuf;                // Y分量
        frame->data[1] = outbuf + y_size;       // U分量
        frame->data[2] = outbuf + y_size*5/4;   // V分量
        
        frame->pts = count;
        count++;

        // 视频编码处理
        avcodec_send_frame(cod_ctx, frame);  // 发送一帧图像数据
        ret = avcodec_receive_packet(cod_ctx, packet); // 接收一帧压缩数据 
        if (ret == 0) {  // 编码成功，将数据写入输出文件
            packet->stream_index = video_stream->index;
            ret = av_write_frame(fmt_ctx, packet);
            if (ret < 0) {
                printf("failed to write frame\n");
                return ;
            }
        }

    }

    // 写入剩余帧数据，可能没有
    flush_encoder(fmt_ctx, cod_ctx, frame, 0);
    // 写入文件尾部信息
    av_write_trailer(fmt_ctx);

    // 释放内存资源
    fclose(fp);
    avcodec_close(cod_ctx);
    av_free(frame);
    av_free(outbuf);
    av_packet_free(&packet);
    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
}

int main(int argc, char const* argv[])
{
    yuv_to_h264("testvideo/output.yuv", 640, 272, "output.h264");
    return 0;
}
