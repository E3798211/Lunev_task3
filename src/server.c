#include <stdio.h>
#include "multithreading.h"
#include "networking.h"

int main(int argc, char* argv[])
{
    int main_sock = init_server();
    if (main_sock < 0)
    {
        printf("init_server() failed\n");
        return EXIT_FAILURE;
    }

    struct client_info clients[N_CLIENTS_MAX] = {};
    int n_clients = find_clients(clients);
    if (n_clients < 0)
    {
        printf("find_clients() failed\n");
        close(main_sock);
        return EXIT_FAILURE;
    }
    else
    if (n_clients == 0)
    {
        printf("Nobody answered. Shutting down.\n");
        close(main_sock);
        return EXIT_FAILURE;
    }
    DBG printf("found %d clients\n", n_clients);

    int res = get_client_info(main_sock, clients, n_clients);
    if (res < 0)
    {
        printf("get_client_info() failed\n");
        close(main_sock);
        return EXIT_FAILURE;
    }
    else
    if (res < n_clients)
    {
        printf("Some clients haven't answered. Shutting down.\n");
        close(main_sock);
        return EXIT_FAILURE;
    }
    DBG printf("All clients connected, info gathered\n");

    // No-one will connect
    close(main_sock);

    distribute_load(clients, n_clients, LEFT_BOUND, RIGHT_BOUND);

    res = start_clients(clients, n_clients);
    if (res)
    {
        printf("start_clients() failed\n");
        return EXIT_FAILURE;
    }

    res = set_client_connections(clients, n_clients);
    if (res)
    {
        printf("set_client_connections() failed\n");
        return EXIT_FAILURE;
    }

    res = wait_for_clients_finish(clients, n_clients);
    if (res)
    {
        printf("wait_for_clients_finish() failed\n");
        close(main_sock);
        return EXIT_FAILURE;
    }

    double sum = collect_info(clients, n_clients);
    close_connections(clients, n_clients);
   
    printf("\n\n%lg\n", sum);

    return 0;
}

