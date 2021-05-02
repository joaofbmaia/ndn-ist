#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include "defines.h"
#include "neighbours.h"
#include "parser.h"
#include "returnCodes.h"
#include "routing.h"
#include "search.h"
#include "states.h"
#include "topology.h"
#include "utils.h"

/******************************************************************************
 * eventLoop()
 *
 * Arguments: nodeServer - Address of node server 
 *            neighbours - struct with all topology information
 *            routingTable - struct with routing table content
 *            objectTable - Table with all own objects information
 *            interestTable - Table with all interest requests
 *            cache - Cache containing most recent data
 * Returns:   
 * Side-Effects: 
 *
 * Description: Executes event loop of the program.  
 *****************************************************************************/
void eventLoop(struct sockaddr_in *nodeServer, struct neighbours *neighbours, struct routingTable *routingTable, struct objectTable *objectTable, struct interestTable *interestTable, struct cache *cache) {
    fd_set rfds;
    struct timeval timeout;

    int counter, commandCode, maxfd, err, messageCode, ret, errFd;
    char buffer[BUFFER_SIZE];
    char net[BUFFER_SIZE], id[BUFFER_SIZE];

    char commandName[BUFFER_SIZE], commandNet[BUFFER_SIZE], commandId[BUFFER_SIZE];
    struct sockaddr_in commandAddrInfo;
    char messageId[BUFFER_SIZE], messageName[BUFFER_SIZE];
    struct sockaddr_in messageAddrInfo;
    char *message;

    int changedState;
    int printPrompt;

    enum state state;

    state = notRegistered;
    changedState = 0;
    printPrompt = 1;
    while (state != goingOut) {
        FD_ZERO(&rfds);

        switch (state) {
            case notRegistered:
                maxfd = setFds(&rfds, neighbours);
                break;

            case waitingExtern:
                maxfd = setFds(&rfds, neighbours);
                message = getMessageFromBuffer(neighbours->external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    if (messageCode == MC_EXTERN) {
                        err = finishJoin(neighbours, &messageAddrInfo, nodeServer, net);
                        if (err) {
                            printErrorMessage(err);
                            changedState = 1;
                            err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                            if (err) {
                                printf("\n");
                                printErrorMessage(err);
                            }
                            printf("\nCouldn't connect to network 😕\n");
                            printPrompt = 1;
                            state = notRegistered;
                            break;
                        }
                        printf("\nSuccesfully joined network! 👏\n");
                        printPrompt = 1;
                        state = registered;
                        err = advertiseToEdge(neighbours->external.fd, routingTable);
                        if (err) {
                            state = neighbourDisconnectionHandler(registered, -1, neighbours, routingTable);
                        }
                        changedState = 1;
                        break;
                    }
                }

                break;

            case loneRegistered:
                maxfd = setFds(&rfds, neighbours);
                for (int i = 0; i < neighbours->numberOfInternals; i++) {
                    message = getMessageFromBuffer(neighbours->internal[i].readBuffer);
                    if (message) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        if (messageCode == MC_NEW) {
                            err = loneNewInternalHandler(neighbours, i, &messageAddrInfo);
                            if (err) {
                                state = neighbourDisconnectionHandler(loneRegistered, i, neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            err = advertiseToEdge(neighbours->internal[i].fd, routingTable);
                            if (err) {
                                state = neighbourDisconnectionHandler(loneRegistered, i, neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            state = registered;
                            changedState = 1;
                            break;
                        }
                    }
                }
                break;

            case registered:
                maxfd = setFds(&rfds, neighbours);

                message = getMessageFromBuffer(neighbours->external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    switch (messageCode) {
                        case MC_EXTERN:
                            externMessageHandler(neighbours, &messageAddrInfo);
                            break;
                        case MC_ADVERTISE:
                            addNodeToRoutingTable(neighbours->external.fd, messageId, routingTable);
                            state = broadcastAdvertise(neighbours->external.fd, messageId, routingTable, registered, neighbours);
                            changedState = 1;
                            break;
                        case MC_WITHDRAW:
                            removeNodeFromRoutingTable(messageId, routingTable);
                            state = broadcastWithdraw(neighbours->external.fd, messageId, routingTable, registered, neighbours);
                            changedState = 1;
                            break;
                        case MC_INTEREST:
                            errFd = interestHandler(messageName, objectTable, interestTable, cache, routingTable, neighbours->external.fd);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        case MC_DATA:
                            errFd = dataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        case MC_NODATA:
                            errFd = noDataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        default:
                            break;
                    }
                }

                if (changedState) {
                    break;
                }

                for (int i = 0; i < neighbours->numberOfInternals; i++) {
                    while ((message = getMessageFromBuffer(neighbours->internal[i].readBuffer))) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        switch (messageCode) {
                            case MC_NEW:
                                err = newInternalHandler(neighbours, i, &messageAddrInfo);
                                if (err) {
                                    state = neighbourDisconnectionHandler(registered, i, neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                err = advertiseToEdge(neighbours->internal[i].fd, routingTable);
                                if (err) {
                                    state = neighbourDisconnectionHandler(registered, i, neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;

                            case MC_ADVERTISE:
                                addNodeToRoutingTable(neighbours->internal[i].fd, messageId, routingTable);
                                state = broadcastAdvertise(neighbours->internal[i].fd, messageId, routingTable, registered, neighbours);
                                changedState = 1;
                                break;

                            case MC_WITHDRAW:
                                removeNodeFromRoutingTable(messageId, routingTable);
                                state = broadcastWithdraw(neighbours->internal[i].fd, messageId, routingTable, registered, neighbours);
                                changedState = 1;
                                break;
                            case MC_INTEREST:
                                errFd = interestHandler(messageName, objectTable, interestTable, cache, routingTable, neighbours->internal[i].fd);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            case MC_DATA:
                                errFd = dataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            case MC_NODATA:
                                errFd = noDataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
                break;
            case waitingRecovery:
                maxfd = setFds(&rfds, neighbours);

                message = getMessageFromBuffer(neighbours->external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    switch (messageCode) {
                        case MC_EXTERN:
                            externMessageHandler(neighbours, &messageAddrInfo);
                            state = broadcastExtern(registered, neighbours, routingTable);
                            err = advertiseToEdge(neighbours->external.fd, routingTable);
                            if (err) {
                                state = neighbourDisconnectionHandler(registered, -1, neighbours, routingTable);
                            }
                            if (state == notRegistered) {
                                printf("\nsomething very wrong\n");
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                printPrompt = 1;
                                break;
                            }
                            changedState = 1;
                            break;
                        case MC_ADVERTISE:
                            addNodeToRoutingTable(neighbours->external.fd, messageId, routingTable);
                            state = broadcastAdvertise(neighbours->external.fd, messageId, routingTable, registered, neighbours);
                            changedState = 1;
                            break;
                        case MC_WITHDRAW:
                            removeNodeFromRoutingTable(messageId, routingTable);
                            state = broadcastWithdraw(neighbours->external.fd, messageId, routingTable, registered, neighbours);
                            changedState = 1;
                            break;

                        case MC_INTEREST:
                            errFd = interestHandler(messageName, objectTable, interestTable, cache, routingTable, neighbours->external.fd);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        case MC_DATA:
                            errFd = dataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        case MC_NODATA:
                            errFd = noDataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                            if (errFd) {
                                state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                changedState = 1;
                                break;
                            }
                            break;
                        default:
                            break;
                    }

                    if (changedState) {
                        break;
                    }
                }

                for (int i = 0; i < neighbours->numberOfInternals; i++) {
                    while ((message = getMessageFromBuffer(neighbours->internal[i].readBuffer))) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        switch (messageCode) {
                            case MC_NEW:
                                err = newInternalHandler(neighbours, i, &messageAddrInfo);
                                if (err) {
                                    state = neighbourDisconnectionHandler(state, i, neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;

                            case MC_ADVERTISE:
                                addNodeToRoutingTable(neighbours->internal[i].fd, messageId, routingTable);
                                state = broadcastAdvertise(neighbours->internal[i].fd, messageId, routingTable, registered, neighbours);
                                changedState = 1;
                                break;

                            case MC_WITHDRAW:
                                removeNodeFromRoutingTable(messageId, routingTable);
                                state = broadcastWithdraw(neighbours->internal[i].fd, messageId, routingTable, registered, neighbours);
                                changedState = 1;
                                break;

                            case MC_INTEREST:
                                errFd = interestHandler(messageName, objectTable, interestTable, cache, routingTable, neighbours->internal[i].fd);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            case MC_DATA:
                                errFd = dataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            case MC_NODATA:
                                errFd = noDataHandler(messageName, interestTable, cache, routingTable, &printPrompt);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
                break;
            default:;
        }

        if (changedState) {
            changedState = 0;
            continue;
        }

        if (printPrompt) {
            printPrompt = 0;
            printf("> ");
            fflush(stdout);
        }

        timeout.tv_sec = SEL_TIMEOUT;
        timeout.tv_usec = 0;

        counter = select(maxfd + 1, &rfds, NULL, NULL, &timeout);

        if (counter == 0) {
            removeStaleEntriesFromInterestTable(interestTable, &printPrompt);
        }

        for (; counter > 0; counter--) {
            switch (state) {
                case notRegistered:
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                strcpy(net, commandNet);
                                strcpy(id, commandId);
                                neighbours->external.addrressInfo = commandAddrInfo;
                                printf("Connecting...\n");
                                err = join(nodeServer, net, neighbours, id, routingTable);
                                if (!err) {
                                    if (neighbours->external.fd == 0) {
                                        printf("Succesfully joined network! 👏\n");
                                        state = loneRegistered;
                                    } else {
                                        state = waitingExtern;
                                    }
                                } else {
                                    printErrorMessage(err);
                                }
                                break;
                            case CC_JOINBOOT:
                                strcpy(net, commandNet);
                                strcpy(id, commandId);
                                neighbours->external.addrressInfo = commandAddrInfo;
                                printf("Connecting...\n");
                                err = join(nodeServer, net, neighbours, id, routingTable);
                                if (!err) {
                                    state = waitingExtern;
                                } else {
                                    printErrorMessage(err);
                                }
                                break;
                            case CC_EXIT:
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            default:
                                printf("error: network not joined\n");
                        }

                        printPrompt = 1;
                    }
                    break;
                case waitingExtern:
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                        }

                        printPrompt = 1;
                    }
                    //checks if external neighbour has sent a message
                    if (FD_ISSET(neighbours->external.fd, &rfds)) {
                        FD_CLR(neighbours->external.fd, &rfds);
                        ret = readTcpStreamToBuffer(neighbours->external.fd, neighbours->external.readBuffer, BUFFER_SIZE);
                        if (ret == -1) {
                            //read error occurred, better close connection
                            printErrorMessage(ret);
                            err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                            if (!err) {
                                printf("Succesfully left network\n");
                            } else {
                                printErrorMessage(err);
                            }
                            state = notRegistered;
                        }
                        if (ret == 1) {
                            //external neighbour closed connection on their side
                            printf("error: external node terminated connection before sending recovery node\n");
                            err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                            if (!err) {
                                printf("Succesfully left network\n");
                            } else {
                                printErrorMessage(err);
                            }
                            state = notRegistered;
                        }
                    }
                    break;
                case loneRegistered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            case CC_SHOWTOPOLOGY:
                                showTopology(neighbours);
                                break;
                            case CC_SHOWROUTING:
                                showRouting(routingTable);
                                break;
                            case CC_SHOWCACHE:
                                showCache(cache);
                                break;
                            case CC_CREATE:
                                err = createObject(commandName, objectTable, id);
                                if (err == -1) {
                                    printf("error: no space for more objects\n");
                                } else if (err == 1) {
                                    printf("error: object already exists, but thanks for reminding me 😊\n");
                                } else {
                                    printf("successfully created object %s.%s\n", id, commandName);
                                }
                                break;
                            case CC_GET:
                                errFd = getObject(commandName, objectTable, interestTable, cache, routingTable);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            default:
                                printf("error: already joined network\n");
                        }

                        printPrompt = 1;
                    }

                    if (changedState) {
                        changedState = 0;
                        break;
                    }

                    //checks if anyone is trying to connect to the network
                    if (FD_ISSET(neighbours->self.fd, &rfds)) {
                        FD_CLR(neighbours->self.fd, &rfds);
                        neighbours->internal[neighbours->numberOfInternals].fd = accept(neighbours->self.fd, NULL, NULL);
                        if (neighbours->internal[neighbours->numberOfInternals].fd == -1) {
                            printErrorMessage(-1);
                        }
                        neighbours->numberOfInternals++;
                        state = loneRegistered;
                    }

                    //checks if any of the internal neighbours sent a message
                    for (int i = 0; i < neighbours->numberOfInternals; i++) {
                        if (FD_ISSET(neighbours->internal[i].fd, &rfds)) {
                            FD_CLR(neighbours->internal[i].fd, &rfds);
                            ret = readTcpStreamToBuffer(neighbours->internal[i].fd, neighbours->internal[i].readBuffer, BUFFER_SIZE);
                            if (ret) {
                                if (ret == -1) {
                                    printErrorMessage(ret);
                                }
                                state = neighbourDisconnectionHandler(loneRegistered, i, neighbours, routingTable);
                                if (state == notRegistered) {
                                    printf("something very wrong\n");
                                    err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                    if (!err) {
                                        printf("Succesfully left network\n");
                                    } else {
                                        printErrorMessage(err);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case waitingRecovery:
                case registered:
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            case CC_SHOWTOPOLOGY:
                                showTopology(neighbours);
                                break;
                            case CC_SHOWROUTING:
                                showRouting(routingTable);
                                break;
                            case CC_SHOWCACHE:
                                showCache(cache);
                                break;
                            case CC_CREATE:
                                err = createObject(commandName, objectTable, id);
                                if (err == -1) {
                                    printf("error: no space for more objects\n");
                                } else if (err == 1) {
                                    printf("error: object already exists\n");
                                } else {
                                    printf("successfully created object %s.%s\n", id, commandName);
                                }
                                break;
                            case CC_GET:
                                errFd = getObject(commandName, objectTable, interestTable, cache, routingTable);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, neighbours), neighbours, routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;
                            default:
                                printf("error: already joined network\n");
                        }

                        printPrompt = 1;
                    }

                    if (changedState) {
                        changedState = 0;
                        break;
                    }

                    //checks if anyone is trying to connect to the network
                    if (FD_ISSET(neighbours->self.fd, &rfds)) {
                        FD_CLR(neighbours->self.fd, &rfds);
                        neighbours->internal[neighbours->numberOfInternals].fd = accept(neighbours->self.fd, NULL, NULL);
                        if (neighbours->internal[neighbours->numberOfInternals].fd == -1) {
                            printErrorMessage(-1);
                        }
                        neighbours->numberOfInternals++;
                    }

                    //checks if external neighbour has sent a message
                    if (FD_ISSET(neighbours->external.fd, &rfds)) {
                        FD_CLR(neighbours->external.fd, &rfds);
                        ret = readTcpStreamToBuffer(neighbours->external.fd, neighbours->external.readBuffer, BUFFER_SIZE);
                        if (ret) {
                            if (ret == -1) {
                                printErrorMessage(ret);
                            }
                            state = neighbourDisconnectionHandler(state, -1, neighbours, routingTable);
                            if (state == notRegistered) {
                                printf("something very wrong\n");
                                err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                if (!err) {
                                    printf("Succesfully left network\n");
                                } else {
                                    printErrorMessage(err);
                                }
                                break;
                            }
                        }
                    }

                    //checks if any of the internal neighbours sent a message
                    for (int i = 0; i < neighbours->numberOfInternals; i++) {
                        if (FD_ISSET(neighbours->internal[i].fd, &rfds)) {
                            FD_CLR(neighbours->internal[i].fd, &rfds);
                            ret = readTcpStreamToBuffer(neighbours->internal[i].fd, neighbours->internal[i].readBuffer, BUFFER_SIZE);
                            if (ret) {
                                if (ret == -1) {
                                    printErrorMessage(ret);
                                }
                                state = neighbourDisconnectionHandler(state, i, neighbours, routingTable);
                                if (state == notRegistered) {
                                    printf("something very wrong\n");
                                    err = leave(nodeServer, net, neighbours, routingTable, objectTable, interestTable, cache);
                                    if (!err) {
                                        printf("Succesfully left network\n");
                                    } else {
                                        printErrorMessage(err);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;

                default:;
            }
        }
    }
}