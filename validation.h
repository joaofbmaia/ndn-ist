#ifndef VALIDATION_H
#define VALIDATION_H

#include <arpa/inet.h>

void argumentParser(int argc, char *argv[], struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer);

#endif
