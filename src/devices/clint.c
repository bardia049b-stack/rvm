#include "clint.h"

#include "../cpu/cpu.h"

rvm_err clint_init(clint *c) {
    if (!c)
        return RVM_ERR_BADARG;
    memset(c, 0, sizeof(*c));
    for (u32 i = 0; i < CLINT_NUM_HARTS; i++)
        c->mtimecmp[i] = UINT64_MAX;
    c->base_ns = rvm_now_ns();
    c->free_running = true;
    return RVM_OK;
}

u64 clint_mtime(const clint *c) {
    return c->mtime;
}

void clint_set_virtual_time(clint *c, bool on, u64 base) {
    if (!c)
        return;
    c->virtual_time = on;
    c->insns0 = base;
    c->mtime0 = c->mtime;
    if (on)
        c->free_running = false;
}

void clint_advance(clint *c, u64 insns_now) {
    if (!c || !c->virtual_time || insns_now <= c->insns0)
        return;
    u64 m = c->mtime0 + ((insns_now - c->insns0) * CLINT_TIMEBASE_HZ) / CLINT_VIRTUAL_MIPS;
    if (m > c->mtime)
        c->mtime = m;
}

u64 clint_tick(clint *c) {
    if (c->free_running) {
        u64 elapsed = rvm_now_ns() - c->base_ns;
        c->mtime = elapsed / (1000000000ULL / CLINT_TIMEBASE_HZ);
    }
    u64 bits = 0;
    for (u32 i = 0; i < CLINT_NUM_HARTS; i++) {
        if (c->msip[i] & 1)
            bits |= MIP_MSIP;
        if (c->mtime >= c->mtimecmp[i]) {
            bits |= MIP_STIP | MIP_MTIP;
        }
    }
    return bits;
}

bool clint_load(void *dev, u64 off, u32 size, u64 *out) {
    clint *c = (clint *)dev;
    *out = 0;
    if (off >= CLINT_MTIME_OFF) {
        if (c->free_running)
            clint_tick(c);
        u64 v = c->mtime;
        u32 idx = (u32)((off - CLINT_MTIME_OFF) / 4);
        *out = (size == 8) ? v : (u32)((v >> (32 * (idx & 1))) & 0xFFFFFFFFu);
        return true;
    }
    if (off >= CLINT_MTIMECMP_OFF) {
        u64 rel = off - CLINT_MTIMECMP_OFF;
        u32 hart = (u32)(rel / 8);
        if (hart >= CLINT_NUM_HARTS)
            return true;
        u64 v = c->mtimecmp[hart];
        *out = (size == 8) ? v : (u32)((v >> (32 * ((rel / 4) & 1))) & 0xFFFFFFFFu);
        return true;
    }
    if (size == 4) {
        u32 hart = (u32)(off / 4);
        if (hart < CLINT_NUM_HARTS)
            *out = c->msip[hart];
    }
    return true;
}

bool clint_store(void *dev, u64 off, u32 size, u64 val) {
    clint *c = (clint *)dev;
    if (off >= CLINT_MTIME_OFF) {
        if (size == 8) {
            c->mtime = val;
            c->free_running = false;
        } else {
            u32 idx = (u32)((off - CLINT_MTIME_OFF) / 4);
            u64 m = c->mtime;
            if (idx & 1)
                m = (m & 0x00000000FFFFFFFFULL) | (val << 32);
            else
                m = (m & 0xFFFFFFFF00000000ULL) | (u32)val;
            c->mtime = m;
            c->free_running = false;
        }
        return true;
    }
    if (off >= CLINT_MTIMECMP_OFF) {
        u64 rel = off - CLINT_MTIMECMP_OFF;
        u32 hart = (u32)(rel / 8);
        if (hart >= CLINT_NUM_HARTS)
            return true;
        if (size == 8) {
            c->mtimecmp[hart] = val;
        } else {
            u64 m = c->mtimecmp[hart];
            if ((rel / 4) & 1)
                m = (m & 0x00000000FFFFFFFFULL) | (val << 32);
            else
                m = (m & 0xFFFFFFFF00000000ULL) | (u32)val;
            c->mtimecmp[hart] = m;
        }
        LOG_TRACE("clint: hart%u mtimecmp <- %llu (mtime %llu)", hart,
                  (unsigned long long)c->mtimecmp[hart], (unsigned long long)c->mtime);
        return true;
    }
    if (size == 4) {
        u32 hart = (u32)(off / 4);
        if (hart < CLINT_NUM_HARTS)
            c->msip[hart] = (u32)val & 1;
    }
    return true;
}
