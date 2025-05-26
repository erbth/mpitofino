#ifndef __UTILS_H
#define __UTILS_H


inline int32_t gen_data_int32(size_t seed, size_t i)
{
	return seed * 0x100000 + i;
}


inline double time_diff(const struct timespec* a, const struct timespec* b)
{
	return (a->tv_sec - b->tv_sec) + (a->tv_nsec - b->tv_nsec)*1e-9;
}

#endif /* __UTILS_H */
