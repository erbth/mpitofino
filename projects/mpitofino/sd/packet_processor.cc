#include "packet_processor.h"

PacketProcessor::PacketProcessor()
{
	try
	{
		/* Create raw socket */

		/* Find interface */

		/* Bind socket to interface */
	}
	catch (...)
	{
		cleanup();
		throw;
	}
}

PacketProcessor::~PacketProcessor()
{
	cleanup();
}

void PacketProcessor::cleanup()
{
}
