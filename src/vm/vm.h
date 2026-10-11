#ifndef RVM_VM_H
#define RVM_VM_H

#include <stdatomic.h>

#include "../cpu/cpu.h"
#include "../devices/clint.h"
#include "../devices/plic.h"
#include "../devices/uart.h"
#include "../devices/virtio.h"
#include "../devices/virtio_blk.h"
#include "../devices/virtio_net.h"
#include "../loader/loader.h"
#include "../mmu/mmu.h"
#include "sbi.h"

typedef void (*vm_write_fn)(void *ud, const u8 *buf, size_t n);
typedef int (*vm_poll_fn)(void *ud, u8 *buf, size_t max);

typedef struct vm_opts {
    u64 ram_size;

    const char *kernel_path;
    const char *dtb_path;
    const char *initrd_path;
    const char *disk_path;
    bool create_disk;
    u64 disk_size;

    const char *bootargs;
    bool virtual_time;
    const char *isa;
    const char *mmu_type;

    u64 entry_override;
    u64 dtb_addr;
    u64 initrd_addr;
    bool raw_kernel;

    u64 max_insns;
    bool trace;
    u64 trace_from;
    rvm_loglevel log_level;

    vm_write_fn write;
    void *write_ud;
    vm_poll_fn poll;
    void *poll_ud;
} vm_opts;

void vm_opts_default(vm_opts *o);

typedef struct vm {
    vm_opts opts;

    bus bus;
    mmu mmu;
    cpu cpu;
    clint clint;
    plic plic;
    uart uart;
    sbi sbi;

    virtio_blk blk;
    virtio_net net;
    virtio vio[RVM_VIRTIO_COUNT];
    bool blk_present;
    bool net_present;

    u8 *dtb;
    u32 dtb_len;
    u64 dtb_addr;
    u64 entry;

    _Atomic bool running;
    bool irq_dirty;
    u32 exit_code;
    u64 insns;
    u64 trap_log;
    u32 term_col;
    u8 term_esc;
    u8 term_param[8];
    u8 term_paramlen;
    u64 start_ns;

    u8 outbuf[1024];
    u32 outlen;
} vm;

rvm_err vm_new(vm *v, const vm_opts *o);
rvm_err vm_load(vm *v);
rvm_err vm_run(vm *v);
void vm_stop(vm *v, u32 code);
void vm_free(vm *v);

void vm_console_in(vm *v, const u8 *buf, size_t n);
void vm_print_stats(const vm *v);

#endif
