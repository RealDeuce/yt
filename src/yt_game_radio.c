#include "yt_game.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
yt_xannor_victory_winner(const uint8_t *player, size_t player_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Congratulations go to ";
	static const uint8_t suffix[] = " who defeated the Xannor HQ!!!";
	size_t needed = sizeof(prefix) - 1U + player_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (player == NULL && player_length != 0U))
		return false;
	*length = 0U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (player_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, player, player_length);
	memcpy(row + sizeof(prefix) - 1U + player_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_fixed_text_contains(const uint8_t field[YT_TEXT_FIELD_SIZE],
    const uint8_t *needle, size_t needle_length)
{
	size_t offset;

	if (field == NULL || (needle == NULL && needle_length != 0U))
		return false;
	if (needle_length == 0U)
		return true;
	if (needle_length > YT_TEXT_FIELD_SIZE)
		return false;
	for (offset = 0; offset + needle_length <= YT_TEXT_FIELD_SIZE;
	    ++offset) {
		if (memcmp(field + offset, needle, needle_length) == 0)
			return true;
	}
	return false;
}

bool
yt_radio_player_prompt(const struct yt_player *player, uint8_t *prompt,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t suffix[] = " [Y]? ";
	size_t stored;

	if (length != NULL)
		*length = 0;
	if (player == NULL || prompt == NULL || length == NULL)
		return false;
	stored = yt_player_stored_name(player, prompt);
	if (stored + sizeof(suffix) - 1U > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "radio player prompt capacity");
		}
		return false;
	}
	memcpy(prompt + stored, suffix, sizeof(suffix) - 1U);
	*length = stored + sizeof(suffix) - 1U;
	return true;
}

bool
yt_radio_tuning_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Tuning in to ";
	static const uint8_t suffix[] = "'s frequency.";
	uint8_t stored[YT_TEXT_FIELD_SIZE];
	size_t stored_length;
	size_t needed;

	if (length != NULL)
		*length = 0U;
	if (player == NULL || row == NULL || length == NULL)
		return false;
	stored_length = yt_player_stored_name(player, stored);
	needed = sizeof(prefix) - 1U + stored_length + sizeof(suffix) - 1U;
	if (needed > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "radio tuning row capacity");
		}
		return false;
	}
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (stored_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, stored, stored_length);
	memcpy(row + sizeof(prefix) - 1U + stored_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}
