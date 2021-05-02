#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include "neighbours.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/******************************************************************************
 * setFds()
 *
 * Arguments: rfds - read file descriptor set
 *            neighbours - struct with all topology information
 * Returns:   File descriptor with highest numerical value
 * Side-Effects: 
 *
 * Description: Puts the appropriate file descriptors in set
 *              and indetefies the maximum file descriptor.
 *****************************************************************************/
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

/******************************************************************************
 * writeBufferToTcpSream()
 *
 * Arguments: fd - target file descriptor
 *            writeBuffer - buffer with the message to be written
 * Returns:   0 if ok or -1 if error
 * Side-Effects: 
 *
 * Description: Writes a message to a tcp stream.
 *****************************************************************************/
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


/******************************************************************************
 * readTcpSreamTBuffer()
 *
 * Arguments: fd - target file descriptor
 *            readBuffer - buffer with the message to be read
 *            bufferSize - read buffer size
 * Returns:   0 if ok, -1 if error, or 1 if zero bytes read
 * Side-Effects: 
 *
 * Description: Reads a message from a tcp stream.
 *****************************************************************************/
int readTcpStreamToBuffer(int fd, char *readBuffer, int bufferSize) {
    int bytesRead;
    char *stringTerminatorLocation;

    stringTerminatorLocation = strchr(readBuffer, '\0');

    //appends message or partial message to read buffer
    bytesRead = read(fd, stringTerminatorLocation, bufferSize);
    if (bytesRead == -1) {
        return -1;
    }
    if (bytesRead == 0) {
        return 1;
    }

    //terminates string
    stringTerminatorLocation[bytesRead] = '\0';

    return 0;
}

/******************************************************************************
 * writeBufferToTcpSream()
 *
 * Arguments: buffer - buffer containing the message
 * Returns:   message retrieved
 * Side-Effects: 
 *
 * Description: Retrives a message from a buffer.
 *****************************************************************************/
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

/******************************************************************************
 * removeInternalFromTable()
 *
 * Arguments: internalIndex - index of internal to be removed
 *            neighbours - struct with all topology information
 * Returns:   
 * Side-Effects: 
 *
 * Description: Removes an internal neighbour from the table of
 *              internals.
 *****************************************************************************/
void removeInternalFromTable(int internalIndex, struct neighbours *neighbours) {
    for(int  i = internalIndex + 1; i < neighbours->numberOfInternals; i++) {
        neighbours->internal[i - 1] = neighbours->internal[i];
    }
    memset(&neighbours->internal[neighbours->numberOfInternals - 1], 0, sizeof neighbours->internal[neighbours->numberOfInternals - 1]);
    neighbours->numberOfInternals--;
}

/******************************************************************************
 * fdToIndex()
 *
 * Arguments: fd - file descriptor to be converted
 *            neighbours - struct with all topology information
 * Returns:   index of the converted file descriptor
 * Side-Effects: 
 *
 * Description: Converts a file descriptor into an index for the 
 *              table of internals.
 *****************************************************************************/
int fdToIndex(int fd, struct neighbours *neighbours) {
    for (int i = 0; i < neighbours->numberOfInternals; i++) {
        if (neighbours->internal[i].fd == fd) return i;
    }
    if (neighbours->external.fd == fd) return -1;
    return -2;
}

/******************************************************************************
 * printErrorMessage()
 *
 * Arguments: errCode - number that identifies error type
 * Returns:   
 * Side-Effects: 
 *
 * Description: Prints error message depending on error type.
 *****************************************************************************/
void printErrorMessage (int errCode){
    if (errCode == -1){
        fprintf(stderr, "error: %s\n", strerror(errno));
    }

    if (errCode == -2){
        fprintf(stderr, "error: could not send message to node server\n");
    } 

    if (errCode == -3){
        fprintf(stderr, "error: node server took too long to respond\n");
    } 
    
    if (errCode == -4){
        fprintf(stderr, "error: recieved malformed expression from node server\n");
    } 

    if (errCode == -5){
        fprintf(stderr, "error: unxpected response from node server\n");
    } 

    if (errCode == -6){
        fprintf(stderr, "error: invalid address format read from node server\n");
    } 

    if (errCode == -7){
        fprintf(stderr, "error: exceeded number of tries when trying to connect to a node from net\n");
    }

    if (errCode == -8){
        fprintf(stderr, "error: failed to connect to specified node\n");
    }

    if (errCode == -10){
        fprintf(stderr, "error: failed to connect to recovery node\n");
    }
}
