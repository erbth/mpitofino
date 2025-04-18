#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <google/protobuf/stubs/common.h>
#include "config.h"
#include "node_daemon.h"

using namespace std;


void main_exc()
{
	NodeDaemon nd;

	printf("mpitofino node daemon version %d.%d.%d ready.\n",
		   MPITOFINO_VERSION_MAJOR, MPITOFINO_VERSION_MINOR, MPITOFINO_VERSION_PATCH);

	nd.main();
}

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

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
