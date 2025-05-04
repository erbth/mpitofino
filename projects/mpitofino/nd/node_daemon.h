#ifndef __ND_NODE_DAEMON_H
#define __ND_NODE_DAEMON_H

#include <filesystem>
#include <functional>
#include <string>
#include <list>
#include <tuple>
#include <map>
#include <set>
#include "common/epoll.h"
#include "common/signalfd.h"
#include "common/utils.h"
#include "common/simple_types.h"
#include "client_lib_nd.pb.h"

extern "C" {
#include <netinet/in.h>
}


struct Client final
{
	WrappedFD wfd;

	Client(WrappedFD&& wfd);
	Client(Client&&) = delete;

	/* tag -> local port */
	std::map<uint64_t, uint16_t> channels;

	/* (client_id, tag) -> get_channel response */
	std::map<
		std::tuple<uint64_t, uint64_t>,
		GetChannelResponse>
	pending_get_channel_responses;

	inline int get_fd()
	{
		return wfd.get_fd();
	}
};


class NodeDaemon final
{
protected:
	Epoll epoll;
	SignalFD sfd{{SIGINT, SIGTERM}, epoll,
		         std::bind(&NodeDaemon::on_signal, this, std::placeholders::_1)};

	/* Unix domain socket for local communication */
	std::filesystem::path service_socket_path;
	WrappedFD service_wfd;

	/* High performance network (hpn) */
	struct sockaddr_in hpn_node_addr{};
	MacAddr hpn_node_mac;

	/* UDP socket to receive TDP PDUs */
	WrappedFD tdp_wfd;

	/* The hpn switch to which this node is connected. If the switch *
	 * is not known (yet), this shall be 0.0.0.0. */
	IPv4Addr nearest_switch_ip;
	int switch_port{};

	/* The connection to the nearest switch; i.e. the connection to
	the rest of the control plane */
	WrappedFD switch_wfd;


	bool running = true;
	void on_signal(int signo);


	/* Client management */
	/* Use a std::list s.t. the memory addresses stay constant */
	std::list<Client> clients;

	void on_client_conn(int fd, uint32_t events);

	void on_client_fd(Client* c, int fd, uint32_t events);
	void on_client_get_channel(Client* c, const GetChannel& msg);

	/* Local port allocator */
	uint16_t next_coll_port{};
	uint16_t get_free_coll_port();


	/* Topology discovery/change */
	void initialize_tdp();
	void on_tdp_fd(int _fd, uint32_t events);

	void switch_changed();
	void connect_to_switch();
	void disconnect_from_switch();

	void on_switch_fd(int fd, uint32_t events);


public:
	NodeDaemon();
	NodeDaemon(NodeDaemon&&) = delete;

	/* Main loop */
	void main();
};

#endif /* __ND_NODE_DAEMON_H */
