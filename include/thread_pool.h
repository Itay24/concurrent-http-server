#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

#define WORKER_NUM 4
#define QUEUE_CAPACITY 64

typedef struct {
    int fds[QUEUE_CAPACITY];

    int next_worker; //next connection we remove
    int next_place; //place for next connection
    int count; //of connection waiting
    bool stopping;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} connection_queue;

int queue_init(connection_queue *queue);
int queue_push(connection_queue *queue, int client_fd);
int queue_pop(connection_queue *queue);
void queue_stop(connection_queue *queue);
void queue_destroy(connection_queue *queue);

#endif

