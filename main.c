#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "validation.h"
#include "commandCodes.h"


int main(int argc, char *argv[]) {
    struct sockaddr_in nodeSelf, nodeServer, nodeExtern;

    fd_set rfds;
    int counter, command;

    char buffer[1024];
    char net[256], id[256], name[256];

    argumentParser(argc, argv, &nodeSelf, &nodeServer);

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fileno(stdout), &rfds);

        counter = select(fileno(stdout) + 1, &rfds, NULL, NULL, NULL);

        for (; counter; counter--) {
            // Alguem escreveu no stdin
            if (FD_ISSET(fileno(stdout), &rfds)) {
                fgets(buffer, 1024, stdin);
                command = commandParser(buffer, net, id, name, &nodeExtern);
                printf("%d\n", command);
            }
        }
    }

    return 0;
}
