MPICC=/opt/nec/ve/mpi/latest/bin64/mpicc

ALL: veprof test1 test2 test3_omp test4_mpi test5

clean:
	rm veprof.o veprof

veprof.o: veprof.cpp
	g++ -g -O3 -std=c++11 -I /opt/nec/ve/veos/include/ -I /opt/nec/ve/veos/include/veosinfo/ -c veprof.cpp 

veprof: veprof.o
	g++ -g -O3 -std=c++11 veprof.o -L /opt/nec/ve/veos/lib64/ -lveosinfo -o veprof -lboost_program_options -lncurses


test1: test1.c
	/opt/nec/ve/bin/ncc -g test1.c -o test1

test2: test2.cpp
	/opt/nec/ve/bin/nc++ test2.cpp -o test2

test3_omp: test3_omp.cpp
	/opt/nec/ve/bin/nc++ -O1 -fopenmp test3_omp.cpp -o test3_omp

test4_mpi: test4_mpi.c
	$(MPICC) test4_mpi.c -o test4_mpi

test5: test5.f90
	/opt/nec/ve/bin/nfort test5.f90 -o test5
