#ifndef ROUTING_H
#define ROUTING_H

#include "defines.h"
#include "neighbours.h"

struct routingTableEntry {
    char id[BUFFER_SIZE];
    int edgeFd;
};

struct routingTable {
    struct routingTableEntry entry[MAX_LIST_SIZE];
    int size;
};

int advertiseToEdge(int edge, struct routingTable *routingTable);
enum state broadcastAdvertise(int originEdge, char *id, struct routingTable *routingTable, enum state state, struct neighbours *neighbours);
void addNodeToRoutingTable(int edge, char *id, struct routingTable *routingTable);
void removeNodeFromRoutingTable(char *id, struct routingTable *routingTable);
void showRouting(struct routingTable *routingTable);

#endif