# Compiling

``` shell
mkdir build
cd build

cmake $SDE/p4studio/ -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL \
                     -DCMAKE_MODULE_PATH=$SDE/cmake \
                     -DP4_NAME=allreduce_poc \
                     -DP4_LANG=p4-16 \
                     -DTOFINO=ON \
                     -DP4_PATH=$PWD/../allreduce_poc.p4
                     
make
make install
```
