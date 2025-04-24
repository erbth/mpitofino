#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "common/epoll.h"
#include "common/signalfd.h"
#include "state_repository.h"
#include "packet_processor.h"
#include "asic_driver.h"
#include "distributed_control_plane_agent.h"

using namespace std;


/* Architecture:
 *
 * The controller comprises several different components, which each specialize
 * on a certain task like interfacing with bf_switchd or handling client
 * requests. The interface between these components is a central state
 * repository with rudimentary publish-subscribe functionality. */


void main_exc()
{
	/* Initialize bfrt library */
	//init_asic_driver_bfrt();

	Epoll epoll;

	/* Register signal handler */
	bool running = true;

	SignalFD sfd({SIGINT, SIGTERM}, epoll,
				 [&running](int signo){ running = false; });


	/* Create different components */
	StateRepository state_repository;
	state_repository.read_config_file();

	PacketProcessor packet_processor(state_repository, epoll);
	ASICDriver asic_driver(state_repository);
	distributed_control_plane_agent::Agent dcp_agent(state_repository, epoll);


	/* Main loop */
	printf("mpitofino switch control plane daemon ready\n");
	while (running)
		epoll.process_events(-1);
}


int main(int argc, char** argv)
{
	try
	{
		main_exc();
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}
}
