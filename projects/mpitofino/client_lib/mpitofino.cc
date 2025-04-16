#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "mpitofino.h"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
}


using namespace std;

namespace mpitofino
{

/* Global stateless utility functions */
size_t get_datatype_element_size(datatype_t dtype)
{
	switch (dtype)
	{
	case datatype_t::INT32:
		return sizeof(int32_t);

	default:
		throw runtime_error("Invalid datatype");
	};
}


inline void convert_endianess_32(const uint32_t* src, uint32_t* dst, size_t count)
{
	for (size_t i = 0; i < count; i ++)
	{
		auto v = src[i];
		dst[i] =
			((v & 0xff) << 24) |
			((v & 0xff00) << 8) |
			((v & 0xff0000) >> 8) |
			((v & 0xff000000) >> 24);
	}
}

/* Tofino's ALUs use big endian (network byte order) */
inline void convert_endianess(const void* src, void* dst, size_t size, datatype_t dtype)
{
	switch (dtype)
	{
	case datatype_t::INT32:
		convert_endianess_32((const uint32_t*) src, (uint32_t*) dst, size / 4);
		break;

	default:
		throw runtime_error("Unsupported datatype");
	};
}


/* Client */
Client::Client()
{
	/* Get IP address of interface connected to high performance fabric. NOTE:
	 * This shall later be done by node daemon. */
	struct ifaddrs* ifa = nullptr;
	check_syscall(getifaddrs(&ifa), "getifaddrs");

	FINALLY(
	{
		for (auto i = ifa; i != nullptr; i = i->ifa_next)
		{
			if (!(i->ifa_flags & IFF_UP))
				continue;

			if (i->ifa_flags & IFF_LOOPBACK)
				continue;

			if (!i->ifa_addr)
				continue;

			if (i->ifa_addr->sa_family != AF_INET)
				continue;

			hpf_local_addr = *((struct sockaddr_in*) i->ifa_addr);
		}
	},
	{
		freeifaddrs(ifa);
	});
}

Client::~Client()
{
}

struct sockaddr_in Client::get_hpf_local_addr()
{
	return hpf_local_addr;
}


CollectiveChannel::CollectiveChannel()
{
	fd.set_errno(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));

	/* Shall later be obtained from node daemon */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0x4000);
	addr.sin_addr.s_addr = htonl(0x0a0a8000);
}

void CollectiveChannel::bind(const struct sockaddr_in* addr)
{
	check_syscall(
			::bind(fd.get_fd(), (const struct sockaddr*) addr, sizeof(*addr)),
			"bind collective channel socket");
}


/* Aggregation group */
AggregationGroup::AggregationGroup(Client& client)
	: client(client)
{
}

AggregationGroup::~AggregationGroup()
{
}


CollectiveChannel* AggregationGroup::get_channel(tag_t tag)
{
	{
		auto i = channels.find(tag);
		if (i != channels.end())
			return &(i->second);
	}

	/* Request a collective channel from the node daemon */
	auto [i,created] = channels.emplace();
	auto ch = &(i->second);

	/* NOTE: ip and port shall later be allocated by the node daemon */
	auto addr = client.get_hpf_local_addr();
	addr.sin_port = htons(0x2000);

	ch->bind(&addr);

	return ch;
}


void AggregationGroup::allreduce(
		const void* sbuf, void* dbuf, size_t count,
		datatype_t dtype, tag_t tag)
{
	/* Get or create collective channel */
	auto ch = get_channel(tag);

	/* Exchange data */
	size_t size = count * get_datatype_element_size(dtype);
	const size_t block_size = 256;

	uint8_t buf[block_size];
	memset(buf, 0, block_size);

	for (size_t i = 0; i < size; i += block_size)
	{
		auto to_send = min(block_size, size - i);
		convert_endianess((const uint8_t*) sbuf + i, buf, to_send, dtype);

		/* Send block */
		if (sendto(ch->fd.get_fd(), buf, block_size, 0,
					(const struct sockaddr*) &ch->addr, sizeof(ch->addr))
				!= (ssize_t) block_size)
		{
			throw system_error(errno, generic_category(),
					"sendto(collective channel)");
		}

		printf("sent\n");


		/* Receive block */
		struct sockaddr_in recv_addr{};
		socklen_t recv_addr_len = sizeof(recv_addr);

		auto ret = recvfrom(ch->fd.get_fd(), buf, block_size, 0,
				(struct sockaddr*) &recv_addr, &recv_addr_len);

		if (ret != block_size)
			throw system_error(errno, generic_category(),
					"recvfrom(collective channel)");

		printf("received\n");

		convert_endianess(buf, (uint8_t*) dbuf + i, to_send, dtype);
	}
}


}
