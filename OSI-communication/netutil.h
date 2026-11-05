#ifndef NETUTIL_H
#define NETUTIL_H

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* TCP is a byte stream, so every frame is sent with a 4-byte length prefix. */

static inline int send_all(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    while (n) {
        ssize_t w = send(fd, p, n, 0);
        if (w <= 0) return -1;
        p += w; n -= (size_t)w;
    }
    return 0;
}

static inline int recv_all(int fd, void *buf, size_t n)
{
    char *p = buf;
    while (n) {
        ssize_t r = recv(fd, p, n, 0);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static inline int send_frame(int fd, const char *data)
{
    uint32_t len = htonl((uint32_t)strlen(data));
    if (send_all(fd, &len, 4)) return -1;
    return send_all(fd, data, strlen(data));
}

static inline int recv_frame(int fd, char *out, size_t cap)
{
    uint32_t len;
    if (recv_all(fd, &len, 4)) return -1;
    len = ntohl(len);
    if (len >= cap) return -1;
    if (recv_all(fd, out, len)) return -1;
    out[len] = '\0';
    return (int)len;
}

#endif
