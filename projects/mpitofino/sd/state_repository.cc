#include <algorithm>
#include "state_repository.h"


void StateRepository::switching_table_add_entry(
	const MacAddr& addr, uint16_t port)
{
	switching_table[addr] = port;
}

void StateRepository::switching_table_remove_entry(const MacAddr& addr)
{
	auto i = switching_table.find(addr);
	if (i != switching_table.end())
		switching_table.erase(i);
}

void StateRepository::switching_table_clear()
{
	switching_table.clear();
}

StateRepository::switching_table_t StateRepository::switching_table_get() const
{
	return switching_table;
}

StateRepository::subscription_t
StateRepository::subscribe_switching_table(subscriber_t s)
{
	return reinterpret_cast<void*>(&subscribers_switching_table.emplace_back(s));
}

void StateRepository::unsubscribe_switching_table(subscription_t s)
{
	auto i = find_if(
		subscribers_switching_table.begin(),
		subscribers_switching_table.end(),
		[s](auto& e){ return &e == s; });

	if (i != subscribers_switching_table.end())
		subscribers_switching_table.erase(i);
}
