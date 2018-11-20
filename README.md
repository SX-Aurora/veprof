# veprof
VE process profiler running on VH

this is work in progress, expect output format changes.

Goal of this tool is to get a profile of an application running on VE card without instrumenting
the executable in any way, with low overhead. The executable has to contain symbols.

Do not use with openmp codes for the moment.

## usage

sample with 100hz:

> ./veprof ./exe

sample with 50 hz

> ./veprof -s 50 ./exe

sample a wrapper calling exe

> ./veprof -e ./exe ./wrapper.sh

## prerequisites

* ve card
* veos > 1.3.0 (needs support for reading counters from card as well as some bug fixes)

## known bugs

* deadlocks with openmp codes, the application deadlocks when being profiled.
* there is an issue with card numbering in full card mode, with systems with one VE card, that should be no problem.

## future directions

* MPI support
* profiling not only on subroutine level, but more fine grained
* may be timeline sampling

