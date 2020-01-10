#include "protocol.h"


/**
 *
 * @param socket
 * @param buffer
 * @param bufsize
 * @return < 0: error; == 0: connection closed; > 0: size of recevied
 */
int recv_packet (int socket, void* buffer, size_t bufsize) {
    if (bufsize == 0) {
        return 1;
    }
    char *bufptr = (char*)buffer;
    ssize_t ntorecv  = bufsize;

    do {
        ssize_t nbytes = recv(socket, bufptr, ntorecv, 0);
        // error
        if (nbytes < 0) {
            return nbytes;
        }
        // connection closed, network error
        if (nbytes == 0) {
            return 0;
        }
        bufptr += nbytes;
        ntorecv -= nbytes;
    } while (ntorecv >  0);
    return bufsize;
}



int send_packet(int socket, void* buffer, size_t bufsize) {
    if (bufsize == 0) {
        return 1;
    }
    char *bufptr = (char *)buffer;
    ssize_t ntosend = bufsize;
    do {
        ssize_t nbytes = send(socket, bufptr, ntosend, 0);
        if (nbytes < 0) {
            return nbytes;
        }
        // connection closed
        if (nbytes == 0) {
            return 0;
        }
        bufptr += nbytes;
        ntosend -= nbytes;
    } while(ntosend > 0);
    return bufsize;
}
