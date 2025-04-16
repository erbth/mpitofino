  * p4:  P4 program, which runs on the tofino switch; i.e. the dataplane code
  * sd:  switch daemon, which runs on the switch; part of the control plane
  * lib: libmpitofino, which contains the client-side public interface

# Compiling

Currently, only GCC has been tested to compile mpitofino.

## Switch-part:

Compile the switch-daemon (sd), e.g. with these commands:

```bash
cmake -DWITH_SD=ON
```

and compile the p4 code in the `p4`-subdirectory.


## Client-part:

```bash
cmake  -DWITH_CLIENT_LIB=ON
```
