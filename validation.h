#ifndef VALIDATION_H
#define VALIDATION_H

#include <arpa/inet.h>

void argumentParser(int argc, char *argv[], struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer);
int commandParser(char *buffer, char *net, char *id, char *name, struct sockaddr_in *nodeExtern);

#endif
