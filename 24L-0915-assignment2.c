#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define MATRIX_SIZE 10
#define FIFO_PATH "query_gate"
#define VAULT_PATH "vaultDB"
#define CAESAR_SHIFT 5
#define CRITICAL 90
#define READINGS 50

int matrix[MATRIX_SIZE][MATRIX_SIZE];

typedef struct {
    int timestamp;
    int data_size;
} Header;

typedef struct {
    int thread_id;
    int row_start;
    int row_end;
    int col_start;
    int col_end;
} ThreadArgs;

void run_sensor(int write_fd, int sensor_id) {
    srand(time(NULL) ^ getpid() ^ sensor_id);
    for (int i = 0; i < READINGS; i++) {
        int val = rand() % 101;
        printf("sensor %d (pid %d) generated: %d\n", sensor_id, getpid(), val);
        write(write_fd, &val, sizeof(int));
        sleep(2);
    }
    close(write_fd);
    exit(0);
}

void run_child(int fd0, int fd1, int out_fd, int id) {
    for (int i = 0; i < READINGS; i++) {
        int a, b;
        if (read(fd0, &a, sizeof(int)) <= 0) break;
        if (read(fd1, &b, sizeof(int)) <= 0) break;
        int avg = (a + b) / 2;
        printf("child %d averaged: %d\n", id, avg);
        write(out_fd, &avg, sizeof(int));
    }
    close(fd0);
    close(fd1);
    close(out_fd);
    exit(0);
}

void *process_quadrant(void *arg) {
    ThreadArgs *t = (ThreadArgs *)arg;

    for (int r = t->row_start; r < t->row_end; r++) {
        for (int c = t->col_start; c < t->col_end; c++) {
            if (matrix[r][c] > CRITICAL) {
                int sum = 0, count = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = r + dr;
                        int nc = c + dc;
                        if (nr >= t->row_start && nr < t->row_end &&
                            nc >= t->col_start && nc < t->col_end) {
                            sum += matrix[nr][nc];
                            count++;
                        }
                    }
                }
                int old = matrix[r][c];
                matrix[r][c] = (count > 0) ? sum / count : matrix[r][c];
                printf("thread %d: peak %d at [%d][%d] replaced with %d\n",
                       t->thread_id, old, r, c, matrix[r][c]);
            }
        }
    }
    return NULL;
}

void process_b(int in_fd, int out_fd) {
    int data[MATRIX_SIZE * MATRIX_SIZE];
    int n = 0;
    ssize_t bytes;

    while (n < MATRIX_SIZE * MATRIX_SIZE) {
        bytes = read(in_fd, (char *)data + n * sizeof(int),
                     (MATRIX_SIZE * MATRIX_SIZE - n) * sizeof(int));
        if (bytes <= 0) break;
        n += bytes / sizeof(int);
    }
    close(in_fd);

    printf("process B: applying caesar shift to %d values\n", n);
    for (int i = 0; i < n; i++)
        data[i] = (data[i] + CAESAR_SHIFT) % 101;

    write(out_fd, data, n * sizeof(int));
    close(out_fd);
    exit(0);
}

void process_c(int in_fd) {
    int data[MATRIX_SIZE * MATRIX_SIZE];
    int n = 0;
    ssize_t bytes;

    while (n < MATRIX_SIZE * MATRIX_SIZE) {
        bytes = read(in_fd, (char *)data + n * sizeof(int),
                     (MATRIX_SIZE * MATRIX_SIZE - n) * sizeof(int));
        if (bytes <= 0) break;
        n += bytes / sizeof(int);
    }
    close(in_fd);

    Header hdr;
    hdr.timestamp = (int)time(NULL);
    hdr.data_size = n;

    int fd = open(VAULT_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open vaultDB");
        exit(1);
    }

    write(fd, &hdr, sizeof(Header));
    write(fd, data, n * sizeof(int));
    close(fd);
    printf("process C: wrote %d values to vaultDB\n", n);
    exit(0);
}

void print_matrix() {
    printf("\n--- matrix ---\n");
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++)
            printf("%3d ", matrix[i][j]);
        printf("\n");
    }
    printf("\n");
}

void dashboard_loop() {
    if (access(FIFO_PATH, F_OK) == 0)
        unlink(FIFO_PATH);

    if (mkfifo(FIFO_PATH, 0644) < 0) {
        perror("mkfifo");
        exit(1);
    }

    printf("waiting for dashboard on %s ...\n", FIFO_PATH);

    while (1) {
        int fifo_fd = open(FIFO_PATH, O_RDONLY);
        if (fifo_fd < 0) {
            if (errno == EINTR) continue;
            perror("open fifo");
            break;
        }

        int code = 0;
        ssize_t n = read(fifo_fd, &code, sizeof(int));
        close(fifo_fd);

        if (n <= 0) {
            printf("dashboard disconnected, waiting again...\n");
            continue;
        }

        printf("got request code: %d\n", code);

        pid_t pid = fork();
        if (pid == 0) {
            char code_str[16];
            snprintf(code_str, sizeof(code_str), "%d", code);
            char *args[] = { "./formatter", VAULT_PATH, code_str, NULL };
            execv("./formatter", args);
            perror("execv");
            exit(1);
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
        } else {
            perror("fork");
        }
    }
}

int main() {
    srand(time(NULL));

    int gc_pipe[4][2];
    int c_pipe[2][2];

    for (int i = 0; i < 4; i++) {
        if (pipe(gc_pipe[i]) < 0) { perror("pipe"); exit(1); }
    }
    for (int i = 0; i < 2; i++) {
        if (pipe(c_pipe[i]) < 0) { perror("pipe"); exit(1); }
    }

    pid_t child0 = fork();
    if (child0 < 0) { perror("fork"); exit(1); }

    if (child0 == 0) {
        close(c_pipe[1][0]); close(c_pipe[1][1]);
        close(c_pipe[0][0]);
        close(gc_pipe[2][0]); close(gc_pipe[2][1]);
        close(gc_pipe[3][0]); close(gc_pipe[3][1]);

        pid_t gc0 = fork();
        if (gc0 == 0) {
            close(gc_pipe[0][0]);
            close(gc_pipe[1][0]); close(gc_pipe[1][1]);
            close(c_pipe[0][1]);
            run_sensor(gc_pipe[0][1], 0);
        }
        close(gc_pipe[0][1]);

        pid_t gc1 = fork();
        if (gc1 == 0) {
            close(gc_pipe[1][0]);
            close(gc_pipe[0][0]);
            close(c_pipe[0][1]);
            run_sensor(gc_pipe[1][1], 1);
        }
        close(gc_pipe[1][1]);

        run_child(gc_pipe[0][0], gc_pipe[1][0], c_pipe[0][1], 0);
    }

    close(gc_pipe[0][0]); close(gc_pipe[0][1]);
    close(gc_pipe[1][0]); close(gc_pipe[1][1]);
    close(c_pipe[0][1]);

    pid_t child1 = fork();
    if (child1 < 0) { perror("fork"); exit(1); }

    if (child1 == 0) {
        close(c_pipe[0][0]); close(c_pipe[0][1]);
        close(c_pipe[1][0]);

        pid_t gc2 = fork();
        if (gc2 == 0) {
            close(gc_pipe[2][0]);
            close(gc_pipe[3][0]); close(gc_pipe[3][1]);
            close(c_pipe[1][1]);
            run_sensor(gc_pipe[2][1], 2);
        }
        close(gc_pipe[2][1]);

        pid_t gc3 = fork();
        if (gc3 == 0) {
            close(gc_pipe[3][0]);
            close(gc_pipe[2][0]);
            close(c_pipe[1][1]);
            run_sensor(gc_pipe[3][1], 3);
        }
        close(gc_pipe[3][1]);

        run_child(gc_pipe[2][0], gc_pipe[3][0], c_pipe[1][1], 1);
    }

    close(gc_pipe[2][0]); close(gc_pipe[2][1]);
    close(gc_pipe[3][0]); close(gc_pipe[3][1]);
    close(c_pipe[1][1]);

    int idx = 0;
    int got[2] = {0, 0};

    while (idx < MATRIX_SIZE * MATRIX_SIZE) {
        for (int ch = 0; ch < 2 && idx < MATRIX_SIZE * MATRIX_SIZE; ch++) {
            if (got[ch] >= READINGS) continue;
            int val;
            if (read(c_pipe[ch][0], &val, sizeof(int)) <= 0) continue;
            matrix[idx / MATRIX_SIZE][idx % MATRIX_SIZE] = val;
            printf("parent got value %d at [%d][%d]\n", val,
                   idx / MATRIX_SIZE, idx % MATRIX_SIZE);
            idx++;
            got[ch]++;
        }
    }

    close(c_pipe[0][0]);
    close(c_pipe[1][0]);

    waitpid(child0, NULL, 0);
    waitpid(child1, NULL, 0);

    print_matrix();

    pthread_t threads[4];
    ThreadArgs targs[4] = {
        {0,  0, 5,  0, 5},
        {1,  0, 5,  5, 10},
        {2,  5, 10, 0, 5},
        {3,  5, 10, 5, 10}
    };

    for (int i = 0; i < 4; i++)
        pthread_create(&threads[i], NULL, process_quadrant, &targs[i]);

    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);

    printf("after stabilization:\n");
    print_matrix();

    int pipe_ab[2], pipe_bc[2];
    pipe(pipe_ab);
    pipe(pipe_bc);

    pid_t proc_c = fork();
    if (proc_c == 0) {
        close(pipe_ab[0]); close(pipe_ab[1]);
        close(pipe_bc[1]);
        process_c(pipe_bc[0]);
    }
    close(pipe_bc[0]);

    pid_t proc_b = fork();
    if (proc_b == 0) {
        close(pipe_ab[1]);
        process_b(pipe_ab[0], pipe_bc[1]);
    }
    close(pipe_ab[0]);
    close(pipe_bc[1]);

    int flat[MATRIX_SIZE * MATRIX_SIZE];
    for (int i = 0; i < MATRIX_SIZE; i++)
        for (int j = 0; j < MATRIX_SIZE; j++)
            flat[i * MATRIX_SIZE + j] = matrix[i][j];

    write(pipe_ab[1], flat, sizeof(flat));
    close(pipe_ab[1]);

    waitpid(proc_b, NULL, 0);
    waitpid(proc_c, NULL, 0);

    printf("vault done\n");

    dashboard_loop();

    unlink(FIFO_PATH);
    return 0;
}