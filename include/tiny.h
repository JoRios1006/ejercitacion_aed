#pragma once

/* ─── arch guard ─────────────────────────────────────────────────────────── */
#if !defined(__x86_64__)
#  error "tiny.h: x86-64 only"
#endif
#if !defined(__linux__)
#  error "tiny.h: Linux only"
#endif

/* ─── syscall numbers ────────────────────────────────────────────────────── */
#define SYS_WRITE 1
#define SYS_EXIT  60
#define STDOUT    1

_Static_assert(SYS_WRITE == 1,  "syscall ABI mismatch");
_Static_assert(SYS_EXIT  == 60, "syscall ABI mismatch");
_Static_assert(STDOUT    == 1,  "fd assumption broken");

/* ─── stack scratchpad ───────────────────────────────────────────────────── */
/*
 * STACK(off) yields a void* into the current frame at [rbp - off].
 * REQUIRES: caller has a valid frame pointer (rbp set up via push/mov).
 * Naked _start must do: pushq %rbp / movq %rsp, %rbp / subq $N, %rsp
 * before any STACK usage. -fno-omit-frame-pointer if in doubt.
 */
#define STACK(off) ((void*)((char*)__builtin_frame_address(0) - (off)))

/* ─── xmm scalar f64 ops ─────────────────────────────────────────────────── */
#define DEF(addr, val) ({                                                      \
    double _v = (val);                                                         \
    asm volatile(                                                              \
        "movsd %1, %%xmm0\n"                                                   \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "m"(_v) : "xmm0", "memory");                            \
})

#define MUL(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "mulsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define SUM(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "addsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define ROOT(out, a) ({                                                        \
    asm volatile(                                                              \
        "sqrtsd (%1), %%xmm0\n"                                                \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a) : "xmm0", "memory");                              \
})

/* ─── dtoa ───────────────────────────────────────────────────────────────── */
/*
 * Converts double x to decimal string in out[].
 * Integer part via x87 fbstp (BCD), fractional part via repeated *10.
 * Trailing fractional zeros stripped.
 * Returns byte length (no NUL terminator written).
 * out must be at least 32 bytes.
 */
static inline int dtoa(double x, char *out) {
    char *start = out;

    if (x < 0.0) { *out++ = '-'; x = -x; }

    unsigned long i = (unsigned long)x;
    double f = x - (double)i;

    /* integer digits via x87 BCD store — no soft division needed */
    unsigned char bcd[10];
    asm volatile("fildq %1\nfbstp %0" : "=m"(bcd) : "m"(i));

    int leading = 1;
    for (int j = 8; j >= 0; j--) {
        unsigned char hi = (bcd[j] >> 4) & 0xF;
        unsigned char lo =  bcd[j]       & 0xF;
        if (hi || !leading) { *out++ = '0' + hi; leading = 0; }
        if (lo || !leading) { *out++ = '0' + lo; leading = 0; }
    }
    if (leading) *out++ = '0';  /* x was 0 */

    /* fractional digits — up to 15, trailing zeros stripped */
    char frac[15];
    int flen = 0, last_nonzero = -1;
    for (int j = 0; j < 15; j++) {
        f *= 10.0;
        int d = (int)f;
        frac[j] = '0' + d;
        f -= (double)d;
        if (d) last_nonzero = j;
    }
    if (last_nonzero >= 0) {
        *out++ = '.';
        for (int j = 0; j <= last_nonzero; j++)
            *out++ = frac[j];
    }

    return (int)(out - start);
}

/* ─── print ──────────────────────────────────────────────────────────────── */
/*
 * PRINT(addr)    — write decimal repr of *(double*)addr to stdout
 * PRINTLN(addr)  — same + newline
 *
 * "r" constraints on buf/len: avoids "g" ambiguity where GCC might
 * select a memory operand the inline asm mov can't encode directly.
 */
#define _PRINT_IMPL(addr, nl) ({                                               \
    char _buf[32];                                                             \
    int  _len = dtoa(*(double*)(addr), _buf);                                  \
    if (nl) { _buf[_len++] = '\n'; }                                           \
    register long   _rax asm("rax") = SYS_WRITE;                              \
    register long   _rdi asm("rdi") = STDOUT;                                  \
    register char  *_rsi asm("rsi") = _buf;                                    \
    register long   _rdx asm("rdx") = _len;                                    \
    asm volatile("syscall"                                                     \
        : "+r"(_rax)                                                           \
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)                                     \
        : "rcx", "r11", "memory");                                             \
})

#define PRINT(addr)   _PRINT_IMPL(addr, 0)
#define PRINTLN(addr) _PRINT_IMPL(addr, 1)

/* ─── exit ───────────────────────────────────────────────────────────────── */
#define EXIT(code) ({                                                          \
    register long _rax asm("rax") = SYS_EXIT;                                 \
    register long _rdi asm("rdi") = (code);                                   \
    asm volatile("syscall" :: "r"(_rax), "r"(_rdi));                          \
    __builtin_unreachable();                                                   \
})
