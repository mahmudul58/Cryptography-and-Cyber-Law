/* OSI server: receives a frame over a real TCP socket and decapsulates it through 7 layers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "layers.h"
#include "netutil.h"

int main(int argc, char **argv)
{
    int once = 0, port = 8080;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) once = 1;
        else port = atoi(argv[i]);
    }

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (bind(ls, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(ls, 5) < 0) { perror("listen"); return 1; }

    printf("[SERVER] listening on 127.0.0.1:%d\n", port);
    fflush(stdout);

    do {
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) { perror("accept"); break; }

        char frame[BUF_SIZE], msg[512];
        if (recv_frame(fd, frame, sizeof frame) < 0) {
            fprintf(stderr, "[SERVER] failed to read frame\n");
            close(fd);
            continue;
        }

        char ack[1024];
        if (decapsulate(frame, msg, sizeof msg) == 0)
            snprintf(ack, sizeof ack, "ACK - got \"%s\"", msg);
        else
            snprintf(ack, sizeof ack, "NAK - frame rejected");
        send_frame(fd, ack);
        close(fd);
    } while (!once);

    close(ls);
    return 0;
}
