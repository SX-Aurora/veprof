#include <stdio.h>

void iamthe99percent() {
}

void iamthe1percent() {
}

int main() {	
#pragma omp parallel for
	for(long i=0; i<10000000L; i++) {
		if(i%100000L==0) printf("working... %ld\n",i);
		iamthe1percent();
		for(int nine=0; nine<99; nine++)
			iamthe99percent() ;
	}
	printf("done.\n");
}
	
