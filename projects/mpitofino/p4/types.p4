/* -*- P4_16 -*- */

struct my_ingress_headers_t {
	ethernet_h	ethernet;
}

struct my_ingress_metadata_t {
	/* Same size as PortId_t */
	bit<9> mac_moved;
	PortId_t ingress_port;
}


struct my_egress_headers_t {
}

struct my_egress_metadata_t {
}
