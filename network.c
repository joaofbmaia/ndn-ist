#include "network.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "defines.h"

/*Gets node list from node server
Write header at a later stage*/
int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList) {
    int fd, n, ret, counter;
    char buffer[BUFFER_SIZE];
    char *token;
    char netBuffer[BUFFER_SIZE];
    char headerBuffer[BUFFER_SIZE];

    int port;
    char ip[INET_ADDRSTRLEN];

    fd_set rfds;
    struct timeval timeout;

    socklen_t addrlen;

    timeout.tv_usec = 0;
    timeout.tv_sec = SEL_TIMEOUT;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    sprintf(buffer, "NODES %s", net);
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    ret = select(fd + 1, &rfds, NULL, NULL, &timeout);
    if (ret == 0) {
        close(fd);
        return -3;
    } else if (ret == -1) {
        close(fd);
        return -1;
    }

    addrlen = sizeof(*nodeServer);
    n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *) nodeServer, &addrlen);
    if (n == -1) {
        close(fd);
        return -1;
    }

    buffer[n] = '\0';

    token = strtok(buffer, "\n");

    if(sscanf(token, "%s %s", headerBuffer, netBuffer) != 2) {
        close(fd);
        return -4;
    } else if(strcmp("NODESLIST", headerBuffer)|| strcmp(net, netBuffer)) {
        return -5;
    }

    counter = 0;
    token = strtok(NULL, "\n");
    while (token != NULL){
        ret = sscanf(token, "%s %d", ip, &port);
        if( ret != 2) {
            return -6;
        }
        if (!inet_pton(AF_INET, ip, (&nodeList[counter].sin_addr))) {
            return -6;
        }

        if (port < 1 || port > 65536) {
            return -6;
        }
        nodeList[counter].sin_port = htons(port);
        
        counter++;
        token = strtok(NULL, "\n");
    }

    return counter;

}

/*Confirms registration on node Server
Write header at a later stage*/
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net) {
    int fd, n;
    char buffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;
    sprintf(buffer, "REG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }
    //get registration message
    return fd;
}