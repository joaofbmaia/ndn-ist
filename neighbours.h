#ifndef NEIGHBOURS_H
#define NEIGHBOURS_H

#include <arpa/inet.h>
#include"defines.h"

/*Struct with node information
Write complete header at a later stage*/
struct node {
    int fd;
    struct sockaddr_in addrressInfo;
    char readBuffer[BUFFER_SIZE]; 
};

/*Struct with info about all relevant nodes
Write complete header at a later stage*/
struct neighbours {
    struct node self;
    struct node external;
    struct node recovery;
    struct node internal[MAX_LIST_SIZE];
    int numberOfInternals;
};

#endif