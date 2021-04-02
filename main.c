#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <stdlib.h>
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

    enum { notReg,
           registered,
           goingOut } state;

    time_t t;

    argumentParser(argc, argv, &nodeSelf, &nodeServer);

    srand((unsigned) time(&t));
    
    memset(&nodeExtern, 0, sizeof nodeExtern);
    

    state = notReg;
    printf("> ");
    fflush(stdout);
    while (state != goingOut) {
        FD_ZERO(&rfds);

        switch (state) {
            case notReg:
                FD_SET(fileno(stdin), &rfds);
                maxfd = fileno(stdin);
                break;
            default:;
        }

        counter = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        //if select goes wrong maybe put a message
        for (; counter > 0; counter--) {
            switch (state) {
                case notReg:
                    // Algtr quem escreveu no stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &nodeExtern);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                err = join(&nodeSelf, &nodeServer, &nodeExtern, &recoveryNode, net, &externFd, &listeningFd);
                                if (externFd >= 0) {
                                    printf("Fáaaacil!\n");
                                    state = registered;
                                } else {
                                    printf("error: %d\n", err);
                                }
                                break;
                            case CC_JOINBOOT:
                                //reg(char *net, char *id);
                                printf("O gigante está entrando, mas sem consentimento\n");
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

                default:;
            }
        }
    }

    return 0;
}
