#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <arpa/inet.h>
#include "neighbours.h"
#include "routing.h"
#include "search.h"

int join(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours, char *id, struct routingTable *routingTable);
int leave(struct sockaddr_in *nodeServer, char *net, struct neighbours *neighbours, struct routingTable *routingTable, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache);
int loneNewInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo);
int newInternalHandler(struct neighbours *neighbours, int internalIndex, struct sockaddr_in *addrinfo);
int finishJoin(struct neighbours *neighbours, struct sockaddr_in *addrinfo, struct sockaddr_in *nodeServer, char *net);
int reg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
int unreg(struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer, char *net);
void showTopology(struct neighbours *neighbours);
void closeInternal(int internalIndex, struct neighbours *neighbours);
void closeExternal(struct neighbours *neighbours);
void promoteRandomInternalToExternal(struct neighbours *neighbours);
enum state broadcastExtern(enum state state, struct neighbours *neighbours, struct routingTable *routingTable);
int connectToRecovery(struct neighbours *neighbours);
void externMessageHandler(struct neighbours *neighbours, struct sockaddr_in *addrinfo);
enum state neighbourDisconnectionHandler(enum state state, int neighbourIndex, struct neighbours *neighbours, struct routingTable *routingTable);


#endif