#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define SHM_SIZE 256

// ---------- shared data ----------
char shared_memory[SHM_SIZE];

// ---------- synchronisation ----------
sem_t writer_sem;          // signals writer that all readers are done
sem_t reader_turn[100];    // each reader gets its own semaphore
int   n_readers;           // total number of reader threads
int   readers_done;        // how many readers finished in this round

pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;

// ---------- Writer ----------
void* writer_thread(void* arg) {
    while (1) {
        // --- take input ---
        printf("\n[Writer] Enter data: ");
        fflush(stdout);
        fgets(shared_memory, SHM_SIZE, stdin);
        shared_memory[strcspn(shared_memory, "\n")] = '\0'; // strip newline
        printf("[Writer] Written to shared memory: \"%s\"\n", shared_memory);

        // --- reset done counter ---
        pthread_mutex_lock(&done_mutex);
        readers_done = 0;
        pthread_mutex_unlock(&done_mutex);

        // --- wake reader 0 to start the chain ---
        sem_post(&reader_turn[0]);

        // --- wait until ALL readers have finished ---
        sem_wait(&writer_sem);
    }
    return NULL;
}

// ---------- Reader ----------
void* reader_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);

    while (1) {
        // wait for our turn
        sem_wait(&reader_turn[id]);

        // read shared memory
        printf("[Reader %d] Read from shared memory: \"%s\"\n", id + 1, shared_memory);

        // signal next reader (if any); otherwise signal writer
        if (id + 1 < n_readers) {
            sem_post(&reader_turn[id + 1]);
        } else {
            // last reader: notify writer
            sem_post(&writer_sem);
        }
    }
    return NULL;
}

int main() {
    printf("Enter number of reader threads (n): ");
    scanf("%d", &n_readers);
    getchar(); // consume leftover newline

    if (n_readers < 1 || n_readers > 100) {
        fprintf(stderr, "n must be between 1 and 100.\n");
        return 1;
    }

    // ---------- initialise semaphores ----------
    sem_init(&writer_sem, 0, 0);
    for (int i = 0; i < n_readers; i++)
        sem_init(&reader_turn[i], 0, 0);

    // ---------- create reader threads ----------
    pthread_t readers[100];
    for (int i = 0; i < n_readers; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&readers[i], NULL, reader_thread, id);
    }

    // ---------- create writer thread ----------
    pthread_t writer;
    pthread_create(&writer, NULL, writer_thread, NULL);

    // ---------- join (runs forever; Ctrl+C to stop) ----------
    pthread_join(writer, NULL);
    for (int i = 0; i < n_readers; i++)
        pthread_join(readers[i], NULL);

    // ---------- cleanup ----------
    sem_destroy(&writer_sem);
    for (int i = 0; i < n_readers; i++)
        sem_destroy(&reader_turn[i]);

    return 0;
}