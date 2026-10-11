#include "cpu.h"

const char *csr_name(u32 a) {
    switch (a) {
    case CSR_MSTATUS:
        return "mstatus";
    case CSR_MISA:
        return "misa";
    case CSR_MEDELEG:
        return "medeleg";
    case CSR_MIDELEG:
        return "mideleg";
    case CSR_MIE:
        return "mie";
    case CSR_MTVEC:
        return "mtvec";
    case CSR_MCOUNTEREN:
        return "mcounteren";
    case CSR_MSCRATCH:
        return "mscratch";
    case CSR_MEPC:
        return "mepc";
    case CSR_MCAUSE:
        return "mcause";
    case CSR_MTVAL:
        return "mtval";
    case CSR_MIP:
        return "mip";
    case CSR_MHARTID:
        return "mhartid";
    case CSR_MVENDORID:
        return "mvendorid";
    case CSR_MARCHID:
        return "marchid";
    case CSR_MIMPID:
        return "mimpid";
    case CSR_MCYCLE:
        return "mcycle";
    case CSR_MINSTRET:
        return "minstret";
    case CSR_SSTATUS:
        return "sstatus";
    case CSR_SIE:
        return "sie";
    case CSR_STVEC:
        return "stvec";
    case CSR_SCOUNTEREN:
        return "scounteren";
    case CSR_SSCRATCH:
        return "sscratch";
    case CSR_SEPC:
        return "sepc";
    case CSR_SCAUSE:
        return "scause";
    case CSR_STVAL:
        return "stval";
    case CSR_SIP:
        return "sip";
    case CSR_STIMECMP:
        return "stimecmp";
    case CSR_SATP:
        return "satp";
    case CSR_FFLAGS:
        return "fflags";
    case CSR_FRM:
        return "frm";
    case CSR_FCSR:
        return "fcsr";
    case CSR_CYCLE:
        return "cycle";
    case CSR_TIME:
        return "time";
    case CSR_INSTRET:
        return "instret";
    default:
        return NULL;
    }
}

static u64 mstatus_with_sd(u64 v) {
    bool dirty = ((v & MSTATUS_FS) == (3ULL << MSTATUS_FS_SHIFT)) ||
                 ((v & MSTATUS_XS) == (3ULL << MSTATUS_XS_SHIFT));
    return dirty ? (v | MSTATUS_SD) : (v & ~MSTATUS_SD);
}

u64 csr_read(cpu *c, u32 a, bool *illegal) {
    if (illegal)
        *illegal = false;
    if (((a >> 10) & 3) == 3) {
    }

    switch (a) {
    case CSR_MSTATUS:
        return mstatus_with_sd(c->csr[CSR_MSTATUS]);
    case CSR_SSTATUS:
        return mstatus_with_sd(c->csr[CSR_MSTATUS]) & SSTATUS_MASK;

    case CSR_MISA:
        return c->csr[CSR_MISA];

    case CSR_MIE:
        return c->csr[CSR_MIE];
    case CSR_SIE:
        return c->csr[CSR_MIE] & c->csr[CSR_MIDELEG];

    case CSR_MIP:
        return c->csr[CSR_MIP] | c->hw_mip;
    case CSR_SIP:
        return (c->csr[CSR_MIP] | c->hw_mip) & c->csr[CSR_MIDELEG];

    case CSR_TIME:
        return c->hw_time;
    case CSR_CYCLE:
        return c->cycles;
    case CSR_INSTRET:
        return c->instret;
    case CSR_MCYCLE:
        return c->cycles;
    case CSR_MINSTRET:
        return c->instret;

    case CSR_FFLAGS:
        return c->csr[CSR_FCSR] & 0x1F;
    case CSR_FRM:
        return (c->csr[CSR_FCSR] >> 5) & 0x7;
    case CSR_FCSR:
        return c->csr[CSR_FCSR] & 0xFF;

    case CSR_MHARTID:
        return c->hartid;
    case CSR_MVENDORID:
        return 0;
    case CSR_MARCHID:
        return 0x72766dULL;
    case CSR_MIMPID:
        return 1;

    case CSR_STIMECMP:
        return c->csr[CSR_STIMECMP];

    default:
        if (a < 4096)
            return c->csr[a];
        if (illegal)
            *illegal = true;
        return 0;
    }
}

void csr_write(cpu *c, u32 a, u64 v, bool *illegal) {
    if (illegal)
        *illegal = false;
    if (((a >> 10) & 3) == 3) {
        if (illegal)
            *illegal = true;
        return;
    }

    switch (a) {
    case CSR_MSTATUS:
        c->csr[CSR_MSTATUS] = v & ~(MSTATUS_MPP);
        c->csr[CSR_MSTATUS] |= (v & MSTATUS_MPP);
        break;

    case CSR_SSTATUS:
        c->csr[CSR_MSTATUS] = (c->csr[CSR_MSTATUS] & ~SSTATUS_MASK) | (v & SSTATUS_MASK);
        break;

    case CSR_MISA:
        break;

    case CSR_MIE:
        c->csr[CSR_MIE] = v & 0x2AAA;
        break;
    case CSR_SIE: {
        u64 d = c->csr[CSR_MIDELEG];
        c->csr[CSR_MIE] = (c->csr[CSR_MIE] & ~d) | (v & d);
        break;
    }

    case CSR_MIP:
        c->csr[CSR_MIP] = (c->csr[CSR_MIP] & ~MIP_SSIP) | (v & MIP_SSIP);
        break;
    case CSR_SIP: {
        u64 d = c->csr[CSR_MIDELEG] & MIP_SSIP;
        c->csr[CSR_MIP] = (c->csr[CSR_MIP] & ~d) | (v & d);
        break;
    }

    case CSR_SATP: {
        if (c->priv == PRV_S && (c->csr[CSR_MSTATUS] & MSTATUS_TVM)) {
            if (illegal)
                *illegal = true;
            return;
        }
        u64 mode = (v >> 60) & 0xF;
        if (mode != SATP_MODE_BARE &&
            !((mode == SATP_MODE_SV39) || (mode == SATP_MODE_SV48) || (mode == SATP_MODE_SV57))) {
            break;
        }
        bool changed = (c->csr[CSR_SATP] != v);
        c->csr[CSR_SATP] = v;
        if (changed && c->mmu)
            mmu_flush(c->mmu);
        break;
    }

    case CSR_FFLAGS:
        c->csr[CSR_FCSR] = (c->csr[CSR_FCSR] & ~0x1FU) | (v & 0x1F);
        break;
    case CSR_FRM:
        c->csr[CSR_FCSR] = (c->csr[CSR_FCSR] & ~0xE0U) | ((v & 0x7) << 5);
        break;
    case CSR_FCSR:
        c->csr[CSR_FCSR] = v & 0xFF;
        break;

    case CSR_MEPC:
        c->csr[CSR_MEPC] = v & ~1ULL;
        break;
    case CSR_SEPC:
        c->csr[CSR_SEPC] = v & ~1ULL;
        break;

    case CSR_MCYCLE:
        c->cycles = v;
        break;
    case CSR_MINSTRET:
        c->instret = v;
        break;

    default:
        if (a < 4096) {
            c->csr[a] = v;
        } else if (illegal) {
            *illegal = true;
        }
        break;
    }
}

static const u32 irq_priority[] = {IRQ_M_EXT, IRQ_M_SOFT, IRQ_M_TIMER,
                                   IRQ_S_EXT, IRQ_S_SOFT, IRQ_S_TIMER};

u64 cpu_pending_interrupt(cpu *c) {
    u64 mip = c->csr[CSR_MIP] | c->hw_mip;
    u64 mie = c->csr[CSR_MIE];
    u64 mideleg = c->csr[CSR_MIDELEG];
    u64 pending = mip & mie;
    if (!pending)
        return 0;

    u64 to_m = pending & ~mideleg;
    u64 to_s = pending & mideleg;
    u64 take = 0;

    if (c->priv < PRV_M) {
        take |= to_m;
    } else if (c->csr[CSR_MSTATUS] & MSTATUS_MIE) {
        take |= to_m;
    }
    if (c->priv < PRV_S) {
        take |= to_s;
    } else if (c->priv == PRV_S && (c->csr[CSR_MSTATUS] & MSTATUS_SIE)) {
        take |= to_s;
    }
    if (!take)
        return 0;

    for (u32 i = 0; i < RVM_ARRAY_SIZE(irq_priority); i++) {
        u32 b = irq_priority[i];
        if (take & (1ULL << b))
            return (1ULL << 63) | b;
    }
    return 0;
}

static u64 tvec_target(u64 tvec, u64 cause, bool interrupt) {
    u64 base = tvec & ~3ULL;
    u32 mode = (u32)(tvec & 3);
    if (mode == 1 && interrupt)
        return base + 4 * (cause & 0xF);
    return base;
}

void cpu_trap(cpu *c, u64 cause, u64 tval, bool interrupt) {
    bool deleg = false;
    if (c->priv <= PRV_S) {
        u64 mask = interrupt ? c->csr[CSR_MIDELEG] : c->csr[CSR_MEDELEG];
        if (cause < 64 && (mask & (1ULL << cause)))
            deleg = true;
    }

    c->n_traps++;
    c->last_cause = cause | (interrupt ? (1ULL << 63) : 0);
    c->last_tval = tval;
    c->last_from_priv = c->priv;

    u64 *status = &c->csr[CSR_MSTATUS];

    if (deleg) {
        c->csr[CSR_SEPC] = c->pc & ~1ULL;
        c->csr[CSR_SCAUSE] = cause | (interrupt ? (1ULL << 63) : 0);
        c->csr[CSR_STVAL] = tval;
        *status = (*status & ~MSTATUS_SPIE) | ((*status & MSTATUS_SIE) ? MSTATUS_SPIE : 0);
        *status &= ~MSTATUS_SIE;
        *status = (*status & ~MSTATUS_SPP) | ((c->priv == PRV_S) ? MSTATUS_SPP : 0);
        c->priv = PRV_S;
        c->pc = tvec_target(c->csr[CSR_STVEC], cause, interrupt);
    } else {
        c->csr[CSR_MEPC] = c->pc & ~1ULL;
        c->csr[CSR_MCAUSE] = cause | (interrupt ? (1ULL << 63) : 0);
        c->csr[CSR_MTVAL] = tval;
        *status = (*status & ~MSTATUS_MPIE) | ((*status & MSTATUS_MIE) ? MSTATUS_MPIE : 0);
        *status &= ~MSTATUS_MIE;
        *status = (*status & ~MSTATUS_MPP) | ((u64)c->priv << MSTATUS_MPP_SHIFT);
        c->priv = PRV_M;
        c->pc = tvec_target(c->csr[CSR_MTVEC], cause, interrupt);
    }
    LOG_TRACE("trap: cause=%llu tval=0x%llx %s -> %s-mode pc=0x%llx",
              (unsigned long long)(cause | (interrupt ? (1ULL << 63) : 0)),
              (unsigned long long)tval, interrupt ? "irq" : "exc", deleg ? "S" : "M",
              (unsigned long long)c->pc);
}

void cpu_mret(cpu *c) {
    u64 *s = &c->csr[CSR_MSTATUS];
    u64 mpp = (*s & MSTATUS_MPP) >> MSTATUS_MPP_SHIFT;
    c->priv = (u32)mpp;
    *s = (*s & ~MSTATUS_MIE) | ((*s & MSTATUS_MPIE) ? MSTATUS_MIE : 0);
    *s |= MSTATUS_MPIE;
    *s &= ~MSTATUS_MPP;
    c->pc = c->csr[CSR_MEPC];
}

void cpu_sret(cpu *c) {
    u64 *s = &c->csr[CSR_MSTATUS];
    u64 spp = (*s & MSTATUS_SPP) ? PRV_S : PRV_U;
    c->priv = (u32)spp;
    *s = (*s & ~MSTATUS_SIE) | ((*s & MSTATUS_SPIE) ? MSTATUS_SIE : 0);
    *s |= MSTATUS_SPIE;
    *s &= ~MSTATUS_SPP;
    c->pc = c->csr[CSR_SEPC];
}

void cpu_init_isa(cpu *c) {
    u64 ext = (1ULL << 8) | (1ULL << 12) | (1ULL << 0) | (1ULL << 5) | (1ULL << 3) | (1ULL << 2) |
              (1ULL << 18) | (1ULL << 20);
    c->csr[CSR_MISA] = (2ULL << 62) | ext;
}
