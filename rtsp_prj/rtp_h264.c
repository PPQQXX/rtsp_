#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sock.h"
#include "rtp.h"
#include "rtp_h264.h"
#include "packet_queue.h"
#include "libavformat/avformat.h"
#include "pthread.h"
#include "capture_h264.h"

typedef struct _FILE_MANAGER {
    char *filename;
    FILE *fp;
    uint8_t *filebuf;
    int filesize;
    int readpos;    // 读取位置
    int searchpos;  // 搜索位置
} file_t;

file_t *create_file_manger(const char *path, char *permission) {
    file_t *me = malloc(sizeof(*me));
    memset(me, 0, sizeof(*me));

    me->filename = strdup(path);
    me->fp = fopen(path, permission);
    struct stat st;
    stat(me->filename, &st);
    me->filesize = st.st_size;
    me->filebuf = (uint8_t *)malloc(me->filesize);
    me->readpos = 0;
    me->searchpos = 0;
    return (me->filename &&  me->fp && me->filesize && me->filebuf)? me:NULL;
}

void destroy_file_manager(file_t *file) {
    free(file->filename);
    free(file->filebuf);
    fclose(file->fp);
    free(file);
}

int file_read_finish(file_t *file) {
    return file->readpos == file->filesize;
}
int file_search_finish(file_t *file) {
    return file->searchpos == file->filesize;
}


int startcode_len(unsigned char *buf) {
    if (buf[0] == 0 && buf[1] == 0) {
        if (buf[2] == 1) return 3;
        else if(buf[3] == 1) return 4;
    }
    return -1;
}

// 返回起始码长度
int get_h264_nalu(file_t *file, uint8_t *nalu, int *nalulen) {
    static int count = 4*1024;    // 每次读4K字节
    int last_pos = file->searchpos;

    if (file->readpos == 0) {   // 第一次读
        fread(file->filebuf, 1, count, file->fp);
        file->readpos += count;
    }

    int codelen = startcode_len(&file->filebuf[last_pos]);
    if (codelen != 3 && codelen != 4) {
        printf("failed to find startcode\n");
        return -1;
    }

    int found = 0;
    file->searchpos += codelen;  // 跳过起始码，继续搜索
    // 动态搜索, 找到下一个起始码
    while (!found) {
        if (file->searchpos < file->readpos) {
            if (file->searchpos + 4 >= file->filesize) { // 防止在函数内数组越界
                file->searchpos = file->filesize;
                break;  // 最后一个NALU
            }
            if (startcode_len(&file->filebuf[file->searchpos]) < 0)
                file->searchpos++;
            else
                found = 1;
        } else {
            int len = fread(&file->filebuf[file->readpos], 1, count, file->fp);
            if (len == count)
                file->readpos += count;
            else
                file->readpos = file->filesize;
        }
    }

    *nalulen = file->searchpos - last_pos - codelen;
    memcpy(nalu, &file->filebuf[last_pos+codelen], *nalulen);

    return codelen;
}

/*
 * 2字节片段信息
 * 第一个字节：|F|NRI|Type| = 1:2:5 bit   // 前3bit与nalu_hdr相同，Type为28
 * 第二个字节：|S|E|R|Type| = 1:1:1:5 bit // 起始，结束标志，NALU类型
 */
static void load_fragment_msg(struct rtp_packet *packet, uint8_t nalu_hdr, int frag_type) {
    packet->payload[0] = (nalu_hdr & 0x60) | 28;  // FU-A类型码28，FU-B类型码29
    packet->payload[1] = nalu_hdr & 0x1F;
    switch (frag_type) {
        case RTP_FRAG_TYPE_START:  packet->payload[1] |= 0x80; break;
        case RTP_FRAG_TYPE_END:    packet->payload[1] |= 0x40; break;
        default: break;
    }
}

int rtp_send_fragment(int sockfd, client_t *client, uint8_t *data, int datalen, struct rtp_packet *packet) {
    memcpy(packet->payload+2, data, datalen);
    int size = rtp_send_packet(sockfd, client->ip, client->rtp_port, \
            packet, RTP_HEADER_SIZE+2+datalen);
    if (size < 0) return -1;
    packet->header.seq++;
    return size;
}

int rtp_send_h264_nalu(int sockfd, client_t *client, uint8_t *nalu, int nalulen, struct rtp_packet *packet) {
    int send_bytes = 0;
    uint8_t nalu_hdr = nalu[0];

    if (nalulen <= RTP_MAX_PTK_SIZE) {
        // rtp_packet = [12字节rtp头, 一个完整的nalu]
        memcpy(packet->payload, nalu, nalulen);
        int ret = rtp_send_packet(sockfd, client->ip, client->rtp_port, packet, nalulen+RTP_HEADER_SIZE);
        if (ret < 0) return -1;

        packet->header.seq++;
        send_bytes += ret;
    } else {
        // rtp_packet = [12字节rtp头, 2字节片段信息, NALU数据片段]
        int t = nalulen - 1;  // payload不包括nalu头部
        int ptk_cnt = t / RTP_MAX_PTK_SIZE;  // 分片个数
        if (t % RTP_MAX_PTK_SIZE != 0) ptk_cnt += 1;

        int nalu_pos = 1; // 跳过nalu头部
        for (int i = 0; i < ptk_cnt - 1; i++) {
            if (i == 0) 
                load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_START);
            else 
                load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_COMMON);

            int ret = rtp_send_fragment(sockfd, client, &nalu[nalu_pos], RTP_MAX_PTK_SIZE, packet);
            if (ret < 0) return -1;
            send_bytes += ret;
            nalu_pos += RTP_MAX_PTK_SIZE;
        }

        // 发送最后一个RTP包分片
        load_fragment_msg(packet, nalu_hdr, RTP_FRAG_TYPE_END);
        int ret = rtp_send_fragment(sockfd, client, &nalu[nalu_pos], nalulen-nalu_pos, packet);
        if (ret < 0) return -1;
        send_bytes += ret;
    }

    return send_bytes;
}

int rtp_play_h264(int sockfd, client_t *client) {
    file_t *file = create_file_manger(H264_FILE, "r");
    if (file == NULL) return -1;
    //printf("h264 file size: %d\n", file->filesize);

    struct rtp_packet *packet = (struct rtp_packet *)malloc(RTP_PACKET_SIZE);
    rtp_header_init(&packet->header, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0x88923423);
    //printf("version:%d payloadtype:%d\n", packet->header.version, packet->header.payloadtype);

    uint8_t *nalu = malloc(1024*1024);
    int nalulen, index = 0;
    while (!file_search_finish(file)) {
        int codelen = get_h264_nalu(file, nalu, &nalulen);
        if (codelen < 0) {
            break;
        } else if (codelen == 4) { // 该NALU是一帧的开始
            index++;
            //printf("read %4d rtp frame:%d\n", index, nalulen);
            usleep(1000*1000/H264_FPS);
        }

        rtp_send_h264_nalu(sockfd, client, nalu, nalulen, packet);
        if ((nalu[0] & 0x1f) != 7 && (nalu[0] & 0x1f) != 8)   // SPS、PPS不需要加时间戳
            packet->header.timestamp += 90000/H264_FPS;
    }

    free(packet);
    free(nalu);
    destroy_file_manager(file);
    return 0;
}

int rtp_h264_test(void)
{
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        printf("failed to init socket\n");
        return -1;
    }
    
    client_t client;
    client.ip = strdup(RTP_CLIENT_IP);
    client.rtp_port = RTP_CLIENT_PORT;
    
    rtp_play_h264(sockfd, &client);

    close_socket(sockfd);
    return 0;
}


/************************ For Capture H264 *****************************/
int rtp_play_h264_rt(int sockfd, client_t *client) {
    struct rtp_packet *rtp_pkt = (struct rtp_packet *)malloc(RTP_PACKET_SIZE);
    rtp_header_init(&rtp_pkt->header, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0x88923423);

    pthread_t cap_tid;
    pthread_create(&cap_tid, NULL, capture_video_thread, NULL);

    // 必须等video_que初始化完成，才能执行add_user
    while (video_que == NULL)
        sched_yield(); 
    queue_add_user(video_que, READER_ROLE);

    int index = 0;
    while (1) {
        AVPacket *packet = dequeue(video_que);
        if (packet == NULL) break;

        rtp_send_h264_nalu(sockfd, client, packet->data, packet->size, rtp_pkt);
        rtp_pkt->header.timestamp += 90000/CAMERA_FPS;

        index++;
        //printf("send %4d rtp frame:%d\n", index, packet->size);
        usleep(1000*1000/CAMERA_FPS);
        av_packet_free(&packet);
    }

    free(rtp_pkt);
    if (video_que) // 如果capture线程先退出，则video_que已被销毁
        queue_del_user(video_que, READER_ROLE);
    pthread_cancel(cap_tid);
    pthread_join(cap_tid, NULL);
}



