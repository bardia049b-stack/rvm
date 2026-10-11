#ifndef RVM_BUS_H
#define RVM_BUS_H

#include "../rvm.h"

#define RVM_BUS_MAX_DEVICES 16

struct bus;

typedef bool (*bus_read_fn)(void *dev, u64 off, u32 size, u64 *out);
typedef bool (*bus_write_fn)(void *dev, u64 off, u32 size, u64 val);

typedef struct bus_device {
    const char *name;
    u64 base;
    u64 size;
    void *dev;
    bus_read_fn read;
    bus_write_fn write;
} bus_device;

typedef struct bus {
    u8 *ram;
    u64 ram_size;
    u64 ram_base;

    bus_device devs[RVM_BUS_MAX_DEVICES];
    u32 ndev;

    u64 n_ram_load, n_ram_store, n_mmio_load, n_mmio_store, n_fault;
} bus;

rvm_err bus_init(bus *b, u64 ram_size);
void bus_free(bus *b);

rvm_err bus_attach(bus *b, const char *name, u64 base, u64 size, void *dev, bus_read_fn rd,
                   bus_write_fn wr);

static inline bool bus_ram_valid(const bus *b, u64 addr, u32 size) {
    if (addr < b->ram_base)
        return false;
    u64 off = addr - b->ram_base;
    return off + size <= b->ram_size && off + size >= off;
}

static inline u8 *bus_ram_ptr(bus *b, u64 addr) {
    return b->ram + (addr - b->ram_base);
}

bool bus_load(bus *b, u64 addr, u32 size, u64 *out);
bool bus_store(bus *b, u64 addr, u32 size, u64 val);

bool bus_read_bytes(bus *b, u64 addr, void *dst, size_t n);
bool bus_write_bytes(bus *b, u64 addr, const void *src, size_t n);

const bus_device *bus_find(const bus *b, u64 addr);

#endif
