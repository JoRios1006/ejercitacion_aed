#include <float.h>
#include <math.h>
#include <stdio.h>
#define EPSILON 1e-15

int main(void) {
  double numero = 2;
  double raiz;
  double guess = numero / 2; 
  double last_guess;
PROBAR:
  if (fabs((guess * guess) - numero) <= EPSILON) {
    raiz = guess;
    goto MOSTRAR;
  } else {
    guess = (guess + (numero / guess)) / 2;
    goto PROBAR;
  }
MOSTRAR:
  printf("RAIZ %1.15f\n ", raiz);
  return 0;
}
