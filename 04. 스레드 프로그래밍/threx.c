#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

pthread_mutex_t mutex;
volatile long int counter = 0;
int limit;

void *worker(void *a)
{
    for (int i = 0; i < limit; ++i)
    {
        pthread_mutex_lock(&mutex);
        counter++;
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}

int main(int argc, int *argv[])
{
    if (argc < 3)
    {
        printf("인자가 적습니다.\n");
        return 1;
    }

    pthread_t *tid;

    int numTh = atoi(argv[1]);
    if (numTh == 0)
    {
        printf("numTh가 잘못되었습니다.\n");
        return 1;
    }
    limit = atoi(argv[2]);
    if (limit == 0)
    {
        printf("limit가 잘못되었습니다.\n");
        return 1;
    }
    tid = malloc(sizeof(pthread_t) * numTh);
    pthread_mutex_init(&mutex, NULL);
    for (int i = 0; i < numTh; ++i)
    {
        if (pthread_create(&tid[i], NULL, worker, NULL) != 0)
        {
            printf("Thread creation error!\n");
            return 1;
        }
    }
    for (int i = 0; i < numTh; ++i)
    {
        if (pthread_join(tid[i], NULL) != 0)
        {
            printf("Thread join error!\n");
            return 1;
        }
    }
    pthread_mutex_destroy(&mutex);
    printf("Result: %ld\n", counter);

    pthread_exit(NULL);
}