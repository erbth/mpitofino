/* -*- P4_16 -*- */

// Other definitions
// Broadcast groups (match definitions in ASIC driver of the control plane)
#define MCAST_FLOOD 1
#define MCAST_BCAST 2


struct node_bitmap_t {
	bit<32> low;
	bit<32> high;
}


struct my_ingress_headers_t {
	recirc_fanout_h recirc_fanout;
	ethernet_h	ethernet;
	ipv4_h ipv4;
	udp_h udp;
	roce_h roce;
	aggregate_h aggregate;
	recirc_fanout_payload_h recirc_fanout_payload;
	roce_checksum_h roce_checksum;
}

struct my_ingress_metadata_t {
	cpoffload_h cpoffload;

	/* S.t. the ingress port will be available in the deparser. */
	PortId_t ingress_port;
	bit<1> is_coll;  // process packet with collectives unit

	bit<16> agg_unit;
	node_bitmap_t node_bitmap;
	bool agg_have_unit;
	bool agg_is_clear;  // set on final recirculation before the result is sent

	bridge_header_t bridge_header;
}


struct my_egress_headers_t {
	recirc_fanout_h recirc_fanout;
	ethernet_h	ethernet;
	ipv4_h ipv4;
	udp_h udp;
	roce_h roce;
	roce_ack_h roce_ack;
}

struct my_egress_metadata_t {
	bridge_header_t bridge_header;
}


/* For aggregation */
typedef bit<32> value_t;
struct value_pair_t {
	value_t low;
	value_t high;
}
