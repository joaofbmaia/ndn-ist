#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include "commandCodes.h"
#include "defines.h"
#include "network.h"
#include "neighbours.h"
#include "validation.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in  nodeServer;
    fd_set rfds;

    struct neighbours neighbours;

    int counter, command, maxfd, err;
    char buffer[BUFFER_SIZE];
    char net[BUFFER_SIZE], id[BUFFER_SIZE], name[BUFFER_SIZE];

    enum { notRegistered,
           loneRegistered,
           registered,
           goingOut } state;

    time_t t;

    memset(&neighbours, 0, sizeof neighbours);
    
    argumentParser(argc, argv, &neighbours.self.addrressInfo, &nodeServer);

    srand((unsigned) time(&t));

    state = notRegistered;
    printf("> ");
    fflush(stdout);
    while (state != goingOut) {
        FD_ZERO(&rfds);

        switch (state) {
            case notRegistered:
                maxfd = setFds(&rfds, &neighbours);
                break;

            case loneRegistered:
                maxfd = setFds(&rfds, &neighbours);
                break;
                
            case registered:
                maxfd = setFds(&rfds, &neighbours);
                break;
            default:;
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
                        command = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                printf("O gigante está entrando!\n");
                                err = join(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já entrou!\n");
                                    if (neighbours.external.fd == 0) {
                                        printf("O gigante está sozinho :c\n");
                                        state = loneRegistered;
                                    } else {
                                        state = registered;
                                    }
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_JOINBOOT:
                                //reg(char *net, char *id);
                                printf("O gigante está entrando, mas sem consentimento\n");
                                err = join(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já entrou!\n");
                                    state = registered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_EXIT:
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            default:
                                printf("error: network not joined\n");
                        }

                        if (state != goingOut) {
                            printf("> ");
                            fflush(stdout);
                        }
                    }
                    break;
                case loneRegistered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            default:
                                printf("error: already joined network\n");
                        }

                        if (state != goingOut) {
                            printf("> ");
                            fflush(stdout);
                        }
                    }
                    /*
                    if (FD_ISSET(listeningFd, &rfds)) {
                        internFd[internCounter] = accept(listeningFd, (struct sockaddr *) &nodeSelf, sizeof(nodeSelf));
                        if (internFd[internCounter] == -1) {
                            printf("error: %d\n", err);
                        }

                    }
                    */
                    break;
                    case registered:
                    // someone wrote in stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &neighbours.external.addrressInfo);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeServer, net, &neighbours);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                printf("Sayonara!\n");
                                state = goingOut;
                                break;
                            default:
                                printf("error: already joined network\n");
                        }

                        if (state != goingOut) {
                            printf("> ");
                            fflush(stdout);
                        }
                    }
                    break;
                default:;
            }
        }
    }

    return 0;
}
