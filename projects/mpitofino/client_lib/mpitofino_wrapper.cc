#include <new>
#include "mpitofino.h"
#include "common/visibility.h"

using namespace std;
using namespace mpitofino;


extern "C" {
#include "include/mpitofino.h"

	
struct _mpitofino_ctx_t
{
	Client* client;
};

struct _mpitofino_aggregation_group_t
{
	AggregationGroup* agg_group;
};


SHLIB_EXPORTED mpitofino_ctx_t* mpitofino_create_ctx(mpitofino_client_id_t client_id)
{
	try
	{
		auto ctx = new mpitofino_ctx_t();

		try
		{
			ctx->client = new Client(client_id);
			return ctx;
		}
		catch (...)
		{
			delete ctx;
			throw;
		}
	}
	catch (...)
	{
		return nullptr;
	}
}

SHLIB_EXPORTED void mpitofino_destroy_ctx(mpitofino_ctx_t* ctx)
{
	if (ctx)
	{
		if (ctx->client)
			delete ctx->client;

		delete ctx;
	}
}


SHLIB_EXPORTED mpitofino_aggregation_group_t* mpitofino_create_aggregation_group(
	mpitofino_ctx_t* ctx,
	mpitofino_client_id_t* client_ids,
	size_t cnt_client_ids)
{
	try
	{
		vector<uint64_t> _client_ids;
		_client_ids.reserve(cnt_client_ids);
		
		for (size_t i = 0; i < cnt_client_ids; i++)
			_client_ids.push_back(client_ids[i]);

		auto agg_group = new mpitofino_aggregation_group_t();

		try
		{
			agg_group->agg_group = new AggregationGroup(*ctx->client, _client_ids);
			return agg_group;
		}
		catch (...)
		{
			delete agg_group;
			throw;
		}
	}
	catch (...)
	{
		return nullptr;
	}
}

SHLIB_EXPORTED void mpitofino_destroy_aggregation_group(
	mpitofino_aggregation_group_t* aggregation_group)
{
	if (aggregation_group)
	{
		if (aggregation_group->agg_group)
			delete aggregation_group->agg_group;

		delete aggregation_group;
	}
}


SHLIB_EXPORTED int mpitofino_allreduce(
	mpitofino_aggregation_group_t* aggregation_group,
	const void* sbuf,
	void* dbuf,
	size_t count,
	mpitofino_datatype_t dtype,
	mpitofino_tag_t tag)
{
	try
	{
		aggregation_group->agg_group->allreduce(
			sbuf, dbuf, count, (datatype_t) dtype, tag);
		return 0;
	}
	catch (...)
	{
		return -1;
	}
}


}
