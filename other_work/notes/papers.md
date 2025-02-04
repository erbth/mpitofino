# FP emulation
## Tiny-FPU
https://ieeexplore.ieee.org/abstract/document/9401149

## Unlocking the Power of Inline Floating-Point Operations on Programmable Switches
https://www.usenix.org/conference/nsdi22/presentation/yuan

compares to SwitchML; uses Tofino (but discovers limitations); moves to Banzai
open-source switch architecture

## Tofino + P4: A Strong Compound for AQM on High-Speed Networks?
https://ieeexplore.ieee.org/abstract/document/9463943

Tofino has no multiplication in its ALU

## NetCL: A Unified Programming Framework for In-Network Computing

more details about multiplication; memory is stage local


## Enabling In-Network Floating-Point Arithmetic for Efficient Computation Offloading
https://ieeexplore.ieee.org/abstract/document/9896997


# Applications / Benchmarks

## SwitchML

## Performance metrics in a hybrid MPI–OpenMP based molecular dynamics simulation with short-range interactions
https://www.sciencedirect.com/science/article/abs/pii/S0743731513002505

Table lookup based FP implementation, approximation, 7 stages?


# DPA/DPU communication acceleration


# Other open questions
  * Is a single copy enough? -> probably need rdma capable nics

  * Can we avoid cpu involvement for memory copy?

  * Administrative side: Where to put code, notes, ...? shared (lrz gitlab?) or
    somewhere local to me?


# On SR-IOV and VM passthrough

## SR-IOV: Performance Benefits for Virtualized Interconnects

https://dl.acm.org/doi/10.1145/2616498.2616537

## Survey on SR-IOV performance (Seminar at nas.cit.tum)

https://www.net.in.tum.de/fileadmin/TUM/NET/NET-2022-01-1/NET-2022-01-1_09.pdf
