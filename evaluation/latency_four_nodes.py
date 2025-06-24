#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


SIZE = [64, 128, 256, 512, 1024, 2048, 4096]
MPITOFINO =  [7.244324e-06, 7.235475e-06, 7.229687e-06, 7.263417e-06, 7.375210e-06, 7.432665e-06, 7.829676e-06]
OMPI_BASIC = [5.309509e-06, 5.437151e-06, 6.528594e-06, 6.723512e-06, 7.200763e-06, 8.323660e-06, 9.836908e-06]
OMPI_TUNED = [4.856398e-06, 4.996459e-06, 6.105195e-06, 6.339971e-06, 6.735508e-06, 7.702766e-06, 8.742499e-06]


def main():
    plt.plot(SIZE, list(map(lambda x : x * 1e6, MPITOFINO)), label="mpitofino", marker='o')
    plt.plot(SIZE, list(map(lambda x : x * 1e6, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(SIZE, list(map(lambda x : x * 1e6, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("Size / Bytes")
    plt.ylabel("Latency / µs")
    plt.title("Latency vs. data size")
    plt.legend()
    plt.ylim(0)
    plt.xticks(SIZE)
    plt.xscale('log', base=2)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
