#ifndef RVM_PLIC_H
#define RVM_PLIC_H

#include "../rvm.h"

#define PLIC_MAX_SRC 64
#define PLIC_NUM_CTX 2
#define PLIC_CTX_M   0
#define PLIC_CTX_S   1

typedef struct plic {
    u32 priority[PLIC_MAX_SRC];
    u32 pending[(PLIC_MAX_SRC + 31) / 32];
    u32 enable[PLIC_NUM_CTX][(PLIC_MAX_SRC + 31) / 32];
    u32 threshold[PLIC_NUM_CTX];
    u32 claimed[PLIC_NUM_CTX];

    u64 n_raise, n_claim;
} plic;

rvm_err plic_init(plic *p);
bool plic_load(void *dev, u64 off, u32 size, u64 *out);
bool plic_store(void *dev, u64 off, u32 size, u64 val);

void plic_raise(plic *p, u32 src);

u64 plic_update(plic *p);

#endif
