#ifndef RETURNCODES_H
#define RETURNCODES_H

//COMMAND CODES
#define CC_ERROR -1
#define CC_JOIN 1
#define CC_JOINBOOT 2
#define CC_CREATE 3
#define CC_GET 4
#define CC_SHOWTOPOLOGY 5
#define CC_SHOWROUTING 6
#define CC_SHOWCACHE 7
#define CC_LEAVE 8
#define CC_EXIT 9

//MESSAGE CODES
#define MC_ERROR -1
#define MC_NEW 1
#define MC_EXTERN 2
#define MC_ADVERTISE 3
#define MC_WITHDRAW 4
#define MC_INTEREST 5
#define MC_DATA 6
#define MC_NODATA 7

#endif