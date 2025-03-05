We need:
  * Basic L2 ethernet switching for point-to-point connections (using RoCE v2).
    Maybe DCB?
  * Some means of message processing for collective operations
  
  
# Compiling

``` shell
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
