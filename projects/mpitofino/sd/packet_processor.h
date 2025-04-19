/** A module to process packets redirect to the control plane, i.e. to the CPU
 * network interface. */

#ifndef __PACKET_PROCESSOR_H
#define __PACKET_PROCESSOR_H

#include "common/utils.h"
#include "common/epoll.h"
#include "common/timerfd.h"
#include "common/simple_types.h"
#include "state_repository.h"
#include "packet_headers.h"


class PacketProcessor final
{
protected:
	Epoll& epoll;
	StateRepository& st_repo;

	TimerFD discovery_protocol_timer{
		epoll,
		std::bind(&PacketProcessor::on_discovery_protocol_timer, this)
	};

	WrappedFD com_fd;
	int int_idx = 0;

	void cleanup();

	void on_com_fd_data(int, uint32_t events);

	void parse_packet(const char* ptr, size_t size);
	void parse_packet_arp(const EthernetHdr& src_eth_hdr, const char* ptr, size_t size);

	void parse_packet_ipv4(const EthernetHdr& src_eth_hdr, const char* ptr, size_t size);
	void parse_packet_ipv4_icmp(
			const EthernetHdr& src_eth_hdr, const IPv4Hdr& src_ipv4_hdr,
			const char* ptr, size_t size);

	void send_packet(const char* ptr, size_t size);

	void on_discovery_protocol_timer();

public:
	PacketProcessor(StateRepository& st_repo, Epoll& epoll);
	~PacketProcessor();
};

#endif /* __PACKET_PROCESSOR_H */
