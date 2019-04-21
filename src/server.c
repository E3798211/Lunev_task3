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

    printf("found %d clients\n", n_clients);










    // double result = integrate(LEFT_BOUND, RIGHT_BOUND, n_threads);
    
    return 0;
}

