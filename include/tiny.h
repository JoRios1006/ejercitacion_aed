#pragma once

/* ─── arch guard ─────────────────────────────────────────────────────────── */
#if !defined(__x86_64__)
#  error "tiny.h: x86-64 only"
#endif
#if !defined(__linux__)
#  error "tiny.h: Linux only"
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TINY.H — no-libc x86-64 Linux toolkit
 *
 * Philosophy: data as truth, code as pointer arithmetic.
 *             no branches where masks suffice.
 *             no libc, no headers, no runtime.
 *
 * Sections:
 *   1.  syscall numbers
 *   2.  alignment & pointer utils
 *   3.  stack frame helpers
 *   4.  xmm scalar f64 arithmetic
 *   5.  f64 comparisons → double flag
 *   6.  f64 control flow
 *   7.  f64 array helpers
 *   8.  integer bit ops
 *   9.  string & memory ops
 *  10.  dtoa / atof / atoi
 *  11.  I/O  (print f64, int, str / read)
 *  12.  brk / slab allocator
 *  13.  tiny_str_t  (German-style SSO string)
 * ═══════════════════════════════════════════════════════════════════════════ */


/* ─── 1. syscall numbers ─────────────────────────────────────────────────── */
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_EXIT  60
#define SYS_BRK   12
#define STDIN     0
#define STDOUT    1

_Static_assert(SYS_READ  == 0,  "syscall ABI mismatch");
_Static_assert(SYS_WRITE == 1,  "syscall ABI mismatch");
_Static_assert(SYS_EXIT  == 60, "syscall ABI mismatch");
_Static_assert(STDOUT    == 1,  "fd assumption broken");


/* ─── 2. alignment & pointer utils ──────────────────────────────────────── */
#define ALIGN_UP(s)         (((s) + 7) & ~7)   /* round up to 8-byte boundary   */
#define ALIGN_DOWN(s)       ((s) & ~7)          /* round down to 8-byte boundary */

#define OFFSET(ptr, bytes)  ((void*)((char*)(ptr) + (bytes)))
#define INDEX(base, i)      ((void*)((double*)(base) + (i)))
#define DEREF(addr)         (*(double*)(addr))


/* ─── 3. stack frame helpers ─────────────────────────────────────────────── */
/*
 * STACK(off) — void* to [rbp - off] in current frame.
 * REQUIRES valid frame pointer (-fno-omit-frame-pointer).
 * Use inside non-naked fns only (naked fns have no GCC-managed frame).
 */
#define STACK(off) ((void*)((char*)__builtin_frame_address(0) - (off)))

#define PUSH_FRAME(n) \
    asm volatile(                \
        "pushq %%rbp\n"          \
        "movq  %%rsp, %%rbp\n"   \
        "subq  %0, %%rsp"        \
        :: "i"(n) : "rsp", "rbp")

#define POP_FRAME \
    asm volatile(                \
        "movq %%rbp, %%rsp\n"    \
        "popq %%rbp"             \
        ::: "rsp", "rbp")


/* ─── 4. xmm scalar f64 arithmetic ──────────────────────────────────────── */
/*
 * All ops take void* addresses pointing to doubles on the stack/heap.
 * DEF  — *addr  = literal double
 * COPY — *dst   = *src
 * SWAP — swap *a <-> *b           (xmm0/xmm1, no stack temp)
 * INC  — *addr += 1.0
 * DEC  — *addr -= 1.0
 * SUM  — *out   = *a + *b
 * SUB  — *out   = *a - *b
 * MUL  — *out   = *a * *b
 * DIV  — *out   = *a / *b
 * MOD  — *out   = fmod(*a, *b)    (x87 fprem1, no libm)
 * ROOT — *out   = sqrt(*a)        (sqrtsd)
 * ABS  — *out   = |*a|            (andpd sign mask, branchless)
 * NEG  — *out   = -*a             (xorpd sign mask, branchless)
 * MIN  — *out   = min(*a, *b)     (minsd)
 * MAX  — *out   = max(*a, *b)     (maxsd)
 * AVG  — *out   = (*a + *b) * 0.5 (xmm, no overflow)
 */
#define DEF(addr, val) ({                                                      \
    double _v = (val);                                                         \
    asm volatile(                                                              \
        "movsd %1, %%xmm0\n"                                                   \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "m"(_v) : "xmm0", "memory");                            \
})

#define COPY(dst, src) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(dst), "r"(src) : "xmm0", "memory");                            \
})

#define SWAP(a, b) ({                                                          \
    asm volatile(                                                              \
        "movsd (%0), %%xmm0\n"                                                 \
        "movsd (%1), %%xmm1\n"                                                 \
        "movsd %%xmm1, (%0)\n"                                                 \
        "movsd %%xmm0, (%1)"                                                   \
        :: "r"(a), "r"(b) : "xmm0", "xmm1", "memory");                        \
})

#define INC(addr) ({                                                           \
    static const double _one = 1.0;                                            \
    asm volatile(                                                              \
        "movsd (%0),   %%xmm0\n"                                               \
        "addsd (%1),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "r"(&_one) : "xmm0", "memory");                         \
})

#define DEC(addr) ({                                                           \
    static const double _one = 1.0;                                            \
    asm volatile(                                                              \
        "movsd (%0),   %%xmm0\n"                                               \
        "subsd (%1),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(addr), "r"(&_one) : "xmm0", "memory");                         \
})

#define SUM(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "addsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define SUB(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "subsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define MUL(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "mulsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define DIV(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "divsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

/* MOD via x87 fprem1 — iterates until C2 flag clears (full remainder done) */
#define MOD(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "fldl   (%2)\n"                                                        \
        "fldl   (%1)\n"                                                        \
        "1: fprem1\n"                                                          \
        "fnstsw %%ax\n"                                                        \
        "testb  $4, %%ah\n"                                                    \
        "jnz    1b\n"                                                          \
        "fstpl  (%0)\n"                                                        \
        "fstp   %%st(0)"                                                       \
        :: "r"(out), "r"(a), "r"(b) : "ax", "memory");                        \
})

#define ROOT(out, a) ({                                                        \
    asm volatile(                                                              \
        "sqrtsd (%1), %%xmm0\n"                                                \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a) : "xmm0", "memory");                              \
})

/* ABS: clear IEEE 754 sign bit via andpd with 0x7FFFFFFFFFFFFFFF mask */
#define ABS(out, a) ({                                                         \
    static const long long _absmask = 0x7FFFFFFFFFFFFFFFLL;                   \
    asm volatile(                                                              \
        "movsd  (%1),   %%xmm0\n"                                              \
        "andpd  (%2),   %%xmm0\n"                                              \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a), "r"(&_absmask) : "xmm0", "memory");              \
})

/* NEG: flip IEEE 754 sign bit via xorpd with 0x8000000000000000 mask */
#define NEG(out, a) ({                                                         \
    static const long long _negmask = (long long)0x8000000000000000LL;        \
    asm volatile(                                                              \
        "movsd  (%1),   %%xmm0\n"                                              \
        "xorpd  (%2),   %%xmm0\n"                                              \
        "movsd  %%xmm0, (%0)"                                                  \
        :: "r"(out), "r"(a), "r"(&_negmask) : "xmm0", "memory");              \
})

#define MIN(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "minsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define MAX(out, a, b) ({                                                      \
    asm volatile(                                                              \
        "movsd (%1), %%xmm0\n"                                                 \
        "maxsd (%2), %%xmm0\n"                                                 \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b) : "xmm0", "memory");                      \
})

#define AVG(out, a, b) ({                                                      \
    static const double _half = 0.5;                                           \
    asm volatile(                                                              \
        "movsd (%1),   %%xmm0\n"                                               \
        "addsd (%2),   %%xmm0\n"                                               \
        "mulsd (%3),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(b), "r"(&_half) : "xmm0", "memory");         \
})


/* ─── 5. f64 comparisons -> double flag ─────────────────────────────────── */
/*
 * Each macro stores 1.0 (true) or 0.0 (false) at out.
 * Uses ucomisd — NaN-safe (NaN comparisons always yield false).
 *
 * IS_GT(out,a,b) — *a >  *b
 * IS_LT(out,a,b) — *a <  *b
 * IS_GE(out,a,b) — *a >= *b
 * IS_LE(out,a,b) — *a <= *b
 * IS_EQ(out,a,b) — *a == *b
 * IS_NE(out,a,b) — *a != *b
 * CMP(out,a,b)   — -1.0 / 0.0 / 1.0  (less / equal / greater)
 */
#define _CMP_IMPL(out, a, b, setcc) ({                                         \
    static const double _t = 1.0, _f = 0.0;                                   \
    asm volatile(                                                              \
        "movsd   (%1),   %%xmm0\n"                                             \
        "ucomisd (%2),   %%xmm0\n"                                             \
        setcc "  %%al\n"                                                       \
        "testb   %%al,   %%al\n"                                               \
        "jz      1f\n"                                                         \
        "movsd   (%3),   %%xmm1\n"                                             \
        "jmp     2f\n"                                                         \
        "1: movsd (%4),  %%xmm1\n"                                             \
        "2: movsd %%xmm1, (%0)"                                                \
        :: "r"(out), "r"(a), "r"(b), "r"(&_t), "r"(&_f)                       \
        : "xmm0", "xmm1", "rax", "memory");                                   \
})

#define IS_GT(out, a, b)  _CMP_IMPL(out, a, b, "seta")
#define IS_LT(out, a, b)  _CMP_IMPL(out, a, b, "setb")
#define IS_GE(out, a, b)  _CMP_IMPL(out, a, b, "setae")
#define IS_LE(out, a, b)  _CMP_IMPL(out, a, b, "setbe")
#define IS_EQ(out, a, b)  _CMP_IMPL(out, a, b, "sete")
#define IS_NE(out, a, b)  _CMP_IMPL(out, a, b, "setne")

/* CMP: stores -1.0 / 0.0 / 1.0 — useful as sort comparator */
#define CMP(out, a, b) ({                                                      \
    static const double _neg = -1.0, _zer = 0.0, _pos = 1.0;                  \
    asm volatile(                                                              \
        "movsd   (%1),   %%xmm0\n"                                             \
        "ucomisd (%2),   %%xmm0\n"                                             \
        "movsd   (%5),   %%xmm1\n"                                             \
        "ja      1f\n"                                                         \
        "jb      2f\n"                                                         \
        "jmp     3f\n"                                                         \
        "1: movsd (%4),  %%xmm1\n jmp 3f\n"                                   \
        "2: movsd (%3),  %%xmm1\n"                                             \
        "3: movsd %%xmm1, (%0)"                                                \
        :: "r"(out), "r"(a), "r"(b),                                           \
           "r"(&_neg), "r"(&_pos), "r"(&_zer)                                  \
        : "xmm0", "xmm1", "memory");                                           \
})


/* ─── 6. f64 control flow ────────────────────────────────────────────────── */
/*
 * FOR_RANGE(idx_addr, start, end)
 *   — idx_addr: void* to a double on the stack used as loop counter
 *   — iterates while *idx < end, INC each iteration
 *   — use: FOR_RANGE(STACK(8), 0.0, 10.0) { ... }
 *
 * FOR_DOWN(idx_addr, start, end)
 *   — iterates while *idx > end, DEC each iteration
 *
 * WHILE_NZ(flag_addr)
 *   — loops while *(double*)flag_addr != 0.0
 *   — set to 0.0 inside body to exit
 *
 * IF_GT/LT/GE/LE/EQ/NE(a, b) { ... }
 *   — conditional block on two void* addr values via DEREF
 */
#define FOR_RANGE(idx_addr, start, end) \
    for (DEF((idx_addr), (start));      \
         DEREF(idx_addr) < (end);       \
         INC(idx_addr))

#define FOR_DOWN(idx_addr, start, end)  \
    for (DEF((idx_addr), (start));      \
         DEREF(idx_addr) > (end);       \
         DEC(idx_addr))

#define WHILE_NZ(flag_addr) \
    while (DEREF(flag_addr) != 0.0)

#define IF_GT(a, b)  if (DEREF(a) >  DEREF(b))
#define IF_LT(a, b)  if (DEREF(a) <  DEREF(b))
#define IF_GE(a, b)  if (DEREF(a) >= DEREF(b))
#define IF_LE(a, b)  if (DEREF(a) <= DEREF(b))
#define IF_EQ(a, b)  if (DEREF(a) == DEREF(b))
#define IF_NE(a, b)  if (DEREF(a) != DEREF(b))


/* ─── 7. f64 array helpers ───────────────────────────────────────────────── */
/*
 * FILL(base, count, val) — set count doubles starting at base to val
 * ZERO(addr)             — zero one double slot
 * ARRAY_GET(base,i,dst)  — dst = base[i]
 * ARRAY_SET(base,i,src)  — base[i] = src
 */
#define FILL(base, count, val) ({                                              \
    double  _fv = (double)(val);                                               \
    double *_fp = (double*)(base);                                             \
    for (long _fi = 0; _fi < (long)(count); _fi++) _fp[_fi] = _fv;            \
})

#define ZERO(addr)              DEF((addr), 0.0)
#define ARRAY_GET(base, i, dst) COPY((dst),            INDEX((base), (i)))
#define ARRAY_SET(base, i, src) COPY(INDEX((base),(i)), (src))


/* ─── 8. integer bit ops ─────────────────────────────────────────────────── */
/*
 * All operate on unsigned long (64-bit) unless noted.
 * Branchless where possible — masks, not jumps.
 *
 * IABS(x)          — |x|, no branch. mask = x>>63; (x+mask)^mask
 * IMIN(x,y)        — branchless min
 * IMAX(x,y)        — branchless max
 * IAVG(x,y)        — (x&y)+((x^y)>>1), no overflow
 * IS_POW2(x)       — 1 if x is power of 2 (x > 0 required)
 * IS_OPP_SIGN(x,y) — 1 if x and y have opposite signs
 * NEXT_POW2(v)     — next power of 2 >= v (32-bit)
 * POPCOUNT(v)      — count set bits, parallel method (32-bit, 12 ops)
 * TRAILING_ZEROS(v)— trailing zero count, DeBruijn multiply (32-bit, 4 ops)
 * MERGE(a,b,mask)  — (a&~mask)|(b&mask), branchless bit-select
 * BIT_SET(x,n)     — set bit n
 * BIT_CLR(x,n)     — clear bit n
 * BIT_TST(x,n)     — test bit n (nonzero if set)
 * BIT_FLP(x,n)     — flip bit n
 * COND_SET(w,m,f)  — if f: w|=m else w&=~m, branchless
 */
#define IABS(x)           ({ long _x=(long)(x); long _m=_x>>63; (_x+_m)^_m; })
#define IMIN(x,y)         ({ long _x=(long)(x),_y=(long)(y); _y^((_x^_y)&-((_x)<(_y))); })
#define IMAX(x,y)         ({ long _x=(long)(x),_y=(long)(y); _x^((_x^_y)&-((_x)<(_y))); })
#define IAVG(x,y)         ({ unsigned long _x=(x),_y=(y); (_x&_y)+((_x^_y)>>1); })
#define IS_POW2(x)        ({ unsigned long _x=(x); (_x && !(_x&(_x-1))); })
#define IS_OPP_SIGN(x,y)  (((long)(x)^(long)(y)) < 0)
#define MERGE(a,b,mask)   ((a)^(((a)^(b))&(mask)))
#define BIT_SET(x,n)      ((x) |=  (1UL<<(n)))
#define BIT_CLR(x,n)      ((x) &= ~(1UL<<(n)))
#define BIT_TST(x,n)      ((x) &   (1UL<<(n)))
#define BIT_FLP(x,n)      ((x) ^=  (1UL<<(n)))
#define COND_SET(w,m,f)   ((w) = ((w)&~(m))|(-(unsigned long)(f)&(m)))

/* NEXT_POW2 — round 32-bit v up to next power of 2 (0 stays 0) */
#define NEXT_POW2(v) ({                                                        \
    unsigned int _v = (unsigned int)(v);                                       \
    _v--; _v|=_v>>1; _v|=_v>>2; _v|=_v>>4; _v|=_v>>8; _v|=_v>>16; _v++;     \
    _v;                                                                        \
})

/*
 * POPCOUNT — parallel Hamming weight, 32-bit.
 * 12 ops, no table, no 64-bit multiply needed.
 */
#define POPCOUNT(v) ({                                                         \
    unsigned int _v = (unsigned int)(v);                                       \
    _v  = _v - ((_v >> 1) & 0x55555555U);                                     \
    _v  = (_v & 0x33333333U) + ((_v >> 2) & 0x33333333U);                     \
    _v  = ((_v + (_v >> 4)) & 0x0F0F0F0FU);                                   \
    (int)((_v * 0x01010101U) >> 24);                                           \
})

/*
 * TRAILING_ZEROS — count trailing zero bits, 32-bit.
 * DeBruijn multiply + lookup: 4 ops, no branch.
 * Returns 32 if v == 0.
 */
#define TRAILING_ZEROS(v) ({                                                   \
    static const int _db32[32] = {                                             \
         0, 1,28, 2,29,14,24, 3,30,22,20,15,25,17, 4, 8,                      \
        31,27,13,23,21,19,16, 7,26,12,18, 6,11, 5,10, 9                        \
    };                                                                         \
    unsigned int _v = (unsigned int)(v);                                       \
    _v ? _db32[((unsigned int)((_v & -_v) * 0x077CB531U)) >> 27] : 32;        \
})


/* ─── 9. string & memory ops ─────────────────────────────────────────────── */
/*
 * STRLEN(s)
 *   — length of NUL-terminated string via repne scasb.
 *     One asm instruction, no C loop.
 *
 * MEM_COPY(dst, src, n)
 *   — copy n bytes via rep movsb.
 *     No alignment requirement. Use to move data between STACK and slab.
 *
 * NTH(arr, idx, len)
 *   — safe indexed access into any pointer array (char**, void**, etc.)
 *   — returns arr[idx] if 0 <= idx < len, else NULL.
 *   — NULL = explicit "no value" sentinel. 0 is a valid index, NULL is not.
 *   — unsigned cast trick: negative idx wraps to huge value -> OOB -> NULL.
 *     Single comparison, no branch at call site with -O2.
 *
 * FIND_IF(base, count, stride, pred)
 *   — linear scan. Returns void* to first element where pred(ptr) != 0.
 *   — Returns NULL if not found.
 *   — stride = sizeof(element), pred = int(*)(void*)
 *
 * Example — day lookup (your Python one-liner, in C, no libc):
 *
 *   static const char *_semana[] = {
 *       "LUNES","MARTES","MIERCOLES","JUEVES","VIERNES","SABADO","DOMINGO"
 *   };
 *   char buf[16]; READ(buf, sizeof(buf));
 *   long n         = tiny_atoi(buf, NULL) - 1;
 *   const char *dia = NTH(_semana, n, 7);
 *   if (dia) PRINTLN_STR(dia, STRLEN(dia));
 *   else     PRINTLN_STR("ERROR", 5);
 */

static inline long tiny_strlen(const char *s) {
    long len;
    asm volatile(
        "cld\n"
        "xorb  %%al,  %%al\n"      /* search for NUL */
        "movq  $-1,   %%rcx\n"     /* max scan length */
        "repne scasb\n"            /* scan until [rdi] == 0 */
        "notq  %%rcx\n"
        "decq  %%rcx"              /* rcx = length excluding NUL */
        : "=c"(len)
        : "D"(s)
        : "rax", "memory");
    return len;
}
#define STRLEN(s) tiny_strlen(s)

static inline void tiny_memcopy(void *dst, const void *src, long n) {
    asm volatile(
        "cld\n"
        "rep movsb"
        : "+D"(dst), "+S"(src), "+c"(n)
        :: "memory");
}
#define MEM_COPY(dst, src, n) tiny_memcopy((dst), (src), (n))

#define NTH(arr, idx, len) \
    ((unsigned long)(idx) < (unsigned long)(len) ? (arr)[(idx)] : (void*)0)

static inline void *tiny_find_if(void *base, long count, long stride,
                                  int (*pred)(void *)) {
    char *p = (char *)base;
    for (long i = 0; i < count; i++, p += stride)
        if (pred(p)) return (void *)p;
    return (void *)0;
}
#define FIND_IF(base, count, stride, pred) \
    tiny_find_if((base), (count), (stride), (pred))


/* ─── 10. dtoa / atof / atoi ─────────────────────────────────────────────── */
/*
 * dtoa(x, out)          — double -> decimal string. No NUL. Returns length.
 *                         out must be >= 32 bytes.
 *                         Integer part via x87 fbstp (BCD). Trailing zeros stripped.
 * tiny_atof(s, end_ptr) — decimal string -> double. Skips leading whitespace.
 * tiny_atoi(s, end_ptr) — decimal string -> long.  Skips leading whitespace.
 *                         Both set *end_ptr to first unparsed char (pass NULL to ignore).
 */
static inline int dtoa(double x, char *out) {
    char *start = out;
    if (x < 0.0) { *out++ = '-'; x = -x; }
    unsigned long i = (unsigned long)x;
    double f = x - (double)i;
    unsigned char bcd[10];
    asm volatile("fildq %1\nfbstp %0" : "=m"(bcd) : "m"(i));
    int leading = 1;
    for (int j = 8; j >= 0; j--) {
        unsigned char hi = (bcd[j] >> 4) & 0xF;
        unsigned char lo =  bcd[j]       & 0xF;
        if (hi || !leading) { *out++ = '0' + hi; leading = 0; }
        if (lo || !leading) { *out++ = '0' + lo; leading = 0; }
    }
    if (leading) *out++ = '0';
    char frac[15]; int last_nonzero = -1;
    for (int j = 0; j < 15; j++) {
        f *= 10.0; int d = (int)f;
        frac[j] = '0' + d; f -= (double)d;
        if (d) last_nonzero = j;
    }
    if (last_nonzero >= 0) {
        *out++ = '.';
        for (int j = 0; j <= last_nonzero; j++) *out++ = frac[j];
    }
    return (int)(out - start);
}

static inline double tiny_atof(const char *s, const char **end_ptr) {
    double result = 0.0, frac = 1.0; int neg = 0, in_frac = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { neg = 1; s++; }
    for (; *s; s++) {
        if      (*s >= '0' && *s <= '9') {
            if (in_frac) { frac *= 0.1; result += (*s - '0') * frac; }
            else         { result = result * 10.0 + (*s - '0'); }
        } else if (*s == '.') { in_frac = 1; }
        else break;
    }
    if (end_ptr) *end_ptr = s;
    return neg ? -result : result;
}

static inline long tiny_atoi(const char *s, const char **end_ptr) {
    long result = 0; int neg = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    if (end_ptr) *end_ptr = s;
    return neg ? -result : result;
}


/* ─── 11. I/O ────────────────────────────────────────────────────────────── */
/*
 * READ(buf,len)          — read stdin -> buf. Returns bytes read (int).
 * PRINT(addr)            — print *(double*)addr
 * PRINTLN(addr)          — print *(double*)addr + newline
 * PRINT_INT(n)           — print signed long
 * PRINTLN_INT(n)         — print signed long + newline
 * PRINT_STR(ptr,len)     — write raw bytes  (use STRLEN for len)
 * PRINTLN_STR(ptr,len)   — write raw bytes + newline
 * EXIT(code)             — _exit syscall, noreturn
 */
#define READ(buf, len) ({                           \
    register long  _rax asm("rax") = SYS_READ;     \
    register long  _rdi asm("rdi") = STDIN;         \
    register char *_rsi asm("rsi") = (char*)(buf);  \
    register long  _rdx asm("rdx") = (long)(len);   \
    asm volatile("syscall"                          \
        : "+r"(_rax)                               \
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)         \
        : "rcx", "r11", "memory");                 \
    (int)_rax;                                     \
})

#define _WRITE_IMPL(ptr, len) ({                                               \
    register long   _rax asm("rax") = SYS_WRITE;                              \
    register long   _rdi asm("rdi") = STDOUT;                                  \
    register char  *_rsi asm("rsi") = (char*)(ptr);                            \
    register long   _rdx asm("rdx") = (long)(len);                             \
    asm volatile("syscall"                                                     \
        : "+r"(_rax)                                                           \
        : "r"(_rdi), "r"(_rsi), "r"(_rdx)                                     \
        : "rcx", "r11", "memory");                                             \
})

#define _PRINT_IMPL(addr, nl) ({                                               \
    char _buf[32]; int _len = dtoa(*(double*)(addr), _buf);                    \
    if (nl) { _buf[_len++] = '\n'; }                                           \
    _WRITE_IMPL(_buf, _len);                                                   \
})
#define PRINT(addr)   _PRINT_IMPL(addr, 0)
#define PRINTLN(addr) _PRINT_IMPL(addr, 1)

/* integer -> decimal via x87 BCD, same engine as dtoa, integer part only */
static inline int _itoa(long x, char *out) {
    char *start = out;
    if (x < 0) { *out++ = '-'; x = -x; }
    unsigned long u = (unsigned long)x;
    unsigned char bcd[10];
    asm volatile("fildq %1\nfbstp %0" : "=m"(bcd) : "m"(u));
    int leading = 1;
    for (int j = 8; j >= 0; j--) {
        unsigned char hi = (bcd[j] >> 4) & 0xF;
        unsigned char lo =  bcd[j]       & 0xF;
        if (hi || !leading) { *out++ = '0' + hi; leading = 0; }
        if (lo || !leading) { *out++ = '0' + lo; leading = 0; }
    }
    if (leading) *out++ = '0';
    return (int)(out - start);
}

#define _PRINT_INT_IMPL(n, nl) ({                                              \
    char _ibuf[24]; int _ilen = _itoa((long)(n), _ibuf);                       \
    if (nl) { _ibuf[_ilen++] = '\n'; }                                         \
    _WRITE_IMPL(_ibuf, _ilen);                                                 \
})
#define PRINT_INT(n)   _PRINT_INT_IMPL(n, 0)
#define PRINTLN_INT(n) _PRINT_INT_IMPL(n, 1)

#define _PRINT_STR_IMPL(ptr, len, nl) ({                                       \
    _WRITE_IMPL((ptr), (len));                                                 \
    if (nl) { char _nl = '\n'; _WRITE_IMPL(&_nl, 1); }                        \
})
#define PRINT_STR(ptr, len)   _PRINT_STR_IMPL(ptr, len, 0)
#define PRINTLN_STR(ptr, len) _PRINT_STR_IMPL(ptr, len, 1)

#define EXIT(code) ({                                                          \
    register long _rax asm("rax") = SYS_EXIT;                                 \
    register long _rdi asm("rdi") = (code);                                   \
    asm volatile("syscall" :: "r"(_rax), "r"(_rdi));                          \
    __builtin_unreachable();                                                   \
})


/* ─── 12. brk / slab allocator ───────────────────────────────────────────── */
/*
 * tiny_brk(addr)  — raw brk syscall. Pass NULL to query current brk.
 * BRK_GET()       — query current program break
 * BRK_SET(addr)   — set program break
 * tiny_sbrk(n)    — grow heap by n bytes. Returns old brk (base of new region).
 *                   Returns (void*)-1 on failure.
 *
 * SlabPool — O(1) fixed-size allocator on top of brk heap.
 *   slab_init(pool, slot_size, total_slots)
 *     — one brk call total. slot_size clamped to sizeof(SlabSlot) min,
 *       then ALIGN_UP'd so xmm doubles stored in slots stay aligned.
 *     — builds free list in O(n).
 *   slab_alloc(pool) — pop head. O(1). Returns NULL if exhausted.
 *   slab_free(pool, ptr) — push back. O(1). No bounds check.
 */
static inline void *tiny_brk(void *addr) {
    register long  _rax asm("rax") = SYS_BRK;
    register void *_rdi asm("rdi") = addr;
    asm volatile("syscall"
        : "+r"(_rax) : "r"(_rdi) : "rcx", "r11", "memory");
    return (void *)_rax;
}

#define BRK_GET()     tiny_brk((void*)0)
#define BRK_SET(addr) tiny_brk((void*)(addr))

static inline void *tiny_sbrk(long n) {
    void *cur = BRK_GET();
    void *new = BRK_SET((char*)cur + n);
    return (new == cur) ? (void*)-1L : cur;
}

typedef struct SlabSlot { struct SlabSlot *next; } SlabSlot;

typedef struct {
    void     *mem_base;
    long      slot_size;
    long      total_slots;
    SlabSlot *free_list;
} SlabPool;

static inline int slab_init(SlabPool *pool, long slot_size, long total_slots) {
    if (slot_size < (long)sizeof(SlabSlot)) slot_size = sizeof(SlabSlot);
    slot_size = ALIGN_UP(slot_size);
    long  bytes = slot_size * total_slots;
    void *base  = tiny_sbrk(bytes);
    if (base == (void*)-1L) return -1;
    pool->mem_base    = base;
    pool->slot_size   = slot_size;
    pool->total_slots = total_slots;
    pool->free_list   = (SlabSlot*)0;
    char *p = (char*)base + bytes - slot_size;
    for (long i = 0; i < total_slots; i++, p -= slot_size) {
        SlabSlot *s     = (SlabSlot*)p;
        s->next         = pool->free_list;
        pool->free_list = s;
    }
    return 0;
}

static inline void *slab_alloc(SlabPool *pool) {
    SlabSlot *s = pool->free_list;
    if (!s) return (void*)0;
    pool->free_list = s->next;
    return (void*)s;
}

static inline void slab_free(SlabPool *pool, void *ptr) {
    SlabSlot *s     = (SlabSlot*)ptr;
    s->next         = pool->free_list;
    pool->free_list = s;
}


/* ─── 13. tiny_str_t  (German-style SSO string) ──────────────────────────
 *
 * Layout (16 bytes total, always):
 *
 *   Inlined  (built with S()):
 *     [ uint32_t len (bit31=0) | char data[12] ]
 *
 *   Pointer  (built with S_PTR() / S_VIEW() / STR_SLICE()):
 *     [ uint32_t len (bit31=1) | char prefix[4] | char *ptr ]
 *       bit31 of length = HEAP flag. Real length = len & 0x7FFFFFFF.
 *       prefix = first min(4,len) bytes, for fast EQ rejection.
 *       ptr    = borrowed pointer, not owned.
 *
 * Why bit31 flag? Disambiguates inline vs heap regardless of string length.
 *   Previous design used len<=12 as the discriminant — broken for short
 *   heap strings (slice, S_PTR of 1-char string, etc).
 * Why prefix? Fast rejection in STR_EQ without a cache miss on ptr.
 * No NUL stored. Length always explicit.
 * No alloc. No ownership. Caller manages pointer lifetime.
 * ─────────────────────────────────────────────────────────────────────────── */

#include <stdint.h>

#define _TINY_STR_HEAP_FLAG  ((uint32_t)0x80000000u)
#define _TINY_STR_LEN_MASK   ((uint32_t)0x7FFFFFFFu)

typedef struct {
    union {
        struct {
            uint32_t length;        /* bit31=0 → inline */
            char     data[12];
        } inlined;
        struct {
            uint32_t length;        /* bit31=1 → heap */
            char     prefix[4];
            char    *ptr;
        } heap;
    };
} tiny_str_t;

/* ── discriminant & accessors ── */
#define STR_IS_INLINED(s)   (!((s).inlined.length & _TINY_STR_HEAP_FLAG))
#define STR_LEN(s)          ((s).inlined.length & _TINY_STR_LEN_MASK)
#define STR_DATA(s) \
    (STR_IS_INLINED(s) ? (const char*)(s).inlined.data \
                       : (const char*)(s).heap.ptr)

/* ── compile-time literal constructor (inline path) ──
 * Hard error if literal > 12 chars. Use S_PTR() for longer strings.
 * bit31 left 0 — inline flag.
 */
#define S(lit) (__extension__({                                                \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "S(): literal exceeds 12 chars max inline. Use S_PTR().");             \
    (tiny_str_t){ .inlined = {                                                 \
        .length = (uint32_t)(sizeof(lit)-1),   /* bit31=0, always inline */    \
        .data   = lit                                                          \
    }};                                                                        \
}))

/* ── runtime pointer constructor (heap path) ──
 * Sets bit31. Copies first min(len,4) bytes to prefix.
 * Does NOT copy string data. ptr must outlive this tiny_str_t.
 */
static inline tiny_str_t S_PTR(const char *ptr, uint32_t len) {
    tiny_str_t s;
    s.heap.length = len | _TINY_STR_HEAP_FLAG;   /* set heap flag */
    uint32_t plen = len < 4 ? len : 4;
    for (uint32_t i = 0;    i < plen; i++) s.heap.prefix[i] = ptr[i];
    for (uint32_t i = plen; i < 4;    i++) s.heap.prefix[i] = 0;
    s.heap.ptr = (char *)ptr;
    return s;
}

#define S_VIEW(ptr, len) S_PTR((ptr), (len))

/* ── equality ──────────────────────────────────────────────────────────────
 * len mismatch       → false (no data touch)
 * prefix mismatch    → false (one uint32 load from each, no ptr deref)
 * prefix match       → rep cmpsb on full data
 * Copy to locals: macro args may be temporaries — &(expr) is not an lvalue.
 */
static inline int _tiny_str_memeq(const char *a, const char *b, long n) {
    int result;
    asm volatile(
        "cld\n"
        "repe cmpsb\n"
        "sete %b0"
        : "=r"(result), "+S"(a), "+D"(b), "+c"(n)
        :: "memory");
    return result;
}

#define STR_EQ(s1, s2) ({                                                      \
    tiny_str_t _a = (s1), _b = (s2);                                           \
    int _eq = 0;                                                               \
    if (STR_LEN(_a) == STR_LEN(_b)) {                                          \
        /* bytes [4..7] = prefix in both arms (same offset after length).     \
           uint32 load = compare first 4 chars in one instruction. */          \
        uint32_t _p1, _p2;                                                     \
        __builtin_memcpy(&_p1, (const char*)&_a + 4, 4);                      \
        __builtin_memcpy(&_p2, (const char*)&_b + 4, 4);                      \
        if (_p1 == _p2)                                                        \
            _eq = _tiny_str_memeq(STR_DATA(_a), STR_DATA(_b),                 \
                                  (long)STR_LEN(_a));                          \
    }                                                                          \
    _eq;                                                                       \
})

/* ── compare against string literal ── */
#define STR_EQ_LIT(s, lit) ({                                                  \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "STR_EQ_LIT: literal > 12 chars. Use S_PTR() + STR_EQ().");           \
    uint32_t _ll = (uint32_t)(sizeof(lit)-1);                                  \
    int _eq = 0;                                                               \
    if (STR_LEN(s) == _ll)                                                     \
        _eq = _tiny_str_memeq(STR_DATA(s), (lit), (long)_ll);                 \
    _eq;                                                                       \
})

/* ── starts with literal ── */
#define STR_STARTS_WITH(s, lit) ({                                             \
    _Static_assert(sizeof(lit)-1 <= 12,                                        \
        "STR_STARTS_WITH: literal > 12 chars.");                               \
    uint32_t _pl = (uint32_t)(sizeof(lit)-1);                                  \
    int _sw = 0;                                                               \
    if (STR_LEN(s) >= _pl)                                                     \
        _sw = _tiny_str_memeq(STR_DATA(s), (lit), (long)_pl);                 \
    _sw;                                                                       \
})

/* ── slice — pointer-mode view, no alloc ──
 * Borrows from s. s must outlive the slice.
 * start >= len → empty string.
 */
static inline tiny_str_t STR_SLICE(tiny_str_t s, uint32_t start, uint32_t n) {
    uint32_t slen = STR_LEN(s);
    if (start >= slen) return S_PTR("", 0);
    uint32_t avail = slen - start;
    uint32_t take  = n < avail ? n : avail;
    return S_PTR(STR_DATA(s) + start, take);   /* always heap flag → no confusion */
}

/* ── find byte — repne scasb, returns index or -1 ── */
static inline long STR_FIND_BYTE(tiny_str_t s, char byte) {
    uint32_t len = STR_LEN(s);
    if (!len) return -1L;
    const char *start = STR_DATA(s);
    const char *p     = start;
    long remaining    = (long)len;
    asm volatile(
        "cld\n"
        "repne scasb"
        : "+D"(p), "+c"(remaining)
        : "a"((int)(unsigned char)byte)
        : "memory");
    /* repne scasb stops AFTER the matching byte (rdi points one past).
     * remaining was decremented once extra after match.
     * Not found: remaining reaches 0 with ZF clear.
     * Use sete on ZF to detect match cleanly. */
    int found;
    asm volatile("sete %b0" : "=r"(found) :: "cc");
    if (!found) return -1L;
    /* p now points one past the match; subtract start to get index */
    return (long)(p - start) - 1L;
}

/* ── I/O ── */
#define STR_PRINT(s)   _WRITE_IMPL(STR_DATA(s), STR_LEN(s))
#define STR_PRINTLN(s) ({                                                      \
    _WRITE_IMPL(STR_DATA(s), STR_LEN(s));                                      \
    char _nl = '\n'; _WRITE_IMPL(&_nl, 1);                                     \
})
