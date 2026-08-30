#include "yt_random.h"
#include "yt_platform.h"

#include <math.h>
#include <string.h>

static bool
system_fill(void *context, void *buffer, size_t length, struct yt_error *error)
{
	(void)context;
	return yt_platform_entropy(buffer, length, error);
}

void
yt_random_init(struct yt_random *random)
{
	memset(random, 0, sizeof(*random));
	random->fill = system_fill;
}

void
yt_random_set_provider(struct yt_random *random, yt_random_fill_fn fill,
    void *context)
{
	random->fill = fill != NULL ? fill : system_fill;
	random->context = context;
	random->has_last = false;
	random->draws = 0;
}

bool
yt_random_next(struct yt_random *random, float *value, struct yt_error *error)
{
	uint8_t bytes[3];
	uint32_t sample;

	if (!random->fill(random->context, bytes, sizeof(bytes), error))
		return false;
	sample = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
	    | ((uint32_t)bytes[2] << 16);
	random->last = (float)sample / 16777216.0f;
	random->has_last = true;
	++random->draws;
	*value = random->last;
	return true;
}

static float
random_single_mul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
random_single_sub(float left, float right)
{
	volatile float result = left - right;
	return result;
}

static float
random_single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

bool
yt_random_one_based_single(struct yt_random *random, float range,
    float *value, struct yt_error *error)
{
	float selection;
	volatile float product;
	volatile float integral;
	volatile float result;

	if (!yt_random_next(random, &selection, error))
		return false;
	product = selection * range;
	integral = floorf(product);
	result = integral + 1.0f;
	*value = result;
	return true;
}

bool
yt_random_integer(struct yt_random *random, int range, int *value,
    struct yt_error *error)
{
	float selection;

	if (range < 1) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "random integer range");
		}
		return false;
	}
	if (!yt_random_one_based_single(random, (float)range, &selection,
	    error))
		return false;
	*value = (int)selection;
	return true;
}

bool
yt_random_nested_single(struct yt_random *random, float count, float *range,
    float *value, struct yt_error *error)
{
	float index;
	float terminal;

	if (random == NULL || range == NULL || value == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "nested random arguments");
		}
		return false;
	}
	if (count == 0.0f || *range == 0.0f)
		return true;
	terminal = count;
	for (index = 1.0f; index <= terminal;
	    index = random_single_add(index, 1.0f)) {
		float selection;
		float integral;
		float result;

		if (!yt_random_next(random, &selection, error))
			return false;
		integral = floorf(random_single_mul(selection, *range));
		result = random_single_add(integral, 1.0f);
		*value = result;
		*range = result;
	}
	return true;
}

bool
yt_random_nested_integer(struct yt_random *random, int count, int range,
    int *value, struct yt_error *error)
{
	float current;
	float result;
	uint64_t starting_draws;
	bool succeeded;

	if (value == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "nested random result");
		}
		return false;
	}
	current = (float)range;
	result = 0.0f;
	starting_draws = random != NULL ? random->draws : 0U;
	succeeded = yt_random_nested_single(random, (float)count, &current,
	    &result, error);
	if (random != NULL && random->draws != starting_draws)
		*value = (int)result;
	return succeeded;
}

bool
yt_random_market_bases(struct yt_random *random, float bases[3],
    struct yt_error *error)
{
	static const float center[3] = {20.0f, 30.0f, 40.0f};
	static const float span[3] = {5.0f, 7.0f, 10.0f};
	size_t commodity;

	for (commodity = 0; commodity < 3; ++commodity) {
		float first;
		float second;

		if (!yt_random_next(random, &first, error)
		    || !yt_random_next(random, &second, error))
			return false;
		bases[commodity] = random_single_add(
		    random_single_sub(center[commodity],
		    random_single_mul(first, span[commodity])),
		    random_single_mul(second, span[commodity]));
	}
	return true;
}

float
yt_random_last(const struct yt_random *random)
{
	return random->has_last ? random->last : 0.0f;
}
