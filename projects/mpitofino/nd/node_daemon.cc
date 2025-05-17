#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "common/com_utils.h"
#include "common/packet_headers.h"
#include "ctrl_sd.pb.h"
#include "node_daemon.h"

extern "C" {
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/udp.h>
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
		/* Only used to access ioctls */
		WrappedFD aux_sock;
		aux_sock.set_errno(socket(AF_INET, SOCK_DGRAM, 0), "socket");

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

			/* Query MAC address of interface */
			struct ifreq req{};
			memcpy(req.ifr_name, i->ifa_name, max(strlen(i->ifa_name), (size_t) IFNAMSIZ));
			req.ifr_name[IFNAMSIZ - 1] = '\0';

			check_syscall(
				ioctl(aux_sock.get_fd(), SIOCGIFHWADDR, &req),
				"ioctl(SIOCGIFHWADDR)");

			memcpy(&hpn_node_mac, req.ifr_hwaddr.sa_data, sizeof(hpn_node_mac));
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
		WrappedFD wfd;
		wfd.set_errno(
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

		if (::bind(wfd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)) < 0)
		{
			if (errno == EACCES)
				continue;

			check_syscall(-1, "bind(unix domain socket)");
		}

		/* Listen */
		check_syscall(listen(wfd.get_fd(), 10), "listen(unix domain socket)");

		/* Socket ready */
		service_wfd = move(wfd);
		service_socket_path = s;
	}

	if (!service_wfd)
		throw runtime_error("Unable to find suitable unix domain socket path.");

	fprintf(stderr, "Using unix domain socket at `%s'.\n",
			service_socket_path.c_str());

	/* Add socket to epoll instance */
	epoll.add_fd(service_wfd.get_fd(), EPOLLIN,
				 bind(&NodeDaemon::on_client_conn, this,
					 placeholders::_1, placeholders::_2));

	initialize_tdp();
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
					 bind(&NodeDaemon::on_client_fd, this, &c,
						 placeholders::_1, placeholders::_2));
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

			/* Check if the channels used by the client are still in
			   use by other clients, otherwise unref them at the
			   switch. */
			for (auto [ch_tag,ch_local_port] : c->channels)
			{
				size_t users = 0;

				for (auto& c2 : clients)
				{
					if (&c2 == c)
						continue;

					if (c2.channels.find(ch_tag) != c2.channels.end())
						users++;
				}

				if (users == 0)
				{
					proto::ctrl_sd::NdRequest switch_req;
					auto uc = switch_req.mutable_unref_channel();

					uc->set_tag(ch_tag);

					send_protobuf_message_simple_stream(switch_wfd.get_fd(), switch_req);
				}
			}

			clients.erase(i);
		}
	}
}

void NodeDaemon::on_client_get_channel(Client* c, const GetChannel& msg)
{
	/* If the switch is not known yet, ignore the request (IMPROVE:
	 * delay the request until the switch is known) */
	if (nearest_switch_ip.is_0000())
		return;


	c->channels.insert({msg.tag(), msg.local_qp()});

	/* Request channel from switch */
	proto::ctrl_sd::NdRequest switch_req;
	auto sgc = switch_req.mutable_get_channel();
	
	for (auto& cid : msg.agg_group_client_ids())
		sgc->add_agg_group_client_ids(cid);
	
	sgc->set_client_id(msg.client_id());
	sgc->set_tag(msg.tag());
	sgc->set_type(msg.type());

	uint64_t client_mac;
	memcpy(&client_mac, &hpn_node_mac, sizeof(hpn_node_mac));

	sgc->set_client_qp(msg.local_qp());
	sgc->set_client_ip(hpn_node_addr.sin_addr.s_addr);
	sgc->set_client_mac(client_mac);
	sgc->set_switch_port(switch_port);

	send_protobuf_message_simple_stream(switch_wfd.get_fd(), switch_req);

	/* Enqueue reply onto pending get_channel responses list */
	GetChannelResponse reply;
	c->pending_get_channel_responses.try_emplace({msg.client_id(), msg.tag()}, reply);
}


uint16_t NodeDaemon::get_free_coll_port()
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
		for (auto& c : clients)
		{
			for (auto& [t,ch_port] : c.channels)
			{
				if (ch_port == port)
				{
					taken = true;
					break;
				}
			}
		}

		if (!taken)
			return port;
	}

	throw runtime_error("No free local port for collectives");
}


void NodeDaemon::initialize_tdp()
{
	tdp_wfd.set_errno(socket(AF_INET, SOCK_DGRAM, 0), "socket(udp for tdp)");

	/* Bind socket */
	/* IMPROVE: The socket should only listen on the interface on the
	high performance network */
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(UDP_PORT_TDP)
	};

	addr.sin_addr.s_addr = INADDR_ANY;

	check_syscall(
		bind(tdp_wfd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)),
		"bind(udp for tdp)");

	/* Add to epoll instance */
	epoll.add_fd(tdp_wfd.get_fd(), EPOLLIN,
				 bind(&NodeDaemon::on_tdp_fd, this,
					 placeholders::_1, placeholders::_2));
}


void NodeDaemon::on_tdp_fd(int _fd, uint32_t events)
{
	char buf[128];

	auto ret = check_syscall(
		read(tdp_wfd.get_fd(), buf, sizeof(buf)),
		"read(tdp)");

	if (ret != sizeof(TDPHdr))
	{
		fprintf(stderr, "Received invalid TDP PDU\n");
		return;
	}

	const auto& tdp_hdr = *reinterpret_cast<const TDPHdr*>(buf);

	if (nearest_switch_ip == tdp_hdr.ctrl_ip)
		return;
	
	if (!nearest_switch_ip.is_0000())
		switch_changed();

	switch_port = ntohs(tdp_hdr.port);
	nearest_switch_ip = tdp_hdr.ctrl_ip;

	printf("New nearest switch: %s\n", to_string(nearest_switch_ip).c_str());

	connect_to_switch();

	/* TODO: forward port to TM */
}


void NodeDaemon::switch_changed()
{
	throw runtime_error("Nearest switch changed; this is not supported yet.");
}


void NodeDaemon::connect_to_switch()
{
	WrappedFD wfd;

	wfd.set_errno(socket(AF_INET, SOCK_STREAM, 0), "socket(control plane)");

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(TCP_PORT_CTRL_COLL)
	};
	memcpy(&addr.sin_addr.s_addr, &nearest_switch_ip, sizeof(nearest_switch_ip));

	check_syscall(
		connect(wfd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)),
		"connect(control plane)");

	/* Add socket to epoll instance */
	epoll.add_fd(wfd.get_fd(), EPOLLIN | EPOLLHUP | EPOLLRDHUP,
				 bind(&NodeDaemon::on_switch_fd, this,
					 placeholders::_1, placeholders::_2));

	switch_wfd = move(wfd);
}

void NodeDaemon::disconnect_from_switch()
{
	throw runtime_error("Disconnecting from the switch during runtime is not "
						"supported yet.");
}


void NodeDaemon::on_switch_fd(int fd, uint32_t events)
{
	bool disconnect = events & (EPOLLHUP | EPOLLRDHUP);
	
	if (events & EPOLLIN)
	{
		/* IMPROVE: Support different requests from the switch */
		auto switch_resp = recv_protobuf_message_simple_stream<
			proto::ctrl_sd::GetChannelResponse>(switch_wfd.get_fd());

		/* Find corresponding outstanding response */
		bool processed = false;
		for (auto& c : clients)
		{
			auto i = c.pending_get_channel_responses.find(
				{switch_resp->client_id(), switch_resp->tag()});

			if (i == c.pending_get_channel_responses.end())
				continue;

			/* Complete get_channel response and reply */
			auto& reply = i->second;

			reply.set_fabric_ip(switch_resp->fabric_ip());
			reply.set_fabric_qp(switch_resp->fabric_qp());

			send_protobuf_message_simple_dgram(c.get_fd(), reply);

			processed = true;
			break;
		}

		if (!processed)
		{
			fprintf(stderr, "received get_channel response from switch for "
					"non-existent client\n");
		}
	}

	if (disconnect)
		disconnect_from_switch();
}


void NodeDaemon::main()
{
	while (running)
		epoll.process_events(-1);
}
