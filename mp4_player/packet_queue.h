#ifndef __PACKET_QUEUE_H
#define __PACKET_QUEUE_H

#include <pthread.h>
#define MAX_QUEUE 128
typedef void * Elem_t;

// 只提供数据管理机制，并不实际创建和销毁结构
struct myqueue {
    int head;
    int tail;
    int size;
    Elem_t buffer[MAX_QUEUE];
    pthread_mutex_t mut;    // 支持多线程，保证数据同步
    pthread_cond_t cond;
};

struct myqueue *create_queue_buffer(void);
int enqueue(struct myqueue *q, Elem_t t);
Elem_t dequeue(struct myqueue *q, int writefinish);
void destroy_queue_buffer(struct myqueue *q);


#endif
