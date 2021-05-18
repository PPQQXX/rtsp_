#include <stdio.h>
#include <stdlib.h>
#include "packet_queue.h"

/*
struct myqueue {
    int head;
    int tail;
    int size;
    Elem_t buffer[MAX_QUEUE];
    pthread_mutex_t mut;    // 支持多线程，保证数据同步
    pthread_cond_t cond;
};
*/

struct myqueue *create_queue_buffer(void) {
    struct myqueue *q = malloc(sizeof(*q));
    q->head = q->tail = q->size = 0;
    for (int i = 0; i < MAX_QUEUE; i++) 
        q->buffer[i] = NULL;
    pthread_mutex_init(&q->mut, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

static int next_pos(int pos) {
    return (pos + 1) % MAX_QUEUE;
}

void queue_wakeup(struct myqueue *q) {
    pthread_cond_broadcast(&q->cond);
}

// 虽然加了读者写者的条件，但因为一直没有数据变化的广播，所以无法唤醒退出
// 一直卡在pthread_cond_wait，可以考虑添加一个检测读者/写者是否都存在的线程
// 如果有一方缺失，则调用pthread_cond_broadcast，唤醒被阻塞的线程
int enqueue(struct myqueue *q, Elem_t t, int reader_count) {
    int res = 0;
    pthread_mutex_lock(&q->mut);

    // 缓冲区为满且还有读者存在时，等待才有意义
    while (q->size == MAX_QUEUE && reader_count > 0) {
        pthread_cond_wait(&q->cond, &q->mut);
    }
    if (reader_count != 0) {  
        q->buffer[q->tail] = t;
        q->tail = next_pos(q->tail);
        q->size++;
        pthread_cond_broadcast(&q->cond);
    } else {
        res = -1;
    }

    pthread_mutex_unlock(&q->mut);
    return res;
}

Elem_t dequeue(struct myqueue *q, int writer_count) {
    Elem_t res;
    pthread_mutex_lock(&q->mut);
   
    // 缓冲区为空且还有写者存在时，等待才有意义
    while (q->size == 0 && writer_count > 0) {
        pthread_cond_wait(&q->cond, &q->mut);
    }

    if (q->size > 0) {   // 缓冲区还有数据 
        res = q->buffer[q->head];   
        q->buffer[q->head] = NULL;  // 避免野指针
        q->head = next_pos(q->head);
        q->size--;
        pthread_cond_broadcast(&q->cond);
    } else {    
        res = NULL;
    }
    
    pthread_mutex_unlock(&q->mut);
    return res;
}

void destroy_queue_buffer(struct myqueue *q) {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (q->buffer[i]) free(q->buffer[i]);
    }
    pthread_mutex_destroy(&q->mut);
    pthread_cond_destroy(&q->cond);
    free(q);
}


