# veprof
**VE process profiler running on VH**

*this is work in progress, expect output format changes.*

Goal of this tool is to get a profile of an application running on VE card without instrumenting
the executable in any way, with lowest possible overhead. The executable has to contain symbols.

Output is a textfile containing performance counters and routine names, which can be viewed with `veprof_display`.

The idea is to complement the relatively expensive (in terms of overhead) but precise `ftrace` tool
with something less precise but cheaper.

Do not use with openmp codes for the moment, see bugs section.

## usage

sample with 100hz:

    ./veprof ./exe

sample with 50 hz

    ./veprof -s 50 ./exe

sample a wrapper calling exe

    ./veprof -e ./exe ./wrapper.sh

sample an openmp code (needs some VEOS patches beyond 1.3.2)

    ./veprof --openmp ./exe

display gathered results:

    ./veprof_display veprof.out

gives something like

```
FUNCTION            SAMPLES   TIME    TIME VECTIME  VTIME   VOP MFLOPS   MOPS    AVG    L1$ PRTCNF  LLC$E
                          %      %     [s]     [s]      %     %                 VLEN  MISS%    [s]   HIT%
main                  53.33  55.00    0.08    0.08 100.00 98.63  33388  67707 254.46   0.00   0.00 100.00
subroutine            46.67  45.00    0.06    0.06  96.21 98.55  32109  65167 254.46   0.00   0.00 100.00
```

## prerequisites

* ve card
* veos > 1.3.0 (needs support for reading counters from card as well as some bug fixes)

## building

    make

## known bugs

* sometimes application deadlock when beeing profiled (under investigation)
* very rear deadlocks with openmp codes, the application deadlocks when being profiled.

## future directions

* MPI support
* profiling not only on subroutine level, but more fine grained
* may be timeline sampling

