#include <cstdio>
#include "common/utils.h"

extern "C" {
#include <arpa/inet.h>
}

using namespace std;


string to_hex_string(int i)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "0x%x", i);
	buf[sizeof(buf) - 1] = 0;

	return string(buf);
}


uint16_t internet_checksum(const char* data, size_t size)
{
	/* Taken from RFC1071 */
	uint32_t sum = 0;

	while (size > 1)
	{
		sum += *((const uint16_t*) data);

		data += 2;
		size -= 2;
	}

	if (size > 0)
		sum += *((const uint8_t*) data);

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}
