/* -*- P4_16 -*- */

#include <core.p4>
#include <tna.p4>

#include "headers.p4"
#include "types.p4"


#define SWITCHING_TABLE_SIZE	262144


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

	action drop() {
		ig_dprsr_md.drop_ctl = 1;
	}

	table switching_table {
		key = { hdr.ethernet.dst_addr : exact; }
		actions = { send; drop; }

		default_action = send(64);
		size = SWITCHING_TABLE_SIZE;
	}

	apply {
		if (hdr.ethernet.isValid()) {
			switching_table.apply();
		}
	}
}

control IngressDeparser(
	packet_out pkt,
	inout my_ingress_headers_t hdr,
	in my_ingress_metadata_t meta,
	in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md)
{
	apply {
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
