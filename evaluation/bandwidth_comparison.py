#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


NODES = [2, 3, 4]
MPITOFINO =  [5.590595e+09, 5.526812e+09, 5.666537e+09] 
OMPI_BASIC = [1.632208e+09, 8.420144e+08, 7.499337e+08]
OMPI_TUNED = [1.160150e+09, 7.551288e+08, 8.720550e+08]


def main():
    plt.subplot(1,2,1)

    plt.plot(NODES, list(map(lambda x : x / 1e9, MPITOFINO)), label="mpitofino", marker='o')
    plt.plot(NODES, list(map(lambda x : x / 1e9, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(NODES, list(map(lambda x : x / 1e9, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("# Nodes")
    plt.ylabel("Bandwidth usage / GB/s")
    plt.title("Bandwidth usage vs. node count")
    plt.legend()
    plt.ylim(0)
    plt.xticks(NODES)


    plt.subplot(1,2,2)

    plt.plot(NODES, list(map(lambda x : x / 1e9, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(NODES, list(map(lambda x : x / 1e9, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("# Nodes")
    plt.ylabel("Bandwidth usage / GB/s")
    plt.title("Bandwidth usage vs. node count")
    plt.legend()
    plt.ylim(0)
    plt.xticks(NODES)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
