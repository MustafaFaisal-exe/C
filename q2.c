#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#define SHM_KEY   0x1234
#define SEM_NAME  "/temp_sem"
#define THRESHOLD 37.5f

typedef struct {
    float temperature;
    int   done;          // 1 = sensor finished writing
} SharedData;

int main() {
    // ---------- shared memory setup ----------
    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }

    SharedData* shdata = (SharedData*)shmat(shmid, NULL, 0);
    if (shdata == (void*)-1) { perror("shmat"); exit(1); }

    shdata->temperature = 0.0f;
    shdata->done        = 0;

    // ---------- named semaphore setup ----------
    sem_unlink(SEM_NAME);
    sem_t* sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) { perror("sem_open"); exit(1); }

    srand((unsigned)time(NULL));

    pid_t pid = fork();

    // ========== SENSOR PROCESS ==========
    if (pid == 0) {
        for (int i = 0; i < 10; i++) {
            float temp = 35.0f + ((float)(rand() % 600)) / 100.0f; // 35.0 – 41.0

            sem_wait(sem);              // acquire lock before writing
            shdata->temperature = temp;
            printf("[Sensor]  Writing temperature: %.2f C\n", temp);
            sem_post(sem);             // release lock after writing

            sleep(1);
        }
        sem_wait(sem);
        shdata->done = 1;
        sem_post(sem);

        shmdt(shdata);
        sem_close(sem);
        exit(0);
    }

    // ========== DISPLAY PROCESS ==========
    else {
        while (1) {
            sleep(1);

            sem_wait(sem);             // acquire lock before reading
            float temp = shdata->temperature;
            int   done = shdata->done;
            sem_post(sem);             // release lock after reading

            printf("[Display] Current temperature: %.2f C", temp);
            if (temp > THRESHOLD)
                printf("  *** ALERT: Temperature exceeds %.1f C! ***", THRESHOLD);
            printf("\n");

            if (done) {
                printf("[Display] Sensor finished. Exiting.\n");
                break;
            }
        }

        shmdt(shdata);
        shmctl(shmid, IPC_RMID, NULL);  // remove shared memory
        sem_close(sem);
        sem_unlink(SEM_NAME);
    }

    return 0;
}