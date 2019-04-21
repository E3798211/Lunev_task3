
#ifndef   NETWORKING_H_INCLUDED
#define   NETWORKING_H_INCLUDED

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define PORT 3000

// Client

int find_server(struct sockaddr_in* server_addr);

#endif // NETWORKING_H_INCLUDED

