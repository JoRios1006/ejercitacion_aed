#include <stdio.h>
int main(void) {
  int numero = 9;
    float raiz;
    float guess = 3;
PROBAR:  
    if ((int)(guess * guess) == numero) {
	raiz = guess;
	goto MOSTRAR;
    } else {
	guess = (guess + (numero / guess)) / 2;
	goto PROBAR;
    };
MOSTRAR:
    printf("RAIZ %i \n ", ((int) raiz));
    return 0;
}
