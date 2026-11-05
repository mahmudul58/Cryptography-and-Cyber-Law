#ifndef LAYERS_H
#define LAYERS_H

#include <stddef.h>

#define BUF_SIZE 4096
#define MAX_MSG  256

/* Sender side: Application (L7) -> Physical (L1).
 * Builds the final frame in `frame`. Returns 0 on success, -1 on error.
 * Every layer prints one "[TX] L<n> <name> | <note> | <data>" line. */
int encapsulate(const char *msg, char *frame, size_t cap, int src_port, int dst_port);

/* Receiver side: Physical (L1) -> Application (L7).
 * Writes the recovered message into `msg`. Returns 0 on success, -1 on error.
 * Every layer prints one "[RX] L<n> <name> | <note> | <data>" line. */
int decapsulate(const char *frame, char *msg, size_t cap);

#endif
