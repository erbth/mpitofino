#ifndef __COMMON_UTILS_H
#define __COMMON_UTILS_H

#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <string>
#include <functional>

extern "C" {
#include <unistd.h>
}

#define FINALLY(X,F) try { X; F; } catch (...) { F; throw; }

/* The class `WrapepdFD' and the function `check_syscall' were copied and
 * partially changed from sdfs, file src/common/utils.h, available from
 * https://github.com/erbth/sdfs under the MIT license. */
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

	inline void set_errno(int new_fd)
	{
		set_errno(new_fd, "");
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

template <typename T>
class WrappedObject final
{
public:
	using destructor_t = std::function<void(T*)>;
protected:
	T* ptr{};
	destructor_t destructor;

	inline void destruct()
	{
		if (ptr)
		{
			destructor(ptr);
			ptr = nullptr;
		}
	}

public:
	inline WrappedObject(destructor_t destructor)
		: destructor(destructor)
	{
	};

	inline WrappedObject& operator=(WrappedObject&& o)
	{
		destruct();

		destructor = o.destructor;
		ptr = o.ptr;

		o.ptr = nullptr;

		return *this;
	}

	inline WrappedObject& operator=(T* ptr)
	{
		destruct();

		this->ptr = ptr;

		return *this;
	}

	inline T* operator->()
	{
		if (!ptr)
			throw std::runtime_error("bad object");

		return ptr;
	}
	
	inline ~WrappedObject()
	{
		destruct();
	}

	inline operator bool()
	{
		return ptr != nullptr;
	}

	T* get()
	{
		return ptr;
	}
};

std::string to_hex_string(int i);

/* E.g. for IPv4 and ICMP; RFC1071 */
uint16_t internet_checksum(const char* data, size_t size);

#endif /* __COMMON_UTILS_H */
