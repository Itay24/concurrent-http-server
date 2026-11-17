#include "http.h"
#include "server.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h> 



int main(void) {
    return run_server();
}