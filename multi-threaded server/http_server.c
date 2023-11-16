#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

void *consumer_thread(void *arg) {
    connection_queue_t *que = (connection_queue_t *) arg;
    int fd;
    char file[BUFSIZE];
    while (keep_going == 1) { // maybe the while loop is only one we need
        if (que->shutdown == 1) {
            break;
        }
        // if an error occurs while trying to get the
        // fd, then we skip this iteration
        if ((fd = connection_dequeue(que)) == -1) { 
            continue;   
        }
        
        strcpy(file, serve_dir);
        read_http_request(fd, file);
        write_http_response(fd, file);

        if (close(fd) == -1) {
            perror("close");
        }
    }

    return NULL;
}


int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    serve_dir = argv[1];
    const char *port = argv[2];

    connection_queue_t q;
    connection_queue_init(&q);

    // Set up SIFINT for clean up
    struct sigaction sigact;
    sigset_t set;
    sigset_t old;
    sigact.sa_handler = handle_sigint;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0; 
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // Hints for socket, either IP version 4 or 6
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // We'll be acting as a server
    struct addrinfo *server;

    // getting address ready for the scoket and connection
    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return 1;
    }

    // initializing socket fd
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }

    // bind socket to port it'll receive from
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);

    // make the socket a server type socket
    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sock_fd);
        return 1;
    }

    // block signals for the threads that will get initiated below
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, &old);
    int result;
    pthread_t consumer[N_THREADS];

    // make n consumer threads that will consume queue content
    for (int i = 0; i < N_THREADS; i++) {
        if ((result = pthread_create(consumer + i, NULL, consumer_thread, &q)) != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            return 1;
        }
    }
    
    // reset signal handling so the main thread will receive them
    sigprocmask(SIG_SETMASK, &old, NULL); 

    // the producer will loop to accept client requests, and add 
    // them to the queue
    while (keep_going != 0) {
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sock_fd);
                return 1;
            } else {
                break;
            }
        }
        if (connection_enqueue(&q, client_fd) ) {
            continue;   // the response would include a 404 error
        }               // 
    }

    // if any clean up step fails, it will be recorded in exit_code but
    // all other clean up functions will continue to run regardless
    int exit_code = 0;
    if (connection_queue_shutdown(&q) == -1) {
        perror("can't shutdown");
        exit_code = 1;
    }
    
    for (int i = 0; i < N_THREADS; i++) {
        if ((result = pthread_join(consumer[i], NULL)) != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(result));
            exit_code = 1;
        }
    }
    if (connection_queue_free(&q) != 0) {
        perror("problem with free");
        exit_code = 1;
    }

    if (close(sock_fd) == -1) {
        perror("close fd");
        exit_code = 1;
    }
    return exit_code;
}
