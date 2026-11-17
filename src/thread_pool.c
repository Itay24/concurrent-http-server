#include "thread_pool.h"
#include <stdio.h>
#include <unistd.h>

int queue_init(connection_queue *queue) {
    queue->next_worker = 0;
    queue->next_place = 0;
    queue->count = 0;
    queue->stopping = false;

    int lock_check = pthread_mutex_init(&queue->mutex, NULL);

    if (lock_check != 0) { //lock_init failed
        return -1;
    }

    lock_check = pthread_cond_init(&queue->not_empty, NULL);

    if (lock_check != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    return 0;
}

int queue_push(connection_queue *queue, int client_fd) {

    pthread_mutex_lock(&queue->mutex);

    if (queue->stopping || queue->count == QUEUE_CAPACITY) { //queue is full or we stop
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    queue->fds[queue->next_place] = client_fd; //inserting to the next place

    queue->next_place = (queue->next_place + 1) % QUEUE_CAPACITY; //moving mext place index
    queue->count++; //added another connection

    pthread_cond_signal(&queue->not_empty); //wakes one worker waiting for a connection, if there's any

    pthread_mutex_unlock(&queue->mutex); //release lock

    return 0;
}

int queue_pop(connection_queue *queue) {  

    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0 && !queue->stopping) { //waiting for a worker to be available
        pthread_cond_wait(&queue->not_empty, &queue->mutex); //giving up the lock
    }

    if (queue->stopping) {
    pthread_mutex_unlock(&queue->mutex);
    return -1;
}

    int client_fd = queue->fds[queue->next_worker]; //taking the oldest connection in the queue

    queue->next_worker = (queue->next_worker + 1) % QUEUE_CAPACITY; //updating the next woker
    queue->count--; //one less connection waiting

    pthread_mutex_unlock(&queue->mutex);

    return client_fd;
}

void queue_stop(connection_queue *queue) {
    pthread_mutex_lock(&queue->mutex);

    queue->stopping = true;

    pthread_cond_broadcast(&queue->not_empty); //waking up other threads

    pthread_mutex_unlock(&queue->mutex);
}

// Call only after all worker threads have been joined
void queue_destroy(connection_queue *queue) {
    pthread_mutex_lock(&queue->mutex);

    while (queue->count > 0) {
        int client_fd = queue->fds[queue->next_worker];

        close(client_fd);

        queue->next_worker = (queue->next_worker + 1) % QUEUE_CAPACITY;
        queue->count--;
    }

    pthread_mutex_unlock(&queue->mutex);

    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
}