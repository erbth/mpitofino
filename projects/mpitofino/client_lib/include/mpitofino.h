#ifndef __MPITOFINO_H__
#define __MPITOFINO_H__

/** Public C api of libmpitofino */

#include <stdint.h>


struct _mpitofino_ctx_t;
struct _mpitofino_aggregation_group_t;

typedef struct _mpitofino_ctx_t mpitofino_ctx_t;
typedef struct _mpitofino_aggregation_group_t mpitofino_aggregation_group_t;

typedef uint64_t mpitofino_client_id_t;
typedef uint64_t mpitofino_tag_t;

enum mpitofino_datatype_t
{
	MPITOFINO_DATATYPE_INT32 = 1
};


mpitofino_ctx_t* mpitofino_create_ctx(mpitofino_client_id_t client_id);
void mpitofino_destroy_ctx(mpitofino_ctx_t*);

/* @param ctx  must exist during the aggregation group's lifetime */
mpitofino_aggregation_group_t* mpitofino_create_aggregation_group(
	mpitofino_ctx_t* ctx,
	mpitofino_client_id_t* client_ids,
	size_t cnt_client_ids);

void mpitofino_destroy_aggregation_group(
	mpitofino_aggregation_group_t* aggregation_group);

/* @returns  0 on success, negative on error */
int mpitofino_allreduce(
	mpitofino_aggregation_group_t* aggregation_group,
	const void* sbuf,
	void* dbuf,
	size_t count,
	mpitofino_datatype_t dtype,
	mpitofino_tag_t tag);

#endif /* __MPITOFINO_H */
