#ifndef __COMMON_UTILS_H
#define __COMMON_UTILS_H

#include <system_error>
#include <string>

extern "C" {
#include <unistd.h>
}

/* The class `WrapepdFD' and the function `check_syscall' were copied from sdfs,
 * file src/common/utils.h, available from https://github.com/erbth/sdfs under
 * the MIT license. */
class WrappedFD final
{
protected:
	int fd;

public:
	inline WrappedFD()
		: fd(-1)
	{
	}

	inline WrappedFD(int fd)
		: fd(fd)
	{
	}

	inline ~WrappedFD()
	{
		if (fd >= 0)
			::close(fd);
	}

	inline WrappedFD(WrappedFD&& o)
		: fd(o.fd)
	{
		o.fd = -1;
	}

	inline WrappedFD& operator=(WrappedFD&& o) noexcept
	{
		if (fd >= 0)
			::close(fd);

		fd = o.fd;
		o.fd = -1;

		return *this;
	}

	inline int operator=(int nfd) noexcept
	{
		if (fd >= 0)
			::close(fd);

		fd = nfd;

		return fd;
	}

	/* If value is < 0, throw system_error
	 * It is guaranteed that this will not throw if new_fd >= 0. */
	inline void set_errno(int new_fd, const char* msg)
	{
		if (new_fd < 0)
			throw std::system_error(errno, std::generic_category(), msg);

		if (fd >= 0)
			::close(fd);

		fd = new_fd;
	}

	inline int get_fd()
	{
		return fd;
	}

	inline operator bool() const
	{
		return fd >= 0;
	}

	inline void close()
	{
		if (fd >= 0)
		{
			::close(fd);
			fd = -1;
		}
	}
};

template <typename T>
inline T check_syscall(T ret, const char* msg)
{
	if (ret < 0)
		throw std::system_error(errno, std::generic_category(), msg);

	return ret;
}
/* End copied from sdfs */

std::string to_hex_string(int i);

#endif /* __COMMON_UTILS_H */
