#ifndef RVM_SBI_H
#define RVM_SBI_H

#include "../cpu/cpu.h"

#define SBI_EXT_BASE 0x10ULL
#define SBI_EXT_TIME 0x54494D45ULL
#define SBI_EXT_IPI  0x735049ULL
#define SBI_EXT_RFNC 0x52464E43ULL
#define SBI_EXT_SRST 0x53525354ULL
#define SBI_EXT_DBCN 0x4442434EULL

#define SBI_LEGACY_CONSOLE_PUTCHAR 0x01ULL

#define SBI_SUCCESS             0
#define SBI_ERR_FAILED          -1
#define SBI_ERR_NOT_SUPPORTED   -2
#define SBI_ERR_INVALID_PARAM   -3
#define SBI_ERR_DENIED          -4
#define SBI_ERR_INVALID_ADDRESS -5

#define SBI_SRST_SHUTDOWN 0
#define SBI_SRST_REBOOT   1

typedef struct sbi {
    bus *bus;
    void (*write)(void *ud, const u8 *buf, size_t n);
    void *write_ud;
    bool (*shutdown)(void *ud, u32 type, u32 reason);
    void *shutdown_ud;
    void (*set_timer)(void *ud, u64 stime);
    void *set_timer_ud;

    u64 n_calls, n_unsupported;
} sbi;

void sbi_init(sbi *s, bus *b);

bool sbi_handle(sbi *s, cpu *c);

#endif
