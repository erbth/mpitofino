/* -*- P4_16 -* */

#include <core.p4>
#include <tna.p4>

#include "headers.p4"
#include "types.p4"
#include "collectives.p4"
#include "collectives_distributor.p4"


#define SWITCHING_TABLE_SIZE	2048

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
		meta.ingress_port = ig_intr_md.ingress_port;
		meta.mac_for_digest = 0;
		meta.is_coll = 0;

		/* Collectives module's state */
		meta.agg_unit = 65535;
		meta.agg_have_unit = false;
		meta.node_bitmap = {0, 0};
		meta.agg_is_clear = false;

		meta.bridge_header.setValid();
		meta.bridge_header.agg_unit = 65535;
		meta.bridge_header.is_roce_ack = false;

		transition select(ig_intr_md.ingress_port) {
			64 : parse_cpoffload;
			128 &&& 0x180 : parse_recirc_fanout;
			256 &&& 0x180 : parse_recirc_fanout;
			default : parse_ethernet;
		}
	}

	state parse_cpoffload {
		/* Strip pseudo header that designates this as Ethernet II frame */
		pkt.advance(14*8);

		pkt.extract(meta.cpoffload);

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
			4791 : parse_roce;
			default : accept;
		}
	}

	state parse_roce {
		pkt.extract(hdr.roce);

		/* NOTE: IB ACK packets will be 28 octets long in UDP. This is
		bad logic, but the match registers of the parser appear to be
		exhausted already... */
		transition select(hdr.udp.length) {
			28 : accept;
			280 : parse_aggregate;
			default : accept;
		}
	}

	state parse_aggregate {
		pkt.extract(hdr.aggregate);

		transition select(ig_intr_md.ingress_port) {
			68 : parse_aggregate_recirculated;  // pipe 0 recirculation port
			196 : parse_aggregate_recirculated;  // pipe 1 recirculation port 1
			324 : parse_aggregate_recirculated;  // pipe 2 recirculation port 1
			452 : parse_aggregate_recirculated;  // pipe 3 recirculation port 1
			default : accept;
		}
	}

	state parse_aggregate_recirculated {
		meta.agg_is_clear = true;

		transition accept;
	}


	state parse_recirc_fanout {
		pkt.extract(hdr.recirc_fanout);
		pkt.extract(hdr.recirc_fanout_payload);
		pkt.extract(hdr.roce_checksum);
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

	CRCPolynomial<bit<32>>(0x04c11db7, true, true, false, 0xffffffff, 0xffffffff) icrc_poly;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, icrc_poly) icrc_hash1;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, icrc_poly) icrc_hash2;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, icrc_poly) icrc_hash3;


	action roce_ack_reflect(bit<24> dst_qp)
	{
		bit<48> tmp_mac;
		bit<32> tmp_ip;
		
		tmp_mac = hdr.ethernet.src_addr;
		hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
		hdr.ethernet.dst_addr = tmp_mac;

		tmp_ip = hdr.ipv4.src_addr;
		hdr.ipv4.src_addr = hdr.ipv4.dst_addr;
		hdr.ipv4.dst_addr = tmp_ip;

		hdr.udp.src_port = 4791;

		hdr.roce.dst_qp = dst_qp;

		ig_tm_md.ucast_egress_port = meta.ingress_port;
		meta.bridge_header.is_roce_ack = true;
	}
	
	table roce_ack_reflector {
		key = {
			hdr.ipv4.src_addr : exact;
			hdr.roce.dst_qp : exact;
		}

		actions = {
			roce_ack_reflect;
			NoAction;
		}
		default_action = NoAction;

		size = 1024;
	}


	action adapt_recirc_fanout_sub() {
		ig_tm_md.ucast_egress_port = meta.ingress_port & 9w0x7f;
	}

	action adapt_recirc_fanout_add() {
		ig_tm_md.ucast_egress_port = meta.ingress_port | 9w0x80;
	}
	
	/* Somehow does not work with meta.ingress_port[8:7] *and* use of
	meta.ingress_port in the actions. */
	table adapt_recirc_fanout_port {
		key = {
			meta.ingress_port : ternary;
		}

		actions = {
			adapt_recirc_fanout_sub;
			adapt_recirc_fanout_add;
			NoAction;
		}
		default_action = NoAction;
		size = 2;

		const entries = {
			(128 &&& 0x180) : adapt_recirc_fanout_sub();
			(256 &&& 0x180) : adapt_recirc_fanout_add();
		}
	}


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

		/* Save current mac address, as it might be changed later by the RoCE
		   ack reflector */
		meta.mac_for_digest = hdr.ethernet.src_addr;
	}

	@idletime_precision(3)
	table switching_table_src {
		key = {
			hdr.ethernet.src_addr : ternary;
			meta.ingress_port : ternary;
		}
		actions = { NoAction; stsrc_unknown; }

		default_action = stsrc_unknown;
		size = SWITCHING_TABLE_SIZE;
		idle_timeout = true;
	}


	apply {
		ig_tm_md.bypass_egress = 0;

		/* Logics violation: This should really be in the else if-if-clause... */
		if (hdr.aggregate.isValid())
		{
			collectives.apply(hdr, meta, ig_intr_md, ig_dprsr_md, ig_tm_md);
		}
		else if (hdr.recirc_fanout.isValid())
		{
			adapt_recirc_fanout_port.apply();
			hdr.roce_checksum.icrc = hdr.recirc_fanout.icrc;
			ig_tm_md.bypass_egress = 1;
			ig_dprsr_md.drop_ctl = 0;

			hdr.recirc_fanout.setInvalid();
		}

		if (meta.cpoffload.isValid() && meta.cpoffload.port_id != 65535)
		{
			/* Override all other actions and output the packet on the
			 * specified port. */
			ig_tm_md.ucast_egress_port = (PortId_t) meta.cpoffload.port_id;
			ig_dprsr_md.drop_ctl = 0;
		}
		else if (hdr.ethernet.isValid())
		{

			/* Ethernet switch */
			switching_table_src.apply();
			switching_table.apply();

			/* NOTE: Avoid Layer-2 address checking (layering violation!) s.t.
			   the unit_selector can happen a stage earlier. */
			if (hdr.aggregate.isValid())
			{
				if (meta.agg_have_unit)
				{
					bit<32> tmp;

					tmp = icrc_hash1.get({
						hdr.aggregate.val00,
						hdr.aggregate.val01,
						hdr.aggregate.val02,
						hdr.aggregate.val03,
						hdr.aggregate.val04,
						hdr.aggregate.val05,
						hdr.aggregate.val06,
						hdr.aggregate.val07,
						hdr.aggregate.val08,
						hdr.aggregate.val09,
						hdr.aggregate.val10,
						hdr.aggregate.val11,
						hdr.aggregate.val12,
						hdr.aggregate.val13,
						hdr.aggregate.val14,
						hdr.aggregate.val15,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0
					});

					tmp = tmp ^ icrc_hash2.get({
						hdr.aggregate.val16,
						hdr.aggregate.val17,
						hdr.aggregate.val18,
						hdr.aggregate.val19,
						hdr.aggregate.val20,
						hdr.aggregate.val21,
						hdr.aggregate.val22,
						hdr.aggregate.val23,
						hdr.aggregate.val24,
						hdr.aggregate.val25,
						hdr.aggregate.val26,
						hdr.aggregate.val27,
						hdr.aggregate.val28,
						hdr.aggregate.val29,
						hdr.aggregate.val30,
						hdr.aggregate.val31,
						hdr.aggregate.val32,
						hdr.aggregate.val33,
						hdr.aggregate.val34,
						hdr.aggregate.val35,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0,
						32w0
					});

					meta.bridge_header.icrc = tmp ^ icrc_hash3.get({
						hdr.aggregate.val36,
						hdr.aggregate.val37,
						hdr.aggregate.val38,
						hdr.aggregate.val39,
						hdr.aggregate.val40,
						hdr.aggregate.val41,
						hdr.aggregate.val42,
						hdr.aggregate.val43,
						hdr.aggregate.val44,
						hdr.aggregate.val45,
						hdr.aggregate.val46,
						hdr.aggregate.val47,
						hdr.aggregate.val48,
						hdr.aggregate.val49,
						hdr.aggregate.val50,
						hdr.aggregate.val51,
						hdr.aggregate.val52,
						hdr.aggregate.val53,
						hdr.aggregate.val54,
						hdr.aggregate.val55,
						hdr.aggregate.val56,
						hdr.aggregate.val57,
						hdr.aggregate.val58,
						hdr.aggregate.val59,
						hdr.aggregate.val60,
						hdr.aggregate.val61,
						hdr.aggregate.val62,
						hdr.aggregate.val63
					});
				}
			}
			else if (meta.is_coll == 1 && hdr.roce.isValid() && hdr.roce.opcode == 17)
			{
				/* ACK */
				roce_ack_reflector.apply();
			}
			else if (meta.is_coll == 1)
			{
				/* Forward to CPU to handle e.g. ICMP */
				ig_tm_md.ucast_egress_port = 64;
			}
		}

		ig_tm_md.level1_exclusion_id = (bit<16>) ig_intr_md.ingress_port;

		if (ig_tm_md.bypass_egress == 1)
			meta.bridge_header.setInvalid();
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
				meta.mac_for_digest,
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

		transition select(meta.bridge_header.agg_unit) {
			65535 : parse_non_aggregate;
			default: parse_aggregate;
		}
	}

	state parse_non_aggregate {
		transition select(meta.bridge_header.is_roce_ack) {
			true : parse_roce_ack;
			default : accept;
		}
	}

	state parse_roce_ack {
		pkt.extract(hdr.ethernet);
		pkt.extract(hdr.ipv4);
		pkt.extract(hdr.udp);
		pkt.extract(hdr.roce);
		pkt.extract(hdr.roce_ack);

		transition accept;
	}

	state parse_aggregate {
		pkt.extract(hdr.ethernet);
		pkt.extract(hdr.ipv4);
		pkt.extract(hdr.udp);
		pkt.extract(hdr.roce);

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

	CRCPolynomial<bit<32>>(0x04c11db7, true, true, false, 0xffffffff, 0xffffffff) icrc_poly;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, icrc_poly) icrc_hash1;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, icrc_poly) icrc_hash2;

	apply {
		if (hdr.roce_ack.isValid())
		{
			bit<32> tmp;

			tmp = icrc_hash1.get({
				hdr.ipv4.version,
				hdr.ipv4.ihl,
				8w0xff,
				hdr.ipv4.total_length,
				hdr.ipv4.identification,
				hdr.ipv4.flags,
				hdr.ipv4.fragment_offset,
				8w0xff,
				hdr.ipv4.protocol,
				16w0xffff,
				hdr.ipv4.src_addr,
				hdr.ipv4.dst_addr,
				hdr.udp.src_port,
				hdr.udp.dst_port,
				hdr.udp.length,
				16w0xffff,
				hdr.roce.opcode,
				hdr.roce.se,
				hdr.roce.migreq,
				hdr.roce.pad_cnt,
				hdr.roce.version,
				hdr.roce.partition_key,
				8w0xff,
				hdr.roce.dst_qp,
				hdr.roce.ack_req,
				hdr.roce.reserved2,
				hdr.roce.psn,
				hdr.roce_ack.syndrome,
				hdr.roce_ack.msn
			}) ^ 0x71acd589;  // Correcting summand because of missing LRH

			/* Not sure why an endianess conversion is required here */
			hdr.roce_ack.icrc = tmp[7:0] ++ tmp[15:8] ++ tmp[23:16] ++ tmp[31:24];
		}
		else if (hdr.roce.isValid())
		{
			collectives_distributor.apply(hdr, meta, eg_intr_md);

			bit<32> tmp;

			/* Update ICRC (CRC(A^B) = CRC(A) ^ CRC(B) ^ const; and
			padding to the left with zeros works transparently by
			applaying the same equality. see
			e.g. https://stackoverflow.com/a/23126768) */
			tmp = meta.bridge_header.icrc ^ icrc_hash2.get({
				hdr.ipv4.version,
				hdr.ipv4.ihl,
				8w0xff,
				hdr.ipv4.total_length,
				hdr.ipv4.identification,
				hdr.ipv4.flags,
				hdr.ipv4.fragment_offset,
				8w0xff,
				hdr.ipv4.protocol,
				16w0xffff,
				hdr.ipv4.src_addr,
				hdr.ipv4.dst_addr,
				hdr.udp.src_port,
				hdr.udp.dst_port,
				hdr.udp.length,
				16w0xffff,
				hdr.roce.opcode,
				hdr.roce.se,
				hdr.roce.migreq,
				hdr.roce.pad_cnt,
				hdr.roce.version,
				hdr.roce.partition_key,
				8w0xff,
				hdr.roce.dst_qp,
				hdr.roce.ack_req,
				hdr.roce.reserved2,
				hdr.roce.psn,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0,
				32w0
			});

			/* Not sure why an endianess conversion is required here */
			tmp = tmp[7:0] ++ tmp[15:8] ++ tmp[23:16] ++ tmp[31:24];

			hdr.recirc_fanout.setValid();

			/* xor const */
			hdr.recirc_fanout.icrc = tmp ^ 0x60d95a72;
		}
	}
}

control EgressDeparser(
	packet_out pkt,
	inout my_egress_headers_t hdr,
	in my_egress_metadata_t meta,
	in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md)
{
	Checksum() ipv4_checksum;

	apply {
		if (hdr.ipv4.isValid()) {
			hdr.ipv4.header_checksum = ipv4_checksum.update({
				hdr.ipv4.version,
				hdr.ipv4.ihl,
				hdr.ipv4.tos,
				hdr.ipv4.total_length,
				hdr.ipv4.identification,
				hdr.ipv4.flags,
				hdr.ipv4.fragment_offset,
				hdr.ipv4.ttl,
				hdr.ipv4.protocol,
				hdr.ipv4.src_addr,
				hdr.ipv4.dst_addr});
		}

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
