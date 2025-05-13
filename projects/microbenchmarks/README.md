# Runinng

With mpitofino:
```bash
mpirun --host n01,n02 --prefix /opt/openmpi --mca pml ucx -x UCX_TLS=^ud,ud:aux --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=/home/therb/projects/mpip4/projects/mpitofino/build/client_lib/ -x XDG_RUNTIME_DIR=/home/therb/.xdg_runtime_dir --tag-output $PWD/src/test-mpi
```

The `basic` component is required for the operations that mtof does implement. `libnbc` is required for the non-blocking collectives, which `basic` does not implement.

With basic:
```bash
mpirun --host n01,n02 --prefix /opt/openmpi --mca pml ucx -x UCX_TLS=^ud,ud:aux --mca coll basic,libnbc -x LD_LIBRARY_PATH=/home/therb/projects/mpip4/projects/mpitofino/build/client_lib/ -x XDG_RUNTIME_DIR=/home/therb/.xdg_runtime_dir --tag-output $PWD/src/test-mpi
```
