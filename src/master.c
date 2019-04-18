
#define _GNU_SOURCE

#include <sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "service.h"
#include "system.h"
#include "multithreading.h"

#ifdef DEBUG
    #define DBG
#else
    #define DBG if(0)
#endif

int main(int argc, char* argv[])
{
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

    double result = integrate(LEFT_BOUND, RIGHT_BOUND, n_threads);
    
    return 0;
}

