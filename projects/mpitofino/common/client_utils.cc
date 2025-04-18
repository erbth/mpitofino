#include "common/client_utils.h"

using namespace std;
namespace fs = std::filesystem;


fs::path determine_nd_socket()
{
	/* Try $XDG_RUNTIME_DIR/mpitofino-nd (if nd is run without root
	privileges) */
}
