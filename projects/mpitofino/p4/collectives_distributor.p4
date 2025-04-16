control CollectivesDistributor(
	inout my_egress_headers_t hdr,
	inout my_egress_metadata_t meta,
	in egress_intrinsic_metadata_t eg_intr_md)
{
	/* NOTE: It might be better to factor out the dst_mac resoluation into an
	 * arp table that can than be used by multiple L3 services implemented in
	 * the tofino. */
	action output_address_set(
		mac_addr_t src_mac, mac_addr_t dst_mac,
		ipv4_addr_t src_ip, ipv4_addr_t dst_ip,
		bit<16> src_port, bit<16> dst_port)
	{
		hdr.ethernet.src_addr = src_mac;
		hdr.ethernet.dst_addr = dst_mac;

		hdr.ipv4.src_addr = src_ip;
		hdr.ipv4.dst_addr = dst_ip;

		hdr.udp.src_port = src_port;
		hdr.udp.dst_port = dst_port;

		// Disable checksum
		hdr.udp.checksum = 0;
	}

	table output_address {
		key = {
			meta.bridge_header.agg_unit : exact;
			eg_intr_md.egress_rid : exact;
		}

		actions = {
			output_address_set;
			NoAction;
		}
		default_action = NoAction;

		size = 1024;

		const entries = {
			(1, 1) : output_address_set(0x028000000400, 0x525500000015, 0x0a0a8000, 0x0a0a0001, 0x4000, 0x2000);
			(1, 2) : output_address_set(0x028000000400, 0x525500000014, 0x0a0a8000, 0x0a0a0002, 0x4000, 0x2000);
		}
	}

	apply {
		/* Fill source and destination addresses on result packet output */
		output_address.apply();
	}
}
