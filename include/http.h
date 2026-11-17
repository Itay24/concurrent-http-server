#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <sys/types.h>

ssize_t send_all(int fd, const void *buffer, size_t length);
int serve_file(int client_fd, const char *filepath);

#endif