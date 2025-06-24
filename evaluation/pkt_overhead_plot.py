#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt




def overhead(s):
    return s / (s + 14 + 20 + 8 + 12 + 4 + 4 + 8 + 12)

def main():
    pkt_sizes = [64, 128, 256, 512, 1024, 2048, 4096]
    overheads = list(map(overhead, pkt_sizes))

    plt.plot(pkt_sizes, overheads, linestyle='', marker='x')

    plt.xlabel("Packet size / Bytes")
    plt.ylabel("Bandwidth efficiency")
    plt.title("Bandwidth efficiency vs. packet size")
    plt.ylim(0)
    plt.xscale('log', base=2)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
