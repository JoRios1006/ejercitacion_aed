#include <stdio.h>
#include <stdbool.h>
int main(void) {
	float lado;
	printf("INGRESE EL LADO:");
	if (!scanf("%f", &lado)) {
		printf("BURRO ESO NO ES UN NUMERO \n");
		return 1;
	}
	int perimetro = lado * 4;
	int area = lado * lado;
	printf("Tu perimetro es de %d y tu area es de %d \n",  perimetro, area);
	printf(area == perimetro
			   ? "Area y Perimtro es igual \n"
			   :"No son iguales \n");
	return 0;
}
