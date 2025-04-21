#ifndef __COMMON_COM_UTILS_H
#define __COMMON_COM_UTILS_H

#include <cstdint>
#include <string>
#include <stdexcept>
#include <system_error>
#include <google/protobuf/message.h>
#include "common/utils.h"
#include "common/dynamic_buffer.h"

extern "C" {
#include <endian.h>
#include <unistd.h>
}


/* Work only for DGRAM/SEQPKT based sockets (because it requires fixed
datagram size semantics) */
void send_protobuf_message_simple_dgram(int fd, const google::protobuf::Message& msg)
{
	std::string s;
	if (!msg.SerializeToString(&s))
		throw std::invalid_argument("Failed to serialize message");

	auto ret = check_syscall(
		write(fd, s.c_str(), s.size()),
		"send_protobuf_message_simple_dgram::write");

	if (ret != (ssize_t) s.size())
	{
		throw std::runtime_error(
			"send_protobuf_message_simple_dgram::write wrote less bytes to "
			"dgram transport than requested");
	}
}

template<class T>
T recv_protobuf_message_simple_dgram(int fd)
{
	char buf[65536];

	auto ret = check_syscall(
		read(fd, buf, sizeof(buf)),
		"recv_protobuf_message_simple_dgram::read");

	if (ret == sizeof(buf))
		throw std::runtime_error("recv_protobuf_message_simple_dgram: message too large");

	T msg;
	if (!msg.ParseFromArray(buf, ret))
		throw std::runtime_error("Failed to receive message");

	return msg;
}


/* Feature message size encoding for stream transports */
void send_protobuf_message_simple_stream(int fd, const google::protobuf::Message& msg)
{
	std::string s;
	if (!msg.SerializeToString(&s))
		throw std::invalid_argument("Faled to serialize message");

	/* Write length */
	uint32_t length = htobe32(s.size());
	if (check_syscall(write(fd, &length, sizeof(length)), "write") != sizeof(length))
		throw std::runtime_error("failed to write message length");

	for (size_t pos = 0; pos < s.size();)
	{
		auto ret = check_syscall(
			write(fd, s.c_str() + pos, s.size() - pos),
			"write");

		pos += ret;
	}
}

template<class T>
T recv_protobuf_message_simple_stream(int fd)
{
	uint32_t length;
	
	/* Read message size */
	for (size_t pos = 0; pos < sizeof(length);)
	{
		auto ret = check_syscall(
			read(fd, (char*) &length + pos, sizeof(length) - pos),
			"read");

		pos += ret;
	}

	length = be32toh(length);

	/* Read message */
	dynamic_buffer buf;
	buf.ensure_size(length);

	for (size_t pos = 0; pos < length;)
	{
		auto ret = check_syscall(
			read(fd, buf.ptr() + pos, length - pos),
			"read");

		pos += ret;
	}

	T msg;
	if (!msg.ParseFromArray(buf.ptr(), length))
		throw std::runtime_error("Failed to receive message");

	return msg;
}

#endif /* __COMMON_COM_UTILS_H */
