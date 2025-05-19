control CollectivesDistributor(
	inout my_egress_headers_t hdr,
	inout my_egress_metadata_t meta,
	in egress_intrinsic_metadata_t eg_intr_md)
{
	/* NOTE: It might be better to factor out the dst_mac resoluation into an
	 * arp table that can then be used by multiple L3 services implemented in
	 * the tofino. */
	action output_address_set(
		mac_addr_t src_mac, mac_addr_t dst_mac,
		ipv4_addr_t src_ip, ipv4_addr_t dst_ip,
		bit<24> dst_qp)
	{
		hdr.ethernet.src_addr = src_mac;
		hdr.ethernet.dst_addr = dst_mac;

		hdr.ipv4.src_addr = src_ip;
		hdr.ipv4.dst_addr = dst_ip;

		hdr.udp.src_port = 4791;
		hdr.udp.dst_port = 4791;

		// Disable checksum
		hdr.udp.checksum = 0;

		hdr.roce.dst_qp = dst_qp;
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
	}

	apply {
		/* Fill source and destination addresses on result packet output */
		output_address.apply();

		/* Calculate checksum */
	}
}
