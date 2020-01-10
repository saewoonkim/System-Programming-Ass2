
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>


typedef int bool;

#define True 1
#define False 0



enum Command {
    SUB,
    UNSUB,
    CHANNELS,
    NEXT,
    NEXTWITHID,
    LIVEFEED,
    LIVEFEEDWITHID,
    SEND,
    STOP,
    BYE,
    ACK,
    NAK,
    NOTHING,
};

typedef struct
{
    enum Command command;
    int channel_id;
    int nbytes;
} Header;


int recv_packet (int socket, void* buffer, size_t bufsize);
int send_packet(int socket, void* buffer, size_t bufsize);

#endif //_PROTOCOL_H
