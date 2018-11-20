#include <stdlib.h>
#include <stdio.h>

#define SIZE 50000L
//#define SIZE 200000L

void subroutine(int i, double *a, double *b) {
	for(int j=0; j< SIZE; j++) {
		b[i] += a[j];
	}
}

main() {
	
	double *a, *b;

	a=malloc(SIZE*8);
	b=malloc(SIZE*8);

	if(a==NULL || b==NULL) {
		printf("OOM\n");
		exit(1);
	}

	for(int i=0; i< SIZE; i++) {
		a[i] = 0.0;
		b[i] = 0.0;
	}

	for(int i=0; i< SIZE; i++) {
		for(int j=0; j< SIZE; j++) {
			a[i] += b[j];
		}
		subroutine(i, a,b);
	}

	printf(">>>> %ld\n", a[1]);
}
