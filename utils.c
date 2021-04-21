#include <stdio.h>
#include <sys/select.h>
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