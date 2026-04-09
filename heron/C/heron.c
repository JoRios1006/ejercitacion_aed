#include <stddef.h>
#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define STDOUT 1
#define EPSILON 0.000000000000001

static inline void wprint(const char *str, size_t len) {
  asm volatile(
      "mov %0, %%rax; mov %1, %%rdi; mov %2, %%rsi; mov %3, %%rdx; syscall" ::
          "i"(SYS_WRITE),
      "g"(STDOUT), "g"(str), "g"(len)
      : "rax", "rdi", "rsi", "rdx");
}

static inline void wexit() {
  asm volatile("mov %0, %%rax;mov $0, %%rdi; syscall" ::"g"(SYS_EXIT)
               : "rax", "rdi");
}

static inline double _abs(double n) {
  union {
    double d;
    uint64_t u;
  } converter;
  converter.d = n;
  converter.u &= 0x7FFFFFFFFFFFFFFF;
  return converter.d;
}

uint8_t ram_block[128];
void *ram = (void *)ram_block;
#define GUESS    *(double *)((char *)ram + 0)
#define LEN      *(int *)((char *)ram + 8)
#define PRINT_BUF ((char *)ram + 12)
static inline int double_to_str(double n, char *buf) {
  int idx = 0;
  if (n < 0) {
    buf[idx++] = '-';
    n = -n;
  }
  uint64_t int_part  = (uint64_t)n;
  double   frac      = n - (double)int_part;
  char tmp[20];
  int  tlen = 0;
  if (int_part == 0) {
    tmp[tlen++] = '0';
  } else {
    uint64_t v = int_part;
    while (v > 0) {
      tmp[tlen++] = '0' + (v % 10);
      v /= 10;
    }
    for (int a = 0, b = tlen - 1; a < b; a++, b--) {
      char c = tmp[a]; tmp[a] = tmp[b]; tmp[b] = c;
    }
  }
  for (int i = 0; i < tlen; i++) buf[idx++] = tmp[i];
  buf[idx++] = '.';
  for (int i = 0; i < 15; i++) {
    frac *= 10.0;
    int digit = (int)frac;
    buf[idx++] = '0' + digit;
    frac -= digit;
  }
  return idx;
}

#define TARGET 2
void _start() {
  GUESS = TARGET / 2.0;
LOOP:
  GUESS = (GUESS + TARGET / GUESS) / 2.0;
  if (_abs((TARGET / GUESS) - GUESS) > EPSILON * 2) {
    goto LOOP;
  }

 PRINT:  
  wprint("ROOT OF ", 8);
  LEN = double_to_str(TARGET, PRINT_BUF);
  wprint(PRINT_BUF, LEN);
  wprint("  ", 2);
  LEN = double_to_str(GUESS, PRINT_BUF);
  wprint(PRINT_BUF, LEN);
  wprint("\n", 1);
  wexit();
}
