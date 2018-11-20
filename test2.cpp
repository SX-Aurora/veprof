#include <stdio.h>

void iamthe99percent() {
}

void iamthe1percent() {
}

int main(int argc, char**argv) {	
	printf("args: ");
	for(int i=1; i<argc; i++) {
		printf(" <%s>", argv[i]);
	}
	printf("\n");
	for(long i=0; i<10000000L; i++) {
		iamthe1percent();
   		if(i%100000L==0) printf("working... %ld\n",i);
		for(int nine=0; nine<99; nine++)
			iamthe99percent() ;
	}
}
	
