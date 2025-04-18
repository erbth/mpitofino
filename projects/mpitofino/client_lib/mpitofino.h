/** Internal header of libmpitofino */
#ifndef __MPITOFINO_H
#define __MPITOFINO_H

#include <cstdint>
#include <vector>
#include <memory>
#include <map>
#include <filesystem>
#include "common/utils.h"

extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
}


namespace mpitofino
{

typedef uint64_t tag_t;

enum class datatype_t
{
	INT32 = 1
};


/* Global stateless utility functions */
size_t get_datatype_element_size(datatype_t dtype);


/* mpitofino client; essentially the main class to instantiate */
class Client final
{
protected:
	/* Unix domain socket for communicaton with node daemon (nd) */
	std::filesystem::path nd_socket_path;
	WrappedFD nd_wfd;

public:
	Client();
	~Client();

	Client(Client&&) = delete;

	/* TODO: This should support concurrency somehow; probably through
	   locking */
	inline int get_nd_fd()
	{
		return nd_wfd.get_fd();
	}
};


/* Contains some helper functions, should however not be instantiated directly
 * but rather be used by AggregationGroup. */
struct CollectiveChannel
{
	WrappedFD fd;

	struct sockaddr_in local_addr{};
	std::vector<struct sockaddr_in> fabric_addrs;

	CollectiveChannel(
		const struct sockaddr_in& local_addr,
		const std::vector<struct sockaddr_in> fabric_addrs);
};

/* Like an MPI communicator */
class AggregationGroup final
{
protected:
	Client& client;

	/* Collective channels */
	std::map<tag_t, CollectiveChannel> channels;

	CollectiveChannel* get_channel(tag_t tag);

public:
	AggregationGroup(Client& client);

	~AggregationGroup();

	AggregationGroup(AggregationGroup&&) = delete;


	/* Blocking collective operations */
	void allreduce(const void* sbuf, void* dbuf, size_t count,
			datatype_t dtype, tag_t tag);
};

}

#endif /* __MPITOFINO_H */
