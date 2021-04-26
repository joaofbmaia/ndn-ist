#ifndef PARSER_H
#define PARSER_H

#include <arpa/inet.h>

void argumentParser(int argc, char *argv[], struct sockaddr_in *nodeSelf, struct sockaddr_in *nodeServer);
int commandParser(char *buffer, char *net, char *id, char *name, struct sockaddr_in *nodeExtern);
int messageParser(char *buffer, char *id, char *name, struct sockaddr_in *addrinfo);

#endif
