#ifndef __PACKET_QUEUE_H
#define __PACKET_QUEUE_H

#include <pthread.h>
#define MAX_QUEUE 256
typedef void * Elem_t;

#define READER_ROLE 1   // 读者
#define WRITER_ROLE 2   // 写者


// 只提供数据管理机制，并不实际创建和销毁结构
struct myqueue {
    int head;
    int tail;
    int size;
    Elem_t buffer[MAX_QUEUE];
    pthread_mutex_t mut;    // 支持多线程，保证数据同步
    pthread_cond_t cond;

    int reader_count;
    int writer_count;
};



struct myqueue *create_queue_buffer(void);
void destroy_queue_buffer(struct myqueue *q);

int queue_add_user(struct myqueue *q, int id);
int queue_del_user(struct myqueue *q, int id);

int enqueue(struct myqueue *q, Elem_t t);
Elem_t dequeue(struct myqueue *q);


#endif
