
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "multithreading.h"

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
/* 'sock' expected to be TCP and already connected 
    Functions return amount of bytes read          */
ssize_t send_msg(int sock, void* msg, size_t msg_size, int flags);
ssize_t recv_msg(int sock, void* buf, size_t buf_size, int flags);
int set_tcp_keepalive(int sock, int idle, int cnt, int intvl);


// Client

#define N_NOTIFY_RETRIES 4

int start_keepalive_check(int* server);
/* Opens fd */
int find_server    (struct sockaddr_in* server_addr);
int notify_server  (int server, struct sockaddr_in* server_addr);
int server_handshake(struct sockaddr_in* server_addr);
int establish_main_connection(struct sockaddr_in* server_addr);
int send_info      (int server, size_t n_threads);
int receive_bound  (int server, double* bound);

// Server

struct client_info
{
    int sock;
    int n_threads;
    uint32_t addr;
    int finished;
    double left_bound;
    double right_bound;
    double result;
#ifdef    DEBUG
    char ip[16];
#endif // DEBUG
};

#define N_CLIENTS_MAX 16
#define N_WAITING_RETRIES   4
#define N_BROADCAST_RETRIES 4
#define CLIENTS_HANDSHAKE_TIMEOUT { .tv_sec = 0, .tv_usec = 200000 }
#define CLIENTS_INFO_TIMEOUT      { .tv_sec = 0, .tv_usec = 200000 }
#define CLIENTS_PING_TIMEOUT      { .tv_sec = 5, .tv_usec =      0 }

int init_server();
/* Expects 'clients' to be zeroed */
int find_clients   (struct client_info clients[N_CLIENTS_MAX]);
int wait_for_clients_start(int sock, 
                           struct client_info clients[N_CLIENTS_MAX]);
/* Returns amount of clients registered */
int register_client(struct sockaddr_in* client_addr,
                    struct client_info clients[N_CLIENTS_MAX], int n_clients);
int get_client_info(int sock, struct client_info clients[N_CLIENTS_MAX],
                    int n_clients);
void distribute_load(struct client_info clients[N_CLIENTS_MAX], int n_clients,
                     double left_bound, double right_bound);
int start_clients  (struct client_info clients[N_CLIENTS_MAX], int n_clients);
int set_client_connections(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients);
int wait_for_clients_finish(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients);
double collect_info(struct client_info clients[N_CLIENTS_MAX], int n_clients);
void close_connections(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients);

#endif // NETWORKING_H_INCLUDED

