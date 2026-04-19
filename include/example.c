#include "tiny.h"
#define CATETO_A STACK(8)
#define CATETO_B STACK(16)
#define CUADRADO_A STACK(24)
#define CUADRADO_B STACK(32)
#define SUMA_CATETOS STACK(40)
#define RAIZ STACK(48)


__attribute__((noinline)) void yo_pongo_el_entry_point_donde_se_me_cante() {
    DEF(  CATETO_A,  2.0);
    DEF(  CATETO_B, 5.0);
    MUL(  CUADRADO_A,  CATETO_A, CATETO_A);
    MUL(  CUADRADO_B, CATETO_B, CATETO_B);
    SUM(  SUMA_CATETOS,  CUADRADO_A,  CUADRADO_B);
    ROOT( RAIZ,  SUMA_CATETOS);
    PRINTLN(RAIZ);
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
