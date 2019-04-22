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
    DBG printf("Handshake completed\n");
   
    int main_sock = establish_main_connection(&server_addr);
    if (main_sock < 0)
    {
        printf("establish_main_connection() failed\n");
        return EXIT_FAILURE;
    }
    DBG printf("Connected to server\n");

    res = send_info(main_sock, n_threads);
    if (res)
    {
        printf("send_info() failed\n");
        return EXIT_FAILURE;
    }
    DBG printf("Info sent\n");

    double left_bound, right_bound;
    res = receive_bound(main_sock, &left_bound);
    if (res)
    {
        printf("receive_bound() [left ] failed\n");
        return EXIT_FAILURE;
    }
    res = receive_bound(main_sock, &right_bound);
    if (res)
    {
        printf("receive_bound() [right] failed\n");
        return EXIT_FAILURE;
    }
    DBG printf("Left =  %lg; Right = %lg\n", left_bound, right_bound);


// TEMPORARY
//    left_bound = -1;
//    right_bound = 1;


    double sum = integrate(left_bound, right_bound, n_threads);

    res = send_msg(main_sock, &sum, sizeof(sum));
    if (res < 0)
    {
        printf("send_msg() failed\n");
        return EXIT_FAILURE;
    }
    DBG printf("Result sent\n");

    DBG printf("Client quit\n");
    return 0;
}

