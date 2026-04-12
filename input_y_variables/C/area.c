typedef __UINT32_TYPE__ u32i;
typedef __UINT64_TYPE__ u64;
typedef unsigned u;
typedef unsigned long uL;
typedef unsigned char uC;
#define SYS_WRITE 1
#define STDOUT 1
#define SCPY(p, s) (__builtin_memcpy(p, s, sizeof(s)-1), p += sizeof(s)-1)
#include <string.h>
#include <stdio.h>
/*
  BENCH SETUP
*/
#define DONT_OPTIMIZE(x) __asm__ volatile("" : "+r"(x))
#define ITERATIONS 10000
static inline u64 rdtsc(void) {
  u32i lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((u64)hi << 32) | lo;
}
u64 ciclos[ITERATIONS];

/* Implementacion más canónica y "legible"
Mínimo:  7586 ciclos
Promedio: 586071 ciclos
< 10k:    259
< 50k:    9543
< 100k:   118
< 500k:   16
>= 500k:  64
Mediana:  18405 ciclos
P95:      32712 ciclos
int main(void) { // Casteamos todo a void para decirle a gcc que no me interesa
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    scanf("%lf", &base);
    scanf("%lf", &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    printf("Área:      %.2f\n", area);
    printf("Perímetro: %.2f\n", perimetro);
    // TERMINA EL TEST

    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);
  return 0;
}
*/

/* Implementación reemplazando printf por puts, hay que convertir los double
Mínimo:  7759 ciclos
Promedio: 333808 ciclos
< 10k:    164
< 50k:    9603
< 100k:   99
< 500k:   30
>= 500k:  104
Mediana:  16542 ciclos
P95:      28674 ciclos
Mínimo:  7759 ciclos
Promedio: 333808 ciclos
int main(void) { // Casteamos todo a void para decirle a gcc que no me interesa
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    char area_str[50];
    char perimetro_str[50];
    scanf("%lf", &base);
    scanf("%lf", &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    snprintf(area_str, sizeof(area_str), "%f", area);
    snprintf(perimetro_str, sizeof(perimetro_str), "%f", perimetro);
    fputs("Área:      ", stdout);
    puts(area_str);
    fputs("Perimetro:      ", stdout);
    puts(perimetro_str);
    // TERMINA EL TEST

    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);

  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  return 0;
}
*/

/* Conversion de Double a String con assembly de los 80
Mínimo:  7440 ciclos
Promedio: 417154 ciclos
< 10k:    753
< 50k:    8916
< 100k:   146
< 500k:   48
>= 500k:  137
Mediana:  14649 ciclos
P95:      37040 ciclos

static inline int dtoa(char *out, double x) {
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
  int IDX = 8;
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

int main(void) {
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    char area_str[50];
    char perimetro_str[50];
    scanf("%lf", &base);
    scanf("%lf", &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    dtoa(area_str, area);
    dtoa(perimetro_str, perimetro);
    fputs("Área:      ", stdout);
    puts(area_str);
    fputs("Perimetro:      ", stdout);
    puts(perimetro_str);
    // TERMINA EL TEST

    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);
  return 0;
}

*/

/* dtoa solamente con aritmética
Mínimo:  6582 ciclos
Promedio: 990164 ciclos
< 10k:    1013
< 50k:    8751
< 100k:   115
< 500k:   8
>= 500k:  113
Mediana:  13508 ciclos
P95:      31336 ciclos
static inline int dtoa(char *out, double x) {
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

int main(void) {
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    char area_str[50];
    char perimetro_str[50];
    scanf("%lf", &base);
    scanf("%lf", &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    dtoa(area_str, area);
    dtoa(perimetro_str, perimetro);
    fputs("Área:      ", stdout);
    puts(area_str);
    fputs("Perimetro:      ", stdout);
    puts(perimetro_str);
    // TERMINA EL TEST

    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);
  return 0;
}
*/

/* Un solo write
Mínimo:  4133 ciclos
Promedio: 411422 ciclos
< 10k:    4703
< 50k:    4943
< 100k:   157
< 500k:   52
>= 500k:  145
Mediana:  10349 ciclos
P95:      33524 ciclos
static inline int dtoa(char *out, double x) {
  char *start = out;
  if (x < 0) {
    *out++ = '-';
    x = -x;
  }

  unsigned long i = (unsigned long)x;
  double f = x - (double)i;

  char rev[20], *p = rev;
  do {
    *p++ = '0' + (i % 10);
    i /= 10;
  } while (i);
  while (p > rev)
    *out++ = *--p;

  *out++ = '.';
  for (int d = 0; d < 15; d++) {
    f *= 10.0;
    int digit = (int)f;
    *out++ = '0' + digit;
    f -= digit;
  }
  return out - start;
}
int main(void) {
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    char area_str[50];
    char perimetro_str[50];
    scanf("%lf", &base);
    scanf("%lf", &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    char buf[128];
    char *p = buf;
    __builtin_memcpy(p, "Área:      ", 11);
    p += 11;
    p += dtoa(p, area);
    *p++ = '\n';
    __builtin_memcpy(p, "Perimetro: ", 11);
    p += 11;
    p += dtoa(p, perimetro);
    *p++ = '\n';
    fwrite(buf, 1, p - buf, stdout);
    // TERMINA EL TEST

    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);
  return 0;
}
 */


/* Directamente la syscall
Mínimo:  3880 ciclos
Promedio: 359539 ciclos
< 10k:    5417
< 50k:    4336
< 100k:   85
< 500k:   32
>= 500k:  130
Mediana:  9325 ciclos
P95:      25842 ciclos
*/
static inline int dtoa(char *out, double x) {
  char *start = out;
  if (x < 0) {
    *out++ = '-';
    x = -x;
  }

  unsigned long i = (unsigned long)x;
  double f = x - (double)i;

  char rev[20], *p = rev;
  do {
    *p++ = '0' + (i % 10);
    i /= 10;
  } while (i);
  while (p > rev)
    *out++ = *--p;

  *out++ = '.';
  for (int d = 0; d < 15; d++) {
    f *= 10.0;
    int digit = (int)f;
    *out++ = '0' + digit;
    f -= digit;
  }
  return out - start;
}

static inline void sys_write(const char *s, uL len) {
asm volatile(
      "syscall"
      :
      : "a"(SYS_WRITE), // RAX
        "D"(STDOUT),    // RDI
        "S"(s),         // RSI
        "d"(len)        // RDX
      : "rcx", "r11", "memory" // ¡ESTO ES LO IMPORTANTE!
  );
}
int main(void) {
  for (int i = 0; i < ITERATIONS; i++) {
    u64 t0 = rdtsc();

    // EMPIEZA EL TEST
    double base, altura; // Pre asignar variables de forma canónica
    char area_str[50];
    char perimetro_str[50];
    scanf("%lf %lf", &base, &altura);
    double area = base * altura;
    double perimetro = 2.0 * (base + altura);
    char buf[256];
    memset(buf, 0, sizeof(buf));
    char *ptr = buf;
    SCPY(ptr, "Area:      ");
    ptr += dtoa(ptr, area);
    *ptr++ = '\n';
    SCPY(ptr, "Perimetro: ");
    ptr += dtoa(ptr, perimetro);
    *ptr++ = '\n';
    sys_write(buf, ptr - buf);
    // TERMINA EL TEST
    u64 t1 = rdtsc();
    ciclos[i] = t1 - t0;
  }
  u64 min = ciclos[0];
  u64 sum = 0;
  for (int i = 0; i < ITERATIONS; i++) {
    if (ciclos[i] < min)
      min = ciclos[i];
    sum += ciclos[i];
  }
  printf("Mínimo:  %lu ciclos\n", min);
  printf("Promedio: %lu ciclos\n", sum / ITERATIONS);
  u64 buckets[5] = {0};
  for (int i = 0; i < ITERATIONS; i++) {
    u64 c = ciclos[i];
    if (c < 10000)
      buckets[0]++;
    else if (c < 50000)
      buckets[1]++;
    else if (c < 100000)
      buckets[2]++;
    else if (c < 500000)
      buckets[3]++;
    else
      buckets[4]++;
  }
  printf("< 10k:    %lu\n", buckets[0]);
  printf("< 50k:    %lu\n", buckets[1]);
  printf("< 100k:   %lu\n", buckets[2]);
  printf("< 500k:   %lu\n", buckets[3]);
  printf(">= 500k:  %lu\n", buckets[4]);
  for (int i = 0; i < ITERATIONS - 1; i++)
    for (int j = 0; j < ITERATIONS - i - 1; j++)
      if (ciclos[j] > ciclos[j + 1]) {
        u64 tmp = ciclos[j];
        ciclos[j] = ciclos[j + 1];
        ciclos[j + 1] = tmp;
      }

  printf("Mediana:  %lu ciclos\n", ciclos[ITERATIONS / 2]);
  printf("P95:      %lu ciclos\n", ciclos[(int)(ITERATIONS * 0.95)]);
  return 0;
}
