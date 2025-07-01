# Compiling `mpitofino`'s P4 program

Check that Barefoot's SDE (preferably version 9.9.1) is installed and enabled:

```shell
$ echo $SDE

# Should output something similar to:
# /home/therb/bf-sde-9.9.1
```

In the current directory (`/projects/mpitofino/p4` relative to the
repository's root) execute the following commands in a shell session (depending
on how you installed the SDE, you might need to run some of them with root
privileges, i.e. with `sudo`):

```shell
mkdir build
cd build

cmake $SDE/p4studio/ -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL \
                     -DCMAKE_MODULE_PATH=$SDE/cmake \
                     -DP4_NAME=mpitofino \
                     -DP4_LANG=p4-16 \
                     -DTOFINO=ON \
                     -DP4_PATH=$PWD/../mpitofino.p4

make
make install
```

...after that, the P4 program should be ready for use.
