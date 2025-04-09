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

#endif /* __PACKET_HEADER_H */
