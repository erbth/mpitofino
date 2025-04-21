#ifndef __COMMON_SIMPLE_TYPES_H
#define __COMMON_SIMPLE_TYPES_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <regex>
#include <stdexcept>
#include <string>

extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
}


struct MacAddr
{
	uint8_t addr[6]{};

	inline MacAddr() = default;

	inline MacAddr(std::string s)
	{
		const std::regex re("([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2})");
		std::smatch m;
		if (!regex_match(s, m, re))
			throw std::runtime_error("Invalid format for MAC address");

		for (int i = 1; i <= 6; i++)
		{
			auto c1 = m[i].str()[0];
			auto c2 = m[i].str()[1];

			if (c1 >= 'a' && c1 <= 'f')
				c1 -= 'a' - 0xa;
			else if (c1 >= 'A' && c1 <= 'F')
				c1 -= 'A' - 0xa;
			else
				c1 -= '0';

			if (c2 >= 'a' && c2 <= 'f')
				c2 -= 'a' - 0xa;
			else if (c2 >= 'A' && c2 <= 'F')
				c2 -= 'A' - 0xa;
			else
				c2 -= '0';

			addr[i-1] = ((uint8_t) c1 << 4) | ((uint8_t) c2);
		}
	}

	/* Useful for deriving MAC addresses */
	inline MacAddr operator+(unsigned offset)
	{
		MacAddr n = *this;

		unsigned carry = 0;
		for (int i = 5; i >= 0; i--)
		{
			unsigned v = offset & 0xff;
			offset >>= 8;

			v += carry + n.addr[i];
			n.addr[i] = v & 0xff;

			carry = v >> 8;
		}

		return n;
	}

} __attribute__((__packed__));

/* Make sure that there's no vtable or anything s.t. type punning works as
 * expected */
static_assert(sizeof(MacAddr) == 6);

struct IPv4Addr
{
	uint8_t addr[4]{};

	inline IPv4Addr() = default;

	inline IPv4Addr(std::string s)
	{
		const std::regex re("([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3}).([0-9]{1,3})");
		std::smatch m;
		if (!regex_match(s, m, re))
			throw std::runtime_error("Invalid format for IPv4 address");

		for (int i = 1; i <= 4; i++)
		{
			auto v = atoi(m[i].str().c_str());
			if (v < 0 || v > 255)
				throw std::runtime_error("Invalid format for IPv4 address");

			addr[i-1] = v;
		}
	}

	inline bool operator==(const IPv4Addr& o) const
	{
		return memcmp(addr, o.addr, 4) == 0;
	}

	inline bool is_0000() const
	{
		return addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0;
	}
}
__attribute__((__packed__));

struct EthernetHdr
{
	MacAddr dst;
	MacAddr src;
	uint16_t type;
} __attribute__((__packed__));


/* A comparison function S.t. MacAddr can be used in RB-tree maps */
inline int operator<(const MacAddr& a, const MacAddr& b)
{
	for (size_t i = 0; i < sizeof(a.addr); i++)
	{
		if (a.addr[i] < b.addr[i])
			return true;
		else if (a.addr[i] > b.addr[i])
			return false;
	}

	/* Equality */
	return false;
}

/* to_string functions for datatypes */
inline std::string to_string(const MacAddr& a)
{
	char buf[18];

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
			 a.addr[0], a.addr[1], a.addr[2], a.addr[3], a.addr[4], a.addr[5]);

	return std::string(buf, sizeof(buf) - 1);
}

inline std::string to_string(const IPv4Addr& addr)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
			(unsigned) addr.addr[0], (unsigned) addr.addr[1],
			(unsigned) addr.addr[2], (unsigned) addr.addr[3]);

	buf[sizeof(buf) - 1] = 0;
	return std::string(buf);
}

std::string to_string(const EthernetHdr& e);

std::string to_string(const struct sockaddr_in& addr);

#endif  /* __COMMON_SIMPLE_TYPES_H */
