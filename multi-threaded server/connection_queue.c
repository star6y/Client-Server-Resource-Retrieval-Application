#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;

    int num;
    if ((num = pthread_mutex_init(&queue->lock, NULL)) != 0) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(num));
        return -1;
    }
    if ((num = pthread_cond_init(&queue->queue_full, NULL)) != 0) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(num));
        return -1;
    }
    if ((num = pthread_cond_init(&queue->queue_empty, NULL)) != 0) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(num));
        return -1;
    }
    return 0;
}

// add the fd to the queue, but not if shutdown = 1, or if queue at capacity
int connection_enqueue(connection_queue_t *queue, int connection_fd) {
    int num;
    if ((num = pthread_mutex_lock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(num));
        return -1;
    }

    if (queue->shutdown == 0) {
        while (queue->length == CAPACITY && queue->shutdown == 0) { // while shutdown == 0
            if ((num = pthread_cond_wait(&queue->queue_full, &queue->lock)) != 0) {
                fprintf(stderr, "pthread_cond_wait: %s\n", strerror(num));
                return -1;
            }
        }
        if (queue->shutdown != 0) {
            pthread_mutex_unlock(&queue->lock); // no error check
            return -1;              // because error already returned
        }

        queue->client_fds[queue->write_idx] = connection_fd;
        queue->length++;
        queue->write_idx++;
        queue->write_idx = queue->write_idx % CAPACITY;
        if ((num = pthread_cond_signal(&queue->queue_empty)) != 0) {
            fprintf(stderr, "pthread_cond_signal: %s\n", strerror(num));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    if (queue->shutdown != 0) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    
    if ((num = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(num));
        return -1;
    }
    return 0;
}

// read fd from queue, and return it
int connection_dequeue(connection_queue_t *queue) {
    int fd;
    int num;
    if ((num = pthread_mutex_lock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(num));
        return -1;
    }

    if (queue->shutdown == 0 ) {
        while (queue->length == 0 && queue->shutdown == 0) { // and while shutdown == 0
            if ((num = pthread_cond_wait(&queue->queue_empty, &queue->lock)) != 0) {
                fprintf(stderr, "pthread_cond_wait: %s\n", strerror(num));
                return -1;
            }       
        }
        if (queue->shutdown != 0) {
            pthread_mutex_unlock(&queue->lock);
            return -1;
        } 
        
        queue->length--;
        fd = queue->client_fds[queue->read_idx];
        queue->read_idx++;
        queue->read_idx = queue->read_idx % CAPACITY;

        if ((num = pthread_cond_signal(&queue->queue_full)) != 0) {
            fprintf(stderr, "pthread_cond_signal: %s\n", strerror(num));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    if (queue->shutdown != 0) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }    
    if ((num = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(num));
        return -1;
    }
    return fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    int num;
    if ((num = pthread_mutex_lock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(num));
        return -1;
    }
    queue->shutdown++;
    if ((num = pthread_cond_broadcast(&queue->queue_empty)) != 0) {
        perror("broadcast q full");
        return -1;
    }
    if ((num = pthread_cond_broadcast(&queue->queue_full)) != 0) {
        perror("broadcast q empty");
        return -1;
    }   
    if ((num = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(num));
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    if (pthread_mutex_destroy(&queue->lock) != 0) {
        perror("destroy");
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_full) != 0) {
        perror("destroy");
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_empty) != 0) {
        perror("destroy");
        return -1;
    }
    
    return 0;
}