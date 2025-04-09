#!/usr/bin/env python3

"""
This file is/was actually to be used with an intermediate stage of mpitofino
directly, not with allreduce_poc.
"""

from functools import lru_cache
import hashlib
import random
import scapy.all as scapy
import socket
import struct
import sys


DEFAULT_LENGTH = 64
INTERFACE = 'enp0s3'


def get_hostname():
    return socket.gethostname().split('.')[0]


@lru_cache
def get_payload(length=DEFAULT_LENGTH):
    r = random.Random(hashlib.md5(get_hostname().encode()).digest()[0])

    if get_hostname() == "n01":
        base = 1024
    else:
        base = 2048

    buf = bytearray()
    for i in range(length):
        #buf += struct.pack('>i', r.randint(-2**31, 2**31-1))
        buf += struct.pack('>i', base + i)

    return buf


def send_packet():
    pkt = scapy.Ether(dst="02:80:00:00:04:00")/scapy.IP(dst="10.10.128.0")/scapy.UDP(sport=0x2000, dport=0x4000)/get_payload()
    scapy.sendp(pkt, iface="enp0s3")


def main():
    send_packet()


if __name__ == '__main__':
    main()
    sys.exit(0)
