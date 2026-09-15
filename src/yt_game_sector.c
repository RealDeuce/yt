#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
yt_player_stored_name(const struct yt_player *player,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint_mbf32(
	    player->record.bytes + YT_F85, 0U, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "%s", overflow ? "player name CINT"
			    : "player name LEFT$ length");
		}
		return false;
	}
	stored = (size_t)requested;
	if (stored > YT_TEXT_FIELD_SIZE)
		stored = YT_TEXT_FIELD_SIZE;
	if (stored > 0 && name != NULL)
		memcpy(name, player->record.bytes, stored);
	if (length != NULL)
		*length = stored;
	return true;
}

static bool
stored_record_name(const struct yt_record *record,
    const char *operation, uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint_mbf32(
	    record->bytes + YT_F85, 0U, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0U;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    operation);
		}
		return false;
	}
	stored = (size_t)requested;
	if (stored > YT_TEXT_FIELD_SIZE)
		stored = YT_TEXT_FIELD_SIZE;
	if (stored > 0U && name != NULL)
		memcpy(name, record->bytes, stored);
	if (length != NULL)
		*length = stored;
	return true;
}

bool
yt_port_stored_name(const struct yt_port *port,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	return stored_record_name(&port->record,
	    "port name LEFT$ length", name, length, error);
}

bool
yt_planet_stored_name(const struct yt_planet *planet,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	return stored_record_name(&planet->record,
	    "planet name LEFT$ length", name, length, error);
}

bool
yt_sector_mine_warning_row(float mines, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "** WARNING! SECTOR HAS";
	static const uint8_t suffix[] = " MINES! **";
	struct yt_game_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!yt_game_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !yt_game_row_number(&builder, mines, false)
	    || !yt_game_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_candidate_eligible(int candidate, int current_player_record,
    float cached_sector, float logical_sector)
{
	return candidate != current_player_record
	    && cached_sector == logical_sector;
}

bool
yt_sector_cloak_revealed(float draw, float cached_cloak)
{
	return draw > cached_cloak && cached_cloak != 0.0f;
}

size_t
yt_sector_sensor_targets(const float caller_warps[6], float targets[6])
{
	size_t count = 0U;
	size_t slot;

	if (caller_warps == NULL || targets == NULL)
		return 0U;
	for (slot = 0U; slot < 6U; ++slot) {
		if (caller_warps[slot] != 0.0f)
			targets[count++] = caller_warps[slot];
	}
	return count;
}

bool
yt_sector_port_row(const struct yt_port *port, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Port: ";
	static const uint8_t separator[] = ", Selling: ";
	static const uint8_t equipment[] = "Equ";
	static const uint8_t organics[] = "Org";
	static const uint8_t ore[] = "Ore";
	const uint8_t *commodity = ore;
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct yt_game_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (port == NULL || !yt_port_stored_name(port, name, &name_length,
	    error))
		return false;
	if (port->commodity_class == 1.0f)
		commodity = equipment;
	else if (port->commodity_class == 2.0f)
		commodity = organics;
	if (!yt_game_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !yt_game_row_append(&builder, name, name_length)
	    || !yt_game_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !yt_game_row_append(&builder, commodity, sizeof(ore) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_planet_row(const struct yt_planet *planet, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t prefix[] = "Planet: ";
	static const uint8_t separator[] = " * Forces:";
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct yt_game_row_builder builder = {row, capacity, 0U};
	float forces;

	if (length != NULL)
		*length = 0U;
	if (planet == NULL || !yt_planet_stored_name(planet, name,
	    &name_length, error))
		return false;
	forces = (float)qb_int((double)planet->ground_forces);
	if (!yt_game_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !yt_game_row_append(&builder, name, name_length)
	    || !yt_game_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !yt_game_row_number(&builder, forces, false))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_player_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error)
{
	static const uint8_t indent[] = "    ";
	static const uint8_t team[] = " - Team:";
	static const uint8_t fighters[] = " - Fighters:";
	static const uint8_t shields[] = " - Shields:";
	uint8_t name[YT_TEXT_FIELD_SIZE];
	size_t name_length;
	struct yt_game_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (player == NULL || !yt_player_stored_name(player, name,
	    &name_length, error))
		return false;
	if (!yt_game_row_append(&builder, indent, sizeof(indent) - 1U)
	    || !yt_game_row_append(&builder, name, name_length))
		return false;
	if (player->team > 0.0f
	    && (!yt_game_row_append(&builder, team, sizeof(team) - 1U)
	    || !yt_game_row_number(&builder, player->team, false)))
		return false;
	if (!yt_game_row_append(&builder, fighters, sizeof(fighters) - 1U)
	    || !yt_game_row_number(&builder, player->fighters, true)
	    || !yt_game_row_append(&builder, shields, sizeof(shields) - 1U)
	    || !yt_game_row_number(&builder, player->shields, false))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_sector_fighter_row(const struct yt_sector *sector,
    int current_player_record, const struct yt_player *owner,
    const struct yt_sector *team_overlay, uint8_t *row, size_t capacity,
    size_t *length, uint8_t *scratch, size_t scratch_capacity,
    size_t *scratch_length, bool *scratch_changed, struct yt_error *error)
{
	static const uint8_t belonging[] = " (Belong to ";
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t self[] = "YOU)";
	static const uint8_t team_prefix[] = " Team [";
	static const uint8_t overlay_prefix[] = " [";
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length = 0U;
	struct yt_game_row_builder builder = {row, capacity, 0U};
	struct yt_game_row_builder scratch_builder = {
		scratch, scratch_capacity, 0U
	};
	bool changed = false;

	if (length != NULL)
		*length = 0U;
	if (scratch_changed != NULL)
		*scratch_changed = false;
	if (sector == NULL
	    || !yt_game_row_number(&builder, sector->fighters, true)
	    || !yt_game_row_append(&builder, belonging,
	    sizeof(belonging) - 1U))
		return false;
	if (sector->fighter_owner == (float)current_player_record) {
		if (!yt_game_row_append(&builder, self, sizeof(self) - 1U))
			return false;
	}
	else {
		if (sector->fighter_owner == -1.0f) {
			if (!yt_game_row_append(&scratch_builder, xannor,
			    sizeof(xannor) - 1U))
				return false;
			changed = true;
		}
		else if (sector->fighter_owner == -2.0f) {
			if (!yt_game_row_append(&scratch_builder, mercenaries,
			    sizeof(mercenaries) - 1U))
				return false;
			changed = true;
		}
		else {
			char number[64];
			int number_length;
			bool overflow;
			int team_name_length;
			size_t stored_team_length;

			if (owner == NULL || !yt_player_stored_name(owner,
			    owner_name, &owner_name_length, error)
			    || !yt_game_row_append(&scratch_builder, owner_name,
			    owner_name_length))
				return false;
			changed = true;
			if (owner->team != 0.0f) {
				number_length = qb_str_single(number, sizeof(number),
				    owner->team);
				if (number_length < 1 || team_overlay == NULL
				    || !yt_game_row_append(&scratch_builder,
				    team_prefix, sizeof(team_prefix) - 1U)
				    || !yt_game_row_append(&scratch_builder,
				    number + 1, (size_t)number_length - 1U)
				    || !yt_game_row_append(&scratch_builder, "]", 1U))
					return false;
				team_name_length = (int)qb_cint_mbf32(
				    team_overlay->record.bytes + YT_F73, 0U,
				    &overflow);
				if (overflow || team_name_length < 0) {
					if (error != NULL) {
						error->status = YT_RANGE;
						snprintf(error->operation,
						    sizeof(error->operation), "%s",
						    "team name LEFT$ length");
					}
					return false;
				}
				stored_team_length = (size_t)team_name_length;
				if (stored_team_length > YT_TEXT_FIELD_SIZE)
					stored_team_length = YT_TEXT_FIELD_SIZE;
				if (stored_team_length > 0U
				    && (!yt_game_row_append(&scratch_builder,
				    overlay_prefix, sizeof(overlay_prefix) - 1U)
				    || !yt_game_row_append(&scratch_builder,
				    team_overlay->record.bytes, stored_team_length)
				    || !yt_game_row_append(&scratch_builder, "]", 1U)))
					return false;
			}
		}
		if (!yt_game_row_append(&builder, scratch_builder.row,
		    scratch_builder.length)
		    || !yt_game_row_append(&builder, ")", 1U))
			return false;
	}
	if (length != NULL)
		*length = builder.length;
	if (changed && scratch_length != NULL)
		*scratch_length = scratch_builder.length;
	if (scratch_changed != NULL)
		*scratch_changed = changed;
	return true;
}
