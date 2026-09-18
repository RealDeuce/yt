#include "yt_game.h"

#include "qb.h"

#include <string.h>

static bool
salvage_append(uint8_t *row, size_t capacity, size_t *position,
    const void *text, size_t length)
{
	if (*position > capacity || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(row + *position, text, length);
	*position += length;
	return true;
}

bool
yt_salvage_header_row(const uint8_t *salvor, size_t salvor_length,
    const uint8_t *victim, size_t victim_length, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = " *** ";
	static const uint8_t middle[] = " salvaged the following from ";
	static const uint8_t suffix[] = "'s ship:";
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (salvor == NULL && salvor_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	if (!salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U))
		return false;
	if (!salvage_append(row, capacity, &position, salvor, salvor_length))
		return false;
	if (!salvage_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U))
		return false;
	if (!salvage_append(row, capacity, &position, victim, victim_length))
		return false;
	if (!salvage_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_salvage_simple_row(enum yt_salvage_simple_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const char *const labels[] = {
		"Credits:", "Cruise Missiles:", "Plasma Bolts:",
		"Ground Forces:", "Sector Mines:"
	};
	static const uint8_t prefix[] = "  -  ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || kind < YT_SALVAGE_CREDITS
	    || kind > YT_SALVAGE_MINES)
		return false;
	number_length = qb_str_single(number, sizeof(number), amount);
	if (number_length < 0)
		return false;
	if (!salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U))
		return false;
	if (!salvage_append(row, capacity, &position, labels[kind],
	    strlen(labels[kind])))
		return false;
	if (!salvage_append(row, capacity, &position, number,
	    (size_t)number_length))
		return false;
	*length = position;
	return true;
}

bool
yt_salvage_cargo_row(enum yt_salvage_cargo_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const char *const suffix[] = {
		" empty holds", " holds of ore", " holds of organics",
		" holds of equipment"
	};
	static const uint8_t prefix[] = "  - ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || kind < YT_SALVAGE_EMPTY_HOLDS
	    || kind > YT_SALVAGE_EQUIPMENT)
		return false;
	number_length = qb_str_single(number, sizeof(number), amount);
	if (number_length < 0)
		return false;
	if (!salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U))
		return false;
	if (!salvage_append(row, capacity, &position, number,
	    (size_t)number_length))
		return false;
	if (!salvage_append(row, capacity, &position, suffix[kind],
	    strlen(suffix[kind])))
		return false;
	*length = position;
	return true;
}
