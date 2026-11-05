/* OSI client: encapsulates a message through 7 layers, sends it over a real TCP socket. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include "layers.h"
#include "netutil.h"

int main(int argc, char **argv)
{
    const char *msg = argc > 1 ? argv[1] : "Hello OSI Model";
    int port = argc > 2 ? atoi(argv[2]) : 8080;

    if (strlen(msg) == 0 || strlen(msg) > MAX_MSG) {
        fprintf(stderr, "Message must be 1-%d characters.\n", MAX_MSG);
        return 1;
    }
    for (const char *c = msg; *c; c++)
        if ((unsigned char)*c < 32 || (unsigned char)*c > 126) {
            fprintf(stderr, "Use plain printable ASCII characters only.\n");
            return 1;
        }

    char frame[BUF_SIZE];
    printf("[CLIENT] message: %s\n", msg);
    if (encapsulate(msg, frame, sizeof frame, 50000, port)) {
        fprintf(stderr, "Encapsulation failed.\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect (is the server running?)");
        close(fd);
        return 1;
    }

    printf("[WIRE] client -> server : %zu bytes over TCP 127.0.0.1:%d\n", strlen(frame), port);
    fflush(stdout);
    if (send_frame(fd, frame)) { perror("send"); close(fd); return 1; }

    char ack[BUF_SIZE];
    if (recv_frame(fd, ack, sizeof ack) >= 0)
        printf("[CLIENT] server replied: %s\n", ack);

    close(fd);
    return 0;
}
