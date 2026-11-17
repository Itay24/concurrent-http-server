#include "server.h"
#include "http.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include "thread_pool.h"
#include <signal.h>
#include <poll.h>
#include <fcntl.h>



static void handle_client(int client_fd) {

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };

        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                    &timeout, sizeof(timeout)) == -1) {
            perror("setsockopt timeout failed");
            close(client_fd);
            return;
        }

        printf("Client connected!\n");

        char buffer[4096];

        size_t total_received = 0;
        bool headers_complete = false;
        ssize_t add_rec = 0;
        bool receive_failed = false;

        while (!headers_complete && total_received < sizeof(buffer) - 1) { //handling partial recieves
            
            add_rec = recv(client_fd, buffer + total_received, sizeof(buffer)-1 - total_received, 0);

            if (add_rec == 0) {
                    printf("Client disconnected before completing request\n");
                    receive_failed = true;
                    break;
                }

            if (add_rec < 0) {

                if (errno == EINTR) {
                    continue;
                }

                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Client timed out\n");
                }else {
                    perror("recv failed");
                }

                receive_failed = true;
                break;
            }
         
            total_received += (size_t)add_rec;
            buffer[total_received] = '\0';

            if (strstr(buffer, "\r\n\r\n") != NULL) {
                headers_complete = true;
            }

        }



        if (receive_failed) {
            close(client_fd);
            return;
        }

       if (!headers_complete) {
            const char *response =
                "HTTP/1.1 431 Request Header Fields Too Large\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 17\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Headers Too Large";

            if (send_all(client_fd, response, strlen(response)) == -1) {
                perror("send failed");
            }

            close(client_fd);
            return;
}

        bool valid_request = false;

        char *endof_line = strstr(buffer, "\r\n");

        if (endof_line == NULL) {
            printf("Invalid request line\n");
            close(client_fd);
            return;
        }

        size_t request_length = (size_t)(endof_line - buffer);

        if (request_length > 512) {
            printf("Request line too long\n");
            close(client_fd);
            return;
        }

        char request_line[513];
        memcpy(request_line, buffer, request_length);
        request_line[request_length] = '\0';

        char method[16];
        char path[256];
        char version[16];
        char extra; //we'll check for extra characters



            int parsed = sscanf(request_line, "%15s %255s %15s %c",
                                method, path, version, &extra); //separating request into buffers. numbers prevent buffer overflow
        
            if (parsed == 3) {
                valid_request = true;
                if (strcmp(version, "HTTP/1.1") != 0 &&
                    strcmp(version, "HTTP/1.0") != 0) {
                    printf("Unsupported HTTP version\n");
                    valid_request = false;
                }

                if (path[0] != '/'){
                    printf("Unsupported HTTP path\n");
                    valid_request = false;
                }
                printf("Method: %s\n", method);
                printf("Path: %s\n", path);
                printf("Version: %s\n", version);
            }
            else {
                printf("Invalid HTTP request\n");
            }

        const char *body;
        const char *status = "200 OK";
        const char *allowed_method = "";

        if (!valid_request) {
            body = "Bad Request";
            status = "400 Bad Request";
        }

        else if (strcmp(method, "GET") != 0) {
            body = "Method Not Allowed";
            status = "405 Method Not Allowed";
            allowed_method = "Allow: GET\r\n";
        }

        else if (strcmp(path, "/") == 0) {
            body = "Hello, World!";
        }

        else if (strcmp(path, "/index.html") == 0) {
            if (serve_file(client_fd, "www/index.html") == -1) {
                fprintf(stderr, "Failed to serve HTML file\n");
            }

            close(client_fd);
            return;
        }


        else if (strcmp(path, "/hello") == 0) {
            body = "Welcome to my HTTP server!";
        }
        else if (strcmp(path, "/about") == 0) {
            body = "Built using C and Linux.";
        }
        else {
            body = "404 Not Found";
            status = "404 Not Found";
        }


        char response[4096];

        int response_len = snprintf(response, sizeof(response),
            "HTTP/1.1 %s\r\n"
            "%s"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            status,
            allowed_method,
            strlen(body),
            body
        );

        if (response_len < 0 ||
            (size_t)response_len >= sizeof(response)) {
            fprintf(stderr, "Response formatting failed\n");
            close(client_fd);
            return;
        }

        ssize_t bytes_sent = send_all(client_fd, response, (size_t)response_len);

        if (bytes_sent == -1) {
            perror("send failed");
        } else {
            printf("Sent %zd bytes\n", bytes_sent);
        }

        close(client_fd);
}


static void *pool_worker(void *arg) {
    connection_queue *queue = (connection_queue *)arg;

    while (true) {
        int client_fd = queue_pop(queue);

        if (client_fd == -1) {
            break;
        }
        handle_client(client_fd);
    }

    return NULL;
}

static void *signal_worker(void *arg) {
    int write_fd = *(int *)arg;

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGUSR1);

    int received_signal;

    int result = sigwait(&signals, &received_signal);

    if (result != 0) {
        fprintf(stderr, "sigwait failed: %s\n", strerror(result));

        // Notify the main thread that the signal worker failed.
        char notification = 'E';
        if (write(write_fd, &notification, 1) == -1) {
            perror("notification failed");
        }
        return NULL;
    }

    if (received_signal == SIGUSR1) {
        return NULL;
    }

    char notification = 'X';

    if (write(write_fd, &notification, 1) == -1) {
        perror("shutdown notification failed");
    }

    return NULL;
}

int run_server(void) {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1){
        perror("socket failure");
        return 1;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
}
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr)) == -1) {
        perror("bind failure");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen failure");
        close(server_fd);
        return 1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);

    if (flags == -1 ||
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl failed");
        close(server_fd);
        return 1;
    }

    printf("Waiting for a client...\n");

    int shutdown_pipe[2];

    if (pipe(shutdown_pipe) == -1) {
        perror("pipe failed");
        close(server_fd);
        return 1;
    }

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGUSR1);

    int result = pthread_sigmask(SIG_BLOCK, &signals, NULL);

    if (result != 0) {
        fprintf(stderr, "pthread_sigmask failed: %s\n",
                strerror(result));

        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        close(server_fd);
        return 1;
    }

    connection_queue queue;

    if (queue_init(&queue) != 0) {
        fprintf(stderr, "Queue initialization failed\n");

        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        close(server_fd);

        return 1;
    }

    pthread_t workers[WORKER_NUM];

    int created_workers = 0;

    for (int i = 0; i < WORKER_NUM; i++) {
        int worker = pthread_create(
            &workers[i],
            NULL,
            pool_worker,
            &queue
        );
        

        if (worker != 0) {
            fprintf(stderr, "Worker creation failed: %s\n",
                    strerror(worker));
            break;
        }

        created_workers++;
}

    if (created_workers != WORKER_NUM) {
        queue_stop(&queue);

        for (int i = 0; i < created_workers; i++) {
            pthread_join(workers[i], NULL);
        }

        queue_destroy(&queue);
        close(shutdown_pipe[0]);
        close(shutdown_pipe[1]);
        close(server_fd);
        return 1;
    }

    pthread_t signal_thread;

int signal_result = pthread_create(
    &signal_thread,
    NULL,
    signal_worker,
    &shutdown_pipe[1]
);

if (signal_result != 0) {
    fprintf(stderr, "Signal thread creation failed: %s\n",
            strerror(signal_result));

    queue_stop(&queue);

    for (int i = 0; i < created_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    queue_destroy(&queue);
    close(shutdown_pipe[0]);
    close(shutdown_pipe[1]);
    close(server_fd);

    return 1;
}


    struct pollfd fds[2];

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    fds[1].fd = shutdown_pipe[0];
    fds[1].events = POLLIN; 

    bool shutdown_requested = false;

while (true) {
    int ready = poll(fds, 2, -1);

    if (ready == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("poll failed");
        break;
    }

    if (fds[1].revents & POLLIN) {
        char notification;

        ssize_t bytes_read = read(
            shutdown_pipe[0],
            &notification,
            1
        );

        if (bytes_read == 1 && notification == 'X') {
            printf("Shutdown requested!\n");
            shutdown_requested = true;
        } else {
            fprintf(stderr, "Signal worker error\n");
        }

        break;
    }   
    if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        fprintf(stderr, "Shutdown pipe error\n");
        break;
    }

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        fprintf(stderr, "Listening socket error\n");
        break;
    }

    if (fds[0].revents & POLLIN) {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR ||
                errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                continue;
            }

            perror("accept failure");
            break;
        }

        if (queue_push(&queue, client_fd) != 0) {
            fprintf(stderr, "Connection queue full or stopping\n");
            close(client_fd);
        }
    }
}

    // Stop the signal worker if shutdown wasn't caused by Ctrl+C.
if (!shutdown_requested) {
    int result = pthread_kill(signal_thread, SIGUSR1);

    if (result != 0) {
        fprintf(stderr, "pthread_kill failed: %s\n",
                strerror(result));
    }
}

pthread_join(signal_thread, NULL);


queue_stop(&queue); //stop the connection workers.

for (int i = 0; i < created_workers; i++) {
    pthread_join(workers[i], NULL);
}

// Release resources.
queue_destroy(&queue);

close(shutdown_pipe[0]);
close(shutdown_pipe[1]);
close(server_fd);

return shutdown_requested ? 0 : 1;

}