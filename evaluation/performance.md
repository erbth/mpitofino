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
