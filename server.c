#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/sem.h>

#include "memmanage.h"
#include "server.h"
#include "protocol.h"


union mysemun{
    int val;
    struct semid_ds *buf;
    unsigned short *arry;
};




int bufsize = 2048;

// global Server instance
Server *global_server;

// is exit
int *isExit;

// to generate global unique id for message, this is increment
int *global_unique_msg_id = NULL;

// exit cleanly
void exitCleanly(int status);

// initialize channel
void initChannel(Channel *channel);

// set SO_REUSEADDR option for sock
void setSockOpt(int sock);

// initialize client
void initClient(Client *client);

// insert client into global_server's client list
void addClient(Client *client);

// remove client from global_server's client list
void removeClient(Client *client);

// generate an unique id for every client
int getClientUniqueId();

// a wrapper function for send_pack()
int svr_send_pack(Client *client, void *buf, size_t bufsize);

// generate an unique id for every message
int get_unique_msg_id();

int set_semvalue(int sem_id);
void del_semvalue(int sem_id);
int semaphore_p(int sem_id);
int semaphore_v(int sem_id);

// each of the following functions corresponds to a command
void reply_sub(Client *client, Header header);
void reply_next(Client *client, Header header);
void reply_send(Client *client, Header header);
void reply_unsub(Client *client, Header header);
void reply_channels(Client *client, Header header);
void reply_livefeed_with_id(Client *client, Header header);
void reply_next_with_id(Client *client, Header header);
void reply_livefeed_all(Client *client, Header header);
void reply_stop(Client *client);
void reply_bye(Client *client);
void reply_livefeed_with_id_end(Client *client, Header header);


int get_unique_msg_id() {
    int temp = *global_unique_msg_id;
    (*global_unique_msg_id)++;
    return temp;
}

void exitCleanly(int status) {
    Client *client = global_server->client_head->next;
    // loop for close socket for every client
    while(client != NULL) {
        client->start = False;
        close(client->live_socket);
        close(client->client_socket);
        client = client->next;
    }


    for(int i = 0; i < 256; i++) {
        del_semvalue(global_server->channels[i].sem_id);
    }

    // free memmanager
    sharedMemFree();
    // exit with status
    del_semvalue(global_server->sem_id);
    exit(status);
}

void initChannel(Channel *channel) {
    channel->msg_num = 0;
    channel->msg_head = (Message*)getSharedMemBlock(sizeof(Message));
    channel->msg_head->prev = NULL;
    channel->msg_head->next = NULL;
    channel->msg_tail = channel->msg_head;
    channel->sem_id = semget((key_t)channel->id, 1, 0666 | IPC_CREAT);
    set_semvalue(channel->sem_id);
}

void setSockOpt(int sock) {
    int on = 1;
    int status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (status < 0) {
        fprintf(stderr,"setsockopt error: %s(errno: %d)\n", strerror(errno), errno);
        exitCleanly(-1);
    }
}


void initClient(Client *client) {
    client->next = NULL;
    client->prev = NULL;
    client->start = False;
    client->liveall = False;
    client->subnum = 0;
    client->client_id = getClientUniqueId();
    for (int i = 0; i < 256; i++) {
        client->model[i] = 1;
        client->channels[i] = 0;
        client->msg_ptr[i] = NULL;
        client->read_msg_num[i] = 0;
        client->livefeed_id[i] = 0;
    }
}

void removeClient(Client *client) {
    if (client->prev != NULL) {
        client->prev->next = client->next;
    }
    if (client->next == NULL) {
        global_server->client_tail = client->prev;
    } else {
        client->next->prev = client->prev;
    }
}

void addClient(Client *client) {
    client->prev = global_server->client_tail;
    client->next = NULL;
    global_server->client_tail->next = client;
    global_server->client_tail = client;
}

int getClientUniqueId() {
    static int ID = 0;
    int curId = ID;
    ID++;
    return curId;
}


int svr_send_pack(Client *client, void *buf, size_t bufsize) {
    if (!client->start) {
        return 0;
    }
    int status = send_packet(client->client_socket, buf, bufsize);
    if (status < 0) {
        fprintf(stderr,"send error: %s(errno: %d)\n", strerror(errno), errno);
    } else if (status == 0) {
        // closed by peer
        client->start = False;
    }
    return status;
}

int set_semvalue(int sem_id) {
    union mysemun sem_union;
    sem_union.val = 1;
    if(semctl(sem_id, 0, SETVAL, sem_union) == -1)
        return 0;
    return 1;
}

void del_semvalue(int sem_id) {
    union mysemun sem_union;
    semctl(sem_id, 0, IPC_RMID, sem_union);
}

int semaphore_p(int sem_id) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    //P()
    sem_b.sem_op = -1;
    sem_b.sem_flg = SEM_UNDO;
    if(semop(sem_id, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_p failed\n");
        return 0;
    }
    return 1;
}
int semaphore_v(int sem_id) {
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    //V()
    sem_b.sem_op = 1;
    sem_b.sem_flg = SEM_UNDO;
    if(semop(sem_id, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_v failed\n");
        return 0;
    }
    return 1;
}



Server* create(int port) {
    Server *server = (Server*)getSharedMemBlock(sizeof(Server));
    global_unique_msg_id = (int*)getSharedMemBlock(sizeof(int));
    *global_unique_msg_id = 1;

    for (int i = 0; i < 256; i++) {
        server->channels[i].id = i;
        initChannel(&server->channels[i]);
    }
    server->client_head = (Client*)getSharedMemBlock(sizeof(Client));
    server->client_head->prev = NULL;
    server->client_head->next = NULL;
    server->client_tail = server->client_head;
    server->sem_id = semget((key_t)6666, 1, 0666 | IPC_CREAT);
    set_semvalue(server->sem_id);

    if ((server->listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr,"create socket error: %s(errno: %d)\n", strerror(errno),errno);
        exitCleanly(1);
    }
    // set SO_REUSEADDR
    setSockOpt(server->listen_socket);

    server->addr.sin_family = AF_INET;
    server->addr.sin_port = htons(port);
    server->addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server->listen_socket, (struct sockaddr*)&server->addr, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "bind socket error: %s(errno: %d)\n", strerror(errno),errno);
        exitCleanly(1);
    }

    if(listen(server->listen_socket, 20) == -1){
        fprintf(stderr, "listen socket error: %s(errno: %d)\n", strerror(errno),errno);
        exitCleanly(1);
    }
    server->start = False;
    return server;
}

void reply_sub(Client *client, Header header) {
    // already subscribed
    char buf[bufsize];
    memset(buf, 0, bufsize);
    if (client->channels[header.channel_id] == 1) {
        sprintf(buf, "Already subscribed to channel %d.", header.channel_id);
        header.command = NAK;
        header.nbytes = strlen(buf);
    } else {
        client->channels[header.channel_id] = 1;
        client->read_msg_num[header.channel_id] = 0;
        client->msg_ptr[header.channel_id] = global_server->channels[header.channel_id].msg_tail;
        sprintf(buf, "Subscribed to channel %d.", header.channel_id);
        header.command = ACK;
        header.nbytes = strlen(buf);
        client->subnum++;
    }

    svr_send_pack(client, &header, sizeof(header));
    svr_send_pack(client, buf, strlen(buf));
}

void reply_unsub(Client *client, Header header) {
    // already subscribed
    char buf[bufsize];
    memset(buf, 0, bufsize);
    if (client->channels[header.channel_id] == 0) {
        sprintf(buf, "Not subscribed to channel %d.", header.channel_id);
        header.command = NAK;
    } else {
        client->channels[header.channel_id] = 0;
        client->model[header.channel_id] = 1;
        client->read_msg_num[header.channel_id] = 0;
        client->msg_ptr[header.channel_id] = global_server->channels[header.channel_id].msg_tail;
        sprintf(buf, "Unsubscribed from channel %d.", header.channel_id);
        header.command = ACK;
        client->subnum--;
    }
    header.nbytes = strlen(buf);
    svr_send_pack(client, &header, sizeof(header));
    svr_send_pack(client, buf, strlen(buf));
}

void reply_send(Client *client, Header header) {
    char buf[bufsize];
    memset(buf, 0, sizeof(buf));
    recv_packet(client->client_socket, buf, header.nbytes);
    int channel_id = header.channel_id;
    if (channel_id < 0 || channel_id > 255) {
        return;
    }
    Message *new_msg = (Message*)getSharedMemBlock(sizeof(Message));
    new_msg->msg_id = get_unique_msg_id();
    new_msg->next = NULL;
    strcpy(new_msg->msg, buf);


    //  get channel lock
    while (!semaphore_p(global_server->channels[channel_id].sem_id)) {
        fprintf(stderr, "get lock error, channel id=%d\n", channel_id);
    }

    global_server->channels[channel_id].msg_tail->next = new_msg;
    new_msg->prev = global_server->channels[channel_id].msg_tail;
    global_server->channels[channel_id].msg_tail = new_msg;
    global_server->channels[channel_id].msg_num++;
    while (!semaphore_v(global_server->channels[channel_id].sem_id)) {
        fprintf(stderr, "release lock error, channel id=%d\n", channel_id);
    }
}

int cmp ( const void *a , const void *b)
{
    return *(int *)a - *(int *)b;
}

void reply_next(Client *client, Header header) {
    char buf[bufsize];
    memset(buf, 0, sizeof(buf));

    // max 256 unread message
    int messageid[256];
    int num = 0;

    if (client->subnum == 0) {
        header.command = ACK;
        sprintf(buf, "%s", "Not subscribed to any channels.");
        header.nbytes = strlen(buf);
        svr_send_pack(client, &header, sizeof(header));
        svr_send_pack(client, buf, strlen(buf));
    }

    for(int i = 0; i < 256; i++) {
        // if exist unread message
        if (client->channels[i] == 1 && client->msg_ptr[i] != NULL && client->msg_ptr[i]->next != NULL) {
            messageid[num] = client->msg_ptr[i]->next->msg_id;
            num++;
        }
    }

    if (num == 0) {
        header.command = NOTHING;
        svr_send_pack(client, &header, sizeof(header));
        return;
    }
    qsort(messageid, num, sizeof(messageid[0]), cmp);
    int n = 0;
    while(num > 0) {
        for (int i = 0; i < 256; i++) {
            if (client->channels[i] == 1 && client->msg_ptr[i] != NULL && client->msg_ptr[i]->next != NULL &&
                    client->msg_ptr[i]->next->msg_id == messageid[n]) {
                memset(buf, 0, sizeof(buf));
                sprintf(buf, "%d:%s", i, client->msg_ptr[i]->next->msg);
                client->msg_ptr[i] = client->msg_ptr[i]->next;
                client->read_msg_num[i]++;
                header.command = ACK;
                header.nbytes = strlen(buf);
                svr_send_pack(client, &header, sizeof(header));
                svr_send_pack(client, buf, strlen(buf));
                break;
            }
        }
        n++;
        num--;
    }
    header.command = NOTHING;
    svr_send_pack(client, &header, sizeof(header));
}

void replay_next_with_id(Client *client, Header header) {
    char buf[bufsize];
    memset(buf, 0, bufsize);
    if (client->channels[header.channel_id] == 0) {
        header.command = NAK;
        sprintf(buf, "Not subscribed to channel %d.", header.channel_id);
        header.nbytes = strlen(buf);
    } else {
        Message *temp = NULL;
        if (client->msg_ptr[header.channel_id] != NULL) {
            temp = client->msg_ptr[header.channel_id]->next;
        }
        if (NULL != temp) {
            header.command = ACK;
            sprintf(buf, "%s", temp->msg);
            header.nbytes = strlen(buf);
            client->msg_ptr[header.channel_id] = temp;
            client->read_msg_num[header.channel_id]++;
        } else {
            // no new message to send
            header.command = NOTHING;
            header.nbytes = 0;
        }
    }
    svr_send_pack(client, &header, sizeof(header));
    if (header.command != NOTHING) {
        svr_send_pack(client, buf, strlen(buf));
    }
}

void reply_channels(Client *client, Header header) {
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < 256; i++) {
        int first_value = 0;
        int second_value = 0;
        int third_value = 0;
        if (client->channels[i] == 1) {
            first_value = global_server->channels[i].msg_num;
            second_value = client->read_msg_num[i];
            Message *temp = client->msg_ptr[i]->next;
            while (temp != NULL) {
                third_value++;
                temp = temp->next;
            }
            if (strlen(buf) != 0) {
                sprintf(buf, "%s%d\t%d\t%d\n", buf, first_value, second_value, third_value);
            } else {
                sprintf(buf, "%d\t%d\t%d\n", first_value, second_value, third_value);
            }
        }
    }
    if (strlen(buf) == 0) {
        header.command = NOTHING;
        header.nbytes = 0;
    } else {
        header.command = ACK;
        header.nbytes = strlen(buf);
    }
    svr_send_pack(client, &header, sizeof(header));
    if (header.command != NOTHING) {
        svr_send_pack(client, buf, strlen(buf));
    }
}

void reply_livefeed_with_id(Client *client, Header header) {
    char buf[bufsize];
    memset(buf, 0, bufsize);
    if (client->channels[header.channel_id] == 0) {
        header.command = NAK;
        sprintf(buf, "Not subscribed to channel %d.", header.channel_id);
        header.nbytes = strlen(buf);
        send_packet(client->client_socket, &header, sizeof(header));
        send_packet(client->client_socket, buf, strlen(buf));
        return;
    }
    header.command = ACK;
    send_packet(client->client_socket, &header, sizeof(header));

    client->model[header.channel_id] = 0;
    client->livefeed_id[header.channel_id] = 1;
}


void reply_livefeed_all(Client *client, Header header) {
    char buf[bufsize];
    memset(buf, 0, bufsize);

    if (client->subnum == 0)  {
        header.command = NAK;
        sprintf(buf, "%s", "Not subscribed to any channels.");
        header.nbytes = strlen(buf);
        send_packet(client->client_socket, &header, sizeof(header));
        send_packet(client->client_socket, buf, strlen(buf));
        return;
    }
    client->liveall = True;

    for (int i = 0; i < 256; i++) {
        if (client->channels[i] == 1) {
            client->model[i] = 0;
        }
    }
    header.command = ACK;
    send_packet(client->client_socket, &header, sizeof(header));
}

// thread function for send livefeed message
void *livefeed_id(void *arg) {
    Header header;
    char buf[bufsize];
    while (global_server->start) {
        Client *client = global_server->client_head->next;
        while(client != NULL) {
            for (int i = 0; i< 256; i++) {
                    Message *temp = NULL;
                    if (client->channels[i] == 1 && client->model[i] == 0 && client->msg_ptr[i] != NULL) {
                        temp = client->msg_ptr[i]->next;
                        Message *pre = client->msg_ptr[i];
                        while (temp != NULL) {
                            memset(buf, 0, sizeof(buf));
                            header.command = ACK;
                            if (client->livefeed_id[i] == 1) {
                                sprintf(buf, "%s", temp->msg);
                            } else {
                                sprintf(buf, "%d:%s", i,temp->msg);
                            }
                            header.nbytes = strlen(buf);
                            send_packet(client->live_socket, &header, sizeof(header));
                            send_packet(client->live_socket, buf, strlen(buf));
                            client->read_msg_num[header.channel_id]++;
                            pre = temp;
                            temp = temp->next;
                        }
                        client->msg_ptr[i] = pre;
                    }
                }
            client = client->next;
        }
        sleep(1);
    }
}

void reply_stop(Client *client) {
    client->liveall = False;
    for (int i = 0; i < 256; i++) {
        client->model[i] = 1;
        client->livefeed_id[i] = 0;
    }
}

void reply_bye(Client *client) {
    close(client->client_socket);
    close(client->live_socket);
    client->start = False;
}




void run_server(Client* client) {
    client->start = True;
    char buf[100];
    sprintf(buf, "Welcome! Your client ID is %d.", client->client_id);
    send_packet(client->client_socket, buf, strlen(buf));
    while(client->start) {
        Header header;
        int status = recv_packet(client->client_socket, &header, sizeof(header));
        if (status < 0) {
            fprintf(stderr, "recv error, clientId=%d\n", client->client_id);
            continue;
        }
        if (status == 0) {
            fprintf(stderr, "connection closed by peer, pid: %d, client addr: %s:%d\n", getpid(), client->ip, client->port);
            close(client->live_socket);
            close(client->client_socket);
            client->start = False;
            continue;
        }

        switch (header.command) {
            case SUB:
                reply_sub(client, header);
                break;
            case CHANNELS:
                reply_channels(client, header);
                break;
            case UNSUB:
                reply_unsub(client, header);
                break;
            case NEXTWITHID:
                replay_next_with_id(client, header);
                break;
            case NEXT:
                reply_next(client, header);
                break;
            case LIVEFEEDWITHID:
                reply_livefeed_with_id(client, header);
                break;
            case LIVEFEED:
                reply_livefeed_all(client, header);
                break;
            case SEND:
                reply_send(client, header);
                break;
            case STOP:
                reply_stop(client);
                break;
            case BYE:
                reply_bye(client);
                break;
            default:
                printf("error command: %d\n", header.command);
        }
    }
    removeClient(client);
    exit(0);
}

int start(Server *server) {
    server->start = True;
    pthread_t pthreadid;
    pthread_create(&pthreadid, NULL, livefeed_id, NULL);

    while (server->start) {
        struct sockaddr_in sock_addr;
        int addr_length = sizeof(sock_addr);
        int socket_fd = accept (server->listen_socket, (struct sockaddr*)&sock_addr, (socklen_t*)&addr_length);
        int live_fd = accept(server->listen_socket, (struct sockadd*)&sock_addr, (socklen_t*)&addr_length);
        if (socket_fd < 0 || live_fd < 0) {
            fprintf(stderr, "accept error: %s(errno: %d)\n", strerror(errno), errno);
            continue;
        }

        Client *new_client = (Client*)getSharedMemBlock(sizeof(Client));
        new_client->client_socket = socket_fd;
        new_client->live_socket = live_fd;
        inet_ntop(AF_INET,&sock_addr.sin_addr, new_client->ip, sizeof(new_client->ip));
        new_client->port = ntohs(sock_addr.sin_port);

        initClient(new_client);
        addClient(new_client);
        pid_t pid = fork();
        if (pid == 0) {     // child
            run_server(new_client);
        } else {
            if (pid < 0) {
                printf("fork failed: %s(errno: %d)\n", strerror(errno), errno);
            } else {
                printf("forked server pid: %d | client address: %s:%d\n", pid, new_client->ip, new_client->port);
            }
        }
    }
    return 0;
}



void ctrl_c(int signo)
{
    if (*isExit == 0) {
        exitCleanly(0);
        *isExit = 1;
    }
}


char *exename;

void Usage() {
    fprintf(stderr,"Usage: %s [port]\n", exename);
    exit(0);
}




int main(int argc, char **argv) {
    exename = argv[0];
    int port = 8888;
    if (argc < 2) {
       Usage();
    }
    port = atoi(argv[1]);

    // function for ctrl-c signal
    signal(SIGINT, ctrl_c);
    // init memmanager
    sharedMemInit();
    isExit = (int*)getSharedMemBlock(sizeof(int));
    *isExit = 0;
    global_server = create(port);
    start(global_server);
}
