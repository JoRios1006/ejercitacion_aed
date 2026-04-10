#define SYS_WRITE 1
#define SYS_EXIT  60
#define STDOUT    1
#define EPSILON   0.000000000000001
#define TARGET    2.0

static inline void sys_write(const char *s, unsigned long len) {
    asm volatile(
        "mov %0, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "syscall"
        :: "i"(SYS_WRITE), "i"(STDOUT), "g"(s), "g"(len)
        : "rax", "rdi", "rsi", "rdx");
}

static inline void sys_exit(void) {
    asm volatile(
        "mov %0, %%rax\n"
        "mov $0, %%rdi\n"
        "syscall"
        :: "i"(SYS_EXIT) : "rax", "rdi");
}

static inline double _abs(double n) {
    union { double d; unsigned long u; } c;
    c.d = n;
    c.u &= 0x7FFFFFFFFFFFFFFF;
    return c.d;
}

static inline int ftoa(double x, char *out) {
    char *start = out;
    if (x < 0) { *out++ = '-'; x = -x; }

    unsigned long i = (unsigned long)x;
    double        f = x - (double)i;

    char rev[20], *p = rev;
    do { *p++ = '0' + (i % 10); i /= 10; } while (i);
    while (p > rev) *out++ = *--p;

    *out++ = '.';
    for (int d = 0; d < 15; d++) {
        f *= 10.0;
        int digit = (int)f;
        *out++ = '0' + digit;
        f -= digit;
    }
    return out - start;
}

#define GUESS (*(double *)((char *)__builtin_frame_address(0) - 128))
#define BUF   ((char *)__builtin_frame_address(0) - 120)

void _start() {
    GUESS = TARGET / 2.0;

CONVERGE:
    GUESS = (GUESS + TARGET / GUESS) * 0.5;
    if (_abs((TARGET / GUESS) - GUESS) > EPSILON * 2)
        goto CONVERGE;

PRINT: {
    char *p = BUF;
    __builtin_memcpy(p, "ROOT OF ", 8); p += 8;
    p += ftoa(TARGET, p);
    __builtin_memcpy(p, "  ", 2);       p += 2;
    p += ftoa(GUESS, p);
    *p++ = '\n';
    sys_write(BUF, p - BUF);
}

    sys_exit();
}
