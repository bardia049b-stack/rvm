#include "../src/loader/loader.h"
#include "fixtures/harness.h"
#include "test.h"

#include <unistd.h>

static size_t build_elf(u8 *buf, u64 entry, u64 paddr, const u8 *payload, u32 plen,
                        u32 memsz_extra) {
    const u32 EHSZ = 64, PHENTSZ = 56;
    size_t data_off = EHSZ + PHENTSZ;
    memset(buf, 0, data_off + plen);

    buf[0] = 0x7F;
    buf[1] = 'E';
    buf[2] = 'L';
    buf[3] = 'F';
    buf[4] = ELFCLASS64;
    buf[5] = ELFDATA2LSB;
    buf[6] = 1;

#define PUT16(off, v)                                                                              \
    do {                                                                                           \
        buf[(off)] = (u8)(v);                                                                      \
        buf[(off) + 1] = (u8)(((u16)(v)) >> 8);                                                    \
    } while (0)
#define PUT32(off, v)                                                                              \
    do {                                                                                           \
        for (u32 _i = 0; _i < 4; _i++)                                                             \
            buf[(off) + _i] = (u8)(((u32)(v)) >> (8 * _i));                                        \
    } while (0)
#define PUT64(off, v)                                                                              \
    do {                                                                                           \
        for (u32 _i = 0; _i < 8; _i++)                                                             \
            buf[(off) + _i] = (u8)(((u64)(v)) >> (8 * _i));                                        \
    } while (0)

    PUT16(16, 2);
    PUT16(18, EM_RISCV);
    PUT32(20, 1);
    PUT64(24, entry);
    PUT64(32, EHSZ);
    PUT64(40, 0);
    PUT32(48, 0);
    PUT16(52, EHSZ);
    PUT16(54, PHENTSZ);
    PUT16(56, 1);
    PUT16(58, 64);
    PUT16(60, 0);
    PUT16(62, 0);

    PUT32(EHSZ + 0, PT_LOAD);
    PUT32(EHSZ + 4, 7);
    PUT64(EHSZ + 8, data_off);
    PUT64(EHSZ + 16, paddr);
    PUT64(EHSZ + 24, paddr);
    PUT64(EHSZ + 32, plen);
    PUT64(EHSZ + 40, (u64)plen + memsz_extra);
    PUT64(EHSZ + 48, 0x1000);

    memcpy(buf + data_off, payload, plen);
#undef PUT16
#undef PUT32
#undef PUT64
    return data_off + plen;
}

void test_loader_elf(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);

    u8 img[4096];
    const u64 paddr = RVM_RAM_BASE + 0x200000ULL;
    const u64 entry = paddr + 0x100;
    u8 payload[64];
    for (u32 i = 0; i < sizeof(payload); i++)
        payload[i] = (u8)(0xA0 + i);
    size_t n = build_elf(img, entry, paddr, payload, sizeof(payload), 64);

    const char *path = "/tmp/rvm-test-kernel.elf";
    FILE *fp = fopen(path, "wb");
    CHECK(fp != NULL);
    if (fp) {
        CHECK_U64((u64)fwrite(img, 1, n, fp), (u64)n);
        fclose(fp);
    }

    loader_stats st;
    u64 got_entry = 0;
    CHECK(loader_load_elf(&t.bus, path, &got_entry, &st) == RVM_OK);
    CHECK_U64(got_entry, entry);
    CHECK_U64(st.segments, 1);
    CHECK_U64(st.lowest, paddr);
    CHECK_U64(st.bytes, sizeof(payload) + 64);

    u8 back[64];
    CHECK(bus_read_bytes(&t.bus, paddr, back, sizeof(back)));
    CHECK_MEM(back, payload, sizeof(payload));
    u8 tail[64];
    CHECK(bus_read_bytes(&t.bus, paddr + sizeof(payload), tail, sizeof(tail)));
    u8 zeros[64] = {0};
    CHECK_MEM(tail, zeros, sizeof(zeros));

    QUIET_BEGIN();
    const char *bad = "/tmp/rvm-test-notelf.bin";
    fp = fopen(bad, "wb");
    CHECK(fp != NULL);
    if (fp) {
        fputs("this is definitely not an ELF file", fp);
        fclose(fp);
    }
    CHECK(loader_load_elf(&t.bus, bad, &got_entry, &st) == RVM_ERR_BADARG);

    size_t m = build_elf(img, entry, 0x1000, payload, sizeof(payload), 0);
    fp = fopen(bad, "wb");
    if (fp) {
        fwrite(img, 1, m, fp);
        fclose(fp);
    }
    CHECK(loader_load_elf(&t.bus, bad, &got_entry, &st) == RVM_ERR_RANGE);
    QUIET_END();

    unlink(path);
    unlink(bad);
    th_free(&t);
}

void test_loader_blob(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);
    const char *path = "/tmp/rvm-test-blob.bin";
    u8 data[256];
    for (u32 i = 0; i < sizeof(data); i++)
        data[i] = (u8)(i ^ 0x5A);
    FILE *fp = fopen(path, "wb");
    CHECK(fp != NULL);
    if (fp) {
        fwrite(data, 1, sizeof(data), fp);
        fclose(fp);
    }
    u64 sz = 0;
    const u64 addr = RVM_RAM_BASE + 0x400000ULL;
    CHECK(loader_load_blob(&t.bus, path, addr, &sz) == RVM_OK);
    CHECK_U64(sz, sizeof(data));
    u8 back[256];
    CHECK(bus_read_bytes(&t.bus, addr, back, sizeof(back)));
    CHECK_MEM(back, data, sizeof(data));

    QUIET_BEGIN();
    CHECK(loader_load_blob(&t.bus, path, RVM_RAM_BASE + (16ULL << 20), &sz) == RVM_ERR_RANGE);
    CHECK(loader_load_blob(&t.bus, "/tmp/rvm-does-not-exist.bin", addr, &sz) == RVM_ERR_IO);
    QUIET_END();
    unlink(path);
    th_free(&t);
}

static u32 be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

typedef struct fdt_prop_found {
    bool found;
    u32 len;
    const u8 *data;
} fdt_prop_found;

static fdt_prop_found fdt_find(const u8 *blob, const char *node, const char *prop) {
    fdt_prop_found r = {false, 0, NULL};
    u32 off_struct = be32(blob + 8);
    u32 off_strings = be32(blob + 12);
    u32 total = be32(blob + 4);
    const u8 *s = blob + off_struct;
    const u8 *end = blob + total;
    int depth = 0;
    char path[256];
    path[0] = 0;

    while (s + 4 <= end) {
        u32 tok = be32(s);
        s += 4;
        switch (tok) {
        case 1: {
            const char *name = (const char *)s;
            s += strlen(name) + 1;
            while (((uintptr_t)s & 3) != 0)
                s++;
            depth++;
            if (depth == 1)
                snprintf(path, sizeof(path), "%s", name);
            else
                snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%s", name);
            break;
        }
        case 2:
            if (depth > 0) {
                depth--;
                char *slash = strrchr(path, '/');
                if (slash && depth > 0)
                    *slash = 0;
                else if (depth == 0)
                    path[0] = 0;
            }
            break;
        case 3: {
            u32 len = be32(s);
            u32 nameoff = be32(s + 4);
            const u8 *data = s + 8;
            s += 8 + len;
            while (((uintptr_t)s & 3) != 0)
                s++;
            const char *pname = (const char *)(blob + off_strings + nameoff);
            if (strcmp(pname, prop) == 0 && strcmp(path, node) == 0) {
                r.found = true;
                r.len = len;
                r.data = data;
                return r;
            }
            break;
        }
        case 4:
            break;
        case 9:
            return r;
        default:
            return r;
        }
    }
    return r;
}

static bool fdt_has_node(const u8 *blob, const char *path) {
    const u8 *p = blob + be32(blob + 8);
    const u8 *end = blob + be32(blob + 12);
    char cur[256];
    size_t clen = 0;
    cur[0] = 0;

    while (p + 4 <= end) {
        u32 tok = be32(p);
        p += 4;
        switch (tok) {
        case 1: {
            size_t nlen = strlen((const char *)p);
            p += (nlen + 4) & ~(size_t)3;
            if (nlen && clen + nlen + 2 < sizeof cur) {
                cur[clen++] = '/';
                memcpy(cur + clen, p - ((nlen + 4) & ~(size_t)3), nlen);
                clen += nlen;
                cur[clen] = 0;
            }
            if (strcmp(cur, path) == 0)
                return true;
            break;
        }
        case 2:
            while (clen && cur[--clen] != '/') {
            }
            cur[clen] = 0;
            break;
        case 3: {
            u32 len = be32(p);
            p += 8 + ((len + 3) & ~(u32)3);
            break;
        }
        case 4:
            break;
        case 9:
            return false;
        default:
            return false;
        }
    }
    return false;
}

void test_fdt_blob(void) {
    dtb_opts o;
    memset(&o, 0, sizeof(o));
    o.ram_base = RVM_RAM_BASE;
    o.ram_size = 1024ULL << 20;
    o.model = "rvm,virt";
    o.bootargs = "console=ttyS0 root=/dev/vda";
    o.isa = "rv64imafdc";
    o.mmu_type = "riscv,sv57";
    o.n_virtio = 8;
    o.timebase_hz = CLINT_TIMEBASE_HZ;
    o.stdout_path = "/soc/serial@10000000";
    o.initrd_start = 0x84000000ULL;
    o.initrd_end = 0x84100000ULL;

    u8 *blob = NULL;
    u32 len = 0;
    CHECK(dtb_build(&o, &blob, &len) == RVM_OK);
    CHECK(blob != NULL);
    CHECK(len > 100);

    CHECK_U64(be32(blob + 0), FDT_MAGIC);
    CHECK_U64(be32(blob + 4), len);
    CHECK_U64(be32(blob + 20), 17);
    CHECK_U64(be32(blob + 24), 16);
    CHECK_U64(be32(blob + 8), 40);

    CHECK(fdt_has_node(blob, "/cpus"));
    CHECK(fdt_has_node(blob, "/cpus/cpu@0"));
    CHECK(fdt_has_node(blob, "/cpus/cpu@0/interrupt-controller"));
    CHECK(fdt_has_node(blob, "/memory@80000000"));
    CHECK(fdt_has_node(blob, "/soc"));
    CHECK(fdt_has_node(blob, "/soc/clint@2000000"));
    CHECK(fdt_has_node(blob, "/soc/plic@c000000"));
    CHECK(fdt_has_node(blob, "/soc/serial@10000000"));
    CHECK(fdt_has_node(blob, "/soc/virtio_mmio@10001000"));
    CHECK(fdt_has_node(blob, "/soc/virtio_mmio@10008000"));
    CHECK(fdt_has_node(blob, "/chosen"));

    fdt_prop_found f = fdt_find(blob, "", "#address-cells");
    CHECK(f.found);
    CHECK_U64(f.len, 4);
    CHECK_U64(be32(f.data), 2);

    f = fdt_find(blob, "/memory@80000000", "reg");
    CHECK(f.found);
    CHECK_U64(f.len, 16);
    CHECK_U64(((u64)be32(f.data + 0) << 32) | be32(f.data + 4), RVM_RAM_BASE);
    CHECK_U64(((u64)be32(f.data + 8) << 32) | be32(f.data + 12), 1024ULL << 20);

    f = fdt_find(blob, "/cpus/cpu@0", "riscv,isa");
    CHECK(f.found);
    CHECK_STR((const char *)f.data, "rv64imafdc");

    f = fdt_find(blob, "/cpus/cpu@0", "mmu-type");
    CHECK(f.found);
    CHECK_STR((const char *)f.data, "riscv,sv57");

    f = fdt_find(blob, "/cpus", "timebase-frequency");
    CHECK(f.found);
    CHECK_U64(be32(f.data), CLINT_TIMEBASE_HZ);

    f = fdt_find(blob, "/chosen", "bootargs");
    CHECK(f.found);
    CHECK_STR((const char *)f.data, "console=ttyS0 root=/dev/vda");

    f = fdt_find(blob, "/chosen", "linux,initrd-end");
    CHECK(f.found);
    CHECK_U64(f.len, 8);

    f = fdt_find(blob, "/soc/serial@10000000", "interrupts");
    CHECK(f.found);
    CHECK_U64(be32(f.data), RVM_UART_IRQ);

    f = fdt_find(blob, "/soc/virtio_mmio@10001000", "interrupts");
    CHECK(f.found);
    CHECK_U64(be32(f.data), 1);

    f = fdt_find(blob, "/soc/virtio_mmio@10008000", "interrupts");
    CHECK(f.found);
    CHECK_U64(be32(f.data), 8);

    f = fdt_find(blob, "/soc/plic@c000000", "riscv,ndev");
    CHECK(f.found);
    CHECK_U64(be32(f.data), RVM_PLIC_MAX_SRC - 1);

    u32 off_strings = be32(blob + 12);
    u32 size_strings = be32(blob + 32);
    CHECK(off_strings + size_strings <= len);

    free(blob);
}
