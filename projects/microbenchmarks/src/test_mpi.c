#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

int fequals(double a, double b)
{
	return fabs(a - b) < 1e-50;
}

inline double gen_data_double(size_t seed, size_t i)
{
	return M_PI * i - seed * i * i;
}

inline int32_t gen_data_int32(size_t seed, size_t i)
{
	return seed * 0x100000 + i;
}


int test_p2p()
{
	int size, rank;

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int left_partner = rank > 0 ? rank - 1 : size - 1;
	int right_partner = rank < (size - 1) ? rank + 1 : 0;

	const size_t cnt_data = 1024;
	double send_vals[cnt_data];
	double recv_vals[cnt_data];
	double exp_vals[cnt_data];

	memset(send_vals, 0, sizeof(send_vals));
	memset(recv_vals, 0, sizeof(recv_vals));
	memset(exp_vals,  0, sizeof(exp_vals));

	for (size_t i = 0; i < cnt_data; i++)
	{
		send_vals[i] = gen_data_double(rank, i);
		exp_vals[i]  = gen_data_double(left_partner, i);
	}


	if (rank == 0)
	{
		MPI_Recv(recv_vals, cnt_data, MPI_DOUBLE, left_partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send(send_vals, cnt_data, MPI_DOUBLE, right_partner, 0, MPI_COMM_WORLD);
	}
	else
	{
		MPI_Send(send_vals, cnt_data, MPI_DOUBLE, right_partner, 0, MPI_COMM_WORLD);
		MPI_Recv(recv_vals, cnt_data, MPI_DOUBLE, left_partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}


	int err = 0;
	for (size_t i = 0; i < cnt_data; i++)
	{
		if (!fequals(recv_vals[i], exp_vals[i]))
		{
			printf("test_p2p: rank %d[%zu]: %e != %e (exp)\n",
				   rank, i, recv_vals[i], exp_vals[i]);
			err = 1;
		}
	}

	printf("test_p2p: %s\n", err ? "ERROR" : "ok");
	return err;
}


int test_coll_allreduce()
{
	int size, rank;

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	const size_t cnt_data = 1024;
	int32_t send_vals[cnt_data];
	int32_t recv_vals[cnt_data];
	int32_t exp_vals[cnt_data];

	memset(send_vals, 0, sizeof(send_vals));
	memset(recv_vals, 0, sizeof(recv_vals));
	memset(exp_vals, 0, sizeof(exp_vals));

	for (size_t i = 0; i < cnt_data; i++)
	{
		send_vals[i] = gen_data_int32(rank, i);

		for (size_t j = 0; j < size; j++)
			exp_vals[i] +=  gen_data_int32(j, i);
	}


	MPI_Allreduce(send_vals, recv_vals, cnt_data, MPI_INT32_T, MPI_SUM, MPI_COMM_WORLD);


	int err = 0;
	for (size_t i = 0; i < cnt_data; i++)
	{
		if (recv_vals[i] != exp_vals[i])
		{
			printf("test_coll_allreduce: rank %d[%zu]: %d != %d (exp)\n",
				   rank, i, (int) recv_vals[i], (int) exp_vals[i]);
			err = 1;
			break;
		}
	}
	
	printf("test_coll_allreduce: %s\n", err ? "ERROR" : "ok");
	return err;
}


int main(int argc, char** argv)
{
	int size, rank;

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	printf("Rank: %d, communicator size: %d\n", rank, size);

	int res = 0;
	res += test_p2p();
	res += test_coll_allreduce();

	MPI_Finalize();
	return res > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
