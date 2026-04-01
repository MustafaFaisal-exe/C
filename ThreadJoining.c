#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void* compute(void* arg) {
    int *result = malloc(sizeof(int));
    *result = 42;
    pthread_exit(result);
}

int main() {
    pthread_t tid;
    void* ret;

    pthread_create(&tid, NULL, compute, NULL);
    pthread_join(tid, &ret);

    printf("Returned value: %d\n", *(int*)ret);

    return 0;
}