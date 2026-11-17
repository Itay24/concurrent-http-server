#include "http.h"

#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


ssize_t send_all(int fd, const void *buffer, size_t length) {
    const char *data = buffer;
    size_t total_sent = 0;

    while (total_sent < length) {
        ssize_t to_add = send(fd, data + total_sent, length - total_sent, MSG_NOSIGNAL);

        if (to_add == -1) {
            if (errno == EINTR) { //means the system call was interrupted by a signal
                continue; //retrying
            }
            return -1;
}
        if (to_add == 0) {
            return -1;
}

        total_sent = total_sent + to_add;
    }

    return (ssize_t)total_sent;


    }

    int serve_file(int client_fd, const char *filepath) {
        FILE *file = fopen(filepath, "rb");

       if (file == NULL) {
            perror("fopen");

            const char *response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 14\r\n"
                "Connection: close\r\n"
                "\r\n"
                "File Not Found";

            if (send_all(client_fd, response, strlen(response)) == -1) {
                perror("send failed");
                return -1;
            }

            return 0;
        }

        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return -1;
        }

        long file_size = ftell(file);

        if (file_size < 0){
            fclose(file);
            return -1;
        }

        if (fseek(file, 0, SEEK_SET) != 0){
            fclose(file);
            return -1;
        }

        char header[512];

        int header_len = snprintf(
            header,
            sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            file_size
        );

        if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
            fclose(file);
            return -1;
        }

        if (send_all(client_fd, header, (size_t)header_len) == -1) {
            fclose(file);
            return -1;
        }

        char buffer[4096];

        size_t curr_read = 0;

        size_t expected_size = (size_t)file_size;
        while (curr_read < expected_size) {

            size_t rem = expected_size - curr_read;
            size_t to_read;

            if (rem < sizeof(buffer)) {
                to_read = rem;
            }else {
                to_read = sizeof(buffer);
            }
            size_t to_add = fread(buffer, 1, to_read, file);

            if (to_add == 0) {
                if (ferror(file)) {
                    perror("fread failed");
                } else {
                    printf("Unexpected end of file\n");
                }

                fclose(file);
                return -1;
            }

             if (send_all(client_fd, buffer, to_add) == -1) {
                perror("send_all failed");
                fclose(file);
                return -1;
            }
            curr_read += to_add; 
        }

        fclose(file);
        return 0;
    }