#ifndef RVM_CPU_H
#define RVM_CPU_H

#include "../bus/bus.h"
#include "../mmu/mmu.h"
#include "../rvm.h"

#define CSR_CYCLE   0xC00
#define CSR_TIME    0xC01
#define CSR_INSTRET 0xC02
#define CSR_FFLAGS  0x001
#define CSR_FRM     0x002
#define CSR_FCSR    0x003

#define CSR_SSTATUS    0x100
#define CSR_SIE        0x104
#define CSR_STVEC      0x105
#define CSR_SCOUNTEREN 0x106
#define CSR_SENVCFG    0x10A
#define CSR_SSCRATCH   0x140
#define CSR_SEPC       0x141
#define CSR_SCAUSE     0x142
#define CSR_STVAL      0x143
#define CSR_SIP        0x144
#define CSR_STIMECMP   0x14D
#define CSR_SATP       0x180

#define CSR_MVENDORID   0xF11
#define CSR_MARCHID     0xF12
#define CSR_MIMPID      0xF13
#define CSR_MHARTID     0xF14
#define CSR_MSTATUS     0x300
#define CSR_MISA        0x301
#define CSR_MEDELEG     0x302
#define CSR_MIDELEG     0x303
#define CSR_MIE         0x304
#define CSR_MTVEC       0x305
#define CSR_MCOUNTEREN  0x306
#define CSR_MENVCFG     0x30A
#define CSR_MSTATUSH    0x310
#define CSR_MSCRATCH    0x340
#define CSR_MEPC        0x341
#define CSR_MCAUSE      0x342
#define CSR_MTVAL       0x343
#define CSR_MIP         0x344
#define CSR_MTINST      0x34A
#define CSR_MTVAL2      0x34B
#define CSR_PMPCFG0     0x3A0
#define CSR_PMPADDR0    0x3B0
#define CSR_MCYCLE      0xB00
#define CSR_MINSTRET    0xB02
#define CSR_MCYCLECFG   0x321
#define CSR_MINSTRETCFG 0x322

#define MSTATUS_UIE       (1ULL << 0)
#define MSTATUS_SIE       (1ULL << 1)
#define MSTATUS_MIE       (1ULL << 3)
#define MSTATUS_UPIE      (1ULL << 4)
#define MSTATUS_SPIE      (1ULL << 5)
#define MSTATUS_UBE       (1ULL << 6)
#define MSTATUS_MPIE      (1ULL << 7)
#define MSTATUS_SPP       (1ULL << 8)
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_MPP       (3ULL << 11)
#define MSTATUS_FS_SHIFT  13
#define MSTATUS_FS        (3ULL << 13)
#define MSTATUS_XS_SHIFT  15
#define MSTATUS_XS        (3ULL << 15)
#define MSTATUS_MPRV      (1ULL << 17)
#define MSTATUS_SUM       (1ULL << 18)
#define MSTATUS_MXR       (1ULL << 19)
#define MSTATUS_TVM       (1ULL << 20)
#define MSTATUS_TW        (1ULL << 21)
#define MSTATUS_TSR       (1ULL << 22)
#define MSTATUS_SD        (1ULL << 63)

#define SSTATUS_MASK                                                                               \
    (MSTATUS_UIE | MSTATUS_SIE | MSTATUS_UPIE | MSTATUS_SPIE | MSTATUS_UBE | MSTATUS_SPP |         \
     MSTATUS_FS | MSTATUS_XS | MSTATUS_SUM | MSTATUS_MXR | MSTATUS_SD)

#define IRQ_S_SOFT  1
#define IRQ_M_SOFT  3
#define IRQ_S_TIMER 5
#define IRQ_M_TIMER 7
#define IRQ_S_EXT   9
#define IRQ_M_EXT   11

#define MIP_SSIP (1ULL << IRQ_S_SOFT)
#define MIP_MSIP (1ULL << IRQ_M_SOFT)
#define MIP_STIP (1ULL << IRQ_S_TIMER)
#define MIP_MTIP (1ULL << IRQ_M_TIMER)
#define MIP_SEIP (1ULL << IRQ_S_EXT)
#define MIP_MEIP (1ULL << IRQ_M_EXT)

#define RVM_MEDELEG_DEFAULT                                                                        \
    ((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) | (1ULL << 4) | (1ULL << 5) |           \
     (1ULL << 6) | (1ULL << 7) | (1ULL << 8) | (1ULL << 12) | (1ULL << 13) | (1ULL << 14) |        \
     (1ULL << 15) | (1ULL << 18) | (1ULL << 19) | (1ULL << 20) | (1ULL << 21))
#define RVM_MIDELEG_DEFAULT (MIP_SSIP | MIP_STIP | MIP_SEIP)

#define EXC_INST_MISALIGNED  0
#define EXC_INST_FAULT       1
#define EXC_ILLEGAL_INST     2
#define EXC_BREAKPOINT       3
#define EXC_LOAD_MISALIGNED  4
#define EXC_LOAD_FAULT       5
#define EXC_STORE_MISALIGNED 6
#define EXC_STORE_FAULT      7
#define EXC_ECALL_U          8
#define EXC_ECALL_S          9
#define EXC_ECALL_M          11
#define EXC_INST_PAGE_FAULT  12
#define EXC_LOAD_PAGE_FAULT  13
#define EXC_STORE_PAGE_FAULT 15

#define SATP_MODE_BARE 0ULL
#define SATP_MODE_SV39 8ULL
#define SATP_MODE_SV48 9ULL
#define SATP_MODE_SV57 10ULL

#define PTE_V     0x001
#define PTE_R     0x002
#define PTE_W     0x004
#define PTE_X     0x008
#define PTE_U     0x010
#define PTE_G     0x020
#define PTE_A     0x040
#define PTE_D     0x080
#define PTE_RSW   0x300
#define PTE_PPN   0x003FFFFFFFFFFC00ULL
#define PTE_FLAGS 0x3FF

typedef enum {
    ACC_EXEC = 1,
    ACC_LOAD = 2,
    ACC_STORE = 3,
} access_kind;

typedef enum {
    STEP_OK = 0,
    STEP_TRAP,
    STEP_WFI,
    STEP_ECALL_M,
    STEP_SHUTDOWN,
    STEP_FAULT,
} step_result;

typedef struct cpu {
    u64 x[32];
    u64 pc;
    u32 priv;

    u64 f[32];

    u64 csr[4096];

    u64 hw_mip;
    u64 hw_time;

    u64 last_cause;
    u64 last_tval;
    u32 last_from_priv;

    u64 instret;
    u64 cycles;
    u64 n_traps;
    u64 n_ecalls;

    bus *bus;
    mmu *mmu;
    u32 hartid;

    u64 rsrv_addr;
    bool rsrv_valid;

    u32 last_insn_len;

    bool halted;
} cpu;

rvm_err cpu_init(cpu *c, bus *b, mmu *m, u32 hartid);
void cpu_reset(cpu *c, u64 entry_pc, u64 dtb_addr);

static inline u64 cpu_rd(const cpu *c, u32 r) {
    return r ? c->x[r & 31] : 0;
}
static inline void cpu_wr(cpu *c, u32 r, u64 v) {
    if (r & 31)
        c->x[r & 31] = v;
}

u64 csr_read(cpu *c, u32 addr, bool *illegal);
void csr_write(cpu *c, u32 addr, u64 val, bool *illegal);
const char *csr_name(u32 addr);

void cpu_trap(cpu *c, u64 cause, u64 tval, bool interrupt);
void cpu_mret(cpu *c);
void cpu_sret(cpu *c);
u64 cpu_pending_interrupt(cpu *c);

step_result cpu_step(cpu *c);
bool cpu_exec32(cpu *c, u32 insn, step_result *res);

void cpu_init_isa(cpu *c);

bool cpu_mem_load(cpu *c, u64 va, u32 size, u64 *out, u32 *cause);
bool cpu_mem_store(cpu *c, u64 va, u32 size, u64 val, u32 *cause);

bool fp_exec(cpu *c, u32 insn, step_result *res);

u32 c_expand(u16 ci, bool *illegal);

static inline s32 sext(s64 v, u32 bits) {
    u32 sh = 32 - bits;
    s32 t = (s32)((u32)v << sh);
    return t >> sh;
}
static inline s64 sext64(u64 v, u32 bits) {
    u32 sh = 64 - bits;
    s64 t = (s64)(v << sh);
    return t >> sh;
}

#endif
