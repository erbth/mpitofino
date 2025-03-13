#include <cstdio>
#include "common/utils.h"

using namespace std;


string to_hex_string(int i)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "0x%x", i);
	buf[sizeof(buf) - 1] = 0;

	return string(buf);
}
