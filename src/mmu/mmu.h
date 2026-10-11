#ifndef RVM_MMU_H
#define RVM_MMU_H

#include "../rvm.h"

#define MMU_TLB_BITS  6
#define MMU_TLB_SIZE  (1u << MMU_TLB_BITS)
#define MMU_PAGE_SIZE 4096ULL

struct bus;

typedef struct tlb_entry {
    u64 va_page;
    u64 pa_page;
    u64 page_len;
    u32 perm;
    u32 mode;
    u32 asid;
    u64 tag;
    bool valid;
} tlb_entry;

typedef struct mmu {
    struct bus *bus;
    tlb_entry tlb[MMU_TLB_SIZE];
    u64 tag;

    u64 n_walk, n_tlb_hit, n_tlb_miss, n_fault;
} mmu;

typedef struct mmu_xlat {
    bool ok;
    u64 pa;
    u32 cause;
} mmu_xlat;

rvm_err mmu_init(mmu *m, struct bus *b);
void mmu_free(mmu *m);

void mmu_flush(mmu *m);
void mmu_flush_page(mmu *m, u64 va);

mmu_xlat mmu_translate(mmu *m, u64 va, u32 eff_priv, u64 satp, bool mxr, bool sum, u32 acc);

#endif
