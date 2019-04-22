
#include "multithreading.h"

double integrate(double left_bound, double right_bound, size_t n_threads)
{
    DBG printf("Requested for threads\t%4lu\n\n", n_threads);

    struct sysconfig_t sys;
    if (get_system_config(&sys))
    {
        printf("get_system_config() failed\n");
        return EXIT_FAILURE;
    }
   
    size_t n_thread_args = 
        ((n_threads < sys.n_proc_onln)? sys.n_proc_onln : n_threads);
    DBG printf("Creating  args:\t\t%4lu\n", n_thread_args);

    errno = 0;
    struct arg_t* thread_args = 
        aligned_alloc(sys.cache_line, sys.cache_line * n_thread_args);
    if (!thread_args)
    {
        perror("posix_memalign()");
        return EXIT_FAILURE;
    }
    DBG printf("Allocated %lu bytes each %lu bytes aligned to %p\n\n",
               sys.cache_line * n_thread_args, sizeof(struct arg_t), 
               thread_args);
    
    pthread_t tids[((n_threads < sys.n_proc_onln)? 
                     sys.n_proc_onln : n_threads)];

    DBG printf("Starting threads\n");
    if (start_threads(&sys, n_threads, tids, thread_args, 
                      left_bound, right_bound))
    {
        printf("start_threads() failed\n");
        return EXIT_FAILURE;
    }

    DBG printf("Joining threads\n");
    double sum = 0;
    if (join_threads(&sys, n_threads, tids, thread_args, &sum))
    {
        printf("join_threads() failed\n");
        return EXIT_FAILURE;
    }

    delete_config(&sys);
    free(thread_args);
   
    return sum;
}

void* routine(void* arg)
{
    register struct arg_t* bounds = (struct arg_t*)arg;
    register double left  = bounds->left;
    register double right = bounds->right;
    register double sum = 0;
    for(register double i = left; i < right; i += INTEGRAL_STEP)
        sum += INTEGRAL_STEP*f(i);

    bounds->sum = sum;
 
    return NULL;
}

/*
    threads_per_cpu expected to be of sys->n_proc_onln size
 */
static void distribute_threads(int threads_per_cpu[], size_t n_threads,
                               struct sysconfig_t* sys)
{ 
    const int n_threads_per_cpu = n_threads / sys->n_proc_onln;
    int extra_threads = n_threads % sys->n_proc_onln;
    
    for(int i = 0; i < sys->n_proc_onln; i++)
        threads_per_cpu[i] = n_threads_per_cpu;
    for(int i = sys->n_proc_onln - 1; extra_threads--; i--)
        threads_per_cpu[i]++;
}

static void init_thread_args(struct arg_t args[], size_t n_threads,
                             double left_bound, double right_bound,
                             struct sysconfig_t* sys)
{
    const double average_load  = (right_bound - left_bound) / sys->n_proc_onln;
    
    int threads_per_cpu[sys->n_proc_onln];
    distribute_threads(threads_per_cpu, n_threads, sys);

    double current = left_bound;
    int cur_cpu = 0;
    int threads_left = threads_per_cpu[0];
    for(size_t i = 0; i < n_threads; i++)
    {
        if (!threads_left)
            threads_left = threads_per_cpu[++cur_cpu];
        threads_left--;

        double step = average_load/threads_per_cpu[cur_cpu];

        args[i].left  = current;
        args[i].right = current + step;

        current += step;
    }

    for(int i = 0; i < n_threads; i++)
        DBG printf("args[%2d] = %lg\n", i, args[i].right - args[i].left);
}

static int set_thread_affinity(pthread_attr_t* attr, int cpu_num)
{
    cpu_set_t cpuset;

    errno = 0;
    if (pthread_attr_init(attr))
    {
        perror("pthread_attr_init");
        return EXIT_FAILURE;
    }

    CPU_ZERO(&cpuset);
    CPU_SET(cpu_num, &cpuset);
 
    errno = 0;
    if (pthread_attr_setaffinity_np(attr, sizeof(cpuset), &cpuset))
    {
        perror("pthread_attr_setaffinity_np");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


static int start_idle_threads(struct sysconfig_t* sys, size_t n_threads, 
                        pthread_t tids[], struct arg_t args[],
                        double left_bound, double right_bound)
{ 
    double thread_load = (right_bound - left_bound) / n_threads;
    init_thread_args(args, sys->n_proc_onln, 
    left_bound, left_bound + thread_load*sys->n_proc_onln, sys);

    pthread_attr_t attrs[sys->n_proc_onln];
    for(size_t i = 0; i < sys->n_proc_onln; i++)
        if (set_thread_affinity(&attrs[i], sys->cpus[i].number))
        {
            printf("set_thread_affinity() failed\n");
            return EXIT_FAILURE;
        }

    for(size_t i = 1; i < sys->n_proc_onln; i++)
    {
        errno = 0;
        if (pthread_create(&tids[i], &attrs[i], routine, &args[i]))
        {
            perror("pthread_create()");
            return EXIT_FAILURE;
        }
    }


    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(sys->cpus[0].number, &cpuset);
 
    errno = 0;
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset))
    {
        perror("pthread_attr_setaffinity_np");
        return EXIT_FAILURE;
    }
    
    tids[0] = pthread_self();
    routine(&args[0]);
    
    return EXIT_SUCCESS;
}

static int start_useful_threads(struct sysconfig_t* sys, size_t n_threads, 
                        pthread_t tids[], struct arg_t args[],
                        double left_bound, double right_bound)
{
    DBG printf("Threads per cpu: %2lu\nextra threads  : %2lu\n",
                n_threads / sys->n_proc_onln, 
                n_threads % sys->n_proc_onln);

    init_thread_args(args, n_threads, left_bound, right_bound, sys);

    int threads_per_cpu[sys->n_proc_onln];
    distribute_threads(threads_per_cpu, n_threads, sys);

    for(int i = 0; i < sys->n_proc_onln; i++)
        DBG printf("load[%2d] = %2d\n", i, threads_per_cpu[i]);

    pthread_attr_t attrs[n_threads];
    int cur_cpu = 0;
    int threads_left = threads_per_cpu[0];
    for(size_t i = 0; i < n_threads; i++)
    {
        if (!threads_left)
            threads_left = threads_per_cpu[++cur_cpu];
        threads_left--;

        if (set_thread_affinity(&attrs[i], cur_cpu))
        {
            printf("set_thread_affinity() failed\n");
            return EXIT_FAILURE;
        }
    }

    for(size_t i = 1; i < n_threads; i++)
    {
        errno = 0;
        if (pthread_create(&tids[i], &attrs[i], routine, &args[i]))
        {
            perror("pthread_create()");
            return EXIT_FAILURE;
        }
        DBG printf("Created %2lu'th thread\n", i);
    }
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(sys->cpus[0].number, &cpuset);
 
    errno = 0;
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset))
    {
        perror("pthread_attr_setaffinity_np");
        return EXIT_FAILURE;
    }
    
    tids[0] = pthread_self();
    routine(&args[0]);
    
    return EXIT_SUCCESS;
}

int start_threads(struct sysconfig_t* sys, size_t n_threads, 
                        pthread_t tids[], struct arg_t args[],
                        double left_bound, double right_bound)
{
    if (n_threads <= sys->n_proc_onln)
        return start_idle_threads(sys, n_threads, tids, args,
                                  left_bound, right_bound);
    else
        return start_useful_threads(sys, n_threads, tids, args,
                                  left_bound, right_bound);
}



int join_threads (struct sysconfig_t* sys, size_t n_threads, 
                         pthread_t tids[], struct arg_t args[], double* sum)
{
    *sum = 0;
    size_t n_real_threads = 
        ((n_threads <= sys->n_proc_onln)? sys->n_proc_onln : n_threads);
    for(size_t i = 1; i < n_real_threads; i++)
    {
        DBG printf("Joining %lu'th thread\n", i);
        errno = 0;
        if (pthread_join(tids[i], NULL))
        {
            perror("pthread_join()");
            printf("Failed to join %lu'th thread\n", i);
            return EXIT_FAILURE;
        }
        if (i < n_threads)
            *sum += args[i].sum;
    }
    *sum += args[0].sum;

    return EXIT_SUCCESS;
}




