#ifndef RVM_UI_H
#define RVM_UI_H

#include "../vm/vm.h"

typedef struct ui_stdio {
    bool raw_mode;
    bool ok;
    void *saved;
} ui_stdio;

rvm_err ui_stdio_init(ui_stdio *u);
void ui_stdio_shutdown(ui_stdio *u);

void ui_stdio_attach(vm_opts *o, ui_stdio *u);

int ui_stdio_poll(void *ud, u8 *buf, size_t max);
void ui_stdio_write(void *ud, const u8 *buf, size_t n);

#endif
