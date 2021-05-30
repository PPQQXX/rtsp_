#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "packet_queue.h"

struct myqueue *create_queue_buffer(void) {
    struct myqueue *q = malloc(sizeof(*q));
    q->head = q->tail = q->size = 0;
    for (int i = 0; i < MAX_QUEUE; i++) 
        q->buffer[i] = NULL;
    q->reader_count = 0;
    q->writer_count = 0;
    pthread_mutex_init(&q->mut, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

static int next_pos(int pos) {
    return (pos + 1) % MAX_QUEUE;
}

int enqueue(struct myqueue *q, Elem_t t) {
    pthread_mutex_lock(&q->mut);
    if (q->reader_count <= 0) goto _Error; // 读者不存在，存数据到队列没有意义 

    while (q->size == MAX_QUEUE && q->reader_count > 0) {
        pthread_cond_wait(&q->cond, &q->mut);
    }
    q->buffer[q->tail] = t;
    q->tail = next_pos(q->tail);
    q->size++;
    pthread_cond_broadcast(&q->cond);

_Error:
    pthread_mutex_unlock(&q->mut);
    return 0;
}

Elem_t dequeue(struct myqueue *q) {
    Elem_t res;
    pthread_mutex_lock(&q->mut);
   
    // 缓冲区为空且还有写者存在时，等待才有意义
    while (q->size == 0 && q->writer_count > 0) {
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



int queue_add_user(struct myqueue *q, int id) {
    int res = 0;
    pthread_mutex_lock(&q->mut);
    if (id == READER_ROLE) q->reader_count++;
    else if (id == WRITER_ROLE) q->writer_count++;
    else {  // 身份非法
        res = -1;
    }
    pthread_mutex_unlock(&q->mut);
    return res;
}

int queue_del_user(struct myqueue *q, int id) {
    int res = 0;
    pthread_mutex_lock(&q->mut);
    if (id == READER_ROLE) q->reader_count--;
    else if (id == WRITER_ROLE) q->writer_count--;
    else {  // 身份非法
        res = -1;
    }
    pthread_mutex_unlock(&q->mut);
    return res;

}


