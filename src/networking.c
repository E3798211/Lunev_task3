
#include "networking.h"

// Common

int send_address(int sock, struct sockaddr_in* addr, int n_times)
{
    assert(addr);
    if (n_times < 0)    return EXIT_FAILURE;

    char msg = '1';
    for(int i = 0; i < n_times; i++)
    {
        errno = 0;
        int res = sendto(sock, &msg, sizeof(msg), 0,
                         (struct sockaddr*)addr, sizeof(*addr));
        if (res < 0)
        {
            perror("sendto()");
            return EXIT_FAILURE;
        }
    }

    DBG printf("Sent byte to %s %d times\n", inet_ntoa(addr->sin_addr), 
                                             n_times);

    return EXIT_SUCCESS;
}

ssize_t send_msg(int sock, void* msg, size_t msg_size, int flags)
{
    ssize_t bytes_sent_all = 0;
    ssize_t bytes_sent;
    do
    {
        errno = 0;
        bytes_sent = send(sock, (char*)msg + bytes_sent_all,
                                  msg_size - bytes_sent_all, flags);
        if (bytes_sent < 0)
        {
            perror("send()");
            return -1;
        }
        bytes_sent_all += bytes_sent;
    }while(bytes_sent && bytes_sent_all < msg_size);

    return bytes_sent_all;
}

ssize_t recv_msg(int sock, void* buf, size_t msg_size, int flags)
{
    ssize_t bytes_read_all = 0;
    ssize_t bytes_read;
    do
    {
        errno = 0;
        bytes_read = recv(sock, (char*)buf + bytes_read_all, 
                                  msg_size - bytes_read_all, flags);
        if (bytes_read < 0)
        {
            perror("recv()");
            return -1;
        }
        bytes_read_all += bytes_read;
    }while(bytes_read && bytes_read_all < msg_size);
    
    return bytes_read_all;
}

int set_tcp_keepalive(int sock, int idle, int cnt, int intvl)
{
    int res    = 0;
    int option = 1;
    errno = 0;
    res = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
                     &option, sizeof(option));
    if (res)
    {
        perror("setsockopt()");
        printf("SO_KEEPALIVE failed\n");
        return -1;
    }
    
    option = idle;
    errno = 0;
    res = setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, 
                     &option, sizeof(option));
    if (res)
    {
        perror("setsockopt()");
        printf("TCP_KEEPIDLE failed\n");
        return -1;
    }

    option = cnt;
    errno = 0;
    res = setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, 
                     &option, sizeof(option));
    if (res)
    {
        perror("setsockopt()");
        printf("TCP_KEEPCNT failed\n");
        return -1;
    }

    option = intvl;
    errno = 0;
    res = setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, 
                     &option, sizeof(option));
    if (res)
    {
        perror("setsockopt()");
        printf("TCP_KEEPINTVL failed\n");
        return -1;
    }

    return sock;
}

// Client

int create_keepalive_sock()
{
    errno = 0;
    int keep_alive_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (keep_alive_sock < 0)
    {
        perror("socket()");
        return EXIT_FAILURE;
    }

    int res = set_tcp_keepalive(keep_alive_sock, TCP_IDLE, TCP_CNT, TCP_INTVL);
    if (res < 0)
    {
        printf("set_tcp_keepalive() failed\n");
        return -1;
    }

    int n_syn_retries = TCP_SYN_RETRY;
    errno = 0;
    res = setsockopt(keep_alive_sock, IPPROTO_TCP, TCP_SYNCNT,
                                      &n_syn_retries, sizeof(n_syn_retries));
    if (res)
    {
        perror("setsockopt()");
        printf("TCP_SYNCNT failed\n");
        return -1;
    }

    return keep_alive_sock;
}

/* Function to check connection in the background 
   Never returns. Return == error                 */
static void* check_connection(void* arg)
{
    struct server_pack* con = (struct server_pack*)arg;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(KEEPALIVE_PORT);
    server_addr.sin_addr.s_addr = con->serv_s_addr;

    int server = con->sock;

    // TCP_SYNCNT set to only 3 -> thread won't wait for a long time
    int res = connect(server, (struct sockaddr*)&server_addr, 
                              sizeof(server_addr));
    if (res)
    {
        perror("connect()");
        exit(EXIT_FAILURE);
    }
    
    int nfds = server + 1;
    fd_set set;
    FD_ZERO(&set);
    FD_SET (server, &set);

    errno = 0;
    res = select(nfds, &set, NULL, NULL, NULL);
    if (res < 0)
    {
        perror("select()");
        exit (EXIT_FAILURE);
    }
    
    // socket is available for read -> server's dead

    exit(EXIT_FAILURE);
}

int start_keepalive_check(int server, struct sockaddr_in* server_addr,
                          struct server_pack* check_thread_arg)
{
    check_thread_arg->sock        = server;
    check_thread_arg->serv_s_addr = server_addr->sin_addr.s_addr;

    pthread_t check_thread;
    if (pthread_create(&check_thread, NULL, 
                       check_connection, check_thread_arg))
    {
        perror("pthread_create()");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int find_server(struct sockaddr_in* server_addr)
{
    assert(server_addr);

    int res;

    errno = 0;
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    if (server < 0)
    {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in bcast_addr;
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port   = htons(HANDSHAKE_PORT);
    bcast_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    errno = 0;
    res = bind(server, (struct sockaddr*)&bcast_addr, sizeof(bcast_addr));
    if (res)
    {
        perror("bind()");
        close(server);
        return -1;
    }

    // Getting server addr
    char msg;
    socklen_t server_addr_size = sizeof(*server_addr);
    errno = 0;
    res = recvfrom(server, &msg, sizeof(msg), 0,
                   (struct sockaddr*)server_addr, &server_addr_size);
    if (res < 0)
    {
        perror("recvfrom()");
        close(server);
        return -1;
    }

    DBG printf("Server address: %s\n", inet_ntoa(server_addr->sin_addr));

    return server;
}


int notify_server(int server, struct sockaddr_in* server_addr)
{
    assert(server_addr);
    
    int res;

    int optval = 1;
    errno = 0;
    res = setsockopt(server, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    if (res)
    {
        perror("setsockopt()");
        return -1;
    }

    res = send_address(server, server_addr, N_NOTIFY_RETRIES);
    if (res)
    {
        printf("send_address() failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int server_handshake(struct sockaddr_in* server_addr)
{
    assert(server_addr);
 
    int server = find_server(server_addr);
    if (server_addr < 0)
    {
        printf("Server not found\n");
        return EXIT_FAILURE;
    }

    int res = notify_server(server, server_addr);
    if (res < 0)
    {
        printf("Failed to notify server\n");
        return EXIT_FAILURE;
    }

    close(server);

    return EXIT_SUCCESS;
}

int establish_main_connection(struct sockaddr_in* server_addr)
{
    assert(server_addr);

    errno = 0;
    int main_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (main_sock < 0)
    {
        perror("socket()");
        return -1;
    }

    server_addr->sin_port = htons(MAIN_PORT);

    errno = 0;
    int res = connect(main_sock, (struct sockaddr*)server_addr, 
                      sizeof(*server_addr));
    if (res)
    {
        perror("connect()");
        return -1;
    }

    return main_sock;
}

int send_info(int server, size_t n_threads)
{
    char n_threads_shortened = (char)n_threads;

    errno = 0;
    int res = send(server, &n_threads_shortened, sizeof(n_threads_shortened),0);
    if (res < 0)
    {
        perror("send()");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int receive_bound(int server, double* bound)
{
    ssize_t msg_size = recv_msg(server, bound, sizeof(double), 0);
    if (msg_size < 0)
    {
        printf("recv_msg() failed\n");
        return EXIT_FAILURE;
    }
    else
    if (msg_size < sizeof(double))
    {
        printf("Received %zd bytes - assuming server dead.\n", msg_size);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// Server

static int create_receiveing_sock(int domain, int type, int protocol,
                                  const struct sockaddr_in* addr)
{
    errno = 0;
    int sock = socket(domain, type, protocol);
    if (sock < 0)
    {
        perror("socket()");
        return -1;
    }

    int yes = 1;
    errno = 0;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (res)
    {
        perror("setsockopt()");
        return -1;
    }
    
    errno = 0;
    res = bind(sock, (struct sockaddr*)addr, sizeof(*addr));
    if (res)
    {
        perror("bind()");
        close(sock);
        return -1;
    }

    errno = 0;
    res = listen(sock, N_CLIENTS_MAX);
    if (res)
    {
        perror("listen()");
        close(sock);
        return -1;
    }

    return sock;
}

int init_server(int* main_sock, int* keepalive_sock)
{
    assert(main_sock);
    assert(keepalive_sock);
/*
    errno = 0;
    int main_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (main_socket < 0)
    {
        perror("socket()");
        return -1;
    }

    int yes = 1;
    errno = 0;
    int res = setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, 
                                      &yes, sizeof(yes));
    if (res)
    {
        perror("setsockopt()");
        return -1;
    }
*/

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(MAIN_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    *main_sock = create_receiveing_sock(AF_INET, SOCK_STREAM | SOCK_NONBLOCK,
                                        0, &server_addr);
    if (*main_sock < 0)
    {
        printf("create_receiveing_sock() failed\n");
        printf("main_sock failed\n");
        return EXIT_FAILURE;
    }

    server_addr.sin_port   = htons(KEEPALIVE_PORT);

    *keepalive_sock = create_receiveing_sock(AF_INET, SOCK_STREAM, 0,
                                             &server_addr);
    if (*keepalive_sock < 0)
    {
        printf("create_receiveing_sock() failed\n");
        printf("keepalive_sock failed\n");
        return EXIT_FAILURE;
    }
/*
    errno = 0;
    res = bind(main_socket, (struct sockaddr*)&server_addr, 
               sizeof(server_addr));
    if (res)
    {
        perror("bind()");
        close(main_socket);
        return -1;
    }

    errno = 0;
    res = listen(main_socket, N_CLIENTS_MAX);
    if (res)
    {
        perror("listen()");
        close(main_socket);
        return -1;
    }

    return main_socket;
*/
    return EXIT_SUCCESS;
}

int find_clients(struct client_info clients[N_CLIENTS_MAX])
{
    int res;

    errno = 0;
    int bcast = socket(AF_INET, SOCK_DGRAM, 0);
    if (bcast < 0)
    {
        perror("socket()");
        return -1;
    }

    int optval = 1;
    res = setsockopt(bcast, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    if (res)
    {
        perror("setsockopt()");
        close(bcast);
        return -1;
    }

    struct sockaddr_in bcast_addr;
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port   = htons(HANDSHAKE_PORT);
    inet_pton(AF_INET, "255.255.255.255", &(bcast_addr.sin_addr));


    int n_clients = 0;
    int retries   = 1;
    do
    {
        // Broadcast self address
        res = send_address(bcast, &bcast_addr, retries);
        if (res)
        {
            perror("send_address() failed\n");
            close(bcast);
            return EXIT_FAILURE;
        }

        n_clients = wait_for_clients_start(bcast, clients);
        if (n_clients < 0)
        {
            printf("wait_for_clients_start() failed\n");
            close(bcast);
            return -1;
        }
    }while(!n_clients && retries++ < N_WAITING_RETRIES);

    close(bcast);
    return n_clients;
}

int wait_for_clients_start(int sock, 
                           struct client_info clients[N_CLIENTS_MAX])
{
    struct sockaddr_in client;
    socklen_t client_addr_len = sizeof(client);
    int n_clients = 0;

    int clients_waiting = 0;
    int nfds = sock + 1;
    do
    {
        struct timeval timeout = CLIENTS_HANDSHAKE_TIMEOUT;
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET (sock, &sock_set);

        clients_waiting = select(nfds, &sock_set, NULL, NULL, &timeout);
        if (clients_waiting < 0)
        {
            perror("select()");
            return -1;
        }
        else 
        if (clients_waiting == 0)   // Timeout or msgs ended
            return n_clients;
        else
        {
            char msg;
            errno = 0;
            int res = recvfrom(sock, &msg, sizeof(msg), 0,
                               (struct sockaddr*)&client, &client_addr_len);
            if (res < 0)
            {
                perror("recvfrom()");
                return -1;
            }
            n_clients = register_client(&client, clients, n_clients);

            #ifdef    DEBUG
            printf("received byte from %s\n", clients[n_clients - 1].ip);
            #endif // DEBUG
        }
    }while(clients_waiting);
    
    return n_clients;
}

int register_client(struct sockaddr_in* client_addr,
                    struct client_info clients[N_CLIENTS_MAX], int n_clients)
{
    assert(client_addr);

    for(int i = 0; i < N_CLIENTS_MAX; i++)
    {
        if (clients[i].addr == client_addr->sin_addr.s_addr)
        {
            DBG printf("Client %s repeated\n", 
                       inet_ntoa(client_addr->sin_addr));

            return n_clients;
        }
        else
        if (clients[i].addr == 0)
        {
            DBG printf("Client %s new\n", inet_ntoa(client_addr->sin_addr));
            clients[i].addr = client_addr->sin_addr.s_addr;
            #ifdef    DEBUG
            strcpy(clients[i].ip, inet_ntoa(client_addr->sin_addr));
            #endif // DEBUG
            return n_clients + 1;
        }
    }

    return n_clients;
}

/*
    Maybe N_WAITING_RETRIES should be changed to separate value
 */
int get_client_info(int sock, struct client_info clients[N_CLIENTS_MAX],
                    int n_clients)
{
    int nfds = sock + 1;

    int n_answered = 0;
    int retries    = 1;
    int clients_waiting = 0;
    do
    {
        struct timeval timeout = CLIENTS_INFO_TIMEOUT;
        fd_set sock_set;
        FD_ZERO(&sock_set);
        FD_SET (sock, &sock_set);

        clients_waiting = select(nfds, &sock_set, NULL, NULL, &timeout);
        if (clients_waiting < 0)
        {
            perror("select()");
            return -1;
        }
        else
        if (clients_waiting == 0)
        {
            DBG printf("Waiting: %d answered, %d'th wait\n", n_answered,
                                                             retries);
            retries++;
        }
        else
        {
            errno = 0;
            int new_sock = accept(sock, NULL, NULL);
            if (new_sock < 0)
            {
                perror("accept()");
                return -1;
            }

            char n_threads = 0;
            errno = 0;
            int received = recv(new_sock, &n_threads, sizeof(n_threads), 0);
            if (received < 0)
            {
                perror("recv()");
                return -1;
            }
            else
            if (received == 0)
            {
                printf("Client died after connecting\n");
                return -1;
            }

            clients[n_answered].sock      = new_sock;
            clients[n_answered].n_threads = n_threads;

            n_answered++;
            
            DBG printf("Connection accepted, %d answered\n", n_answered);
        }

    }while(n_answered < n_clients && retries < N_WAITING_RETRIES);

    return n_answered;
}

int start_clients(struct client_info clients[N_CLIENTS_MAX], int n_clients)
{
    int res;
    size_t size = sizeof(clients[0].left_bound);

    for(int i = 0; i < n_clients; i++)
    {
        double bounds[2] = { clients[i]. left_bound,
                             clients[i].right_bound };
        for(int k = 0; k < 2; k++)
        {
            res = send_msg(clients[i].sock, &bounds[k], size, 0);
            if (res < 0)
            {
                printf("send_msg() failed\n");
                return EXIT_FAILURE;
            }
            else
            if (res < size)
            {
                printf("send_msg() sent %d bytes of %lu\n", res, size);
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

int set_client_connections(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients)
{
#define SOCK(i) clients[(i)].sock
    int res = 0;
    for(int i = 0; i < n_clients; i++)
    {
        res = set_tcp_keepalive(SOCK(i), 5, 2, 1);
        if (res < 0)
        {
            printf("set_tcp_keepalive() failed\n");
            return EXIT_FAILURE;
        }

/*
        int option = 1;
        errno = 0;
        res = setsockopt(SOCK(i), SOL_SOCKET, SO_KEEPALIVE,
                         &option, sizeof(option));
        if (res)
        {
            perror("setsockopt()");
            printf("SO_KEEPALIVE failed\n");
            return EXIT_FAILURE;
        }
        
        option = 10;    // 10 sec until assuming connection dead
        errno = 0;
        res = setsockopt(SOCK(i), IPPROTO_TCP, TCP_KEEPIDLE, 
                         &option, sizeof(option));
        if (res)
        {
            perror("setsockopt()");
            printf("TCP_KEEPIDLE failed\n");
            return EXIT_FAILURE;
        }

        option = 5;     // 5 packets
        errno = 0;
        res = setsockopt(SOCK(i), IPPROTO_TCP, TCP_KEEPCNT, 
                         &option, sizeof(option));
        if (res)
        {
            perror("setsockopt()");
            printf("TCP_KEEPCNT failed\n");
            return EXIT_FAILURE;
        }

        option = 1;     // 1 sec between packets
        errno = 0;
        res = setsockopt(SOCK(i), IPPROTO_TCP, TCP_KEEPINTVL, 
                         &option, sizeof(option));
        if (res)
        {
            perror("setsockopt()");
            printf("TCP_KEEPINTVL failed\n");
            return EXIT_FAILURE;
        }
*/
    }
#undef SOCK

    return EXIT_SUCCESS;
}

int init_client_keepalive(int keepalive_sock,
                struct client_info clients[N_CLIENTS_MAX], int n_clients)
{
    int clients_connected = 0;
    while(clients_connected < n_clients)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET (keepalive_sock, &set);
        int nfds = keepalive_sock + 1;
        struct timeval timeout = CLIENTS_PING_TIMEOUT;

        errno = 0;
        int ready = select(nfds, &set, NULL, NULL, &timeout);
        if (ready < 0)
        {
            perror("select()");
            return -1;
        }
        else
        if (ready == 0)
        {
            break;
        }
        else
        {
            errno = 0;
            int sock = accept(keepalive_sock, NULL, NULL);
            if (sock < 0)
            {
                perror("accept()");
                return -1;
            }

            clients[clients_connected++].keepalive = sock;
        }
    }

    return clients_connected;
}

void distribute_load(struct client_info clients[N_CLIENTS_MAX], int n_clients,
                     double left_bound, double right_bound)
{
    int n_threads_all = 0;
    for(int i = 0; i < n_clients; i++)
        n_threads_all += clients[i].n_threads;
    DBG printf("Threads amount: %d\n", n_threads_all);

    double load_per_thread = (right_bound - left_bound)/n_threads_all;

    double current = LEFT_BOUND;
    for(int i = 0; i < n_clients; i++)
    {
        clients[i].left_bound  = current;
        clients[i].right_bound = 
            (current += load_per_thread * clients[i].n_threads);
    }
}



static int  max_fds(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients)
{
    int nfds = -1;
    for(int i = 0; i < n_clients; i++)
        if (!clients[i].finished && clients[i].sock > nfds) 
            nfds = clients[i].sock;
    return nfds;
}

static void fill_fd_set(struct client_info clients[N_CLIENTS_MAX], 
                        int n_clients, fd_set* set)
{
    for(int i = 0; i < n_clients; i++)
        if (!clients[i].finished)
            FD_SET(clients[i].sock, set);
}

/* Returns if any client is dead */
static int gather_results(struct client_info clients[N_CLIENTS_MAX],
                          int n_clients, fd_set* set)
{
    for(int i = 0; i < n_clients; i++)
    {
        if (FD_ISSET(clients[i].sock, set))
        {
            clients[i].finished  = 1;

            double client_result = 0;
            int res = recv_msg(clients[i].sock, &client_result,
                               sizeof(client_result), 0);
            if (res < 0)
            {
                printf("recv_msg() failed\n");
                return -1;   
            }
            else
            if (res == 0)
            {
                return 1;   // Assuming client dead
            }
            else
            if (res < sizeof(client_result))
            {
                printf("recv_msg() failed to recieve whole message.\n");
                return -1;
            }
            
            clients[i].result = client_result;
        }
    }

    return 0;
}

int wait_for_clients_finish(struct client_info clients[N_CLIENTS_MAX],
                            int n_clients)
{
    int clients_waiting = 0;
    while(1)
    {
        int max = max_fds(clients, n_clients);
        if (max < 0)        break;  // No clients left
        
        int nfds = max + 1;
        struct timeval timeout = CLIENTS_PING_TIMEOUT;

        fd_set clients_socks;
        FD_ZERO(&clients_socks);
        fill_fd_set(clients, n_clients, &clients_socks);

        errno = 0;
        clients_waiting = select(nfds, &clients_socks, NULL, NULL, &timeout);
        if (clients_waiting <  0)
        {
            perror("select()");
            return EXIT_FAILURE;
        }
        if (clients_waiting == 0)
        {
            DBG printf("Continuing poll, all alive\n");
        }
        else
        {
            int client_dead = gather_results(clients, n_clients, 
                                             &clients_socks);
            if (client_dead < 0)
            {
                printf("gather_results() failed\n");
                return EXIT_FAILURE;
            }
            else
            if (client_dead)
            {
                printf("Client is dead\n");
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}


double collect_info(struct client_info clients[N_CLIENTS_MAX], int n_clients)
{
    double sum = 0;
    for(int i = 0; i < n_clients; i++)
        sum += clients[i].result;
    return sum;
}

void close_connections(struct client_info clients[N_CLIENTS_MAX],
                    int n_clients)
{
    for(int i = 0; i < n_clients; i++)
    {
        close(clients[i].sock);
        close(clients[i].keepalive);
    }
}




