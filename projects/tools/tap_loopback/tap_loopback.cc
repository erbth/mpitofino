/** Create additional loopback devices on a linux system using tap
interfaces. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <system_error>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/if.h>
#include <linux/if_tun.h>
}

using namespace std;


void run_subprocess(vector<string> args)
{
	char* c_args[args.size()];
	for (size_t i = 0; i < args.size(); i++)
		c_args[i] = (char*) args[i].c_str();

	auto child = fork();
	if (child < 0)
		throw system_error(errno, generic_category(), "fork");
	
	if (child == 0)
	{
		execvp(c_args[0], c_args);
		perror("execvp");
		exit(EXIT_FAILURE);
	}

	int wstatus = 0;
	if (waitpid(child, &wstatus, 0) != child)
		throw system_error(errno, generic_category(), "Failed to wait for child");

	if (!(WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0))
		throw runtime_error("Failed to run child process");
}


void main_exc(const string& ifname)
{
	if (ifname.size() + 1 > IFNAMSIZ)
		throw invalid_argument("Interface name too long");

	/* See linux/Documentation/networking/tuntap.rst */
	struct ifreq ifr{};

	auto fd = open("/dev/net/tun", O_RDWR | O_CLOEXEC);
	if (fd < 0)
		throw system_error(errno, generic_category(), "open(/dev/net/tun)");

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';

	if (ioctl(fd, TUNSETIFF, &ifr) < 0)
		throw system_error(errno, generic_category(), "ioctl(TUNSETIFF)");


	/* Enable interface and enable promiscous mode */
	run_subprocess({"ip", "link", "set", ifname, "up", "promisc", "on"});


	/* Main loop */
	char buf[65536];
	for (;;)
	{
		auto ret = read(fd, buf, sizeof(buf));
		if (ret < 0)
			throw system_error(errno, generic_category(), "read(tap)");

		if (ret == 0)
			break;

		auto ret_wr = write(fd, buf, ret);
		if (ret_wr != ret)
			throw system_error(errno, generic_category(), "write(tap)");
	}
}


int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <interface name>\n",
				argc > 0 ? argv[0] : "<?>");

		return EXIT_FAILURE;
	}

	try
	{
		main_exc(argv[1]);
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}
}
