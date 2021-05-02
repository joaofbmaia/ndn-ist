#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "neighbours.h"
#include "routing.h"
#include "search.h"

void eventLoop(struct sockaddr_in *nodeServer, struct neighbours *neighbours, struct routingTable *routingTable, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache);

#endif