# Overview of the system

The system consists of a switch and multiple worker nodes. All these components
might be either VMs (in which case the switch is emulated by the Barefoot SDE's
`tofino-model` emulator) or physical components. The nodes need RoCEv2 capable
HCAs, also these might be provided by Linux's software RoCE HCA `rxe`.

The switch and the nodes should be connected on the 100GBit-ports using direct
cables (and no additional switches in between), and need a separate network for
control plane traffic, which we will call the 'auxiliary network'). The latter
might be a simple 1GBit network - it is just important that the nodes can reach
the switch through its IP-address on that network (but the nodes' traffic to the
switch might even be sent through a NAT).

The switch runs `mpitofino`'s P4-program and a C++ control plane program, the
`switch daemon` (`sd`), to manage the former. The switch daemon will also
communicate with the nodes once they are connected. It runs on the switch's
x86-CPU, therefore it is important to connect the CPU's network interface to the
auxiliary network (and give it an IP-address...).

The nodes each run an instance of the `node daemon` (`nd`), which listens for
topology discovery beacons on the 100GBit-network and automatically connects to
the swich daemon once it received the switch daemon's address through the
beacons. It also creates a Unix-domain-socket to provide `mpitofino`'s services
to client applications, i.e. MPI-programs. Client applications may then use
`libmpitofino` to connect to the node daemon, and perform the data transmission
on the 100GBit-network. In short, `libmpitofino` provides a simple API for the
collective communication primitives and abstracts from all `mpitofino` specific
tasks.


# Compiling and running the switch-side

The instruction in this chapter must be performed on the switch. So first, log
into either your switch-VM or your real hardware Tofino switch.

First, compile and activate Barefoot's SDE (preferably version 9.9.1). Once
done, the environment variable `$SDE` should be set. You can check this with a
quick `echo`-command in the shell:

```bash
$ echo $SDE
# outputs e.g.: /home/therb/bf-sde-9.9.1
```

Next, proceed with the instructions in [p4/README.md](p4/README.md).

After successfully compiling the P4 program, you need to compile the C++
program.

For this, execute the following commands in the current directory
(`/projects/mpitofino` relative to the repository's root):

> [!note]
> The switch deamon requires dependencies like `cmake` and Google's
> `protobuf`. However these should have been installed as dependencies of the
> SDE already - if not, just install everything cmake reports as missing...

```bash
mkdir build
cd build

cmake -DWITH_SD=ON ..

make [-j8]  # Adjust to the number of cores in your system to speedup the build process
```


## Running in a VM with the emulator

If your development VM does not have the required network configuration set yet
(you would know if it had...), perform the actions in
[README.vm_setup.md](README.vm_setup.md) before continuing further.

First, copy the run-script to the build directory:
```bash
cp ../run_sd.sh .
```

Create a config file for the switch daemon. Therefore you have to:
  * identify the network interface which connects the ASIC and the CPU. In case
    of our prebuilt test VMs it is called `cpu_eth_0`. `ip link list` might be
    helpful.
	
  * Choose a MAC address for the switch on the high performance network (this is
    the 100GBit network on the real hardware, and the emulated network in case
    of the VM). It should be 'locally administered', i.e. bit 1 in the most
    significant byte must be set. A good choice might be `02:80:00:00:00:00`.
	
  * Choose an IP address for the switch on the high performance network. The
    nodes' IP addresses on the high performance network must be from the same IP
    range, and the range should be unique on all participating components
    (usually just the nodes and the switch). Determine the network's broadcast
    address as well (e.g. switch ip 10.10.128.0/16 -> broadcast 10.10.255.255).
	
  * Find out the IP of the switch on the auxiliary network.
  
Then create the config file based on the example in
[sd.example.conf](sd.example.conf):

```bash
cat << 'EOF' > sd.conf
cpu_interface_name <cpu_eth_0>

base_mac_addr <02:80:00:00:00:00>
collectives_module_ip_addr <10.10.128.0>
collectives_module_broadcast_addr <10.10.255.255>

control_ip_addr <172.18.0.11>
EOF
```

Replace the values in angle brackets with the actual values you chose.

After that, create a second session on the switch, enalbe the SDE in this
session, change to the P4 build directory (`/projects/mpitofino/p4/build`
relative to the repository's root), and start `tofino-model` (which emulates the
real ASIC):

```bash
$SDE/run_tofino_model.sh -p mpitofino -f ~/portmap.json -q
```

The argument `-q` can be omitted to get detailed output for every processed
packet, including the emulated chip's internal state - but be aware that this
heavily impacts performance and creates large log files.

Then, run the switch daemon with in the main session:

```bash
./run_sd.sh
```

. It automatically loads the P4-program and should reach operational state.

## Running on real hardware

One assembles the config file in the same way as in the virtual environment,
however the cpu interface is of course not a dummy interfaces:

```bash
cat << 'EOF' > sd.conf
cpu_interface_name enp4s0f0

base_mac_addr <02:80:00:00:00:00>
collectives_module_ip_addr <10.10.128.0>
collectives_module_broadcast_addr <10.10.255.255>

control_ip_addr <172.18.0.11>
EOF
```

No virtual loopback interfaces are required, but physical ports need to be
enabled and some of them put into loopback mode. This can be done by e.g. by
executing `/projects/tools/enable_ports_stordis2.sh` after starting the switch
daemon (alter the script according to the physical connectivity of your switch).

Additionally, it might be useful to run the switch daemon in a screen session,
because one likely would like it to persist after logoff (or accidental
disconnect of an ssh session).

If this is the first run after the switch's reboot, one needs to load the ASIC's
kernel driver, initialize a DMA pool first, and enable the CPU interface using
this shell snipped (the DMA-pool part was taken from `run_switchd.sh` of the
SDE's tools, which is not used directly by `mpitofino`):

```bash
sudo $SDE/install/bin/bf_kdrv_mod_load $SDE_INSTALL

sudo ip l s enp4s0f0 up promisc on

# The following part was copied from $SDE/run_switchd.sh:
function add_hugepage() {
    sudo sh -c 'echo "#Enable huge pages support for DMA purposes" >> /etc/sysctl.conf'
    sudo sh -c 'echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf'
}


echo "Setting up DMA Memory Pool"
hp=$(sudo sysctl -n vm.nr_hugepages)

if [ $hp -lt 128 ]; then
	if [ $hp -eq 0 ]; then
		add_hugepage
	else
		nl=$(egrep -c vm.nr_hugepages /etc/sysctl.conf)
		if [ $nl -eq 0 ]; then
			add_hugepage
		else
			sudo sed -i 's/vm.nr_hugepages.*/vm.nr_hugepages = 128/' /etc/sysctl.conf
		fi
	fi
	sudo sysctl -p /etc/sysctl.conf
fi

if [ ! -d /mnt/huge ]; then
	sudo mkdir /mnt/huge
fi
sudo mount -t hugetlbfs nodev /mnt/huge
```


# Compiling and running the node-side (interal API)

The client side code, comprised of the node daemon, libmpitofino, and the
OpenMPI integration (see next section) has the following dependencies which need
to be installed or made available through other means first:

* cmake 3.13 or later (in a HPC environment, `module load cmake` might be
  required)
* pkg-config
* `libibverbs` from `rdma-core`
* Google's `protobuf` including `protoc`


Then, the client-side code can be compiled with the following commands in the
current directory (`/projects/mpitofino` relative to the repository's root):

```bash
mkdir build
cd build

cmake -DWITH_CLIENT_LIB=ON ..

make [-j8]  # Adjust to the number of cores in your system to speedup the build process
```

. `WITH_CLIENT_LIB=ON` will automatically enable `WITH_ND=ON`, which enables the
node daemon. If you use more than one node and they do not share the directory
in which the code is compiled, it needs to be distributed to all nodes.

Next, start an instance of the node daemon in an extra session on each client
node:

```bash
nd/mpitofino-nd
```

Within 10 seconds, it should automatically discover the switch and connect to
it. Once done, it will print the switch's IP address on the auxiliary network to
stdout.

Now you can test the basic functionality with the example client, which uses
`mpitofino`'s native C++ API. Execute the following command on each node in the
main session:

```bash
client_lib/mpitofino_example <node id starting from 1> <number of nodes>
```

. Of cource, one can invoke the example client on a subset of nodes and adjust
the number (second parameter) accordingly.

When working in a virtual environment with `tofino-model`, it is usually
required to reduce the amount of data sent by the example client, as the running
time is otherwise very large due to the model's performance restrictions. This
can be done e.g. by changing the line:

```C++
	-const size_t cnt_elem = 1024 * 1024 * 100;
	+const size_t cnt_elem = 128;
```

in `client_lib/example.cc`.

The simplest test involves only a single node:

```bash
client_lib/mpitofino_example 1 1
```

and may yield the following output (captured in our virtual development
environment):

```
therb@n01.mpip4:~/projects/mpip4/projects/mpitofino/build$ client_lib/mpitofino_example 1 1
Node id: 1
Available IB devices:
  0: ib0 (guid: 5055:00ff:fe00:0015) <-- chosen

  chosen gid: 1

dt: 2.998829e+00s, datasize: 5.120000e+02B, rate: 6.829333e+02B/s, latency: 7.497071e-01s
```
.



# Compiling and running the node-side OpenMPI-integration

First, compile the node-daemon and `libmpitofino` using the [instructions
above](#Compiling and running the node-side (internal API)), then follow
[/projects/openmpi/README.md](../openmpi/README.md).


# Reproducing the numbers from the thesis

Most of the performance figures have been created using the microbenchmark in
`/projects/microbenchmarks/src/mpi_coll_performance.c`. The source code is
configured for a bandwidth-related benchmark, but can be changed to a
latency-benchmark by increasing the number of repetitions, decreasing the data
transmitted data size, and recompiling. The corresponding variables are called
`cnt_rep` and `cnt_data` in the source code. `cnt_data`, however, specifies the
number of 32bit integers, thus the actual aggregated number of bytes will be
`cnt_data * 4`.

From a real-hardware node in our testbed, the microbenchmark may be invoked
using:

```bash
mpirun --host wolpy12:4,wolpy13:4,wolpy14:4,wolpy15:4 --prefix $HOME/opt/openmpi --mca pml ucx --mca coll mtof,basic,libnbc -x LD_LIBRARY_PATH=$HOME/mpip4/projects/mpitofino/build/client_lib/ --tag-output $PWD/src/mpi_coll_performance
```

, with 4 MPI ranks per node, hence 16 ranks in total, which should almost quite
achieve the maximum theoretical throughput, accounting for overhead by headers.

Any other MPI program can be run in a similar way, just exchange the program
executable. However be aware that `mpitofino` does only support 32 bit integer
additions for `MPI_Allreduce`. All other collectives and point-2-point
communication should fallback to OpenMPI's other implementations part of the
modular component architecture's respective frameworks, but `MPI_Allreduce`
operations with different data types and/or operations do not fallback at the
moment. (However this could be implemented.)


# Important subdirectories
  * p4:  P4 program, which runs on the tofino switch; i.e. the dataplane code
  * sd:  switch daemon, which runs on the switch; part of the control plane
  * client_lib: libmpitofino, which contains the client-side public interface
