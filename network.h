#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>

int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);

#endif