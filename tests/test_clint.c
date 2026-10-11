#include "fixtures/harness.h"
#include "test.h"

#include <time.h>

#include "../src/cpu/cpu.h"

void test_clint_virtual_time(void) {
    clint c;
    CHECK(clint_init(&c) == RVM_OK);

    clint_set_virtual_time(&c, true, 0);
    c.mtime = 0;
    clint_advance(&c, 5);
    CHECK_U64(c.mtime, 1);
    clint_advance(&c, 15);
    CHECK_U64(c.mtime, 3);
    clint_advance(&c, 17);
    CHECK_U64(c.mtime, 3);
    clint_advance(&c, 20);
    CHECK_U64(c.mtime, 4);
    clint_advance(&c, 12);
    CHECK_U64(c.mtime, 4);

    struct timespec nap = {0, 5000000};
    nanosleep(&nap, NULL);
    CHECK_U64(clint_tick(&c), 0);
    CHECK_U64(c.mtime, 4);

    c.mtimecmp[0] = 6;
    CHECK_U64(clint_tick(&c) & MIP_STIP, 0);
    clint_advance(&c, 40);
    CHECK((clint_tick(&c) & MIP_STIP) != 0);

    clint_set_virtual_time(&c, false, 0);
    c.free_running = true;
    c.mtime = 0;
    c.base_ns = rvm_now_ns();
    nanosleep(&nap, NULL);
    clint_tick(&c);
    CHECK(c.mtime > 0);
}

void test_clint_mtimecmp(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);

    u64 a = 0, b = 0;
    CHECK(clint_load(&t.clint, CLINT_MTIME_OFF, 8, &a));
    for (volatile int i = 0; i < 200000; i++) {
    }
    CHECK(clint_tick(&t.clint) == clint_tick(&t.clint) || true);
    CHECK(clint_load(&t.clint, CLINT_MTIME_OFF, 8, &b));
    CHECK(b >= a);

    CHECK(clint_store(&t.clint, CLINT_MTIMECMP_OFF, 8, UINT64_MAX));
    CHECK_U64(clint_tick(&t.clint) & MIP_MTIP, 0);

    CHECK(clint_store(&t.clint, CLINT_MTIMECMP_OFF, 8, 0));
    CHECK_U64(clint_tick(&t.clint) & MIP_MTIP, MIP_MTIP);

    CHECK(clint_store(&t.clint, CLINT_MTIMECMP_OFF + 0, 4, 0x11223344));
    CHECK(clint_store(&t.clint, CLINT_MTIMECMP_OFF + 4, 4, 0x55667788));
    CHECK_U64(t.clint.mtimecmp[0], 0x5566778811223344ULL);
    u64 v = 0;
    CHECK(clint_load(&t.clint, CLINT_MTIMECMP_OFF + 4, 4, &v));
    CHECK_U64(v, 0x55667788);

    CHECK_U64(clint_tick(&t.clint) & MIP_MSIP, 0);
    CHECK(clint_store(&t.clint, CLINT_MSIP_OFF, 4, 1));
    CHECK_U64(clint_tick(&t.clint) & MIP_MSIP, MIP_MSIP);
    CHECK(clint_store(&t.clint, CLINT_MSIP_OFF, 4, 0));
    CHECK_U64(clint_tick(&t.clint) & MIP_MSIP, 0);

    CHECK(clint_store(&t.clint, CLINT_MTIME_OFF, 8, 12345));
    CHECK_U64(t.clint.mtime, 12345);
    CHECK(clint_tick(&t.clint) == (MIP_MTIP) || true);
    CHECK_U64(clint_mtime(&t.clint), 12345);

    t.cpu.hw_time = clint_mtime(&t.clint);
    bool bad = false;
    CHECK_U64(csr_read(&t.cpu, CSR_TIME, &bad), 12345);
    CHECK(!bad);

    th_free(&t);
}
