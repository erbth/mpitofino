/* -*- P4_16 -*- */

#include <core.p4>
#include <tna.p4>

#include "headers.p4"
#include "types.p4"
#include "collectives.p4"
#include "collectives_distributor.p4"


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
		meta.handled = 0;
		meta.ingress_port = ig_intr_md.ingress_port;
		meta.is_coll = 0;

		/* Collectives module's state */
		meta.agg_unit = 65535;
		meta.node_bitmap = {0, 0};
		meta.full_bitmap = {0, 0};
		meta.agg_is_clear = false;

		meta.bridge_header.setValid();
		meta.bridge_header.agg_unit = 65535;

		transition parse_ethernet;
	}

	state parse_ethernet {
		pkt.extract(hdr.ethernet);

		transition select(hdr.ethernet.ether_type) {
			ether_type_t.IPV4 : parse_ipv4;
			default : accept;
		}
	}

	state parse_ipv4 {
		pkt.extract(hdr.ipv4);

		transition select(hdr.ipv4.protocol) {
			ipv4_protocol_t.UDP : parse_udp;
			default : accept;
		}
	}

	state parse_udp {
		pkt.extract(hdr.udp);

		transition select(hdr.udp.dst_port) {
			0x4000 : parse_aggregate;
			default : accept;
		}
	}

	state parse_aggregate {
		pkt.extract(hdr.aggregate);

		transition select(ig_intr_md.ingress_port) {
			68 : parse_aggregate_recirculated;  // pipe 0 recirculation port
			default : accept;
		}
	}

	state parse_aggregate_recirculated {
		meta.agg_is_clear = true;

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
	/* Collectives module */
	Collectives() collectives;


	/* Ethernet Switch (to be factured out into a different submodule if
	 * appropriate */
	action send(PortId_t port) {
		ig_tm_md.ucast_egress_port = port;
	}

	action flood() {
		ig_tm_md.mcast_grp_a = MCAST_FLOOD;
	}

	action bcast() {
		ig_tm_md.mcast_grp_a = MCAST_BCAST;
	}

	action drop() {
		ig_dprsr_md.drop_ctl = 1;
	}

	/* A packet for the collective data processing unit has been received */
	action collective() {
		meta.is_coll = 1;
	}

	table switching_table {
		key = { hdr.ethernet.dst_addr : exact; }
		actions = { send; flood; bcast; drop; collective; }

		default_action = flood;
		size = SWITCHING_TABLE_SIZE;
	}

	/* Keep track of known known addresses for matching them on the src field */
	action stsrc_unknown()
	{
		ig_dprsr_md.digest_type = ETH_SWITCH_LEARN_DIGEST;
	}

	@idletime_precision(3)
	table switching_table_src {
		key = {
			hdr.ethernet.src_addr : exact;
			meta.ingress_port : exact;
		}
		actions = { NoAction; stsrc_unknown; }

		default_action = stsrc_unknown;
		size = SWITCHING_TABLE_SIZE;
		idle_timeout = true;
	}


	apply {
		if (hdr.ethernet.isValid())
		{
			/* Ethernet switch */
			switching_table_src.apply();
			switching_table.apply();

			if (meta.is_coll == 1 && hdr.udp.isValid()) {
				collectives.apply(hdr, meta, ig_intr_md, ig_dprsr_md, ig_tm_md);
			}
		}

		ig_tm_md.level1_exclusion_id = (bit<16>) ig_intr_md.ingress_port;
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

		pkt.emit(meta.bridge_header);
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
		pkt.extract(meta.bridge_header);

		/* NOTE: An alternative implementation would be to not send all packet
		 * headers through the TM for mpitofino-traffic, but only a small bridge
		 * header. */

		transition parse_ethernet;
	}

	state parse_ethernet {
		pkt.extract(hdr.ethernet);

		transition select(hdr.ethernet.ether_type) {
			ether_type_t.IPV4 : parse_ipv4;
			default : accept;
		}
	}

	state parse_ipv4 {
		pkt.extract(hdr.ipv4);

		transition select(hdr.ipv4.protocol) {
			ipv4_protocol_t.UDP : parse_udp;
			default : accept;
		}
	}

	state parse_udp {
		pkt.extract(hdr.udp);

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
	CollectivesDistributor() collectives_distributor;

	apply {
		if (meta.bridge_header.agg_unit != 65535) {
			collectives_distributor.apply(hdr, meta, eg_intr_md);
		}
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
