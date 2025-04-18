#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include "common/com_utils.h"
#include "node_daemon.h"

extern "C" {
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <net/if.h>
}

using namespace std;
namespace fs = std::filesystem;


Client::Client(WrappedFD&& wfd)
	: wfd(move(wfd))
{
}


NodeDaemon::NodeDaemon()
{
	/* Identify interface of high performance network. For now, simply
	 * take the last interface; however obiously a more sophisticated
	 * heuristic would be required. */
	struct ifaddrs* ifa = nullptr;
	check_syscall(getifaddrs(&ifa), "getifaddrs");

	FINALLY(
	{
		for (auto i = ifa; i != nullptr; i = i->ifa_next)
		{
			if (!(i->ifa_flags & IFF_UP))
				continue;

			if (i->ifa_flags & IFF_LOOPBACK)
				continue;

			if (!i->ifa_addr)
				continue;

			if (i->ifa_addr->sa_family != AF_INET)
				continue;

			hpn_node_addr = *((struct sockaddr_in*) i->ifa_addr);
		}
	},
	{
		freeifaddrs(ifa);
	});
	

	/* Create unix domain socket */
	/* Try /run/mpitofino-nd (only works if the nd is run with root
	   privileges) and $XDG_RUNTIME_DIR/mpitofino-nd */
	vector<fs::path> socket_cand;
	socket_cand.push_back("/run/mpitofino-sd");

	auto val = getenv("XDG_RUNTIME_DIR");
	if (val && strlen(val) > 0 && val[0] == '/')
		socket_cand.push_back(string(val) + "/mpitofino-sd");

	for (const auto& s : socket_cand)
	{
		WrappedFD fd;
		fd.set_errno(
			socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0),
			"socket(AF_UNIX)");

		/* Try to unlink the socket-file in case it exists already */
		auto sock_stat = fs::symlink_status(s);
		if (fs::exists(s))
		{
			if (!is_socket(s))
				continue;

			auto ret = unlink(s.c_str());
			if (ret < 0)
			{
				if (errno == EACCES)
					continue;

				check_syscall(-1, "unlink(unix domain socket)");
			}
		}

		/* Try to bind socket */
		struct sockaddr_un addr = {
			.sun_family = AF_UNIX
		};

		strncpy(addr.sun_path, s.c_str(), sizeof(addr.sun_path));
		addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

		if (::bind(fd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)) < 0)
		{
			if (errno == EACCES)
				continue;

			check_syscall(-1, "bind(unix domain socket)");
		}

		/* Listen */
		check_syscall(listen(fd.get_fd(), 10), "listen(unix domain socket)");

		/* Socket ready */
		service_fd = move(fd);
		service_socket_path = s;
	}

	if (!service_fd)
		throw runtime_error("Unable to find suitable unix domain socket path.");

	fprintf(stderr, "Using unix domain socket at `%s'.\n",
			service_socket_path.c_str());

	/* Add socket to epoll instance */
	epoll.add_fd(service_fd.get_fd(), EPOLLIN,
				 bind_front(&NodeDaemon::on_client_conn, this));
}


void NodeDaemon::on_signal(int signo)
{
	running = false;
	fprintf(stderr, "Exiting gracefully due to signal\n");
}


void NodeDaemon::on_client_conn(int fd, uint32_t events)
{
	WrappedFD wfd;
	
	/* Should not happen */
	if (!(events & EPOLLIN))
		return;
	
	/* Accept connection */
	wfd.set_errno(
		accept4(fd, nullptr, nullptr, SOCK_CLOEXEC),
		"accept4(unix domain socket)");

	/* Create new client */
	auto& c = clients.emplace_back(move(wfd));

	try
	{
		epoll.add_fd(c.get_fd(), EPOLLIN | EPOLLHUP | EPOLLRDHUP,
					 bind_front(&NodeDaemon::on_client_fd, this, &c));
	}
	catch (...)
	{
		clients.pop_back();
		throw;
	}

	printf("New client connected on fd %d\n", clients.back().get_fd());
}


void NodeDaemon::on_client_fd(Client* c, int fd, uint32_t events)
{
	bool remove_client = events & (EPOLLHUP | EPOLLRDHUP);

	if (events & EPOLLIN)
	{
		/* Receive datagram */
		auto msg = recv_protobuf_message_simple_dgram<ClientRequest>(c->get_fd());
		switch (msg.messages_case())
		{
		case ClientRequest::kGetChannel:
			on_client_get_channel(c, msg.get_channel());
			break;

		case ClientRequest::MESSAGES_NOT_SET:
			/* EOF */
			remove_client = true;
			break;
			
		default:
			throw runtime_error("Unsupported message from client");
		}
	}


	if (remove_client)
	{
		printf("Client with fd %d disconnected\n", c->get_fd());

		auto i = find_if(clients.begin(), clients.end(), [=](auto& c2) { return &c2 == c; });
		if (i != clients.end())
		{
			epoll.remove_fd(c->get_fd());
			clients.erase(i);
		}
	}
}

void NodeDaemon::on_client_get_channel(Client* c, const GetChannel& msg)
{
	/* TODO: Request channel from switch */
	
	/* Reply */
	GetChannelResponse reply;

	reply.set_local_port(0x2000);
	reply.set_local_ip(hpn_node_addr.sin_addr.s_addr);

	reply.set_fabric_port(0x4000);
	reply.set_fabric_ip(0x0a0a8000);

	send_protobuf_message_simple(c->get_fd(), reply);
}


void NodeDaemon::main()
{
	while (running)
		epoll.process_events(-1);
}
