#include <stdio.h>
#include "multithreading.h"
#include "networking.h"

int main(int argc, char* argv[])
{
    int main_sock, keepalive_sock;
    int res = init_server(&main_sock, &keepalive_sock);
    if (res)
    {
        printf("init_server() failed\n");
        goto EXIT;
    }

    struct client_info clients[N_CLIENTS_MAX] = {};
    int n_clients = find_clients(clients);
    if (n_clients < 0)
    {
        printf("find_clients() failed\n");
        goto EXIT;
    }
    else
    if (n_clients == 0)
    {
        printf("Nobody answered. Shutting down.\n");
        goto EXIT;
    }
    DBG printf("found %d clients\n", n_clients);

    res = get_client_info(main_sock, clients, n_clients);
    if (res < 0)
    {
        printf("get_client_info() failed\n");
        goto EXIT;
    }
    else
    if (res < n_clients)
    {
        printf("Some clients haven't answered. Shutting down.\n");
        goto EXIT;
    }
    DBG printf("All clients connected, info gathered\n");

    // No-one will connect
    // close(main_sock);

    distribute_load(clients, n_clients, LEFT_BOUND, RIGHT_BOUND);

    res = start_clients(clients, n_clients);
    if (res)
    {
        printf("start_clients() failed\n");
        goto EXIT;
    }

    res = set_client_connections(clients, n_clients);
    if (res)
    {
        printf("set_client_connections() failed\n");
        goto EXIT;
    }

    res = init_client_keepalive(keepalive_sock, clients, n_clients);
    if (res < 0)
    {
        printf("init_client_keepalive() failed\n");
        goto EXIT;
    }
    else
    if (res < n_clients)
    {
        printf("init_client_keepalive(): connected only %d clients\n",
               n_clients);
        goto EXIT;
    }
    DBG printf("%d clients checking me\n", res);

    res = wait_for_clients_finish(clients, n_clients);
    if (res)
    {
        printf("wait_for_clients_finish() failed\n");
        goto EXIT;
    }
    DBG printf("All clients returned answer\n");

    double sum = collect_info(clients, n_clients);
    close_connections(clients, n_clients);
   
    printf("\n\n%lg\n", sum);

EXIT:
    close(main_sock);
    close(keepalive_sock);
    return 0;
}

