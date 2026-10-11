#ifndef RVM_RVM_H
#define RVM_RVM_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define RVM_UNUSED(x) ((void)(x))

#define RVM_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define RVM_ALIGN_UP(v, a)   (((v) + ((a) - 1)) & ~((__typeof__(v))(a) - 1))
#define RVM_ALIGN_DOWN(v, a) ((v) & ~((__typeof__(v))(a) - 1))
#define RVM_IS_ALIGNED(v, a) (((v) & ((__typeof__(v))(a) - 1)) == 0)

#define RVM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RVM_MAX(a, b) ((a) > (b) ? (a) : (b))

#define PRV_U 0u
#define PRV_S 1u
#define PRV_H 2u
#define PRV_M 3u

#define RVM_RAM_BASE    0x80000000ULL
#define RVM_RAM_MIN     (8ULL << 20)
#define RVM_RAM_DEFAULT (1024ULL << 20)
#define RVM_RAM_MAX     (8ULL << 30)

#define RVM_CLINT_BASE 0x02000000ULL
#define RVM_CLINT_SIZE 0x00010000ULL
#define RVM_CLINT_IRQ  3

#define RVM_PLIC_BASE    0x0C000000ULL
#define RVM_PLIC_SIZE    0x04000000ULL
#define RVM_PLIC_MAX_SRC 64

#define RVM_UART_BASE 0x10000000ULL
#define RVM_UART_SIZE 0x00000100ULL
#define RVM_UART_IRQ  10

#define RVM_VIRTIO_BASE   0x10001000ULL
#define RVM_VIRTIO_STRIDE 0x00001000ULL
#define RVM_VIRTIO_SIZE   (RVM_VIRTIO_STRIDE * RVM_VIRTIO_COUNT)
#define RVM_VIRTIO_COUNT  8
#define RVM_VIRTIO_IRQ(i) (1u + (u32)(i))

#define RVM_DTB_DEFAULT_ADDR    0x82200000ULL
#define RVM_INITRD_DEFAULT_ADDR 0x84000000ULL

#define RVM_KERNEL_ENTRY 0x80200000ULL

typedef enum {
    RVM_OK = 0,
    RVM_ERR = -1,
    RVM_ERR_NOMEM = -2,
    RVM_ERR_IO = -3,
    RVM_ERR_RANGE = -4,
    RVM_ERR_BADARG = -5,
    RVM_ERR_UNSUPPORTED = -6,
    RVM_ERR_NOTFOUND = -7,
} rvm_err;

const char *rvm_strerror(rvm_err e);

typedef enum {
    RVM_LOG_TRACE = 0,
    RVM_LOG_DEBUG = 1,
    RVM_LOG_INFO = 2,
    RVM_LOG_WARN = 3,
    RVM_LOG_ERROR = 4,
    RVM_LOG_OFF = 5,
} rvm_loglevel;

void rvm_log_set_level(rvm_loglevel lvl);
rvm_loglevel rvm_log_get_level(void);

typedef void (*rvm_log_fn)(void *ud, rvm_loglevel lvl, const char *line);
void rvm_log_set_sink(rvm_log_fn fn, void *ud);
void rvm_log(rvm_loglevel lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void rvm_vlog(rvm_loglevel lvl, const char *fmt, va_list ap);

#define LOG_TRACE(...) rvm_log(RVM_LOG_TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) rvm_log(RVM_LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  rvm_log(RVM_LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...)  rvm_log(RVM_LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) rvm_log(RVM_LOG_ERROR, __VA_ARGS__)

typedef void (*rvm_trace_fn)(void *ud, u64 pc, u32 insn, u32 insn_len);
void rvm_trace_set(rvm_trace_fn fn, void *ud);
rvm_trace_fn rvm_trace_get(void **ud);

void rvm_trace_from(u64 pc);
bool rvm_trace_armed(u64 pc);

u64 rvm_now_ns(void);

#endif
