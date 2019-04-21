
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

#ifdef  DEBUG
    #define DBG
#else
    #define DBG if(0)
#endif


#define PORT 3000

// Client

#define N_NOTIFY_RETRIES 4

/* Opens fd */
int find_server(struct sockaddr_in* server_addr);
int notify_server(int server, struct sockaddr_in* server_addr);
int server_handshake(struct sockaddr_in* server_addr);
int establish_main_connection(struct sockaddr_in* server_addr);

#endif // NETWORKING_H_INCLUDED

