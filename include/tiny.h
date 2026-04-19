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
 *  11.  I/O  (print f64, int, str, read)
 *  12.  brk / slab allocator
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
#define ALIGN_UP(s)         (((s) + 7) & ~7)
#define ALIGN_DOWN(s)       ((s) & ~7)

#define OFFSET(ptr, bytes)  ((void*)((char*)(ptr) + (bytes)))
#define INDEX(base, i)      ((void*)((double*)(base) + (i)))
#define DEREF(addr)         (*(double*)(addr))


/* ─── 3. stack frame helpers ─────────────────────────────────────────────── */
/*
 * STACK(off) — void* to [rbp - off] in current frame.
 * REQUIRES valid frame pointer (-fno-omit-frame-pointer).
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
 * All ops take void* addresses pointing to doubles.
 * DEF  — *addr  = literal
 * COPY — *dst   = *src
 * SWAP — swap *a, *b            (xmm0/xmm1, no temp on stack)
 * INC  — *addr += 1.0
 * DEC  — *addr -= 1.0
 * SUM  — *out   = *a + *b
 * SUB  — *out   = *a - *b
 * MUL  — *out   = *a * *b
 * DIV  — *out   = *a / *b
 * MOD  — *out   = fmod(*a,*b)   (x87 fprem1, no libm)
 * ROOT — *out   = sqrt(*a)
 * ABS  — *out   = |*a|          (andpd sign mask, branchless)
 * NEG  — *out   = -*a           (xorpd sign mask, branchless)
 * MIN  — *out   = min(*a,*b)    (minsd)
 * MAX  — *out   = max(*a,*b)    (maxsd)
 * AVG  — *out   = (*a+*b)*0.5   (no overflow risk)
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

/* MOD via x87 fprem1 — loops until C2 clears (partial remainder done) */
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

#define ABS(out, a) ({                                                         \
    static const long long _absmask = 0x7FFFFFFFFFFFFFFFLL;                   \
    asm volatile(                                                              \
        "movsd (%1),   %%xmm0\n"                                               \
        "andpd (%2),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
        :: "r"(out), "r"(a), "r"(&_absmask) : "xmm0", "memory");              \
})

#define NEG(out, a) ({                                                         \
    static const long long _negmask = (long long)0x8000000000000000LL;        \
    asm volatile(                                                              \
        "movsd (%1),   %%xmm0\n"                                               \
        "xorpd (%2),   %%xmm0\n"                                               \
        "movsd %%xmm0, (%0)"                                                   \
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


/* ─── 5. f64 comparisons → double flag ──────────────────────────────────── */
/*
 * Stores 1.0 (true) or 0.0 (false) at out. NaN → false (ucomisd).
 * IS_GT / IS_LT / IS_GE / IS_LE / IS_EQ / IS_NE
 * CMP  — stores -1.0 / 0.0 / 1.0  (less / equal / greater)
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
 * FOR_RANGE(idx_addr, start, end)  — forward, INC each iteration
 * FOR_DOWN(idx_addr, start, end)   — reverse, DEC each iteration
 * WHILE_NZ(flag_addr)              — loop while *flag != 0.0
 * IF_GT/LT/GE/LE/EQ/NE(a,b)       — conditional block on two addrs
 */
#define FOR_RANGE(idx_addr, start, end) \
    for (DEF((idx_addr),(start)); DEREF(idx_addr) < (end); INC(idx_addr))

#define FOR_DOWN(idx_addr, start, end)  \
    for (DEF((idx_addr),(start)); DEREF(idx_addr) > (end); DEC(idx_addr))

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
 * FILL(base, count, val)    — set count doubles to val
 * ZERO(addr)                — zero one double slot
 * ARRAY_GET(base, i, dst)   — dst  = base[i]
 * ARRAY_SET(base, i, src)   — base[i] = src
 *
 * NTH(arr, idx, len)
 *   Bounds-checked pointer lookup for any pointer array (const char*[], etc).
 *   Returns arr[idx] if 0 <= idx < len, NULL otherwise.
 *   NULL = explicit sentinel meaning "not a valid index".
 *   0 is a valid index — never used as sentinel here.
 *   Unsigned cast on idx kills negative indices for free (wrap → huge → OOB).
 *
 * find_if(base, count, stride, pred)
 *   Linear scan. Returns void* to first element where pred(ptr) != 0, or NULL.
 *   Define pred as: static inline int my_pred(void *p) { ... }
 */
#define FILL(base, count, val) ({                                              \
    double  _fv = (val);                                                       \
    double *_fp = (double*)(base);                                             \
    for (long _fi = 0; _fi < (long)(count); _fi++) _fp[_fi] = _fv;            \
})

#define ZERO(addr) DEF((addr), 0.0)

#define ARRAY_GET(base, i, dst) \
    COPY((dst), INDEX((base), (i)))

#define ARRAY_SET(base, i, src) \
    COPY(INDEX((base), (i)), (src))

#define NTH(arr, idx, len) \
    ((unsigned long)(idx) < (unsigned long)(len) ? (arr)[(idx)] : (void*)0)

static inline void *find_if(void *base, long count, long stride,
                             int (*pred)(void *)) {
    char *p = (char *)base;
    for (long i = 0; i < count; i++, p += stride)
        if (pred((void *)p)) return (void *)p;
    return (void *)0;
}


/* ─── 8. integer bit ops ─────────────────────────────────────────────────── */
/*
 * Pure integer / bitwise. Operate on unsigned long / long unless noted.
 * Techniques from: Bit Twiddling Hacks — Sean Eron Anderson, Stanford.
 *                  (individual snippets are public domain)
 *
 * IS_POW2(v)        — 1 if v is a power of 2 (v > 0)
 * IS_OPP_SIGN(x,y)  — 1 if x and y have opposite signs
 * SIGN(v)           — -1, 0, or +1
 * IABS(v)           — |v| without branch  (signed long)
 * IMIN(x,y)         — branchless min      (signed long)
 * IMAX(x,y)         — branchless max      (signed long)
 * IAVG(x,y)         — (x+y)/2 no overflow: (x&y)+((x^y)>>1)
 * MERGE(a,b,mask)   — bits from b where mask=1, from a where mask=0
 * NEXT_POW2(v)      — next power of 2, 32-bit unsigned
 * POPCOUNT(v)       — set bit count, 32-bit (parallel Hamming weight)
 * TRAILING_ZEROS(v) — trailing zero count, 32-bit (DeBruijn multiply)
 */

#define IS_POW2(v)        ((unsigned long)(v) && !((v) & ((v) - 1)))
#define IS_OPP_SIGN(x,y)  (((long)(x) ^ (long)(y)) < 0)
#define SIGN(v)           (((long)(v) > 0L) - ((long)(v) < 0L))

#define IABS(v) ({                                                             \
    long _v = (v);                                                             \
    long _m = _v >> (sizeof(long)*8 - 1);                                     \
    (_v + _m) ^ _m;                                                            \
})

#define IMIN(x,y) ({                                                           \
    long _x = (x), _y = (y);                                                  \
    _y ^ ((_x ^ _y) & -((_x) < (_y)));                                        \
})

#define IMAX(x,y) ({                                                           \
    long _x = (x), _y = (y);                                                  \
    _x ^ ((_x ^ _y) & -((_x) < (_y)));                                        \
})

/* overflow-safe: no intermediate x+y */
#define IAVG(x,y)         (((x) & (y)) + (((x) ^ (y)) >> 1))

/* branchless bit-select: bits from b where mask=1, a where mask=0 */
#define MERGE(a, b, mask) ((a) ^ (((a) ^ (b)) & (mask)))

/* round up to next power of 2, 32-bit; 0→0, pow2→same */
#define NEXT_POW2(v) ({                                                        \
    unsigned int _v = (unsigned int)(v) - 1;                                  \
    _v |= _v >> 1;  _v |= _v >> 2;                                            \
    _v |= _v >> 4;  _v |= _v >> 8;                                            \
    _v |= _v >> 16; _v + 1;                                                    \
})

/* parallel popcount, 32-bit */
#define POPCOUNT(v) ({                                                         \
    unsigned int _v = (unsigned int)(v);                                      \
    _v = _v - ((_v >> 1) & 0x55555555u);                                      \
    _v = (_v & 0x33333333u) + ((_v >> 2) & 0x33333333u);                      \
    (int)(((_v + (_v >> 4) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);              \
})

/* DeBruijn trailing zeros, 32-bit — static table, one multiply */
static const int _tiny_debruijn_tz[32] = {
     0,  1, 28,  2, 29, 14, 24,  3, 30, 22, 20, 15, 25, 17,  4,  8,
    31, 27, 13, 23, 21, 19, 16,  7, 26, 12, 18,  6, 11,  5, 10,  9
};
#define TRAILING_ZEROS(v) \
    _tiny_debruijn_tz[((unsigned int)((v) & -(v)) * 0x077CB531u) >> 27]


/* ─── 9. string & memory ops ─────────────────────────────────────────────── */
/*
 * STRLEN(s)           — length via repne scasb. Does NOT count NUL. O(n).
 * MEM_COPY(dst,src,n) — copy n bytes via rep movsb.
 * MEM_ZERO(dst,n)     — zero n bytes via rep stosb.
 */

/*
 * repne scasb scans [rdi] while != al, decrementing rcx.
 * Start rcx=-1 (unlimited). After: rcx = -(len+2).
 * ~rcx = len+1, subtract 1 → len.
 */
#define STRLEN(s) ({                                                           \
    const char *_s = (s);                                                      \
    long _len;                                                                 \
    asm volatile(                                                              \
        "cld\n"                                                                \
        "repne scasb"                                                          \
        : "=c"(_len), "+D"(_s)                                                 \
        : "0"(-1L), "a"(0)                                                     \
        : "memory");                                                           \
    ~_len - 1L;                                                                \
})

#define MEM_COPY(dst, src, n) ({                                               \
    void *_d = (dst); const void *_s = (src); long _n = (n);                  \
    asm volatile(                                                              \
        "cld\n"                                                                \
        "rep movsb"                                                            \
        : "+D"(_d), "+S"(_s), "+c"(_n)                                         \
        :: "memory");                                                          \
})

#define MEM_ZERO(dst, n) ({                                                    \
    void *_d = (dst); long _n = (n);                                           \
    asm volatile(                                                              \
        "cld\n"                                                                \
        "rep stosb"                                                            \
        : "+D"(_d), "+c"(_n)                                                   \
        : "a"(0)                                                               \
        : "memory");                                                           \
})


/* ─── 10. dtoa / atof / atoi ─────────────────────────────────────────────── */
/*
 * dtoa(x, out)           — double → decimal string. No NUL. Returns length.
 *                          out >= 32 bytes.
 * tiny_atof(s, end_ptr)  — decimal string → double.
 * tiny_atoi(s, end_ptr)  — decimal string → long.
 * Both skip leading whitespace and handle '-'.
 * *end_ptr set to first unparsed char (pass NULL to ignore).
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
        if (*s >= '0' && *s <= '9') {
            if (in_frac) { frac *= 0.1; result += (*s - '0') * frac; }
            else         { result = result * 10.0 + (*s - '0'); }
        } else if (*s == '.') { in_frac = 1; } else { break; }
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
 * READ(buf, len)        — read stdin → buf. Returns bytes read (int).
 * PRINT(addr)           — print *(double*)addr
 * PRINTLN(addr)         — print *(double*)addr + '\n'
 * PRINT_INT(n)          — print signed long
 * PRINTLN_INT(n)        — print signed long + '\n'
 * PRINT_STR(ptr, len)   — write raw bytes
 * PRINTLN_STR(ptr, len) — write raw bytes + '\n'
 * EXIT(code)            — _exit syscall, noreturn
 *
 * Typical pattern for string lookup:
 *   const char *day = NTH(_semana, idx, 7);
 *   if (day) PRINTLN_STR(day, STRLEN(day));
 *   else     PRINTLN_STR("ERROR", 5);
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

/* integer → string via x87 BCD (same engine as dtoa, integer part only) */
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
