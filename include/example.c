#include "tiny.h"
#define CATETO_A STACK(8)
#define CATETO_B STACK(16)
#define CUADRADO_A STACK(24)
#define CUADRADO_B STACK(32)
#define SUMA_CATETOS STACK(40)
#define RAIZ STACK(48)

int is_even(void *p) {
    long v = *(long*)p;
    return (v & 1) == 0;
}
__attribute__((noinline)) void yo_pongo_el_entry_point_donde_se_me_cante() {
    DEF(  CATETO_A,  2.0);
    DEF(  CATETO_B, 5.0);
    MUL(  CUADRADO_A,  CATETO_A, CATETO_A);
    MUL(  CUADRADO_B, CATETO_B, CATETO_B);
    SUM(  SUMA_CATETOS,  CUADRADO_A,  CUADRADO_B);
    ROOT( RAIZ,  SUMA_CATETOS);
    PRINTLN(RAIZ);
/* ─── 1. STRLEN + PRINT_STR ───────────────────────────── */
    const char *msg = "Hello tiny world";
    long len = STRLEN(msg);
    PRINT_STR(msg, len);
	PRINTLN_STR(" <- printed using raw syscall", 29);

    /* ─── 2. MEM_COPY ─────────────────────────────────────── */
       char copy[32];
       MEM_COPY(copy, msg, len);
       PRINTLN_STR(copy, len);

    /* ─── 3. NTH ──────────────────────────────────────────── */
      const char *days[] = {"MON","TUE","WED","THU","FRI","SAT","SUN"};
      const char *d = NTH(days, 2, 7);
      PRINTLN_STR(d, STRLEN(d));

    /* ─── 4. FIND_IF ─────────────────────────────────────── */
    long nums[] = {1, 3, 5, 8, 9};
    long *found = FIND_IF(nums, 5, sizeof(long), is_even);
    if (found) PRINTLN_INT(*found);

    /* ─── 5. atoi / atof / dtoa ──────────────────────────── */
    const char *num_str = "1234";
    long n = tiny_atoi(num_str, 0);
    PRINTLN_INT(n);

    const char *float_str = "3.1415";
    double f = tiny_atof(float_str, 0);
    PRINTLN(&f);

    char buf[32];
    int blen = dtoa(42.125, buf);
    PRINTLN_STR(buf, blen);

    /* ─── 6. READ ────────────────────────────────────────── */
    char input[16];
    PRINTLN_STR("Type a number:", 14);
    int r = READ(input, sizeof(input));
    long user = tiny_atoi(input, 0);
    PRINTLN_INT(user);

    /* ─── 7. slab allocator ─────────────────────────────── */
    SlabPool pool;
    slab_init(&pool, sizeof(long), 4);

    long *a = slab_alloc(&pool);
    long *b = slab_alloc(&pool);

    *a = 111;
    *b = 222;

    PRINTLN_INT(*a);
    PRINTLN_INT(*b);

    slab_free(&pool, a);
    slab_free(&pool, b);

    /* ─── 8. tiny_str_t (SSO string) ─────────────────────── */
    tiny_str_t s1 = S("HELLO");
    tiny_str_t s2 = S_PTR("HELLO_WORLD_LONG", 16);

    STR_PRINTLN(s1);
    STR_PRINTLN(s2);

    /* equality */
	//   if (STR_EQ(s1, S("HELLO")))
    //    PRINTLN_STR("EQ works", 8);

    /* starts with */
    if (STR_STARTS_WITH(s2, "HELL"))
        PRINTLN_STR("prefix ok", 9);

    /* slice */
    tiny_str_t slice = STR_SLICE(s2, 6, 5);
    STR_PRINTLN(slice);

    /* find byte */
    long idx = STR_FIND_BYTE(s2, '_');
    PRINTLN_INT(idx);

    /* ─── 9. PRINT (double) ─────────────────────────────── */
    double x = 9.75;
    PRINTLN(&x);
    EXIT(0);
}

__attribute__((naked)) void _start() {
    asm volatile(
        "pushq %rbp\n"
        "movq  %rsp, %rbp\n"
        "subq  $128, %rsp\n"
        "call  yo_pongo_el_entry_point_donde_se_me_cante\n"
        "xor   %edi, %edi\n"
        "mov   $60,  %eax\n"
        "syscall"
    );
}
