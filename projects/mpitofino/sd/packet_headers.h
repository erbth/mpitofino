#ifndef __PACKET_HEADER_H
#define __PACKET_HEADER_H

#include <cstdio>
#include <string>

enum class ether_type_t : uint16_t
{
	IPv4 = 0x0800,
	ARP  = 0x0806
};

/* A header for "offloading" packets to/from the control plane from/to the
 * dataplane. */
struct CPOffloadHdr
{
	uint16_t port_id;  // Tofino port id (not front panel port)
}
__attribute__((__packed__));

struct ARPHdr
{
	uint16_t hardware_type;            // always 0x0001 (ethernet)
	uint16_t protocol_type;            // 0x0800 (ipv4)
	uint8_t  hardware_address_length;  // 6
	uint8_t  protocol_address_length;  // 4
	uint16_t operation;                // 0x0001: request, 0x0002: reply
	MacAddr  sender_hardware_address;
	IPv4Addr sender_protocol_address;
	MacAddr  target_hardware_address;
	IPv4Addr target_protocol_address;
}
__attribute__((__packed__));


enum class ipv4_protocol_t : uint8_t
{
	ICMP = 0x01
};

struct IPv4Hdr
{
	uint8_t version_ihl;
	uint8_t tos;
	uint16_t total_length;
	uint16_t identification;
	uint16_t flags_fragment_offset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t header_checksum;
	IPv4Addr src_addr;
	IPv4Addr dst_addr;
}
__attribute__((__packed__));


struct ICMPHdr
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t id;
	uint16_t seq;
}
__attribute__((__packed__));

#endif /* __PACKET_HEADER_H */
