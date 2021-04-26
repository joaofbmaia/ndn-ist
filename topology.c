#include "topology.h"
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
#include "utils.h"

int openListener(struct neighbours *neighbours);
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct neighbours *neighbours);
int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList);

/*Executes command join
Write complete header at a later stage*/
int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours) {
    int nodeListSize = 1;
    int err;
    struct sockaddr_in nodeList[MAX_LIST_SIZE];
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

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

        // starts listener
        err = openListener(neighbours);
        if (err) {
            return err;
        }

        // registers with node server
        err = reg(&neighbours->self.addrressInfo, nodeServer, net);
        if (err) {
            return err;
        }

    } else {
        //connects to external neighbour in the net
        err = connectToNodeExtern(nodeListSize, nodeList, neighbours);
        if (err) {
            memset(&neighbours->external, 0, sizeof neighbours->external);
            return err;
        }

        //creates buffer with message to be written
        sprintf(writeBuffer, "NEW %s %d\n", inet_ntop(AF_INET, &neighbours->self.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->self.addrressInfo.sin_port));

        // send NEW
        err = writeBufferToTcpStream(neighbours->external.fd, writeBuffer);

        if (err) {
            close(neighbours->external.fd);
            memset(&neighbours->external, 0, sizeof neighbours->external);
            return err;
        }
    }

    return 0;
}

int openListener(struct neighbours *neighbours) {
    int err;

    //opens TCP  server so other nodes can conncect to you
    neighbours->self.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (neighbours->self.fd == -1) {
        return neighbours->self.fd;
    }
    //binds listening fd to the port in nodeSelf
    err = bind(neighbours->self.fd, (struct sockaddr *) &neighbours->self.addrressInfo, sizeof(neighbours->self.addrressInfo));
    if (err == -1) {
        return err;
    }
    //starts to listen
    err = listen(neighbours->self.fd, 5);
    if (err == -1) {
        return err;
    }

    return 0;
}

/*Executes leave command
Write complete header at a later stage*/
int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours) {
    int err;

    //close fds
    close(neighbours->self.fd);
    close(neighbours->external.fd);
    for (int i  = 0; i < neighbours->numberOfInternals; i++) {
        close(neighbours->internal[i].fd);
    }

    //reset neighbours
    memset(&neighbours->external, 0, sizeof neighbours->external);
    memset(&neighbours->recovery, 0, sizeof neighbours->recovery);
    memset(neighbours->internal, 0, sizeof neighbours->internal);
    neighbours->self.fd = 0;
    neighbours->numberOfInternals = 0;

    //unregister from node server
    err = unreg(&neighbours->self.addrressInfo, nodeServer, net);
    if (err) {
        return err;
    }

    return 0;
}

/*Processes NEW message in loneRegister case
Write complete header at a later stage*/
int loneNewInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo) {
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    int err;

    neighbours->internal[internalIndex].addrressInfo = *addrinfo;

    //promotes this internal to external
    neighbours->external.addrressInfo = neighbours->internal[internalIndex].addrressInfo;

    //sends n info as recovery node
    sprintf(writeBuffer, "EXTERN %s %d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));

    err = writeBufferToTcpStream(neighbours->internal[internalIndex].fd, writeBuffer);

    if (err) {
        memset(&neighbours->external.addrressInfo, 0, sizeof neighbours->external.addrressInfo);
        /* close fd and remove node from internalNodes vector */
    }

    return err;
}

/*Processes NEW message in Register case
Write complete header at a later stage*/
int newInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo) {
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    int err;

    neighbours->internal[internalIndex].addrressInfo = *addrinfo;

    sprintf(writeBuffer, "EXTERN %s %d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));

    err = writeBufferToTcpStream(neighbours->internal[internalIndex].fd, writeBuffer);

    if (err) {
        /* close fd and remove node from internalNodes vector */
    }

    return err;
}

int externMessageHandler(struct neighbours *neighbours, struct sockaddr_in *addrinfo, struct sockaddr_in *nodeServer, char *net) {
    int err;

    //set recovery info
    neighbours->recovery.addrressInfo = *addrinfo;

    // starts listener
    err = openListener(neighbours);
    if (err) {
        return err;
    }

    // registers with node server
    err = reg(&neighbours->self.addrressInfo, nodeServer, net);
    if (err) {
        return err;
    }

    return 0;
}

/*Chooses node from node list and connects to external neighbour joining the net
Return value: returns error code
Write complete header at a later stage */
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct neighbours *neighbours) {
    int err, nodeIndex;
    int triesRemaining = MAX_CONNECT_ATTEMPTS;

    neighbours->external.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (neighbours->external.fd == -1) return neighbours->external.fd;

    //If you want to join without choosing the node (join without boot)
    if (!neighbours->external.addrressInfo.sin_family) {  //if sin_family == 0, means nodeExtern has not been set. when set, sin_family = AF_INET
        err = -1;
        while (err && triesRemaining) {
            nodeIndex = rand() % nodeListSize;
            err = connect(neighbours->external.fd, (struct sockaddr *) &nodeList[nodeIndex], sizeof(nodeList[nodeIndex]));
            triesRemaining--;
        }
        //exceed number of tries
        if (err) {
            close(neighbours->external.fd);
            return -7;
        }

        //saves external neigbour info
        neighbours->external.addrressInfo = nodeList[nodeIndex];

        //else you connect with the specified node
    } else {
        err = connect(neighbours->external.fd, (struct sockaddr *) &neighbours->external.addrressInfo, sizeof(neighbours->external.addrressInfo));
        //error connecting to the specified node
        if (err) {
            close(neighbours->external.fd);
            return -8;
        }
    }

    return 0;
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

        nodeList[counter].sin_family = AF_INET;

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

    //error in case of invalid response from node server
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

void showTopology(struct neighbours *neighbours) {
    char addrBuffer[INET_ADDRSTRLEN];

    printf("external node: %s:%d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));
    printf("recovery node: %s:%d\n", inet_ntop(AF_INET, &neighbours->recovery.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->recovery.addrressInfo.sin_port));
}