#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include "neighbours.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*Sets fds and returns max fd
Write complete header at a later stage*/
int setFds(fd_set *rfds, struct neighbours *neighbours) {
    int maxFd;
    //sets stdin fd
    FD_SET(fileno(stdin), rfds);
    maxFd = fileno(stdin);
    //if needed sets listening fd
    if (neighbours->self.fd > 0) {
        FD_SET(neighbours->self.fd, rfds);
        maxFd = MAX(neighbours->self.fd, maxFd);
    }
    //if needed sets extern fd
    if (neighbours->external.fd > 0) {
        FD_SET(neighbours->external.fd, rfds);
        maxFd = MAX(neighbours->external.fd, maxFd);
    }
    //if needed sets intern fds
    for (int i = 0; i < neighbours->numberOfInternals; i++) {
        FD_SET(neighbours->internal[i].fd, rfds);
        maxFd = MAX(neighbours->internal[i].fd, maxFd);
    }
    return maxFd;
}

/*writes message 
Write header at a later stage*/
int writeBufferToTcpStream(int fd, char *writeBuffer) {
    int bytesLeft = strlen(writeBuffer);
    int bytesWritten = 0;

    //cycle keeps repeating until message is fully written
    while (bytesLeft > 0) {
        bytesWritten = write(fd, &writeBuffer[bytesWritten], bytesLeft);
        //error in function write: use errno
        if (bytesWritten == -1) {
            return -1;
        }
        bytesLeft -= bytesWritten;
    }

    return 0;
}

/*Reads message to read buffer
Write header at a later stage*/
int readTcpStreamToBuffer(int fd, char *readBuffer, int bufferSize) {
    int bytesRead;
    char *stringTerminatorLocation;

    stringTerminatorLocation = strchr(readBuffer, '\0');

    //appends message or partial message to read buffer
    bytesRead = read(fd, stringTerminatorLocation, bufferSize);
    if (bytesRead == -1) {
        return -1;
    }
    if (bytesRead = 0) {
        return 1;
    }

    //terminates string
    stringTerminatorLocation[bytesRead] = '\0';

    return 0;
}

/*Gets a full message from the read buffer
Write header at a later stage*/
char *getMessageFromBuffer(char *buffer) {
    int messageSize;
    char *newlineLocation;
    static char message[BUFFER_SIZE];
    char temp[BUFFER_SIZE];

    
    //looks for newline character
    newlineLocation = strchr(buffer, '\n');
    
    //if it does not find a new line there is no message to be prcessed
    if (newlineLocation == NULL) {
        return NULL;
    }

    //if it finds a message it copies it and deletes it from buffer
    messageSize = newlineLocation - buffer + 1;
    strncpy(message, buffer, messageSize);
    message[messageSize] = '\0';

    //remove message from buffer and copy remaining data in buffer to the beggining
    strcpy(temp, newlineLocation + 1);
    strcpy(buffer, temp);

    return message;
}

void removeInternalFromTable(int internalIndex, struct neighbours *neighbours) {
    for(int  i = internalIndex + 1; i < neighbours->numberOfInternals; i++) {
        neighbours->internal[i - 1] = neighbours->internal[i];
    }
    memset(&neighbours->internal[neighbours->numberOfInternals - 1], 0, sizeof neighbours->internal[neighbours->numberOfInternals - 1]);
    neighbours->numberOfInternals--;
}