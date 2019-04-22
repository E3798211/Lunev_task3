
#ifndef   NETWORKING_H_INCLUDED
#define   NETWORKING_H_INCLUDED

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>

#ifdef  DEBUG
    #define DBG
#else
    #define DBG if(0)
#endif


// Common

#define HANDSHAKE_PORT  3000
#define MAIN_PORT       3001

/* Expects UDP socket */
int send_address(int sock, struct sockaddr_in* sddr, int n_times);


// Client

#define N_NOTIFY_RETRIES 4

/* Opens fd */
int find_server(struct sockaddr_in* server_addr);
int notify_server(int server, struct sockaddr_in* server_addr);
int server_handshake(struct sockaddr_in* server_addr);
int establish_main_connection(struct sockaddr_in* server_addr);
int send_info(int server, size_t n_threads);
int receive_bound(int server, double* bound);

// Server

struct client_info
{
    int sock;
    int n_threads;
    uint32_t addr;
#ifdef    DEBUG
    char ip[16];
#endif // DEBUG
};

#define N_CLIENTS_MAX 16
#define N_WAITING_RETRIES   4
#define N_BROADCAST_RETRIES 4
#define CLIENTS_HANDSHAKE_TIMEOUT { .tv_sec = 0, .tv_usec = 200000 }
#define CLIENTS_INFO_TIMEOUT      { .tv_sec = 0, .tv_usec = 200000 }

int init_server();
/* Expects 'clients' to be zeroed */
int find_clients(struct client_info clients[N_CLIENTS_MAX]);
int wait_for_clients_start(int sock, 
                           struct client_info clients[N_CLIENTS_MAX]);
/* Returns amount of clients registered */
int register_client(struct sockaddr_in* client_addr,
                    struct client_info clients[N_CLIENTS_MAX], int n_clients);
int get_client_info(int sock, struct client_info clients[N_CLIENTS_MAX],
                    int n_clients);

#endif // NETWORKING_H_INCLUDED

