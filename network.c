#include "network.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net) {
    int fd, n;
    char buffer[512];
    char addrBuffer[INET_ADDRSTRLEN];
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;
    sprintf(buffer, "REG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    return fd;
}