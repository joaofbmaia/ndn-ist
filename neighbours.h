#ifndef NEIGHBOURS_H
#define NEIGHBOURS_H

#include <arpa/inet.h>
#include "defines.h"

/*Struct with node information
Write complete header at a later stage*/
struct node {
    int fd;                           //fd for the socket; in case of node self it is the listening fd
    struct sockaddr_in addrressInfo;  //IP and TCP port of the node
    char readBuffer[BUFFER_SIZE];     //buffer for reading messages from this node
};

/*Struct with info about all relevant nodes
Write complete header at a later stage*/
struct neighbours {
    struct node self;                     //yourself
    struct node external;                 //external neighbour node info
    struct node recovery;                 //recovery node info
    struct node internal[MAX_LIST_SIZE];  //table with all internal neighbours nodes info
    int numberOfInternals;                //number of internal neighbours
};

#endif