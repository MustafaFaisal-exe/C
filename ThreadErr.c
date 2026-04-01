#include <stdio.h>
#include <pthread.h>
#include <errno.h>

void* task(void* arg) {
    return NULL;
}

int main() {
    pthread_t tid;
    int result = pthread_create(&tid, NULL, task, NULL);

    if (result != 0) {
        perror("Thread creation failed");
        return 1;
    }

    pthread_join(tid, NULL);
    return 0;
}