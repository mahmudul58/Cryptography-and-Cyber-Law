#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "layers.h"

/* ---------- small helpers ---------- */

static void log_layer(const char *dir, int no, const char *name,
                      const char *note, const char *data)
{
    printf("[%s] L%d %-12s | %s | %s\n", dir, no, name, note, data);
    fflush(stdout);
}

/* out = TAG{header|payload} */
static int wrap(char *out, size_t cap, const char *tag,
                const char *hdr, const char *payload)
{
    int n = snprintf(out, cap, "%s{%s|%s}", tag, hdr, payload);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* in = TAG{header|payload}  ->  header, payload */
static int unwrap(const char *in, const char *tag,
                  char *hdr, size_t hcap, char *payload, size_t pcap)
{
    size_t tl = strlen(tag), len = strlen(in);
    if (strncmp(in, tag, tl) != 0 || in[tl] != '{') return -1;
    if (len == 0 || in[len - 1] != '}') return -1;

    const char *h = in + tl + 1;
    const char *bar = strchr(h, '|');
    if (!bar) return -1;

    size_t hl = (size_t)(bar - h);
    const char *p = bar + 1;
    size_t pl = (size_t)((in + len - 1) - p);
    if (hl >= hcap || pl >= pcap) return -1;

    memcpy(hdr, h, hl);       hdr[hl] = '\0';
    memcpy(payload, p, pl);   payload[pl] = '\0';
    return 0;
}

static unsigned checksum(const char *s)
{
    unsigned c = 0;
    while (*s) c += (unsigned char)*s++;
    return c & 0xFF;
}

/* Presentation layer: shift every byte by 3, then hex-encode.
 * (A toy "encryption" so the layer really transforms the data.) */
static int pres_encode(const char *in, char *out, size_t cap)
{
    if (strlen(in) * 2 + 1 > cap) return -1;
    for (; *in; in++, out += 2)
        snprintf(out, 3, "%02X", (unsigned char)(*in + 3));
    *out = '\0';
    return 0;
}

static int pres_decode(const char *in, char *out, size_t cap)
{
    size_t n = strlen(in);
    if (n % 2 != 0 || n / 2 >= cap) return -1;
    for (size_t i = 0; i < n / 2; i++) {
        unsigned v;
        if (sscanf(in + 2 * i, "%2x", &v) != 1) return -1;
        out[i] = (char)(v - 3);
    }
    out[n / 2] = '\0';
    return 0;
}

/* ---------- SENDER: encapsulation (L7 -> L1) ---------- */

int encapsulate(const char *msg, char *frame, size_t cap, int src_port, int dst_port)
{
    char app[BUF_SIZE], pres[BUF_SIZE], ses[BUF_SIZE], tcp[BUF_SIZE];
    char ip[BUF_SIZE], eth[BUF_SIZE], hdr[160], enc[BUF_SIZE];

    /* L7 Application: add app header with message length */
    snprintf(hdr, sizeof hdr, "len=%zu", strlen(msg));
    if (wrap(app, sizeof app, "APP", hdr, msg)) return -1;
    log_layer("TX", 7, "Application", "app header added", app);

    /* L6 Presentation: transform (shift + hex) the data */
    if (pres_encode(app, enc, sizeof enc)) return -1;
    if (wrap(pres, sizeof pres, "PRS", "enc=shift3+hex", enc)) return -1;
    log_layer("TX", 6, "Presentation", "data encoded", pres);

    /* L5 Session: add session id */
    if (wrap(ses, sizeof ses, "SES", "id=1001", pres)) return -1;
    log_layer("TX", 5, "Session", "session id added", ses);

    /* L4 Transport: TCP-like header with ports */
    snprintf(hdr, sizeof hdr, "sport=%d,dport=%d,seq=1", src_port, dst_port);
    if (wrap(tcp, sizeof tcp, "TCP", hdr, ses)) return -1;
    log_layer("TX", 4, "Transport", "TCP header added", tcp);

    /* L3 Network: IP header with addresses */
    if (wrap(ip, sizeof ip, "IP", "src=127.0.0.1,dst=127.0.0.1,ttl=64", tcp)) return -1;
    log_layer("TX", 3, "Network", "IP header added", ip);

    /* L2 Data Link: Ethernet-like frame + checksum */
    if (wrap(eth, sizeof eth, "ETH",
             "dst=aa:bb:cc:dd:ee:02,src=aa:bb:cc:dd:ee:01", ip)) return -1;
    char tail[8];
    snprintf(tail, sizeof tail, "#%02X", checksum(eth));
    if (strlen(eth) + strlen(tail) + 1 > sizeof eth) return -1;
    strcat(eth, tail);
    log_layer("TX", 2, "DataLink", "frame + checksum added", eth);

    /* L1 Physical: bytes -> bits, ready for the wire */
    if (strlen(eth) + 1 > cap) return -1;
    strcpy(frame, eth);
    char bits[64] = "";
    for (int i = 0; i < 3 && frame[i]; i++) {
        for (int b = 7; b >= 0; b--)
            strcat(bits, ((unsigned char)frame[i] >> b) & 1 ? "1" : "0");
        strcat(bits, " ");
    }
    char note[64];
    snprintf(note, sizeof note, "%zu bytes = %zu bits", strlen(frame), strlen(frame) * 8);
    strcat(bits, "...");
    log_layer("TX", 1, "Physical", note, bits);
    return 0;
}

/* ---------- RECEIVER: decapsulation (L1 -> L7) ---------- */

static int fail(int no, const char *name, const char *why)
{
    printf("[RX] L%d %-12s | ERROR | %s\n", no, name, why);
    fflush(stdout);
    return -1;
}

int decapsulate(const char *frame, char *msg, size_t cap)
{
    char a[BUF_SIZE], b[BUF_SIZE], hdr[160], eth[BUF_SIZE];

    /* L1 Physical */
    char note[64];
    snprintf(note, sizeof note, "%zu bytes received", strlen(frame));
    log_layer("RX", 1, "Physical", note, frame);

    /* L2 Data Link: verify checksum, strip Ethernet header */
    const char *hash = strrchr(frame, '#');
    if (!hash) return fail(2, "DataLink", "checksum missing");
    size_t body = (size_t)(hash - frame);
    if (body >= sizeof eth) return fail(2, "DataLink", "frame too large");
    memcpy(eth, frame, body); eth[body] = '\0';
    unsigned got = 0;
    if (sscanf(hash + 1, "%2x", &got) != 1 || got != checksum(eth))
        return fail(2, "DataLink", "checksum mismatch");
    if (unwrap(eth, "ETH", hdr, sizeof hdr, a, sizeof a))
        return fail(2, "DataLink", "bad ETH frame");
    log_layer("RX", 2, "DataLink", "checksum OK, ETH removed", a);

    /* L3 Network */
    if (unwrap(a, "IP", hdr, sizeof hdr, b, sizeof b))
        return fail(3, "Network", "bad IP header");
    log_layer("RX", 3, "Network", "IP header removed", b);

    /* L4 Transport */
    if (unwrap(b, "TCP", hdr, sizeof hdr, a, sizeof a))
        return fail(4, "Transport", "bad TCP header");
    log_layer("RX", 4, "Transport", "TCP header removed", a);

    /* L5 Session */
    if (unwrap(a, "SES", hdr, sizeof hdr, b, sizeof b))
        return fail(5, "Session", "bad session header");
    log_layer("RX", 5, "Session", "session id removed", b);

    /* L6 Presentation: remove header, decode data */
    if (unwrap(b, "PRS", hdr, sizeof hdr, a, sizeof a))
        return fail(6, "Presentation", "bad presentation header");
    if (pres_decode(a, b, sizeof b))
        return fail(6, "Presentation", "decode failed");
    log_layer("RX", 6, "Presentation", "data decoded", b);

    /* L7 Application */
    if (unwrap(b, "APP", hdr, sizeof hdr, a, sizeof a))
        return fail(7, "Application", "bad app header");
    if (strlen(a) >= cap) return fail(7, "Application", "message too long");
    strcpy(msg, a);
    log_layer("RX", 7, "Application", "message delivered", msg);
    return 0;
}
