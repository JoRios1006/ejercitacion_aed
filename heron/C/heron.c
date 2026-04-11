#define SYS_WRITE 1
#define SYS_EXIT 60
#define STDOUT 1
#define TARGET 2.0
#define EPSILON 0.000000000000001
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
    unsigned long u;
  } c;
  c.d = n;
  c.u &= 0x7FFFFFFFFFFFFFFF;
  return c.d;
}
#define STACK(type, offset)                                                    \
  (*(type *)((char *)__builtin_frame_address(0) - (offset)))
#define GUESS STACK(double, 128)
#define IDX STACK(int, 120)
#define BUF STACK(char *, 116)

static inline int dtoa(double x, char *out) {
  struct {
    u hi : 4, lo : 4, wr : 1;
  } n = {0};
  char *start = out;
  if (x < 0)
    *out++ = '-';
  if (x < 0)
    x = -x;
  uL i = x;
  double f = x - i;
  uC bcd[10];
  asm volatile("fildq %1\nfbstp %0\n" : "=m"(bcd) : "m"(i));
  IDX = 8;
UNPACK_NIBBLES:
  n.hi = (bcd[IDX] >> 4) & 0xF;
  n.lo = bcd[IDX] & 0xF;
  out += (n.wr |= !!n.hi, *out = '0' + n.hi, n.wr);
  out += (n.wr |= !!n.lo, *out = '0' + n.lo, n.wr && IDX > 0 || IDX == 0);
  if (--IDX >= 0)
    goto UNPACK_NIBBLES;

  *out++ = '.';
  IDX = 0;
  int digit;
GENERATE_DECIMALS:
  f *= 10;
  digit = f;
  *out++ = '0' + digit;
  f -= digit;
  if (IDX++ < 15)
    goto GENERATE_DECIMALS;

  return out - start;
}

void _start() {
  GUESS = 1.0;

CONVERGE:
  GUESS = (GUESS + TARGET / GUESS) * 0.5;
  if (_abs((TARGET / GUESS) - GUESS) > EPSILON * 2)
    goto CONVERGE;

  char *p = &STACK(char, 120);
  *(long *)p = 0x20464f20544f4f52LL;
  p += 8;
  p += dtoa(2.0, p);
  *(short *)p = 0x2020;
  p += 2;
  p += dtoa(GUESS, p);
  *p++ = '\n';
  sys_write(&STACK(char, 120), p - &STACK(char, 120));

  sys_exit();
}
