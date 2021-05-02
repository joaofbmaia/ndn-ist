#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "eventLoop.h"
#include "neighbours.h"
#include "parser.h"
#include "routing.h"
#include "search.h"
#include "topology.h"

int main(int argc, char *argv[]) {
    struct sockaddr_in nodeServer;

    // when declared as static variable will be stored in data segment, instead of stack. No more stack overflows 😎
    static struct neighbours neighbours;
    static struct routingTable routingTable;
    static struct objectTable objectTable;
    static struct interestTable interestTable;
    static struct cache cache;

    struct sigaction act;

    // set SIGPIPE action to ignore
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
    }

    srand((unsigned) time(NULL));

    memset(&neighbours, 0, sizeof neighbours);
    memset(&routingTable, 0, sizeof routingTable);
    memset(&objectTable, 0, sizeof objectTable);
    memset(&interestTable, 0, sizeof interestTable);
    memset(&cache, 0, sizeof cache);

    argumentParser(argc, argv, &neighbours.self.addrressInfo, &nodeServer);

    eventLoop(&nodeServer, &neighbours, &routingTable, &objectTable, &interestTable, &cache);

    return 0;
}