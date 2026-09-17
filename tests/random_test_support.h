#ifndef RANDOM_TEST_SUPPORT_H
#define RANDOM_TEST_SUPPORT_H

#include "yt_random.h"

static inline void
yt_test_random_use_provider(struct yt_random *random, yt_random_fill_fn fill,
    void *context)
{
	random->fill = fill;
	random->context = context;
	random->draws = 0;
}

#endif
