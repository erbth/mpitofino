#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include "utils.h"


int main(int argc, char** argv)
{
	int size, rank;

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	printf("Rank: %d, communicator size: %d\n", rank, size);

	/* 1 GiB */
	const size_t cnt_data = 1024 * 1024 * 256;
	int32_t* send_vals = aligned_alloc(4096, cnt_data * sizeof(int32_t));
	int32_t* recv_vals = aligned_alloc(4096, cnt_data * sizeof(int32_t));
	int32_t* exp_vals  = aligned_alloc(4096, cnt_data * sizeof(int32_t));

	for (size_t i = 0; i < cnt_data; i++)
	{
		send_vals[i] = gen_data_int32(rank, i);
		recv_vals[i] = 0;

		exp_vals[i] = 0;
		for (size_t j = 0; j < size; j++)
			exp_vals[i] +=  gen_data_int32(j, i);
	}


	/* Bandwidth test */
	const size_t cnt_rep = 16;

	/* Warm-up */
	for (size_t i = 0; i < cnt_rep; i++)
		MPI_Allreduce(send_vals, recv_vals, cnt_data, MPI_INT32_T, MPI_SUM, MPI_COMM_WORLD);

	struct timespec t1, t2;
	clock_gettime(CLOCK_MONOTONIC, &t1);

	for (size_t i = 0; i < cnt_rep; i++)
		MPI_Allreduce(send_vals, recv_vals, cnt_data, MPI_INT32_T, MPI_SUM, MPI_COMM_WORLD);

	clock_gettime(CLOCK_MONOTONIC, &t2);


	int err = 0;
	for (size_t i = 0; i < cnt_data; i++)
	{
		if (recv_vals[i] != exp_vals[i])
		{
			printf("rank %d[%zu]: %d != %d (exp)\n",
				   rank, i, (int) recv_vals[i], (int) exp_vals[i]);
			err = 1;
			break;
		}
	}

	if (err)
		printf("data differs\n");


	double dt = time_diff(&t2, &t1);
	double dsize = cnt_data * sizeof(int32_t) * cnt_rep;
	double rate = dsize / dt;
	double latency = dt / cnt_rep;

	printf("Bandwidth test:\n");
	printf("  total data size: %eBytes, duration: %es, rate: %eBytes/s, latency: %es\n",
		   dsize, dt, rate, latency);




	MPI_Finalize();
	return err > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
