#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "commandCodes.h"
#include "validation.h"
#include "network.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeSelf, nodeServer, nodeExtern;

    fd_set rfds;
    int counter, command, maxfd, nodeServerConnection;

    char buffer[1024];
    char net[256], id[256], name[256];

    enum { notReg,
           regWait,
           goingOut } state;

    argumentParser(argc, argv, &nodeSelf, &nodeServer);

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
            case regWait:
                //FD_SET();
                break;
            default:;
        }

        counter = select(maxfd + 1, &rfds, NULL, NULL, NULL);

        for (; counter; counter--) {
            switch (state) {
                case notReg:
                    // Alguem escreveu no stdin
                    if (FD_ISSET(fileno(stdin), &rfds)) {
                        FD_CLR(fileno(stdin), &rfds);
                        fgets(buffer, 1024, stdin);
                        command = commandParser(buffer, net, id, name, &nodeExtern);
                        switch (command) {
                            case CC_ERROR:
                                break;
                            case CC_JOIN:
                                nodeServerConnection = reg(&nodeSelf, &nodeServer, net);
                                if(nodeServerConnection > 0) {
                                printf("O gigante está entrando\n");
                                state = regWait;
                                }
                                else {
                                printf("error: failed to send registration message\n");   
                                }
                                break;
                            case CC_JOINBOOT:
                                //reg(char *net, char *id);
                                printf("O gigante está entrando, mas sem consentimento\n");
                                state = regWait;
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
