#ifndef __STATE_REPOSITORY_H
#define __STATE_REPOSITORY_H

#include <functional>
#include <map>
#include <list>
#include "../common/simple_types.h"

/* Design:
 *
 * Currently, all datasets are implemented directly with dataset-specific
 * methods for accessing them. This does not scale as the number of datasets
 * grows, but we'll cross that bridge as we get there. */

class StateRepository final
{
public:
	using switching_table_t = std::map<MacAddr, uint16_t>;

	using subscriber_t = std::function<void()>;
	/* A handle to unsubscribe */
	using subscription_t = void*;

protected:
	using subscriber_list_t = std::vector<subscriber_t>;

	switching_table_t switching_table;
	subscriber_list_t subscribers_switching_table;

	/* Basic switch configuration */
	/* Meant as base MAC address from which all of the system's MAC addresses
	 * are derived (e.g. addresses per port etc.) */
	MacAddr base_mac_addr{"02:80:00:00:00:00"};
	IPv4Addr collectives_module_ip_addr{"10.10.128.0"};

public:
	void switching_table_add_entry(const MacAddr& addr, uint16_t port);
	void switching_table_remove_entry(const MacAddr& addr);
	void switching_table_clear();
	switching_table_t switching_table_get() const;

	subscription_t subscribe_switching_table(subscriber_t);
	void unsubscribe_switching_table(subscription_t);

	MacAddr get_base_mac_addr();

	/* Derived MAC address of the collectives-unit */
	MacAddr get_collectives_module_mac_addr();

	/* IP-address of the collectives module */
	IPv4Addr get_collectives_module_ip_addr();
};

#endif /* __STATE_REPOSITORY_H */
