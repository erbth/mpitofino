#ifndef __COMMON_SIMPLE_TYPES_H
#define __COMMON_SIMPLE_TYPES_H

#include <cstdio>
#include <cstdint>
#include <string>

struct MacAddr
{
	uint8_t addr[6];
} __attribute__((__packed__));

struct EthernetHdr
{
	MacAddr dst;
	MacAddr src;
	uint16_t type;
} __attribute__((__packed__));


/* A comparison function S.t. MacAddr can be used in RB-tree maps */
inline int operator<=>(const MacAddr& a, const MacAddr& b)
{
	for (size_t i = 0; i < sizeof(a.addr); i++)
	{
		if (a.addr[i] < b.addr[i])
			return -1;
		else if (a.addr[i] > b.addr[i])
			return 1;
	}

	/* Equality */
	return 0;
}

/* to_string functions for datatypes */
inline std::string to_string(const MacAddr& a)
{
	char buf[18];

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
			 a.addr[0], a.addr[1], a.addr[2], a.addr[3], a.addr[4], a.addr[5]);

	return std::string(buf, sizeof(buf) - 1);
}

std::string to_string(const EthernetHdr& e);

#endif  /* __COMMON_SIMPLE_TYPES_H */
