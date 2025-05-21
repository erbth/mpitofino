#!/usr/bin/env python3
import binascii
import re
import sys


# See also: https://stackoverflow.com/a/23126768


def load_packet(filename):
    data = bytearray()
    
    # Load wireshark hex dump
    with open(filename, 'r') as f:
        for l in f:
            m = re.fullmatch(r'[0-9a-zA-Z]+   (.*)', l.strip())
            data += bytes.fromhex(m[1].replace(' ', ''))

    return bytes(data)


class ICRC:
    def __init__(self):
        self._acc = 0xffffffff
        self._polynomial = 0x04C11DB7

    def update_bit(self, b: bool):
        self._acc = self._acc << 1
        msb = (self._acc & 0x100000000) > 0

        b = msb ^ b

        self._acc = (self._acc & 0xfffffffe) | (0x1 if b > 0 else 0)

        self._acc = self._acc ^ ((self._polynomial & 0xfffffffe) * (1 if b else 0))

    def update_byte(self, b):
        for i in range(8):
            self.update_bit(b & 0x1 == 0x1)
            b = b >> 1

    def update_bytes(self, bs):
        for b in bs:
            self.update_byte(b)

    @property
    def value(self):
        output = self._acc

        # binary not without negation
        output = output ^ 0xffffffff

        output = \
            ((output & 0x00000001) << 7) | \
            ((output & 0x00000002) << 5) | \
            ((output & 0x00000004) << 3) | \
            ((output & 0x00000008) << 1) | \
            ((output & 0x00000010) >> 1) | \
            ((output & 0x00000020) >> 3) | \
            ((output & 0x00000040) >> 5) | \
            ((output & 0x00000080) >> 7) | \
            \
            ((output & 0x00000100) << 7) | \
            ((output & 0x00000200) << 5) | \
            ((output & 0x00000400) << 3) | \
            ((output & 0x00000800) << 1) | \
            ((output & 0x00001000) >> 1) | \
            ((output & 0x00002000) >> 3) | \
            ((output & 0x00004000) >> 5) | \
            ((output & 0x00008000) >> 7) | \
            \
            ((output & 0x00010000) << 7) | \
            ((output & 0x00020000) << 5) | \
            ((output & 0x00040000) << 3) | \
            ((output & 0x00080000) << 1) | \
            ((output & 0x00100000) >> 1) | \
            ((output & 0x00200000) >> 3) | \
            ((output & 0x00400000) >> 5) | \
            ((output & 0x00800000) >> 7) | \
            \
            ((output & 0x01000000) << 7) | \
            ((output & 0x02000000) << 5) | \
            ((output & 0x04000000) << 3) | \
            ((output & 0x08000000) << 1) | \
            ((output & 0x10000000) >> 1) | \
            ((output & 0x20000000) >> 3) | \
            ((output & 0x40000000) >> 5) | \
            ((output & 0x80000000) >> 7)

        return output


def main():
    pkt = load_packet('packet.hex')

    eth  = pkt[0:14]
    ip   = pkt[14:34]
    udp  = pkt[34:42]
    bth  = pkt[42:54]
    data = pkt[54:310]
    icrc = pkt[310:]

    c = ICRC()

    # Masked LRH
    c.update_bytes(b'\xff\xff\xff\xff\xff\xff\xff\xff')

    ip2 = bytearray(ip)
    udp2 = bytearray(udp)


    ip2[1] = 0xff
    ip2[8] = 0xff
    ip2[10] = 0xff
    ip2[11] = 0xff

    udp2[6] = 0xff
    udp2[7] = 0xff

    
    c.update_bytes(ip2)
    c.update_bytes(udp2)
    
    # BTH
    bth2 = bytearray(bth)

    # Mask resv8a
    bth2[4] = 0xff
    c.update_bytes(bth2)

    # Payload
    c.update_bytes(data)


    print("computed: %s" % hex(c.value))
    print("expected: 0x%x%x%x%x" % (icrc[0], icrc[1], icrc[2], icrc[3]))


if __name__ == '__main__':
    main()
    sys.exit(0)
