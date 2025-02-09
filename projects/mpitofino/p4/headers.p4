/* -*- P4_16 -*- */

/* Types to be used in headers */
typedef bit<48> mac_addr_t;

enum bit<16> ether_type_t {
	IPV4 = 0x0800,
	ARP  = 0x0806
}

header ethernet_h {
	mac_addr_t		dst_addr;
	mac_addr_t		src_addr;
	ether_type_t	ether_type;
}
