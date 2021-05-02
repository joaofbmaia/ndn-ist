#ifndef SEARCH_H
#define SEARCH_H

#include <time.h>
#include "defines.h"
#include "routing.h"

struct object {
    char name[BUFFER_SIZE];
};

struct objectTable {
    struct object entry[MAX_OBJECTS];
    int size;
};

struct interest {
    char name[BUFFER_SIZE];
    char sourceEdge;
    time_t creationTime;
};

struct interestTable {
    struct interest entry[MAX_OBJECTS];
    int size;
};

struct cache{
    struct object entry[CACHE_SIZE];
    int size;
};

int createObject(char *objectSubName, struct objectTable *objectTable, char *id);
int getObject(char *objectName, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable);
int interestHandler(char *objectName, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable, int sourceEdge);
int dataHandler(char *objectName, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable);
int noDataHandler(char *objectName, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable);
void removeFromInterestTable(char *objectName, struct interestTable *interestTable);
int removeStaleEntriesFromInterestTable(struct interestTable *interestTable);
void pushToCache(struct object *object, struct cache *cache);
struct object *retrieveFromCache(char *name, struct cache *cache);
void showCache(struct cache *cache);

#endif