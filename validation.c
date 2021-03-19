#include "validation.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_REGUDP 59000
#define DEFAULT_REGIP "193.136.138.142"

void argError(void);

void argumentParser(int argc, char *argv[], struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer) {
    int port;

    memset(nodeSelf, 0, sizeof *nodeSelf);
    memset(nodeServer, 0, sizeof *nodeServer);

    if (argc < 3) {
        errno = EINVAL;
    } else if (argc > 5) {
        errno = E2BIG;
    } else {
        errno = 0;
    }

    if (errno) {
        argError();
    }

    if (!inet_pton(AF_INET, argv[1], &(nodeSelf->sin_addr))) {
        errno = EINVAL;
        argError();
    }

    if (sscanf(argv[2], "%d", &port) == 1) {
        if (port < 1 || port > 65536) {
            errno = EINVAL;
            argError();
        }
    } else {
        errno = EINVAL;
        argError();
    }

    nodeSelf->sin_port = port;
    nodeSelf->sin_family = AF_INET;

    if (argc < 5) {
        nodeServer->sin_port = DEFAULT_REGUDP;
    } else {
        if (sscanf(argv[4], "%d", &port) == 1) {
            if (port < 1 || port > 65536) {
                errno = EINVAL;
                argError();
            }
        } else {
            errno = EINVAL;
            argError();
        }
    }

    if (argc < 4) {
        inet_pton(AF_INET, DEFAULT_REGIP, &(nodeServer->sin_addr));
    } else {
        if (!inet_pton(AF_INET, argv[3], &(nodeServer->sin_addr))) {
            errno = EINVAL;
            argError();
        }
    }

    nodeServer->sin_family = AF_INET;
    return;
}

void argError(void) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    fprintf(stderr, "usage: ndn IP TCP [regIP] [regUDP]\n");
    exit(errno);
}