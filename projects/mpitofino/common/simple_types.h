#ifndef __SIMPLE_TYPES_H
#define __SIMPLE_TYPES_H

#include <cstdio>
#include <cstdint>
#include <string>

struct MacAddr
{
	uint8_t addr[6];

	inline std::string to_string()
	{
		char buf[18];

		snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
				 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

		return std::string(buf, sizeof(buf) - 1);
	}
};

#endif  /* __SIMPLE_TYPES_H */
