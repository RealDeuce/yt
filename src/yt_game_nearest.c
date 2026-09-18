#include "yt_game.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int
yt_nearest_filter_selector(const uint8_t *response, size_t length)
{
	static const uint8_t alphabet[] = "123ATYEU";
	size_t offset;

	if (length == 0U)
		return 4;
	if (length > sizeof(alphabet) - 1U)
		return 0;
	for (offset = 0U; offset + length <= sizeof(alphabet) - 1U;
	    ++offset) {
		if (memcmp(alphabet + offset, response, length) == 0)
			return (int)offset + 1;
	}
	return 0;
}

bool
yt_nearest_direction_prompt(int selector, uint8_t *prompt,
    size_t capacity, size_t *length)
{
	static const char *const commodities[3] = {
		"Equipment", "Organics", "Ore"
	};
	int written;

	if (selector < 1 || selector > 3 || prompt == NULL || length == NULL)
		return false;
	written = snprintf((char *)prompt, capacity,
	    "Find ports [B] Buying or [S] Selling %s -=> ",
	    commodities[selector - 1]);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

static bool
nearest_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
nearest_single(float value, float *result, struct yt_error *error,
    const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_encode(value, raw);
	if (!isfinite(value) || status == QB_MBF_OVERFLOW
	    || status == QB_MBF_DOMAIN)
		return nearest_error(error, operation);
	*result = qb_mbf32_decode(raw);
	return true;
}

static bool
nearest_add(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left + right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_sub(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left - right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_mul(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left * right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_div(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value;

	if (right == 0.0f)
		return nearest_error(error, operation);
	value = left / right;
	return nearest_single(value, result, error, operation);
}

static void
nearest_market_copy(struct yt_nearest_market *market,
    const struct yt_port *port)
{
	size_t index;

	memset(market, 0, sizeof(*market));
	market->stored_day = port->last_day;
	market->stored_minute = port->last_minute;
	for (index = 0U; index < 3U; ++index) {
		market->stock[index] = port->stock[index];
		market->production[index] = port->production[index];
		market->factor[index] = port->factor[index];
	}
}

bool
yt_nearest_market_project(struct yt_nearest_market *market,
    const struct yt_port *port, const float base_price[3],
    int16_t current_day, float timer_seconds, struct yt_error *error)
{
	float day_delta;
	float minute_delta;
	size_t index;

	if (market == NULL || port == NULL || base_price == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "nearest market arguments");
			error->path[0] = '\0';
		}
		return false;
	}
	nearest_market_copy(market, port);
	if (!nearest_div(timer_seconds, 60.0f, &market->minute, error,
	    "nearest market minute"))
		return false;
	if (!nearest_sub((float)current_day, (float)market->stored_day,
	    &day_delta, error,
	    "nearest market day delta"))
		return false;
	if (!nearest_sub(market->minute, market->stored_minute, &minute_delta,
	    error, "nearest market minute delta"))
		return false;
	if (!nearest_div(minute_delta, 1440.0f, &minute_delta, error,
	    "nearest market minute fraction"))
		return false;
	if (!nearest_add(day_delta, minute_delta, &market->elapsed, error,
	    "nearest market elapsed"))
		return false;
	if (market->elapsed > 10.0f || market->elapsed < 0.0f)
		market->elapsed = 10.0f;

	for (index = 0U; index < 3U; ++index) {
		float growth;
		float candidate;
		float numerator;
		float denominator;
		float ratio;
		float scale;
		float raw;
		float rounded;

		if (!nearest_mul(market->production[index], market->elapsed,
		    &growth, error, "nearest market growth"))
			return false;
		if (!nearest_add(port->stock[index], growth,
		    &market->stock[index], error, "nearest market stock"))
			return false;
		if (!nearest_div(market->stock[index], 10.0f, &candidate,
		    error, "nearest market production comparison"))
			return false;
		if (candidate > market->production[index]) {
			if (!nearest_div(market->stock[index], 10.0f,
			    &market->production[index], error,
			    "nearest market production replacement"))
				return false;
		}
		if (!nearest_mul(market->factor[index], market->stock[index],
		    &numerator, error, "nearest market numerator"))
			return false;
		if (!nearest_mul(market->production[index], 1000.0f,
		    &denominator, error, "nearest market denominator"))
			return false;
		if (!nearest_div(numerator, denominator, &ratio, error,
		    "nearest market ratio"))
			return false;
		if (!nearest_sub(1.0f, ratio, &scale, error,
		    "nearest market scale"))
			return false;
		if (!nearest_mul(base_price[index], scale, &raw, error,
		    "nearest market raw price"))
			return false;
		if (!nearest_add(raw, 0.5f, &rounded, error,
		    "nearest market price rounding"))
			return false;
		rounded = floorf(rounded);
		if (!nearest_single(rounded, &market->price[index], error,
		    "nearest market price INT"))
			return false;
		if (market->price[index] < 1.0f)
			market->price[index] = 1.0f;
	}
	return true;
}
