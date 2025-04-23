#include <algorithm>
#include <string>
#include <filesystem>
#include "common/utils.h"
#include "asic_driver.h"
#include "bfrt_tools.h"

using namespace std;
using namespace bfrt;
namespace fs = std::filesystem;


ASICDriver::ASICDriver(StateRepository& st_repo)
	: st_repo(st_repo)
{
	/* Initialize bf_switchd */
	switchd_ctx.running_in_background = true;

	/* Ports are already there */
	switchd_ctx.skip_port_add = false;

	switchd_ctx.server_listen_local_only = true;
	switchd_ctx.dev_sts_thread = true;
	switchd_ctx.dev_sts_port = 7777;

	sde_install = getenv("SDE_INSTALL");
	if (sde_install.empty())
		throw runtime_error("Environment variable SDE_INSTALL is not set");

	switchd_ctx.install_dir = (char*) sde_install.c_str();

	program_conf_file = fs::path(sde_install) / "share/p4/targets/tofino/mpitofino.conf";
	switchd_ctx.conf_file = (char*) program_conf_file.c_str();

	auto bf_ret = bf_switchd_lib_init(&switchd_ctx);
	if (bf_ret != BF_SUCCESS)
		throw runtime_error("Failed to create bf_switchd");


	/* Retrieve BfRtInfo */
	auto& dev_mgr = BfRtDevMgr::getInstance();
	bf_ret = dev_mgr.bfRtInfoGet(dev_tgt.dev_id, "mpitofino", &bfrt_info);

	if (bf_ret != BF_SUCCESS)
		throw runtime_error("Failed to read BfRtInfo");


	/* Create session */
	session = BfRtSession::sessionCreate();
	if (!session)
		throw runtime_error("Failed to connect to bf_switchd");


	/* Lookup tables */
	find_tables();


	/* Perform initial setup */
	initial_setup();


	/* Subscribe to state repository */
	st_repo.subscribe_channels(bind(&ASICDriver::on_st_repo_channels, this));
}

void ASICDriver::find_tables()
{
#define RES_TBL(N, T) check_bf_status( \
		bfrt_info->bfrtTableFromNameGet(N, &T), \
		("Failed to resolve table " N));

	RES_TBL("$pre.node", pre_node)
	RES_TBL("$pre.mgid", pre_mgid)
	RES_TBL("Ingress.switching_table", switching_table);
	RES_TBL("Ingress.switching_table_src", switching_table_src);

	RES_TBL("Ingress.collectives.unit_selector", collectives_unit_selector);
	RES_TBL("Ingress.collectives.check_complete", collectives_check_complete);
	RES_TBL("Egress.collectives_distributor.output_address", collectives_output_address);

	RES_TBL("Ingress.collectives.agg00.choose_action", collectives_choose_action[0]);
	RES_TBL("Ingress.collectives.agg01.choose_action", collectives_choose_action[1]);
	RES_TBL("Ingress.collectives.agg02.choose_action", collectives_choose_action[2]);
	RES_TBL("Ingress.collectives.agg03.choose_action", collectives_choose_action[3]);
	RES_TBL("Ingress.collectives.agg04.choose_action", collectives_choose_action[4]);
	RES_TBL("Ingress.collectives.agg05.choose_action", collectives_choose_action[5]);
	RES_TBL("Ingress.collectives.agg06.choose_action", collectives_choose_action[6]);
	RES_TBL("Ingress.collectives.agg07.choose_action", collectives_choose_action[7]);
	RES_TBL("Ingress.collectives.agg08.choose_action", collectives_choose_action[8]);
	RES_TBL("Ingress.collectives.agg09.choose_action", collectives_choose_action[9]);
	RES_TBL("Ingress.collectives.agg10.choose_action", collectives_choose_action[10]);
	RES_TBL("Ingress.collectives.agg11.choose_action", collectives_choose_action[11]);
	RES_TBL("Ingress.collectives.agg12.choose_action", collectives_choose_action[12]);
	RES_TBL("Ingress.collectives.agg13.choose_action", collectives_choose_action[13]);
	RES_TBL("Ingress.collectives.agg14.choose_action", collectives_choose_action[14]);
	RES_TBL("Ingress.collectives.agg15.choose_action", collectives_choose_action[15]);
	RES_TBL("Ingress.collectives.agg16.choose_action", collectives_choose_action[16]);
	RES_TBL("Ingress.collectives.agg17.choose_action", collectives_choose_action[17]);
	RES_TBL("Ingress.collectives.agg18.choose_action", collectives_choose_action[18]);
	RES_TBL("Ingress.collectives.agg19.choose_action", collectives_choose_action[19]);
	RES_TBL("Ingress.collectives.agg20.choose_action", collectives_choose_action[20]);
	RES_TBL("Ingress.collectives.agg21.choose_action", collectives_choose_action[21]);
	RES_TBL("Ingress.collectives.agg22.choose_action", collectives_choose_action[22]);
	RES_TBL("Ingress.collectives.agg23.choose_action", collectives_choose_action[23]);
	RES_TBL("Ingress.collectives.agg24.choose_action", collectives_choose_action[24]);
	RES_TBL("Ingress.collectives.agg25.choose_action", collectives_choose_action[25]);
	RES_TBL("Ingress.collectives.agg26.choose_action", collectives_choose_action[26]);
	RES_TBL("Ingress.collectives.agg27.choose_action", collectives_choose_action[27]);
	RES_TBL("Ingress.collectives.agg28.choose_action", collectives_choose_action[28]);
	RES_TBL("Ingress.collectives.agg29.choose_action", collectives_choose_action[29]);
	RES_TBL("Ingress.collectives.agg30.choose_action", collectives_choose_action[30]);
	RES_TBL("Ingress.collectives.agg31.choose_action", collectives_choose_action[31]);
}

void ASICDriver::initial_setup()
{
	unique_lock lk_st(m_switching_table);

	/* Create multicast groups */
	/* PRE uses port-stride per pipe of 128; 16 100G ports per pipe */
	vector<bf_rt_id_t> port_ids;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 16; j++)
			port_ids.push_back(i*128 + j*4);
	}

	vector<bf_rt_id_t> port_xids(port_ids.cbegin(), port_ids.cend());

	for (auto p : port_ids)
	{
		check_bf_status(pre_node->tableEntryAdd(*session, dev_tgt,
					*table_create_key<uint64_t>(pre_node, {"$MULTICAST_NODE_ID", p}),
					*table_create_data<uint64_t, vector<bf_rt_id_t>, vector<bf_rt_id_t>>(
						pre_node,
						{"$MULTICAST_RID", 1},
						{"$MULTICAST_LAG_ID", vector<bf_rt_id_t>()},
						{"$DEV_PORT", vector<bf_rt_id_t>({p})}
					)
				),
				"Add entry to table pre.node");
	}

	check_bf_status(pre_mgid->tableEntryAdd(*session, dev_tgt,
				*table_create_key<uint64_t>(pre_mgid, {"$MGID", MCAST_FLOOD}),
				*table_create_data<vector<bf_rt_id_t>, vector<bool>, vector<bf_rt_id_t>>(
					pre_mgid,
					{"$MULTICAST_NODE_ID", port_ids},
					{"$MULTICAST_NODE_L1_XID_VALID", vector<bool>(port_xids.size(), true)},
					{"$MULTICAST_NODE_L1_XID", port_xids}
				)
			),
			"Add entry to table pre.mgid");

	/* Same, but with CPU port for broadcasts */
	port_ids.push_back(64);
	port_xids.push_back(64);
	for (auto& p : port_ids)
		p |= 0x0200;

	for (auto p : port_ids)
	{
		check_bf_status(pre_node->tableEntryAdd(*session, dev_tgt,
					*table_create_key<uint64_t>(pre_node, {"$MULTICAST_NODE_ID", p}),
					*table_create_data<uint64_t, vector<bf_rt_id_t>, vector<bf_rt_id_t>>(
						pre_node,
						{"$MULTICAST_RID", 1},
						{"$MULTICAST_LAG_ID", vector<bf_rt_id_t>()},
						{"$DEV_PORT", vector<bf_rt_id_t>({p & ~0x200})}
					)
				),
				"Add entry to table pre.node");
	}

	check_bf_status(pre_mgid->tableEntryAdd(*session, dev_tgt,
				*table_create_key<uint64_t>(pre_mgid, {"$MGID", MCAST_BCAST}),
				*table_create_data<vector<bf_rt_id_t>, vector<bool>, vector<bf_rt_id_t>>(
					pre_mgid,
					{"$MULTICAST_NODE_ID", port_ids},
					{"$MULTICAST_NODE_L1_XID_VALID", vector<bool>(port_xids.size(), true)},
					{"$MULTICAST_NODE_L1_XID", port_xids}
				)
			),
			"Add entry to table pre.mgid");


	/* Setup idle timeout callback for basic ethernet switch */
	std::unique_ptr<BfRtTableAttributes> attr;
	check_bf_status(
			switching_table_src->attributeAllocate(
				TableAttributesType::IDLE_TABLE_RUNTIME,
				TableAttributesIdleTableMode::NOTIFY_MODE,
				&attr),
			"Allocate idle timeout attribute");

	check_bf_status(
			attr->idleTableNotifyModeSet(
				true,
				bind(&ASICDriver::eth_switch_idle_cb, this,
					placeholders::_1, placeholders::_2, placeholders::_3),
				50, 50, 5000,
				nullptr),
			"Enable idle timeout callback");

	check_bf_status(
			switching_table_src->tableAttributesSet(
				*session, dev_tgt, 0, *attr),
			"Set idle timeout attribute");

	/* Setup learn callback for basic ethernet switch */
	check_bf_status(
			bfrt_info->bfrtLearnFromNameGet(
				"IngressDeparser.eth_switch_digest",
				&eth_switch_learn),
			"Failed to resolve ethernet switch digest");

	check_bf_status(
			eth_switch_learn->learnFieldIdGet(
				"src_mac", &eth_switch_learn_src_mac_id),
			"Get ethernet switch digest field id failed");

	check_bf_status(
			eth_switch_learn->learnFieldIdGet(
				"ingress_port", &eth_switch_learn_ingress_port_id),
			"Get ethernet switch digest field id failed");

	check_bf_status(
			eth_switch_learn->bfRtLearnCallbackRegister(
				session, dev_tgt,
				bind(&ASICDriver::eth_switch_learn_cb, this,
					placeholders::_1, placeholders::_2, placeholders::_3,
					placeholders::_4, placeholders::_5),
				nullptr),
			"Failed to register learn callback");


	/* Add special entries to switching table */
	update_switching_table(MacAddr("ff:ff:ff:ff:ff:ff"), "Ingress.bcast");
	update_switching_table(
			st_repo.get_collectives_module_mac_addr(),
			"Ingress.collective");

	/* Ignore all packets from the CPU port for learning */
	uint8_t value[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t mask[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	check_bf_status(table_add_or_mod(*switching_table_src,
				*session, dev_tgt,
				*table_create_key<const uint8_t*, uint64_t>(
					switching_table_src,
					table_field_desc_t<const uint8_t*>::create_ternary(
						"hdr.ethernet.src_addr", value, mask, sizeof(value)),
					table_field_desc_t<uint64_t>::create_ternary(
						"meta.ingress_port", 64, 0x1ff)
				),
				*table_create_data_action<>(
					switching_table_src,
					"NoAction"
				)
			),
			"Failed to add entry to switching_table_src");

	/* Ignore all packets coming from the recirculation port for mac address
	 * learning. */
	check_bf_status(table_add_or_mod(*switching_table_src,
				*session, dev_tgt,
				*table_create_key<const uint8_t*, uint64_t>(
					switching_table_src,
					table_field_desc_t<const uint8_t*>::create_ternary(
						"hdr.ethernet.src_addr", value, mask, sizeof(value)),
					table_field_desc_t<uint64_t>::create_ternary(
						"meta.ingress_port", 68, 0x1ff)
				),
				*table_create_data_action<>(
					switching_table_src,
					"NoAction"
				)
			),
			"Failed to add entry to switching_table_src");
}


bf_status_t ASICDriver::eth_switch_learn_cb(
		const bf_rt_target_t,
		const std::shared_ptr<bfrt::BfRtSession>,
		std::vector<std::unique_ptr<bfrt::BfRtLearnData>> learn_data,
		bf_rt_learn_msg_hdl* const learn_msg_hdl,
		const void*)
{
	unique_lock lk_st(m_switching_table);

	for (auto& d : learn_data)
	{
		MacAddr src_mac;
		uint64_t ingress_port;

		check_bf_status(
				d->getValue(
					eth_switch_learn_src_mac_id,
					sizeof(src_mac), reinterpret_cast<uint8_t*>(&src_mac)),
				"Failed to get src mac from digest");

		check_bf_status(
				d->getValue(
					eth_switch_learn_ingress_port_id,
					&ingress_port),
				"Failed to ingress port from digest");

		/* Update src table */
		uint8_t mask[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
		check_bf_status(table_add_or_mod(*switching_table_src,
					*session, dev_tgt,
					*table_create_key<const uint8_t*, uint64_t>(
						switching_table_src,
						table_field_desc_t<const uint8_t*>::create_ternary(
							"hdr.ethernet.src_addr", reinterpret_cast<uint8_t*>(&src_mac),
							mask, sizeof(src_mac)),
						table_field_desc_t<uint64_t>::create_ternary(
							"meta.ingress_port", ingress_port, 0x1ff)
					),
					*table_create_data_action<uint64_t>(
						switching_table_src,
						"NoAction",
						{"$ENTRY_TTL", 5000}
					)
				),
				"Failed to add entry to switching_table_src");

		/* Update switching table */
		update_switching_table(src_mac, "Ingress.send", ingress_port);
	}

	check_bf_status(
			eth_switch_learn->bfRtLearnNotifyAck(session, learn_msg_hdl),
			"Failed to acknowledge learn data");

	return BF_SUCCESS;
}

void ASICDriver::eth_switch_idle_cb(
		const bf_rt_target_t& bf_rt_tgt, const BfRtTableKey* key, void* cookie)
{
	unique_lock lk_st(m_switching_table);

	MacAddr src_addr;

	bf_rt_id_t field_src_addr;
	check_bf_status(
			switching_table_src->keyFieldIdGet(
				"hdr.ethernet.src_addr", &field_src_addr),
			"Failed to resolve field id");

	uint8_t mask[sizeof(src_addr)];
	check_bf_status(
			key->getValueandMask(
				field_src_addr,
				sizeof(src_addr),
				reinterpret_cast<uint8_t*>(&src_addr), mask),
			"Failed to get src addr");


	/* Remove entry from src table */
	check_bf_status(
			switching_table_src->tableEntryDel(*session, dev_tgt, *key),
			"Failed to delete table entry");


	/* Remove entry from switching table */
	check_bf_status(switching_table->tableEntryDel(
				*session, dev_tgt,
				*table_create_key<const uint8_t*>(
					switching_table,
					{"hdr.ethernet.dst_addr", reinterpret_cast<uint8_t*>(&src_addr), sizeof(src_addr)}
				)
			),
			"Failed to delete table entry");
}


void ASICDriver::update_switching_table(
		const MacAddr& addr, const char* action, uint64_t port)
{
	check_bf_status(table_add_or_mod(*switching_table,
				*session, dev_tgt,
				*table_create_key<const uint8_t*>(
					switching_table,
					{"hdr.ethernet.dst_addr", reinterpret_cast<const uint8_t*>(&addr), sizeof(addr)}
				),
				*table_create_data_action<uint64_t>(
					switching_table,
					action,
					{"port", port}
				)
			),
			"Failed to update entry in switching_table");
}

void ASICDriver::update_switching_table(
		const MacAddr& addr, const char* action)
{
	check_bf_status(table_add_or_mod(*switching_table,
				*session, dev_tgt,
				*table_create_key<const uint8_t*>(
					switching_table,
					{"hdr.ethernet.dst_addr", reinterpret_cast<const uint8_t*>(&addr), sizeof(addr)}
				),
				*table_create_data_action<>(
					switching_table,
					action
				)
			),
			"Failed to update entry in switching_table");
}


void ASICDriver::on_st_repo_channels()
{
	/* Get channels from st_repo */
	auto repo_channels = st_repo.get_channels();

	/* Get channels from ASIC */
	/* TODO */

	/* Add missing channels and client portals on ASIC */
	for (auto& rc : repo_channels)
	{
		uint16_t mcast_grp = 0x10 + rc->agg_unit / 8;

		uint32_t full_bitmap_low = 0;
		uint32_t full_bitmap_high = 0;

		/* Construct full bitmap */
		if (rc->participants.size() > 64)
			throw runtime_error("Too participants in collectives channel.");

		full_bitmap_low = (1ULL << min(rc->participants.size(), 32UL)) - 1;
		if (rc->participants.size() > 32)
			full_bitmap_high = (1ULL << (rc->participants.size() - 32)) - 1;


		/* Create/update multicast group */
		vector<bf_rt_id_t> port_ids;
		vector<tuple<bf_rt_id_t, bf_rt_id_t, bf_rt_id_t>> group_ports;

		size_t pos = 1;
		for (auto& [ipart,part] : rc->participants)
		{
			group_ports.push_back({
					0x1000 + 0x200 * (rc->agg_unit / 8) + (pos-1),
					part.switch_port * 4,
					pos});

			pos++;
		}

		for (auto [pid, p, rid] : group_ports)
		{
			port_ids.push_back(pid);

			check_bf_status(table_add_or_mod(*pre_node, *session, dev_tgt,
						*table_create_key<uint64_t>(pre_node, {"$MULTICAST_NODE_ID", pid}),
						*table_create_data<uint64_t, vector<bf_rt_id_t>, vector<bf_rt_id_t>>(
							pre_node,
							{"$MULTICAST_RID", rid},
							{"$MULTICAST_LAG_ID", vector<bf_rt_id_t>()},
							{"$DEV_PORT", vector<bf_rt_id_t>({p})}
						)
					),
					"Add entry to table pre.node");
		}

		check_bf_status(table_add_or_mod(*pre_mgid, *session, dev_tgt,
					*table_create_key<uint64_t>(pre_mgid, {"$MGID", mcast_grp}),
					*table_create_data<vector<bf_rt_id_t>, vector<bool>, vector<bf_rt_id_t>>(
						pre_mgid,
						{"$MULTICAST_NODE_ID", port_ids},
						{"$MULTICAST_NODE_L1_XID_VALID", vector<bool>(port_ids.size(), false)},
						{"$MULTICAST_NODE_L1_XID", vector<bf_rt_id_t>(port_ids.size(), 0)}
					)
				),
				"Add entry to table pre.mgid");


		/* Construct worker bitmap */
		uint32_t node_bitmap_low = 1;
		uint32_t node_bitmap_high = 0;
		
		/* Insert participants */
		pos = 1;
		for (auto& [ipart,part] : rc->participants)
		{
			if (part.ip.is_0000())
				continue;
			
			/* Unit selector */
			check_bf_status(table_add_or_mod(
				*collectives_unit_selector, *session, dev_tgt,
				*table_create_key<const uint8_t*, const uint8_t*, uint64_t, uint64_t>(
					collectives_unit_selector,
					{"hdr.ipv4.src_addr",
					 reinterpret_cast<const uint8_t*>(&part.ip),
					 sizeof(part.ip)},
					{"hdr.ipv4.dst_addr",
					 reinterpret_cast<const uint8_t*>(&rc->fabric_ip),
					 sizeof(rc->fabric_ip)},
					{"hdr.udp.src_port", part.port},
					{"hdr.udp.dst_port", rc->fabric_port}),
				*table_create_data_action<uint64_t, uint64_t, uint64_t, uint64_t>(
					collectives_unit_selector, "Ingress.collectives.select_agg_unit",
					{"mcast_grp", mcast_grp},
					{"agg_unit", rc->agg_unit},
					{"node_bitmap_low", node_bitmap_low},
					{"node_bitmap_high", node_bitmap_high})),
			"Failed to update Collectives.unit_selector table");

			/* Update worker bitmap */
			if (node_bitmap_low == (1ULL << 31))
				node_bitmap_high = 1;
			else
				node_bitmap_high <<= 1;

			node_bitmap_low <<= 1;


			/* Output address update */
			check_bf_status(table_add_or_mod(
				*collectives_output_address, *session, dev_tgt,
				*table_create_key<uint64_t, uint64_t>(
					collectives_output_address,
					{"meta.bridge_header.agg_unit", rc->agg_unit},
					{"eg_intr_md.egress_rid", pos}),
				*table_create_data_action<const uint8_t*, const uint8_t*,
						const uint8_t*, const uint8_t*, uint64_t, uint64_t>(
					collectives_output_address,
					"Egress.collectives_distributor.output_address_set",
					{"src_mac",
					 reinterpret_cast<const uint8_t*>(&rc->fabric_mac),
					 sizeof(rc->fabric_mac)},
					{"dst_mac",
					 reinterpret_cast<const uint8_t*>(&part.mac),
					 sizeof(part.mac)},
					{"src_ip",
					 reinterpret_cast<const uint8_t*>(&rc->fabric_ip),
					 sizeof(rc->fabric_ip)},
					{"dst_ip",
					 reinterpret_cast<const uint8_t*>(&part.ip),
					 sizeof(part.ip)},
					{"src_port", rc->fabric_port},
					{"dst_port", part.port})),
			"Failed to update CollectivesDistributor.output_address table");

			pos++;
		}

		/* Insert or update check_complete/full map */
		check_bf_status(table_add_or_mod(
			*collectives_check_complete, *session, dev_tgt,
			*table_create_key<uint64_t, uint64_t, uint64_t, bool>(
				collectives_check_complete,
				{"meta.agg_unit", rc->agg_unit},
				table_field_desc_t<uint64_t>::create_ternary(
					"meta.node_bitmap.low", full_bitmap_low, 0xffffffff),
				table_field_desc_t<uint64_t>::create_ternary(
					"meta.node_bitmap.high", full_bitmap_high, 0xffffffff),
				{"meta.agg_is_clear", false}),
			*table_create_data_action<>(
				collectives_check_complete,
				"Ingress.collectives.check_complete_true")),
		"Failed to update Collectives.check_complete table");

		check_bf_status(table_add_or_mod(
			*collectives_check_complete, *session, dev_tgt,
			*table_create_key<uint64_t, uint64_t, uint64_t, bool>(
				collectives_check_complete,
				{"meta.agg_unit", rc->agg_unit},
				table_field_desc_t<uint64_t>::create_ternary(
					"meta.node_bitmap.low", 0, 0),
				table_field_desc_t<uint64_t>::create_ternary(
					"meta.node_bitmap.high", 0, 0),
				{"meta.agg_is_clear", true}),
			*table_create_data_action<>(
				collectives_check_complete,
				"Ingress.collectives.check_complete_clear")),
		"Failed to update Collectives.check_complete table");

		/* Insert or update choose_action */
		for (int i = 0; i < 32; i++)
		{
			char buf[3];
			snprintf(buf, sizeof(buf), "%02d", i);
			buf[2] = '\0';
			string istr(buf);

			check_bf_status(table_add_or_mod(
				*collectives_choose_action[i], *session, dev_tgt,
				*table_create_key<uint64_t, bool>(
					collectives_choose_action[i],
					{"meta.agg_unit", rc->agg_unit},
					{"meta.agg_is_clear", false}),
				*table_create_data_action<>(
					collectives_choose_action[i],
					("Ingress.collectives.agg" + istr + ".agg_int_32_add").c_str())),
			"Failed to update Collectives.AggregationUnit.choose_action table");

			check_bf_status(table_add_or_mod(
				*collectives_choose_action[i], *session, dev_tgt,
				*table_create_key<uint64_t, bool>(
					collectives_choose_action[i],
					{"meta.agg_unit", rc->agg_unit},
					{"meta.agg_is_clear", true}),
				*table_create_data_action<>(
					collectives_choose_action[i],
					("Ingress.collectives.agg" + istr + ".agg_int_32_clear").c_str())),
			"Failed to update Collectives.AggregationUnit.choose_action table");
		}
	}

	/* Delete excess channels from ASIC */
	/* TODO */
}
