#include "validation.h"
#include "commandCodes.h"
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

    nodeSelf->sin_port = htons(port);
    nodeSelf->sin_family = AF_INET;

    if (argc < 5) {
        port = DEFAULT_REGUDP;
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

    nodeServer->sin_port = htons(port);

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

int commandParser(char *buffer, char *net, char *id, char *name, struct sockaddr_in *nodeExtern) {
    char tokens[6][256];
    int tokenCount;

    int port;
    
    tokenCount = sscanf(buffer, "%s %s %s %s %s %s", tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);

    if (tokenCount == EOF) {
        return CC_ERROR;
    }

    if (!strcmp(tokens[0], "join")) {
        if (tokenCount == 3) {
            strcpy(net, tokens[1]);
            strcpy(id, tokens[2]);
            return CC_JOIN;
        } else if (tokenCount == 5) {
            strcpy(net, tokens[1]);
            strcpy(id, tokens[2]);
            if (!inet_pton(AF_INET, tokens[3], &nodeExtern->sin_addr)) {
                fprintf(stderr, "%s", "error: invalid IP format\n");
                fprintf(stderr, "%s", "usage: join net id [bootIP bootTCP]\n");
                return CC_ERROR;
            }
            if (sscanf(tokens[4], "%d", &port) == 1) {
                if (port < 1 || port > 65536) {
                    fprintf(stderr, "%s", "error: invalid TCP port\n");
                    fprintf(stderr, "%s", "usage: join net id [bootIP bootTCP]\n");
                    return CC_ERROR;
                }
            } else {
                fprintf(stderr, "%s", "error: invalid TCP port\n");
                fprintf(stderr, "%s", "usage: join net id [bootIP bootTCP]\n");
                return CC_ERROR;
            }
            nodeExtern->sin_port = port;
            nodeExtern->sin_family = AF_INET;
            return CC_JOINBOOT;
        } else {
            fprintf(stderr, "%s", "error: invalid format\n");
            fprintf(stderr, "%s", "usage: join net id [bootIP bootTCP]\n");
        }

    } else if (!strcmp(tokens[0], "create")) {
        if (tokenCount == 2) {
            strcpy(name, tokens[1]);
            return CC_CREATE;
        } else {
            fprintf(stderr, "%s", "error: invalid format\n");
            fprintf(stderr, "%s", "usage: create subname\n");
        }
    } else if (!strcmp(tokens[0], "get")) {
        if (tokenCount == 2) {
            if(!strchr(tokens[1], '.')) {
                fprintf(stderr, "%s", "error: invalid name format\n");
                fprintf(stderr, "%s", "usage: get id.subname\n");
                return CC_ERROR;
            }
            strcpy(name, tokens[1]);
            return CC_GET;
        } else {
            fprintf(stderr, "%s", "error: invalid format\n");
            fprintf(stderr, "%s", "usage: get name\n");
        }
    } else if (!strcmp(tokens[0], "show")) {
        if (tokenCount != 2) {
            fprintf(stderr, "%s", "error: invalid format\n");
            fprintf(stderr, "%s", "usage: show topology,routing,cache\n");
        } else if (!strcmp(tokens[1], "topology")){
            return CC_SHOWTOPOLOGY;
        } else if (!strcmp(tokens[1], "routing")){
            return CC_SHOWROUTING;
        } else if (!strcmp(tokens[1], "cache")){
            return CC_SHOWCACHE;
        } else {
            fprintf(stderr, "%s", "error: invalid format\n");
            fprintf(stderr, "%s", "usage: show topology,routing,cache]\n");
        }

    } else if (!strcmp(tokens[0], "leave")) {
        return CC_LEAVE;
    } else if (!strcmp(tokens[0], "exit")) {
        return CC_EXIT;
    } else if (!strcmp(tokens[0], "st")) {
        return CC_SHOWTOPOLOGY;
    } else if (!strcmp(tokens[0], "sr")) {
        return CC_SHOWROUTING;
    } else if (!strcmp(tokens[0], "sc")) {
        return CC_SHOWCACHE;
    }
    else {
        fprintf(stderr, "%s", "error: unknown command\n");
    }

    return CC_ERROR;
}
