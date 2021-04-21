#ifndef UTILS_H
#define UTILS_H

#include <sys/select.h>
#include "neighbours.h"

int setFds(fd_set *rfds, struct neighbours *neighbours);

#endif