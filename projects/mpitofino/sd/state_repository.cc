#include <algorithm>
#include <stdexcept>
#include "state_repository.h"

using namespace std;


const CollectiveChannel* StateRepository::get_channel(uint64_t tag)
{
	auto i = channels.find(tag);
	if (i == channels.end())
		return nullptr;
	return &i->second;
}

vector<const CollectiveChannel*> StateRepository::get_channels()
{
	vector<const CollectiveChannel*> res;

	for (auto& [t,c] : channels)
		res.push_back(&c);

	return res;
}

void StateRepository::add_channel(const CollectiveChannel& channel)
{
	auto [i,inserted] = channels.insert({channel.tag, channel});
	if (!inserted)
		throw invalid_argument("channel exists already");

	notify_subscribers(subscribers_channels);
}

void StateRepository::update_channel_participant(
	uint64_t tag, uint64_t client_id, IPv4Addr ip, uint16_t port,
	MacAddr mac, uint16_t switch_port)
{
	auto i = channels.find(tag);
	if (i == channels.end())
		throw invalid_argument("No such channel");

	auto& c = i->second;

	auto j = c.participants.find(client_id);
	if (j == c.participants.end())
		throw invalid_argument("No such participant in channel");

	auto& p = j->second;

	if (p.ip != ip || p.port != port || p.mac != mac ||
		p.switch_port != switch_port)
	{
		p.ip = ip;
		p.port = port;
		p.mac = mac;
		p.switch_port = switch_port;

		notify_subscribers(subscribers_channels);
	}
}


uint16_t StateRepository::get_free_coll_port()
{
	int cnt_wraps = 0;
	
	while (cnt_wraps < 2)
	{
		if (next_coll_port < 0x4000 || next_coll_port >= 0x8000)
		{
			next_coll_port = 0x4000;
			cnt_wraps++;
		}

		auto port = next_coll_port++;

		bool taken = false;
		for (auto& [t,c] : channels)
		{
			if (c.fabric_port == port)
			{
				taken = true;
				break;
			}
		}

		if (!taken)
			return port;
	}

	throw runtime_error("No free fabric port for collectives");
}


void* StateRepository::subscribe_channels(subscriber_t sub)
{
	return (void*) &subscribers_channels.emplace_back(sub);
}

void StateRepository::unsubscribe_channels(void* handle)
{
	auto i = find_if(
		subscribers_channels.begin(), subscribers_channels.end(),
		[=](auto& s){ return &s == (subscriber_t*) handle; });

	if (i == subscribers_channels.end())
		throw invalid_argument("No such subscription");

	subscribers_channels.erase(i);
}


MacAddr StateRepository::get_base_mac_addr()
{
	return base_mac_addr;
}

MacAddr StateRepository::get_collectives_module_mac_addr()
{
	return base_mac_addr + 1024;
}

IPv4Addr StateRepository::get_collectives_module_ip_addr()
{
	return collectives_module_ip_addr;
}

IPv4Addr StateRepository::get_collectives_module_broadcast_addr()
{
	return collectives_module_broadcast_addr;
}

IPv4Addr StateRepository::get_control_ip_addr()
{
	return control_ip_addr;
}
