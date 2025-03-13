#!/usr/bin/env python3
from functools import lru_cache
import hashlib
import random
import scapy.all as scapy
import socket
import struct
import sys


DEFAULT_LENGTH = 1
INTERFACE = 'enp0s3'


def get_hostname():
    return socket.gethostname().split('.')[0]


@lru_cache
def get_src_mac():
    match get_hostname():
        case 'n01':
            return '52:55:00:00:00:15'
        case 'n02':
            return '52:55:00:00:00:14'
        case _:
            raise RuntimeError("Unknown hostname")

@lru_cache
def get_dst_mac():
    match get_hostname():
        case 'n01':
            return '52:55:00:00:00:14'
        case 'n02':
            return '52:55:00:00:00:15'
        case _:
            raise RuntimeError("Unknown hostname")


@lru_cache
def get_payload(length=DEFAULT_LENGTH):
    r = random.Random(hashlib.md5(get_hostname().encode()).digest()[0])

    buf = bytearray()
    for i in range(length):
        buf += struct.pack('>i', r.randint(-2**31, 2**31-1))
        #buf += struct.pack('>i', 1)

    return buf


def send_packet():
    pkt = scapy.Ether(src=get_src_mac(), dst='62:00:00:00:00:00', type=0x88b6)/get_payload()
    scapy.sendp(pkt, iface=INTERFACE)


def main():
    send_packet()


if __name__ == '__main__':
    main()
    sys.exit(0)
