#include <stdexcept>
#include "common/packet_headers.h"
#include "common/simple_types.h"
#include "common/com_utils.h"
#include "distributed_control_plane_agent.h"

extern "C" {
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
}

using namespace std;


namespace distributed_control_plane_agent {

	
Client::Client(WrappedFD&& wfd, const struct sockaddr_in& addr)
	: wfd(move(wfd)), addr(addr)
{
}


Agent::Agent(
		StateRepository& st_repo, Epoll& epoll)
	:
		st_repo(st_repo), epoll(epoll)
{
	initialize_client_interface();
}


void Agent::initialize_client_interface()
{
	WrappedFD wfd;
	wfd.set_errno(socket(AF_INET, SOCK_STREAM, 0), "socket(tcp)");

	/* Set SO_REUSEADDR */
	int reuseaddr = 1;
	check_syscall(
		setsockopt(wfd.get_fd(), SOL_SOCKET, SO_REUSEADDR,
				   &reuseaddr, sizeof(reuseaddr)),
		"setsockopt");

	/* Bind and listen */
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(TCP_PORT_CTRL_COLL)
	};
	addr.sin_addr.s_addr = INADDR_ANY;

	check_syscall(
		bind(wfd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)),
		"bind(tcp)");

	check_syscall(listen(wfd.get_fd(), 10), "listen(tcp)");

	/* Add to epoll instance */
	epoll.add_fd(wfd.get_fd(), EPOLLIN,
				 bind(&Agent::on_new_client, this,
					 placeholders::_1, placeholders::_2));

	client_listen_wfd = move(wfd);
}


void Agent::on_new_client(int, uint32_t)
{
	WrappedFD wfd;
	struct sockaddr_in addr{};
	socklen_t addrlen = sizeof(addr);
	
	wfd.set_errno(
		accept4(client_listen_wfd.get_fd(), (struct sockaddr*) &addr, &addrlen, 0),
		"accept4(client)");

	printf("New client connected from %s\n", to_string(addr).c_str());
	clients.emplace_back(move(wfd), addr);

	/* Add fd to epoll instance */
	try
	{
		epoll.add_fd(clients.back().wfd.get_fd(),
					 EPOLLIN | EPOLLHUP | EPOLLRDHUP,
					 bind(&Agent::on_client_fd, this, &clients.back(),
						 placeholders::_1, placeholders::_2));
	}
	catch (...)
	{
		clients.pop_back();
		throw;
	}
}


void Agent::on_client_fd(Client* client, int fd, uint32_t events)
{
	bool disconnect = events & (EPOLLHUP | EPOLLRDHUP);


	if (events & EPOLLIN)
	{
		/* Receive message */
		/* IMPROVE: This blocks if not all data of the message has
		been received yet. Maybe use a receive buffer if this becomes
		a bottleneck. */
		auto msg = recv_protobuf_message_simple_stream<proto::ctrl_sd::NdRequest>(fd);
		if (msg)
		{
			switch (msg->messages_case())
			{
			case proto::ctrl_sd::NdRequest::kGetChannel:
				on_client_get_channel(client, msg->get_channel());
				break;

			default:
				throw runtime_error("Unsupported message from client");
			}
		}
		else
		{
			disconnect = true;
		}
	}


	if (disconnect)
	{
		auto i = find_if(clients.begin(), clients.end(),
						 [=](auto& c){ return &c == client; });

		if (i != clients.end())
		{
			printf("Disconnecting client from %s\n",
				   to_string(client->addr).c_str());

			epoll.remove_fd(client->wfd.get_fd());
			clients.erase(i);
		}
	}
}


void Agent::on_client_get_channel(Client* client, const proto::ctrl_sd::GetChannel& msg)
{
	/* Check if the channel exists already */
	auto ch = st_repo.get_channel(msg.tag());

	/* If not, allocate it, otherwise check for matching parameters */
	if (!ch)
	{
		CollectiveChannel c;

		c.tag = msg.tag();
		c.fabric_ip = st_repo.get_collectives_module_ip_addr();
		c.fabric_port = st_repo.get_free_coll_port();
		c.fabric_mac = st_repo.get_collectives_module_mac_addr();

		c.agg_unit = 0;

		for (auto cid : msg.agg_group_client_ids())
		{
			CollectiveChannel::Participant p;
			p.client_id = cid;
			c.participants.insert({cid, p});
		}

		c.type = msg.type();

		st_repo.add_channel(c);
		ch = st_repo.get_channel(c.tag);
	}

	/* Update client parameters */
	auto client_ip = msg.client_ip();
	auto client_mac = msg.client_mac();

	st_repo.update_channel_participant(
		msg.tag(), msg.client_id(),
		*reinterpret_cast<IPv4Addr*>(&client_ip), msg.client_port(),
		*reinterpret_cast<MacAddr*>(&client_mac), msg.switch_port());

	/* Return channel parameters. Note that the ASIC has already been
	updated synchronously during the st_repo calls above. */
	proto::ctrl_sd::GetChannelResponse resp;
	resp.set_client_id(msg.client_id());
	resp.set_tag(ch->tag);
	resp.set_fabric_ip(*reinterpret_cast<const uint32_t*>(&ch->fabric_ip));
	resp.set_fabric_port(ch->fabric_port);

	send_protobuf_message_simple_stream(client->wfd.get_fd(), resp);
}


}
