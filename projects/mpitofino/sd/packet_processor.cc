#include <system_error>
#include <stdexcept>
#include "packet_processor.h"

extern "C" {
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <net/if.h>
}

using namespace std;

#define CPU_INTERFACE_NAME "cpu_eth_0"

PacketProcessor::PacketProcessor(StateRepository& st_repo, Epoll& epoll)
	: epoll(epoll), st_repo(st_repo)
{
	try
	{
		/* Create raw socket */
		com_fd.set_errno(
			socket(AF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(ETH_P_ALL)),
			"Failed to create raw AF_PACKET socket for CPU-interface");

		/* Find interface */
		int_idx = if_nametoindex(CPU_INTERFACE_NAME);
		if (int_idx == 0)
		{
			throw system_error(errno, generic_category(),
							   "Failed to find CPU-interface");
		}

		/* Bind socket to interface */
		struct sockaddr_ll addr = {
			.sll_family = AF_PACKET,
			.sll_ifindex = int_idx
		};

		check_syscall(bind(com_fd.get_fd(),
						   (struct sockaddr*) &addr, sizeof(addr)),
					  "Failed to bind CPU-interface socket");

		/* Put interface into promiscuous mode */
		struct packet_mreq req = {
			.mr_ifindex = int_idx,
			.mr_type = PACKET_MR_PROMISC
		};

		check_syscall(setsockopt(com_fd.get_fd(),
								 SOL_PACKET, PACKET_ADD_MEMBERSHIP,
								 &req, sizeof(req)),
					  "Failed to enable promiscuous mode on CPU-interface");

		/* Add socket to epoll instance */
		epoll.add_fd(com_fd.get_fd(), EPOLLIN | EPOLLHUP | EPOLLRDHUP,
					 bind_front(&PacketProcessor::on_com_fd_data, this));
	}
	catch (...)
	{
		cleanup();
		throw;
	}
}

PacketProcessor::~PacketProcessor()
{
	cleanup();
}

void PacketProcessor::cleanup()
{
}


void PacketProcessor::on_com_fd_data(int, uint32_t events)
{
	if (events & (EPOLLHUP | EPOLLRDHUP))
		throw runtime_error("CPU-interface went offline");

	if (!(events & EPOLLIN))
		return;

	/* Read a packet (assume maximum size of 16k; 'Jumbo-Frames' should not
	 * exceed 9k anyway. */
	char buf[16384];

	struct sockaddr_ll addr{};
	socklen_t addrlen = sizeof(addr);

	ssize_t ret = recvfrom(com_fd.get_fd(), buf, sizeof(buf),
			0, (sockaddr*) &addr, &addrlen);

	if (ret == sizeof(buf))
	{
		fprintf(stderr, "Dropped too big packet\n");
		return;
	}

	/* Some packets of a foreign interface may be captured during the time
	 * period where the socket has been created but not yet bound. */
	if (addr.sll_ifindex != int_idx)
		return;

	/* Parse packet */
	if (ret < 60)
	{
		fprintf(stderr, "Dropped too small packet (%zd octets)\n", ret);
		return;
	}

	auto& hdr = *reinterpret_cast<EthernetHdr*>(buf);
	printf("Received packet on CPU interface: %s, size %zd\n",
		   to_string(hdr).c_str(), ret);
}
