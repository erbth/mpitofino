/** Internal header of libmpitofino */
#ifndef __MPITOFINO_H
#define __MPITOFINO_H

#include <cstdint>
#include <vector>
#include <memory>
#include <map>
#include <filesystem>
#include <infiniband/verbs.h>
#include "common/utils.h"
#include "common/dynamic_buffer.h"

extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
}


namespace mpitofino
{

typedef uint64_t tag_t;

/* NOTE: This enum does also exist in the public header. Keep in sync
when updating. */
enum class datatype_t
{
	INT32 = 1
};


/* Global stateless utility functions */
size_t get_datatype_element_size(datatype_t dtype);


/* mpitofino client; essentially the main class to instantiate */
struct CollectiveChannel;
	
class Client final
{
	friend CollectiveChannel;

protected:
	/* Unix domain socket for communicaton with node daemon (nd) */
	std::filesystem::path nd_socket_path;
	WrappedFD nd_wfd;

	/* RoCE HCA context */
	WrappedObject<ibv_device*> ib_dev_list{ibv_free_device_list};
	ibv_device  *ib_dev{};
	WrappedObject<ibv_context> ib_ctx{ibv_close_device};

public:
	const uint64_t client_id;
	
	/* @param client_id must be unique across the cluster, e.g. a
	tuple (application id, MPI tag) */
	Client(uint64_t client_id);
	~Client();

	Client(Client&&) = delete;

	void open_ib_device();

	/* TODO: This should support concurrency somehow; probably through
	   locking */
	inline int get_nd_fd()
	{
		return nd_wfd.get_fd();
	}
};


/* Contains some helper functions, should however not be instantiated directly
 * but rather be used by AggregationGroup. */
class AggregationGroup;

struct CollectiveChannel
{
	Client& client;
	AggregationGroup& agg_group;

	const size_t chunk_size = 4096;
	dynamic_aligned_buffer ib_buf{4096, chunk_size * 2};

	WrappedObject<ibv_pd> ib_pd{ibv_dealloc_pd};
	WrappedObject<ibv_mr> ib_mr{ibv_dereg_mr};
	WrappedObject<ibv_cq> ib_cq{ibv_destroy_cq};
	WrappedObject<ibv_qp> ib_qp{ibv_destroy_qp};

	uint32_t local_qp;
	uint32_t fabric_ip;
	uint32_t fabric_qp;

	CollectiveChannel(Client& client, AggregationGroup& agg_group);
	void finalize_qp();

protected:
	void setup_ib_pd();
	void setup_ib_qp();
};

/* Like an MPI communicator */
class AggregationGroup final
{
	friend CollectiveChannel;
	
protected:
	Client& client;

	/* Participating node ids */
	std::vector<uint64_t> client_ids;
	
	/* Collective channels */
	std::map<tag_t, CollectiveChannel> channels;

	CollectiveChannel* get_channel(tag_t tag);

public:
	AggregationGroup(Client& client, const std::vector<uint64_t>& client_ids);

	~AggregationGroup();

	AggregationGroup(AggregationGroup&&) = delete;


	/* Blocking collective operations */
	void allreduce(const void* sbuf, void* dbuf, size_t count,
			datatype_t dtype, tag_t tag);
};

}

#endif /* __MPITOFINO_H */
