#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE 5

int buffer[BUFFER_SIZE];
int in = 0, out = 0;

sem_t empty;   // counts empty slots (init = BUFFER_SIZE)
sem_t full;    // counts filled slots (init = 0)
sem_t mutex;   // binary semaphore for mutual exclusion (init = 1)

void* Producer(void* arg) {
    int item = 0;
    while (1) {
        item++;
        sem_wait(&empty);   // wait if buffer is full
        sem_wait(&mutex);   // enter critical section

        buffer[in] = item;
        printf("Producer produced: %d at index %d\n", item, in);
        in = (in + 1) % BUFFER_SIZE;

        sem_post(&mutex);   // leave critical section
        sem_post(&full);    // signal that a new item is available

        sleep(1);
    }
    return NULL;
}

void* Consumer(void* arg) {
    int item;
    while (1) {
        sem_wait(&full);    // wait if buffer is empty
        sem_wait(&mutex);   // enter critical section

        item = buffer[out];
        printf("Consumer consumed: %d from index %d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;

        sem_post(&mutex);   // leave critical section
        sem_post(&empty);   // signal that a slot is now free

        sleep(2);
    }
    return NULL;
}

int main() {
    pthread_t prod_thread, cons_thread;

    // Initialize semaphores
    sem_init(&empty, 0, BUFFER_SIZE); // all slots empty initially
    sem_init(&full,  0, 0);           // no items in buffer initially
    sem_init(&mutex, 0, 1);           // binary semaphore (mutex)

    pthread_create(&prod_thread, NULL, Producer, NULL);
    pthread_create(&cons_thread, NULL, Consumer, NULL);

    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&mutex);

    return 0;
}