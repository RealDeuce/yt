#include "yt_game_internal.h"

#include "qb.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool
yt_game_error(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

bool
yt_game_join_parts(const uint8_t *first, size_t first_length,
    const uint8_t *second, size_t second_length,
    const uint8_t *third, size_t third_length,
    const uint8_t *fourth, size_t fourth_length,
    const uint8_t *fifth, size_t fifth_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	const uint8_t *parts[5] = {first, second, third, fourth, fifth};
	const size_t sizes[5] = {first_length, second_length, third_length,
	    fourth_length, fifth_length};
	size_t needed = 0U;
	size_t cursor = 0U;
	size_t index;

	if (length == NULL)
		return false;
	*length = 0U;
	for (index = 0U; index < 5U; ++index) {
		if (parts[index] == NULL && sizes[index] != 0U)
			return false;
		if (SIZE_MAX - needed < sizes[index])
			return false;
		needed += sizes[index];
	}
	if (needed > capacity || (row == NULL && needed != 0U))
		return false;
	for (index = 0U; index < 5U; ++index) {
		if (sizes[index] != 0U) {
			memcpy(row + cursor, parts[index], sizes[index]);
			cursor += sizes[index];
		}
	}
	*length = needed;
	return true;
}

bool
yt_game_row_append(struct yt_game_row_builder *builder, const void *data,
    size_t length)
{
	if (length > builder->capacity - builder->length
	    || (length != 0U && (builder->row == NULL || data == NULL)))
		return false;
	if (length != 0U)
		memcpy(builder->row + builder->length, data, length);
	builder->length += length;
	return true;
}

bool
yt_game_row_number(struct yt_game_row_builder *builder, float value,
    bool promoted)
{
	char number[64];
	int length = promoted
	    ? qb_str_double(number, sizeof(number), (double)value)
	    : qb_str_single(number, sizeof(number), value);

	return length >= 0
	    && yt_game_row_append(builder, number, (size_t)length);
}
