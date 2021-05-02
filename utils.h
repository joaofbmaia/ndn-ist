#ifndef UTILS_H
#define UTILS_H

#include <sys/select.h>
#include "neighbours.h"

int setFds(fd_set *rfds, struct neighbours *neighbours);
int writeBufferToTcpStream(int fd, char *writeBuffer);
int readTcpStreamToBuffer(int fd, char* readBuffer, int bufferSize);
char *getMessageFromBuffer(char *buffer);
void removeInternalFromTable(int internalIndex, struct neighbours *neighbours);
int fdToIndex(int fd, struct neighbours *neighbours); 
void printErrorMessage (int errCode);

#endif