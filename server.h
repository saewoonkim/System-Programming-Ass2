#ifndef _SERVER_H
#define _SERVER_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include "protocol.h"


typedef struct message
{
    char msg[1025];                    // maximum message length of 1024 bytes
    int msg_id;                        // unique message id
    struct message *prev;
    struct message *next;
} Message;


typedef struct client
{
    int client_id;                      // client id
    int client_socket;                  // client socket, use to receive and send command
    int live_socket;                    // live socket, used to send LIVEFEED message to client
    int model[256];                     // 0 is LIVEFEED, 1 is normal. 0 means needs to push messages in real time
    int channels[256];                  // channels[n] == 1, means the client subscribe channel n
    int livefeed_id[256];               // livefeed_id[n] == 1, means the client use follow command: LIVEFEED n
    int read_msg_num[256];              // read_msg_num[n] is number of readed message
    Message *msg_ptr[256];              // msgPtr[n] is already read message
    bool start;                         // indicates whether the process corresponding to this client starts looping
    int subnum;                         // the number of subscribe channel
    struct client *prev;                // previous pointer
    struct client *next;                // next pointer
    bool liveall;                       // LIVEFEED for all channels, means client use follow command: LIVEFEED
    char ip[INET_ADDRSTRLEN];           // client ip
    int port;                           // client port
} Client;


typedef struct channel
{
    int id;                     // 0-255
    int msg_num;                // total message number
    int sem_id;                 // the semaphore to ensure that only one process updates the message list at a time.
    Message *msg_head;          // head of message list
    Message *msg_tail;          // tail of message list
} Channel;


typedef struct server
{
    Channel channels[256];          // up to 256 channels
    Client *client_head;            // head of client list
    Client *client_tail;            // tail of client list
    int listen_socket;              // server listen socket
    struct sockaddr_in addr;        // server address
    bool start;                     // is start
    int sem_id;                     // message id lock
} Server;


Server* create(int port);
int start(Server *server);



#endif // _SERVER_H
