/** Simple example application to test and showcase libmpitofino. Needs to be
 * executed on multiple client nodes concurrently. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "mpitofino.h"

using namespace std;


void print_help()
{
	printf("Usage: mpitofino_example <1-based node id> <number of nodes>\n");
}


int32_t generate_value(uint64_t node_id, size_t offset)
{
	return node_id * 0x100000 + offset;
}


int main(int argc, char** argv)
{
	/* Parse commandline arguments */
	if (argc != 3)
	{
		print_help();
		return 2;
	}

	uint64_t node_id = strtoul(argv[1], nullptr, 10);
	uint64_t cnt_nodes = strtoul(argv[2], nullptr, 10);

	if (node_id < 1 || node_id > cnt_nodes)
	{
		fprintf(stderr, "Invalid node id\n");
		return 2;
	}

	printf("Node id: %lu\n", (unsigned long) node_id);

	/* Build a list of all client ids of this aggregation group */
	vector<uint64_t> client_ids;
	for (uint64_t i = 1; i < cnt_nodes + 1; i++)
		client_ids.push_back(i);

	/* Create a libmpitofino client and aggregation group */
	mpitofino::Client client(node_id);
	mpitofino::AggregationGroup agg_group(client, client_ids);


	/* Perform an allreduce operation */
	const size_t cnt_elem = 256 * 2;
	int32_t* sbuf =     (int32_t*) aligned_alloc(256, cnt_elem * sizeof(int32_t));
	int32_t* dbuf =     (int32_t*) aligned_alloc(256, cnt_elem * sizeof(int32_t));
	int32_t* expected = (int32_t*) aligned_alloc(256, cnt_elem * sizeof(int32_t));

	for (size_t i = 0; i < cnt_elem; i++)
	{
		sbuf[i] = generate_value(node_id, i);
		dbuf[i] = 0;

		expected[i] = 0;

		for (uint64_t j = 1; j <= cnt_nodes; j++)
			expected[i] += generate_value(j, i);
	}


	agg_group.allreduce(sbuf, dbuf, cnt_elem, mpitofino::datatype_t::INT32, 1);


	/* Compare result */
	bool match = true;
	for (size_t i = 0; i < cnt_elem; i++)
	{
		if (expected[i] != dbuf[i])
		{
			printf("mismatch: 0x%08lx: 0x%08x != 0x%08x\n",
					(long) i, (int) expected[i], (int) dbuf[i]);

			match = false;
		}
	}

	if (match)
	{
		printf("data matches.\n");
		return 0;
	}
	else
	{
		printf("data differs.\n");
		return 1;
	}
}
