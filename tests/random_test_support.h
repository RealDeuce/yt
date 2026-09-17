#ifndef RANDOM_TEST_SUPPORT_H
#define RANDOM_TEST_SUPPORT_H

#include "yt_random.h"

#include <stdlib.h>

#define YT_TEST_RANDOM_BINDINGS 256U

struct yt_test_random_binding {
	yt_random_fill_fn fill;
	void *context;
	uint64_t draws;
};

static struct yt_test_random_binding
    yt_test_random_bindings[YT_TEST_RANDOM_BINDINGS];
static size_t yt_test_random_binding_count;

static inline bool
yt_test_random_fill(void *context, void *buffer, size_t length,
    struct yt_error *error)
{
	struct yt_test_random_binding *binding = context;

	if (!binding->fill(binding->context, buffer, length, error))
		return false;
	++binding->draws;
	return true;
}

static inline void
yt_test_random_use_provider(struct yt_random *random, yt_random_fill_fn fill,
    void *context)
{
	struct yt_test_random_binding *binding;

	if (random->fill == yt_test_random_fill) {
		binding = random->context;
	} else {
		if (yt_test_random_binding_count >= YT_TEST_RANDOM_BINDINGS)
			abort();
		binding = &yt_test_random_bindings[yt_test_random_binding_count++];
	}
	binding->fill = fill;
	binding->context = context;
	binding->draws = 0;
	random->fill = yt_test_random_fill;
	random->context = binding;
}

static inline uint64_t
yt_test_random_draws(const struct yt_random *random)
{
	const struct yt_test_random_binding *binding;

	if (random->fill != yt_test_random_fill)
		return 0U;
	binding = random->context;
	return binding->draws;
}

#define TEST_DRAWS(random) yt_test_random_draws(&(random))

#endif
