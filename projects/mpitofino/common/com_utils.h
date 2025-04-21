#ifndef __COMMON_COM_UTILS_H
#define __COMMON_COM_UTILS_H

#include <string>
#include <stdexcept>
#include <system_error>
#include <google/protobuf/message.h>
#include "common/utils.h"
#include "common/dynamic_buffer.h"

extern "C" {
#include <unistd.h>
}


void send_protobuf_message_simple(int fd, const google::protobuf::Message& msg)
{
	std::string s;
	if (!msg.SerializeToString(&s))
		throw std::invalid_argument("Failed to serialize message");

	for (size_t pos = 0; pos < s.size(); pos++)
	{
		auto ret = check_syscall(
			write(fd, s.c_str() + pos, s.size() - pos),
			"send_protobuf_message_simple::write");

		pos += ret;
	}
}


/* Works only for DGRAM/SEQPKT based sockets (because it requires
fixed datagram size semantics) */
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

/* Features message size detection */
template<class T>
T recv_protobuf_message_simple_stream(int fd)
{
	dynamic_buffer buf;
	buf.ensure_size(128);

	auto ret = check_syscall(
		read(fd, buf.ptr(), buf.size()),
		"recv_protobuf_message_simple_dgram::read");

	if (ret == sizeof(buf))
		throw std::runtime_error("recv_protobuf_message_simple_dgram: message too large");

	T msg;
	if (!msg.ParseFromArray(buf, ret))
		throw std::runtime_error("Failed to receive message");

	return msg;
}

#endif /* __COMMON_COM_UTILS_H */
