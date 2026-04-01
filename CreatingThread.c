#include <stdio.h>
#include <pthread.h>

void* myThread(void* arg) {
    printf("Hello from thread!\n");
    return NULL;
}

int main() {
    pthread_t tid;

    pthread_create(&tid, NULL, myThread, NULL);

    printf("Hello from main!\n");

    pthread_join(tid, NULL); // wait for thread

    return 0;
}