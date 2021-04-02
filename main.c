#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include "commandCodes.h"
#include "defines.h"
#include "network.h"
#include "validation.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeSelf, nodeServer, nodeExtern, recoveryNode;

    fd_set rfds;
    int counter, command, maxfd, err;
    int externFd, listeningFd;
    char buffer[BUFFER_SIZE];
    char net[BUFFER_SIZE], id[BUFFER_SIZE], name[BUFFER_SIZE];

    enum { notRegistered,
           loneRegistered,
           registered,
           goingOut } state;

    time_t t;

    argumentParser(argc, argv, &nodeSelf, &nodeServer);

    srand((unsigned) time(&t));

    memset(&nodeExtern, 0, sizeof nodeExtern);

    state = notRegistered;
    printf("> ");
    fflush(stdout);
    while (state != goingOut) {
        FD_ZERO(&rfds);

        switch (state) {
            case notRegistered:
                FD_SET(fileno(stdin), &rfds);
                maxfd = fileno(stdin);
                break;

            case loneRegistered:
                FD_SET(fileno(stdin), &rfds);
                maxfd = fileno(stdin);
                break;
                
            case registered:
                FD_SET(fileno(stdin), &rfds);
                maxfd = fileno(stdin);
                break;
            default:;
        }

        counter = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        //if select goes wrong maybe put a message
        for (; counter > 0; counter--) {
            switch (state) {
                case notRegistered:
                    // Algtr quem escreveu no stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &nodeExtern);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                printf("O gigante está entrando!\n");
                                err = join(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
                                if (!err) {
                                    printf("O gigante já entrou!\n");
                                    if (externFd == -1) {
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
                                err = join(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
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
                    // Algtr quem escreveu no stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &nodeExtern);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
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
                    case registered:
                    // Algtr quem escreveu no stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &nodeExtern);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_LEAVE:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
                                if (!err) {
                                    printf("O gigante já saiu!\n");
                                    state = notRegistered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_EXIT:
                                printf("O gigante está saindo\n");
                                err = leave(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
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
