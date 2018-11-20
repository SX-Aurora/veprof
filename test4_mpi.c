#include <mpi.h>

void iamthe99percent() {
}

void iamthe1percent() {
}

int main(int argc, char **argv) {	

	int size, rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	for(long i=0; i<10000000L; i++) {
		if (i%size == rank) {
			iamthe1percent();
			for(int nine=0; nine<99; nine++)
				iamthe99percent() ;
		}
	}
	
	MPI_Finalize();
}
	
