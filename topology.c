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
#include "routing.h"
#include "states.h"
#include "utils.h"
#include "search.h"

int openListener(struct neighbours *neighbours);
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct neighbours *neighbours);
int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList);

/******************************************************************************
 * join()
 *
 * Arguments: nodeServer - Address of node server 
 *            net - net name 
 *            neighbours - struct with all topology information
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Executes the first part of registration process by choosing,
 *              connecting and sending message "NEW" to the external neighbour  
 *****************************************************************************/
int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours, char *id, struct routingTable *routingTable) {
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

    //add itself to routing table
    addNodeToRoutingTable(0, id, routingTable);

    return 0;
}

/******************************************************************************
 * openListner()
 *
 * Arguments: neighbours - struct with all topology information
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Opens listening socket. 
 *****************************************************************************/
int openListener(struct neighbours *neighbours) {
    int err;

    //opens TCP  server so other nodes can conncect to you
    neighbours->self.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (neighbours->self.fd == -1) {
        return neighbours->self.fd;
    }

    //binds listening fd to the port in nodeSelf
    struct sockaddr_in portOnly;
    portOnly = neighbours->self.addrressInfo;
    portOnly.sin_addr.s_addr = INADDR_ANY;

    err = bind(neighbours->self.fd, (struct sockaddr *) &portOnly, sizeof(portOnly));
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


/******************************************************************************
 * leave()
 *
 * Arguments: nodeServer - Address of node server 
 *            net - net name 
 *            neighbours - struct with all topology information
 *            routingTable - struct with routing table content
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Executes leave command by terminating all tcp sessions
 *              and reseting all the structers.
 *****************************************************************************/
int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours, struct routingTable *routingTable, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache) {
    int err;

    //close fds
    if (neighbours->self.fd > 0) {
        close(neighbours->self.fd);
    }

    if (neighbours->external.fd > 0) {
        close(neighbours->external.fd);
    }

    for (int i = 0; i < neighbours->numberOfInternals; i++) {
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

    //reset routing table
    memset(routingTable, 0, sizeof *routingTable);
    //mandar o resto das tabelas para dar memeset

    //reset objects
    memset(objectTable, 0, sizeof *objectTable);
    memset(interestTable, 0, sizeof *interestTable);
    memset(cache, 0, sizeof *cache);

    return 0;
}


/******************************************************************************
 * loneNewInternalHandler()
 *
 * Arguments: neighbours - struct with all topology information
 *            internalIndex - index of internal that is the target for the 
 *            addrinfo - Address to be sent in EXTERN message
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Responds to NEW message, in lone register case, by saving 
 *              neighbour information, promoting it to external neighbour
 *              and sending EXTERN message to neighbour trying to connect.
 * 
 *****************************************************************************/
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

    return err;
}


/******************************************************************************
 * newInternalHandler()
 *
 * Arguments: neighbours - struct with all topology information
 *            internalIndex - index of internal that is the target for the 
 *            addrinfo - Address to be sent in EXTERN message
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Responds to NEW message by saving neighbour information and
 *              sending EXTERN message to neighbour. 
 *
 *****************************************************************************/
int newInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo) {
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    int err;

    neighbours->internal[internalIndex].addrressInfo = *addrinfo;

    sprintf(writeBuffer, "EXTERN %s %d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));

    err = writeBufferToTcpStream(neighbours->internal[internalIndex].fd, writeBuffer);

    return err;
}

/******************************************************************************
 * finishJoin()
 *
 * Arguments: neighbours - struct with all topology information
 *            addrinfo - Address recieved from EXTERN message
 *            nodeServer - Address of node server  
 *            net - net name 
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Complete registration process by saving recovery node,
 *              opening listener and registering on node server.
 * 
 *****************************************************************************/
int finishJoin(struct neighbours *neighbours, struct sockaddr_in *addrinfo, struct sockaddr_in *nodeServer, char *net) {
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

/******************************************************************************
 * connectToNodeExtern()
 *
 * Arguments: nodeListSize - Number of nodes registered on node server 
 *            nodeList - Table with the nodes read from node server
 *            neighbours - struct with all topology information
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Selects and tries to connect to an external neighbour
 *              read from node server.
 *****************************************************************************/
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

/******************************************************************************
 * getNodeList()
 *
 * Arguments: nodeServer - Address of node server 
 *            net - net name 
 *            nodeList - Table with the nodes read from node server 
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Gets nodes registered in node server and stores them in the
 *              node list.
 *****************************************************************************/
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

    //sets a timeout for the node server response
    ret = select(fd + 1, &rfds, NULL, NULL, &timeout);
    //error: node server timed out
    if (ret == 0) {
        close(fd);
        return -3;
    } else if (ret == -1) {
        close(fd);
        return -1;
    }
    
    //recieves message from node server 
    n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
    if (n == -1) {
        close(fd);
        return -1;
    }

    buffer[n] = '\0';

    
    token = strtok(buffer, "\n");

    //error: node server sent useless  information
    if (sscanf(token, "%s %s", headerBuffer, netBuffer) != 2) {
        close(fd);
        return -4;
    
      //error: node server sent anti protocol message
    } else if (strcmp("NODESLIST", headerBuffer) || strcmp(net, netBuffer)) {
        return -5;
    }
    //Divides string read to separate each individual registered node
    counter = 0;
    token = strtok(NULL, "\n");
    while (token != NULL) {
        ret = sscanf(token, "%s %d", ip, &port);
        //error in function sscanf
        if (ret != 2) {
            return -6;
        }
        //vallidate ip recieved
        if (!inet_pton(AF_INET, ip, (&nodeList[counter].sin_addr))) {
            return -6;
        }
        //validate port recieved
        if (port < 1 || port > 65536) {
            return -6;
        }
        //stores node in node list
        nodeList[counter].sin_port = htons(port);

        nodeList[counter].sin_family = AF_INET;
        
        //increments node counter
        counter++;  
        token = strtok(NULL, "\n");
    }

    return counter;
}

/******************************************************************************
 * reg()
 *
 * Arguments: nodeSelf - Address of itself 
 *            nodeServer - Address of node server
 *            net - net name 
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Registers in node server
 *****************************************************************************/
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net) {
    int fd, n, ret;
    char buffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    fd_set rfds;
    struct timeval timeout;

    timeout.tv_usec = 0;
    timeout.tv_sec = SEL_TIMEOUT;

    //Opens UDP socket to connect to node server
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    //send registration message to node server
    sprintf(buffer, "REG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));

    //error sending message to node server 
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    //sets a timeout for the node server response
    ret = select(fd + 1, &rfds, NULL, NULL, &timeout);
    
    //error: node server timed out 
    if (ret == 0) {
        close(fd);
        return -3;
    } else if (ret == -1) {
        close(fd);
        return -1;
    }

    //recieves message from node server
    n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
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

/******************************************************************************
 * unreg()
 *
 * Arguments: nodeSelf - Node's own addres info 
 *            nodeServer - Address of node server
 *            net - net name 
 * Returns:   0 if ok, negative if error (describe error codes)
 * Side-Effects: 
 *
 * Description: Unregisters in node server
 *****************************************************************************/
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net) {
    int fd, n, ret;
    char buffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    fd_set rfds;
    struct timeval timeout;

    timeout.tv_usec = 0;
    timeout.tv_sec = SEL_TIMEOUT;

    //Opens UDP socket to connect to node server
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) return fd;

    //send unregistration message to node server
    sprintf(buffer, "UNREG %s %s %d", net, inet_ntop(AF_INET, &nodeSelf->sin_addr, addrBuffer, sizeof addrBuffer), ntohs(nodeSelf->sin_port));
    n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *) nodeServer, sizeof(*nodeServer));

    //error sending message to node server
    if (n != strlen(buffer)) {
        close(fd);
        return -2;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    //If the node server takes more than 3 secs to answer too bad
    ret = select(fd + 1, &rfds, NULL, NULL, &timeout);

    //error: node server timed out 
    if (ret == 0) {
        close(fd);
        return -3;
    } else if (ret == -1) {
        close(fd);
        return -1;
    }

    n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
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

/******************************************************************************
 * showTopology()
 *
 * Arguments: neighbours - struct with all topology information
 * Returns:   
 * Side-Effects: 
 *
 * Description: Prints external and recovery node 
 *****************************************************************************/
void showTopology(struct neighbours *neighbours) {
    char addrBuffer[INET_ADDRSTRLEN];

    printf("external node: %s:%d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));
    printf("recovery node: %s:%d\n", inet_ntop(AF_INET, &neighbours->recovery.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->recovery.addrressInfo.sin_port));
}

/******************************************************************************
 * closeInternal()
 *
 * Arguments: internalIndex - index of the internal to be closed
 *            neighbours - struct with all topology information
 * 
 * Returns:   
 * Side-Effects: 
 *
 * Description: Terminate internal tcp session 
 *****************************************************************************/
void closeInternal(int internalIndex, struct neighbours *neighbours) {
    close(neighbours->internal[internalIndex].fd);
    removeInternalFromTable(internalIndex, neighbours);
}

/******************************************************************************
 * closeExternal()
 *
 * Arguments: neighbours - struct with all topology information
 * Returns:   
 * Side-Effects: 
 *
 * Description: Terminate external tcp session  
 *****************************************************************************/
void closeExternal(struct neighbours *neighbours) {
    close(neighbours->external.fd);
    memset(&neighbours->external, 0, sizeof neighbours->external);
}

/******************************************************************************
 * promoteRandomInternalToExternal()
 *
 * Arguments: neighbours - struct with all topology information
 * Returns:   
 * Side-Effects: 
 *
 * Description: Selects a random internal neighbour and promotes him to
 *              external.
 *****************************************************************************/
void promoteRandomInternalToExternal(struct neighbours *neighbours) {
    int internalIndex = rand() % neighbours->numberOfInternals;
    neighbours->external.addrressInfo = neighbours->internal[internalIndex].addrressInfo;
}

/******************************************************************************
 * bradcastExtern()
 *
 * Arguments: state - state of the event loop
 *            neighbours - struct with all topology information
 *            routingTable - Struct with routung table content
 * Returns:   The state to which the event loop progresses
 * Side-Effects: 
 *
 * Description: Spreads EXTERN message to  internal neighbours to
 *              update topology.
 *****************************************************************************/
enum state broadcastExtern(enum state state, struct neighbours *neighbours, struct routingTable *routingTable) {
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    int err;
    int index;

    int errFd[MAX_LIST_SIZE];
    int errCount = 0;

    enum state newState = state;

    sprintf(writeBuffer, "EXTERN %s %d\n", inet_ntop(AF_INET, &neighbours->external.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->external.addrressInfo.sin_port));

    for (int i = 0; i < neighbours->numberOfInternals; i++) {
        err = writeBufferToTcpStream(neighbours->internal[i].fd, writeBuffer);
        if (err) {
            errFd[errCount] = neighbours->internal[i].fd;
            errCount++;
        }
    }

    for (int i = 0; i < errCount; i++) {
        /* close fd and remove node from internalNodes vector */
        index = fdToIndex(errFd[i], neighbours);
        if (index != -2) {
            newState = neighbourDisconnectionHandler(state, index, neighbours, routingTable);
        }
    }

    return newState;
}

/******************************************************************************
 * connectToRecovery()
 *
 * Arguments: neighbours - struct with all topology information
 * Returns:   0 if ok, -1 if error 
 * Side-Effects: 
 *
 * Description: Connects to recovery node and sends NEW message.
 *****************************************************************************/
int connectToRecovery(struct neighbours *neighbours) {
    int err;
    char writeBuffer[BUFFER_SIZE];
    char addrBuffer[INET_ADDRSTRLEN];

    neighbours->external.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (neighbours->external.fd == -1) return neighbours->external.fd;

    neighbours->external.addrressInfo = neighbours->recovery.addrressInfo;

    err = connect(neighbours->external.fd, (struct sockaddr *) &neighbours->recovery.addrressInfo, sizeof(neighbours->recovery.addrressInfo));
    //error connecting to the recovery node
    if (err) {
        return -1;
    }

    //creates buffer with message to be written
    sprintf(writeBuffer, "NEW %s %d\n", inet_ntop(AF_INET, &neighbours->self.addrressInfo.sin_addr, addrBuffer, sizeof addrBuffer), ntohs(neighbours->self.addrressInfo.sin_port));

    // send NEW
    err = writeBufferToTcpStream(neighbours->external.fd, writeBuffer);

    if (err) {
        return err;
    }

    return 0;
}

/******************************************************************************
 * externMessageHandler()
 *
 * Arguments: neighbours - struct with all topology information
 *            addrinfo - Address read in EXTERN message recieved
 * Returns:   
 * Side-Effects: 
 *
 * Description: Updates recovery neighbour info after recieving 
 *              message EXTERNAL.
 *****************************************************************************/
void externMessageHandler(struct neighbours *neighbours, struct sockaddr_in *addrinfo) {
    neighbours->recovery.addrressInfo = *addrinfo;
}

/******************************************************************************
 * neighbourDisconnectionHandler()
 *
 * Arguments: state - state of the event loop
 *            neighbourIndex - index of the neighbour to be removed
 *                              if -1, i is the external neighbour
 *            neighbours - struct with all topology information
 *            routingTable - Struct with routung table content
 * Returns:   The state to which the event loop progresses
 * Side-Effects: 
 *
 * Description: Removes nodes depending on the state, keeping the
 *              net connected. 
 *****************************************************************************/
enum state neighbourDisconnectionHandler(enum state state, int neighbourIndex, struct neighbours *neighbours, struct routingTable *routingTable) {
    int err;
    enum state newState = state;
    int edgeToRemove;
    switch (state) {
        case registered:  //REG
            if (neighbourIndex == -1) {
                if (neighbours->recovery.addrressInfo.sin_addr.s_addr == neighbours->self.addrressInfo.sin_addr.s_addr && neighbours->recovery.addrressInfo.sin_port == neighbours->self.addrressInfo.sin_port) {
                    // ROTINA 3
                    edgeToRemove = neighbours->external.fd;
                    closeExternal(neighbours);
                    //if you are the only node remaining
                    if (neighbours->numberOfInternals == 0) {
                        neighbours->external.addrressInfo = neighbours->self.addrressInfo;
                        newState = loneRegistered;  // goto state LONEREG
                        newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                        return newState;
                    }
                    promoteRandomInternalToExternal(neighbours);
                    newState = broadcastExtern(state, neighbours, routingTable);
                    //inform routing layer about neighbour disconnection
                    newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                    return newState;
                } else {
                    // ROTINA 4
                    edgeToRemove = neighbours->external.fd;
                    closeExternal(neighbours);
                    newState = waitingRecovery;  //goto WAITREC
                    newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                    err = connectToRecovery(neighbours);
                    if (err) {
                        return notRegistered;  // leave and go to state notReg
                    }
                    //inform routing layer about neighbour disconnection
                    return newState;
                }
            } else {
                if (neighbours->internal[neighbourIndex].addrressInfo.sin_addr.s_addr == neighbours->external.addrressInfo.sin_addr.s_addr && neighbours->internal[neighbourIndex].addrressInfo.sin_port == neighbours->external.addrressInfo.sin_port) {
                    // ROTINA 2
                    edgeToRemove = neighbours->internal[neighbourIndex].fd;
                    closeInternal(neighbourIndex, neighbours);
                    //if you are the only node remaining
                    if (neighbours->numberOfInternals == 0) {
                        neighbours->external.addrressInfo = neighbours->self.addrressInfo;
                        newState = loneRegistered;  // goto state LONEREG
                        newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                        return newState;
                    }
                    promoteRandomInternalToExternal(neighbours);
                    newState = broadcastExtern(state, neighbours, routingTable);
                    //inform routing layer about neighbour disconnection
                    newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                    return newState;
                } else {
                    // ROTINA 1
                    edgeToRemove = neighbours->internal[neighbourIndex].fd;
                    closeInternal(neighbourIndex, neighbours);
                    //inform routing layer about neighbour disconnection
                    newState = registered;  // goto state REG
                    newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                    return newState;  // goto state REG
                }
            }
            break;

        case loneRegistered:  //LONEREG
            // ROTINA 5 (1)
            if (neighbours->internal[neighbourIndex].addrressInfo.sin_addr.s_addr == neighbours->external.addrressInfo.sin_addr.s_addr && neighbours->internal[neighbourIndex].addrressInfo.sin_port == neighbours->external.addrressInfo.sin_port) {
                neighbours->external.addrressInfo = neighbours->self.addrressInfo;
            }
            edgeToRemove = neighbours->internal[neighbourIndex].fd;
            closeInternal(neighbourIndex, neighbours);
            //inform routing layer about neighbour disconnection
            newState = loneRegistered;  // goto state LONEREG
            newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
            return newState;
            break;

        case waitingRecovery:  //WAITREC
            if (neighbourIndex == -1) {
                return notRegistered;  // leave and go to state notReg
            } else {
                // ROTINA 1
                edgeToRemove = neighbours->internal[neighbourIndex].fd;
                closeInternal(neighbourIndex, neighbours);
                //inform routing layer about neighbour disconnection
                newState = waitingRecovery;  // goto state WAITREC
                newState = withdrawEdge(edgeToRemove, routingTable, newState, neighbours);
                return newState;
            }
            break;

        default:
            break;
    }
    return notRegistered;
}
