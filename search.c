#include "search.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "routing.h"
#include "utils.h"

/******************************************************************************
 * createObject()
 *
 * Arguments: objectName - String with the name of the object created
 *            objectTable - Table with all own objects information
 * Returns:   0 if ok, -1 if object table is full, 1 if object already exists
 * Side-Effects: 
 *
 * Description: Puts created object into object table 
 *****************************************************************************/
int createObject(char *objectSubname, struct objectTable *objectTable, char *id) {
    char nameBuffer[BUFFER_SIZE];
    //check if object table is full
    if (objectTable->size == MAX_OBJECTS) {
        return -1;
    }

    sprintf(nameBuffer, "%s.%s", id, objectSubname);

    //check if object already exists
    for (int i = 0; i < objectTable->size; i++) {
        if (!strcmp(objectTable->entry[i].name, nameBuffer)) {
            return 1;
        }
    }
    //saves object to oobject table
    strcpy(objectTable->entry[objectTable->size].name, nameBuffer);
    objectTable->size++;
    return 0;
}

/******************************************************************************
 * getObject()
 *
 * Arguments: objectName - String with the name of the object created
 *            objectTable - Table with all own objects information
 *            interestTable - Table with all interest requests
 *            cache - Cache containing most recent data
 *            routingTable - Routing table content
 * Returns:    0 if ok, destEdge if error in write
 * Side-Effects: 
 *
 * Description: Execute command get by looking for object in cache and object
 *              and, if not found, send INTEREST message to target id and add
 *              object and id to interest table.
 *****************************************************************************/
int getObject(char *objectName, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable) {
    char id[BUFFER_SIZE];
    char subname[BUFFER_SIZE];

    char writeBuffer[BUFFER_SIZE + 16];

    int err;

    int destEdge;

    struct object *cachedObject;

    //parses name
    sscanf(objectName, "%[^.].%s", id, subname);

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

    //search cache for object
    cachedObject = retrieveFromCache(objectName, cache);
    if (cachedObject) {
        printf("Received object: %s\n", cachedObject->name);
        return 0;
    }

    //is for me? 👉😳👈
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
        return destEdge;
    }

    //add interest request to interest table
    strcpy(interestTable->entry[interestTable->size].name, objectName);
    interestTable->entry[interestTable->size].sourceEdge = 0;
    interestTable->entry[interestTable->size].creationTime = time(NULL);
    interestTable->size++;

    return 0;
}

/******************************************************************************
 * interestHandler()
 *
 * Arguments: objectName - String with the name of the object created
 *            objectTable - Table with all own objects information
 *            interestTable - Table with all interest requests
 *            cache - Cache containing most recent data
 *            routingTable - Routing table content
 *            sourceEdge - Edge from which the INTEREST message came from
 * Returns:    0 if ok, destEdge if error in write
 * Side-Effects: 
 *
 * Description: Processes INTEREST message by looking for object 
 *              in cache and object in interest and, if not found, 
 *              send INTEREST message to target id and add object 
 *              and id to interest table.
 *****************************************************************************/
int interestHandler(char *objectName, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable, int sourceEdge) {
    char id[BUFFER_SIZE];
    char subname[BUFFER_SIZE];

    char writeBuffer[BUFFER_SIZE + 16];

    int err;

    int destEdge;

    struct object *cachedObject;

    //parses name
    sscanf(objectName, "%[^.].%s", id, subname);

    //search routing table for destination node
    destEdge = -1;
    for (int i = 0; i < routingTable->size; i++) {
        if (!strcmp(routingTable->entry[i].id, id)) {
            destEdge = routingTable->entry[i].edgeFd;
            break;
        }
    }

    // id doesn't even exist in routing table
    if (destEdge == -1) {
        // SEND NODATA TO SOURCE
        sprintf(writeBuffer, "NODATA %s\n", objectName);
        err = writeBufferToTcpStream(sourceEdge, writeBuffer);
        if (err) {
            return sourceEdge;
        }
        return 0;
    }

    //search cache for object
    cachedObject = retrieveFromCache(objectName, cache);
    if (cachedObject) {
        // object found in cache
        // SEND DATA TO SOURCE
        sprintf(writeBuffer, "DATA %s\n", cachedObject->name);
        err = writeBufferToTcpStream(sourceEdge, writeBuffer);
        if (err) {
            return sourceEdge;
        }
        return 0;
    }

    //INTEREST is for me? 👉😳👈
    if (destEdge == 0) {
        for (int i = 0; i < objectTable->size; i++) {
            if (!strcmp(objectTable->entry[i].name, objectName)) {
                pushToCache(&objectTable->entry[i], cache);
                // object found in object table
                // SEND DATA TO SOURCE
                sprintf(writeBuffer, "DATA %s\n", objectTable->entry[i].name);
                err = writeBufferToTcpStream(sourceEdge, writeBuffer);
                if (err) {
                    return sourceEdge;
                }
                return 0;
            }
        }
        // object not found in object table
        // SEND NODATA TO SOURCE
        sprintf(writeBuffer, "NODATA %s\n", objectName);
        err = writeBufferToTcpStream(sourceEdge, writeBuffer);
        if (err) {
            return sourceEdge;
        }
        return 0;
    }

    // foward INTEREST message to destination edge
    sprintf(writeBuffer, "INTEREST %s\n", objectName);

    err = writeBufferToTcpStream(destEdge, writeBuffer);
    if (err) {
        return destEdge;
    }

    //add interest request to interest table
    strcpy(interestTable->entry[interestTable->size].name, objectName);
    interestTable->entry[interestTable->size].sourceEdge = sourceEdge;
    interestTable->entry[interestTable->size].creationTime = time(NULL);
    interestTable->size++;

    return 0;
}

/******************************************************************************
 * dataHandler()
 *
 * Arguments: objectName - String with the name of the object created
 *            interestTable - Table with all interest requests
 *            cache - Cache containing most recent data
 *            routingTable - Routing table content
 *            writtenToStdin - flag written to 1 if stdin is written
 * Returns:   0 if ok, destEdge if error in write
 * Side-Effects: 
 *
 * Description: Processes DATA message by locating message recepient in 
 *              interest table, if located store data in cache and if 
 *              not recipient of data, foward data to destination.
 *****************************************************************************/
int dataHandler(char *objectName, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTable, int *writtenToStdin) {
    char writeBuffer[BUFFER_SIZE + 16];
    int destEdge = -1, err;

    struct object data;

    strcpy(data.name, objectName);

    // search interest table for matching request
    for (int i = 0; interestTable->size; i++) {
        if (!strcmp(interestTable->entry[i].name, data.name)) {
            pushToCache(&data, cache);
            destEdge = interestTable->entry[i].sourceEdge;
            removeFromInterestTable(data.name, interestTable);
            break;
        }
    }

    // if data not in interest table
    if (destEdge == -1) {
        return 0;
    }

    //DATA is for me? 👉😳👈
    if (destEdge == 0) {
        printf("\nReceived object: %s\n", data.name);
        *writtenToStdin = 1;
        return 0;
    }

    // foward DATA message to destination edge
    sprintf(writeBuffer, "DATA %s\n", objectName);

    err = writeBufferToTcpStream(destEdge, writeBuffer);
    if (err) {
        return destEdge;
    }

    return 0;
}

/******************************************************************************
 * noDataHandler()
 *
 * Arguments: objectName - String with the name of the object created
 *            interestTable - Table with all interest requests
 *            cache - Cache containing most recent data
 *            routingTable - Routing table content
 *            writtenToStdin - flag written to 1 if stdin is written
 * Returns:   0 if ok, destEdge if error in write
 * Side-Effects: 
 *
 * Description: Processes NODATA message by locating message recepient in 
 *              interest table if not recipient of message, foward it to 
 *              destination.
 *****************************************************************************/
int noDataHandler(char *objectName, struct interestTable *interestTable, struct cache *cache, struct routingTable *routingTableint, int *writtenToStdin) {
    char writeBuffer[BUFFER_SIZE + 16];
    int destEdge = -1, err;

    // search interest table for matching request
    for (int i = 0; interestTable->size; i++) {
        if (!strcmp(interestTable->entry[i].name, objectName)) {
            destEdge = interestTable->entry[i].sourceEdge;
            removeFromInterestTable(objectName, interestTable);
            break;
        }
    }

    // if nodata is not in interest table
    if (destEdge == -1) {
        return 0;
    }

    //NODATA is for me? 👉😳👈
    if (destEdge == 0) {
        printf("\nNo object found 🤔\n");
        *writtenToStdin = 1;
        return 0;
    }

    // foward NODATA message to destination edge
    sprintf(writeBuffer, "NODATA %s\n", objectName);

    err = writeBufferToTcpStream(destEdge, writeBuffer);
    if (err) {
        return destEdge;
    }

    return 0;
}

/******************************************************************************
 * removeFromInterestTable()
 *
 * Arguments: objectName - name of the object to be removed from table
 *            interestTable - Table with all interest requests
 * Returns:   
 * Side-Effects: 
 *
 * Description: Finds and removes an object from the interest table
 *****************************************************************************/
void removeFromInterestTable(char *objectName, struct interestTable *interestTable) {
    for (int i = 0; i < interestTable->size; i++) {
        if (!strcmp(interestTable->entry[i].name, objectName)) {
            for (int j = i + 1; j < interestTable->size; j++) {
                interestTable->entry[j - 1] = interestTable->entry[j];
            }
            memset(&interestTable->entry[interestTable->size - 1], 0, sizeof interestTable->entry[interestTable->size - 1]);
            interestTable->size--;
            return;
        }
    }
}

/******************************************************************************
 * removeStaleEntriesFromInterestTable()
 *
 * Arguments: interestTable - Table with all interest requests
 *            writtenToStdin - flag written to 1 if stdin is written
 * Returns:   Flag print promt 1 if a message is printedor 0 if
 *            not
 * Side-Effects: 
 *
 * Description: Finds and removes an object from the interest table
 *              after not recieving an answer for a certain time.
 *****************************************************************************/
void removeStaleEntriesFromInterestTable(struct interestTable *interestTable, int *writtenToStdin) {
    time_t now = time(NULL);
    for (int i = 0; i < interestTable->size; i++) {
        if ((now - interestTable->entry[i].creationTime) > INTEREST_TIMEOUT) {
            if (interestTable->entry[i].sourceEdge == 0) {
                printf("\nInterest message for object %s timed out with no response 😱\n", interestTable->entry[i].name);
                *writtenToStdin = 1;
            }
            removeFromInterestTable(interestTable->entry[i].name, interestTable);
            i--;
        }
    }
}

/******************************************************************************
 * pushToCache()
 *
 * Arguments: object - object to pushed into cache
 *            cache - Cache containing most recent data 
 * Returns:   
 * Side-Effects: 
 *
 * Description: Pushes object into cache and expels oldest entry.
 *****************************************************************************/
void pushToCache(struct object *object, struct cache *cache) {
    for (int i = 0; i < CACHE_SIZE - 1; i++) {
        cache->entry[i + 1] = cache->entry[i];
    }
    cache->entry[0] = *object;
    if (cache->size != CACHE_SIZE) {
        cache->size++;
    }
}

/******************************************************************************
 * retrieveFromCache()
 *
 * Arguments: name - object to be retrived 
 *            cache - Cache containing most recent data
 *
 * Returns:   object to be retrived or NULL if object not found in cache
 * Side-Effects: 
 *
 * Description: Looks for an object in cache and retrives it if found
 *****************************************************************************/
struct object *retrieveFromCache(char *name, struct cache *cache) {
    for (int i = 0; i < cache->size; i++) {
        if (!strcmp(cache->entry[i].name, name)) {
            return &cache->entry[i];
        }
    }
    return NULL;
}

/******************************************************************************
 * showTopology()
 *
 * Arguments:  cache - Cache containing most recent data
 * Returns:   
 * Side-Effects: 
 *
 * Description: Prints cache content 
 *****************************************************************************/
void showCache(struct cache *cache) {
    if (cache->size == 0) {
        printf("Cache is empty 😅\n");
    }
    for (int i = 0; i < cache->size; i++) {
        printf("%s\n", cache->entry[i].name);
    }
}