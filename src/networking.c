
#include "networking.h"

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
    bcast_addr.sin_port   = htons(PORT);
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

    char msg = '1';
    for(int i = 0; i < N_NOTIFY_RETRIES; i++)
    {
        errno = 0;
        res = sendto(server, &msg, sizeof(msg), 0,
                     (struct sockaddr*)server_addr, sizeof(*server_addr));
        if (res < 0)
        {
            perror("sendto()");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}














