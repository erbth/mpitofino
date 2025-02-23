/** A module to process packets redirect to the control plane, i.e. to the CPU
 * network interface. */

#ifndef __PACKET_PROCESSOR_H
#define __PACKET_PROCESSOR_H

#include "common/utils.h"
#include "common/epoll.h"
#include "state_repository.h"


class PacketProcessor final
{
protected:
	Epoll& epoll;
	StateRepository& st_repo;

	WrappedFD com_fd;
	int int_idx = 0;

	void cleanup();

	void on_com_fd_data(int, uint32_t events);

public:
	PacketProcessor(StateRepository& st_repo, Epoll& epoll);
	~PacketProcessor();
};

#endif /* __PACKET_PROCESSOR_H */
