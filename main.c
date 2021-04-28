#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include "defines.h"
#include "neighbours.h"
#include "parser.h"
#include "returnCodes.h"
#include "topology.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeServer;
    fd_set rfds;

    // when declared as static, will be stored in data segmente, instead of stack. No more stack overflows 😎
    static struct neighbours neighbours;

    int counter, commandCode, maxfd, err, messageCode, ret;
    char buffer[BUFFER_SIZE];
    char net[BUFFER_SIZE], id[BUFFER_SIZE], name[BUFFER_SIZE];
    char messageId[BUFFER_SIZE], messageName[BUFFER_SIZE];
    struct sockaddr_in messageAddrInfo;
    char *message;
    socklen_t addrlen = 0;
    struct sockaddr addr;

    int changedState;
    int printPrompt;

    enum { notRegistered,
           waitingExtern,
           loneRegistered,
           registered,
           waitingRecovery,
           goingOut } state;

    time_t t;

    memset(&neighbours, 0, sizeof neighbours);

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
                        err = externMessageHandler(&neighbours, &messageAddrInfo, &nodeServer, net);
                        if (err) {
                            changedState = 1;
                            err = leave(&nodeServer, net, &neighbours);
                            if (err) {
                                printf("Erro no unreg do servidor de nós\n");
                            }
                            printf("O gigante não consegui entrar :c\n");
                            state = notRegistered;
                            break;
                        }
                        printf("O gigante já entrou!\n");
                        state = registered;
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
                                state = loneRegistered;
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
                        commandCode = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                printf("O gigante está entrando!\n");
                                err = join(&nodeServer, net, &neighbours);
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
                                printf("O gigante está entrando, mas sem consentimento\n");
                                err = join(&nodeServer, net, &neighbours);
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
                        commandCode = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
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
                            closeExternal(&neighbours);
                            state = notRegistered;
                        }
                        if (ret == 1) {
                            //external neighbour closed connection on their side
                            printf("error: external node terminated connection before sending recovery node\n");
                            closeExternal(&neighbours);
                            state = notRegistered;
                        }
                    }
                    break;
                case loneRegistered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
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
                            default:
                                printf("error: already joined network\n");
                        }

                        printPrompt = 1;
                    }
                    //checks if anyone is trying to connect to the network
                    if (FD_ISSET(neighbours.self.fd, &rfds)) {
                        FD_CLR(neighbours.self.fd, &rfds);
                        neighbours.internal[neighbours.numberOfInternals].fd = accept(neighbours.self.fd, &addr, &addrlen);
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
                            if (ret == -1) {
                                //read error occurred, better close connection
                                printf("error: -1\n");
                                closeInternal(i, &neighbours);
                            }
                            if (ret == 1) {
                                //internal neighbour closed connection on their side
                                closeInternal(i, &neighbours);
                            }
                        }
                    }
                    break;
                case registered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        commandCode = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (commandCode) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                } else {
                                    printf("error: %d\n", err);
                                }
                                state = notRegistered;
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
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
                            default:
                                printf("error: already joined network\n");
                        }

                        printPrompt = 1;
                    }
                    //checks if anyone is trying to connect to the network
                    if (FD_ISSET(neighbours.self.fd, &rfds)) {
                        FD_CLR(neighbours.self.fd, &rfds);
                        neighbours.internal[neighbours.numberOfInternals].fd = accept(neighbours.self.fd, &addr, &addrlen);
                        if (neighbours.internal[neighbours.numberOfInternals].fd == -1) {
                            printf("error: -1\n");
                        }
                        neighbours.numberOfInternals++;
                    }

                    //checks if external neighbour has sent a message
                    if (FD_ISSET(neighbours.external.fd, &rfds)) {
                        FD_CLR(neighbours.external.fd, &rfds);
                        ret = readTcpStreamToBuffer(neighbours.external.fd, neighbours.external.readBuffer, BUFFER_SIZE);
                        if (ret == -1) {
                            //read error occurred, better close connection
                            printf("error: -1\n");
                            closeExternal(&neighbours);
                            //if recovery node is itself
                            if (neighbours.recovery.addrressInfo.sin_addr.s_addr == neighbours.self.addrressInfo.sin_addr.s_addr && neighbours.recovery.addrressInfo.sin_port == neighbours.self.addrressInfo.sin_port) {
                                //if you are the only node remaining
                                if(neighbours.numberOfInternals == 0){
                                    neighbours.external.addrressInfo = neighbours.self.addrressInfo;
                                    state = loneRegistered;
                                    break;
                                }
                                promoteRandomInternalToExternal(&neighbours);
                                broadcastExtern(&neighbours);
                            } else {
                                err = connectToRecovery(&neighbours);
                                if (err = -10) {
                                    printf("error: could not connect to recovery node. leaving.\n");
                                    err = leave(&nodeServer, net, &neighbours);
                                    if (!err) {
                                        printf("O gigante já saiu!\n");
                                    } else {
                                        printf("error: %d\n", err);
                                    }
                                    state = notRegistered;
                                    break;
                                }
                                state = waitingRecovery;
                            }
                        }
                        if (ret == 1) {
                            //external neighbour closed connection on their side
                            printf("error: external node terminated connection before sending recovery node\n");
                            closeExternal(&neighbours);
                            //if recovery node is itself
                            if (neighbours.recovery.addrressInfo.sin_addr.s_addr == neighbours.self.addrressInfo.sin_addr.s_addr && neighbours.recovery.addrressInfo.sin_port == neighbours.self.addrressInfo.sin_port) {
                                //if you are the only node remaining
                                if(neighbours.numberOfInternals == 0){
                                    neighbours.external.addrressInfo = neighbours.self.addrressInfo;
                                    state = loneRegistered;
                                    break;
                                }
                                promoteRandomInternalToExternal(&neighbours);
                                broadcastExtern(&neighbours);
                            } else {
                                err = connectToRecovery(&neighbours);
                                if (err = -10) {
                                    printf("error: could not connect to recovery node. leaving.\n");
                                    err = leave(&nodeServer, net, &neighbours);
                                    if (!err) {
                                        printf("O gigante já saiu!\n");
                                    } else {
                                        printf("error: %d\n", err);
                                    }
                                    state = notRegistered;
                                    break;
                                }
                                state = waitingRecovery;
                            }
                        }
                    }

                    //checks if any of the internal neighbours sent a message
                    for (int i = 0; i < neighbours.numberOfInternals; i++) {
                        if (FD_ISSET(neighbours.internal[i].fd, &rfds)) {
                            FD_CLR(neighbours.internal[i].fd, &rfds);
                            ret = readTcpStreamToBuffer(neighbours.internal[i].fd, neighbours.internal[i].readBuffer, BUFFER_SIZE);
                            if (ret == -1) {
                                //read error occurred, better close connection
                                printf("error: -1\n");
                                //verefy that internal is not also external node
                                closeInternal(i, &neighbours);
                                //send withdraw messages
                            }
                            if (ret == 1) {
                                //internal neighbour closed connection on their side
                                closeInternal(i, &neighbours);
                                //send withdraw messages
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
