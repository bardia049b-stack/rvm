#ifndef RVM_CLINT_H
#define RVM_CLINT_H

#include "../rvm.h"

#define CLINT_TIMEBASE_HZ 10000000ULL
#define CLINT_NUM_HARTS   1

#define CLINT_MSIP_OFF     0x0000ULL
#define CLINT_MTIMECMP_OFF 0x4000ULL
#define CLINT_MTIME_OFF    0xBFF8ULL

typedef struct clint {
    u32 msip[CLINT_NUM_HARTS];
    u64 mtimecmp[CLINT_NUM_HARTS];
    u64 mtime;
    u64 base_ns;
    bool free_running;
    bool virtual_time;
    u64 insns0;
    u64 mtime0;
} clint;

rvm_err clint_init(clint *c);
bool clint_load(void *dev, u64 off, u32 size, u64 *out);
bool clint_store(void *dev, u64 off, u32 size, u64 val);

u64 clint_tick(clint *c);

void clint_set_virtual_time(clint *c, bool on, u64 base);

void clint_advance(clint *c, u64 insns_now);

#define CLINT_VIRTUAL_MIPS 50000000ULL

u64 clint_mtime(const clint *c);

#endif
