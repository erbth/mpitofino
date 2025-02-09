#ifndef __STATE_REPOSITORY_H
#define __STATE_REPOSITORY_H

#include <map>
#include "simple_types.h"

/* Design:
 *
 * Currently, all datasets are implemented directly with dataset-specific
 * methods for accessing them. This does not scale as the number of datasets
 * grows, but we'll cross that bridge as we get there. */

class StateRepository final
{
public:
	using switching_table_t = std::map<MacAddr, uint16_t>;

protected:
	switching_table_t switching_table;

public:
};

#endif /* __STATE_REPOSITORY_H */
