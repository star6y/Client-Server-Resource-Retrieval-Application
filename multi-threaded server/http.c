#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    char buf[BUFSIZE];
    if (read(fd, buf, BUFSIZE) < 0) {
        perror("read request error");
        return -1;
    }

    char dir[60];
    char *ptr;
    char *token = strtok_r(buf, " ", &ptr); // use strtok_r for tread safe signals/rentry
    token = strtok_r(NULL, " ", &ptr);  //skip the GET
    if (token == NULL) {
        perror("using strtok");
        return -1;
    }

    strcpy(dir, resource_name);
    strcat(dir, token);
    strcpy(resource_name, dir);

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    char header_error[BUFSIZ] = {"HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n"};

    struct stat statbuf;
    if (stat(resource_path, &statbuf) != 0) {
        write(fd, header_error, BUFSIZE);   // if file doesn't exist, write error header to fd
        perror("getting size");
        return -1;
    }
    int size = statbuf.st_size;
    char size_buf[20];
    sprintf(size_buf, "%d", size); //make int size to char size, with base 10

    char buf[BUFSIZE];
    char p = '.';
    char *extension = strchr(resource_path, p); // get file mime

    char header[BUFSIZ] = {"HTTP/1.0 200 OK\r\nContent-Type: "};
    strcat(header, get_mime_type(extension));   // build the 200 OK header
    strcat(header, "\r\nContent-Length: ");
    strcat(header, size_buf);
    strcat(header, "\r\n\r\n");

    if (write(fd, header, strlen(header)) == -1) {  //write header to fd first
        perror("write error");
        return -1;      // no file was opened, no file to close 
    }

    FILE *f = fopen(resource_path, "r");
    if (f == NULL) {
        perror("can't open file");
        return -1;
    }

    int read;
    while ((read = fread(buf,sizeof(char), BUFSIZE, f)) > 0) {
        if(write(fd, buf, BUFSIZE) < 0) {
            fclose(f);
            perror("writing");
            return -1;
        }
    }
    if (read < 0) { // if read error occurred, while loop above would 
        fclose(f);  // stop, then we error check here
        perror("reading");
        return -1;
    }

    if (fclose(f) != 0) {
        perror("closing file");
        return -1;
    }
    return 0;
}
