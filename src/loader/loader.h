#ifndef RVM_LOADER_H
#define RVM_LOADER_H

#include "../bus/bus.h"
#include "fdt.h"

#define ELF_MAGIC0  0x7F
#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define EM_RISCV    243
#define PT_LOAD     1

typedef struct loader_stats {
    u64 segments;
    u64 bytes;
    u64 lowest, highest;
} loader_stats;

rvm_err loader_load_elf(bus *b, const char *path, u64 *entry, loader_stats *st);

rvm_err loader_load_blob(bus *b, const char *path, u64 addr, u64 *size);

typedef enum {
    LOADER_KIND_UNKNOWN = 0,
    LOADER_KIND_ELF,
    LOADER_KIND_PE,
    LOADER_KIND_BLOB
} loader_kind;

loader_kind loader_sniff(const char *path);

rvm_err loader_load_pe(bus *b, const char *path, u64 base, u64 *entry, loader_stats *st);

#define DTB_MAX_VIRTIO 8

typedef struct dtb_opts {
    u64 ram_base;
    u64 ram_size;
    const char *model;
    const char *bootargs;
    const char *isa;
    const char *mmu_type;
    u32 n_virtio;
    u64 initrd_start;
    u64 initrd_end;
    u32 timebase_hz;
    const char *stdout_path;
} dtb_opts;

rvm_err dtb_build(const dtb_opts *o, u8 **out, u32 *out_len);

#endif
