#include "network.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "defines.h"
#include "neighbours.h"

/*Executes command join
Write complete header at a later stage*/
int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours) {
    int nodeListSize = 1;
    int err;
    struct sockaddr_in nodeList[MAX_LIST_SIZE];

    //gets node list from node server
    if (!neighbours->external.addrressInfo.sin_family) {  //if sin_family == 0, means nodeExtern has not been set. when set, sin_family = AF_INET (may have been set by join boot mode)
        nodeListSize = getNodeList(nodeServer, net, nodeList);
        if (nodeListSize < 0) {
            return nodeListSize;
        }
    }

    //if the net is empty register the node as his own recovery and external node
    if (nodeListSize == 0) {
        neighbours->external.addrressInfo = neighbours->self.addrressInfo;
        neighbours->recovery.addrressInfo = neighbours->self.addrressInfo;

    } else {
        //connects to external neighbour in the net
        neighbours->external.fd = connectToNodeExtern(nodeListSize, nodeList, &neighbours->external.addrressInfo);
        if (neighbours->external.fd < 0) {
            return neighbours->external.fd;
        }

        //advertise messages to be implemented
        // get recoveryNode
    }
    // outras macacadas que sejam necessárias

    /*

    //opens TCP  server so other nodes can conncect to you
    *listeningFd = socket(AF_INET, SOCK_STREAM, 0);
    if(*listeningFd == -1) {
        return *listeningFd;
    }
    //binds listening fd to the port in nodeSelf
    err = bind(*listeningFd, (struct sockaddr *) nodeSelf, sizeof(*nodeSelf));
    if(err == -1) {
        return err;
    }
    //starts to listen 
    err = listen(*listeningFd, 5);
    if(err == -1) {
        return err;
    } 

    */

    // reisters with node server
    err = reg(&neighbours->self.addrressInfo, nodeServer, net);
    if (err) {
        return err;
    }

    return 0;
}

int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours) {
    int err;

    // mandar mensagens de bye bye
    // fechar fds todos

    memset(&neighbours->external, 0, sizeof neighbours->external);
    memset(&neighbours->recovery, 0, sizeof neighbours->recovery);
    memset(neighbours->internal, 0, sizeof neighbours->internal);
    neighbours->numberOfInternals = 0;

    err = unreg(&neighbours->self.addrressInfo, nodeServer, net);
    if (err) {
        return err;
    }

    return 0;
}

/*Chooses node from node list and connects to external neighbour joining the net
Return value: returns fd of the socket or error code
Write complete header at a later stage */
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct sockaddr_in *nodeExtern) {
    int err, nodeIndex;
    int triesRemaining = MAX_CONNECT_ATTEMPTS;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return fd;

    //If you want to join without choosing the node (join without boot)
    if (!nodeExtern->sin_family) {  //if sin_family == 0, means nodeExtern has not been set. when set, sin_family = AF_INET
        err = -1;
        while (err && triesRemaining) {
            nodeIndex = rand() % nodeListSize;
            err = connect(fd, (struct sockaddr *) &nodeList[nodeIndex], sizeof(nodeList[nodeIndex]));
            triesRemaining--;
        }
        if (err) {
            close(fd);
            return -7;
        }

        //saves external neigbour info
        memcpy(nodeExtern, &nodeList[nodeIndex], sizeof *nodeExtern);

        //else you connect with the specified node
    } else {
        err = connect(fd, (struct sockaddr *) nodeExtern, sizeof(*nodeExtern));
        if (err) {
            close(fd);
            return -8;
        }
    }

    return fd;
}

/*Gets node list from node server
Return Value: List size or error code
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
    //Opens socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    //Sends message asking for the node list
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

    if (sscanf(token, "%s %s", headerBuffer, netBuffer) != 2) {
        close(fd);
        return -4;
    } else if (strcmp("NODESLIST", headerBuffer) || strcmp(net, netBuffer)) {
        return -5;
    }

    counter = 0;
    token = strtok(NULL, "\n");
    while (token != NULL) {
        ret = sscanf(token, "%s %d", ip, &port);
        if (ret != 2) {
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
    int fd, n, ret;
    char buffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    fd_set rfds;
    struct timeval timeout;

    socklen_t addrlen;

    timeout.tv_usec = 0;
    timeout.tv_sec = SEL_TIMEOUT;

    //Opens UDP socket to connect to node server
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    //send registration message to node server
    sprintf(buffer, "REG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    //If the node server takes more than 3 secs to answer
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

    if (strcmp(buffer, "OKREG")) {
        close(fd);
        return -9;
    }

    close(fd);
    return 0;
}

/*Confirms unregistration on node Server
Write header at a later stage*/
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net) {
    int fd, n, ret;
    char buffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    fd_set rfds;
    struct timeval timeout;

    socklen_t addrlen;

    timeout.tv_usec = 0;
    timeout.tv_sec = SEL_TIMEOUT;

    //Opens UDP socket to connect to node server
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    //send unregistration message to node server
    sprintf(buffer, "UNREG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    //If the node server takes more than 3 secs to answer too bad
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

    if (strcmp(buffer, "OKUNREG")) {
        close(fd);
        return -9;
    }

    close(fd);
    return 0;
}