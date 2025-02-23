/* -*- P4_16 -*- */

#include <core.p4>
#include <tna.p4>

#include "headers.p4"
#include "types.p4"


#define SWITCHING_TABLE_SIZE	65536

const bit<3> ETH_SWITCH_LEARN_DIGEST = 0;


/***************************** Ingress ****************************************/
parser IngressParser(
	packet_in pkt,
	out my_ingress_headers_t hdr,
	out my_ingress_metadata_t meta,
	out ingress_intrinsic_metadata_t ig_intr_md)
{
	state start {
		pkt.extract(ig_intr_md);
		pkt.advance(PORT_METADATA_SIZE);

		transition meta_init;
	}

	state meta_init {
		meta.mac_moved = 0;
		meta.ingress_port = ig_intr_md.ingress_port;

		transition parser_ethernet;
	}

	state parser_ethernet {
		pkt.extract(hdr.ethernet);

		transition accept;
	}
}

control Ingress(
	inout my_ingress_headers_t hdr,
	inout my_ingress_metadata_t meta,
	in ingress_intrinsic_metadata_t ig_intr_md,
	in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
	inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
	inout ingress_intrinsic_metadata_for_tm_t ig_tm_md)
{
	action send(PortId_t port) {
		ig_tm_md.ucast_egress_port = port;
	}

	action bcast() {
		ig_tm_md.mcast_grp_a = 1;
	}

	action drop() {
		ig_dprsr_md.drop_ctl = 1;
	}

	table switching_table {
		key = { hdr.ethernet.dst_addr : exact; }
		actions = { send; bcast; drop; }

		default_action = bcast;
		size = SWITCHING_TABLE_SIZE;
	}

	/* Keep track of known known addresses for matching them on the src field */
	action stsrc_unknown()
	{
		ig_dprsr_md.digest_type = ETH_SWITCH_LEARN_DIGEST;
	}

	action stsrc_known(PortId_t port)
	{
		meta.mac_moved = port ^ ig_intr_md.ingress_port;
	}

	@idletime_precision(3)
	table switching_table_src {
		key = { hdr.ethernet.src_addr : exact; }
		actions = { stsrc_known; stsrc_unknown; }

		default_action = stsrc_unknown;
		size = SWITCHING_TABLE_SIZE;
		idle_timeout = true;
	}

	apply {
		if (hdr.ethernet.isValid())
		{
			switching_table_src.apply();

			/* If the MAC address moved, treat it as unknown */
			if (meta.mac_moved != 0)
				stsrc_unknown();

			switching_table.apply();
		}
	}
}


struct eth_switch_digest_t {
	mac_addr_t src_mac;
	PortId_t ingress_port;
}

control IngressDeparser(
	packet_out pkt,
	inout my_ingress_headers_t hdr,
	in my_ingress_metadata_t meta,
	in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md)
{
	Digest<eth_switch_digest_t>() eth_switch_digest;

	apply {
		if (ig_dprsr_md.digest_type == ETH_SWITCH_LEARN_DIGEST)
		{
			eth_switch_digest.pack({
				hdr.ethernet.src_addr,
				meta.ingress_port
			});
		}

		pkt.emit(hdr);
	}
}


/***************************** Egress *****************************************/
parser EgressParser(
	packet_in pkt,
	out my_egress_headers_t hdr,
	out my_egress_metadata_t meta,
	out egress_intrinsic_metadata_t eg_intr_md)
{
	state start {
		pkt.extract(eg_intr_md);
		transition accept;
	}
}

control Egress(
	inout my_egress_headers_t hdr,
	inout my_egress_metadata_t meta,
	in egress_intrinsic_metadata_t eg_intr_md,
	in egress_intrinsic_metadata_from_parser_t eg_prsr_md,
	inout egress_intrinsic_metadata_for_deparser_t eg_dprsr_md,
	inout egress_intrinsic_metadata_for_output_port_t eg_oport_md)
{
	apply {
	}
}

control EgressDeparser(
	packet_out pkt,
	inout my_egress_headers_t hdr,
	in my_egress_metadata_t meta,
	in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md)
{
	apply {
		pkt.emit(hdr);
	}
}


/************************** P4 pipeline definition ****************************/
Pipeline(
	IngressParser(),
	Ingress(),
	IngressDeparser(),
	EgressParser(),
	Egress(),
	EgressDeparser()
) pipe;

Switch(pipe) main;
