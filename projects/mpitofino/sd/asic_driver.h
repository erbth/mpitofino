#ifndef __ASIC_DRIVER_H
#define __ASIC_DRIVER_H

#include <mutex>
#include <bf_rt/bf_rt.hpp>
#include "state_repository.h"

extern "C" {
#include <bf_switchd/bf_switchd.h>
}

/* NOTE: This class is not cleanly destructible, and there must be only one
 * instance of it per program. This does also mean that its constructor may only
 * be called once during a program's lifetime, even if it fails - so no retrying
 * of initialization. */
class ASICDriver final
{
protected:
	StateRepository& st_repo;

	/* bf_switchd config; strings must live as long as the context */
	std::string sde_install;
	std::string program_conf_file;
	bf_switchd_context_t switchd_ctx{};

	bf_rt_target_t dev_tgt = {
		.dev_id = 0,
		.pipe_id = 0xffff  // All pipes
	};

	const bfrt::BfRtInfo* bfrt_info = nullptr;
	std::shared_ptr<bfrt::BfRtSession> session;

	const bfrt::BfRtTable* pre_mgid = nullptr;
	const bfrt::BfRtTable* pre_node = nullptr;

	const bfrt::BfRtLearn* eth_switch_learn = nullptr;
	bf_rt_id_t eth_switch_learn_src_mac_id{};
	bf_rt_id_t eth_switch_learn_ingress_port_id{};

	std::mutex m_switching_table;

	const bfrt::BfRtTable* switching_table = nullptr;
	const bfrt::BfRtTable* switching_table_src = nullptr;

	bf_status_t eth_switch_learn_cb(
			const bf_rt_target_t bf_rt_tgt,
			const std::shared_ptr<bfrt::BfRtSession> session,
			std::vector<std::unique_ptr<bfrt::BfRtLearnData>> learn_data,
			bf_rt_learn_msg_hdl* const learn_msg_hdl,
			const void* cookie);

	void eth_switch_idle_cb(
			const bf_rt_target_t& bf_rt_tgt,
			const bfrt::BfRtTableKey* key,
			void* cookie);

	/* Initialization */
	void find_tables();
	void initial_setup();

public:
	ASICDriver(StateRepository& st_repo);
};

#endif /* __ASIC_DRIVER_H */
