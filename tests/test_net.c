#include "test.h"
#include "../src/devices/net_user.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define GUEST_IP    0x0a00020fu
#define GUEST_PORT  40000u
#define TOTAL       120000u
#define REFUSE_EVERY 40u
#define REFUSE_AT     7u
#define LOSE_EVERY  40u

static u8 g_pattern[TOTAL];
static u8 g_got[TOTAL];

typedef struct {
    net_user *n;
    int acc;
    u16 rport;
    u32 gseq;
    u32 first;
    u32 payload;
    u32 fills;
    u32 refuses;
    u32 lost_seg;
    u32 refused;
    u32 dropped;
    bool started;
} fake_guest;

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static u16 csum16(const u8 *b, int n) {
    u32 s = 0;
    while (n > 1) {
        s += ((u32)b[0] << 8) | b[1];
        b += 2;
        n -= 2;
    }
    if (n)
        s += (u32)b[0] << 8;
    while (s >> 16)
        s = (s & 0xffff) + (s >> 16);
    return (u16)~s;
}

static void put16(u8 *p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}
static void put32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}
static u32 get32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void guest_send(fake_guest *g, u8 flags, u32 ack) {
    u8 ip[20];
    u8 tcp[20];
    memset(ip, 0, sizeof(ip));
    memset(tcp, 0, sizeof(tcp));
    u16 tot = (u16)(sizeof(ip) + sizeof(tcp));
    ip[0] = 0x45;
    put16(ip + 2, tot);
    ip[8] = 64;
    ip[9] = 6;
    put32(ip + 12, GUEST_IP);
    put32(ip + 16, 0x7f000001u);
    put16(tcp, GUEST_PORT);
    put16(tcp + 2, g->rport);
    put32(tcp + 4, 1);
    put32(tcp + 8, ack);
    tcp[12] = 0x50;
    tcp[13] = flags;
    put16(tcp + 14, 0xffff);
    put16(tcp + 16, csum16((const u8 *)tcp, 20));
    u8 eth[14];
    memset(eth, 0, sizeof(eth));
    put16(eth + 12, 0x0800);
    u8 frame[54];
    memcpy(frame, eth, 14);
    memcpy(frame + 14, ip, 20);
    memcpy(frame + 34, tcp, 20);
    net_user_input(g->n, frame, sizeof(frame));
}

static bool guest_output(void *ud, const u8 *frame, u32 len) {
    fake_guest *g = (fake_guest *)ud;
    g->fills++;
    if (g->fills % REFUSE_EVERY == REFUSE_AT) {
        g->refused++;
        return false;
    }
    if (len < 34)
        return true;
    const u8 *ip = frame + 14;
    u32 ihl = (u32)((ip[0] & 0x0f) * 4);
    u32 tot = (u32)((ip[2] << 8) | ip[3]);
    if (ihl < 20 || tot < ihl + 20 || 14u + tot > len)
        return true;
    const u8 *tcp = ip + ihl;
    u32 doff = (u32)((tcp[12] >> 4) * 4);
    u32 dlen = tot - ihl - doff;
    u32 seq = get32(tcp + 4);
    u8 flags = tcp[13];

    if ((flags & 0x12) == 0x12 && dlen == 0) {
        g->gseq = g->first = seq + 1;
        g->started = true;
        return true;
    }
    if (dlen) {
        if (seq != g->gseq)
            return true;
        g->dropped++;
        if ((g->lost_seg++ % LOSE_EVERY) == (LOSE_EVERY - 1))
            return true;
        u32 off = seq - g->first;
        if (off < TOTAL) {
            if (off + dlen > TOTAL)
                dlen = TOTAL - off;
            memcpy(g_got + off, tcp + doff, dlen);
            if (off + dlen > g->payload)
                g->payload = off + dlen;
        }
        g->gseq = seq + dlen;
    }
    guest_send(g, 0x10, g->gseq);
    return true;
}

void test_net_user_bulk(void) {
    test_begin("net_user_bulk");
    for (u32 i = 0; i < TOTAL; i++)
        g_pattern[i] = (u8)(i * 31u + 7u);

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(ls >= 0);
    int on = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port = 0;
    CHECK(bind(ls, (struct sockaddr *)&sa, sizeof(sa)) == 0);
    socklen_t sl = sizeof(sa);
    CHECK(getsockname(ls, (struct sockaddr *)&sa, &sl) == 0);
    CHECK(listen(ls, 8) == 0);
    fcntl(ls, F_SETFL, fcntl(ls, F_GETFL, 0) | O_NONBLOCK);

    fake_guest g;
    memset(&g, 0, sizeof(g));
    g.acc = -1;
    g.rport = ntohs(sa.sin_port);
    CHECK(net_user_init(&g.n, guest_output, &g) == RVM_OK);

    guest_send(&g, 0x02, 0);
    for (int i = 0; i < 50 && g.acc < 0; i++) {
        int a = accept(ls, NULL, NULL);
        if (a >= 0) {
            fcntl(a, F_SETFL, fcntl(a, F_GETFL, 0) | O_NONBLOCK);
            g.acc = a;
        } else
            usleep(2000);
    }
    CHECK(g.acc >= 0);
    CHECK(g.started);

    u32 sent = 0;
    u32 prev = 0;
    double t_start = now_s(), t_prog = t_start;
    while (g.payload < TOTAL && now_s() - t_start < 25.0) {
        while (sent < TOTAL) {
            ssize_t w = send(g.acc, g_pattern + sent, TOTAL - sent, MSG_DONTWAIT);
            if (w > 0) {
                sent += (u32)w;
                continue;
            }
            break;
        }
        net_user_poll(g.n);
        if (g.payload != prev) {
            prev = g.payload;
            t_prog = now_s();
            continue;
        }
        if (now_s() - t_prog > 0.02)
            usleep(20000);
    }

    CHECK_U64(g.payload, TOTAL);
    CHECK(memcmp(g_got, g_pattern, TOTAL) == 0);
    CHECK(g.refused > 0);
    CHECK(g.lost_seg / LOSE_EVERY > 0);
    CHECK(g.refused >= 3);

    close(g.acc);
    close(ls);
    net_user_free(g.n);
    test_end();
}
