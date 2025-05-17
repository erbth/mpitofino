#include <cstdio>
#include <infiniband/verbs.h>
#include "ib_utils.h"

using namespace std;


string format_ib_guid(uint64_t guid)
{
	string s;
	
	for (int i = 0; i < 8; i++)
	{
		auto val = (guid >> i * 8UL) & 0xffUL;
		char buf[5];
		snprintf(buf, sizeof(buf), "%02x", (unsigned) val);
		buf[4] = '\0';
		s += string(buf);

		if (i % 2 == 1 && i < 7)
			s += ":"s;
	}

	return s;
}
