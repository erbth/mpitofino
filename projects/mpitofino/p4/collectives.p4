control AggregationUnit(
	inout my_ingress_metadata_t meta,
	inout bit<32> a,
	inout bit<32> b)
{
	/* 256 collective channels * 4 units/channel = 1024
	 * values */
	Register<value_pair_t, bit<12>>(1024, {0, 0}) values;
	RegisterAction<value_pair_t, bit<12>, value_t>(values) value_int_32_add = {
		void apply(inout value_pair_t value, out value_t new_value) {
			value.low = value.low + a;
			value.high = value.high + b;

			new_value = value.low;
		}
	};

	/* Will be called on the final recirculation */
	RegisterAction<value_pair_t, bit<12>, value_t>(values) value_int_32_clear = {
		void apply(inout value_pair_t value, out value_t new_value) {
			new_value = value.high;

			value.low = 0;
			value.high = 0;
		}
	};


	action agg_int_32_add() {
		/* Aggregate and output low value */
		a = value_int_32_add.execute((bit<12>) meta.agg_unit);
	}

	action agg_int_32_clear() {
		/* Output high value and clear */
		b = value_int_32_clear.execute((bit<12>) meta.agg_unit);
	}


	table choose_action {
		key = {
			meta.agg_unit : exact;
			meta.agg_is_clear : exact;
		}

		actions = {
			agg_int_32_add;
			agg_int_32_clear;
			NoAction;
		}
		default_action = NoAction;

		/* 1024 units collective channels * 2 options */
		size = 2048;

		//const entries = {
		//	(1, false) : agg_int_32_add;
		//	(1, true ) : agg_int_32_clear;
		//}
	}

	apply {
		choose_action.apply();
	}
}


control Collectives(
	inout my_ingress_headers_t hdr,
	inout my_ingress_metadata_t meta,
	in ingress_intrinsic_metadata_t ig_intr_md,
	inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
	inout ingress_intrinsic_metadata_for_tm_t ig_tm_md)
{
	/* Aggregation */
	AggregationUnit() agg00;
	AggregationUnit() agg01;
	AggregationUnit() agg02;
	AggregationUnit() agg03;
	AggregationUnit() agg04;
	AggregationUnit() agg05;
	AggregationUnit() agg06;
	AggregationUnit() agg07;
	AggregationUnit() agg08;
	AggregationUnit() agg09;
	AggregationUnit() agg10;
	AggregationUnit() agg11;
	AggregationUnit() agg12;
	AggregationUnit() agg13;
	AggregationUnit() agg14;
	AggregationUnit() agg15;
	AggregationUnit() agg16;
	AggregationUnit() agg17;
	AggregationUnit() agg18;
	AggregationUnit() agg19;
	AggregationUnit() agg20;
	AggregationUnit() agg21;
	AggregationUnit() agg22;
	AggregationUnit() agg23;
	AggregationUnit() agg24;
	AggregationUnit() agg25;
	AggregationUnit() agg26;
	AggregationUnit() agg27;
	AggregationUnit() agg28;
	AggregationUnit() agg29;
	AggregationUnit() agg30;
	AggregationUnit() agg31;


	/* A 64-bit bitmap to track state of the aggregation at this
	 * aggregation-node (switch). Note how the ALU restricts the final
	 * comparison in such a way that we cannot use a 64bit low/high register
	 * 'word-pair' and also read the result within one pipeline pass. */
	Register<bit<32>, bit<12>>(1024, 0) state_bitmaps_low;
	Register<bit<32>, bit<12>>(1024, 0) state_bitmaps_high;

	RegisterAction<bit<32>, bit<12>, bit<32>>(state_bitmaps_low) update_bitmap_low = {
		void apply(inout bit<32> bitmap, out bit<32> new_bitmap) {
			bitmap = bitmap | meta.node_bitmap.low;
			new_bitmap = bitmap;
		}
	};

	RegisterAction<bit<32>, bit<12>, bit<32>>(state_bitmaps_high) update_bitmap_high = {
		void apply(inout bit<32> bitmap, out bit<32> new_bitmap) {
			bitmap = bitmap | meta.node_bitmap.high;
			new_bitmap = bitmap;
		}
	};

	RegisterAction<bit<32>, bit<12>, bit<32>>(state_bitmaps_low) clear_bitmap_low = {
		void apply(inout bit<32> bitmap, out bit<32> new_bitmap) {
			bitmap = 0;
			new_bitmap = bitmap;
		}
	};

	RegisterAction<bit<32>, bit<12>, bit<32>>(state_bitmaps_high) clear_bitmap_high = {
		void apply(inout bit<32> bitmap, out bit<32> new_bitmap) {
			bitmap = 0;
			new_bitmap = bitmap;
		}
	};


	action check_complete_true() {
		ig_dprsr_md.drop_ctl = 0;
		invalidate(ig_tm_md.mcast_grp_a);

		/* Recirculate (NOTE: Would need to be changed dynamically when
		 * splitting the switch into virtual switches according to pipes) */
		ig_tm_md.ucast_egress_port = 68;
	}

	action check_complete_clear() {
		ig_dprsr_md.drop_ctl = 0;
	}

	table check_complete {
		key = {
			meta.agg_unit : exact;
			meta.node_bitmap.low : ternary;
			meta.node_bitmap.high : ternary;
			meta.agg_is_clear : exact;
		}

		actions = {
			check_complete_true;
			check_complete_clear;
			NoAction;
		}

		default_action = NoAction;

		/* TODO: Adjust all table sizes to match the control plane */
		size = 2048;

		//const entries = {
		//	(1, 0x3, 0x0, false) : check_complete_true();
		//	(1, _, _, true) : check_complete_clear();
		//}
	}


	action no_agg_unit() {
		ig_dprsr_md.drop_ctl = 1;
	}

	action select_agg_unit(bit<16> mcast_grp, bit<16> agg_unit,
			bit<32> node_bitmap_low, bit<32> node_bitmap_high)
	{
		// Setup aggregation configuration
		ig_tm_md.mcast_grp_a = mcast_grp;
		ig_dprsr_md.drop_ctl = 1;
		meta.agg_unit = agg_unit;
		meta.bridge_header.agg_unit = agg_unit;
		meta.node_bitmap.low  = node_bitmap_low;
		meta.node_bitmap.high = node_bitmap_high;

		meta.handled = 1;
	}

	table unit_selector {
		key = {
			hdr.ipv4.src_addr : exact;
			hdr.ipv4.dst_addr : exact;
			hdr.udp.src_port : exact;
			hdr.udp.dst_port : exact;
		}

		actions = {
			no_agg_unit;
			select_agg_unit;
		}

		default_action = no_agg_unit;

		//const entries = {
		//	(0x0a0a0001, 0x0a0a8000, 0x4000, 0x6000) : select_agg_unit(0x10, 1, 0x1, 0);
		//	(0x0a0a0002, 0x0a0a8000, 0x4000, 0x6000) : select_agg_unit(0x10, 1, 0x2, 0);
		//}

		size = 1024;
	}

	apply {
		/* Select an aggregation unit based on channel, client, and (in the
		 * future) sequence number */
		unit_selector.apply();

		if (meta.agg_unit != 65535 && !meta.agg_is_clear)
		{
			/* Track progress of aggregation at this aggregation-node (switch)
			 * */
			meta.node_bitmap.low = update_bitmap_low.execute((bit<12>) meta.agg_unit);
			meta.node_bitmap.high = update_bitmap_high.execute((bit<12>) meta.agg_unit);
		}
		else if (meta.agg_unit != 65535)
		{
			clear_bitmap_low.execute((bit<12>) meta.agg_unit);
			clear_bitmap_high.execute((bit<12>) meta.agg_unit);
		}

		check_complete.apply();

		/* These statements do not need the gateway, because they contain a
		 * table which matches on meta.agg_unit anyway */
		agg00.apply(meta, hdr.aggregate.val00, hdr.aggregate.val01);
		agg01.apply(meta, hdr.aggregate.val02, hdr.aggregate.val03);
		agg02.apply(meta, hdr.aggregate.val04, hdr.aggregate.val05);
		agg03.apply(meta, hdr.aggregate.val06, hdr.aggregate.val07);
		agg04.apply(meta, hdr.aggregate.val08, hdr.aggregate.val09);
		agg05.apply(meta, hdr.aggregate.val10, hdr.aggregate.val11);
		agg06.apply(meta, hdr.aggregate.val12, hdr.aggregate.val13);
		agg07.apply(meta, hdr.aggregate.val14, hdr.aggregate.val15);
		agg08.apply(meta, hdr.aggregate.val16, hdr.aggregate.val17);
		agg09.apply(meta, hdr.aggregate.val18, hdr.aggregate.val19);
		agg10.apply(meta, hdr.aggregate.val20, hdr.aggregate.val21);
		agg11.apply(meta, hdr.aggregate.val22, hdr.aggregate.val23);
		agg12.apply(meta, hdr.aggregate.val24, hdr.aggregate.val25);
		agg13.apply(meta, hdr.aggregate.val26, hdr.aggregate.val27);
		agg14.apply(meta, hdr.aggregate.val28, hdr.aggregate.val29);
		agg15.apply(meta, hdr.aggregate.val30, hdr.aggregate.val31);
		agg16.apply(meta, hdr.aggregate.val32, hdr.aggregate.val33);
		agg17.apply(meta, hdr.aggregate.val34, hdr.aggregate.val35);
		agg18.apply(meta, hdr.aggregate.val36, hdr.aggregate.val37);
		agg19.apply(meta, hdr.aggregate.val38, hdr.aggregate.val39);
		agg20.apply(meta, hdr.aggregate.val40, hdr.aggregate.val41);
		agg21.apply(meta, hdr.aggregate.val42, hdr.aggregate.val43);
		agg22.apply(meta, hdr.aggregate.val44, hdr.aggregate.val45);
		agg23.apply(meta, hdr.aggregate.val46, hdr.aggregate.val47);
		agg24.apply(meta, hdr.aggregate.val48, hdr.aggregate.val49);
		agg25.apply(meta, hdr.aggregate.val50, hdr.aggregate.val51);
		agg26.apply(meta, hdr.aggregate.val52, hdr.aggregate.val53);
		agg27.apply(meta, hdr.aggregate.val54, hdr.aggregate.val55);
		agg28.apply(meta, hdr.aggregate.val56, hdr.aggregate.val57);
		agg29.apply(meta, hdr.aggregate.val58, hdr.aggregate.val59);
		agg30.apply(meta, hdr.aggregate.val60, hdr.aggregate.val61);
		agg31.apply(meta, hdr.aggregate.val62, hdr.aggregate.val63);
	}
}
