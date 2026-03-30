#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define FIFO_PATH "query_gate"

int main(int argc, char *argv[]) {
    int code = 0;

    if (argc >= 2) {
        code = atoi(argv[1]);
    } else {
        printf("enter request code (1 = status, 2 = logs): ");
        scanf("%d", &code);
    }

    if (code != 1 && code != 2) {
        printf("invalid code, use 1 or 2\n");
        return 1;
    }

    printf("connecting to aegis system...\n");

    int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENXIO) {
            printf("main system not ready, waiting...\n");
            fd = open(FIFO_PATH, O_WRONLY);
        }
        if (fd < 0) {
            perror("open fifo");
            printf("is the main system running?\n");
            return 1;
        }
    }

    write(fd, &code, sizeof(int));
    close(fd);
    printf("request sent\n");

    return 0;
}