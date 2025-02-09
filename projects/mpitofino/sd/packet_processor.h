/** A module to process packets redirect to the control plane, i.e. to the CPU
 * network interface. */

#ifndef __PACKET_PROCESSOR_H
#define __PACKET_PROCESSOR_H

#include <thread>


class PacketProcessor final
{
protected:
	void cleanup();

public:
	PacketProcessor();
	~PacketProcessor();
};

#endif /* __PACKET_PROCESSOR_H */
