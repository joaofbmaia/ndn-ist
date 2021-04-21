#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include "neighbours.h"

int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours);
int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours);
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct sockaddr_in *nodeExtern);
int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList);
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);

#endif