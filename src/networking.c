
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

    DBG printf("sent byte to %s %d times\n", inet_ntoa(addr->sin_addr), 
                                             n_times);

    return EXIT_SUCCESS;
}

// Client

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


// Server

int init_server()
{
    errno = 0;
    int main_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (main_socket < 0)
    {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(MAIN_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    errno = 0;
    int res = bind(main_socket, (struct sockaddr*)&server_addr, 
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
        else        // Empty cell
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
            return EXIT_FAILURE;
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
                return EXIT_FAILURE;
            }

            char n_threads = 0;
            errno = 0;
            int received = recv(new_sock, &n_threads, sizeof(n_threads), 0);
            if (received < 0)
            {
                perror("recv()");
                return EXIT_FAILURE;
            }

            clients[n_answered].sock      = new_sock;
            clients[n_answered].n_threads = n_threads;

            n_answered++;
            
            DBG printf("Connection accepted, %d answered\n", n_answered);
        }

    }while(n_answered < n_clients && retries < N_WAITING_RETRIES);

    return EXIT_SUCCESS;
}















