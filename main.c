#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
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

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeServer;
    fd_set rfds;

    // when declared as static variable will be stored in data segment, instead of stack. No more stack overflows 😎
    static struct neighbours neighbours;
    static struct routingTable routingTable;
    static struct objectTable objectTable;
    static struct interestTable interestTable;
    static struct cache cache;

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

    time_t t;

    struct sigaction act;

    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
    }

    memset(&neighbours, 0, sizeof neighbours);
    memset(&routingTable, 0, sizeof routingTable);
    memset(&objectTable, 0, sizeof objectTable);
    memset(&interestTable, 0, sizeof interestTable);
    memset(&cache, 0, sizeof cache);

    argumentParser(argc, argv, &neighbours.self.addrressInfo, &nodeServer);

    srand((unsigned) time(&t));

    state = notRegistered;
    changedState = 0;
    printPrompt = 1;
    while (state != goingOut) {
        FD_ZERO(&rfds);

        switch (state) {
            case notRegistered:
                maxfd = setFds(&rfds, &neighbours);
                break;

            case waitingExtern:
                maxfd = setFds(&rfds, &neighbours);
                message = getMessageFromBuffer(neighbours.external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    if (messageCode == MC_EXTERN) {
                        err = finishJoin(&neighbours, &messageAddrInfo, &nodeServer, net);
                        if (err) {
                            changedState = 1;
                            err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                            if (err) {
                                printf("Erro no unreg do servidor de nós\n");
                            }
                            printf("O gigante não consegui entrar :c\n");
                            state = notRegistered;
                            break;
                        }
                        printf("O gigante já entrou!\n");
                        state = registered;
                        //send ADVERTISE messages
                        err = advertiseToEdge(neighbours.external.fd, &routingTable);
                        if (err) {
                            state = neighbourDisconnectionHandler(registered, -1, &neighbours, &routingTable);
                        }
                        changedState = 1;
                        break;
                    }
                }

                break;

            case loneRegistered:
                maxfd = setFds(&rfds, &neighbours);
                for (int i = 0; i < neighbours.numberOfInternals; i++) {
                    message = getMessageFromBuffer(neighbours.internal[i].readBuffer);
                    if (message) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        if (messageCode == MC_NEW) {
                            err = loneNewInternalHandler(&neighbours, i, &messageAddrInfo);
                            if (err) {
                                state = neighbourDisconnectionHandler(loneRegistered, i, &neighbours, &routingTable);
                                changedState = 1;
                                break;
                            }
                            //send ADVERTISE messages
                            err = advertiseToEdge(neighbours.internal[i].fd, &routingTable);
                            if (err) {
                                state = neighbourDisconnectionHandler(loneRegistered, i, &neighbours, &routingTable);
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
                maxfd = setFds(&rfds, &neighbours);

                message = getMessageFromBuffer(neighbours.external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    switch (messageCode) {
                        case MC_EXTERN:
                            externMessageHandler(&neighbours, &messageAddrInfo);
                            break;
                        case MC_ADVERTISE:
                            addNodeToRoutingTable(neighbours.external.fd, messageId, &routingTable);
                            state = broadcastAdvertise(neighbours.external.fd, messageId, &routingTable, registered, &neighbours);
                            changedState = 1;
                            break;
                        case MC_WITHDRAW:
                            removeNodeFromRoutingTable(messageId, &routingTable);
                            state = broadcastWithdraw(neighbours.external.fd, messageId, &routingTable, registered, &neighbours);
                            changedState = 1;
                            break;
                        default:
                            break;
                    }
                }

                if (changedState) {
                    break;
                }

                for (int i = 0; i < neighbours.numberOfInternals; i++) {
                    while ((message = getMessageFromBuffer(neighbours.internal[i].readBuffer))) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        switch (messageCode) {
                            case MC_NEW:
                                err = newInternalHandler(&neighbours, i, &messageAddrInfo);
                                if (err) {
                                    state = neighbourDisconnectionHandler(registered, i, &neighbours, &routingTable);
                                    changedState = 1;
                                    break;
                                }
                                //send ADVERTISE messages
                                err = advertiseToEdge(neighbours.internal[i].fd, &routingTable);
                                if (err) {
                                    state = neighbourDisconnectionHandler(registered, i, &neighbours, &routingTable);
                                    changedState = 1;
                                    break;
                                }
                                break;

                            case MC_ADVERTISE:
                                addNodeToRoutingTable(neighbours.internal[i].fd, messageId, &routingTable);
                                state = broadcastAdvertise(neighbours.internal[i].fd, messageId, &routingTable, registered, &neighbours);
                                changedState = 1;
                                break;

                            case MC_WITHDRAW:
                                removeNodeFromRoutingTable(messageId, &routingTable);
                                state = broadcastWithdraw(neighbours.internal[i].fd, messageId, &routingTable, registered, &neighbours);
                                changedState = 1;
                                break;

                            default:
                                break;
                        }
                    }
                }
                break;
            case waitingRecovery:
                maxfd = setFds(&rfds, &neighbours);

                message = getMessageFromBuffer(neighbours.external.readBuffer);
                if (message) {
                    messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                    switch (messageCode) {
                        case MC_EXTERN:
                            externMessageHandler(&neighbours, &messageAddrInfo);
                            state = broadcastExtern(registered, &neighbours, &routingTable);
                            //send ADVERTISE messages
                            err = advertiseToEdge(neighbours.external.fd, &routingTable);
                            if (err) {
                                state = neighbourDisconnectionHandler(registered, -1, &neighbours, &routingTable);
                            }
                            if (state == notRegistered) {
                                printf("smthing wong\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            }
                            changedState = 1;
                            break;
                        case MC_ADVERTISE:
                            addNodeToRoutingTable(neighbours.external.fd, messageId, &routingTable);
                            state = broadcastAdvertise(neighbours.external.fd, messageId, &routingTable, registered, &neighbours);
                            changedState = 1;
                            break;
                        case MC_WITHDRAW:
                            removeNodeFromRoutingTable(messageId, &routingTable);
                            state = broadcastWithdraw(neighbours.external.fd, messageId, &routingTable, registered, &neighbours);
                            changedState = 1;
                            break;
                        default:
                            break;
                    }

                    if (changedState) {
                        break;
                    }
                }

                for (int i = 0; i < neighbours.numberOfInternals; i++) {
                    while ((message = getMessageFromBuffer(neighbours.internal[i].readBuffer))) {
                        messageCode = messageParser(message, messageId, messageName, &messageAddrInfo);
                        switch (messageCode) {
                            case MC_NEW:
                                err = newInternalHandler(&neighbours, i, &messageAddrInfo);
                                if (err) {
                                    printf("error: connection attempt from internal node failed\n");
                                    changedState = 1;
                                    break;
                                }
                                break;

                            case MC_ADVERTISE:
                                addNodeToRoutingTable(neighbours.internal[i].fd, messageId, &routingTable);
                                state = broadcastAdvertise(neighbours.internal[i].fd, messageId, &routingTable, registered, &neighbours);
                                changedState = 1;
                                break;

                            case MC_WITHDRAW:
                                removeNodeFromRoutingTable(messageId, &routingTable);
                                state = broadcastWithdraw(neighbours.internal[i].fd, messageId, &routingTable, registered, &neighbours);
                                changedState = 1;
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

        counter = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        //if select goes wrong maybe put a message
        for (; counter > 0; counter--) {
            switch (state) {
                case notRegistered:
                    // someone wrote in stdin
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
                                neighbours.external.addrressInfo = commandAddrInfo;
                                printf("O gigante está entrando!\n");
                                err = join(&nodeServer, net, &neighbours, id, &routingTable);
                                if (!err) {
                                    if (neighbours.external.fd == 0) {
                                        printf("O gigante já entrou!\n");
                                        printf("O gigante está sozinho :c\n");
                                        state = loneRegistered;
                                    } else {
                                        state = waitingExtern;
                                    }
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_JOINBOOT:
                                strcpy(net, commandNet);
                                strcpy(id, commandId);
                                neighbours.external.addrressInfo = commandAddrInfo;
                                printf("O gigante está entrando, mas sem consentimento\n");
                                err = join(&nodeServer, net, &neighbours, id, &routingTable);
                                if (!err) {
                                    state = waitingExtern;
                                } else {
                                    printf("error: %d\n", err);
                                }

                                //process extra commands present in read buffer (TO BE IMPLEMENTED)
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
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                        }

                        printPrompt = 1;
                    }
                    //checks if external neighbour has sent a message
                    if (FD_ISSET(neighbours.external.fd, &rfds)) {
                        FD_CLR(neighbours.external.fd, &rfds);
                        ret = readTcpStreamToBuffer(neighbours.external.fd, neighbours.external.readBuffer, BUFFER_SIZE);
                        if (ret == -1) {
                            //read error occurred, better close connection
                            printf("error: -1\n");
                            err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                            if (!err) {
                                printf("O gigante já saiu!\n");
                            } else {
                                printf("error: %d\n", err);
                            }
                            state = notRegistered;
                        }
                        if (ret == 1) {
                            //external neighbour closed connection on their side
                            printf("error: external node terminated connection before sending recovery node\n");
                            err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                            if (!err) {
                                printf("O gigante já saiu!\n");
                            } else {
                                printf("error: %d\n", err);
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
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            case CC_SHOWTOPOLOGY:
                                showTopology(&neighbours);
                                break;
                            case CC_SHOWROUTING:
                                showRouting(&routingTable);
                                break;
                            case CC_SHOWCACHE:
                                showCache(&cache);
                                break;
                            case CC_CREATE:
                                err = createObject(commandName, &objectTable, id);
                                if (err) {
                                    printf("error: no space for more objects\n");
                                }
                                else {
                                    printf("successfully created object %s.%s\n", id, commandName);
                                }
                                break;
                            case CC_GET:
                                errFd = getObject(commandName, &objectTable, &interestTable, &cache, &routingTable, id);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, &neighbours), &neighbours, &routingTable);
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
                    if (FD_ISSET(neighbours.self.fd, &rfds)) {
                        FD_CLR(neighbours.self.fd, &rfds);
                        neighbours.internal[neighbours.numberOfInternals].fd = accept(neighbours.self.fd, NULL, NULL);
                        if (neighbours.internal[neighbours.numberOfInternals].fd == -1) {
                            printf("error: -1\n");
                        }
                        neighbours.numberOfInternals++;
                        state = loneRegistered;
                    }

                    //checks if any of the internal neighbours sent a message
                    for (int i = 0; i < neighbours.numberOfInternals; i++) {
                        if (FD_ISSET(neighbours.internal[i].fd, &rfds)) {
                            FD_CLR(neighbours.internal[i].fd, &rfds);
                            ret = readTcpStreamToBuffer(neighbours.internal[i].fd, neighbours.internal[i].readBuffer, BUFFER_SIZE);
                            if (ret) {
                                if (ret == -1) {
                                    printf("error: -1\n");
                                }
                                state = neighbourDisconnectionHandler(loneRegistered, i, &neighbours, &routingTable);
                                if (state == notRegistered) {
                                    printf("smthing wong\n");
                                    err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                    if (!err) {
                                        printf("O gigante já saiu!\n");
                                    } else {
                                        printf("error: %d\n", err);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case waitingRecovery:
                case registered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, commandNet, commandId, commandName, &commandAddrInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            case CC_SHOWTOPOLOGY:
                                showTopology(&neighbours);
                                break;
                            case CC_SHOWROUTING:
                                showRouting(&routingTable);
                                break;
                            case CC_SHOWCACHE:
                                showCache(&cache);
                                break;
                            case CC_CREATE:
                                err = createObject(commandName, &objectTable, id);
                                if (err) {
                                    printf("error: no space for more objects\n");
                                }
                                else {
                                    printf("successfully created object %s.%s\n", id, commandName);
                                }
                                break;
                            case CC_GET:
                                errFd = getObject(commandName, &objectTable, &interestTable, &cache, &routingTable, id);
                                if (errFd) {
                                    state = neighbourDisconnectionHandler(state, fdToIndex(errFd, &neighbours), &neighbours, &routingTable);
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
                    if (FD_ISSET(neighbours.self.fd, &rfds)) {
                        FD_CLR(neighbours.self.fd, &rfds);
                        neighbours.internal[neighbours.numberOfInternals].fd = accept(neighbours.self.fd, NULL, NULL);
                        if (neighbours.internal[neighbours.numberOfInternals].fd == -1) {
                            printf("error: -1\n");
                        }
                        neighbours.numberOfInternals++;
                    }

                    //checks if external neighbour has sent a message
                    if (FD_ISSET(neighbours.external.fd, &rfds)) {
                        FD_CLR(neighbours.external.fd, &rfds);
                        ret = readTcpStreamToBuffer(neighbours.external.fd, neighbours.external.readBuffer, BUFFER_SIZE);
                        if (ret) {
                            if (ret == -1) {
                                printf("error: -1\n");
                            }
                            state = neighbourDisconnectionHandler(state, -1, &neighbours, &routingTable);
                            if (state == notRegistered) {
                                printf("smthing wong\n");
                                err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            }
                        }
                    }

                    //checks if any of the internal neighbours sent a message
                    for (int i = 0; i < neighbours.numberOfInternals; i++) {
                        if (FD_ISSET(neighbours.internal[i].fd, &rfds)) {
                            FD_CLR(neighbours.internal[i].fd, &rfds);
                            ret = readTcpStreamToBuffer(neighbours.internal[i].fd, neighbours.internal[i].readBuffer, BUFFER_SIZE);
                            if (ret) {
                                if (ret == -1) {
                                    printf("error: -1\n");
                                }
                                state = neighbourDisconnectionHandler(state, i, &neighbours, &routingTable);
                                if (state == notRegistered) {
                                    printf("smthing wong\n");
                                    err = leave(&nodeServer, net, &neighbours, &routingTable, &objectTable, &interestTable, &cache);
                                    if (!err) {
                                        printf("O gigante já saiu!\n");
                                    } else {
                                        printf("error: %d\n", err);
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

    return 0;
}