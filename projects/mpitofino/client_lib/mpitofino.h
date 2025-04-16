/** Internal header of libmpitofino */
#ifndef __MPITOFINO_H
#define __MPITOFINO_H

#include <cstdint>
#include <memory>
#include <map>
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
	struct sockaddr_in hpf_local_addr{};

public:
	Client();
	~Client();

	Client(Client&&) = delete;

	struct sockaddr_in get_hpf_local_addr();
};


/* Contains some helper functions, should however not be instantiated directly
 * but rather be used by AggregationGroup. */
struct CollectiveChannel
{
	WrappedFD fd;
	struct sockaddr_in addr{};

	CollectiveChannel();

	void bind(const struct sockaddr_in*);
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
