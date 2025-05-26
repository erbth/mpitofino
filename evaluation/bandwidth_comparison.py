#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


NODES = [2, 3, 4]
MPITOFINO =  [5.590595e+09, 5.526812e+09, 5.666537e+09] 
OMPI_BASIC = [1.632208e+09, 8.420144e+08, 7.499337e+08]
OMPI_TUNED = [1.123863e+09, 7.551288e+08, 8.720550e+08]


def main():
    #plt.plot(NODES, list(map(lambda x : x / 1024**3, MPITOFINO)), label="mpitofino")
    plt.plot(NODES, list(map(lambda x : x / 1024**3, OMPI_BASIC)), label="OpenMPI basic")
    plt.plot(NODES, list(map(lambda x : x / 1024**3, OMPI_TUNED)), label="OpenMPI tuned")

    plt.xlabel("# Nodes")
    plt.ylabel("Bandwidth usage / GiB/s")
    plt.title("Bandwidth usage vs. node count")
    plt.legend()
    plt.ylim(0)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
