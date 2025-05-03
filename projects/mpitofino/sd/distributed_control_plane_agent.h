#ifndef __DISTRIBUTED_CONTROL_PLANE_AGENT_H
#define __DISTRIBUTED_CONTROL_PLANE_AGENT_H

#include <list>
#include <map>
#include <set>
#include "common/utils.h"
#include "common/epoll.h"
#include "ctrl_sd.pb.h"
#include "state_repository.h"


namespace distributed_control_plane_agent {

	
struct Client final
{
	WrappedFD wfd;
	struct sockaddr_in addr;

	Client(WrappedFD&& wfd, const struct sockaddr_in& addr);
	Client(Client&&) = delete;

	std::set<uint64_t> channels;
};


class Agent final
{
protected:
	StateRepository& st_repo;
	Epoll& epoll;

	/* Client (i.e. node daemon) handling */
	WrappedFD client_listen_wfd;

	/* Use liste because it does not involve copying/moving of elements */
	std::list<Client> clients;

	void initialize_client_interface();

	void on_new_client(int, uint32_t);
	void on_client_fd(Client* client, int fd, uint32_t events);
	void on_client_get_channel(Client* client, const proto::ctrl_sd::GetChannel& msg);
	void on_client_unref_channel(Client* client, const proto::ctrl_sd::UnrefChannel& msg);

	std::map<
		uint64_t,
		std::map<Client*, proto::ctrl_sd::GetChannelResponse>>
	pending_get_channel_responses;


	void check_remove_channel(uint64_t tag);
	void check_remove_channels(const std::set<uint64_t>& channels);


public:
	Agent(StateRepository& st_repo, Epoll& epoll);
};


}

#endif /* __DISTRIBUTED_CONTROL_PLANE_AGENT_H */
