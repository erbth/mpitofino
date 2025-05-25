#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <set>
#include <regex>
#include "common/com_utils.h"
#include "mpitofino.h"
#include "client_lib_nd.pb.h"
#include "ib_utils.h"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/un.h>
}

using namespace std;
namespace fs = std::filesystem;


namespace mpitofino
{

/* Global stateless utility functions */
size_t get_datatype_element_size(datatype_t dtype)
{
	switch (dtype)
	{
	case datatype_t::INT32:
		return sizeof(int32_t);

	default:
		throw runtime_error("Invalid datatype");
	};
}


inline void convert_endianess_32(const uint32_t* src, uint32_t* dst, size_t count)
{
	for (size_t i = 0; i < count; i ++)
	{
		auto v = src[i];
		dst[i] =
			((v & 0xff) << 24) |
			((v & 0xff00) << 8) |
			((v & 0xff0000) >> 8) |
			((v & 0xff000000) >> 24);
	}
}

/* Tofino's ALUs use big endian (network byte order) */
inline void convert_endianess(const void* src, void* dst, size_t size, datatype_t dtype)
{
	switch (dtype)
	{
	case datatype_t::INT32:
		convert_endianess_32((const uint32_t*) src, (uint32_t*) dst, size / 4);
		break;

	default:
		throw runtime_error("Unsupported datatype");
	};
}


/* Client */
Client::Client(uint64_t client_id)
	: client_id(client_id)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	/* Open RoCE device */
	open_ib_device();

	/* Determine path of socket of node daemon, and connect */
	vector<fs::path> socket_cand;
	socket_cand.push_back("/run/mpitofino-sd");

	auto val = getenv("XDG_RUNTIME_DIR");
	if (val && strlen(val) > 0 && val[0] == '/')
		socket_cand.push_back(string(val) + "/mpitofino-sd");

	for (const auto& s : socket_cand)
	{
		if (fs::exists(fs::symlink_status(s)))
		{
			nd_socket_path = s;
			break;
		}
	}

	if (nd_socket_path.empty())
		throw runtime_error("Unable to find path of node daemon's unix domain socket");

	WrappedFD fd;
	fd.set_errno(
		socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0),
		"socket(AF_UNIX)");

	/* Try to connect */
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX
	};

	strncpy(addr.sun_path, nd_socket_path.c_str(), sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

	check_syscall(
		connect(fd.get_fd(), (struct sockaddr*) &addr, sizeof(addr)),
		"connect(nd unix domain socket)");

	nd_wfd = move(fd);
}

Client::~Client()
{
}


void Client::open_ib_device()
{
	ib_dev_list = ibv_get_device_list(nullptr);
	if (!ib_dev_list)
		throw runtime_error("Failed ot get IB devices");

	/* Choose device */
	if (*ib_dev_list.get())
		ib_dev = *ib_dev_list.get();

	printf("Available IB devices:\n");
	for (auto ptr = ib_dev_list.get(); *ptr; ptr++)
	{
		printf("  %d: %s (guid: %s)%s\n",
			   ibv_get_device_index(*ptr),
			   ibv_get_device_name(*ptr),
			   format_ib_guid(ibv_get_device_guid(*ptr)).c_str(),
			   ib_dev == *ptr ? " <-- chosen" : "");
	}

	if (!ib_dev)
		throw runtime_error("No suitable IB device found\n");

	printf("\n");

	/* Open device */
	ib_ctx = ibv_open_device(ib_dev);

	/* Choose gid */
	choose_gid();
	printf("  chosen gid: %u\n\n", ib_gid);
}

void Client::choose_gid()
{
	auto base = fs::path("/sys/class/infiniband") / ibv_get_device_name(ib_dev) / "ports/1";
	
	for (unsigned i = 0; i < 256; i++)
	{
		auto p = base / "gids" / to_string(i);
		if (fs::exists(p))
		{
			ifstream f(p);
			string s;
			f >> s;

			/* Select IPv4 addresses */
			if (!regex_match(s, regex("^0000:0000:0000:0000:0000:ffff:[0-9a-fA-F]{4}:[0-9a-fA-F]{4}$")))
				continue;

			/* Check if this is RoCEv2 */
			ifstream f2(base / "gid_attrs/types" / to_string(i));
			string s2, s3;
			f2 >> s2 >> s3;

			if (s2 != "RoCE" || s3 != "v2")
				continue;

			ib_gid = i;
			return;
		}
	}

	throw runtime_error("no suitable IB gid found");
}


CollectiveChannel::CollectiveChannel(
		Client& client, AggregationGroup& agg_group)
	:
		client(client), agg_group(agg_group)
{
	setup_ib_pd();
	setup_ib_qp();
}


void CollectiveChannel::setup_ib_pd()
{
	ib_pd = ibv_alloc_pd(client.ib_ctx.get());
	if (!ib_pd)
		throw runtime_error("Failed to allocate IB PD");

	ib_mr = ibv_reg_mr(ib_pd.get(), ib_buf.ptr(), ib_buf.size(),
					   IBV_ACCESS_LOCAL_WRITE);

	if (!ib_mr)
		throw runtime_error("Failed to register buffer at IB PD");
}


void CollectiveChannel::setup_ib_qp()
{
	/* Create CQ */
	ib_cq = ibv_create_cq(client.ib_ctx.get(), 2, nullptr, nullptr, 0);
	if (!ib_cq)
		throw runtime_error("Failed to create IB CQ");

	/* Create QP */
	ibv_qp_init_attr qp_init_attr = {
		.send_cq = ib_cq.get(),
		.recv_cq = ib_cq.get(),
		.cap = {
			.max_send_wr = (unsigned) req_in_flight,
			.max_recv_wr = (unsigned) req_in_flight,
			.max_send_sge = 1,
			.max_recv_sge = 1
		},
		.qp_type = IBV_QPT_RC
	};

	ib_qp = ibv_create_qp(ib_pd.get(), &qp_init_attr);
	if (!ib_qp)
		throw runtime_error("Failed to create IB QP");

	/* QP -> Init */
	ibv_qp_attr qp_attr = {
		.qp_state = IBV_QPS_INIT,
		.qp_access_flags = 0,
		.pkey_index = 0,
		.port_num = 1
	};

	auto ret = ibv_modify_qp(
		ib_qp.get(),
		&qp_attr,
		IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
		IBV_QP_ACCESS_FLAGS);

	if (ret)
		throw runtime_error("Failed to set IB QP to state Init: " + to_string(ret));
}


void CollectiveChannel::finalize_qp()
{
	/* QP -> RTR */
	ibv_qp_attr qp_attr{};
	
	qp_attr.qp_state = IBV_QPS_RTR;

	qp_attr.ah_attr.is_global = 1;
	qp_attr.ah_attr.sl = 0;
	qp_attr.ah_attr.src_path_bits = 0;
	qp_attr.ah_attr.port_num = 1;
	qp_attr.ah_attr.grh.hop_limit = 1;
	qp_attr.ah_attr.grh.sgid_index = client.ib_gid;

	/* Construct a mapped IPv4-address */
	auto gid_ptr = (uint32_t*) &qp_attr.ah_attr.grh.dgid;
	gid_ptr[3] = fabric_ip;
	gid_ptr[2] = htobe32(0xffffU);

	qp_attr.path_mtu = IBV_MTU_256;
	qp_attr.dest_qp_num = fabric_qp;
	qp_attr.rq_psn = 0;
	qp_attr.max_dest_rd_atomic = 1;
	qp_attr.min_rnr_timer = 0;

	auto ret = ibv_modify_qp(
		ib_qp.get(),
		&qp_attr,
		IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
		IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
		IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);

	if (ret)
		throw runtime_error("Failed to set IB QP to state RTR: " + to_string(ret));

	/* QP -> RTS */
	qp_attr.qp_state = IBV_QPS_RTS;

	qp_attr.sq_psn = 0;
	qp_attr.max_rd_atomic = 1;
	qp_attr.retry_cnt = 0;
	qp_attr.rnr_retry = 0;
	qp_attr.timeout = 0;

	ret = ibv_modify_qp(
		ib_qp.get(),
		&qp_attr,
		IBV_QP_STATE | IBV_QP_SQ_PSN |
		IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT);

	if (ret)
		throw runtime_error("Failed to set IB QP to state RTS: " + to_string(ret));
}


/* Aggregation group */
AggregationGroup::AggregationGroup(
		Client& client, const vector<uint64_t>& client_ids)
	: client(client), client_ids(client_ids)
{
}

AggregationGroup::~AggregationGroup()
{
}


CollectiveChannel* AggregationGroup::get_channel(tag_t tag)
{
	/* Local cache */
	{
		auto i = channels.find(tag);
		if (i != channels.end())
			return &(i->second);
	}

	/* Create collective channel s.t. qp id is populated */
	auto ch = &channels.try_emplace(tag, client, *this).first->second;

	try
	{
		/* Request a collective channel from the node daemon */
		ClientRequest req;
		auto msg = req.mutable_get_channel();

		for (auto id : client_ids)
			msg->add_agg_group_client_ids(id);

		msg->set_client_id(client.client_id);

		msg->set_type(ChannelType::ALLREDUCE_INT32);
		msg->set_tag(tag);
		msg->set_local_qp(ch->ib_qp->qp_num);
		send_protobuf_message_simple_dgram(client.get_nd_fd(), req);


		/* Parse response */
		auto reply = recv_protobuf_message_simple_dgram<GetChannelResponse>(client.get_nd_fd());

		ch->fabric_ip = reply.fabric_ip();
		ch->fabric_qp = reply.fabric_qp();

		ch->finalize_qp();

		return ch;
	}
	catch (...)
	{
		channels.erase(channels.find(tag));
		throw;
	}
}


struct ring_elem_t
{
	char status = 0;

	char* recv_buf{};
	char* send_buf{};
	size_t offset{};
	size_t size{};
};
	
void AggregationGroup::allreduce(
		const void* sbuf, void* dbuf, size_t count,
		datatype_t dtype, tag_t tag)
{
	/* Get or create collective channel */
	auto ch = get_channel(tag);

	/* Exchange data */
	/* Setup ring buffer */
	size_t size = count * get_datatype_element_size(dtype);

	memset(ch->ib_buf.ptr(), 0, ch->ib_buf.size());
	auto send_buf = ch->ib_buf.ptr();
	auto recv_buf = ch->ib_buf.ptr() + ch->chunk_size * ch->req_in_flight;

	const size_t ring_size = ch->req_in_flight;
	ring_elem_t ring[ring_size]{};
	for (size_t i = 0; i < ring_size; i++)
	{
		ring[i].recv_buf = recv_buf + ch->chunk_size*i;
		ring[i].send_buf = send_buf + ch->chunk_size*i;
	}


	size_t send_pos = 0;
	size_t outstanding_reqs = 0;
	size_t ring_pos = 0;

	while (send_pos < size || outstanding_reqs > 0)
	{
		/* Fill ring with new WRs */
		while (send_pos < size)
		{
			size_t next = (ring_pos + 1) % ring_size;
			if (ring[next].status)
				break;

			ring_pos = next;
			ring[ring_pos].status = 2;
			outstanding_reqs += 2;

			auto to_send = min(ch->chunk_size, size - send_pos);
			ring[ring_pos].offset = send_pos;
			ring[ring_pos].size = to_send;

			/* Endianess conversion for send */
			convert_endianess(
				(const uint8_t*) sbuf + send_pos,
				ring[ring_pos].send_buf,
				to_send,
				dtype);

			send_pos += to_send;

			/* Post receive */
			ibv_sge recv_sge = {
				.addr = (uintptr_t) ring[ring_pos].recv_buf,
				.length = (uint32_t) ch->chunk_size,
				.lkey = ch->ib_mr->lkey
			};

			ibv_recv_wr* recv_bad_wr;
			ibv_recv_wr recv_wr = {
				.wr_id = 0x1000 + ring_pos,
				.sg_list = &recv_sge,
				.num_sge = 1
			};

			auto ret = ibv_post_recv(ch->ib_qp.get(), &recv_wr, &recv_bad_wr);
			if (ret)
				throw runtime_error("Failed to post IB receive WR: " + to_string(ret));

			/* Post send */
			ibv_sge send_sge = {
				.addr = (uintptr_t) ring[ring_pos].send_buf,
				.length = (uint32_t) ch->chunk_size,
				.lkey = ch->ib_mr->lkey
			};

			ibv_send_wr* send_bad_wr = nullptr;
			ibv_send_wr send_wr = {
				.wr_id = 0x2000 + ring_pos,
				.sg_list = &send_sge,
				.num_sge = 1,
				.opcode = IBV_WR_SEND,
				.send_flags = IBV_SEND_SIGNALED
			};

			ret = ibv_post_send(ch->ib_qp.get(), &send_wr, &send_bad_wr);
			if (ret)
				throw runtime_error("Failed to post IB send WR: " + to_string(ret));
		}


		/* Process completed WRs */
		ibv_wc wcs[ring_size*2]{};
		auto ret = ibv_poll_cq(ch->ib_cq.get(), sizeof(wcs) / sizeof(wcs[0]), wcs);
		if (ret < 0)
			throw runtime_error("Failed to poll IB CQ");

		for (int i = 0; i < ret; i++)
		{
			auto& wc = wcs[i];

			if (wc.wr_id >= 0x2000)
			{
				auto pos = wc.wr_id - 0x2000;

				if (wc.status != IBV_WC_SUCCESS)
					throw runtime_error("IB send failed: " + to_string(ret));

				ring[pos].status--;
				outstanding_reqs--;
			}
			else
			{
				auto pos = wc.wr_id - 0x1000;

				if (wc.status != IBV_WC_SUCCESS ||
					(wc.byte_len != ch->chunk_size))
				{
					throw runtime_error("IB recv failed: " + to_string(ret));
				}

				/* Endianess conversion of received data */
				convert_endianess(
					ring[pos].recv_buf,
					(uint8_t*) dbuf + ring[pos].offset,
					ring[pos].size,
					dtype);

				/* Mark ring buffer entry as free */
				ring[pos].status--;
				outstanding_reqs--;
			}
		}
	}
}


}
