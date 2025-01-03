#include <stdio.h>

#include <unistd.h>
#include <pthread.h>

void *thread(void *arg)
{
    int id = (int)arg;
    printf("Thread #%d: PID = %d, TID= %ld\n", id, getpid(), pthread_self());
    pthread_exit((void *)(id * 2));
}

int main(void)
{
    int i;
    pthread_t threads[5];

    for (i = 0; i < 5; ++i)
    {
        if (pthread_create(&threads[i], NULL, thread, i) != 0)
        {
            printf("Thread creation error!\n");
            return 1;
        }
    }

    for (i = 0; i < 5; ++i)
    {
        pthread_join(threads[i], NULL);
        printf("Thread %ld is terminated.\n", threads[i]);
    }
    
    pthread_exit(NULL);
}