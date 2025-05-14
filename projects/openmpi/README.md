This directory contains changes to OpenMPI to incorporate mpitofino

A new `coll` mca-component called `mtof` is a thin wrapper around
`libmpitofino` to integrate the latter with OpenMPI.

The directory `patches` contains patch files to be applied to the
OpenMPI version located as git submodule in `/third_party/openmpi` in
alphabetical order. Patches can be created manually e.g. by creating a
clean openmpi source tree with all patches applied and then `diff -uN`-ing
changed files. Make sure that the leading component of the destination
files is `ompi`.

# Creating an OpenMPI work tree with patches applied

```bash
cp -r ../../third_party/openmpi ompi
echo "gitdir: ../../../.git/modules/third_party/openmpi" > ompi/.git
cd ompi
cat ../patches/*.patch | patch -p1
cd ..
```

# Compiling

Preconditions:
  * Compile the mpitofino client_lib in `/projects/mpitofino/build`.
  * If not available on the system, (compile and) install OpenUCX.

First, prepare the source directory once (and on every change to a
`configure.ac` file or e.g. addition of a mca component):

```bash
cd ompi
./autogen.pl
cd ..
```

Then, create a build directory and run `configure`. Make sure to
replace `--prefix` and `--with-ucx` with the correct paths.

```bash
mkdir build
cd build
../ompi/configure --prefix=/opt/openmpi --disable-oshmem --with-ucx=/opt/openucx --without-verbs
```

Compile the entire project with
```bash
make
make install
```
or, when changing source files, only e.g. the `mtof` mca component with
```bash
make -C ompi/mca/coll/mtof install
```
.

# Compiling and running applications

Applications can now be built against the OpenMPI version compiled and
installed above using PkgConfig with an appropriate `PKG_CONFIG_PATH`,
e.g. for the prefix in the example (`/opt/openmpi`)
`PKG_CONFIG_PATH='/opt/openmpi/lib/pkgconfig'`.

Running applications will usually require specifying the prefix and an
`LD_LIBRARY_PATH`. The latter is required s.t. the dynamic linker
finds `libmpitofino`, which has not been installed and is not linked
by absolute path by `autotools`. A typical command-line to run in my
virtualized development environment might look like this (assuming
that mpirun can be found; i.e. `/opt/openmpi/bin` is in `$PATH` for
this example prefix):

```bash
mpirun --host n01,n02,n03,n04 --prefix /opt/openmpi --mca pml ucx -x UCX_TLS=^ud,ud:aux --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=/home/therb/projects/mpip4/projects/mpitofino/build/client_lib/ -x XDG_RUNTIME_DIR=/home/therb/.xdg_runtime_dir --tag-output $PWD/src/test-mpi
```

Note that the current working directory to execute this command was
`/projects/microbenchmarks/build`. Disabling the `ud` and `ud:aux`
transports for PML (p2p messaging) is required to run with the
tofino-model as switch. This is probably the case because the model is
slow and OpenMPI might tolerate only a certain latency before
asserting a timeout condition. Specifying an `XDG_RUNTIME_DIR` should
usually not be required, unless your development system does not have
a `/run/user/<id>` directory and the environment variable pointing to
it.

The other `coll` mca components are required to implement the
operations `mtof` does not provide.
