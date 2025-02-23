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
