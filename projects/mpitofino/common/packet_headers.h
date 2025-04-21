/** Packet headers used by different components */
#ifndef __COMMON_PACKET_HEADERS_H
#define __COMMON_PACKET_HEADERS_H

#include <cstdint>
#include "common/simple_types.h"


#define TCP_PORT_CTRL_COLL 11000


#define UDP_PORT_TDP 0x2000

/* Topology Discovery Protocol (TDP): A Leightweight LLDP alternative
that works over plain UDP sockets and therefore does not require a
sophisticated LLDP setup on the client nodes. */
struct TDPHdr
{
	uint16_t port;
	IPv4Addr ctrl_ip;  // IP address on control network
}
__attribute__((__packed__));

#endif /* __COMMON_PACKET_HEADERS_H */
