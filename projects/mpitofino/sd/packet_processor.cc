#include <algorithm>
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
		int_idx = if_nametoindex(st_repo.get_cpu_interface_name().c_str());
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

		/* Start timer */
		discovery_protocol_timer.start(10 * 1000 * 1000);
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

	case (int) ether_type_t::IPv4:
		parse_packet_ipv4(hdr, ptr, size);
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
			auto& cpo_hdr = *reinterpret_cast<CPOffloadHdr*>(ptr);
			ptr += sizeof(cpo_hdr);

			/* Use switching table */
			cpo_hdr.port_id = htons(65535);

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

void PacketProcessor::parse_packet_ipv4(
		const EthernetHdr& src_eth_hdr, const char* ptr, size_t size)
{
	if (size < 20)
	{
		fprintf(stderr, "Received malformed IPv4 packet\n");
		return;
	}

	auto& hdr = *reinterpret_cast<const IPv4Hdr*>(ptr);
	ptr += sizeof(hdr);
	size -= sizeof(hdr);

	switch (hdr.protocol)
	{
	case (int) ipv4_protocol_t::ICMP:
		parse_packet_ipv4_icmp(src_eth_hdr, hdr, ptr, size);
		break;

	default:
		break;
	}
}

void PacketProcessor::parse_packet_ipv4_icmp(
		const EthernetHdr& src_eth_hdr, const IPv4Hdr& src_ipv4_hdr,
		const char* ptr, size_t size)
{
	if (size < 8)
	{
		fprintf(stderr, "Received malformed ICMP packet\n");
		return;
	}

	auto& hdr = *reinterpret_cast<const ICMPHdr*>(ptr);
	ptr += sizeof(hdr);
	size -= sizeof(hdr);

	if (hdr.type == 8 && hdr.code == 0)
	{
		/* Echo Request */
		char buf[1024];
		char* reply_ptr = buf;

		memset(buf, 0, sizeof(buf));

		/* Control plane offload header */
		auto& cpo_hdr = *reinterpret_cast<CPOffloadHdr*>(reply_ptr);
		reply_ptr += sizeof(cpo_hdr);

		/* Use switching table */
		cpo_hdr.port_id = htons(65535);

		/* Ethernet header */
		auto& eth_hdr = *reinterpret_cast<EthernetHdr*>(reply_ptr);
		reply_ptr += sizeof(eth_hdr);

		eth_hdr.src = st_repo.get_collectives_module_mac_addr();

		/* NOTE: This is actually wrong. The (currently non-existent) ARP
		 * neighbor table shall be used here to (a) be standards compliant and
		 * (b) support setups which relay on it. */
		eth_hdr.dst = src_eth_hdr.src;

		eth_hdr.type = htons((int) ether_type_t::IPv4);

		/* IPv4 header */
		auto& ipv4_hdr = *reinterpret_cast<IPv4Hdr*>(reply_ptr);
		reply_ptr += sizeof(ipv4_hdr);

		ipv4_hdr.version_ihl = 0x45;
		ipv4_hdr.tos = 0;
		ipv4_hdr.identification = 0x1234;
		ipv4_hdr.flags_fragment_offset = 0;
		ipv4_hdr.ttl = 16;
		ipv4_hdr.protocol = (int) ipv4_protocol_t::ICMP;
		ipv4_hdr.src_addr = st_repo.get_collectives_module_ip_addr();
		ipv4_hdr.dst_addr = src_ipv4_hdr.src_addr;

		/* ICMP header */
		auto& icmp_hdr = *reinterpret_cast<ICMPHdr*>(reply_ptr);
		reply_ptr += sizeof(icmp_hdr);

		icmp_hdr.type = 0;
		icmp_hdr.code = 0;
		icmp_hdr.id = hdr.id;
		icmp_hdr.seq = hdr.seq;

		auto data_size = min(sizeof(buf) - (reply_ptr - buf), size);
		memcpy(reply_ptr, ptr, data_size);

		reply_ptr += data_size;

		/* Set checksum */
		icmp_hdr.checksum = internet_checksum(
				(const char*) &icmp_hdr,
				reply_ptr - (char*) &icmp_hdr);


		/* Set IPv4 total length */
		ipv4_hdr.total_length = htons(reply_ptr - buf -
									  sizeof(eth_hdr) - sizeof(cpo_hdr));

		ipv4_hdr.header_checksum = internet_checksum(
				(const char*) &ipv4_hdr, sizeof(ipv4_hdr));

		send_packet(buf, reply_ptr - buf);
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


void PacketProcessor::on_discovery_protocol_timer()
{
	/* Ideally one would use LLDP, but this requires a LLDP setup on the
	   clients, which complicates the test setup more than required. */

	char buf[1024];
	memset(buf, 0, sizeof(buf));

	/* TODO: Query state_repo for active ports and use those */
	for (int port_id = 0; port_id < 4; port_id++)
	{
		char* ptr = buf;

		/* Control plane offload header */
		auto& cpo_hdr = *reinterpret_cast<CPOffloadHdr*>(ptr);
		ptr += sizeof(cpo_hdr);

		cpo_hdr.port_id = htons(port_id * 4);


		/* Ethernet header */
		auto& eth_hdr = *reinterpret_cast<EthernetHdr*>(ptr);
		ptr += sizeof(eth_hdr);

		eth_hdr.src = st_repo.get_collectives_module_mac_addr();
		eth_hdr.dst = MacAddr("ff:ff:ff:ff:ff:ff");
		eth_hdr.type = htons((int) ether_type_t::IPv4);


		/* IPv4 header */
		auto& ipv4_hdr = *reinterpret_cast<IPv4Hdr*>(ptr);
		ptr += sizeof(ipv4_hdr);

		ipv4_hdr.version_ihl = 0x45;
		ipv4_hdr.tos = 0;
		ipv4_hdr.identification = 0x1234;
		ipv4_hdr.flags_fragment_offset = 0;
		ipv4_hdr.ttl = 16;
		ipv4_hdr.protocol = (int) ipv4_protocol_t::UDP;
		ipv4_hdr.src_addr = st_repo.get_collectives_module_ip_addr();
		ipv4_hdr.dst_addr = st_repo.get_collectives_module_broadcast_addr();


		/* UDP header */
		auto& udp_hdr = *reinterpret_cast<UDPHdr*>(ptr);
		ptr += sizeof(udp_hdr);

		udp_hdr.src_port = htons(UDP_PORT_TDP);
		udp_hdr.dst_port = htons(UDP_PORT_TDP);


		/* TDP header */
		auto& tdp_hdr = *reinterpret_cast<TDPHdr*>(ptr);
		ptr += sizeof(tdp_hdr);

		tdp_hdr.port = htons(port_id);
		tdp_hdr.ctrl_ip = st_repo.get_control_ip_addr();


		/* Set UDP length */
		udp_hdr.length = htons(ptr - (const char*) &udp_hdr);
		
		/* Set IPv4 total length */
		ipv4_hdr.total_length = htons(ptr - buf -
									  sizeof(eth_hdr) - sizeof(cpo_hdr));

		ipv4_hdr.header_checksum = internet_checksum(
				(const char*) &ipv4_hdr, sizeof(ipv4_hdr));

		send_packet(buf, ptr - buf);
	}
}
