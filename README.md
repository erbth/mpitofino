# Exploring and Evaluating MPI Support on P4 Programmable Switches

For a tutorial on compiling and running `mpitofino`, skip ahead to
[**mpitofino**](projects/mpitofino/README.md).

This is the main repository for our work on `mpitofino`, a software stack to
offload integer aggregation onto Tofino switches (so-called *in-network
computing*, or *in-fabric computing*).

In addition to the actual `mpitofino` software suite, it houses additional
information and tools.

It is available from [https://github.com/erbth/mpitofino](https://github.com/erbth/mpitofino).

## Repository structure

### evaluation
This directory contains the experimental results used for the thesis, and
additional profiling-results gathered while optimizing the code.

### other_work
A 'scratchpad' to persist thoughts that indirectly influenced the work

### projects
The actual codebase, split into different 'subprojects', lives in this
repository. It is organized into the following subdirectories:

  * **allreduce_poc**: An early proof-of-concept for implementing the
    *allreduce* collective communicaiton primitive on Tofino. It uses a direct
    ethernet-based transport and has all runtime parameters hardcoded. It might
    be useful to understand/test the basic concept without the complex dynamic
    resource allocation and RoCE transport of `mpitofino`, but probably not for
    much more.

  * [**microbenchmarks**](projects/microbenchmarks/README.md):
    Microbenchmarks/test that use OpenMPI to evaluate `mpitofino` on a system
    level (therefore using OpenMPI and not e.g. `mpitofino`'s internal API)

  * [**mpitofino**](projects/mpitofino/README.md): The actual core system
    containing the P4-code, control plane, and an example client which uses
    `mpitofino`'s internal API to test/evaluate the core system.

  * [**openmpi**](projects/openmpi/README.md): The OpenMPI integration, which
    makes `mpitofino` available as a so-called *component* in OpenMPI's *modular
    component architecture*.

  * **tools**: Additional tools required to run the system.

### third_party
Third party repositories used in/with `mpitofino`. Currently this contains a
only *OpenMPI* version 4.8.1 as a git-submodule. It is used as a basis for our
patches that add support for `mpitofino` to OpenMPI.

Hence make sure to clone this repository with `git clone --recursive`, or
download the submodule using `git submodule update --init` if you already cloned
the repository without `--recursive`.
