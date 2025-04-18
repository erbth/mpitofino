#ifndef __ND_NODE_DAEMON_H
#define __ND_NODE_DAEMON_H

#include <filesystem>
#include <functional>
#include <string>
#include <list>
#include "common/epoll.h"
#include "common/signalfd.h"
#include "common/utils.h"
#include "client_lib_nd.pb.h"

extern "C" {
#include <netinet/in.h>
}


struct Client final
{
	WrappedFD wfd;

	Client(WrappedFD&& wfd);
	Client(Client&&) = delete;

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
		         std::bind_front(&NodeDaemon::on_signal, this)};

	/* FD for Unix domain socket for local communication */
	std::filesystem::path service_socket_path;
	WrappedFD service_fd;

	/* High performance network */
	struct sockaddr_in hpn_node_addr;


	bool running = true;
	void on_signal(int signo);


	/* Client management */
	/* Use a std::list s.t. the memory addresses stay constant */
	std::list<Client> clients;

	void on_client_conn(int fd, uint32_t events);

	void on_client_fd(Client* c, int fd, uint32_t events);
	void on_client_get_channel(Client* c, const GetChannel& msg);


public:
	NodeDaemon();
	NodeDaemon(NodeDaemon&&) = delete;

	/* Main loop */
	void main();
};

#endif /* __ND_NODE_DAEMON_H */
