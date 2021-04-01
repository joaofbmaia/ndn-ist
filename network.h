#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>

int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList);
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);

#endif