#include "yt_game_internal.h"

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
