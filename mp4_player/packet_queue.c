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

int enqueue(struct myqueue *q, Elem_t t) {
    pthread_mutex_lock(&q->mut);
    
    while (q->size == MAX_QUEUE) {
        pthread_cond_wait(&q->cond, &q->mut);
    }
    q->buffer[q->tail] = t;
    q->tail = next_pos(q->tail);
    q->size++;

    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mut);
    return 0;
}

Elem_t dequeue(struct myqueue *q, int writefinish) {
    Elem_t res = NULL;
    pthread_mutex_lock(&q->mut);
    
    while (q->size == 0 && writefinish == 0) {
        pthread_cond_wait(&q->cond, &q->mut);
    }

    if (q->size != 0) { // buffer中有内容
        res = q->buffer[q->head];
        q->head = next_pos(q->head);
        q->size--;
        pthread_cond_broadcast(&q->cond);
    } else {    // buffer中没有内容且写操作已完成
        // do nothing 
    }
    
    pthread_mutex_unlock(&q->mut);
    return res;
}

void destroy_queue_buffer(struct myqueue *q) {
    pthread_mutex_destroy(&q->mut);
    pthread_cond_destroy(&q->cond);
    free(q);
}


