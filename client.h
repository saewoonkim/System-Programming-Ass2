#ifndef _CLIENT_H
#define _CLIENT_H

#include "protocol.h"



typedef struct {
    int port;
    unsigned int ip;
    bool start;
    int socket;
    int live_socket;
    char commandstr[100];
    char inputbuf[2048];
    enum Command command;
} Client;


#endif //_CLIENT_H
