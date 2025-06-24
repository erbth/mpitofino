#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt


RANKS = [2, 3, 4, 8, 12, 16]
MPITOFINO =  [6.365069e-06, 6.398665e-06, 7.228587e-06, 8.913111e-06, 1.111608e-05, 1.312078e-05]
OMPI_BASIC = [5.366837e-06, 6.240284e-06, 6.497108e-06, 1.205145e-05, 1.399969e-05, 1.524069e-05]
OMPI_TUNED = [3.032801e-06, 6.185869e-06, 6.099672e-06, 6.746566e-06, 1.066985e-05, 9.047057e-06]


def main():
    plt.plot(RANKS, list(map(lambda x : x * 1e6, MPITOFINO)), label="mpitofino", marker='o')
    plt.plot(RANKS, list(map(lambda x : x * 1e6, OMPI_BASIC)), label="OpenMPI basic", marker='o')
    plt.plot(RANKS, list(map(lambda x : x * 1e6, OMPI_TUNED)), label="OpenMPI tuned", marker='o')

    plt.xlabel("# ranks")
    plt.ylabel("Latency / µs")
    plt.title("Latency vs. number of ranks")
    plt.legend()
    plt.ylim(0)
    plt.xticks(RANKS)

    plt.show()


if __name__ == '__main__':
    main()
    sys.exit(0)
