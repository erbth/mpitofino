```bash
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example  1 1
Node id: 1
data matches.
3.192615e+00s; 2.102003e+07 Bytes/s
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example  1 1
Node id: 1
data matches.
2.535884e+00s; 2.646370e+07 Bytes/s
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example  1 1
Node id: 1
data matches.
2.548175e+00s; 2.633605e+07 Bytes/s
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example  1 1
```

udp initial performance

256 * 1024 * 64 int32 values; 256 bytes payload size per packet



with chunk-based 16k/4 endianess conversion (avx2):
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 3.421965e-01s, datasize: 4.194304e+08B, rate: 4.902802e+09B/s, latency: 8.554912e-02s
```

memcpy instead;
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 3.216400e-01s, datasize: 4.194304e+08B, rate: 5.216147e+09B/s, latency: 8.041000e-02s
```

endianess conversion commented out: almost linerate


# Now with AVX endianess conversion in bulks and omp parallel for

```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 4.577090e-01s, datasize: 4.194304e+08B, rate: 3.665477e+09B/s, latency: 1.144273e-01s

Profilers:
    io:           263.468151ms
    endianess_2:  182.516060ms
    endianess_1:  129.140689ms
  total:  575.124900ms
  time since last report:  6848559842.621687ms
```

w/o io:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

mismatch: 0x00000000: 0x00100000 != 0x00000000
data differs.
dt: 2.386870e-01s, datasize: 4.194304e+08B, rate: 7.028962e+09B/s, latency: 5.967174e-02s

Profilers:
    endianess_2:  165.505303ms
    endianess_1:  135.132393ms
    io:           1.555620ms
  total:  302.193316ms
  time since last report:  6848649037.775718ms
```


w/o endianess conversion:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

mismatch: 0x00000000: 0x00100000 != 0x00000000
data differs.
dt: 1.947961e-01s, datasize: 4.194304e+08B, rate: 8.612708e+09B/s, latency: 4.869902e-02s

Profilers:
    io:           243.076223ms
    endianess_1:  0.079502ms
    endianess_2:  0.076637ms
  total:  243.232362ms
  time since last report:  6848707216.515995ms
```

with nt stores etc. on 4 nodes:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 4
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 2.990221e-01s, datasize: 4.194304e+08B, rate: 5.610694e+09B/s, latency: 7.475553e-02s

Profilers:
    endianess_1:  184.035697ms
    endianess_2:  97.602172ms
    io:           83.065173ms
  total:  364.703042ms
  time since last report:  6854076669.596781ms
```


## with endianess conversion disabled

single node:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

mismatch: 0x00000000: 0x00100000 != 0x00000000
data differs.
dt: 1.793576e-01s, datasize: 4.194304e+08B, rate: 9.354060e+09B/s, latency: 4.483940e-02s

Profilers:
    io:           208.542408ms
    endianess_1:  2.508003ms
    endianess_2:  2.492903ms
  total:  213.543314ms
  time since last report:  6854226510.874283ms
```

2 nodes:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 2
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

mismatch: 0x00000000: 0x00300000 != 0x00000000
data differs.
dt: 1.867189e-01s, datasize: 4.194304e+08B, rate: 8.985279e+09B/s, latency: 4.667973e-02s

Profilers:
    io:           218.065759ms
    endianess_1:  2.487834ms
    endianess_2:  2.474479ms
  total:  223.028072ms
  time since last report:  6854247414.894321ms
```

4 nodes:
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 4
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

mismatch: 0x00000000: 0x00a00000 != 0x00000000
data differs.
dt: 2.007189e-01s, datasize: 4.194304e+08B, rate: 8.358564e+09B/s, latency: 5.017972e-02s

Profilers:
    io:           235.907732ms
    endianess_1:  2.496019ms
    endianess_2:  2.483706ms
  total:  240.887457ms
  time since last report:  6854268556.019040ms
```


## With profiler disabled
1 node
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 2.558376e-01s, datasize: 4.194304e+08B, rate: 6.557761e+09B/s, latency: 6.395939e-02s
```

2 nodes
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 2
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 2.783864e-01s, datasize: 4.194304e+08B, rate: 6.026592e+09B/s, latency: 6.959661e-02s
```

4 nodes
```
ga84low2@wolpy12:~/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 4
Node id: 1
Available IB devices:
  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen

  chosen gid: 3

dt: 2.826127e-01s, datasize: 4.194304e+08B, rate: 5.936469e+09B/s, latency: 7.065318e-02s
```



# MPI
1 rank per node

## mpitofino

2 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Available IB devices:
[1,0]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen
[1,0]<stdout>:
[1,1]<stdout>:Available IB devices:
[1,1]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7450) <-- chosen
[1,1]<stdout>:
[1,0]<stdout>:  chosen gid: 3
[1,0]<stdout>:
[1,0]<stdout>:Rank: 0, communicator size: 2
[1,1]<stdout>:  chosen gid: 3
[1,1]<stdout>:
[1,1]<stdout>:Rank: 1, communicator size: 2
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.072995e+00s, rate: 5.590595e+09Bytes/s, latency: 1.920622e-01s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.072990e+00s, rate: 5.590603e+09Bytes/s, latency: 1.920619e-01s

```

3 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Available IB devices:
[1,0]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen
[1,0]<stdout>:
[1,2]<stdout>:Available IB devices:
[1,2]<stdout>:  0: mlx5_0 (guid: 1070:fd03:0039:b182) <-- chosen
[1,2]<stdout>:
[1,1]<stdout>:Available IB devices:
[1,1]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7450) <-- chosen
[1,1]<stdout>:
[1,0]<stdout>:  chosen gid: 3
[1,0]<stdout>:
[1,0]<stdout>:Rank: 0, communicator size: 3
[1,2]<stdout>:  chosen gid: 3
[1,2]<stdout>:
[1,2]<stdout>:Rank: 2, communicator size: 3
[1,1]<stdout>:  chosen gid: 3
[1,1]<stdout>:
[1,1]<stdout>:Rank: 1, communicator size: 3
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.108459e+00s, rate: 5.526812e+09Bytes/s, latency: 1.942787e-01s
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.108459e+00s, rate: 5.526813e+09Bytes/s, latency: 1.942787e-01s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.108456e+00s, rate: 5.526817e+09Bytes/s, latency: 1.942785e-01s

```

4 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14,wolpy15 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Available IB devices:
[1,0]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7338) <-- chosen
[1,0]<stdout>:
[1,3]<stdout>:Available IB devices:
[1,3]<stdout>:  0: mlx5_0 (guid: 1070:fd03:0039:b132) <-- chosen
[1,3]<stdout>:
[1,2]<stdout>:Available IB devices:
[1,2]<stdout>:  0: mlx5_0 (guid: 1070:fd03:0039:b182) <-- chosen
[1,2]<stdout>:
[1,1]<stdout>:Available IB devices:
[1,1]<stdout>:  0: mlx5_0 (guid: 1070:fd03:002e:7450) <-- chosen
[1,1]<stdout>:
[1,2]<stdout>:  chosen gid: 3
[1,2]<stdout>:
[1,2]<stdout>:Rank: 2, communicator size: 4
[1,0]<stdout>:  chosen gid: 3
[1,0]<stdout>:
[1,0]<stdout>:Rank: 0, communicator size: 4
[1,1]<stdout>:  chosen gid: 3
[1,1]<stdout>:
[1,1]<stdout>:Rank: 1, communicator size: 4
[1,3]<stdout>:  chosen gid: 3
[1,3]<stdout>:
[1,3]<stdout>:Rank: 3, communicator size: 4
[1,3]<stdout>:Bandwidth test:
[1,3]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.031811e+00s, rate: 5.666537e+09Bytes/s, latency: 1.894882e-01s
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.031809e+00s, rate: 5.666541e+09Bytes/s, latency: 1.894880e-01s
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.031811e+00s, rate: 5.666536e+09Bytes/s, latency: 1.894882e-01s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 3.031807e+00s, rate: 5.666545e+09Bytes/s, latency: 1.894879e-01s
```

## openmpi basic

2 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 2
[1,1]<stdout>:Rank: 1, communicator size: 2
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.052554e+01s, rate: 1.632208e+09Bytes/s, latency: 6.578462e-01s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.052554e+01s, rate: 1.632207e+09Bytes/s, latency: 6.578465e-01s

```

3 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 3
[1,1]<stdout>:Rank: 1, communicator size: 3
[1,2]<stdout>:Rank: 2, communicator size: 3
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.040330e+01s, rate: 8.420144e+08Bytes/s, latency: 1.275206e+00s
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.040330e+01s, rate: 8.420144e+08Bytes/s, latency: 1.275206e+00s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.040330e+01s, rate: 8.420141e+08Bytes/s, latency: 1.275206e+00s

```

4 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14,wolpy15 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 4
[1,2]<stdout>:Rank: 2, communicator size: 4
[1,3]<stdout>:Rank: 3, communicator size: 4
[1,1]<stdout>:Rank: 1, communicator size: 4
[1,3]<stdout>:Bandwidth test:
[1,3]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.290852e+01s, rate: 7.499337e+08Bytes/s, latency: 1.431782e+00s
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.290851e+01s, rate: 7.499339e+08Bytes/s, latency: 1.431782e+00s
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.290853e+01s, rate: 7.499332e+08Bytes/s, latency: 1.431783e+00s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.290851e+01s, rate: 7.499340e+08Bytes/s, latency: 1.431782e+00s
```


# openmpi tuned

confirmed that it is using IB/RoCE

2 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll tuned,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 2
[1,1]<stdout>:Rank: 1, communicator size: 2
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.528644e+01s, rate: 1.123863e+09Bytes/s, latency: 9.554024e-01s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.528618e+01s, rate: 1.123883e+09Bytes/s, latency: 9.553860e-01s
```

3 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll tuned,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 3
[1,1]<stdout>:Rank: 1, communicator size: 3
[1,2]<stdout>:Rank: 2, communicator size: 3
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.275091e+01s, rate: 7.551288e+08Bytes/s, latency: 1.421932e+00s
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.275070e+01s, rate: 7.551357e+08Bytes/s, latency: 1.421919e+00s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 2.275050e+01s, rate: 7.551424e+08Bytes/s, latency: 1.421906e+00s
```

4 nodes
```
ga84low2@wolpy12:~/mpip4/projects/microbenchmarks/build$ mpirun --host wolpy12,wolpy13,wolpy14,wolpy15 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll tuned,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
[1,0]<stdout>:Rank: 0, communicator size: 4
[1,3]<stdout>:Rank: 3, communicator size: 4
[1,1]<stdout>:Rank: 1, communicator size: 4
[1,2]<stdout>:Rank: 2, communicator size: 4
[1,3]<stdout>:Bandwidth test:
[1,3]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.970044e+01s, rate: 8.720550e+08Bytes/s, latency: 1.231278e+00s
[1,0]<stdout>:Bandwidth test:
[1,0]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.976097e+01s, rate: 8.693839e+08Bytes/s, latency: 1.235061e+00s
[1,2]<stdout>:Bandwidth test:
[1,2]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.970042e+01s, rate: 8.720562e+08Bytes/s, latency: 1.231276e+00s
[1,1]<stdout>:Bandwidth test:
[1,1]<stdout>:  total data size: 1.717987e+10Bytes, duration: 1.976093e+01s, rate: 8.693854e+08Bytes/s, latency: 1.235058e+00s
```
