#include <stdio.h>
#include "multithreading.h"
#include "networking.h"

int main(int argc, char* argv[])
{
    int res;

    if (argc != 2)
    {
        printf("Usage: %s <n_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    size_t n_threads = 0;
    if (get_positive(argv[1], &n_threads))
    {
        printf("Invalid input\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    res = server_handshake(&server_addr);
    if (res)
    {
        printf("Handshake failed\n");
        return EXIT_FAILURE;
    }

   
    // establish_connection()


    printf("slave\n");
    return 0;
}

