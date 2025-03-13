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
		meta.is_complete_v = 0;
		meta.port_bitmap = 0;
		meta.is_agg = 0;
		meta.agg_complete = 0;

		transition parser_ethernet;
	}

	state parser_ethernet {
		pkt.extract(hdr.ethernet);

		transition select(hdr.ethernet.dst_addr[47:40]) {
			0x62 : parse_aggregate;
			default : accept;
		}
	}

	state parse_aggregate {
		pkt.extract(hdr.aggregate);

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




	Register<value_t, bit<12>>(1, 0) values;
	RegisterAction<value_t, bit<12>, value_t>(values) aggregate_value = {
		void apply(inout value_t value, out value_t new_value) {
			value = value + hdr.aggregate.val000;
			new_value = value;
		}
	};

	RegisterAction<value_t, bit<12>, value_t>(values) aggregate_clear = {
		void apply(inout value_t value, out value_t new_value) {
			new_value = value + hdr.aggregate.val000;
			value = 0;
		}
	};


	Register<node_bitmap_t, bit<12>>(1, 0) bitmaps;
	RegisterAction<node_bitmap_t, bit<12>, bit<32>>(bitmaps) update_bitmap = {
		void apply(inout node_bitmap_t bitmap, out bit<32> complete) {
			if (bitmap == 0x3) {
				bitmap = meta.port_bitmap;
			} else {
				bitmap = bitmap | meta.port_bitmap;
			}

			complete = bitmap;
		}
	};


	action complete() {
		ig_dprsr_md.drop_ctl = 0;
		meta.agg_complete = 1;
	}

	table is_complete {
		key = { meta.is_complete_v : exact; }
		actions = { NoAction; complete; }

		default_action = NoAction;
		size = 16;

		const entries = {
			0x3: complete();
		}
	}

	action aggregate() {
		// Perform aggregation
		ig_tm_md.mcast_grp_a = 0x10;
		ig_dprsr_md.drop_ctl = 1;
		meta.is_agg = 1;
	}




	action get_node_bitmap_spec(bit<32> bitmap) {
		meta.port_bitmap = bitmap;
	}

	table get_node_bitmap {
		key = { ig_intr_md.ingress_port : exact; }
		actions = { get_node_bitmap_spec; }

		const entries = {
			0  : get_node_bitmap_spec(1 << 0);
			4  : get_node_bitmap_spec(1 << 1);
			8  : get_node_bitmap_spec(1 << 2);
			12 : get_node_bitmap_spec(1 << 3);
			16 : get_node_bitmap_spec(1 << 4);
			20 : get_node_bitmap_spec(1 << 5);
			24 : get_node_bitmap_spec(1 << 6);
			28 : get_node_bitmap_spec(1 << 7);
			32 : get_node_bitmap_spec(1 << 8);
			36 : get_node_bitmap_spec(1 << 9);
			40 : get_node_bitmap_spec(1 << 10);
			44 : get_node_bitmap_spec(1 << 11);
			48 : get_node_bitmap_spec(1 << 12);
			52 : get_node_bitmap_spec(1 << 13);
			56 : get_node_bitmap_spec(1 << 14);
			60 : get_node_bitmap_spec(1 << 15);
		}

		size = 64;
	}


	action agg_intermediate() {
		hdr.aggregate.val000 = aggregate_value.execute(0);
	}

	action agg_last() {
		hdr.aggregate.val000 = aggregate_clear.execute(0);
	}

	table agg_table {
		key = { meta.agg_complete : exact; }
		actions = { agg_intermediate; agg_last; }
		size = 2;

		const entries = {
			1w0 : agg_intermediate();
			1w1 : agg_last();
		}
	}





	table switching_table {
		key = { hdr.ethernet.dst_addr : exact; }
		actions = { send; bcast; drop; aggregate; }

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

	action stsrc_static()
	{
	}

	@idletime_precision(3)
	table switching_table_src {
		key = { hdr.ethernet.src_addr : exact; }
		actions = { stsrc_known; stsrc_unknown; stsrc_static; }

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

			if (meta.is_agg == 1) {
				get_node_bitmap.apply();
				meta.is_complete_v = update_bitmap.execute(0);
				is_complete.apply();

				agg_table.apply();
			}
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
