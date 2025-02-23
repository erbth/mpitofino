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
}

void ASICDriver::initial_setup()
{
	unique_lock lk_st(m_switching_table);

	/* Create an all-ports broadcast group */
	/* PRE uses port-stride per pipe of 128; 16 100G ports per pipe */
	vector<bf_rt_id_t> port_ids;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 16; j++)
			port_ids.push_back(i*128 + j*4);
	}

	check_bf_status(pre_node->tableEntryAdd(*session, dev_tgt,
				*table_create_key<uint64_t>(pre_node, {"$MULTICAST_NODE_ID", 1}),
				*table_create_data<uint64_t, vector<bf_rt_id_t>, vector<bf_rt_id_t>>(
					pre_node,
					{"$MULTICAST_RID", 1},
					{"$MULTICAST_LAG_ID", vector<bf_rt_id_t>()},
					{"$DEV_PORT", port_ids}
				)
			),
			"Add entry to table pre.node");

	check_bf_status(pre_mgid->tableEntryAdd(*session, dev_tgt,
				*table_create_key<uint64_t>(pre_mgid, {"$MGID", 1}),
				*table_create_data<vector<bf_rt_id_t>, vector<bool>, vector<bf_rt_id_t>>(
					pre_mgid,
					{"$MULTICAST_NODE_ID", vector<bf_rt_id_t>({1})},
					{"$MULTICAST_NODE_L1_XID_VALID", vector<bool>({false})},
					{"$MULTICAST_NODE_L1_XID", vector<bf_rt_id_t>({0})}
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
		bool is_added{};
		check_bf_status(switching_table_src->tableEntryAddOrMod(
					*session, dev_tgt, 0,
					*table_create_key<uint8_t*>(
						switching_table_src,
						{"hdr.ethernet.src_addr", reinterpret_cast<uint8_t*>(&src_mac), sizeof(src_mac)}
					),
					*table_create_data_action<uint64_t, uint64_t>(
						switching_table_src,
						"stsrc_known",
						{"port", ingress_port},
						{"$ENTRY_TTL", 5000}
					),
					&is_added
				),
				"Failed to add entry to switching_table_src");

		/* Update switching table */
		check_bf_status(switching_table->tableEntryAddOrMod(
					*session, dev_tgt, 0,
					*table_create_key<uint8_t*>(
						switching_table,
						{"hdr.ethernet.dst_addr", reinterpret_cast<uint8_t*>(&src_mac), sizeof(src_mac)}
					),
					*table_create_data_action<uint64_t>(
						switching_table,
						"send",
						{"port", ingress_port}
					),
					&is_added
				),
				"Failed to add entry to switching_table");
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

	check_bf_status(
			key->getValue(
				field_src_addr,
				sizeof(src_addr), reinterpret_cast<uint8_t*>(&src_addr)),
			"Failed to get src addr");


	/* Remove entry from src table */
	check_bf_status(
			switching_table_src->tableEntryDel(*session, dev_tgt, *key),
			"Failed to delete table entry");


	/* Remove entry from switching table */
	check_bf_status(switching_table->tableEntryDel(
				*session, dev_tgt,
				*table_create_key<uint8_t*>(
					switching_table,
					{"hdr.ethernet.dst_addr", reinterpret_cast<uint8_t*>(&src_addr), sizeof(src_addr)}
				)
			),
			"Failed to delete table entry");
}
