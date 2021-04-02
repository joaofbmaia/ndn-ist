#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>

int join(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, struct sockaddr_in *nodeExtern, struct sockaddr_in *recoveryNode, char *net, int *externFd, int *listeningFd);
int leave(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, struct sockaddr_in *nodeExtern, struct sockaddr_in *recoveryNode, char *net, int *externFd, int *listeningFd);
int connectToNodeExtern(int nodeListSize, struct sockaddr_in *nodeList, struct sockaddr_in *nodeExtern);
int getNodeList(struct sockaddr_in *nodeServer, char *net, struct sockaddr_in *nodeList);
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);

#endif