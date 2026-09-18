#include "yt_random.h"
#include "qb.h"
#include "yt_platform.h"

#include <math.h>
#include <string.h>

void
yt_random_init(struct yt_random *random)
{
	memset(random, 0, sizeof(*random));
}

bool
yt_random_next(struct yt_random *random, float *value, struct yt_error *error)
{
	uint8_t bytes[3];
	uint32_t sample;

	if (random->fill != NULL
	    ? !random->fill(random->context, bytes, sizeof(bytes), error)
	    : !yt_platform_entropy(bytes, sizeof(bytes), error))
		return false;
	sample = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
	    | ((uint32_t)bytes[2] << 16);
	*value = (float)sample / 16777216.0f;
	return true;
}

bool
yt_random_one_based_single(struct yt_random *random, float range,
    float *value, struct yt_error *error)
{
	float selection;
	if (!yt_random_next(random, &selection, error))
		return false;
	*value = floorf(selection * range) + 1.0f;
	return true;
}

bool
yt_random_integer(struct yt_random *random, int range, uint16_t *value,
    struct yt_error *error)
{
	float selection;
	uint16_t integral;

	if (range < 1 || range > UINT16_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "random integer range");
		}
		return false;
	}
	if (!yt_random_next(random, &selection, error))
		return false;
	integral = (uint16_t)floorf(selection * (float)range);
	*value = integral + 1U;
	return true;
}

static bool
nested_single(struct yt_random *random, float count, float *range,
    float *value, bool *produced, struct yt_error *error)
{
	float index;
	float terminal;

	*produced = false;
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
	    index = qb_single_add(index, 1.0f)) {
		float selection;
		if (!yt_random_next(random, &selection, error))
			return false;
		*value = qb_single_add(floorf(qb_single_multiply(selection,
		    *range)), 1.0f);
		*range = *value;
		*produced = true;
	}
	return true;
}

bool
yt_random_nested_single(struct yt_random *random, float count, float *range,
    float *value, struct yt_error *error)
{
	bool produced;

	return nested_single(random, count, range, value, &produced, error);
}

bool
yt_random_nested_integer(struct yt_random *random, int count, int range,
    uint16_t *value, struct yt_error *error)
{
	uint16_t current;
	int index;

	if (value == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "nested random result");
		}
		return false;
	}
	if (count == 0 || range == 0)
		return true;
	if (count < 0 || range < 0 || range > UINT16_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "nested random integer range");
		}
		return false;
	}
	current = (uint16_t)range;
	for (index = 0; index < count; ++index) {
		if (!yt_random_integer(random, current, &current, error))
			return false;
		*value = current;
	}
	return true;
}

bool
yt_random_market_bases(struct yt_random *random, float bases[3],
    struct yt_error *error)
{
	static const uint8_t center[3] = {20U, 30U, 40U};
	static const uint8_t span[3] = {5U, 7U, 10U};
	size_t commodity;

	for (commodity = 0; commodity < 3; ++commodity) {
		float first;
		float second;

		if (!yt_random_next(random, &first, error))
			return false;
		if (!yt_random_next(random, &second, error))
			return false;
		bases[commodity] = qb_single_add(
		    qb_single_subtract(center[commodity],
		    qb_single_multiply(first, span[commodity])),
		    qb_single_multiply(second, span[commodity]));
	}
	return true;
}
