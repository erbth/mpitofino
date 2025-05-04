#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "state_repository.h"

using namespace std;


void StateRepository::read_config_file()
{
	/* Determine config file path and open the file */
	vector<string> paths;
	paths.push_back("./sd.conf");
	paths.push_back("/etc/mpitofino/sd.conf");

	ifstream f;
	for (auto& p : paths)
	{
		f = ifstream(p);
		if (f.is_open())
			break;
	}

	if (!f)
		throw runtime_error("Failed to open config file");

	/* Parse file */
	char buf[1024];
	while (f.getline(buf, sizeof(buf)))
	{
		istringstream iss(buf);
		vector<string> parts{
			istream_iterator<string>{iss},
			istream_iterator<string>{}};

		if (parts.empty())
			continue;

		const auto& key = parts[0];

		if (parts.size() != 2)
			throw runtime_error("Expected argument after config key `" + key + "'");

		const auto& value = parts[1];

		if (key == "cpu_interface_name")
		{
			cpu_interface_name = value;
		}
		else if (key == "base_mac_addr")
		{
			base_mac_addr = MacAddr(value);
		}
		else if (key == "collectives_module_ip_addr")
		{
			collectives_module_ip_addr = IPv4Addr(value);
		}
		else if (key == "collectives_module_broadcast_addr")
		{
			collectives_module_broadcast_addr = IPv4Addr(value);
		}
		else if (key == "control_ip_addr")
		{
			control_ip_addr = IPv4Addr(value);
		}
		else
		{
			throw runtime_error("invalid config key `" + key + "'");
		}
	}
}


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

void StateRepository::remove_channel(uint64_t tag)
{
	auto i = channels.find(tag);
	if (i == channels.end())
		throw invalid_argument("No such channel");

	channels.erase(i);

	notify_subscribers(subscribers_channels);
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

uint16_t StateRepository::get_free_agg_unit()
{
	uint16_t unit = 0;

	/* TODO make table sizes etc. consistent and put the correct
	capacity here */
	for (; unit < 256; unit++)
	{
		bool taken = false;
		for (auto& [t,c] : channels)
		{
			if (c.agg_unit == unit)
			{
				taken = true;
				break;
			}
		}

		if (!taken)
			return unit;
	}

	throw runtime_error("No free aggregation unit");
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


string StateRepository::get_cpu_interface_name()
{
	return cpu_interface_name;
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
