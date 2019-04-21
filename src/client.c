#include <stdio.h>
#include "networking.h"

int main()
{

    struct sockaddr_in server_addr;
    int server = find_server(&server_addr);
    notify_server(server, &server_addr);
    close(server);


    printf("slave\n");
    return 0;
}

