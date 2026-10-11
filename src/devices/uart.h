#ifndef RVM_UART_H
#define RVM_UART_H

#include "../rvm.h"

#define UART_RX_SIZE 4096

#define UART_RBR 0
#define UART_THR 0
#define UART_DLL 0
#define UART_IER 1
#define UART_DLM 1
#define UART_IIR 2
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5
#define UART_MSR 6
#define UART_SCR 7

#define LSR_DR   0x01
#define LSR_OE   0x02
#define LSR_THRE 0x20
#define LSR_TEMT 0x40

#define IER_RDA  0x01
#define IER_THRE 0x02

typedef struct uart {
    u8 ier, fcr, lcr, mcr, msr, scr, dll, dlm;

    u8 rx[UART_RX_SIZE];
    u32 rx_head;
    u32 rx_count;

    u32 irq;
    bool irq_level;

    void (*raise)(void *ud, u32 irq, bool level);
    void *raise_ud;

    void (*emit)(void *ud, u8 ch);
    void *emit_ud;

    u64 n_tx, n_rx;
} uart;

rvm_err uart_init(uart *u, u32 irq);
bool uart_load(void *dev, u64 off, u32 size, u64 *out);
bool uart_store(void *dev, u64 off, u32 size, u64 val);

bool uart_push(uart *u, u8 ch);
static inline u32 uart_rx_space(const uart *u) {
    return UART_RX_SIZE - u->rx_count;
}
void uart_set_sink(uart *u, void (*emit)(void *ud, u8 ch), void *ud);
void uart_set_irq(uart *u, void (*raise)(void *ud, u32 irq, bool level), void *ud);

#endif
