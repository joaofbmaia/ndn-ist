#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <arpa/inet.h>
#include "neighbours.h"

int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours);
int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours);
int loneNewInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo);
int newInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo);
int externMessageHandler(struct neighbours *neighbours, struct sockaddr_in *addrinfo, struct sockaddr_in *nodeServer, char *net);
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
void showTopology(struct neighbours *neighbours);

#endif