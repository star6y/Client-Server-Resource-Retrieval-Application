#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    const char *serve_dir = argv[1];
    const char *port = argv[2];

    // Set up SIFINT for clean up
    struct sigaction sigact;
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
    hints.ai_flags = AI_PASSIVE; // Server
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

    // loop to keep accepting client requests, until signal to shutdown
    while (keep_going != 0) {
        // printf("Waiting for client to connect\n");
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
        // printf("New client connected\n");

        char file[BUFSIZE];
        strcpy(file, serve_dir);

        // read request and write response. If error, close the current fd and 
        // skip this loop, waiting for other clients to connect
        if (read_http_request(client_fd, file) == -1) {
            close(client_fd);
            continue;
        }
        if (write_http_response(client_fd, file) == -1) {
            close(client_fd);
            continue;
        }

        if (close(client_fd) == -1) {
            perror("close");
            close(sock_fd);
            return 1;
        }
    }

    if (close(sock_fd) == -1) {
        perror("close");
        return 1;
    }
    return 0;
}
