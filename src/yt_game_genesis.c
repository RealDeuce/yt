#include "yt_game.h"

#include "qb.h"

#include <string.h>

bool
yt_genesis_confirmation_prompt(const uint8_t *trader, size_t trader_length,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Are you that Trader ";
	static const uint8_t suffix[] = " [y/N]";
	size_t needed;

	if (length == NULL || (trader == NULL && trader_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + trader_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	memcpy(prompt, prefix, sizeof(prefix) - 1U);
	if (trader_length != 0U)
		memcpy(prompt + sizeof(prefix) - 1U, trader, trader_length);
	memcpy(prompt + sizeof(prefix) - 1U + trader_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_genesis_insufficient_rows(float required, uint16_t owned,
    uint8_t *first, size_t first_capacity, size_t *first_length,
    uint8_t *second, size_t second_capacity, size_t *second_length)
{
	static const uint8_t first_prefix[] =
	    "You are not up to the challenge. You must own";
	static const uint8_t first_suffix[] = " ports before you are powerful";
	static const uint8_t second_prefix[] =
	    "enough to initiate Genesis. You are";
	static const uint8_t second_suffix[] =
	    " short of fulfilling the prophesy.";
	volatile float shortfall = required - (float)owned;
	char required_text[64];
	char shortfall_text[64];
	int required_length;
	int shortfall_length;
	size_t needed_first;
	size_t needed_second;

	if (first_length == NULL || second_length == NULL)
		return false;
	*first_length = 0U;
	*second_length = 0U;
	required_length = qb_str_single(required_text, sizeof(required_text),
	    required);
	shortfall_length = qb_str_single(shortfall_text, sizeof(shortfall_text),
	    shortfall);
	if (required_length < 0 || shortfall_length < 0)
		return false;
	needed_first = sizeof(first_prefix) - 1U + (size_t)required_length
	    + sizeof(first_suffix) - 1U;
	needed_second = sizeof(second_prefix) - 1U + (size_t)shortfall_length
	    + sizeof(second_suffix) - 1U;
	if (needed_first > first_capacity || needed_second > second_capacity
	    || (needed_first != 0U && first == NULL)
	    || (needed_second != 0U && second == NULL))
		return false;
	memcpy(first, first_prefix, sizeof(first_prefix) - 1U);
	memcpy(first + sizeof(first_prefix) - 1U, required_text,
	    (size_t)required_length);
	memcpy(first + sizeof(first_prefix) - 1U + (size_t)required_length,
	    first_suffix, sizeof(first_suffix) - 1U);
	memcpy(second, second_prefix, sizeof(second_prefix) - 1U);
	memcpy(second + sizeof(second_prefix) - 1U, shortfall_text,
	    (size_t)shortfall_length);
	memcpy(second + sizeof(second_prefix) - 1U + (size_t)shortfall_length,
	    second_suffix, sizeof(second_suffix) - 1U);
	*first_length = needed_first;
	*second_length = needed_second;
	return true;
}
