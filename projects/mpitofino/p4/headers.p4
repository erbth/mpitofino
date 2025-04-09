/* -*- P4_16 -*- */

/* Types to be used in headers */
typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;

enum bit<16> ether_type_t {
	IPV4 = 0x0800
	//ARP  = 0x0806
}

enum bit<8> ipv4_protocol_t {
	ICMP = 0x01,
	UDP  = 0x11
}

header ethernet_h {
	mac_addr_t		dst_addr;
	mac_addr_t		src_addr;
	ether_type_t	ether_type;
}

//header arp_h {
//	bit<16>     hw_type;            // 0x0001 (ethernet)
//	bit<16>     proto_type;         // 0x0800 (ipv4)
//	bit<8>      hw_addr_length;     // 6
//	bit<8>      proto_addr_lenght;  // 4
//	bit<16>     op;                 // 1: request, 2: reply
//	mac_addr_t  sender_hw_addr;
//	ipv4_addr_t sender_proto_addr;
//	mac_addr_t  target_hw_addr;
//	ipv4_addr_t target_proto_addr;
//}

header ipv4_h {
	bit<4>  version;
	bit<4>  ihl;
	bit<8>  tos;
	bit<16> total_length;
	bit<16> identification;
	bit<3>  flags;
	bit<13> fragment_offset;
	bit<8>  ttl;
	ipv4_protocol_t  protocol;
	bit<16> header_checksum;
	ipv4_addr_t src_addr;
	ipv4_addr_t dst_addr;
}

header udp_h {
	bit<16> src_port;
	bit<16> dst_port;
	bit<16> length;
	bit<16> checksum;
}

header aggregate_h {
	bit<32> val00;
	bit<32> val01;
	bit<32> val02;
	bit<32> val03;
	bit<32> val04;
	bit<32> val05;
	bit<32> val06;
	bit<32> val07;
	bit<32> val08;
	bit<32> val09;
	bit<32> val10;
	bit<32> val11;
	bit<32> val12;
	bit<32> val13;
	bit<32> val14;
	bit<32> val15;
	bit<32> val16;
	bit<32> val17;
	bit<32> val18;
	bit<32> val19;
	bit<32> val20;
	bit<32> val21;
	bit<32> val22;
	bit<32> val23;
	bit<32> val24;
	bit<32> val25;
	bit<32> val26;
	bit<32> val27;
	bit<32> val28;
	bit<32> val29;
	bit<32> val30;
	bit<32> val31;
	bit<32> val32;
	bit<32> val33;
	bit<32> val34;
	bit<32> val35;
	bit<32> val36;
	bit<32> val37;
	bit<32> val38;
	bit<32> val39;
	bit<32> val40;
	bit<32> val41;
	bit<32> val42;
	bit<32> val43;
	bit<32> val44;
	bit<32> val45;
	bit<32> val46;
	bit<32> val47;
	bit<32> val48;
	bit<32> val49;
	bit<32> val50;
	bit<32> val51;
	bit<32> val52;
	bit<32> val53;
	bit<32> val54;
	bit<32> val55;
	bit<32> val56;
	bit<32> val57;
	bit<32> val58;
	bit<32> val59;
	bit<32> val60;
	bit<32> val61;
	bit<32> val62;
	bit<32> val63;
}

/* A header to take metadata from the ingress to the egress pipe. */
header bridge_header_t {
	bit<16> agg_unit;
}
