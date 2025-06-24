#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


SIZE = [4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576]


MPITOFINO = [7.89, 7.88, 7.88, 7.82, 7.79, 7.81, 7.84, 7.85, 7.95, 8.00, 8.14, 8.48, 9.15, 10.84, 14.37, 24.34, 42.80, 79.22, 154.02]


OMPI_BASIC = [5.02, 4.99, 5.01, 5.05, 6.16, 6.39, 6.98, 7.39, 7.60, 10.24, 11.85, 16.53, 22.74, 37.15, 73.28, 138.33, 205.14, 389.88, 814.03]

OMPI_TUNED = [4.75, 4.76, 4.76, 4.80, 5.08, 5.16, 6.10, 6.34, 6.69, 7.82, 8.76, 16.31, 19.43, 25.69, 33.97, 45.13, 70.20, 156.11, 316.57]


def main():
    plt.rcParams.update({'font.size': 22})

    plt.plot(SIZE, list(map(lambda x : x, MPITOFINO)), label="mpitofino", marker='o')
    plt.plot(SIZE, list(map(lambda x : x, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(SIZE, list(map(lambda x : x, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("Size / Bytes")
    plt.ylabel("Avg. Latency / µs")
    plt.title("Latency vs. data size")
    plt.legend()
    plt.xticks(SIZE)
    plt.xscale('log', base=2)
    plt.yscale('log')

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
