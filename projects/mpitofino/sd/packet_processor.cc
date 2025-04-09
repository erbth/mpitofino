#include <system_error>
#include <stdexcept>
#include "packet_processor.h"
#include "packet_headers.h"

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
					 bind(&PacketProcessor::on_com_fd_data, this,
						 placeholders::_1, placeholders::_2));
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
		fprintf(stderr, "Dropped too large packet\n");
		return;
	}

	/* Some packets of a foreign interface may be captured during the time
	 * period where the socket has been created but not yet bound. */
	if (addr.sll_ifindex != int_idx)
		return;

	/* Parse packet */
	parse_packet(buf, ret);
}


void PacketProcessor::parse_packet(const char* ptr, size_t size)
{
	if (size < 60)
	{
		fprintf(stderr, "Dropped too small packet (%zd octets)\n", size);
		return;
	}

	auto& hdr = *reinterpret_cast<const EthernetHdr*>(ptr);
	// printf("Received packet on CPU interface: %s, size %zd\n",
	// 	   to_string(hdr).c_str(), size);

	ptr += sizeof(hdr);
	size -= sizeof(hdr);

	switch (ntohs(hdr.type))
	{
	case (int) ether_type_t::ARP:
		parse_packet_arp(hdr, ptr, size);
		break;

	default:
		break;
	}
}

void PacketProcessor::parse_packet_arp(
		const EthernetHdr& src_eth_hdr, const char* ptr, size_t size)
{
	if (size < 28)
	{
		fprintf(stderr, "Received malformed ARP packet\n");
		return;
	}

	auto& hdr = *reinterpret_cast<const ARPHdr*>(ptr);

	auto op = ntohs(hdr.operation);
	if (op == 1)
	{
		/* ARP request */
		if (hdr.target_protocol_address == st_repo.get_collectives_module_ip_addr())
		{
			/* Reply */
			char buf[128];
			char* ptr = buf;

			memset(buf, 0, sizeof(buf));

			/* Control plane offload header */
			//auto& cpo_hdr = *reinterpret_cast<CPOffloadHdr*>(ptr);
			//ptr += sizeof(cpo_hdr);

			/* Ethernet header */
			auto& eth_hdr = *reinterpret_cast<EthernetHdr*>(ptr);
			ptr += sizeof(eth_hdr);

			eth_hdr.src = st_repo.get_collectives_module_mac_addr();
			eth_hdr.dst = src_eth_hdr.src;
			eth_hdr.type = htons((int) ether_type_t::ARP);

			/* ARP header */
			auto& arp_hdr = *reinterpret_cast<ARPHdr*>(ptr);
			ptr += sizeof(arp_hdr);

			arp_hdr.hardware_type = htons(1);
			arp_hdr.protocol_type = htons(0x0800);
			arp_hdr.hardware_address_length = 6;
			arp_hdr.protocol_address_length = 4;
			arp_hdr.operation = htons(2);
			arp_hdr.sender_hardware_address = st_repo.get_collectives_module_mac_addr();
			arp_hdr.sender_protocol_address = st_repo.get_collectives_module_ip_addr();
			arp_hdr.target_hardware_address = hdr.sender_hardware_address;
			arp_hdr.target_protocol_address = hdr.sender_protocol_address;

			send_packet(buf, ptr - buf);
		}
	}
	else if (op == 2)
	{
		/* ARP reply */
	}
}


void PacketProcessor::send_packet(const char* ptr, size_t size)
{
	struct sockaddr_ll addr = {
		.sll_family = AF_PACKET,
		.sll_ifindex = int_idx
	};

	auto ret = sendto(com_fd.get_fd(), ptr, size,
			0, (sockaddr*) &addr, sizeof(addr));

	if (ret != (ssize_t) size)
	{
		fprintf(stderr, "Failed to send packet to dataplane\n");
		return;
	}
}
