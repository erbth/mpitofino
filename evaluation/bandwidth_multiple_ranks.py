#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


RANKS = [1, 2, 3, 4]
MPITOFINO =  [5.675648e+09, 4.538800e+09, 3.073701e+09, 2.319917e+09]
OMPI_BASIC = [7.439069e+08, 5.453641e+08, 3.991439e+08, 3.376006e+08]
OMPI_TUNED = [8.351337e+08, 9.628138e+08, 5.016132e+08, 7.140620e+08]


def main():
    plt.plot(RANKS, list(map(lambda x : x / 1e9, MPITOFINO)), label="mpitofino", marker='o')
    plt.plot(RANKS, list(map(lambda x : x / 1e9, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(RANKS, list(map(lambda x : x / 1e9, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("# Ranks / node")
    plt.ylabel("Bandwidth usage / GB/s")
    plt.title("Bandwidth usage vs. ranks per node")
    plt.legend()
    plt.ylim(0)
    plt.xticks(RANKS)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
