#include "common/utils.h"
#include "common/simple_types.h"

extern "C" {
#include <arpa/inet.h>
}

using namespace std;


string to_string(const EthernetHdr& e)
{
	return "Eth(dst=" + to_string(e.dst) + ", src=" + to_string(e.src) +
		", type=" + to_hex_string(ntohs(e.type));
}


string to_string(const struct sockaddr_in& addr)
{
	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	return buf + ":"s + to_string(ntohs(addr.sin_port));
}
