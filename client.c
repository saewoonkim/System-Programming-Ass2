#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "client.h"



// program name
char *exename;

// buffer size
int bufsize = 2048;

// global Client instance
Client *global_client;




void Exit(Client *client);
void trim_string(char *str);



void next(Client *client);
void next_with_id(Client *client, int channel_id);
void sub(Client *client, int channel_id);
void unsub(Client *client, int channel_id);
void send_msg(Client *client, int channel_id, char *buf);
void channels(Client *cliet);
void livefeed_with_id(Client* client, int channel_id);
void livefeed_all(Client *client);
void stop(Client *client);
void bye(Client *client);


void usage() {
    fprintf(stderr,"Usage: %s [host] [port]\n", exename);
    exit(0);
}


void invalid_id(int id) {
    fprintf(stderr,"Invalid channel: <%d>\n", id);
}



void trim_string(char *str)
{
    char *start, *end;
    int len  = strlen(str);

    start = str;
    end = str + len -1;
    while(*start && isspace(*start))
        start++;
    while(*end && isspace(*end))
        *end-- = 0;
    char buf[2048];
    memset(buf, 0, sizeof(buf));
    strcpy(buf, start);
    strcpy(str, buf);
}


void next(Client *client) {
    Header header;
    header.command = NEXT;
    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"next send error");
        return;
    }
    char buf[bufsize];
    for(;;) {
        recv_packet(client->socket, &header, sizeof(header));
        if (header.command == NOTHING) {
            return;
        }
        memset(buf, 0, sizeof(buf));
        recv_packet(client->socket, buf, header.nbytes);
        printf("%s\n", buf);
    }
}


void next_with_id(Client *client, int channel_id) {
    if (channel_id < 0 || channel_id > 255) {
        invalid_id(channel_id);
        return;
    }

    char buf[bufsize];
    memset(buf, 0, bufsize);

    Header header;
    header.command = NEXTWITHID;
    header.channel_id = channel_id;

    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"next_with_id send error");
        return;
    }
    recv_packet(client->socket, &header, sizeof(header));
    if (header.command == NOTHING) {
        return;
    }
    recv_packet(client->socket, buf, header.nbytes);
    printf("%s\n", buf);
}


void sub(Client *client, int channel_id) {
    if (channel_id < 0 || channel_id > 255) {
        invalid_id(channel_id);
        return;
    }
    Header header;
    header.command = SUB;
    header.channel_id = channel_id;
    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"sub send error\n");
        return;
    }
    char buf[bufsize];
    memset(buf, 0, bufsize);
    recv_packet(client->socket, &header, sizeof(header));
    recv_packet(client->socket, buf, header.nbytes);
    printf("%s\n", buf);
}

void unsub(Client *client, int channel_id) {
    if (channel_id < 0 || channel_id > 255) {
        invalid_id(channel_id);
        return;
    }
    Header header;
    header.command = UNSUB;
    header.channel_id = channel_id;
    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"unsub send error\n");
        return;
    }
    char buf[bufsize];
    memset(buf, 0, bufsize);
    recv_packet(client->socket, &header, sizeof(header));
    recv_packet(client->socket, buf, header.nbytes);
    printf("%s\n", buf);
}


void send_msg(Client *client, int channel_id, char *buf) {
    if (channel_id < 0 || channel_id > 255) {
        invalid_id(channel_id);
        return;
    }
    Header header;
    header.command = SEND;
    header.channel_id = channel_id;
    header.nbytes = strlen(buf);
    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"send_msg send error -1\n");
    }
    status = send_packet(client->socket, buf, strlen(buf));
    if (status < 0) {
        fprintf(stderr,"send_msg send error -2\n");
    }
}

void channels(Client *cliet) {
    Header header;
    header.command = CHANNELS;
    header.nbytes = 0;
    int status = send_packet(cliet->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"channels send error\n");
    }
    recv_packet(cliet->socket, &header, sizeof(header));
    if (header.command == NOTHING) {
        return;
    }
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    recv_packet(cliet->socket, buf, header.nbytes);
    printf("%s", buf);
}

void livefeed_with_id(Client* client, int channel_id) {
    Header header;
    header.command = LIVEFEEDWITHID;
    header.channel_id = channel_id;
    send_packet(client->socket, &header, sizeof(header));
    recv_packet(client->socket, &header, sizeof(header));
    if (header.command == NAK) {
        char buf[bufsize];
        memset(buf, 0, sizeof(buf));
        recv_packet(client->socket, buf, header.nbytes);
        printf("%s\n", buf);
    }
}


void livefeed_all(Client *client) {
    Header header;
    header.command = LIVEFEED;
    send_packet(client->socket, &header, sizeof(header));
    recv_packet(client->socket, &header, sizeof(header));
    if (header.command == NAK) {
        char buf[bufsize];
        memset(buf, 0, bufsize);
        recv_packet(client->socket, buf, header.nbytes);
        printf("%s\n", buf);
    }
}


// thread function
void* live(void *arg) {
    Header header;
    char buf[bufsize];
    while(True) {
        int status = recv_packet(global_client->live_socket, &header, sizeof(header));
        if (status == 0) {
            fprintf(stderr,"connection closed by peer\n");
            Exit(global_client);
        } else if (status < 0) {
            continue;
        }
        memset(buf, 0, sizeof(buf));
        status = recv_packet(global_client->live_socket, buf, header.nbytes);

        if (status == 0) {
            fprintf(stderr,"connection closed by peer\n");
            Exit(global_client);
        }
        printf("%s\n", buf);
    }
}

void stop(Client *client) {
    Header  header;
    header.command = STOP;
    send_packet(client->socket, &header, sizeof(header));
}

void bye(Client *client) {
    Header header;
    header.command = BYE;
    int status = send_packet(client->socket, &header, sizeof(header));
    if (status < 0) {
        fprintf(stderr,"bye send error\n");
    }
    return;
}


int get_channel_id(Client *client) {
    int channel_id;
    sscanf(&client->inputbuf[strlen(client->commandstr)], "%d", &channel_id);
    return channel_id;
}


void Exit(Client *client) {
    close(client->socket);
    close(client->live_socket);
    exit(0);
}


void ctrl_c(int signo)
{
    Exit(global_client);
}



int main(int argc, char **argv) {
    exename = argv[0];
    if (argc < 3) {
        usage();
    }
    signal(SIGINT, ctrl_c);

    Client client;
    global_client = &client;

    client.ip= inet_addr(argv[1]);
    client.port = atoi(argv[2]);
    client.start = False;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client.port);
    server_addr.sin_addr.s_addr = client.ip;

    client.socket = socket(AF_INET, SOCK_STREAM, 0);
    client.live_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(client.socket, (struct sockaddr_in*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }
    if (connect(client.live_socket, (struct sockaddr_in*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    char buf[bufsize];
    memset(buf, 0, sizeof(buf));
    recv(client.socket, buf, bufsize, 0);
    printf("%s\n", buf);

    pthread_t  pid;
    pthread_create(&pid, NULL, live, NULL);
    client.start = True;

    while(client.start) {
        int channel_id = -1;
        // read from stdin

        fgets(client.inputbuf, sizeof(client.inputbuf), stdin);

        trim_string(client.inputbuf);
        sscanf(client.inputbuf, "%s", client.commandstr);

        if (!strcasecmp("NEXT", client.commandstr)) {
            if (strcmp(client.commandstr, client.inputbuf) == 0) {
                next(&client);
            } else {
                channel_id = get_channel_id(&client);
                next_with_id(&client, channel_id);
            }
        } else if (!strcasecmp("SUB", client.commandstr)) {
            channel_id = get_channel_id(&client);
            sub(&client, channel_id);
        } else if (!strcasecmp("SEND", client.commandstr)) {
            channel_id = get_channel_id(&client);
            int i = 0;
            while(!isdigit(client.inputbuf[i]) || isspace(client.inputbuf[i])) {
                i++;
            }
            while(isdigit(client.inputbuf[i])) {
                i++;
            }
            while(isspace(client.inputbuf[i])) {
                i++;
            }

            send_msg(&client, channel_id, &client.inputbuf[i]);

        } else if (!strcasecmp("UNSUB", client.commandstr)) {
            channel_id = get_channel_id(&client);
            unsub(&client, channel_id);
        } else if (!strcasecmp("CHANNELS", client.commandstr)) {
            channels(&client);
        } else if (!strcasecmp("BYE", client.commandstr)) {
            bye(&client);
            Exit(&client);
        } else if (!strcasecmp("LIVEFEED", client.commandstr)) {
            if(strcmp(client.commandstr, client.inputbuf) == 0) {
                livefeed_all(&client);
            } else {
                channel_id = get_channel_id(&client);
                livefeed_with_id(&client, channel_id);
            }
        }  else if (!strcasecmp("STOP", client.commandstr)) {
            stop(&client);
        } else {
            fprintf(stderr,"invalid command: %s\n", client.commandstr);
        }
    }
}




