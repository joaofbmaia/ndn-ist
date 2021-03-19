#include "validation.h"
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeSelf, nodeServer;

    argumentParser(argc, argv, &nodeSelf, &nodeServer);

    return 0;
}