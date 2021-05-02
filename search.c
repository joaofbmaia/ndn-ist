#include "search.h"
#include <string.h>
#include <time.h>
#include "routing.h"
#include <stdio.h>
#include "utils.h"

/******************************************************************************
 * createObject()
 *
 * Arguments: objectName - String with the name of the object created
 *            objectTable - Table with all own object information
 * Returns:   
 * Side-Effects: 
 *
 * Description: Puts created object into object table 
 *
 *****************************************************************************/
int createObject(char *objectSubname, struct objectTable *objectTable, char *id) {
    char nameBuffer[BUFFER_SIZE];
    if (objectTable->size == MAX_OBJECTS) {
        return -1;
    }
    sprintf(nameBuffer, "%s.%s", id, objectSubname);
    strcpy(objectTable->entry[objectTable->size].name, nameBuffer);
    objectTable->size++;
    return 0;
}

/******************************************************************************
 * getObject()
 *
 * Arguments: objectName - String with the name of the object created
 *            objectTable - Table with all own object information
 *            interestTable - Table with all interest requests
 *            cache - Cache containg most recent data
 *            routingTable - Struct with routung table content
 * Returns:   
 * Side-Effects: 
 *
 * Description: Execute command get by looking for object in cache and object
 *              and, if not found, send INTEREST message to target id and add
 *              object and id to interest table.
 *****************************************************************************/
int getObject(char *objectName, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable, char *selfId) {
    char id[BUFFER_SIZE];
    char subname[BUFFER_SIZE];

    char writeBuffer[BUFFER_SIZE + 16];

    int err;

    int destEdge;

    struct object *cachedObject;

    //parses name
    sscanf(objectName, "%[^.].%s", id, subname);

    //search cache for object
    cachedObject = retrieveFromCache(objectName, cache);
    if (cachedObject) {
        printf("Received object: %s\n", cachedObject->name);
        return 0;
    }

    //search routing table for destination node
    destEdge = -1;
    for (int i = 0; i < routingTable->size; i++) {
        if (!strcmp(routingTable->entry[i].id, id)) {
            destEdge = routingTable->entry[i].edgeFd;
            break;
        }
    }

    if (destEdge == -1) {
        printf("No node with that ID present in the net 😕\n");
        return 0;
    }

    //id searched is yourself, check for object on own object table
    if (destEdge == 0) {
        for (int i = 0; i < objectTable->size; i++) {
            if (!strcmp(objectTable->entry[i].name, objectName)) {
                pushToCache(&objectTable->entry[i], cache);
                printf("Received object: %s\n", objectTable->entry[i].name);
                return 0;
            }
        }
        printf("No object found 🤔\n");
        return 0;
    }

    //send INTEREST to target id
    sprintf(writeBuffer, "INTEREST %s\n", objectName);

    err = writeBufferToTcpStream(destEdge, writeBuffer);
    if (err) {
        printf("error: destinantion unreachable\n");
        return err;
    }

    //add interest request to interest table
    strcpy(interestTable->entry[interestTable->size].name, objectName);
    strcpy(interestTable->entry[interestTable->size].source, selfId);
    interestTable->entry[interestTable->size].creationTime = time(NULL);
    interestTable->size++;

    return 0;
}

void pushToCache(struct object *object, struct cache *cache) {
    for (int i = 0; i < CACHE_SIZE - 1; i++) {
        cache->entry[i + 1] = cache->entry[i];
    }
    cache->entry[0] = *object;
    if (cache->size != CACHE_SIZE) {
        cache->size++;
    }
}

struct object *retrieveFromCache(char *name, struct cache *cache) {
    for (int i = 0; i < cache->size; i++) {
        if (!strcmp(cache->entry[i].name, name)) {
            return &cache->entry[i];
        }
    }
    return NULL;
}

void showCache(struct cache *cache) {
    if(cache->size == 0){
        printf("Cache is empty 😅\n");
    }
    for (int i = 0; i < cache->size; i++) {
        printf("%s\n", cache->entry[i].name);
    }
}