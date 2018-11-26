#include <stdio.h>
#include <math.h>

double iamthe99percent(long v) {
	double a[256],r=0.0;
	for(int i=0; i<256; i++) a[i]=sqrt(v+i);
	for(int i=0; i<256; i+=2) r+=a[i];
	return r;
}

double iamthe1percent(long v) {
	double a[256],r=0.0;
	for(int i=0; i<256; i++) a[i]=sqrt(v+i);
	for(int i=0; i<256; i+=2) r+=a[i];
	return r;
}

void dummy(double v) {
}

int main() {	
#pragma omp parallel for
	for(long i=0; i<1000000L; i++) {
		if(i%100000L==0) printf("working... %ld\n",i);
		dummy(iamthe1percent(i));
		for(int nine=0; nine<99; nine++)
			dummy(iamthe99percent(i+nine));
	}
	printf("done.\n");
}
	
