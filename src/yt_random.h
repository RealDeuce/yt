#ifndef YT_RANDOM_H
#define YT_RANDOM_H

#include "yt_common.h"

typedef bool (*yt_random_fill_fn)(void *context, void *buffer, size_t length,
    struct yt_error *error);

struct yt_random {
	yt_random_fill_fn fill;
	void *context;
	float last;
	bool has_last;
	uint64_t draws;
};

void yt_random_init(struct yt_random *random);
void yt_random_set_provider(struct yt_random *random, yt_random_fill_fn fill,
    void *context);
bool yt_random_next(struct yt_random *random, float *value,
    struct yt_error *error);
bool yt_random_one_based_single(struct yt_random *random, float range,
    float *value, struct yt_error *error);
bool yt_random_integer(struct yt_random *random, int range, int *value,
    struct yt_error *error);
bool yt_random_nested_single(struct yt_random *random, float count,
    float *range, float *value, struct yt_error *error);
bool yt_random_nested_integer(struct yt_random *random, int count, int range,
    int *value, struct yt_error *error);
bool yt_random_market_bases(struct yt_random *random, float bases[3],
    struct yt_error *error);

#endif
