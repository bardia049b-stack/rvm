#include "vm.h"

#include <errno.h>
#include <time.h>
#include <unistd.h>

void vm_opts_default(vm_opts *o) {
    memset(o, 0, sizeof(*o));
    o->ram_size = RVM_RAM_DEFAULT;
    o->bootargs =
        "console=ttyS0 earlycon=ns16550a,mmio32,0x10000000 root=/dev/vda rootwait rw";
    o->isa = "rv64imafdc";
    o->mmu_type = "riscv,sv57";
    o->dtb_addr = RVM_DTB_DEFAULT_ADDR;
    o->initrd_addr = RVM_INITRD_DEFAULT_ADDR;
    o->disk_size = 2ULL << 30;
    o->log_level = RVM_LOG_INFO;
}

static void irq_raise(void *ud, u32 irq, bool level) {
    vm *v = (vm *)ud;
    if (level)
        plic_raise(&v->plic, irq);
    v->irq_dirty = true;
}

static void term_note(vm *v, u8 ch) {
    switch (v->term_esc) {
    case 0:
        if (ch == 0x1B) {
            v->term_esc = 1;
            return;
        }
        if (ch == '\n' || ch == '\r') {
            v->term_col = 0;
            return;
        }
        if (ch == '\b') {
            if (v->term_col)
                v->term_col--;
            return;
        }
        if (ch == '\t') {
            v->term_col = (v->term_col | 7u) + 1u;
            return;
        }
        if (ch >= 0x20)
            v->term_col++;
        return;
    case 1:
        v->term_paramlen = 0;
        v->term_esc = (ch == '[') ? 2 : 0;
        return;
    default:
        if (ch >= '0' && ch <= '9') {
            if (v->term_paramlen < sizeof(v->term_param))
                v->term_param[v->term_paramlen++] = ch;
            return;
        }
        if (ch == ';')
            return;
        if (ch == 'n' && v->term_paramlen == 1 && v->term_param[0] == '6') {
            char rep[24];
            int n = snprintf(rep, sizeof(rep), "\033[1;%uR", v->term_col + 1u);
            for (int i = 0; i < n; i++)
                uart_push(&v->uart, (u8)rep[i]);
        }
        v->term_esc = 0;
        return;
    }
}

static void uart_emit(void *ud, u8 ch) {
    vm *v = (vm *)ud;
    term_note(v, ch);
    if (!v->opts.write) {
        fputc(ch, stdout);
        if (ch == '\n')
            fflush(stdout);
        return;
    }
    v->outbuf[v->outlen++] = ch;
    if (v->outlen == sizeof(v->outbuf) || ch == '\n') {
        v->opts.write(v->opts.write_ud, v->outbuf, v->outlen);
        v->outlen = 0;
    }
}

static void uart_flush(vm *v) {
    if (v->outlen && v->opts.write) {
        v->opts.write(v->opts.write_ud, v->outbuf, v->outlen);
    }
    v->outlen = 0;
    fflush(stdout);
}

static bool sbi_shutdown_cb(void *ud, u32 type, u32 reason) {
    vm *v = (vm *)ud;
    LOG_INFO("guest requested %s (reason %u) after %llu instructions",
             type == SBI_SRST_REBOOT ? "reboot" : "shutdown", reason, (unsigned long long)v->insns);
    vm_stop(v, reason);
    return false;
}

static void sbi_set_timer_cb(void *ud, u64 stime) {
    vm *v = (vm *)ud;
    v->clint.mtimecmp[0] = stime;
    v->cpu.csr[CSR_STIMECMP] = stime;
}

static void trace_hook(void *ud, u64 pc, u32 insn, u32 len) {
    vm *v = (vm *)ud;
    LOG_TRACE("%012llx: %s%08x  priv=%u satp=%016llx", (unsigned long long)pc,
              len == 2 ? "    " : "", insn, v->cpu.priv, (unsigned long long)v->cpu.csr[CSR_SATP]);
    RVM_UNUSED(v);
}

rvm_err vm_new(vm *v, const vm_opts *o) {
    if (!v || !o)
        return RVM_ERR_BADARG;
    memset(v, 0, sizeof(*v));
    v->opts = *o;
    rvm_log_set_level(o->log_level);

    rvm_err e;
    if ((e = bus_init(&v->bus, o->ram_size)) != RVM_OK)
        return e;
    if ((e = mmu_init(&v->mmu, &v->bus)) != RVM_OK)
        goto fail;
    if ((e = cpu_init(&v->cpu, &v->bus, &v->mmu, 0)) != RVM_OK)
        goto fail;
    if ((e = clint_init(&v->clint)) != RVM_OK)
        goto fail;

    if (o->virtual_time || (o->bootargs && strstr(o->bootargs, "rvm.time=virtual")))
        clint_set_virtual_time(&v->clint, true, v->cpu.instret);

    v->cpu.csr[CSR_MEDELEG] =
        (1u << EXC_INST_MISALIGNED) | (1u << EXC_INST_FAULT) | (1u << EXC_ILLEGAL_INST) |
        (1u << EXC_BREAKPOINT) | (1u << EXC_LOAD_MISALIGNED) | (1u << EXC_LOAD_FAULT) |
        (1u << EXC_STORE_MISALIGNED) | (1u << EXC_STORE_FAULT) | (1u << EXC_ECALL_U);
    v->cpu.csr[CSR_MIDELEG] = MIP_SSIP | MIP_STIP | MIP_SEIP;
    if ((e = plic_init(&v->plic)) != RVM_OK)
        goto fail;
    if ((e = uart_init(&v->uart, RVM_UART_IRQ)) != RVM_OK)
        goto fail;
    sbi_init(&v->sbi, &v->bus);

    v->cpu.csr[CSR_MCOUNTEREN] = 0xFFFFFFFFu;
    v->cpu.csr[CSR_SCOUNTEREN] = 0xFFFFFFFFu;
    v->cpu.csr[CSR_MENVCFG] = 0;

    bus_attach(&v->bus, "clint", RVM_CLINT_BASE, RVM_CLINT_SIZE, &v->clint, clint_load,
               clint_store);
    bus_attach(&v->bus, "plic", RVM_PLIC_BASE, RVM_PLIC_SIZE, &v->plic, plic_load, plic_store);
    bus_attach(&v->bus, "uart0", RVM_UART_BASE, RVM_UART_SIZE, &v->uart, uart_load, uart_store);

    uart_set_sink(&v->uart, uart_emit, v);
    uart_set_irq(&v->uart, irq_raise, v);

    if (o->disk_path && *o->disk_path) {
        e = virtio_blk_open(&v->blk, o->disk_path, o->disk_size, o->create_disk);
        if (e != RVM_OK)
            goto fail;
        v->blk_present = true;
        virtio_init(&v->vio[0], &v->bus, virtio_blk_backend(&v->blk), RVM_VIRTIO_IRQ(0));
        virtio_set_irq(&v->vio[0], irq_raise, v);
        bus_attach(&v->bus, "virtio0", RVM_VIRTIO_BASE, RVM_VIRTIO_STRIDE, &v->vio[0], virtio_load,
                   virtio_store);
    }

    {
        u32 slot = v->blk_present ? 1u : 0u;
        rvm_err ne = virtio_net_init(&v->net);
        if (ne == RVM_OK) {
            v->net_present = true;
            virtio_init(&v->vio[slot], &v->bus, virtio_net_backend(&v->net), RVM_VIRTIO_IRQ(slot));
            v->net.vio = &v->vio[slot];
            virtio_set_irq(&v->vio[slot], irq_raise, v);
            bus_attach(&v->bus, "virtio-net", RVM_VIRTIO_BASE + slot * RVM_VIRTIO_STRIDE,
                       RVM_VIRTIO_STRIDE, &v->vio[slot], virtio_load, virtio_store);
        }
    }

    for (u32 i = 0; i < RVM_VIRTIO_COUNT; i++) {
        u64 base = RVM_VIRTIO_BASE + i * RVM_VIRTIO_STRIDE;
        if (bus_find(&v->bus, base))
            continue;
        bus_attach(&v->bus, "virtio-empty", base, RVM_VIRTIO_STRIDE, NULL, virtio_absent_load,
                   NULL);
    }

    v->sbi.write = NULL;
    v->sbi.write_ud = NULL;
    v->sbi.shutdown = sbi_shutdown_cb;
    v->sbi.shutdown_ud = v;
    v->sbi.set_timer = sbi_set_timer_cb;
    v->sbi.set_timer_ud = v;

    if (o->trace) {
        rvm_trace_from(o->trace_from);
        rvm_trace_set(trace_hook, v);
    }
    v->running = true;
    return RVM_OK;

fail:
    vm_free(v);
    return e;
}

rvm_err vm_load(vm *v) {
    const vm_opts *o = &v->opts;
    rvm_err e;

    if (!o->kernel_path || !*o->kernel_path)
        return RVM_ERR_BADARG;

    u64 entry = 0;
    loader_kind kind = o->raw_kernel ? LOADER_KIND_BLOB : loader_sniff(o->kernel_path);
    if (kind == LOADER_KIND_BLOB) {
        u64 sz = 0;
        u64 addr = o->entry_override ? o->entry_override : RVM_KERNEL_ENTRY;
        e = loader_load_blob(&v->bus, o->kernel_path, addr, &sz);
        if (e != RVM_OK)
            return e;
        entry = addr;
    } else if (kind == LOADER_KIND_PE) {
        loader_stats st;
        u64 addr = o->entry_override ? o->entry_override : RVM_KERNEL_ENTRY;
        e = loader_load_pe(&v->bus, o->kernel_path, addr, &entry, &st);
        if (e != RVM_OK)
            return e;
    } else if (kind == LOADER_KIND_ELF) {
        loader_stats st;
        e = loader_load_elf(&v->bus, o->kernel_path, &entry, &st);
        if (e != RVM_OK)
            return e;
    } else {
        LOG_ERROR("vm: %s is not an ELF, a PE Image or readable at all", o->kernel_path);
        return RVM_ERR_BADARG;
    }
    v->entry = entry;

    u64 initrd_end = 0;
    if (o->initrd_path && *o->initrd_path) {
        u64 sz = 0;
        e = loader_load_blob(&v->bus, o->initrd_path, o->initrd_addr, &sz);
        if (e != RVM_OK)
            return e;
        initrd_end = o->initrd_addr + sz;
    }

    if (o->dtb_path && *o->dtb_path) {
        u64 sz = 0;
        e = loader_load_blob(&v->bus, o->dtb_path, o->dtb_addr, &sz);
        if (e != RVM_OK)
            return e;
        v->dtb_len = (u32)sz;
        LOG_INFO("vm: loaded external DTB (%llu bytes) at 0x%llx", (unsigned long long)sz,
                 (unsigned long long)o->dtb_addr);
    } else {
        dtb_opts d;
        memset(&d, 0, sizeof(d));
        d.ram_base = RVM_RAM_BASE;
        d.ram_size = o->ram_size;
        d.model = "rvm,virt";
        d.bootargs = o->bootargs;
        d.isa = o->isa;
        d.mmu_type = o->mmu_type;
        d.n_virtio = RVM_VIRTIO_COUNT;
        d.initrd_start = initrd_end ? o->initrd_addr : 0;
        d.initrd_end = initrd_end;
        d.timebase_hz = CLINT_TIMEBASE_HZ;
        d.stdout_path = "/soc/serial@10000000";
        e = dtb_build(&d, &v->dtb, &v->dtb_len);
        if (e != RVM_OK)
            return e;
        if (!bus_ram_valid(&v->bus, o->dtb_addr, v->dtb_len)) {
            free(v->dtb);
            return RVM_ERR_RANGE;
        }
        memcpy(bus_ram_ptr(&v->bus, o->dtb_addr), v->dtb, v->dtb_len);
        LOG_INFO("vm: built DTB in-process (%u bytes) at 0x%llx", v->dtb_len,
                 (unsigned long long)o->dtb_addr);
    }
    v->dtb_addr = o->dtb_addr;

    cpu_reset(&v->cpu, entry, o->dtb_addr);
    v->cpu.priv = PRV_S;
    v->cpu.csr[CSR_MSTATUS] = (PRV_S << MSTATUS_MPP_SHIFT) | MSTATUS_MPIE;
    v->cpu.csr[CSR_MTVEC] = entry & ~3ULL;
    LOG_INFO("vm: entry=0x%llx priv=S a0=%llu a1=0x%llx ram=%llu MiB", (unsigned long long)entry,
             (unsigned long long)v->cpu.x[10], (unsigned long long)v->cpu.x[11],
             (unsigned long long)(o->ram_size >> 20));
    return RVM_OK;
}

#define RVM_POLL_INTERVAL 256u

static void refresh_interrupts(vm *v) {
    clint_advance(&v->clint, v->cpu.instret);
    u64 bits = clint_tick(&v->clint);
    v->cpu.hw_time = clint_mtime(&v->clint);
    v->cpu.hw_mip = bits | plic_update(&v->plic);
}

static void pump_input(vm *v) {
    if (v->net_present)
        virtio_net_poll(&v->net);
    if (!v->opts.poll)
        return;
    u32 space = uart_rx_space(&v->uart);
    if (space == 0)
        return;
    u8 buf[256];
    int n = v->opts.poll(v->opts.poll_ud, buf, RVM_MIN(space, (u32)sizeof(buf)));
    for (int i = 0; i < n; i++) {
        if (!uart_push(&v->uart, buf[i]))
            break;
    }
}

static void idle_until_timer(vm *v) {
    u64 deadline = v->clint.mtimecmp[0];
    u64 now = v->clint.mtime;
    long ns;
    if (deadline == UINT64_MAX || deadline <= now) {
        ns = 1000000;
    } else {
        u64 ticks = deadline - now;
        u64 ns_per_tick = 1000000000ULL / CLINT_TIMEBASE_HZ;
        u64 want = ticks * ns_per_tick;
        ns = (long)RVM_MIN(want, 10000000ULL);
    }
    struct timespec ts = {ns / 1000000000L, ns % 1000000000L};
    nanosleep(&ts, NULL);
}

rvm_err vm_run(vm *v) {
    if (!v->opts.kernel_path)
        return RVM_ERR_BADARG;
    v->start_ns = rvm_now_ns();
    v->running = true;

    while (v->running) {
        if (v->irq_dirty || (v->insns % RVM_POLL_INTERVAL) == 0) {
            pump_input(v);
            refresh_interrupts(v);
            v->irq_dirty = false;
            if (v->outlen && v->opts.write) {
                v->opts.write(v->opts.write_ud, v->outbuf, v->outlen);
                v->outlen = 0;
            }
        }

        step_result r = cpu_step(&v->cpu);
        v->insns++;

        switch (r) {
        case STEP_OK:
            break;

        case STEP_TRAP: {
            u64 cause = v->cpu.last_cause;
            bool is_irq = (cause >> 63) & 1;
            if (v->trap_log < 16 || (v->trap_log & 0xFFFFFu) == 0) {
                LOG_DEBUG("vm: trap %llu cause=%llu tval=0x%llx pc=0x%llx from priv=%u",
                          (unsigned long long)v->trap_log, (unsigned long long)(cause & 0x3F),
                          (unsigned long long)v->cpu.last_tval, (unsigned long long)v->cpu.pc,
                          v->cpu.last_from_priv);
            }
            v->trap_log++;
            if (!is_irq && (cause & 0x3F) == EXC_ECALL_S && v->cpu.last_from_priv == PRV_S) {
                if (!sbi_handle(&v->sbi, &v->cpu))
                    goto done;
                v->cpu.csr[CSR_MEPC] += 4;
                cpu_mret(&v->cpu);
            } else if (is_irq && v->cpu.priv == PRV_M) {
                if ((cause & 0x3F) == 7)
                    v->cpu.csr[CSR_MIP] |= MIP_STIP;
                cpu_mret(&v->cpu);
            } else if (!is_irq && v->cpu.priv == PRV_M && v->cpu.last_from_priv != PRV_M) {
                LOG_ERROR("vm: undelegated exception cause=%llu tval=0x%llx pc=0x%llx priv=%u",
                          (unsigned long long)(cause & 0x3F),
                          (unsigned long long)v->cpu.last_tval,
                          (unsigned long long)v->cpu.pc, v->cpu.last_from_priv);
                vm_stop(v, 1);
            }
            break;
        }

        case STEP_WFI:
            v->cpu.pc += v->cpu.last_insn_len ? v->cpu.last_insn_len : 4;
            v->cpu.instret++;
            pump_input(v);
            refresh_interrupts(v);
            v->irq_dirty = false;
            if (!(v->cpu.hw_mip & v->cpu.csr[CSR_MIE]))
                idle_until_timer(v);
            break;

        case STEP_ECALL_M:
            LOG_INFO("vm: ecall from M-mode (a0=0x%llx a7=0x%llx) -- stopping",
                     (unsigned long long)v->cpu.x[10], (unsigned long long)v->cpu.x[17]);
            v->exit_code = (u32)v->cpu.x[10];
            goto done;

        case STEP_SHUTDOWN:
            goto done;

        case STEP_FAULT:
            LOG_ERROR("vm: emulator-level fault at pc=0x%llx", (unsigned long long)v->cpu.pc);
            uart_flush(v);
            return RVM_ERR;

        default:
            break;
        }

        if (v->opts.max_insns && v->insns >= v->opts.max_insns) {
            LOG_INFO("vm: instruction budget of %llu reached",
                     (unsigned long long)v->opts.max_insns);
            v->exit_code = 0;
            goto done;
        }
    }

done:
    uart_flush(v);
    v->running = false;
    return RVM_OK;
}

void vm_stop(vm *v, u32 code) {
    v->running = false;
    v->exit_code = code;
}

void vm_print_stats(const vm *v) {
    u64 elapsed = rvm_now_ns() - v->start_ns;
    double secs = elapsed ? (double)elapsed / 1e9 : 1e-9;
    double mips = (elapsed > 0) ? (double)v->insns / secs / 1e6 : 0.0;
    fprintf(stderr,
            "\n--- RVM statistics -------------------------------------------\n"
            "  instructions retired : %llu\n"
            "  wall time            : %.3f s\n"
            "  throughput           : %.2f MIPS\n"
            "  traps                : %llu (ecalls %llu, SBI %llu, unsupported %llu)\n"
            "  RAM  loads/stores    : %llu / %llu\n"
            "  MMIO loads/stores    : %llu / %llu (%llu faults)\n"
            "  TLB  hit/miss/walk   : %llu / %llu / %llu (%llu faults)\n"
            "  UART tx/rx           : %llu / %llu\n",
            (unsigned long long)v->insns, secs, mips, (unsigned long long)v->cpu.n_traps,
            (unsigned long long)v->cpu.n_ecalls, (unsigned long long)v->sbi.n_calls,
            (unsigned long long)v->sbi.n_unsupported, (unsigned long long)v->bus.n_ram_load,
            (unsigned long long)v->bus.n_ram_store, (unsigned long long)v->bus.n_mmio_load,
            (unsigned long long)v->bus.n_mmio_store, (unsigned long long)v->bus.n_fault,
            (unsigned long long)v->mmu.n_tlb_hit, (unsigned long long)v->mmu.n_tlb_miss,
            (unsigned long long)v->mmu.n_walk, (unsigned long long)v->mmu.n_fault,
            (unsigned long long)v->uart.n_tx, (unsigned long long)v->uart.n_rx);
    if (v->blk_present)
        fprintf(stderr, "  virtio-blk r/w/flush : %llu / %llu / %llu (%llu errors)\n",
                (unsigned long long)v->blk.n_read, (unsigned long long)v->blk.n_write,
                (unsigned long long)v->blk.n_flush, (unsigned long long)v->blk.n_err);
    if (v->net_present)
        fprintf(stderr, "  virtio-net rx/tx      : %llu / %llu\n",
                (unsigned long long) virtio_net_rx_packets(&v->net),
                (unsigned long long) virtio_net_tx_packets(&v->net));
    fprintf(stderr, "  PLIC raise/claim     : %llu / %llu\n", (unsigned long long)v->plic.n_raise,
            (unsigned long long)v->plic.n_claim);
    fprintf(stderr, "--------------------------------------------------------------\n");
}

void vm_free(vm *v) {
    if (!v)
        return;
    rvm_trace_set(NULL, NULL);
    if (v->blk_present)
        virtio_blk_close(&v->blk);
    if (v->net_present)
        virtio_net_shutdown(&v->net);
    free(v->dtb);
    bus_free(&v->bus);
    memset(v, 0, sizeof(*v));
}

void vm_console_in(vm *v, const u8 *buf, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (!uart_push(&v->uart, buf[i]))
            break;
}
