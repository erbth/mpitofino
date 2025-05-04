#ifndef __STATE_REPOSITORY_H
#define __STATE_REPOSITORY_H

#include <cstdint>
#include <functional>
#include <map>
#include <vector>
#include <list>
#include "common/simple_types.h"
#include "common.pb.h"

/* Design:
 *
 * Currently, all datasets are implemented directly with dataset-specific
 * methods for accessing them. This does not scale as the number of datasets
 * grows, but we'll cross that bridge should we get there. */

struct CollectiveChannel final
{
	uint64_t tag{};
	IPv4Addr fabric_ip;
	uint16_t fabric_port{};
	MacAddr fabric_mac;

	uint16_t agg_unit{};
	
	struct Participant
	{
		uint64_t client_id{};
		IPv4Addr ip;
		uint16_t port{};
		MacAddr mac;
		uint16_t switch_port{};
	};

	/* client-id -> Participant */
	std::map<uint64_t, Participant> participants;

	int type{};
};


class StateRepository final
{
public:
	using subscriber_t = std::function<void()>;
	/* A handle to unsubscribe */
	using subscription_t = void*;

protected:
	/* List provides stable pointers */
	using subscriber_list_t = std::list<subscriber_t>;

	inline void notify_subscribers(subscriber_list_t& l)
	{
		for (auto& s : l)
			s();
	}


	/* Collective channels */
	/* tag -> channel */
	std::map<uint64_t, CollectiveChannel> channels;

	uint16_t next_coll_port{};

	subscriber_list_t subscribers_channels;


	/* Also basic switch configuration, but not chassis-dependent */
	std::string cpu_interface_name;
	
	/* Basic switch configuration */
	/* Meant as base MAC address from which all of the system's MAC addresses
	 * are derived (e.g. addresses per port etc.) */
	MacAddr base_mac_addr;
	IPv4Addr collectives_module_ip_addr;
	IPv4Addr collectives_module_broadcast_addr;

	IPv4Addr control_ip_addr;

public:
	void read_config_file();


	/* Collective channels */
	const CollectiveChannel* get_channel(uint64_t tag);
	std::vector<const CollectiveChannel*> get_channels();

	/* Does only add the channel, but not fill any fields */
	void add_channel(const CollectiveChannel& channel);
	void update_channel_participant(
		uint64_t tag, uint64_t client_id, IPv4Addr ip, uint16_t port,
		MacAddr mac, uint16_t switch_port);

	void remove_channel(uint64_t tag);

	uint16_t get_free_coll_port();
	uint16_t get_free_agg_unit();

	/* publish/subscribe */
	void* subscribe_channels(subscriber_t sub);
	void unsubscribe_channels(void* handle);


	std::string get_cpu_interface_name();
	
	MacAddr get_base_mac_addr();

	/* Derived MAC address of the collectives-unit */
	MacAddr get_collectives_module_mac_addr();

	/* IP-address of the collectives module */
	IPv4Addr get_collectives_module_ip_addr();
	IPv4Addr get_collectives_module_broadcast_addr();

	/* IP-address of the control plane */
	IPv4Addr get_control_ip_addr();
};

#endif /* __STATE_REPOSITORY_H */
