#define SYS_WRITE 1
#define SYS_EXIT 60
#define STDOUT 1
#define EPSILON 0.000000000000001
#define TARGET 2.0
typedef unsigned u;
typedef unsigned long uL;
typedef unsigned char uC;

static inline void sys_write(const char *s, uL len) {
  asm volatile("mov %0, %%rax\n"
               "mov %1, %%rdi\n"
               "mov %2, %%rsi\n"
               "mov %3, %%rdx\n"
               "syscall" ::"i"(SYS_WRITE),
               "i"(STDOUT), "g"(s), "g"(len)
               : "rax", "rdi", "rsi", "rdx");
}

static inline void sys_exit(void) {
  asm volatile("mov %0, %%rax\n"
               "mov $0, %%rdi\n"
               "syscall" ::"i"(SYS_EXIT)
               : "rax", "rdi");
}

static inline double _abs(double n) {
  union {
    double d;
    uL u;
  } c;
  c.d = n;
  c.u &= 0x7FFFFFFFFFFFFFFF;
  return c.d;
}
static inline int dtoa(double x, char *out) {
  struct {
    u hi : 4, lo : 4, wr : 1;
  } n = {0};
  char *start = out;
  if (x < 0)
    *out++ = '-';
  if (x < 0)
    x = -x;

  uL i = (uL)x;
  double f = x - (double)i;

  uC bcd[10];
  asm volatile("fildq %1\nfbstp %0\n" : "=m"(bcd) : "m"(i));

  int b = 8;
UNPACK_NIBBLES:
  n.hi = (bcd[b] >> 4) & 0xF;
  n.lo = bcd[b] & 0xF;
  *out = '0' + n.hi;
  out += (n.wr |= n.hi) != 0;
  *out = '0' + n.lo;
  out += ((n.wr |= n.lo) != 0 & b > 0) | (b == 0);
  b--;
  if (b >= 0)
    goto UNPACK_NIBBLES;
  *out++ = '.';
  int d = 0;
  int digit;
GENERATE_DECIMALS:
  f *= (int) 10;
  digit = f;
  *out++ = '0' + digit;
  f -= digit;
  d++;
  if (d < 15)
    goto GENERATE_DECIMALS;
  return out - start;
}

#define GUESS (*(double *)((char *)__builtin_frame_address(0) - 128))
#define BUF ((char *)__builtin_frame_address(0) - 120)

void _start() {
  GUESS = TARGET / 2.0;

CONVERGE:
  GUESS = (GUESS + TARGET / GUESS) * 0.5;
  if (_abs((TARGET / GUESS) - GUESS) > EPSILON * 2)
    goto CONVERGE;

PRINT: {
  char *p = BUF;
  __builtin_memcpy(p, "ROOT OF ", 8);
  p += 8;
  p += dtoa(TARGET, p);
  __builtin_memcpy(p, "  ", 2);
  p += 2;
  p += dtoa(GUESS, p);
  *p++ = '\n';
  sys_write(BUF, p - BUF);
}

  sys_exit();
}
