The VMs of our development environment have all parameters/files configured
already, and contain a working Barefoot SDE. The instructions in this file are
not required for our development environment (but were actualy created from its
configuration).


# Switch VM

* needs 1 nic on auxiliary network (e.g. VLAN)
* needs 1 nic per connected node with point-to-point tunneling, e.g. QEMU's
  l2tpv3
* needs 1 dummy interface for emulated ASIC to CPU communication
* needs IPv6 disabled (s.t. neighbor discovery for link local addresses does not
  interfere with mpitofino-traffic
* needs promiscous mode enabled on all interfaces of the high performance
  network (i.e. dummy interface and the node-facing interfaces)
* needs one virtual loopback interface per port (the model does not allow to
  put individual ports into loopback mode - however entire pipes can be set to
  loopback, which might be sufficient to run the latest version of mpitofino,
  we did not test this though) - Linux does not support multiple loopback
  interfaces in the kernel, therefore use e.g. the software tap loopback
  interface contained in the repository at `/projects/tools/tap_loopback`,
  compile with
  ```bash
  mkdir build
  cd build
  cmake ..
  make
  ```,
  and then enable with e.g. this systemd unit file:
  ```ini
  # /etc/systemd/system/tap-loopback@.service
  [Unit]
  Description=TAP-based loopback device %i
  
  [Service]
  ExecStart=/home/therb/projects/mpip4/projects/tools/tap_loopback/build/tap_loopback %i
  Type=simple
  
  [Install]
  WantedBy=multi-user.target
  ```
  , and `systemctl enable tap-loopback@lb60.service` (with 61, 62, ... for the
  other node-facing interfaces)
  
* needs a `~/portmap.json` file to configure the mapping of Linux network
  interfaces to network interfaces in the model. For consistency with the
  documentation, it should be placed in the user's home, though its location
  does not have further semantics. It could look like this:
```json
{
    "PortToIf" : [
        { "device_port" :  64, "if" : "cpu_eth_0" },

        { "device_port" :  0, "if" : "enp0s4"  },
        { "device_port" :  4, "if" : "enp0s5"  },
        { "device_port" :  384, "if" : "enp0s6"  },
        { "device_port" :  388, "if" : "enp0s7"  },

        { "device_port" : 128, "if" : "lb60" },
        { "device_port" : 132, "if" : "lb61" },
        { "device_port" : 256, "if" : "lb62" },
        { "device_port" : 260, "if" : "lb63" }
    ]
}

```
  , with lb60..lb63 being the virtual loopback interfaces. Note that the names
  of the node-facing interfaces might differ based on the virtual network driver
  in use, and that the `device_port` can be chosen differently to map the
  virtual connections to the nodes onto different pipes of the emulated ASIC-
  just make sure to provide virtual loopback interfaces accordingly.
  
The other setup can be done e.g. with the following `/etc/rc.local` file:
```bash
#!/bin/bash -e

# Setup network interfaces
ip link add cpu_eth_0 type dummy

ip link set cpu_eth_0 up promisc on

ip link set enp0s4 up promisc on
ip link set enp0s5 up promisc on
ip link set enp0s6 up promisc on
ip link set enp0s7 up promisc on

sysctl net.ipv6.conf.all.disable_ipv6=1
```


# Node VMs

* Must have 1 network interface on the auxiliary network
* Must have 1 network interface on the high performance network, ideally a
  direct tunnel to the corresponding interface of the switch vm (e.g. with
  QEMU's l2tpv3 driver)
* Must have IPv6 disabled to avoid generating neighbor discovery datagrams for
  the link local address of the high performance network's interface
* Must have an IP address set on the switch-facing interface
* Must have a kernel with the `rxe` software RoCEv2 driver (or an equivalent
  virtual HCA) and that driver enabled
  
This might be done with the following `rc.local` file:
```bash
#!/bin/bash -e

sysctl net.ipv6.conf.all.disable_ipv6=1

IPADDR=${HOSTNAME%%.mpip4}
IPADDR=${IPADDR#n}

ip addr add 10.10.0.${IPADDR}/16 dev enp0s3

ip link set enp0s3 up
modprobe rdma_rxe
rdma link add ib0 type rxe netdev enp0s3
```

Note that the IP address on the high performance network is set based on the
hostname in this example. Maybe you have to change this to fit your needs.
