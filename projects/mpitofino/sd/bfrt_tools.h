#ifndef __BFRT_TOOLS_H
#define __BFRT_TOOLS_H

#include <string>
#include <utility>
#include <bf_rt/bf_rt.hpp>
#include "common/utils.h"

namespace bfrt
{

inline void check_bf_status(bf_status_t s, const std::string& msg)
{
	if (s != BF_SUCCESS)
		throw std::runtime_error(msg + ": " + bf_err_str(s) +
							" (" + to_hex_string(s) + ")");
}

template<typename T>
struct table_field_desc_t
{
	const char* name;
	const T& value;
	const size_t size = 0;

	const T* mask = nullptr;

	table_field_desc_t(const char* name, const T& value)
		: name(name), value(value)
	{}

	table_field_desc_t(const char* name, const T& value, size_t size)
		: name(name), value(value), size(size)
	{}


	static table_field_desc_t create_ternary(
		const char* name, const T& value, const T& mask)
	{
		table_field_desc_t d(name, value);

		d.mask = &mask;

		return d;
	}

	static table_field_desc_t create_ternary(
		const char* name, const T& value, const T& mask, size_t size)
	{
		table_field_desc_t d(name, value, size);

		d.mask = &mask;

		return d;
	}
};

template<typename T>
void table_key_set_value(
		BfRtTableKey& key, bf_rt_id_t field_id,
		const table_field_desc_t<T>& arg)
{
	if (arg.mask)
	{
		check_bf_status(
			key.setValueandMask(field_id, arg.value, *arg.mask),
				"Failed to set table key ternary field value");
	}
	else
	{
		check_bf_status(
				key.setValue(field_id, arg.value),
				"Failed to set table key field value");
	}
}

template<>
void table_key_set_value<const uint8_t*>(
		BfRtTableKey& key, bf_rt_id_t field_id,
		const table_field_desc_t<const uint8_t*>& arg)
{
	if (arg.mask)
	{
		check_bf_status(
				key.setValueandMask(field_id, arg.value, *arg.mask, arg.size),
				"Failed to set table key field value");
	}
	else
	{
		check_bf_status(
				key.setValue(field_id, arg.value, arg.size),
				"Failed to set table key field value");
	}
}

template<typename... Ts>
std::unique_ptr<BfRtTableKey> table_create_key(
		const BfRtTable* table, const table_field_desc_t<Ts>&... args)
{
	std::unique_ptr<BfRtTableKey> key;
	check_bf_status(table->keyAllocate(&key), "Failed to allocate key for table");

	(...,[&]
	{
		bf_rt_id_t field_id;
		check_bf_status(
				table->keyFieldIdGet(args.name, &field_id),
				"Invalid table key field id");

		table_key_set_value(*key, field_id, args);
	}());

	return key;
}

template<typename T>
void table_data_set_value(
		BfRtTableData& data, bf_rt_id_t field_id,
		const table_field_desc_t<T>& arg)
{
	check_bf_status(
			data.setValue(field_id, arg.value),
			"Failed to set table data field value");
}

template<>
void table_data_set_value<const uint8_t*>(
		BfRtTableData& data, bf_rt_id_t field_id,
		const table_field_desc_t<const uint8_t*>& arg)
{
	check_bf_status(
			data.setValue(field_id, arg.value, arg.size),
			"Failed to set table data field value");
}

template<typename... Ts>
std::unique_ptr<BfRtTableData> table_create_data(
		const BfRtTable* table, const table_field_desc_t<Ts>&... args)
{
	std::unique_ptr<BfRtTableData> data;
	check_bf_status(table->dataAllocate(&data), "Failed to allocate data for table");

	(...,[&]
	{
		bf_rt_id_t field_id;
		check_bf_status(
				table->dataFieldIdGet(args.name, &field_id),
				"Invalid table data field id");

		table_data_set_value(*data, field_id, args);
	}());

	return data;
}

template<typename... Ts>
std::unique_ptr<BfRtTableData> table_create_data_action(
		const BfRtTable* table, const char* action, const table_field_desc_t<Ts>&... args)
{
	bf_rt_id_t action_id;
	check_bf_status(
			table->actionIdGet(action, &action_id),
			"Failed to resolve action name");

	std::unique_ptr<BfRtTableData> data;
	check_bf_status(
			table->dataAllocate(action_id, &data),
			"Failed to allocate data for table");

	(...,[&]
	{
		bf_rt_id_t field_id;
		check_bf_status(
				table->dataFieldIdGet(args.name, action_id, &field_id),
				"Invalid table data field id");

		table_data_set_value(*data, field_id, args);
	}());

	return data;
}


inline void table_set_asym(
	const BfRtTable& table,
	const bfrt::BfRtSession& session,
	const bf_rt_target_t& dev_tgt,
	bool is_async)
{
	std::unique_ptr<BfRtTableAttributes> ta;
	check_bf_status(
		table.attributeAllocate(TableAttributesType::ENTRY_SCOPE, &ta),
		"table.attributeAllocate");

	std::unique_ptr<BfRtTableEntryScopeArguments> scope_args;
	check_bf_status(
		ta->entryScopeArgumentsAllocate(&scope_args),
		"ta->entryScopeArgumentsAllocate");

	check_bf_status(
		ta->entryScopeParamsSet(
			is_async ? TableEntryScope::ENTRY_SCOPE_SINGLE_PIPELINE
				: TableEntryScope::ENTRY_SCOPE_ALL_PIPELINES,
			*scope_args),
		"entryScopeParamsSet");

	check_bf_status(
		table.tableAttributesSet(session, dev_tgt, *ta),
		"tableAttributesSet");
}


/* Helper for older SDE versions */
inline bf_status_t table_add_or_mod(
		const BfRtTable& table,
		const bfrt::BfRtSession& session,
		const bf_rt_target_t& dev_tgt,
		const BfRtTableKey& key,
		const BfRtTableData& data)
{
	auto ret = table.tableEntryMod(session, dev_tgt, 0, key, data);
	if (ret == BF_SUCCESS)
		return ret;

	/* Object not found is ok, in this case we simply need to add it */
	if (ret != BF_OBJECT_NOT_FOUND)
		return ret;

	return table.tableEntryAdd(session, dev_tgt, key, data);
}


class table_iterator
{
protected:
	const BfRtTable& table;
	const BfRtSession& session;
	const bf_rt_target_t& dev_tgt;

	uint32_t num_entries = 0;
	uint32_t pos = 0;

	std::unique_ptr<BfRtTableKey> cur_key;
	std::unique_ptr<BfRtTableData> cur_data;

	inline void reset()
	{
		cur_key = nullptr;
		cur_data = nullptr;
	}
	
	inline void allocate()
	{
		reset();
		
		check_bf_status(table.keyAllocate(&cur_key), "keyAllocate");
		check_bf_status(table.dataAllocate(&cur_data), "dataAllocate");
	}

public:
	inline table_iterator(
		const BfRtTable& table,
		const BfRtSession& session,
		const bf_rt_target_t& dev_tgt)
		: table(table), session(session), dev_tgt(dev_tgt)
	{
		uint64_t flags;
		BF_RT_FLAG_INIT(flags);
		BF_RT_FLAG_SET(flags, BF_RT_FROM_HW);

		check_bf_status(
			table.tableUsageGet(
				session,
				dev_tgt,
				flags,
				&num_entries),
			"tableUsageGet");

		if (num_entries > 0)
		{
			allocate();

			BF_RT_FLAG_INIT(flags);
			BF_RT_FLAG_SET(flags, BF_RT_FROM_HW);

			check_bf_status(
				table.tableEntryGetFirst(
					session,
					dev_tgt,
					flags,
					cur_key.get(),
					cur_data.get()),
				"tableEntryGetFirst");
		}
	}

	inline void operator++()
	{
		std::unique_ptr<BfRtTableKey> prev_key = move(cur_key);
		reset();

		if (++pos < num_entries)
		{
			allocate();

			uint64_t flags;
			BF_RT_FLAG_INIT(flags);
			BF_RT_FLAG_SET(flags, BF_RT_FROM_HW);

			uint32_t n = 1, n_ret = 0;

			BfRtTable::keyDataPairs kd_pairs{{cur_key.get(), cur_data.get()}};

			check_bf_status(
				table.tableEntryGetNext_n(
					session,
					dev_tgt,
					flags,
					*prev_key,
					n,
					&kd_pairs,
					&n_ret),
				"tableEntryGetNext_n");

			if (n_ret != 1)
				throw std::runtime_error("failed to retrieve next table entry");
		}
	}

	inline operator bool() const
	{
		return cur_key != nullptr;
	}

	inline std::pair<const BfRtTableKey*, const BfRtTableData*> operator*() const
	{
		return {cur_key.get(), cur_data.get()};
	}
};

};

#endif /* __BFRT_TOOLS_H */
