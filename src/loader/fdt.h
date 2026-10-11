#ifndef RVM_FDT_H
#define RVM_FDT_H

#include "../rvm.h"

#define FDT_MAGIC 0xD00DFEEDu

typedef struct fdt {
    u8 *structs;
    u32 structs_len, structs_cap;
    u8 *strings;
    u32 strings_len, strings_cap;
    u32 depth;
    bool error;
} fdt;

rvm_err fdt_init(fdt *f);
void fdt_free(fdt *f);

u32 fdt_intern(fdt *f, const char *name);

rvm_err fdt_begin_node(fdt *f, const char *name);
rvm_err fdt_end_node(fdt *f);
rvm_err fdt_prop_empty(fdt *f, const char *name);
rvm_err fdt_prop_u32(fdt *f, const char *name, u32 v);
rvm_err fdt_prop_u64(fdt *f, const char *name, u64 v);
rvm_err fdt_prop_cells(fdt *f, const char *name, const u32 *cells, u32 n);
rvm_err fdt_prop_reg64(fdt *f, const char *name, u64 base, u64 size);
rvm_err fdt_prop_str(fdt *f, const char *name, const char *s);
rvm_err fdt_prop_strlist(fdt *f, const char *name, const char *const *strs, u32 n);

rvm_err fdt_finish(fdt *f, u8 **out, u32 *out_len);

#endif
