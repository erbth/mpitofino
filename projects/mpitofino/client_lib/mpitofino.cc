#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "common/com_utils.h"
#include "mpitofino.h"
#include "client_lib_nd.pb.h"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/un.h>
}

using namespace std;
namespace fs = std::filesystem;


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
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	/* Determine path of socket of node daemon, and connect */
	vector<fs::path> socket_cand;
	socket_cand.push_back("/run/mpitofino-sd");

	auto val = getenv("XDG_RUNTIME_DIR");
	if (val && strlen(val) > 0 && val[0] == '/')
		socket_cand.push_back(string(val) + "/mpitofino-sd");

	for (const auto& s : socket_cand)
	{
		if (fs::exists(fs::symlink_status(s)))
		{
			nd_socket_path = s;
			break;
		}
	}

	if (nd_socket_path.empty())
		throw runtime_error("Unable to find path of node daemon's unix domain socket");

	WrappedFD fd;
	fd.set_errno(
		socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0),
		"socket(AF_UNIX)");

	/* Try to connect */
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX
	};

	strncpy(addr.sun_path, nd_socket_path.c_str(), sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

	check_syscall(
		connect(fd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)),
		"connect(nd unix domain socket)");

	nd_wfd = move(fd);
}

Client::~Client()
{
}


CollectiveChannel::CollectiveChannel(
		const struct sockaddr_in& local_addr,
		const std::vector<struct sockaddr_in> fabric_addrs)
	:
		local_addr(local_addr), fabric_addrs(fabric_addrs)
{
	fd.set_errno(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));

	/* Bind socket */
	check_syscall(
			::bind(fd.get_fd(), (const struct sockaddr*) &local_addr, sizeof(local_addr)),
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
	/* Local cache */
	{
		auto i = channels.find(tag);
		if (i != channels.end())
			return &(i->second);
	}


	/* Request a collective channel from the node daemon */
	ClientRequest req;
	auto msg = req.mutable_get_channel();
	msg->set_type(ChannelType::ALLREDUCE_INT32);
	msg->set_tag(tag);
	send_protobuf_message_simple_dgram(client.get_nd_fd(), req);


	/* Parse response */
	auto reply = recv_protobuf_message_simple_dgram<GetChannelResponse>(client.get_nd_fd());

	struct sockaddr_in local_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(reply.local_port())
	};
	local_addr.sin_addr.s_addr = reply.local_ip();

	vector<struct sockaddr_in> fabric_addrs;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(reply.fabric_port())
	};
	addr.sin_addr.s_addr = htonl(reply.fabric_ip());
	fabric_addrs.push_back(addr);


	/* Create collective channel */
	return &channels.try_emplace(tag, local_addr, fabric_addrs).first->second;
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
		auto& addr = ch->fabric_addrs.front();
		
		if (sendto(ch->fd.get_fd(), buf, block_size, 0,
					(const struct sockaddr*) &addr, sizeof(addr))
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
