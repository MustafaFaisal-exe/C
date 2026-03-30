#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define MATRIX_SIZE 10
#define CAESAR_SHIFT 5

typedef struct {
    int timestamp;
    int data_size;
} Header;

int decode(int val) {
    int orig = val - CAESAR_SHIFT;
    if (orig < 0) orig += 101;
    return orig;
}

void show_status(Header *hdr, int *data, int n) {
    time_t ts = (time_t)hdr->timestamp;
    printf("timestamp: %s", ctime(&ts));
    printf("data size: %d\n\n", hdr->data_size);

    printf("full reactor matrix (decrypted):\n");
    for (int i = 0; i < MATRIX_SIZE && i * MATRIX_SIZE < n; i++) {
        for (int j = 0; j < MATRIX_SIZE && i * MATRIX_SIZE + j < n; j++) {
            int val = decode(data[i * MATRIX_SIZE + j]);
            printf("%3d ", val);
        }
        printf("\n");
    }

    int sum = 0, peaks = 0;
    for (int i = 0; i < n; i++) {
        int v = decode(data[i]);
        sum += v;
        if (v > 90) peaks++;
    }
    printf("\naverage: %d\n", n > 0 ? sum / n : 0);
    printf("critical peaks: %d\n", peaks);
}

void show_logs(Header *hdr, int *data, int n) {
    time_t ts = (time_t)hdr->timestamp;
    printf("timestamp: %s", ctime(&ts));
    printf("total records: %d\n\n", hdr->data_size);

    int show = n < 10 ? n : 10;
    printf("first %d entries:\n", show);
    printf("index  encrypted  decrypted\n");
    for (int i = 0; i < show; i++) {
        printf("  %3d      %3d        %3d\n", i, data[i], decode(data[i]));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: ./formatter <vaultDB> <request_code>\n");
        return 1;
    }

    const char *path = argv[1];
    int code = atoi(argv[2]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open vaultDB");
        return 1;
    }

    Header hdr;
    if (read(fd, &hdr, sizeof(Header)) != sizeof(Header)) {
        printf("failed to read header\n");
        close(fd);
        return 1;
    }

    int *data = malloc(hdr.data_size * sizeof(int));
    if (!data) {
        perror("malloc");
        close(fd);
        return 1;
    }

    int n = 0;
    ssize_t bytes;
    while (n < hdr.data_size) {
        bytes = read(fd, (char *)data + n * sizeof(int),
                     (hdr.data_size - n) * sizeof(int));
        if (bytes <= 0) break;
        n += bytes / sizeof(int);
    }
    close(fd);

    if (code == 1)
        show_status(&hdr, data, n);
    else if (code == 2)
        show_logs(&hdr, data, n);
    else
        printf("unknown code %d\n", code);

    free(data);
    return 0;
}