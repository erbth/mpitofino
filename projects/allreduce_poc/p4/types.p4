/* -*- P4_16 -*- */

struct my_ingress_headers_t {
	ethernet_h	ethernet;
	aggregate_h aggregate;
}

struct my_ingress_metadata_t {
	/* Same size as PortId_t */
	bit<9> mac_moved;
	PortId_t ingress_port;

	bit<32> is_complete_v;
	bit<32> port_bitmap;
	bit<1>  is_agg;
	bit<1>  agg_complete;
}


struct my_egress_headers_t {
}

struct my_egress_metadata_t {
}


/* For aggregation */
//struct value_t {
//	bit<32> lword;
//	bit<32> hword;
//}

typedef bit<32> value_t;

typedef bit<32> node_bitmap_t;
