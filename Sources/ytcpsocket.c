//
//  Copyright (c) <2014>, skysent
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//  1. Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//  3. All advertising materials mentioning features or use of this software
//  must display the following acknowledgement:
//  This product includes software developed by skysent.
//  4. Neither the name of the skysent nor the
//  names of its contributors may be used to endorse or promote products
//  derived from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY skysent ''AS IS'' AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL skysent BE LIABLE FOR ANY
//  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>

void ytcpsocket_set_block(int socket, int on) {
    int flags;
    flags = fcntl(socket, F_GETFL, 0);
    if( on == 0 ) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    } else {
        flags &= ~O_NONBLOCK;
        fcntl(socket, F_SETFL, flags);
    }
}

int ytcpsocket_connect(const char *host, int port, int timeout) {
    struct sockaddr_in sa;
    struct hostent *hp;
    int sockfd = -1;
    hp = gethostbyname(host);
    if( hp == NULL ) {
        return -1;
    }

    bcopy((char *) hp->h_addr, (char *) &sa.sin_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(port);
    sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0);
    ytcpsocket_set_block(sockfd, 0);
    connect(sockfd, (struct sockaddr *) &sa, sizeof(sa));
    fd_set fdwrite;
    struct timeval tvSelect;
    FD_ZERO(&fdwrite);
    FD_SET(sockfd, &fdwrite);
    tvSelect.tv_sec = timeout;
    tvSelect.tv_usec = 0;

    int retval = select(sockfd + 1, NULL, &fdwrite, NULL, &tvSelect);
    if( retval < 0 ) {
        close(sockfd);
        return -2;
    } else if( retval == 0 ) {//timeout
        close(sockfd);
        return -3;
    } else {
        int error = 0;
        int errlen = sizeof(error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *) &errlen);
        if( error != 0 ) {
            close(sockfd);
            return -4;//connect fail
        }

        int set = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof(int));
        return sockfd;
    }
}

int ytcpsocket_close(int socketfd) {
    return close(socketfd);
}

int ytcpsocket_pull(int socketfd, char *data, int len, int timeout_sec) {
    int readlen = 0;
    int datalen = 0;
    if( timeout_sec > 0 ) {
        fd_set fdset;
        struct timeval timeout;
        timeout.tv_usec = 0;
        timeout.tv_sec = timeout_sec;
        FD_ZERO(&fdset);
        FD_SET(socketfd, &fdset);
        int ret = select(socketfd + 1, &fdset, NULL, NULL, &timeout);
        if( ret <= 0 ) {
            return ret; // select-call failed or timeout occurred (before anything was sent)
        }
    }
    // use loop to make sure receive all data
    do {
        readlen = (int) read(socketfd, data + datalen, len - datalen);
        if( readlen > 0 ) {
            datalen += readlen;
        }
    } while (readlen > 0);

    return datalen;
}

int ytcpsocket_send(int socketfd, const char *data, int len) {
    int byteswrite = 0;
    while (len - byteswrite > 0) {
        int writelen = (int) write(socketfd, data + byteswrite, len - byteswrite);
        if( writelen < 0 ) {
            return -1;
        }
        byteswrite += writelen;
    }
    return byteswrite;
}

int ytcpsocket_bind(const char *address, int port) {
    struct addrinfo hints, *res;
    int bound_socket;

    // first, load up address structs with getaddrinfo():
    int bind_all = (strcmp(address, "0.0.0.0") == 0) || (strcmp(address, "") == 0);

    char port_str[16];

    snprintf(port_str, 16, "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;

    if( bind_all ) {
        printf("binding to any address on %s\n", port_str);
        hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
        getaddrinfo(NULL, port_str, &hints, &res);
    } else {
        printf("binding to address: %s:%s\n", address, port_str);
        getaddrinfo(address, port_str, &hints, &res);
    }

    // make a socket:
    int reuse_socket = 1;

    bound_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    setsockopt(bound_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(reuse_socket));

    // bind it to the port we passed in to getaddrinfo():

    int result = bind(bound_socket, res->ai_addr, res->ai_addrlen);

    if( result == 0 ) {
        printf("bound to socket: %d\n", bound_socket);
        return bound_socket;
    } else {
        return -1;
    }
}

int ytcpsocket_has_data_pending(int socket_fd, int duration) {
    fd_set master;    // master file descriptor list

    FD_ZERO(&master);    // clear the master and temp sets
    FD_SET(socket_fd, &master);

    printf("checking socket: %d\n", socket_fd);

    int64_t real_duration = duration * 1000;

    long seconds = (long)(real_duration / 1000000);
    int usecs = (int)(real_duration % 1000000);

    struct timeval wait_for;
    wait_for.tv_sec = seconds;
    wait_for.tv_usec = usecs;

    struct timeval *wait_for_it = NULL;
    if( duration > 0 ) {
        wait_for_it = &wait_for;
    }

    if( select(socket_fd + 1, &master, NULL, NULL, wait_for_it) == -1 ) {
        return -1;
    }

    if( FD_ISSET(socket_fd, &master) ) {
        return 1;
    }

    return 0;
}

//return socket fd
int ytcpsocket_listen(int listener) {
    if( listen(listener, 128) == 0 ) {
        return listener;
    } else {
        return -2;//listen error
    }
}

//return client socket fd
int ytcpsocket_accept(int listen_socket, char *remoteip, int *remoteport, int timeouts) {
    socklen_t clilen;
    struct sockaddr_in cli_addr;
    clilen = sizeof(cli_addr);

    printf("waiting to accept\n");
    int status = ytcpsocket_has_data_pending(listen_socket, timeouts);
    if( status > 0 ) {
        int incoming_socket = accept(listen_socket, (struct sockaddr *) &cli_addr, &clilen);
        char *incoming_client_ip = inet_ntoa(cli_addr.sin_addr);

        memcpy(remoteip, incoming_client_ip, strlen(incoming_client_ip));
        *remoteport = cli_addr.sin_port;

        if( incoming_socket > 0 ) {
            int set = 1;
            setsockopt(incoming_socket, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof(int));
            return incoming_socket;
        }
    }

    return -1;
}

//return socket port
int ytcpsocket_port(int socketfd) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if( getsockname(socketfd, (struct sockaddr *) &sin, &len) == -1 ) {
        return -1;
    } else {
        return ntohs(sin.sin_port);
    }
}
