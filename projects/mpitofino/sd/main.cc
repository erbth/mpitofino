#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace std;


/* Architecture:
 *
 * The controller comprises several different components, which each specialize
 * on a certain task like interfacing with bf_switchd or handling client
 * requests. The interface between these components is a central state
 * repository with rudimentary publish-subscribe functionality. */


void main_exc()
{
	/* Register signal handler */

	/* Create different components */

	/* Main loop */
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
