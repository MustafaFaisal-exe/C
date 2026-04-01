#include <stdio.h>
#include <pthread.h>

void* worker(void* arg) {
    printf("Thread working...\n");
    pthread_exit("Done");
}

int main() {
    pthread_t tid;
    void* status;

    pthread_create(&tid, NULL, worker, NULL);
    pthread_join(tid, &status);

    printf("Thread returned: %s\n", (char*)status);

    return 0;
}