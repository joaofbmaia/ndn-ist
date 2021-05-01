#include "routing.h"
#include <string.h>
#include <stdio.h>
#include "states.h"
#include "topology.h"
#include "utils.h"
#include "neighbours.h"

/******************************************************************************
 * avertiseToEdge()
 *
 * Arguments: edge - File descriptor that describes that edge 
 *            routingTable - Struct with routung table content
 * Returns:   0 if ok, -1 if error
 * Side-Effects: 
 *
 * Description: Sends ADVERTISE messages to a specific edge.
 *
 *****************************************************************************/
int advertiseToEdge(int edge, struct routingTable *routingTable) {
    int err;
    char writeBuffer[BUFFER_SIZE + 16];
    for (int i = 0; i < routingTable->size; i++) {
        if (routingTable->entry[i].edgeFd != edge) {
            sprintf(writeBuffer, "ADVERTISE %s\n", routingTable->entry[i].id);
            err = writeBufferToTcpStream(edge, writeBuffer);
            if (err) {
                return err;
            }
        }
    }
    return 0;
}

/******************************************************************************
 * broadcastAvertise()
 *
 * Arguments: originEdge - File descriptor that describes message origin edge
 *            id - id of the node to be advertised 
 *            routingTable - Struct with routung table content
 *            state - state to which the state machine has to progress
 *            neighbours - struct with all topology information
 * Returns:   The state to which the state machine progresses
 * Side-Effects: 
 *
 * Description: Spreads ADVERTISE message through ou the network
 *
 *****************************************************************************/
enum state broadcastAdvertise(int originEdge, char *id, struct routingTable *routingTable, enum state state, struct neighbours *neighbours) {
    int errEdge[MAX_LIST_SIZE];
    int errCount = 0;
    int index;
    int err;

    char writeBuffer[BUFFER_SIZE + 16];

    enum state newState = state;
    
    sprintf(writeBuffer, "ADVERTISE %s\n", id);

    for (int i = 0; i < neighbours->numberOfInternals; i++) {
        if (originEdge != neighbours->internal[i].fd) {
            err = writeBufferToTcpStream(neighbours->internal[i].fd, writeBuffer);
            if (err) {
                errEdge[errCount] = neighbours->internal[i].fd;
                errCount++;
            }
            
        }
    }

    for (int i = 0; i < errCount; i++) {
        /* close fd and remove node from internalNodes vector */
        index = fdToIndex(errEdge[i], neighbours);
        if (index != -2) {
            newState = neighbourDisconnectionHandler(state, index, neighbours, routingTable);
        }
    }

    if (originEdge != neighbours->external.fd && neighbours->external.fd != 0) {
        err = writeBufferToTcpStream(neighbours->external.fd, writeBuffer);
        if (err) {
            newState = neighbourDisconnectionHandler(state, -1, neighbours, routingTable);
        }
    }

    return newState;
}

/******************************************************************************
 * broadcastWithdraw()
 *
 * Arguments: originEdge - File descriptor that describes message origin edge
 *            id - id of the node to be advertised 
 *            routingTable - Struct with routung table content
 *            state - state to which the state machine has to progress
 *            neighbours - struct with all topology information
 * Returns:   The state to which the state machine progresses
 * Side-Effects: 
 *
 * Description: Spreads WITHDRAW message through ou the network
 *
 *****************************************************************************/
enum state broadcastWithdraw(int originEdge, char *id, struct routingTable *routingTable, enum state state, struct neighbours *neighbours) {
    int errEdge[MAX_LIST_SIZE];
    int errCount = 0;
    int index;
    int err;

    char writeBuffer[BUFFER_SIZE + 16];

    enum state newState = state;
    
    sprintf(writeBuffer, "WITHDRAW %s\n", id);

    for (int i = 0; i < neighbours->numberOfInternals; i++) {
        if (originEdge != neighbours->internal[i].fd) {
            err = writeBufferToTcpStream(neighbours->internal[i].fd, writeBuffer);
            if (err) {
                errEdge[errCount] = neighbours->internal[i].fd;
                errCount++;
            }
            
        }
    }

    for (int i = 0; i < errCount; i++) {
        /* close fd and remove node from internalNodes vector */
        index = fdToIndex(errEdge[i], neighbours);
        if (index != -2) {
            newState = neighbourDisconnectionHandler(state, index, neighbours, routingTable);
        }
    }

    if (originEdge != neighbours->external.fd && neighbours->external.fd != 0) {
        err = writeBufferToTcpStream(neighbours->external.fd, writeBuffer);
        if (err) {
            newState = neighbourDisconnectionHandler(state, -1, neighbours, routingTable);
        }
    }

    return newState;
}

/******************************************************************************
 * withdrawEdge()
 *
 * Arguments: edge - File descriptor that describes edge to be removed
 *            routingTable - Struct with routung table content 
 *            state - state to which the state machine has to progress
 *            neighbours - struct with all topology information
 * Returns:   The state to which the state machine progresses
 * Side-Effects: 
 *
 * Description: Informs net about all nodes to be removed from routing table
 *
 *****************************************************************************/
enum state withdrawEdge(int edge, struct routingTable *routingTable, enum state state, struct neighbours *neighbours) {
    enum state newState = state;

    for (int i = 0; i < routingTable->size; i++) {
        if (routingTable->entry[i].edgeFd == edge) {
            newState = broadcastWithdraw(edge, routingTable->entry[i].id, routingTable, state, neighbours);
        }
    }
    for (int i = 0; i < routingTable->size; i++) {
        if (routingTable->entry[i].edgeFd == edge) {
            removeNodeFromRoutingTable(routingTable->entry[i].id, routingTable);
        }
    }
    return newState;
}

/******************************************************************************
 * addNodeToRoutingTable()
 *
 * Arguments: edge - File descriptor that describes that edge 
 *            id - id recieved in ADVERTISE message
 *            routingTable - Struct with routung table content
 * Returns:   
 * Side-Effects: 
 *
 * Description: If id already exists in routing table, edge content is
 *              overwrited, else creates a new enttry to routing table.
 *
 *****************************************************************************/
void addNodeToRoutingTable(int edge, char *id, struct routingTable *routingTable) {
    for (int i = 0; i < routingTable->size; i++) {
        if (!strcmp(routingTable->entry[i].id, id)) {
            routingTable->entry[i].edgeFd = edge;
            return;
        }
    }
    routingTable->entry[routingTable->size].edgeFd = edge;
    strcpy(routingTable->entry[routingTable->size].id, id);
    routingTable->size++;
}

/******************************************************************************
 * removeNodeToRoutingTable()
 *
 * Arguments: edge - File descriptor that describes that edge 
 *            id - id to be removed 
 *            routingTable - Struct with routung table content
 * Returns:   
 * Side-Effects: 
 *
 * Description: Finds and removes an id from the routing table
 *
 *****************************************************************************/
void removeNodeFromRoutingTable(char *id, struct routingTable *routingTable) {
    for (int i = 0; i < routingTable->size; i++) {
        if (!strcmp(routingTable->entry[i].id, id)) {
            for (int j = i + 1; j < routingTable->size; j++) {
                routingTable->entry[j - 1] = routingTable->entry[j];
            }
            memset(&routingTable->entry[routingTable->size - 1], 0, sizeof routingTable->entry[routingTable->size - 1]);
            routingTable->size--;
            return;
        }
    }
}

/******************************************************************************
 * showRouting()
 *
 * Arguments:  routingTable - Struct with routung table content
 * Returns:   
 * Side-Effects: 
 *
 * Description: Prints routing table 
 *
 *****************************************************************************/
void showRouting(struct routingTable *routingTable) {
    for (int i = 0; i < routingTable->size; i++) {
        printf("id: %s    edge: %d\n", routingTable->entry[i].id, routingTable->entry[i].edgeFd);
    }
}