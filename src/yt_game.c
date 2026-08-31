#include "yt_game.h"

#include "qb.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
startup_configuration_error(struct yt_error *error, enum yt_status status,
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

static float
startup_single_subtract(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
startup_single_multiply(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
startup_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static bool
owned_planets_error(struct yt_error *error, enum yt_status status,
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
yt_owned_planets_run(struct yt_owned_planets_state *state,
    const struct yt_owned_planets_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t scanning[] = "Scanning...";
	static const uint8_t none[] = "None found!";
	static const uint8_t prefix[] = "Planet: ";
	static const uint8_t infix[] = " Sector:";
	int sector_number;

	if (state == NULL || ops == NULL || ops->read_sector == NULL
	    || ops->read_planet == NULL || ops->present == NULL
	    || ops->set_color == NULL || ops->set_blink == NULL
	    || state->maximum_sector < 0)
		return owned_planets_error(error, YT_INVALID,
		    "owned-planet state");
	state->found = false;
	state->current_sector = 0;
	state->current_link = 0.0f;
	state->current_record_expression = 0.0f;
	state->current_planet_record = 0U;
	state->foreground = 2.0f;
	ops->set_color(context, 2);
	if (!ops->present(context, NULL, 0U, false,
	    "owned-planet opening blank", error)
	    || !ops->present(context, scanning, sizeof(scanning) - 1U, false,
	    "owned-planet scanning row", error)
	    || !ops->present(context, NULL, 0U, false,
	    "owned-planet scanning blank", error))
		return false;
	state->foreground = 3.0f;
	ops->set_color(context, 3);
	for (sector_number = 1; sector_number <= state->maximum_sector;
	    ++sector_number) {
		struct yt_sector sector;
		struct yt_planet planet;
		volatile float record_expression;
		uint32_t physical_record;

		state->current_sector = sector_number;
		if (!ops->read_sector(context, sector_number, &sector, error))
			return false;
		state->current_link = sector.planet;
		if (sector.planet == 0.0f)
			continue;
		record_expression = state->planet_record_base + sector.planet;
		state->current_record_expression = record_expression;
		physical_record = qb_brun_random_record_number(record_expression);
		state->current_planet_record = physical_record;
		if (physical_record == 0U)
			return owned_planets_error(error, YT_RANGE,
			    "owned-planet record number");
		if (!ops->read_planet(context, physical_record, &planet, error))
			return false;
		if (planet.owner == state->current_player) {
			uint8_t row[128];
			char number[64];
			size_t length = 0U;
			int number_length = qb_str_single(number, sizeof(number),
			    (float)sector_number);

			if (number_length < 0)
				return owned_planets_error(error, YT_RANGE,
				    "owned-planet sector format");
			memcpy(row + length, prefix, sizeof(prefix) - 1U);
			length += sizeof(prefix) - 1U;
			memcpy(row + length, planet.record.bytes,
			    YT_TEXT_FIELD_SIZE);
			length += YT_TEXT_FIELD_SIZE;
			memcpy(row + length, infix, sizeof(infix) - 1U);
			length += sizeof(infix) - 1U;
			memcpy(row + length, number, (size_t)number_length);
			length += (size_t)number_length;
			if (!ops->present(context, row, length, true,
			    "owned-planet match row", error))
				return false;
			state->found = true;
		}
	}
	if (!state->found) {
		state->blink = 1.0f;
		ops->set_blink(context, 1.0f);
		return ops->present(context, none, sizeof(none) - 1U, true,
		    "owned-planet none row", error);
	}
	return true;
}

static bool
xannor_victory_error(struct yt_error *error, enum yt_status status,
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
yt_xannor_victory_run(struct yt_xannor_victory_state *state,
    const struct yt_xannor_victory_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t pause[] = "[PAUSE]";
	static const uint8_t bonus[] =
	    "Collect 16,000,000 credit bonus!";
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	size_t player_name_length;
	uint8_t banner[79];
	bool credit_hydrated;
	unsigned ordinal;

	if (state == NULL || ops == NULL || ops->play_file == NULL
	    || ops->present == NULL || ops->wait == NULL
	    || ops->set_foreground == NULL || ops->set_blink == NULL
	    || ops->clear_queue == NULL || ops->mutate_credits == NULL
	    || ops->sound == NULL
	    || ops->append_news == NULL || ops->append_radio == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL)
		return xannor_victory_error(error, YT_INVALID,
		    "Xannor victory state");
	state->winner_length = 0U;
	state->sounds_completed = 0U;
	state->news_completed = 0U;
	state->radio_completed = 0U;
	state->foreground = 7.0f;
	state->pager_foreground = 7.0f;
	ops->set_foreground(context, 7.0f);
	if (!ops->play_file(context, "XannorHQ.TXT", error)
	    || !ops->present(context, pause, sizeof(pause) - 1U,
	    YT_XANNOR_VICTORY_RAW, "Xannor victory pause", error)
	    || !ops->wait(context, 99.0, "Xannor victory wait", error)
	    || !ops->present(context, NULL, 0U, YT_XANNOR_VICTORY_LINE,
	    "Xannor victory post-wait blank", error))
		return false;
	state->blink = 1.0f;
	ops->set_blink(context, 1.0f);
	if (!ops->present(context, bonus, sizeof(bonus) - 1U,
	    YT_XANNOR_VICTORY_BOLD_LINE, "Xannor victory bonus", error))
		return false;
	ops->clear_queue(context);
	credit_hydrated = false;
	if (!ops->mutate_credits(context, state->current_player,
	    16000000.0f, &state->player, &credit_hydrated, error))
		return false;
	if (!credit_hydrated)
		return xannor_victory_error(error, YT_INVALID,
		    "Xannor victory credit hydrate");
	state->awarded_credits = state->player.credits;
	for (ordinal = 0U; ordinal < 3U; ++ordinal) {
		if (!ops->sound(context, 2.0f, "Xannor victory sound", error))
			return false;
		++state->sounds_completed;
	}
	memset(banner, '*', sizeof(banner));
	if (!yt_player_stored_name(&state->player, player_name,
	    &player_name_length, error)
	    || !yt_xannor_victory_winner(player_name, player_name_length,
	    state->winner, sizeof(state->winner), &state->winner_length))
		return false;
	if (!ops->append_news(context, banner, sizeof(banner), error))
		return false;
	++state->news_completed;
	if (!ops->append_news(context, state->winner, state->winner_length,
	    error))
		return false;
	++state->news_completed;
	if (!ops->append_news(context, banner, sizeof(banner), error))
		return false;
	++state->news_completed;
	if (!ops->append_radio(context, banner, sizeof(banner), -2.0f, -2.0f,
	    error))
		return false;
	++state->radio_completed;
	if (!ops->append_radio(context, state->winner, state->winner_length,
	    -2.0f, -2.0f, error))
		return false;
	++state->radio_completed;
	if (!ops->append_radio(context, banner, sizeof(banner), -2.0f, -2.0f,
	    error))
		return false;
	++state->radio_completed;
	if (!ops->read_sector(context, 21, &state->sector, error))
		return false;
	state->sector.metadata = state->current_player;
	return ops->write_sector(context, 21, &state->sector, error);
}

bool
yt_startup_configuration_run(struct yt_startup_configuration_state *state,
    const struct yt_startup_configuration_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_config *config;
	bool overflow;
	int32_t path_count;
	int32_t local_mode;
	float counter;
	size_t index;

	if (state == NULL || ops == NULL || state->config == NULL
	    || state->sector_cache == NULL || state->cloak_cache == NULL
	    || state->cache_count == 0U || ops->close_data == NULL
	    || ops->open_data == NULL
	    || ops->load_config == NULL || ops->store_config == NULL
	    || ops->read_player == NULL || ops->write_player == NULL
	    || ops->random == NULL)
		return false;
	state->installed_handler = 0x45F7U;
	state->handler_installed = true;
	config = state->config;
	if (!ops->close_data(context, error)
	    || !ops->open_data(context, error)
	    || !ops->load_config(context, config, error))
		return false;

	path_count = qb_cint(config->scoreboard_length, &overflow);
	if (overflow || path_count < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "startup scoreboard LEFT$");
	state->scoreboard_path_length = (size_t)path_count;
	if (state->scoreboard_path_length > YT_TEXT_FIELD_SIZE)
		state->scoreboard_path_length = YT_TEXT_FIELD_SIZE;
	memcpy(config->scoreboard, config->record.bytes,
	    state->scoreboard_path_length);
	qb_compat_upper_n((uint8_t *)config->scoreboard,
	    state->scoreboard_path_length);
	config->scoreboard[state->scoreboard_path_length] = '\0';

	if (config->headquarters == 0.0f) {
		if (!yt_record_set_number(&config->record, YT_F117, 85.0f)
		    || !ops->store_config(context, config, error))
			return false;
		config->headquarters = 85.0f;
	}
	if (config->genesis_ports < 20.0f)
		config->genesis_ports = 200.0f;
	if (state->scoreboard_path_length == 0U) {
		static const char default_path[] = "ytscore.asc";

		memcpy(config->scoreboard, default_path, sizeof(default_path));
		state->scoreboard_path_length = sizeof(default_path) - 1U;
	}
	local_mode = qb_cint(state->local_mode, &overflow);
	if (overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "startup local-mode CINT");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode != 0)
		config->local_screen = -1.0f;
	if (config->lottery_plays < 0.0f || config->lottery_plays > 9.0f)
		config->lottery_plays = 3.0f;
	if (config->maximum_planets == 0.0f)
		config->maximum_planets = 100.0f;
	if (config->maximum_holds < 5.0f || config->maximum_holds > 1000.0f)
		config->maximum_holds = 1000.0f;
	if (config->turns_per_day < 100.0f || config->turns_per_day > 2500.0f)
		config->turns_per_day = 500.0f;

	if (state->cache_guard == 0.0f) {
		counter = 2.0f;
		while (counter <= config->sector_offset) {
			struct yt_player player;
			int32_t basic = qb_cint(counter, &overflow);

			if (overflow || basic < 0
			    || (size_t)basic >= state->cache_count)
				return startup_configuration_error(error, YT_RANGE,
				    "startup player-cache index");
			if (!ops->read_player(context, basic, &player, error))
				return false;
			state->sector_cache[basic] = player.sector;
			state->cloak_cache[basic] = player.cloak;
			if (player.cloak < 0.0f || player.cloak > 1.0f) {
				player.cloak = 1.0f;
				state->cloak_cache[basic] = 1.0f;
				if (!yt_record_set_number(&player.record, YT_F125,
				    1.0f)
				    || !ops->write_player(context, basic, &player,
				    error))
					return false;
			}
			counter = startup_single_add(counter, 1.0f);
		}
		state->cache_guard = 1.0f;
	}
	for (index = 0U; index < 2U; ++index) {
		float draw;
		float difference;
		float span;
		float product;

		if (!ops->random(context, &draw, error))
			return false;
		difference = startup_single_subtract(config->port_offset,
		    config->sector_offset);
		span = startup_single_subtract(difference, 2.0f);
		product = startup_single_multiply(draw, span);
		state->black_hole[index] = startup_single_add(floorf(product),
		    2.0f);
	}
	return true;
}

static float projectile_single_add(float left, float right);
static float projectile_single_sub(float left, float right);
static float projectile_single_mul(float left, float right);

void
yt_team_loader_begin(float team_id, struct yt_team_loader_cache *cache,
    bool *needs_overlay)
{
	if (cache == NULL)
		return;
	cache->available = -1.0f;
	memset(cache->roster, 0, sizeof(cache->roster));
	cache->counter = 5.0f;
	if (needs_overlay != NULL)
		*needs_overlay = team_id >= 1.0f && team_id <= 50.0f;
}

bool
yt_team_loader_finish(const struct yt_record *overlay,
    float current_player, uint8_t conversion_mode,
    struct yt_team_loader_cache *cache, enum yt_team_loader_route *route,
    struct yt_error *error)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	float roster[4];
	bool overflow;
	int32_t converted_length;
	size_t name_length;
	size_t index;
	bool live = false;

	if (overlay == NULL || cache == NULL || route == NULL)
		return false;
	for (index = 0; index < YT_ARRAY_LEN(roster); ++index) {
		roster[index] = yt_record_get_number(overlay,
		    roster_offsets[index]);
		if (roster[index] != 0.0f)
			live = true;
	}
	if (!live) {
		*route = YT_TEAM_LOADER_ROSTER_DEAD;
		return true;
	}

	cache->available = 0.0f;
	converted_length = qb_cint_mode(
	    yt_record_get_number(overlay, YT_F73), conversion_mode,
	    &overflow);
	if (overflow || converted_length < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "team loader name length");
		}
		return false;
	}
	name_length = (size_t)converted_length;
	if (name_length > YT_TEXT_FIELD_SIZE)
		name_length = YT_TEXT_FIELD_SIZE;
	memcpy(cache->name, overlay->bytes, name_length);
	cache->name[name_length] = '\0';
	cache->name_length = name_length;
	memcpy(cache->password, overlay->bytes + YT_F113, 4U);
	cache->password[4] = '\0';
	cache->captain = yt_record_get_number(overlay, YT_F77);
	if (cache->captain == current_player)
		cache->captain_flag = -1.0f;
	memcpy(cache->roster, roster, sizeof(roster));
	*route = YT_TEAM_LOADER_LIVE;
	return true;
}

static bool
info_team_append(uint8_t *row, size_t capacity, size_t *length,
    const void *text, size_t text_length)
{
	if (row == NULL || length == NULL || *length > capacity
	    || text_length > capacity - *length
	    || (text == NULL && text_length != 0U))
		return false;
	if (text_length != 0U)
		memcpy(row + *length, text, text_length);
	*length += text_length;
	return true;
}

bool
yt_info_team_resolver_run(struct yt_info_team_state *state,
    const struct yt_info_team_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t none[] = "Team  : None";
	static const uint8_t team_prefix[] = "Team  :";
	static const uint8_t separator[] = ", ";
	static const uint8_t self_prefix[] = "You are the Captain of team";
	static const uint8_t other_prefix[] = "Your Team Captain is: ";
	static const uint8_t promoted[] =
	    "Your team has no captain! You've been promoted to Captain!";
	static const uint8_t congratulations[] =
	    "Congratulations Captain! See Team Menu for your new options!";
	uint8_t row[256];
	char number[64];
	int number_length;
	size_t row_length;
	bool overflow;
	int32_t name_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->load_team == NULL || ops->read_overlay == NULL
	    || ops->write_overlay == NULL || ops->present == NULL)
		return false;
	memset(&state->current_player, 0, sizeof(state->current_player));
	memset(&state->team, 0, sizeof(state->team));
	state->team_id = 0.0f;
	state->captain_flag = 0.0f;
	state->captain_record = 0.0f;
	state->captain_name_length = 0U;
	state->current_is_captain = false;
	state->route = YT_INFO_TEAM_NONE;
	if (!ops->read_player(context, state->current_record,
	    &state->current_player, error))
		return false;
	state->team_id = state->current_player.team;
	if (state->team_id == 0.0f)
		return ops->present(context, none, sizeof(none) - 1U, error)
		    && ops->present(context, NULL, 0U, error);
	if (!ops->load_team(context, state->team_id, state->current_record,
	    &state->captain_flag, &state->team, error))
		return false;
	number_length = qb_str_single(number, sizeof(number), state->team_id);
	row_length = 0U;
	if (number_length < 0 || state->team.name_length > YT_TEXT_FIELD_SIZE
	    || !info_team_append(row, sizeof(row), &row_length, team_prefix,
	    sizeof(team_prefix) - 1U)
	    || !info_team_append(row, sizeof(row), &row_length, number,
	    (size_t)number_length)
	    || !info_team_append(row, sizeof(row), &row_length, separator,
	    sizeof(separator) - 1U)
	    || !info_team_append(row, sizeof(row), &row_length, state->team.name,
	    state->team.name_length))
		return false;
	if (!ops->present(context, row, row_length, error)
	    || !ops->present(context, NULL, 0U, error))
		return false;
	if (state->captain_flag != 0.0f) {
		state->current_is_captain = true;
		state->route = YT_INFO_TEAM_SELF_CAPTAIN;
		row_length = 0U;
		if (!info_team_append(row, sizeof(row), &row_length, self_prefix,
		    sizeof(self_prefix) - 1U)
		    || !info_team_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length)
		    || !info_team_append(row, sizeof(row), &row_length, "!", 1U))
			return false;
		return ops->present(context, row, row_length, error)
		    && ops->present(context, NULL, 0U, error);
	}
	state->captain_record = state->team.captain;
	if (state->captain_record >= 2.0f
	    && state->captain_record <= state->sector_offset) {
		struct yt_player captain;

		if (!ops->read_player(context, state->captain_record, &captain,
		    error))
			return false;
		if (captain.name_length > 0.0f) {
			name_length = qb_cint_mode((double)captain.name_length,
			    state->conversion_mode, &overflow);
			if (overflow || name_length < 0) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "Info captain name length");
				}
				return false;
			}
			state->captain_name_length = (size_t)name_length;
			if (state->captain_name_length > YT_TEXT_FIELD_SIZE)
				state->captain_name_length = YT_TEXT_FIELD_SIZE;
			memcpy(state->captain_name, captain.name,
			    state->captain_name_length);
		}
		else
			state->captain_record = 0.0f;
		if (captain.team != state->team_id)
			state->captain_record = 0.0f;
	}
	if (!(state->captain_record >= 2.0f
	    && state->captain_record <= state->sector_offset)) {
		struct yt_sector fresh;

		state->captain_record = state->current_record;
		state->captain_flag = 1.0f;
		state->current_is_captain = true;
		state->route = YT_INFO_TEAM_PROMOTED;
		if (!ops->read_overlay(context, state->team_id, &fresh, error)
		    || !yt_record_set_number(&fresh.record, YT_F77,
		    state->current_record))
			return false;
		state->team.overlay = fresh;
		state->team.captain = state->current_record;
		if (!ops->write_overlay(context, state->team_id, &fresh, error)
		    || !ops->present(context, promoted, sizeof(promoted) - 1U,
		    error)
		    || !ops->present(context, congratulations,
		    sizeof(congratulations) - 1U, error)
		    || !ops->present(context, NULL, 0U, error))
			return false;
		return true;
	}
	{
		struct yt_player ignored;

		if (!ops->read_player(context, state->captain_record, &ignored,
		    error))
			return false;
	}
	state->route = YT_INFO_TEAM_OTHER_CAPTAIN;
	row_length = 0U;
	if (!info_team_append(row, sizeof(row), &row_length, other_prefix,
	    sizeof(other_prefix) - 1U)
	    || !info_team_append(row, sizeof(row), &row_length,
	    state->captain_name, state->captain_name_length)
	    || !info_team_append(row, sizeof(row), &row_length, "!", 1U))
		return false;
	return ops->present(context, row, row_length, error)
	    && ops->present(context, NULL, 0U, error);
}

static bool
info_panel_present(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const uint8_t *text, size_t length, enum yt_info_panel_output_kind kind,
    float width, struct yt_error *error)
{
	return ops->present(context, text, length, kind, width, state, error);
}

static bool
info_panel_cell(uint8_t *cell, size_t capacity, size_t *length,
    const char *label, const char *value)
{
	static const uint8_t bar = 0xba;

	*length = 0U;
	return info_team_append(cell, capacity, length, &bar, 1U)
	    && info_team_append(cell, capacity, length, label, strlen(label))
	    && info_team_append(cell, capacity, length, value, strlen(value));
}

static bool
info_panel_ordinary(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const char *left_label, const char *left_value,
    const char *right_label, const char *right_value,
    struct yt_error *error)
{
	uint8_t left[160];
	uint8_t right[160];
	size_t left_length;
	size_t right_length;
	static const uint8_t bar = 0xba;

	return info_panel_cell(left, sizeof(left), &left_length, left_label,
	    left_value)
	    && info_panel_cell(right, sizeof(right), &right_length, right_label,
	    right_value)
	    && info_panel_present(state, ops, context, left, left_length,
	    YT_INFO_PANEL_FIXED, 26.0f, error)
	    && info_panel_present(state, ops, context, right, right_length,
	    YT_INFO_PANEL_FIXED, 23.0f, error)
	    && info_panel_present(state, ops, context, &bar, 1U,
	    YT_INFO_PANEL_LINE, 0.0f, error);
}

static bool
info_panel_commodity(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    const char *left_label, const char *left_value,
    const char *right_label, float right_value, struct yt_error *error)
{
	uint8_t left[160];
	uint8_t right[160];
	char number[64];
	size_t left_length;
	size_t right_length;
	int number_length;
	static const uint8_t bar = 0xba;

	number_length = qb_str_single(number, sizeof(number), right_value);
	if (number_length < 0
	    || !info_panel_cell(left, sizeof(left), &left_length, left_label,
	    left_value)
	    || !info_panel_cell(right, sizeof(right), &right_length,
	    right_label, "")
	    || !info_panel_present(state, ops, context, left, left_length,
	    YT_INFO_PANEL_FIXED, 26.0f, error)
	    || !info_panel_present(state, ops, context, right, right_length,
	    YT_INFO_PANEL_FIXED, 17.0f, error))
		return false;
	if (right_value != 0.0f) {
		state->bold = 1.0f;
		state->foreground = 7.0f;
		state->background = 4.0f;
	}
	if (!info_panel_present(state, ops, context, (const uint8_t *)number,
	    (size_t)number_length, YT_INFO_PANEL_FIXED, 6.0f, error))
		return false;
	state->foreground = 2.0f;
	state->background = 0.0f;
	return info_panel_present(state, ops, context, &bar, 1U,
	    YT_INFO_PANEL_LINE, 0.0f, error);
}

bool
yt_info_panel_run(struct yt_info_panel_state *state,
    const struct yt_info_panel_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t top[50] = {
		0xc9, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcb, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbb
	};
	static const uint8_t bottom[50] = {
		0xc8, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xca, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbc
	};
	static const uint8_t title[] = "[ Info ]";
	uint8_t row[256];
	char left[64];
	char right[64];
	size_t row_length;
	float saved_foreground;
	float cloak_percent;
	int length;

	if (state == NULL || ops == NULL || ops->refresh_time == NULL
	    || ops->team == NULL || ops->read_player == NULL
	    || ops->present == NULL
	    || (state->cached_name == NULL && state->cached_name_length != 0U))
		return false;
	if (!ops->refresh_time(context, state->time_text,
	    sizeof(state->time_text), &state->time_text_length, error)
	    || state->time_text_length > sizeof(state->time_text))
		return false;
	saved_foreground = state->foreground;
	state->foreground = 2.0f;
	if (!info_panel_present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_LINE, 0.0f, error)
	    || !info_panel_present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_FIXED, 20.0f, error)
	    || !info_panel_present(state, ops, context, title,
	    sizeof(title) - 1U, YT_INFO_PANEL_LINE, 0.0f, error)
	    || !info_panel_present(state, ops, context, NULL, 0U,
	    YT_INFO_PANEL_LINE, 0.0f, error))
		return false;
	row_length = 0U;
	if (!info_team_append(row, sizeof(row), &row_length, "Name  : ", 8U)
	    || !info_team_append(row, sizeof(row), &row_length,
	    state->cached_name, state->cached_name_length)
	    || !info_panel_present(state, ops, context, row, row_length,
	    YT_INFO_PANEL_LINE, 0.0f, error))
		return false;
	row_length = 0U;
	if (!info_team_append(row, sizeof(row), &row_length, "Time  :", 7U)
	    || !info_team_append(row, sizeof(row), &row_length,
	    state->time_text, state->time_text_length)
	    || !info_panel_present(state, ops, context, row, row_length,
	    YT_INFO_PANEL_LINE, 0.0f, error)
	    || !ops->team(context, error)
	    || !ops->read_player(context, &state->player, error)
	    || !info_panel_present(state, ops, context, top, sizeof(top),
	    YT_INFO_PANEL_LINE, 0.0f, error))
		return false;
	length = qb_str_double(left, sizeof(left), (double)state->player.credits);
	if (length < 0)
		return false;
	length = qb_str_single(right, sizeof(right), state->player.sector);
	if (length < 0 || !info_panel_ordinary(state, ops, context,
	    " Credits.. :", left, " Sector....... :", right, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.turns) < 0
	    || qb_str_single(right, sizeof(right), state->player.holds) < 0
	    || !info_panel_ordinary(state, ops, context, " Turns.... :",
	    left, " Holds........ :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)state->player.fighters) < 0
	    || !info_panel_commodity(state, ops, context, " Fighters. :",
	    left, " Ore.......... :", state->player.ore, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.mines) < 0
	    || !info_panel_commodity(state, ops, context, " Mines.... :",
	    left, " Organics..... :", state->player.organics, error))
		return false;
	if (qb_str_single(left, sizeof(left), state->player.missiles) < 0
	    || !info_panel_commodity(state, ops, context, " Missiles. :",
	    left, " Equipment.... :", state->player.equipment, error))
		return false;
	(void)snprintf(left, sizeof(left), "%s",
	    state->player.danger_scanner == 0.0f ? " NONE" : " Installed");
	if (qb_str_single(right, sizeof(right), state->player.ports_owned) < 0
	    || !info_panel_ordinary(state, ops, context, " Scanner.. :",
	    left, " Ports Owned.. :", right, error))
		return false;
	if (qb_str_double(left, sizeof(left),
	    (double)state->player.shields) < 0)
		return false;
	if (state->anti_cloak != 0.0f)
		(void)snprintf(right, sizeof(right), "%s", " FAIL");
	else {
		cloak_percent = floorf(startup_single_multiply(
		    state->player.cloak, 100.0f));
		if (qb_str_single(right, sizeof(right), cloak_percent) < 0
		    || strlen(right) + 1U >= sizeof(right))
			return false;
		strcat(right, "%");
	}
	if (!info_panel_ordinary(state, ops, context, " Shields.. :", left,
	    " Cloak Energy. :", right, error)
	    || qb_str_single(left, sizeof(left),
	    state->player.ground_forces) < 0
	    || qb_str_single(right, sizeof(right), state->player.plasma) < 0
	    || !info_panel_ordinary(state, ops, context, " Forces... :", left,
	    " Plasma Bolts. :", right, error)
	    || !info_panel_present(state, ops, context, bottom, sizeof(bottom),
	    YT_INFO_PANEL_LINE, 0.0f, error))
		return false;
	state->foreground = saved_foreground;
	return true;
}

static bool
spy_present(struct yt_spy_sweep_state *state,
    const struct yt_spy_sweep_ops *ops, void *context,
    const uint8_t *text, size_t length, enum yt_spy_output_kind kind,
    struct yt_error *error)
{
	return ops->present(context, text, length, kind, state, error);
}

static bool
spy_first_finding(struct yt_spy_sweep_state *state,
    const struct yt_spy_sweep_ops *ops, void *context, size_t spy,
    int sector, struct yt_error *error)
{
	static const uint8_t prefix[] = "*** RADIO MESSAGE FROM SPY ";
	static const uint8_t middle[] =
	    "! The following was found in sector";
	uint8_t row[256];
	char spy_number[64];
	char sector_number[64];
	size_t length = 0U;
	int amount;

	if (!spy_present(state, ops, context, NULL, 0U, YT_SPY_LINE, error))
		return false;
	if (state->found_scratch != 0.0f)
		return true;
	state->found_scratch = 1.0f;
	state->last_reported_sectors[spy] = sector;
	if (!ops->sound(context, 9.0f, error))
		return false;
	amount = qb_str_single(spy_number, sizeof(spy_number), (float)(spy + 1U));
	if (amount < 1)
		return false;
	spy_number[0] = '#';
	if (qb_str_single(sector_number, sizeof(sector_number),
	    (float)sector) < 0
	    || !info_team_append(row, sizeof(row), &length,
	    prefix, sizeof(prefix) - 1U)
	    || !info_team_append(row, sizeof(row), &length,
	    spy_number, (size_t)amount)
	    || !info_team_append(row, sizeof(row), &length,
	    middle, sizeof(middle) - 1U)
	    || !info_team_append(row, sizeof(row), &length,
	    sector_number, strlen(sector_number))
	    || !info_team_append(row, sizeof(row), &length, ":", 1U)
	    || !spy_present(state, ops, context, row, length,
	    YT_SPY_BOLD_LINE, error)
	    || !spy_present(state, ops, context, NULL, 0U,
	    YT_SPY_LINE, error))
		return false;
	return true;
}

bool
yt_spy_sweep_run(struct yt_spy_sweep_state *state,
    const struct yt_spy_sweep_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t disruption[] =
	    "** Space-time disruption detected! **";
	static const uint8_t cloak_notice[] =
	    "The spy detected the shimmering of a cloaking device!";
	static const uint8_t ship_heading[] = "Other Ships: ";
	static const uint8_t fighter_heading[] = "Fighters in sector:";
	float iterator;

	if (state == NULL || ops == NULL || state->spy_sectors == NULL
	    || state->last_reported_sectors == NULL
	    || state->sector_cache == NULL || state->cloak_cache == NULL
	    || ops->read_sector == NULL || ops->update_planet == NULL
	    || ops->read_planet == NULL || ops->read_player == NULL
	    || ops->read_team == NULL || ops->random == NULL
	    || ops->sound == NULL || ops->present == NULL
	    || ops->pause == NULL)
		return false;
	if (state->active_spies == 0.0f)
		return true;
	iterator = 1.0f;
	while (iterator <= state->active_spies) {
		struct yt_sector sector;
		bool overflow;
		int32_t converted_spy = qb_cint(iterator, &overflow);
		size_t spy;
		int sector_number;
		bool first_ship = true;
		int candidate;

		if (overflow || converted_spy < 1
		    || (size_t)converted_spy > state->spy_capacity)
			return startup_configuration_error(error, YT_RANGE,
			    "active spy count adjacent memory");
		spy = (size_t)converted_spy - 1U;
		sector_number = state->spy_sectors[spy];
		state->foreground = 7.0f;
		if (!(sector_number == state->last_reported_sectors[spy]
		    && sector_number != 0)) {
			state->found_scratch = 0.0f;
			state->warp_destination_scratch = (float)sector_number;
			if (!ops->read_sector(context, sector_number, &sector, error))
				return false;
			if ((float)sector_number == state->disruption_sectors[0]
			    || (float)sector_number == state->disruption_sectors[1]) {
				if (!spy_first_finding(state, ops, context, spy,
				    sector_number, error)
				    || !spy_present(state, ops, context, disruption,
				    sizeof(disruption) - 1U, YT_SPY_ATTENTION, error))
					return false;
			}
			if (sector.mines != 0.0f) {
				uint8_t row[128];
				size_t length;

				if (!spy_first_finding(state, ops, context, spy,
				    sector_number, error)
				    || !yt_sector_mine_warning_row(sector.mines, row,
				    sizeof(row), &length)
				    || !spy_present(state, ops, context, row, length,
				    YT_SPY_ATTENTION, error))
					return false;
			}
			if (sector.planet > 0.0f) {
				struct yt_planet planet;
				uint8_t row[160];
				size_t length;

				if (!ops->update_planet(context, sector.planet, error)
				    || !ops->read_planet(context, sector.planet,
				    &planet, error)
				    || !spy_first_finding(state, ops, context, spy,
				    sector_number, error)
				    || !yt_sector_planet_row(&planet, row, sizeof(row),
				    &length, error)
				    || !spy_present(state, ops, context, row, length,
				    YT_SPY_BOLD_LINE, error)
				    || !ops->read_sector(context, sector_number,
				    &sector, error))
					return false;
			}
			for (candidate = 2;
			    (float)candidate <= state->last_player_record;
			    ++candidate) {
				float draw;
				float cloak;
				bool detected;

				if ((size_t)candidate >= state->cache_count)
					return startup_configuration_error(error, YT_RANGE,
					    "last player cache aliases adjacent memory");
				if (!yt_sector_candidate_eligible(candidate,
				    state->current_player_record,
				    state->sector_cache[candidate],
				    (float)sector_number))
					continue;
				if (!ops->random(context, &draw, error))
					return false;
				cloak = state->cloak_cache[candidate];
				detected = yt_sector_cloak_revealed(draw, cloak);
				if (detected) {
					if (!spy_first_finding(state, ops, context,
					    spy, sector_number, error)
					    || !spy_present(state, ops, context,
					    cloak_notice, sizeof(cloak_notice) - 1U,
					    YT_SPY_BOLD_LINE, error))
						return false;
					state->cloak_cache[candidate] = 0.0f;
					if (!ops->sound(context, 4.0f, error))
						return false;
				}
				if (state->cloak_cache[candidate] != 0.0f
				    && !detected)
					continue;
				if (!spy_first_finding(state, ops, context, spy,
				    sector_number, error))
					return false;
				if (first_ship) {
					if (!spy_present(state, ops, context,
					    ship_heading, sizeof(ship_heading) - 1U,
					    YT_SPY_BOLD_LINE, error))
						return false;
					first_ship = false;
				}
				{
					struct yt_player player;
					uint8_t row[256];
					size_t length;

					if (!ops->read_player(context, (float)candidate,
					    &player, error))
						return false;
					state->dead_counter_scratch = startup_single_add(
					    state->dead_counter_scratch, 1.0f);
					if (!yt_sector_player_row(&player, row,
					    sizeof(row), &length, error))
						return false;
					state->bold = 1.0f;
					if (!spy_present(state, ops, context, row,
					    length, YT_SPY_LINE, error))
						return false;
				}
			}
			if (!ops->read_sector(context, sector_number, &sector, error))
				return false;
			if (sector.fighters != 0.0f
			    && sector.fighter_owner
			    != (float)state->current_player_record) {
				struct yt_sector refreshed;
				struct yt_sector displayed;
				struct yt_player owner_player;
				struct yt_sector team_overlay;
				const struct yt_player *owner_pointer = NULL;
				const struct yt_sector *team_pointer = NULL;
				uint8_t row[256];
				uint8_t scratch[160];
				size_t length;
				size_t scratch_length = 0U;
				bool scratch_changed;
				float owner = sector.fighter_owner;

				if (!ops->read_sector(context, sector_number,
				    &refreshed, error)
				    || !spy_first_finding(state, ops, context, spy,
				    sector_number, error)
				    || !spy_present(state, ops, context,
				    fighter_heading, sizeof(fighter_heading) - 1U,
				    YT_SPY_BOLD_RAW, error))
					return false;
				state->bold = 1.0f;
				displayed = refreshed;
				displayed.fighter_owner = owner;
				if (owner != -1.0f && owner != -2.0f) {
					uint8_t ignored_name[YT_TEXT_FIELD_SIZE];
					size_t ignored_length;

					if (!ops->read_player(context, owner,
					    &owner_player, error)
					    || !yt_player_stored_name(&owner_player,
					    ignored_name, &ignored_length, error))
						return false;
					owner_pointer = &owner_player;
					if (owner_player.team != 0.0f) {
						state->dead_counter_scratch =
						    startup_single_add(
						    state->dead_counter_scratch, 1.0f);
						if (!ops->read_team(context,
						    owner_player.team, &team_overlay, error))
							return false;
						team_pointer = &team_overlay;
					}
				}
				if (!yt_sector_fighter_row(&displayed,
				    state->current_player_record, owner_pointer,
				    team_pointer, row, sizeof(row), &length,
				    scratch, sizeof(scratch), &scratch_length,
				    &scratch_changed, error)
				    || !spy_present(state, ops, context, row, length,
				    YT_SPY_LINE, error))
					return false;
			}
		}
		if (state->found_scratch != 0.0f) {
			if (!spy_present(state, ops, context, NULL, 0U,
			    YT_SPY_LINE, error)
			    || !ops->pause(context, state, error))
				return false;
		}
		if (!ops->read_sector(context, sector_number, &sector, error))
			return false;
		state->warp_destination_scratch = 0.0f;
		{
			int32_t warps[6];
			size_t slot;

			for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
				warps[slot] = qb_cint(sector.warps[slot], &overflow);
				if (overflow)
					return startup_configuration_error(error,
					    YT_RANGE, "active spy warp CINT");
			}
			for (;;) {
				float draw;
				int selected;

				if (!ops->random(context, &draw, error))
					return false;
				selected = (int)floorf(startup_single_multiply(
				    draw, 6.0f));
				if (selected < 0 || selected >= 6)
					return startup_configuration_error(error,
					    YT_RANGE, "active spy RND slot");
				state->warp_destination_scratch =
				    (float)warps[selected];
				if (warps[selected] != 0) {
					state->spy_sectors[spy] = warps[selected];
					break;
				}
			}
		}
		iterator = startup_single_add(iterator, 1.0f);
	}
	state->foreground = 0.0f;
	return true;
}

bool
yt_xannor_retaliation_run(struct yt_xannor_retaliation_state *state,
    const struct yt_xannor_retaliation_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_sector headquarters;
	int saved_record;
	float saved_cloak = 0.0f;
	int target_candidate;
	float target;
	int amount;
	int ignored_counterattack = 0;
	char amount_text[64];
	char target_text[64];
	char row[192];
	bool valid_cache;

	if (state == NULL || ops == NULL || state->player == NULL
	    || state->player_record == NULL || state->destroyed == NULL
	    || state->provoker == NULL || state->headquarters == NULL
	    || ops->read_sector == NULL || ops->random == NULL
	    || ops->present == NULL || ops->projectile == NULL
	    || ops->read_player == NULL || ops->wait == NULL)
		return false;

	if (*state->provoker == 0 && state->player->score < 25000000.0f)
		return true;
	if (!ops->read_sector(context, (int)*state->headquarters,
	    &headquarters, error))
		return false;
	if (headquarters.fighters == 0.0f
	    || headquarters.fighter_owner != -1.0f)
		return true;
	if (!ops->random(context, 3, 100, &amount, error)
	    || !ops->present(context, NULL, 0U, false, error))
		return false;

	saved_player = *state->player;
	saved_record = *state->player_record;
	valid_cache = saved_record >= 0
	    && (size_t)saved_record < state->cache_count
	    && state->cloak_cache != NULL;
	if (valid_cache) {
		saved_cloak = state->cloak_cache[saved_record];
		if (*state->provoker != 0)
			state->cloak_cache[saved_record] = 0.0f;
	}
	*state->player_record = -1;
	(void)snprintf(state->player->name, sizeof(state->player->name), "%s",
	    "The Xannor");

	if (!ops->random(context, 1, state->sector_count,
	    &target_candidate, error))
		return false;
	target = (float)target_candidate;
	if (*state->provoker != 0)
		target = saved_player.sector;
	if (qb_str_single(amount_text, sizeof(amount_text), (float)amount) < 0
	    || qb_str_single(target_text, sizeof(target_text), target) < 0
	    || snprintf(row, sizeof(row),
	    "The Xannor have launched%s missiles at sector%s!",
	    amount_text, target_text) < 0)
		return false;
	if (!ops->present(context, (const uint8_t *)row, strlen(row), true,
	    error)
	    || !ops->projectile(context, state->headquarters, target,
	    (float)amount, false, &ignored_counterattack, state->provoker,
	    error))
		return false;

	*state->player_record = saved_record;
	*state->player = saved_player;
	if (valid_cache)
		state->cloak_cache[saved_record] = saved_cloak;
	if (!ops->read_player(context, saved_record, state->player, error))
		return false;
	if (qb_mbf32_truth(state->player->record.bytes + YT_F45)) {
		*state->destroyed = true;
		if (saved_record >= 0
		    && (size_t)saved_record < state->cache_count
		    && state->sector_cache != NULL)
			state->sector_cache[saved_record] = 0.0f;
	}
	if (!ops->wait(context, 4.0, error))
		return false;
	*state->provoker = 0;
	return true;
}

bool
yt_projectile_target_prompt(bool plasma, float displayed, float maximum,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	const char *label = plasma ? " plasma bolt " : " cruise missile ";
	char displayed_text[64];
	char maximum_text[64];
	int written;

	if (prompt == NULL || length == NULL || capacity == 0U
	    || qb_str_single(displayed_text, sizeof(displayed_text), displayed)
	    < 0
	    || qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0)
		return false;
	written = snprintf((char *)prompt, capacity,
	    "You have%s. Send your%sto what sector? [ 1 to%s ] ?",
	    displayed_text, label, maximum_text);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

bool
yt_projectile_cruise_opening_run(float *last_mine_news_sector,
    const struct yt_projectile_cruise_opening_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t loading[] =
	    "Loading course into misile targeting computer.";
	static const uint8_t tracking[] = "*** Tracking Report ***";

	if (last_mine_news_sector == NULL || ops == NULL || ops->sound == NULL
	    || ops->present == NULL)
		return false;
	if (!ops->sound(context, 4.0f, error)
	    || !ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    || !ops->present(context, loading, sizeof(loading) - 1U,
	    YT_PROJECTILE_OPENING_RAW, error)
	    || !ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error))
		return false;
	*last_mine_news_sector = 0.0f;
	return ops->present(context, tracking, sizeof(tracking) - 1U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error);
}

bool
yt_projectile_plasma_opening_run(
    struct yt_projectile_plasma_opening_state *state,
    const struct yt_projectile_plasma_opening_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t mercenary[] = "The Mercenary";
	static const uint8_t loading[] =
	    "Loading course into targeting computer.";
	static const uint8_t tracking[] = "* Tracking Report *";
	char number[64];
	uint8_t row[192];
	const uint8_t *attacker;
	size_t attacker_length;
	int written;
	volatile double quotient;
	volatile float rounded;

	if (state == NULL || ops == NULL || ops->sound == NULL
	    || ops->present == NULL || ops->wait == NULL
	    || (state->player_name == NULL && state->player_name_length != 0U))
		return false;
	if (state->special_attacker != 0.0f) {
		attacker = mercenary;
		attacker_length = sizeof(mercenary) - 1U;
	}
	else {
		attacker = state->player_name;
		attacker_length = state->player_name_length;
	}
	if (attacker_length > sizeof(state->attacker))
		return false;
	if (attacker_length != 0U)
		memcpy(state->attacker, attacker, attacker_length);
	state->attacker_length = attacker_length;
	state->energy = (double)projectile_single_mul(2500000.0f,
	    state->bolts);
	quotient = state->energy / 50.0;
	rounded = (float)quotient;
	state->hop_loss = rounded;

	if (!ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    || !ops->present(context, loading, sizeof(loading) - 1U,
	    YT_PROJECTILE_OPENING_RAW, error)
	    || !ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    || !ops->sound(context, 4.0f, error)
	    || !ops->wait(context, 1.0f, error))
		return false;
	if (qb_str_double(number, sizeof(number), state->energy) < 0)
		return false;
	written = snprintf((char *)row, sizeof(row),
	    "Plasma bolts targeted... firing%s megawatts!", number);
	if (written < 0 || (size_t)written >= sizeof(row)
	    || !ops->present(context, row, (size_t)written,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    || !ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    || !ops->wait(context, 1.0f, error))
		return false;

	state->firing_counter = 1.0f;
	while (state->firing_counter <= state->bolts) {
		if (qb_str_single(number, sizeof(number), state->firing_counter) < 0)
			return false;
		written = snprintf((char *)row, sizeof(row), "Firing%s!", number);
		if (written < 0 || (size_t)written >= sizeof(row)
		    || !ops->present(context, row, (size_t)written,
		    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
		    || !ops->sound(context, 7.0f, error))
			return false;
		state->firing_counter = projectile_single_add(
		    state->firing_counter, 1.0f);
	}
	return ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    && ops->present(context, tracking, sizeof(tracking) - 1U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error)
	    && ops->present(context, NULL, 0U,
	    YT_PROJECTILE_OPENING_DIRECT_LINE, error);
}

bool
yt_projectile_plasma_route_run(
    struct yt_projectile_plasma_route_state *state,
    const struct yt_projectile_plasma_route_ops *ops, void *context,
    struct yt_error *error)
{
	char first[64];
	char second[64];
	uint8_t row[192];
	size_t steps = 0U;
	bool rerouted;

	if (state == NULL || ops == NULL || state->origin == NULL
	    || state->destination == NULL || state->energy == NULL
	    || state->route == NULL || state->route_capacity == 0U
	    || state->step_limit == 0U || ops->build_route == NULL
	    || ops->line == NULL || ops->attention == NULL
	    || ops->wait == NULL || ops->random == NULL
	    || ops->impact == NULL || ops->footer == NULL)
		return false;
	state->route_calls = 0U;
	state->hops = 0U;
	for (;;) {
		bool overflow;
		int destination_index;

		if (++steps > state->step_limit)
			return false;
		if (*state->destination == *state->origin) {
			destination_index = qb_cint(*state->destination, &overflow);
			if (overflow || destination_index < 0
			    || (size_t)destination_index >= state->route_capacity)
				return false;
			*state->origin = 0.0f;
			state->route[0] = (int16_t)destination_index;
			state->route[destination_index] = 0;
		}
		else {
			state->route_status = 0.0f;
			++state->route_calls;
			if (!ops->build_route(context, *state->origin,
			    *state->destination, state->route,
			    state->route_capacity, &state->route_status, error))
				return false;
		}
		state->current_hop = *state->origin;
		rerouted = false;
		for (;;) {
			enum yt_projectile_plasma_impact_route impact_route;
			int current_index;
			int next_hop;
			int written;

			if (++steps > state->step_limit)
				return false;
			if (state->current_hop != *state->origin)
				*state->energy -= (double)state->hop_loss;
			current_index = qb_cint(state->current_hop, &overflow);
			if (overflow || current_index < 0
			    || (size_t)current_index >= state->route_capacity)
				return false;
			next_hop = state->route[current_index];
			state->current_hop = (float)next_hop;
			if (next_hop == 0 || *state->energy < 1.0)
				return ops->footer(context, NULL, 0U, error);
			if (qb_str_single(first, sizeof(first), (float)next_hop) < 0
			    || qb_str_double(second, sizeof(second),
			    floor(*state->energy)) < 0)
				return false;
			written = snprintf((char *)row, sizeof(row),
			    "Bolt entering sector%s.%s Megawatts remaining.",
			    first, second);
			if (written < 0 || (size_t)written >= sizeof(row)
			    || !ops->line(context, row, (size_t)written, error)
			    || !ops->wait(context, 0.5f, error))
				return false;
			++state->hops;
			if ((float)next_hop == state->black_hole[0]
			    || (float)next_hop == state->black_hole[1]) {
				float draw;
				float span;

				*state->origin = (float)next_hop;
				if (!ops->random(context, &draw, error))
					return false;
				span = projectile_single_sub(state->port_record_offset,
				    state->sector_record_offset);
				*state->destination = floorf(projectile_single_add(
				    projectile_single_mul(draw, span), 1.0f));
				if (!ops->line(context, NULL, 0U, error)
				    || qb_str_single(first, sizeof(first),
				    (float)next_hop) < 0
				    || qb_str_single(second, sizeof(second),
				    *state->destination) < 0)
					return false;
				written = snprintf((char *)row, sizeof(row),
				    "The plasma bolt is deflected by a black hole in "
				    "sector%s to sector%s!", first, second);
				if (written < 0 || (size_t)written >= sizeof(row)
				    || !ops->attention(context, row, (size_t)written,
				    error)
				    || !ops->line(context, NULL, 0U, error))
					return false;
				rerouted = true;
				break;
			}
			impact_route = YT_PROJECTILE_PLASMA_NEXT_HOP;
			if (!ops->impact(context, next_hop, state->energy,
			    &impact_route, error))
				return false;
			if (impact_route == YT_PROJECTILE_PLASMA_FOOTER)
				return ops->footer(context, NULL, 0U, error);
			if (impact_route != YT_PROJECTILE_PLASMA_NEXT_HOP)
				return false;
		}
		if (!rerouted)
			return false;
	}
}

bool
yt_projectile_route_entry_run(struct yt_projectile_route_entry_state *state,
    yt_projectile_route_entry_read_player_fn read_player, void *context,
    struct yt_error *error)
{
	if (state == NULL || read_player == NULL)
		return false;
	state->current_hop = state->start;
	state->shooter_team = 0.0f;
	if ((float)state->shooter > 2.0f
	    && (float)state->shooter <= state->maximum_player_record) {
		struct yt_player shooter;

		if (!read_player(context, state->shooter, &shooter, error))
			return false;
		state->shooter_team = shooter.team;
	}
	if (state->shooter_team < 1.0f)
		state->shooter_team = -99999.0f;
	return true;
}

enum yt_projectile_target_result
yt_projectile_target_response(const char *response, float maximum,
    float *target)
{
	struct qb_val_result parsed;
	float candidate;

	if (response == NULL || target == NULL || response[0] == '\0')
		return YT_PROJECTILE_TARGET_CANCEL;
	parsed = qb_val(response);
	candidate = (float)(parsed.valid ? parsed.value : 0.0);
	if (candidate < 1.0f || candidate > maximum)
		return YT_PROJECTILE_TARGET_RETRY;
	*target = candidate;
	return YT_PROJECTILE_TARGET_ACCEPT;
}

float
yt_projectile_quantity_response(const char *response)
{
	struct qb_val_result parsed;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	return (float)floor(parsed.valid ? parsed.value : 0.0);
}

void
yt_projectile_debit_overlay(struct yt_player *player, bool plasma,
    float amount)
{
	volatile float remaining;
	size_t offset;

	if (player == NULL)
		return;
	if (plasma) {
		remaining = player->plasma - amount;
		player->plasma = remaining;
		offset = YT_F113;
	}
	else {
		remaining = player->missiles - amount;
		player->missiles = remaining;
		offset = YT_F97;
	}
	(void)yt_record_set_number(&player->record, offset, remaining);
}

bool
yt_projectile_commit(struct yt_game *game, int player_record,
    struct yt_player *player, bool plasma, float *origin, float target,
    float amount, bool *destroyed, int *counterattack, int *xannor_provoker,
    yt_projectile_resolver_fn resolver, void *resolver_context,
    struct yt_error *error)
{
	if (game == NULL || player == NULL || origin == NULL
	    || destroyed == NULL || counterattack == NULL
	    || xannor_provoker == NULL || resolver == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "projectile commit");
		}
		return false;
	}
	yt_projectile_debit_overlay(player, plasma, amount);
	if (!yt_game_write_player(game, player_record, player, error)
	    || !yt_database_flush(&game->database, error))
		return false;
	/* YT:22AC clears the fatal result only after the PUT completes. */
	*destroyed = false;
	return resolver(resolver_context, origin, target, amount, plasma,
	    counterattack, xannor_provoker, error);
}

float
yt_counterlaunch_score_count(double cached_score, float retained)
{
	static const uint8_t score_factor_raw[8] = {
		0x84, 0x47, 0x1b, 0x47, 0xac, 0xc5, 0x27, 0x70
	};
	volatile double product;
	volatile double integral;
	volatile double result;

	if (cached_score <= 0.0)
		return retained;
	product = cached_score * qb_mbf64_decode(score_factor_raw);
	integral = floor(product);
	result = integral + 1.0;
	return (float)result;
}

void
yt_counterlaunch_debit_overlay(struct yt_player *fresh_target,
    float first_available, float selected_count)
{
	volatile float remaining;

	if (fresh_target == NULL)
		return;
	remaining = first_available - selected_count;
	fresh_target->missiles = remaining;
	(void)yt_record_set_number(&fresh_target->record, YT_F97, remaining);
}

bool
yt_counterlaunch_rows(const uint8_t *target_name,
    size_t target_name_length, float selected_count,
    const uint8_t *saved_name, size_t saved_name_length,
    uint8_t *terminal, size_t terminal_capacity, size_t *terminal_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t middle[] = " shot back with";
	static const uint8_t terminal_suffix[] = " missiles at you!";
	static const uint8_t news_middle[] = " missiles at ";
	char number[64];
	int number_length;
	size_t terminal_needed;
	size_t news_needed;
	size_t position;

	if (terminal_length == NULL || news_length == NULL
	    || (target_name == NULL && target_name_length != 0U)
	    || (saved_name == NULL && saved_name_length != 0U))
		return false;
	*terminal_length = 0U;
	*news_length = 0U;
	number_length = qb_str_single(number, sizeof(number), selected_count);
	if (number_length < 0)
		return false;
	terminal_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(terminal_suffix) - 1U;
	news_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(news_middle) - 1U
	    + saved_name_length + 1U;
	if (terminal_needed > terminal_capacity || news_needed > news_capacity
	    || (terminal_needed != 0U && terminal == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	if (target_name_length != 0U)
		memcpy(terminal + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(terminal + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(terminal + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(terminal + position, terminal_suffix,
	    sizeof(terminal_suffix) - 1U);
	*terminal_length = terminal_needed;

	position = 0U;
	if (target_name_length != 0U)
		memcpy(news + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(news + position, news_middle, sizeof(news_middle) - 1U);
	position += sizeof(news_middle) - 1U;
	if (saved_name_length != 0U)
		memcpy(news + position, saved_name, saved_name_length);
	position += saved_name_length;
	news[position] = '!';
	*news_length = news_needed;
	return true;
}

bool
yt_counterlaunch_run(struct yt_counterlaunch_state *state,
    const struct yt_counterlaunch_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_player attacker;
	struct yt_player debit_player;
	struct yt_player final_player;
	int saved_record;
	float saved_cloak = 0.0f;
	float available;
	float target;
	float origin;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	size_t stored_name_length;
	size_t saved_name_length;
	uint8_t terminal_row[256];
	uint8_t news_row[256];
	size_t terminal_length;
	size_t news_length;
	char attacker_name[YT_TEXT_FIELD_SIZE + 1U];
	bool valid_cache;

	if (state == NULL || ops == NULL || state->player == NULL
	    || state->player_record == NULL || state->destroyed == NULL
	    || state->retained_count == NULL || state->counterattacker == NULL
	    || state->xannor_provoker == NULL || ops->read_player == NULL
	    || ops->random == NULL || ops->write_player == NULL
	    || ops->present == NULL || ops->append_news == NULL
	    || ops->projectile == NULL || ops->wait == NULL)
		return false;

	saved_record = *state->player_record;
	if (*state->counterattacker < 2
	    || *state->counterattacker > state->last_player_record
	    || *state->counterattacker == saved_record)
		return true;
	if (!ops->read_player(context, *state->counterattacker, &attacker,
	    error))
		return false;
	available = attacker.missiles;
	if (qb_mbf32_truth(attacker.record.bytes + YT_F45)
	    || available < 1.0f) {
		*state->counterattacker = 0;
		return true;
	}

	saved_player = *state->player;
	target = saved_player.sector;
	saved_name_length = strlen(saved_player.name);
	if (saved_name_length > sizeof(saved_name))
		saved_name_length = sizeof(saved_name);
	memcpy(saved_name, saved_player.name, saved_name_length);
	valid_cache = saved_record >= 0
	    && (size_t)saved_record < state->cache_count
	    && state->cloak_cache != NULL;
	if (valid_cache) {
		saved_cloak = state->cloak_cache[saved_record];
		state->cloak_cache[saved_record] = 0.0f;
	}
	*state->player_record = *state->counterattacker;
	if (!yt_player_stored_name(&attacker, stored_name,
	    &stored_name_length, error))
		return false;
	memset(attacker_name, 0, sizeof(attacker_name));
	memcpy(attacker_name, stored_name, stored_name_length);
	memcpy(state->player->name, attacker_name,
	    sizeof(state->player->name));

	*state->retained_count = yt_counterlaunch_score_count(
	    (double)saved_player.score, *state->retained_count);
	if (*state->retained_count > available
	    || *state->retained_count == 0.0f) {
		float draw;
		volatile float product;
		volatile float integral;
		volatile float selected;

		if (!ops->random(context, &draw, error))
			return false;
		product = draw * available;
		integral = floorf(product);
		selected = integral + 1.0f;
		*state->retained_count = selected;
	}
	if (!ops->read_player(context, *state->counterattacker,
	    &debit_player, error))
		return false;
	yt_counterlaunch_debit_overlay(&debit_player, available,
	    *state->retained_count);
	if (!ops->write_player(context, *state->counterattacker,
	    &debit_player, error)
	    || !ops->present(context, NULL, 0U, false, error)
	    || !yt_counterlaunch_rows(stored_name, stored_name_length,
	    *state->retained_count, saved_name, saved_name_length,
	    terminal_row, sizeof(terminal_row), &terminal_length, news_row,
	    sizeof(news_row), &news_length)
	    || !ops->present(context, terminal_row, terminal_length, true,
	    error)
	    || !ops->append_news(context, news_row, news_length, error))
		return false;
	origin = attacker.sector;
	if (!ops->projectile(context, &origin, target,
	    state->retained_count, false, state->counterattacker,
	    state->xannor_provoker, error))
		return false;

	*state->counterattacker = 0;
	*state->player_record = saved_record;
	*state->player = saved_player;
	if (valid_cache)
		state->cloak_cache[saved_record] = saved_cloak;
	if (!ops->read_player(context, saved_record, &final_player, error))
		return false;
	if (qb_mbf32_truth(final_player.record.bytes + YT_F45)) {
		*state->destroyed = true;
		if (saved_record >= 0
		    && (size_t)saved_record < state->cache_count
		    && state->sector_cache != NULL)
			state->sector_cache[saved_record] = 0.0f;
	}
	return ops->wait(context, 4.0, error);
}

static float
salvage_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
salvage_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

bool
yt_salvage_cargo_sample(struct yt_salvage_cargo_state *state,
    yt_salvage_cargo_draw_fn draw, void *context, struct yt_error *error)
{
	float counter;

	if (state == NULL || draw == NULL)
		return false;
	memset(state->awards, 0, sizeof(state->awards));
	for (counter = 1.0f; counter <= state->requested;
	    counter = salvage_single_add(counter, 1.0f)) {
		float one_based;
		float pick;
		float boundary;
		int selected;

		if (!draw(context, state->remaining, &one_based, error))
			return false;
		pick = salvage_single_sub(one_based, 1.0f);
		if (pick < state->stock[0])
			selected = 0;
		else {
			boundary = salvage_single_add(state->stock[0],
			    state->stock[1]);
			if (pick < boundary)
				selected = 1;
			else {
				boundary = salvage_single_add(boundary,
				    state->stock[2]);
				selected = pick < boundary ? 2 : 3;
			}
		}
		state->awards[selected] = salvage_single_add(
		    state->awards[selected], 1.0f);
		if (selected < 3)
			state->stock[selected] = salvage_single_sub(
			    state->stock[selected], 1.0f);
		state->remaining = salvage_single_sub(state->remaining, 1.0f);
	}
	return true;
}

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
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, salvor, salvor_length)
	    || !salvage_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !salvage_append(row, capacity, &position, victim, victim_length)
	    || !salvage_append(row, capacity, &position, suffix,
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
	if (number_length < 0
	    || !salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, labels[kind],
	    strlen(labels[kind]))
	    || !salvage_append(row, capacity, &position, number,
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
	if (number_length < 0
	    || !salvage_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !salvage_append(row, capacity, &position, number,
	    (size_t)number_length)
	    || !salvage_append(row, capacity, &position, suffix[kind],
	    strlen(suffix[kind])))
		return false;
	*length = position;
	return true;
}

bool
yt_current_player_hydrate_run(
    struct yt_current_player_hydration_state *state,
    yt_current_player_read_fn read_player, void *context,
    struct yt_error *error)
{
	struct yt_player fresh;
	volatile float current_sector;
	int record;

	if (state == NULL || state->player == NULL || read_player == NULL
	    || state->current_sector_record == NULL)
		return false;
	record = state->player_record;
	if (record < 2 || record > state->last_player_record) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "current player hydration record");
		}
		return false;
	}
	if (!read_player(context, record, &fresh, error))
		return false;

	state->player->record = fresh.record;
	state->player->sector = fresh.sector;
	state->player->fighters = fresh.fighters;
	current_sector = state->sector_record_offset + fresh.sector;
	*state->current_sector_record = current_sector;
	state->player->turns = fresh.turns;
	state->player->credits = fresh.credits;
	state->player->danger_scanner = fresh.danger_scanner;
	state->player->missiles = fresh.missiles;
	state->player->mines = fresh.mines;
	state->player->team = fresh.team;
	state->player->holds = fresh.holds;
	state->player->ore = fresh.ore;
	state->player->organics = fresh.organics;
	state->player->equipment = fresh.equipment;
	state->player->plasma = fresh.plasma;
	state->player->score = fresh.score;
	state->player->ports_owned = fresh.ports_owned;
	state->player->ground_forces = fresh.ground_forces;
	state->player->cloak = fresh.cloak;
	if (record >= 0 && (size_t)record < state->cache_count) {
		if (state->sector_cache != NULL)
			state->sector_cache[record] = fresh.sector;
		if (!state->anti_cloak && state->cloak_cache != NULL)
			state->cloak_cache[record] = fresh.cloak;
	}
	state->player->shields = fresh.shields;
	return true;
}

void
yt_player_decode(struct yt_player *player, const struct yt_record *record)
{
	memset(player, 0, sizeof(*player));
	player->record = *record;
	yt_record_get_text(record, player->name, sizeof(player->name));
	player->last_active = yt_record_get_number(record, YT_F41);
	player->killed_by = yt_record_get_number(record, YT_F45);
	player->turns = yt_record_get_number(record, YT_F49);
	player->shields = yt_record_get_number(record, YT_F53);
	player->sector = yt_record_get_number(record, YT_F57);
	player->fighters = yt_record_get_number(record, YT_F61);
	player->holds = yt_record_get_number(record, YT_F65);
	player->ore = yt_record_get_number(record, YT_F69);
	player->organics = yt_record_get_number(record, YT_F73);
	player->equipment = yt_record_get_number(record, YT_F77);
	player->credits = yt_record_get_number(record, YT_F81);
	player->name_length = yt_record_get_number(record, YT_F85);
	player->team = yt_record_get_number(record, YT_F89);
	player->danger_scanner = yt_record_get_number(record, YT_F93);
	player->missiles = yt_record_get_number(record, YT_F97);
	player->lottery_plays = yt_record_get_number(record, YT_F105);
	player->score = yt_record_get_number(record, YT_F109);
	player->plasma = yt_record_get_number(record, YT_F113);
	player->ports_owned = yt_record_get_number(record, YT_F117);
	player->ground_forces = yt_record_get_number(record, YT_F121);
	player->cloak = yt_record_get_number(record, YT_F125);
	player->mines = yt_record_get_number(record, YT_F129);
}

void
yt_player_encode(struct yt_player *player)
{
	yt_record_set_text_if_changed(&player->record,
	    (const uint8_t *)player->name,
	    strlen(player->name));
	yt_record_set_number_if_changed(&player->record, YT_F41,
	    player->last_active);
	yt_record_set_number_if_changed(&player->record, YT_F45,
	    player->killed_by);
	yt_record_set_number_if_changed(&player->record, YT_F49, player->turns);
	yt_record_set_number_if_changed(&player->record, YT_F53, player->shields);
	yt_record_set_number_if_changed(&player->record, YT_F57, player->sector);
	yt_record_set_number_if_changed(&player->record, YT_F61, player->fighters);
	yt_record_set_number_if_changed(&player->record, YT_F65, player->holds);
	yt_record_set_number_if_changed(&player->record, YT_F69, player->ore);
	yt_record_set_number_if_changed(&player->record, YT_F73,
	    player->organics);
	yt_record_set_number_if_changed(&player->record, YT_F77,
	    player->equipment);
	yt_record_set_number_if_changed(&player->record, YT_F81, player->credits);
	yt_record_set_number_if_changed(&player->record, YT_F85,
	    player->name_length);
	yt_record_set_number_if_changed(&player->record, YT_F89, player->team);
	yt_record_set_number_if_changed(&player->record, YT_F93,
	    player->danger_scanner);
	yt_record_set_number_if_changed(&player->record, YT_F97,
	    player->missiles);
	yt_record_set_number_if_changed(&player->record, YT_F105,
	    player->lottery_plays);
	yt_record_set_number_if_changed(&player->record, YT_F109, player->score);
	yt_record_set_number_if_changed(&player->record, YT_F113, player->plasma);
	yt_record_set_number_if_changed(&player->record, YT_F117,
	    player->ports_owned);
	yt_record_set_number_if_changed(&player->record, YT_F121,
	    player->ground_forces);
	yt_record_set_number_if_changed(&player->record, YT_F125, player->cloak);
	yt_record_set_number_if_changed(&player->record, YT_F129, player->mines);
}

void
yt_sector_decode(struct yt_sector *sector, const struct yt_record *record)
{
	size_t index;

	memset(sector, 0, sizeof(*sector));
	sector->record = *record;
	for (index = 0; index < 6; ++index)
		sector->warps[index] = yt_record_get_number(record,
		    YT_F41 + index * 4U);
	sector->port = yt_record_get_number(record, YT_F65);
	sector->fighters = yt_record_get_number(record, YT_F81);
	sector->fighter_owner = yt_record_get_number(record, YT_F85);
	sector->planet = yt_record_get_number(record, YT_F93);
	sector->metadata = yt_record_get_number(record, YT_F105);
	sector->mines = yt_record_get_number(record, YT_F129);
}

void
yt_sector_encode(struct yt_sector *sector)
{
	size_t index;

	for (index = 0; index < 6; ++index)
		yt_record_set_number_if_changed(&sector->record,
		    YT_F41 + index * 4U,
		    sector->warps[index]);
	yt_record_set_number_if_changed(&sector->record, YT_F65, sector->port);
	yt_record_set_number_if_changed(&sector->record, YT_F81,
	    sector->fighters);
	yt_record_set_number_if_changed(&sector->record, YT_F85,
	    sector->fighter_owner);
	yt_record_set_number_if_changed(&sector->record, YT_F93, sector->planet);
	yt_record_set_number_if_changed(&sector->record, YT_F105,
	    sector->metadata);
	yt_record_set_number_if_changed(&sector->record, YT_F129, sector->mines);
}

void
yt_port_decode(struct yt_port *port, const struct yt_record *record)
{
	size_t index;

	memset(port, 0, sizeof(*port));
	port->record = *record;
	yt_record_get_text(record, port->name, sizeof(port->name));
	port->commodity_class = yt_record_get_number(record, YT_F41);
	port->last_day = yt_record_get_number(record, YT_F45);
	for (index = 0; index < 3; ++index) {
		port->stock[index] = yt_record_get_number(record, YT_F49 + index * 4U);
		port->production[index] =
		    yt_record_get_number(record, YT_F61 + index * 4U);
		port->factor[index] = yt_record_get_number(record, YT_F73 + index * 4U);
	}
	port->name_length = yt_record_get_number(record, YT_F85);
	port->treasury = yt_record_get_number(record, YT_F89);
	port->sector = yt_record_get_number(record, YT_F93);
	port->owner = yt_record_get_number(record, YT_F97);
	port->last_minute = yt_record_get_number(record, YT_F101);
}

void
yt_port_encode(struct yt_port *port)
{
	size_t index;

	yt_record_set_text_if_changed(&port->record, (const uint8_t *)port->name,
	    strlen(port->name));
	yt_record_set_number_if_changed(&port->record, YT_F41,
	    port->commodity_class);
	yt_record_set_number_if_changed(&port->record, YT_F45, port->last_day);
	for (index = 0; index < 3; ++index) {
		yt_record_set_number_if_changed(&port->record,
		    YT_F49 + index * 4U,
		    port->stock[index]);
		yt_record_set_number_if_changed(&port->record,
		    YT_F61 + index * 4U,
		    port->production[index]);
		yt_record_set_number_if_changed(&port->record,
		    YT_F73 + index * 4U,
		    port->factor[index]);
	}
	yt_record_set_number_if_changed(&port->record, YT_F85,
	    port->name_length);
	yt_record_set_number_if_changed(&port->record, YT_F89, port->treasury);
	yt_record_set_number_if_changed(&port->record, YT_F93, port->sector);
	yt_record_set_number_if_changed(&port->record, YT_F97, port->owner);
	yt_record_set_number_if_changed(&port->record, YT_F101,
	    port->last_minute);
}

void
yt_planet_decode(struct yt_planet *planet, const struct yt_record *record)
{
	size_t index;

	memset(planet, 0, sizeof(*planet));
	planet->record = *record;
	yt_record_get_text(record, planet->name, sizeof(planet->name));
	planet->last_day = yt_record_get_number(record, YT_F41);
	for (index = 0; index < 3; ++index) {
		planet->production[index] =
		    yt_record_get_number(record, YT_F45 + index * 4U);
		planet->stock[index] = yt_record_get_number(record, YT_F57 + index * 4U);
	}
	planet->missiles = yt_record_get_number(record, YT_F69);
	planet->owner = yt_record_get_number(record, YT_F73);
	planet->ground_forces = yt_record_get_number(record, YT_F77);
	planet->name_length = yt_record_get_number(record, YT_F85);
	planet->last_minute = yt_record_get_number(record, YT_F89);
	planet->plasma = yt_record_get_number(record, YT_F113);
	planet->bank = yt_record_get_number(record, YT_F117);
	planet->mines = yt_record_get_number(record, YT_F125);
	planet->fighters = yt_record_get_number(record, YT_F129);
}

void
yt_planet_encode(struct yt_planet *planet)
{
	size_t index;

	yt_record_set_text_if_changed(&planet->record,
	    (const uint8_t *)planet->name,
	    strlen(planet->name));
	yt_record_set_number_if_changed(&planet->record, YT_F41,
	    planet->last_day);
	for (index = 0; index < 3; ++index) {
		yt_record_set_number_if_changed(&planet->record,
		    YT_F45 + index * 4U,
		    planet->production[index]);
		yt_record_set_number_if_changed(&planet->record,
		    YT_F57 + index * 4U,
		    planet->stock[index]);
	}
	yt_record_set_number_if_changed(&planet->record, YT_F69,
	    planet->missiles);
	yt_record_set_number_if_changed(&planet->record, YT_F73, planet->owner);
	yt_record_set_number_if_changed(&planet->record, YT_F77,
	    planet->ground_forces);
	yt_record_set_number_if_changed(&planet->record, YT_F85,
	    planet->name_length);
	yt_record_set_number_if_changed(&planet->record, YT_F89,
	    planet->last_minute);
	yt_record_set_number_if_changed(&planet->record, YT_F113,
	    planet->plasma);
	yt_record_set_number_if_changed(&planet->record, YT_F117, planet->bank);
	yt_record_set_number_if_changed(&planet->record, YT_F125, planet->mines);
	yt_record_set_number_if_changed(&planet->record, YT_F129,
	    planet->fighters);
}

bool
yt_game_open(struct yt_game *game, enum yt_open_mode mode,
    struct yt_error *error)
{
	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	if (!yt_database_open(&game->database, "YTDATA.DAT", mode, error)
	    || !yt_config_load(&game->database, &game->config, error)) {
		yt_game_close(game);
		return false;
	}
	return yt_current_date_serial(game->config.epoch_year, &game->today,
	    &game->adjusted_year, error);
}

void
yt_game_close(struct yt_game *game)
{
	yt_database_close(&game->database);
}

bool
yt_game_read_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database, (size_t)basic_record, &record, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

bool
yt_game_write_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error)
{
	yt_player_encode(player);
	return yt_database_write(&game->database, (size_t)basic_record,
	    &player->record, error);
}

bool
yt_game_read_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical_sector),
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

bool
yt_game_write_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	return yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical_sector),
	    &sector->record, error);
}

bool
yt_game_read_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_port_basic_record(&game->config, logical_port),
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

bool
yt_game_write_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&game->database,
	    (size_t)yt_port_basic_record(&game->config, logical_port),
	    &port->record, error);
}

bool
yt_game_read_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical_planet),
	    &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
yt_game_write_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	yt_planet_encode(planet);
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical_planet),
	    &planet->record, error);
}

bool
yt_game_construct_player(struct yt_game *game, int basic_record, float today,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record config_record;
	struct yt_config config;

	if (!yt_database_read(&game->database, 1, &config_record, error))
		return false;
	memset(&config, 0, sizeof(config));
	config.turns_per_day = yt_record_get_number(&config_record, YT_F49);
	config.initial_fighters = yt_record_get_number(&config_record, YT_F65);
	config.initial_credits = yt_record_get_number(&config_record, YT_F69);
	config.initial_holds = yt_record_get_number(&config_record, YT_F73);
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	yt_player_construct(player, &config, today);
	return yt_game_write_player(game, basic_record, player, error);
}

bool
yt_game_set_player_identity(struct yt_game *game, int basic_record,
    const uint8_t *name, size_t length, struct yt_player *player,
    struct yt_error *error)
{
	size_t copied;

	if ((name == NULL && length != 0)
	    || !yt_game_read_player(game, basic_record, player, error))
		return false;
	copied = length < YT_TEXT_FIELD_SIZE ? length : YT_TEXT_FIELD_SIZE;
	if (copied > 0)
		memcpy(player->name, name, copied);
	player->name[copied] = '\0';
	player->name_length = (float)length;
	player->team = 0.0f;
	return yt_game_write_player(game, basic_record, player, error);
}

bool
yt_game_post_login_repairs(struct yt_game *game, int basic_record,
    float maximum_holds, struct yt_player *player,
    struct yt_post_login_repairs *repairs, struct yt_error *error)
{
	struct yt_post_login_repairs applied = {false, false, 0};

	if (repairs != NULL)
		*repairs = applied;
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	if (player->turns < 1.0f) {
		player->turns = 1.0f;
		applied.turns = true;
		if (!yt_game_write_player(game, basic_record, player, error)
		    || !yt_database_flush(&game->database, error)) {
			if (repairs != NULL)
				*repairs = applied;
			return false;
		}
		applied.writes++;
	}
	if (!yt_game_read_player(game, basic_record, player, error)) {
		if (repairs != NULL)
			*repairs = applied;
		return false;
	}
	if (player->holds > maximum_holds) {
		player->ore = 0.0f;
		player->organics = 0.0f;
		player->equipment = maximum_holds;
		player->holds = maximum_holds;
		applied.holds = true;
		if (!yt_game_write_player(game, basic_record, player, error)
		    || !yt_database_flush(&game->database, error)) {
			if (repairs != NULL)
				*repairs = applied;
			return false;
		}
		applied.writes++;
	}
	if (repairs != NULL)
		*repairs = applied;
	return true;
}

bool
yt_sector_force_route(float fighters, float owner, int current_player_record,
    enum yt_sector_force_route *route, int *owner_record,
    struct yt_error *error)
{
	if (route == NULL || owner_record == NULL)
		return false;
	*owner_record = 0;
	if (fighters == 0.0f || owner == (float)current_player_record) {
		*route = YT_SECTOR_FORCE_FRIENDLY;
		return true;
	}
	if (owner <= 0.0f) {
		*route = YT_SECTOR_FORCE_HOSTILE;
		return true;
	}
	if (!isfinite(owner) || owner != floorf(owner)
	    || owner > (float)INT_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "fighter owner record");
		}
		return false;
	}
	*route = YT_SECTOR_FORCE_OWNER_GET;
	*owner_record = (int)owner;
	return true;
}

bool
yt_sector_is_black_hole(float current_sector, float first, float second)
{
	return current_sector == first || current_sector == second;
}

bool
yt_sector_mines_admitted(float mines, float suppression)
{
	return mines > 0.0f && suppression == 0.0f;
}

bool
yt_sector_force_same_team(float current_team, float owner_team)
{
	return current_team != 0.0f && owner_team == current_team;
}

bool
yt_friendship_resolve(float candidate_record,
    float current_player_record, float last_player_record,
    yt_friendship_reader_fn reader, void *reader_context, bool *friendly,
    struct yt_error *error)
{
	struct yt_player current;
	struct yt_player candidate;

	if (friendly == NULL)
		return false;
	*friendly = false;
	if (!isfinite(candidate_record) || !isfinite(current_player_record)
	    || !isfinite(last_player_record)
	    || candidate_record < 2.0f
	    || candidate_record > last_player_record
	    || current_player_record < 2.0f
	    || current_player_record > last_player_record)
		return true;
	if (candidate_record == current_player_record) {
		*friendly = true;
		return true;
	}
	if (reader == NULL)
		return false;
	if (!reader(reader_context, (int)floorf(current_player_record),
	    &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!reader(reader_context, (int)floorf(candidate_record),
	    &candidate, error))
		return false;
	*friendly = candidate.team == current.team;
	return true;
}

enum yt_port_owner_kind
yt_port_owner_classify(float owner, int current_player_record,
    int *owner_record)
{
	uint32_t record;

	if (owner_record != NULL)
		*owner_record = 0;
	if (owner <= 1.0f)
		return YT_PORT_OWNER_SILENT;
	if (owner == (float)current_player_record)
		return YT_PORT_OWNER_SELF;
	if (!isfinite(owner))
		return YT_PORT_OWNER_INVALID;
	record = qb_brun_random_record_number(owner);
	if (record > (uint32_t)INT_MAX)
		return YT_PORT_OWNER_INVALID;
	if (owner_record != NULL)
		*owner_record = (int)record;
	return YT_PORT_OWNER_OTHER;
}

bool
yt_port_owner_compose(enum yt_port_owner_kind kind, float treasury,
    const uint8_t *owner_name, size_t owner_name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is owned by: ";
	static const uint8_t self[] = "YOU, Credits:";
	char treasury_text[80];
	const uint8_t *suffix;
	size_t suffix_length;
	size_t needed;
	int formatted_length;

	if (length == NULL)
		return false;
	*length = 0U;
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_SELF) {
		formatted_length = qb_str_double(treasury_text,
		    sizeof(treasury_text), (double)treasury);
		if (formatted_length < 0)
			return false;
		needed = sizeof(prefix) - 1U + sizeof(self) - 1U
		    + (size_t)formatted_length;
		if (needed > capacity || (needed != 0U && row == NULL))
			return false;
		memcpy(row, prefix, sizeof(prefix) - 1U);
		memcpy(row + sizeof(prefix) - 1U, self, sizeof(self) - 1U);
		memcpy(row + sizeof(prefix) - 1U + sizeof(self) - 1U,
		    treasury_text, (size_t)formatted_length);
		*length = needed;
		return true;
	}
	if (kind != YT_PORT_OWNER_OTHER
	    || (owner_name == NULL && owner_name_length != 0U))
		return false;
	suffix = owner_name;
	suffix_length = owner_name_length;
	needed = sizeof(prefix) - 1U + suffix_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (suffix_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, suffix, suffix_length);
	*length = needed;
	return true;
}

bool
yt_hostile_menu_row(double ship_fighters, double deployed_fighters,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Fighters:";
	static const uint8_t separator[] = " /";
	char ship[64];
	char deployed[64];
	int ship_length;
	int deployed_length;
	size_t needed;
	size_t position = 0;

	if (length == NULL)
		return false;
	*length = 0;
	ship_length = qb_str_double(ship, sizeof(ship), ship_fighters);
	deployed_length = qb_str_double(deployed, sizeof(deployed),
	    deployed_fighters);
	if (ship_length < 0 || deployed_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)ship_length
	    + sizeof(separator) - 1U + (size_t)deployed_length;
	if (needed > capacity || (needed != 0 && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, ship, (size_t)ship_length);
	position += (size_t)ship_length;
	memcpy(row + position, separator, sizeof(separator) - 1U);
	position += sizeof(separator) - 1U;
	memcpy(row + position, deployed, (size_t)deployed_length);
	position += (size_t)deployed_length;
	*length = position;
	return true;
}

enum yt_hostile_menu_route
yt_hostile_menu_dispatch(const char *response)
{
	static const char dispatch[] = "AQBDWT";
	const char *position;

	if (response == NULL || response[0] == '\0'
	    || strcmp(response, "?") == 0)
		return YT_HOSTILE_MENU_HELP;
	if (strcmp(response, "S") == 0)
		return YT_HOSTILE_MENU_SECTOR;
	if (strcmp(response, "I") == 0)
		return YT_HOSTILE_MENU_INFO;
	position = strstr(dispatch, response);
	if (position == NULL)
		return YT_HOSTILE_MENU_INVALID;
	switch (position - dispatch) {
	case 0:
		return YT_HOSTILE_MENU_ATTACK;
	case 1:
		return YT_HOSTILE_MENU_QUIT;
	case 2:
		return YT_HOSTILE_MENU_BRIBE;
	case 3:
		return YT_HOSTILE_MENU_MINE;
	case 4:
		return YT_HOSTILE_MENU_WARP;
	case 5:
		return YT_HOSTILE_MENU_TEAM;
	default:
		return YT_HOSTILE_MENU_INVALID;
	}
}

enum yt_main_shell_route
yt_main_shell_dispatch(const char *response)
{
	static const char dispatch[] = "W)+ABCFLMPQTD$GN";
	static const enum yt_main_shell_route routes[] = {
		YT_MAIN_SHELL_WARP,
		YT_MAIN_SHELL_MISSILE,
		YT_MAIN_SHELL_PLASMA,
		YT_MAIN_SHELL_ATTACK,
		YT_MAIN_SHELL_BUY_PORT,
		YT_MAIN_SHELL_COMPUTER,
		YT_MAIN_SHELL_FIGHTERS,
		YT_MAIN_SHELL_LAND,
		YT_MAIN_SHELL_MOVE,
		YT_MAIN_SHELL_TRADE,
		YT_MAIN_SHELL_QUIT,
		YT_MAIN_SHELL_TEAM,
		YT_MAIN_SHELL_MINES,
		YT_MAIN_SHELL_COLLECT,
		YT_MAIN_SHELL_GENESIS,
		YT_MAIN_SHELL_RENAME_PORT,
	};
	const char *position;

	if (response == NULL || response[0] == '\0')
		return YT_MAIN_SHELL_DISPLAY;
	if (strcmp(response, "X") == 0)
		return YT_MAIN_SHELL_SOUND;
	if (strcmp(response, "S") == 0)
		return YT_MAIN_SHELL_SENSORS;
	position = strchr(dispatch, response[0]);
	if (position != NULL)
		return routes[position - dispatch];
	switch (response[0]) {
	case 'V':
		return YT_MAIN_SHELL_VERSION;
	case 'I':
		return YT_MAIN_SHELL_INFO;
	case 'Z':
		return YT_MAIN_SHELL_INSTRUCTIONS;
	case '?':
		return YT_MAIN_SHELL_HELP;
	default:
		return YT_MAIN_SHELL_INVALID;
	}
}

enum yt_hostile_attack_admission
yt_hostile_attack_admit(float ship_fighters, float commitment)
{
	if (ship_fighters < 1.0f)
		return YT_HOSTILE_ATTACK_NO_FIGHTERS;
	if (commitment > ship_fighters)
		return YT_HOSTILE_ATTACK_TOO_MANY;
	if (commitment < 1.0f)
		return YT_HOSTILE_ATTACK_LESS_THAN_ONE;
	return YT_HOSTILE_ATTACK_ADMITTED;
}

float
yt_hostile_attack_quantum(double remaining_attacker,
    double remaining_defender)
{
	double minimum = remaining_attacker < remaining_defender
	    ? remaining_attacker : remaining_defender;
	volatile double divided = minimum / 20.0;
	float quantum = (float)qb_int(divided);

	return quantum < 1.0f ? 1.0f : quantum;
}

bool
yt_hostile_attack_loses_attacker(float cloak, float draw)
{
	volatile float cloak_term = cloak / 10.0f;
	volatile float total = cloak_term + draw;

	return total < 0.44999998807907104f;
}

enum yt_hostile_surrender_route
yt_hostile_surrender_route(float owner)
{
	if (owner > 1.0f)
		return YT_HOSTILE_SURRENDER_PLAYER;
	if (owner == -1.0f)
		return YT_HOSTILE_SURRENDER_XANNOR;
	if (owner == -2.0f)
		return YT_HOSTILE_SURRENDER_MERCENARY;
	return YT_HOSTILE_SURRENDER_QUIET;
}

bool
yt_fighter_shield_spill_step(double *fighters, float *shields, float draw)
{
	float quantum;

	if (fighters == NULL || shields == NULL
	    || *fighters <= 0.0 || *shields <= 0.0f)
		return false;
	quantum = *fighters > 100.0 && *shields > 100.0f ? 100.0f : 1.0f;
	if (draw >= 0.5f) {
		volatile double reduced = *fighters - (double)quantum;

		*fighters = reduced;
	}
	else {
		volatile float reduced = *shields - quantum;

		*shields = reduced;
	}
	return true;
}

bool
yt_fighter_shield_spill_rows(double fighters, float shields,
    uint8_t *fighter_row, size_t fighter_capacity, size_t *fighter_length,
    uint8_t *shield_row, size_t shield_capacity, size_t *shield_length)
{
	static const char fighter_prefix[] = "Fighters remaining:";
	static const char shield_prefix[] = "Shields reduced to:";
	char fighter_number[64];
	char shield_number[64];
	int fighter_number_length;
	int shield_number_length;
	size_t first_needed;
	size_t second_needed;

	if (fighter_length == NULL || shield_length == NULL)
		return false;
	*fighter_length = 0;
	*shield_length = 0;
	fighter_number_length = qb_str_double(fighter_number,
	    sizeof(fighter_number), fighters);
	shield_number_length = qb_str_single(shield_number,
	    sizeof(shield_number), shields);
	if (fighter_number_length < 0 || shield_number_length < 0)
		return false;
	first_needed = sizeof(fighter_prefix) - 1U
	    + (size_t)fighter_number_length;
	second_needed = sizeof(shield_prefix) - 1U
	    + (size_t)shield_number_length;
	if (first_needed > fighter_capacity || second_needed > shield_capacity
	    || (first_needed != 0 && fighter_row == NULL)
	    || (second_needed != 0 && shield_row == NULL))
		return false;
	memcpy(fighter_row, fighter_prefix, sizeof(fighter_prefix) - 1U);
	memcpy(fighter_row + sizeof(fighter_prefix) - 1U, fighter_number,
	    (size_t)fighter_number_length);
	memcpy(shield_row, shield_prefix, sizeof(shield_prefix) - 1U);
	memcpy(shield_row + sizeof(shield_prefix) - 1U, shield_number,
	    (size_t)shield_number_length);
	*fighter_length = first_needed;
	*shield_length = second_needed;
	return true;
}

bool
yt_hostile_defeated_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const char prefix[] =
	    "You defeated all the fighters and have";
	static const char suffix[] = " left.";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0 && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_xannor_attack_reward_rows(const uint8_t *name, size_t name_length,
    float bonus, double defenders_destroyed,
    uint8_t *display, size_t display_capacity, size_t *display_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t collect[] = "Collect";
	static const uint8_t collected[] = " collected";
	static const uint8_t middle[] = " turns bonus for destroying";
	static const uint8_t suffix[] = " Xannor!!";
	char bonus_text[64];
	char loss_text[64];
	int bonus_length;
	int loss_length;
	size_t clause_length;
	size_t display_needed;
	size_t news_needed;

	if (display_length == NULL || news_length == NULL
	    || (name == NULL && name_length != 0U))
		return false;
	*display_length = 0U;
	*news_length = 0U;
	bonus_length = qb_str_single(bonus_text, sizeof(bonus_text), bonus);
	loss_length = qb_str_double(loss_text, sizeof(loss_text),
	    defenders_destroyed);
	if (bonus_length < 0 || loss_length < 0)
		return false;
	clause_length = (size_t)bonus_length + sizeof(middle) - 1U
	    + (size_t)loss_length + sizeof(suffix) - 1U;
	display_needed = sizeof(collect) - 1U + clause_length;
	news_needed = name_length + sizeof(collected) - 1U + clause_length;
	if (display_needed > display_capacity || news_needed > news_capacity
	    || (display_needed != 0U && display == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	memcpy(display, collect, sizeof(collect) - 1U);
	memcpy(display + sizeof(collect) - 1U, bonus_text,
	    (size_t)bonus_length);
	memcpy(display + sizeof(collect) - 1U + (size_t)bonus_length,
	    middle, sizeof(middle) - 1U);
	memcpy(display + sizeof(collect) - 1U + (size_t)bonus_length
	    + sizeof(middle) - 1U, loss_text, (size_t)loss_length);
	memcpy(display + display_needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	if (name_length != 0U)
		memcpy(news, name, name_length);
	memcpy(news + name_length, collected, sizeof(collected) - 1U);
	memcpy(news + name_length + sizeof(collected) - 1U,
	    display + sizeof(collect) - 1U, clause_length);
	*display_length = display_needed;
	*news_length = news_needed;
	return true;
}

float
yt_xannor_attack_bonus(double defenders_destroyed, float turns,
    float turns_per_day)
{
	volatile double quotient = defenders_destroyed / 256000.0;
	float bonus = (float)qb_int(quotient);
	volatile float sum = turns + bonus;

	if (sum > turns_per_day) {
		volatile float clamped = turns_per_day - turns;

		bonus = clamped;
	}
	return bonus;
}

bool
yt_bribe_ordinary_forces(float owner, float defenders,
    float ship_fighters, float draw)
{
	return owner == -1.0f || (defenders > ship_fighters
	    && draw < 0.33000001311302185f);
}

bool
yt_bribe_mercenary_forces(float defenders, float ship_fighters,
    float first, float second, bool sticky)
{
	return first < 0.05000000074505806f
	    || (ship_fighters < defenders
	    && second > 0.8999999761581421f) || sticky;
}

double
yt_bribe_offer_threshold(float defenders, float draw)
{
	volatile double product = (double)defenders * (double)draw;
	volatile double doubled = product * 2.0;
	volatile double threshold = doubled + (double)defenders;

	return threshold;
}

bool
yt_bribe_offer_accepted(float offer, float credits, double threshold)
{
	return (double)offer <= (double)credits && (double)offer >= threshold;
}

enum yt_bribe_forced_admission
yt_bribe_forced_admit(double ship_fighters, float shields,
    bool mercenary_fatal_gate, float commitment)
{
	if (mercenary_fatal_gate && ship_fighters < 1.0 && shields < 1.0f)
		return YT_BRIBE_FORCED_FATAL;
	if (commitment < 1.0f)
		return YT_BRIBE_FORCED_LESS_THAN_ONE;
	return YT_BRIBE_FORCED_ATTACK;
}

enum yt_sector_mine_admission
yt_sector_mine_admit(float carried, float amount)
{
	if (amount < 1.0f)
		return YT_SECTOR_MINE_BELOW_ONE;
	if (amount > carried)
		return YT_SECTOR_MINE_ABOVE_CARRIED;
	return YT_SECTOR_MINE_ACCEPTED;
}

bool
yt_no_turn_gate_denied(float turns)
{
	return turns <= 0.0f;
}

bool
yt_team_choice_rejected(float choice, float raw_team,
    int32_t captain_cint, int32_t team_cint)
{
	return (choice > 3.0f && raw_team == 0.0f)
	    || (choice > 6.0f && captain_cint != -1)
	    || (choice > 1.0f && choice < 4.0f && team_cint != 0)
	    || choice < 1.0f || choice > 10.0f;
}

void
yt_team_transfer_apply_sector(struct yt_sector *sector,
    double initial_fighters, float amount)
{
	volatile float updated;

	if (sector == NULL)
		return;
	updated = (float)(initial_fighters + (double)amount);
	sector->fighters = updated;
	(void)yt_record_set_number(&sector->record, YT_F81, updated);
}

void
yt_team_transfer_apply_player(struct yt_player *player, float amount)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = (float)((double)player->fighters - (double)amount);
	player->fighters = updated;
	(void)yt_record_set_number(&player->record, YT_F61, updated);
}

void
yt_team_banish_apply_player(struct yt_player *player)
{
	if (player == NULL)
		return;
	player->team = 0.0f;
	(void)yt_record_set_number(&player->record, YT_F89, 0.0f);
}

void
yt_team_roster_overlay(struct yt_record *record, const float roster[4])
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	if (record == NULL || roster == NULL)
		return;
	for (index = 0; index < 4; ++index)
		(void)yt_record_set_number(record, offsets[index], roster[index]);
}

void
yt_team_name_overlay(struct yt_record *record, const uint8_t *name,
    size_t length)
{
	if (record == NULL || (name == NULL && length != 0))
		return;
	yt_record_set_text(record, name, length);
	(void)yt_record_set_number(record, YT_F73, (float)length);
}

bool
yt_team_prepare_name(char *name, size_t *length)
{
	size_t normalized;

	if (name == NULL || length == NULL)
		return false;
	if (strlen(name) < 3U)
		return false;
	normalized = qb_title_case_n((uint8_t *)name, strlen(name));
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	name[normalized] = '\0';
	*length = normalized;
	return true;
}

void
yt_team_password_overlay(struct yt_record *record, const uint8_t password[4])
{
	if (record == NULL || password == NULL)
		return;
	memcpy(record->bytes + YT_F113, password, 4);
}

bool
yt_port_link_missing(float link)
{
	return link == 0.0f;
}

bool
yt_port_name_display_row(const uint8_t *cached, size_t cached_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "This port is called: \"";
	static const uint8_t suffix[] = "\".";
	size_t needed;

	if (length == NULL || (cached == NULL && cached_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + cached_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (cached_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, cached, cached_length);
	memcpy(row + sizeof(prefix) - 1U + cached_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_prepare_candidate(const uint8_t *entered,
    size_t entered_length, const uint8_t *cached, size_t cached_length,
    uint8_t *candidate, size_t capacity, size_t *candidate_length)
{
	size_t normalized;

	if (candidate_length == NULL
	    || (entered == NULL && entered_length != 0U)
	    || (cached == NULL && cached_length != 0U))
		return false;
	*candidate_length = 0U;
	if (candidate == NULL || entered_length > capacity)
		return false;
	if (entered_length != 0U)
		memcpy(candidate, entered, entered_length);
	normalized = qb_title_case_n(candidate, entered_length);
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	if (normalized != 0U) {
		*candidate_length = normalized;
		return true;
	}
	if (cached_length > capacity
	    || (cached_length != 0U && candidate == NULL))
		return false;
	if (cached_length != 0U)
		memcpy(candidate, cached, cached_length);
	*candidate_length = cached_length;
	return true;
}

bool
yt_port_name_confirmation_prompt(const uint8_t *candidate,
    size_t candidate_length, uint8_t *prompt, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] = "\" Is this OK? [y/N]";
	size_t needed;

	if (length == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	*length = 0U;
	needed = 1U + candidate_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	prompt[0] = '"';
	if (candidate_length != 0U)
		memcpy(prompt + 1U, candidate, candidate_length);
	memcpy(prompt + 1U + candidate_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_port_name_overlay(struct yt_port *port, const uint8_t *candidate,
    size_t candidate_length)
{
	size_t copied;

	if (port == NULL || (candidate == NULL && candidate_length != 0U))
		return false;
	yt_record_set_text(&port->record, candidate, candidate_length);
	if (!yt_record_set_number(&port->record, YT_F85,
	    (float)candidate_length))
		return false;
	port->name_length = (float)candidate_length;
	copied = candidate_length < YT_TEXT_FIELD_SIZE
	    ? candidate_length : YT_TEXT_FIELD_SIZE;
	if (copied != 0U)
		memcpy(port->name, candidate, copied);
	port->name[copied] = '\0';
	return true;
}

bool
yt_port_name_editor_run(struct yt_port_name_editor_state *state,
    const struct yt_port_name_editor_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t keep[] = "Press [ENTER] to keep same name.";
	static const uint8_t instruction[] =
	    "Please enter a NAME for your port.";
	static const uint8_t name_prompt[] = "-=> ";
	uint8_t entered[4096];
	uint8_t candidate[4096];
	uint8_t row[4096];
	size_t entered_length;
	size_t candidate_length;
	size_t row_length;

	if (state == NULL || ops == NULL || state->port == NULL
	    || (state->cached == NULL && state->cached_length != 0U)
	    || state->cached_length > sizeof(candidate) || ops->row == NULL
	    || ops->prompt == NULL || ops->edit == NULL || ops->blank == NULL
	    || ops->confirm == NULL || ops->write == NULL)
		return false;
	for (;;) {
		bool accepted;

		if (!yt_port_name_display_row(state->cached,
		    state->cached_length, row, sizeof(row), &row_length)
		    || !ops->row(context, YT_PORT_NAME_CURRENT_ROW, row,
		    row_length, error)
		    || !ops->row(context, YT_PORT_NAME_KEEP_ROW, keep,
		    sizeof(keep) - 1U, error)
		    || !ops->row(context, YT_PORT_NAME_INSTRUCTION_ROW,
		    instruction, sizeof(instruction) - 1U, error)
		    || !ops->prompt(context, name_prompt,
		    sizeof(name_prompt) - 1U, error)
		    || !ops->edit(context, entered, sizeof(entered),
		    &entered_length, error)
		    || entered_length > sizeof(entered)
		    || !yt_port_name_prepare_candidate(entered, entered_length,
		    state->cached, state->cached_length, candidate,
		    sizeof(candidate), &candidate_length))
			return false;
		if (candidate_length == 0U)
			continue;
		if (!ops->blank(context, error)
		    || !yt_port_name_confirmation_prompt(candidate,
		    candidate_length, row, sizeof(row), &row_length)
		    || !ops->confirm(context, row, row_length, &accepted, error))
			return false;
		if (!accepted)
			continue;
		if (!yt_port_name_overlay(state->port, candidate,
		    candidate_length))
			return false;
		return ops->write(context, state->logical_port,
		    &state->port->record, error);
	}
}

bool
yt_port_rename_record(float port_offset, float sector_link,
    int *logical_port, float *relative_port)
{
	volatile float physical = port_offset + sector_link;
	volatile float relative = physical - port_offset;
	uint32_t record;
	int64_t logical;

	if (logical_port == NULL || relative_port == NULL)
		return false;
	record = qb_brun_random_record_number(physical);
	if (port_offset < (float)INT_MIN
	    || port_offset > (float)INT_MAX)
		return false;
	logical = (int64_t)record - (int)port_offset;
	if (logical < INT_MIN || logical > INT_MAX)
		return false;
	*logical_port = (int)logical;
	*relative_port = relative;
	return true;
}

double
yt_port_purchase_price(const float production[3])
{
	volatile float sum12;
	volatile float sum123;
	volatile float divided;
	volatile float integral;
	volatile float result;

	if (production == NULL)
		return 0.0;
	sum12 = production[0] + production[1];
	sum123 = sum12 + production[2];
	divided = sum123 / 10.0f;
	integral = floorf(divided);
	result = integral + 1.0f;
	return (double)result;
}

float
yt_port_purchase_seller_credit(float treasury, float credits, double price)
{
	volatile double subtotal = (double)treasury + (double)credits;
	volatile double total = subtotal + price;

	return (float)total;
}

float
yt_port_purchase_buyer_credit(float credits, double price)
{
	volatile double result = (double)credits - price;

	return (float)result;
}

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
yt_genesis_insufficient_rows(float required, float owned,
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
	volatile float shortfall = required - owned;
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

bool
yt_planet_garrison_prompt(float player_forces, float planet_forces,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "Drop how many ground force units on the planet?";
	static const uint8_t suffix[] = " Available ->";
	volatile float available = player_forces + planet_forces;
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), available);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && prompt == NULL))
		return false;
	memcpy(prompt, prefix, sizeof(prefix) - 1U);
	memcpy(prompt + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(prompt + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_garrison_after(float player_forces, float desired,
    float planet_forces)
{
	volatile float subtracted = player_forces - desired;
	volatile float result = subtracted + planet_forces;

	return result;
}

void
yt_planet_garrison_overlay(struct yt_planet *planet, float desired,
    int player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x40, 0x00};

	if (planet == NULL)
		return;
	planet->ground_forces = desired;
	(void)yt_record_set_number(&planet->record, YT_F77, desired);
	(void)yt_record_set_raw_number(&planet->record, YT_F73, dirty_zero);
	planet->owner = 0.0f;
	if (desired >= 1.0f && player_record != 0) {
		planet->owner = (float)player_record;
		(void)yt_record_set_number(&planet->record, YT_F73,
		    planet->owner);
	}
}

void
yt_planet_garrison_player_overlay(struct yt_player *player, float remaining)
{
	volatile float integral = floorf(remaining);

	if (player == NULL)
		return;
	(void)yt_record_set_number(&player->record, YT_F121, integral);
}

bool
yt_planet_garrison_success_row(float desired, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground force strength now at";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), desired);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_permission_run(struct yt_planet_permission_state *state,
    const struct yt_planet_permission_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t governor[] =
	    "This planet has no governor! Hail to the new planetary governor!!";
	static const uint8_t unrest[] =
	    "Due to the unrest caused by the lack of planetary govornment, ";
	static const uint8_t permission[] = "Permission to land is ";
	static const uint8_t denied[] = "DENIED!";
	uint8_t row[256];
	size_t row_length;
	volatile float updater_logical;
	int owner_record;

	if (state == NULL || ops == NULL || ops->update_planet == NULL
	    || ops->read_planet == NULL || ops->write_planet == NULL
	    || ops->read_player == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->wait == NULL || ops->random == NULL
	    || ops->set_foreground == NULL || ops->set_blink == NULL
	    || state->last_player_record < 0) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet permission state");
			error->path[0] = '\0';
		}
		return false;
	}
	state->physical_planet_record = qb_brun_random_record_number(
	    state->planet_record_value);
	updater_logical = state->planet_record_value - state->planet_offset;
	state->updater_logical = updater_logical;
	state->cached_name_length = 0U;
	state->owner_record = 0;
	state->friendly = false;
	state->vacant = false;
	state->allowed = false;
	state->denied = false;
	state->draws[0] = 0.0f;
	state->draws[1] = 0.0f;
	state->reduced_ground_forces = 0.0f;
	if (!ops->update_planet(context, updater_logical, error)
	    || !ops->read_planet(context, state->physical_planet_record,
	    &state->planet, error)
	    || !yt_planet_stored_name(&state->planet, state->cached_name,
	    &state->cached_name_length, error))
		return false;
	state->cached_owner = state->planet.owner;
	state->cached_ground_forces = state->planet.ground_forces;
	if (yt_planet_landing_immediate_allow(state->cached_ground_forces,
	    state->cached_owner, state->current_player_record)) {
		state->allowed = true;
		return true;
	}
	if (state->current_player_record >= 2
	    && state->current_player_record <= state->last_player_record
	    && yt_planet_landing_valid_owner(state->cached_owner,
	    state->last_player_record, &owner_record)) {
		state->owner_record = owner_record;
		if (!ops->read_player(context, state->current_player_record,
		    &state->friendship_current, error))
			return false;
		if (state->friendship_current.team != 0.0f) {
			if (!ops->read_player(context, owner_record,
			    &state->friendship_owner, error))
				return false;
			state->friendly = yt_sector_force_same_team(
			    state->friendship_current.team,
			    state->friendship_owner.team);
		}
	}
	if (state->friendly) {
		state->allowed = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U, YT_PLANET_PERMISSION_LINE,
	    "planet permission late blank", error))
		return false;
	state->vacant = state->cached_owner == 0.0f;
	if (!state->vacant && yt_planet_landing_valid_owner(
	    state->cached_owner, state->last_player_record, &owner_record)) {
		state->owner_record = owner_record;
		if (!ops->read_player(context, owner_record,
		    &state->vacancy_owner, error))
			return false;
		state->vacant = yt_planet_landing_vacant(state->cached_owner,
		    state->vacancy_owner.killed_by, state->last_player_record);
	}
	if (state->vacant) {
		struct yt_planet fresh;

		if (!ops->present(context, governor, sizeof(governor) - 1U,
		    YT_PLANET_PERMISSION_LINE, "vacant planet governor row", error)
		    || !ops->sound(context, 1.0f, "vacant planet sound", error)
		    || !ops->wait(context, 2.0,
		    "vacant-planet governor wait", error)
		    || !ops->read_planet(context, state->physical_planet_record,
		    &fresh, error)
		    || !ops->random(context, &state->draws[0], error)
		    || !ops->random(context, &state->draws[1], error))
			return false;
		state->reduced_ground_forces = yt_planet_landing_attrition(
		    state->draws[0], state->draws[1],
		    state->cached_ground_forces);
		if (!ops->present(context, unrest, sizeof(unrest) - 1U,
		    YT_PLANET_PERMISSION_LINE, "vacant planet unrest row", error)
		    || !yt_planet_landing_unrest_row(
		    state->reduced_ground_forces, state->cached_ground_forces,
		    row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_PLANET_PERMISSION_LINE,
		    "vacant planet reduction row", error))
			return false;
		yt_planet_landing_vacancy_overlay(&fresh,
		    state->reduced_ground_forces, state->current_player_record);
		state->planet = fresh;
		if (!ops->write_planet(context, state->physical_planet_record,
		    &state->planet, error)
		    || !ops->wait(context, 5.0,
		    "vacant-planet unrest wait", error))
			return false;
		state->allowed = true;
		return true;
	}
	if (!yt_planet_landing_traffic_row(state->cached_name,
	    state->cached_name_length, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_PLANET_PERMISSION_LINE, "planet permission traffic row", error)
	    || !ops->present(context, permission, sizeof(permission) - 1U,
	    YT_PLANET_PERMISSION_RAW, "planet permission prefix", error))
		return false;
	state->blink = 1.0f;
	ops->set_blink(context, 1.0f);
	state->foreground = 3.0f;
	ops->set_foreground(context, 3.0f);
	if (!ops->present(context, denied, sizeof(denied) - 1U,
	    YT_PLANET_PERMISSION_BOLD_LINE, "planet permission denial", error))
		return false;
	state->denied = true;
	state->foreground = 6.0f;
	ops->set_foreground(context, 6.0f);
	return true;
}

bool
yt_planet_landing_record(float planet_offset, float sector_link,
    uint32_t *physical_record, float *updater_logical)
{
	volatile float physical = planet_offset + sector_link;
	volatile float logical = physical - planet_offset;

	if (physical_record == NULL || updater_logical == NULL)
		return false;
	*physical_record = qb_brun_random_record_number(physical);
	*updater_logical = logical;
	return true;
}

bool
yt_planet_landing_immediate_allow(float ground_forces, float owner,
    int current_player_record)
{
	return floorf(ground_forces) <= 0.0f
	    || owner == (float)current_player_record;
}

bool
yt_planet_landing_valid_owner(float owner, int last_player_record,
    int *physical_owner_record)
{
	uint32_t record;

	if (physical_owner_record != NULL)
		*physical_owner_record = 0;
	if (owner < 2.0f || owner > (float)last_player_record)
		return false;
	record = qb_brun_random_record_number(owner);
	if (record > (uint32_t)INT_MAX)
		return false;
	if (physical_owner_record != NULL)
		*physical_owner_record = (int)record;
	return true;
}

bool
yt_planet_landing_vacant(float owner, float owner_status,
    int last_player_record)
{
	return owner == 0.0f
	    || (yt_planet_landing_valid_owner(owner, last_player_record, NULL)
	    && owner_status != 0.0f);
}

float
yt_planet_landing_attrition(float first_draw, float second_draw,
    float cached_ground_forces)
{
	volatile float product = first_draw * second_draw;
	volatile float scaled = product * cached_ground_forces;

	return floorf(scaled);
}

void
yt_planet_landing_vacancy_overlay(struct yt_planet *planet,
    float ground_forces, int current_player_record)
{
	float owner;

	if (planet == NULL)
		return;
	owner = ground_forces > 0.0f ? (float)current_player_record : 0.0f;
	planet->ground_forces = ground_forces;
	planet->owner = owner;
	(void)yt_record_set_number(&planet->record, YT_F77, ground_forces);
	(void)yt_record_set_number(&planet->record, YT_F73, owner);
}

static bool
landing_join_number(const uint8_t *prefix, size_t prefix_length,
    float number, const uint8_t *suffix, size_t suffix_length,
    uint8_t *output, size_t capacity, size_t *length)
{
	char formatted[64];
	int formatted_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	formatted_length = qb_str_single(formatted, sizeof(formatted), number);
	if (formatted_length < 0)
		return false;
	needed = prefix_length + (size_t)formatted_length + suffix_length;
	if (needed > capacity || (needed != 0U && output == NULL))
		return false;
	memcpy(output, prefix, prefix_length);
	memcpy(output + prefix_length, formatted, (size_t)formatted_length);
	memcpy(output + prefix_length + (size_t)formatted_length, suffix,
	    suffix_length);
	*length = needed;
	return true;
}

bool
yt_planet_landing_traffic_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] =
	    "This is space traffic control at planet ";
	size_t needed;

	if (length == NULL || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + planet_name_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_name_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet_name,
		    planet_name_length);
	*length = needed;
	return true;
}

bool
yt_planet_landing_sensor_row(float fresh_ground_forces,
    float cached_carried_forces, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "Sensors report ground forces of";
	static const uint8_t middle[] = " units. You have";
	static const uint8_t suffix[] = ".";
	char defenders[64];
	char carried[64];
	int defenders_length;
	int carried_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	defenders_length = qb_str_single(defenders, sizeof(defenders),
	    floorf(fresh_ground_forces));
	carried_length = qb_str_single(carried, sizeof(carried),
	    cached_carried_forces);
	if (defenders_length < 0 || carried_length < 0)
		return false;
	needed = sizeof(first) - 1U + (size_t)defenders_length
	    + sizeof(middle) - 1U + (size_t)carried_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, first, sizeof(first) - 1U);
	memcpy(row + sizeof(first) - 1U, defenders,
	    (size_t)defenders_length);
	memcpy(row + sizeof(first) - 1U + (size_t)defenders_length,
	    middle, sizeof(middle) - 1U);
	memcpy(row + sizeof(first) - 1U + (size_t)defenders_length
	    + sizeof(middle) - 1U, carried, (size_t)carried_length);
	memcpy(row + needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_landing_amount_prompt(float cached_carried_forces,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "Use how many ground forces? You have";
	static const uint8_t suffix[] = ". [0] ";

	return landing_join_number(prefix, sizeof(prefix) - 1U,
	    cached_carried_forces, suffix, sizeof(suffix) - 1U,
	    prompt, capacity, length);
}

float
yt_planet_landing_commitment(const char *response)
{
	struct qb_val_result parsed;
	volatile double integral;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	integral = floor(parsed.valid ? parsed.value : 0.0);
	return (float)integral;
}

bool
yt_planet_landing_commitment_valid(float commitment,
    float cached_carried_forces)
{
	return commitment >= 1.0f && commitment <= cached_carried_forces;
}

bool
yt_planet_landing_unrest_row(float reduced, float original,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "ground forces have been reduced to";
	static const uint8_t middle[] = " from";
	static const uint8_t suffix[] = "!";
	char reduced_text[64];
	char original_text[64];
	int reduced_length;
	int original_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	reduced_length = qb_str_single(reduced_text, sizeof(reduced_text),
	    reduced);
	original_length = qb_str_single(original_text, sizeof(original_text),
	    original);
	if (reduced_length < 0 || original_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)reduced_length
	    + sizeof(middle) - 1U + (size_t)original_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, reduced_text,
	    (size_t)reduced_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)reduced_length,
	    middle, sizeof(middle) - 1U);
	memcpy(row + sizeof(prefix) - 1U + (size_t)reduced_length
	    + sizeof(middle) - 1U, original_text,
	    (size_t)original_length);
	memcpy(row + needed - (sizeof(suffix) - 1U), suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

void
yt_planet_assault_player_overlay(struct yt_player *player, float commitment)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->ground_forces - commitment;
	player->ground_forces = remaining;
	(void)yt_record_set_number(&player->record, YT_F121, remaining);
}

void
yt_planet_assault_victory_overlay(struct yt_planet *planet, float owner,
    float attackers)
{
	volatile float integral = floorf(attackers);

	if (planet == NULL)
		return;
	planet->owner = owner;
	planet->ground_forces = integral;
	(void)yt_record_set_number(&planet->record, YT_F73, owner);
	(void)yt_record_set_number(&planet->record, YT_F77, integral);
}

void
yt_planet_assault_failure_overlay(struct yt_planet *planet, float defenders)
{
	volatile float integral = floorf(defenders);

	if (planet == NULL)
		return;
	planet->ground_forces = integral;
	(void)yt_record_set_number(&planet->record, YT_F77, integral);
}

void
yt_planet_assault_round(bool attacker_damage, float amount,
    float *attackers, float *defenders)
{
	volatile float product;
	volatile float reduced;

	if (attackers == NULL || defenders == NULL)
		return;
	if (attacker_damage) {
		product = amount * *defenders;
		reduced = *attackers - product;
		*attackers = floorf(reduced);
		if (*attackers < 0.0f)
			*attackers = 0.0f;
	}
	else {
		product = amount * *attackers;
		reduced = *defenders - product;
		*defenders = floorf(reduced);
		if (*defenders < 0.0f)
			*defenders = 0.0f;
	}
}

bool
yt_planet_assault_attack_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, float commitment, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t attacked[] = " attacked planet ";
	static const uint8_t with[] = " with";
	static const uint8_t suffix[] = " ground forces!";
	char number[64];
	int number_length;
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (player_name == NULL && player_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	number_length = qb_str_single(number, sizeof(number), commitment);
	if (number_length < 0)
		return false;
	needed = sizeof(marker) - 1U + player_name_length
	    + sizeof(attacked) - 1U + planet_name_length + sizeof(with) - 1U
	    + (size_t)number_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, marker, sizeof(marker) - 1U);
	cursor += sizeof(marker) - 1U;
	if (player_name_length != 0U) {
		memcpy(row + cursor, player_name, player_name_length);
		cursor += player_name_length;
	}
	memcpy(row + cursor, attacked, sizeof(attacked) - 1U);
	cursor += sizeof(attacked) - 1U;
	if (planet_name_length != 0U) {
		memcpy(row + cursor, planet_name, planet_name_length);
		cursor += planet_name_length;
	}
	memcpy(row + cursor, with, sizeof(with) - 1U);
	cursor += sizeof(with) - 1U;
	memcpy(row + cursor, number, (size_t)number_length);
	cursor += (size_t)number_length;
	memcpy(row + cursor, suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_assault_status_row(bool attacker_damage, float remaining,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t attacker[] = "Your forces remaining  :";
	static const uint8_t defender[] = "Ground forces remaining:";
	static const uint8_t suffix[] = "!";
	const uint8_t *prefix = attacker_damage ? attacker : defender;
	size_t prefix_length = attacker_damage
	    ? sizeof(attacker) - 1U : sizeof(defender) - 1U;

	return landing_join_number(prefix, prefix_length, remaining,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_assault_capture_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t captured[] = " captured planet ";
	static const uint8_t suffix[] = "!";
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (player_name == NULL && player_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(marker) - 1U + player_name_length
	    + sizeof(captured) - 1U + planet_name_length + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, marker, sizeof(marker) - 1U);
	cursor += sizeof(marker) - 1U;
	if (player_name_length != 0U) {
		memcpy(row + cursor, player_name, player_name_length);
		cursor += player_name_length;
	}
	memcpy(row + cursor, captured, sizeof(captured) - 1U);
	cursor += sizeof(captured) - 1U;
	if (planet_name_length != 0U) {
		memcpy(row + cursor, planet_name, planet_name_length);
		cursor += planet_name_length;
	}
	memcpy(row + cursor, suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_planet_assault_failure_row(float defenders, bool news,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t marker[] = " +++ ";
	static const uint8_t prefix[] =
	    "Attack Failed! Ground Forces remaining:";
	static const uint8_t suffix[] = "!";
	uint8_t screen[128];
	size_t screen_length;

	if (!landing_join_number(prefix, sizeof(prefix) - 1U,
	    floorf(defenders), suffix, sizeof(suffix) - 1U,
	    screen, sizeof(screen), &screen_length) || length == NULL)
		return false;
	*length = 0U;
	if (screen_length + (news ? sizeof(marker) - 1U : 0U) > capacity
	    || (row == NULL && screen_length != 0U))
		return false;
	if (news) {
		memcpy(row, marker, sizeof(marker) - 1U);
		memcpy(row + sizeof(marker) - 1U, screen, screen_length);
		*length = sizeof(marker) - 1U + screen_length;
	}
	else {
		memcpy(row, screen, screen_length);
		*length = screen_length;
	}
	return true;
}

bool
yt_planet_creation_credit_row(double credits, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = " credits.";
	char number[64];
	int number_length;
	size_t needed;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), credits);
	if (number_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)number_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	memcpy(row + sizeof(prefix) - 1U + (size_t)number_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

void
yt_planet_creation_overlay(struct yt_planet *planet,
    int current_player_record)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	size_t index;

	if (planet == NULL)
		return;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = 1.0f;
		planet->stock[index] = 10.0f;
		(void)yt_record_set_number(&planet->record,
		    YT_F45 + index * 4U, 1.0f);
		(void)yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, 10.0f);
	}
	planet->mines = 0.0f;
	planet->missiles = 0.0f;
	planet->owner = (float)current_player_record;
	planet->ground_forces = 1.0f;
	planet->plasma = 0.0f;
	planet->bank = 0.0f;
	planet->fighters = 30.0f;
	(void)yt_record_set_raw_number(&planet->record, YT_F125, dirty_zero);
	(void)yt_record_set_raw_number(&planet->record, YT_F69, dirty_zero);
	(void)yt_record_set_number(&planet->record, YT_F73, planet->owner);
	(void)yt_record_set_number(&planet->record, YT_F77, 1.0f);
	(void)yt_record_set_number(&planet->record, YT_F113, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F117, 0.0f);
	(void)yt_record_set_number(&planet->record, YT_F129, 30.0f);
}

void
yt_planet_creation_timestamp_overlay(struct yt_planet *planet,
    float day, float minute)
{
	if (planet == NULL)
		return;
	planet->last_day = day;
	planet->last_minute = minute;
	(void)yt_record_set_number(&planet->record, YT_F41, day);
	(void)yt_record_set_number(&planet->record, YT_F89, minute);
}

void
yt_planet_creation_credit_overlay(struct yt_player *player,
    float price_argument)
{
	volatile float sum;
	volatile float integral;

	if (player == NULL)
		return;
	sum = player->credits + price_argument;
	integral = floorf(sum);
	player->credits = integral;
	(void)yt_record_set_number(&player->record, YT_F81, integral);
}

bool
yt_planet_creation_news(const uint8_t *trader_name,
    size_t trader_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t middle[] = " made a planet: ";
	size_t needed;
	size_t cursor = 0U;

	if (length == NULL || (trader_name == NULL && trader_name_length != 0U)
	    || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + trader_name_length
	    + sizeof(middle) - 1U + planet_name_length;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + cursor, prefix, sizeof(prefix) - 1U);
	cursor += sizeof(prefix) - 1U;
	if (trader_name_length != 0U) {
		memcpy(row + cursor, trader_name, trader_name_length);
		cursor += trader_name_length;
	}
	memcpy(row + cursor, middle, sizeof(middle) - 1U);
	cursor += sizeof(middle) - 1U;
	if (planet_name_length != 0U)
		memcpy(row + cursor, planet_name, planet_name_length);
	*length = needed;
	return true;
}

bool
yt_planet_creation_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Planet \"";
	static const uint8_t suffix[] =
	    "\" created with Genesis Device!";
	size_t needed;

	if (length == NULL || (planet_name == NULL && planet_name_length != 0U))
		return false;
	*length = 0U;
	needed = sizeof(prefix) - 1U + planet_name_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_name_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet_name,
		    planet_name_length);
	memcpy(row + sizeof(prefix) - 1U + planet_name_length, suffix,
	    sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

float
yt_planet_move_destination(const char *response)
{
	struct qb_val_result parsed;
	volatile double integral;

	if (response == NULL)
		return 0.0f;
	parsed = qb_val(response);
	integral = floor(parsed.valid ? parsed.value : 0.0);
	return (float)integral;
}

float
yt_planet_move_maximum(float port_record_offset,
    float sector_record_offset)
{
	volatile float result = port_record_offset - sector_record_offset;

	return result;
}

float
yt_planet_move_add_cost(float cost)
{
	volatile float result = cost + 10.0f;

	return result;
}

float
yt_planet_move_fighter_loss(float fighters, float first_draw,
    float second_draw)
{
	volatile float first_product = first_draw * fighters;
	volatile float first = floorf(first_product) + 1.0f;
	volatile float second_product = second_draw * first;
	volatile float loss = floorf(second_product) + 1.0f;

	return loss;
}

void
yt_planet_move_sector_overlay(struct yt_sector *sector, float planet_link)
{
	if (sector == NULL)
		return;
	sector->planet = planet_link;
	(void)yt_record_set_number(&sector->record, YT_F93, planet_link);
}

void
yt_planet_move_explosion_overlay(struct yt_planet *planet)
{
	static const uint8_t zero_raw[4] = {0, 0, 0, 0};

	if (planet == NULL)
		return;
	planet->name[0] = '\0';
	planet->name_length = 0.0f;
	memcpy(planet->record.bytes, zero_raw, sizeof(zero_raw));
	memset(planet->record.bytes + sizeof(zero_raw), ' ',
	    YT_TEXT_FIELD_SIZE - sizeof(zero_raw));
	(void)yt_record_set_raw_number(&planet->record, YT_F85, zero_raw);
}

void
yt_planet_move_fighter_overlay(struct yt_player *player, float loss)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->fighters - loss;
	player->fighters = remaining;
	(void)yt_record_set_number(&player->record, YT_F61, remaining);
}

void
yt_planet_move_success_overlay(struct yt_player *player,
    float requested_destination)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->turns + -10.0f;
	player->turns = remaining;
	player->sector = requested_destination;
	(void)yt_record_set_number(&player->record, YT_F49, remaining);
	(void)yt_record_set_number(&player->record, YT_F57,
	    requested_destination);
}

static bool
move_join_parts(const uint8_t *first, size_t first_length,
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
yt_planet_move_path_heading(float start, float destination,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "The shortest path from sector";
	static const uint8_t middle[] = " to sector";
	static const uint8_t suffix[] = " is:";
	char start_text[64];
	char destination_text[64];
	int start_length = qb_str_single(start_text, sizeof(start_text), start);
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);

	if (start_length < 0 || destination_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)start_text, (size_t)start_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_summary(float cost, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "Distance is";
	static const uint8_t middle[] = " and will take";
	static const uint8_t suffix[] = " turns.";
	volatile float distance = cost / 10.0f;
	char distance_text[64];
	char cost_text[64];
	int distance_length = qb_str_single(distance_text,
	    sizeof(distance_text), distance);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), cost);

	if (distance_length < 0 || cost_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)distance_text, (size_t)distance_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_turns_row(float turns, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "You have";
	static const uint8_t suffix[] = " turns left.";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), turns);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_explosion_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "The stress was too much! PLANET ";
	static const uint8_t suffix[] = " EXPLODED!";

	return move_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

bool
yt_planet_move_explosion_news(const uint8_t *planet_name,
    size_t planet_name_length, const uint8_t *player_name,
    size_t player_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = " *** Planet ";
	static const uint8_t middle[] = " EXPLODED while being moved by ";
	static const uint8_t suffix[] = "!!!";

	return move_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, middle, sizeof(middle) - 1U,
	    player_name, player_name_length, suffix, sizeof(suffix) - 1U,
	    row, capacity, length);
}

bool
yt_planet_move_loss_row(const uint8_t *actor, size_t actor_length,
    float loss, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " lost";
	static const uint8_t suffix[] = " fighters in the explosion!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), loss);

	if (number_length < 0)
		return false;
	return move_join_parts(actor, actor_length, middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] =
	    " moved! (Xannoron Movers, we move anyTHING, anyWHERE!)";

	return move_join_parts(planet_name, planet_name_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

float
yt_sector_mine_batch(float mines_before)
{
	volatile float quotient;

	if (!(mines_before > 19.0f))
		return 1.0f;
	quotient = mines_before / 10.0f;
	return floorf(quotient);
}

float
yt_sector_mine_shield_result(float shields, float batch, float draw)
{
	volatile float product = draw * 1001.0f;
	volatile float quantum = floorf(product);
	volatile float loss = quantum * batch;
	volatile float result = shields - loss;

	return result < 1.0f ? 0.0f : result;
}

float
yt_sector_mine_cloak_loss(float cloak, float batch, float draw)
{
	volatile float first = draw * batch;
	volatile float scaled = first * 100.0f;
	volatile float integral = floorf(scaled);
	volatile float loss = integral / 100.0f;

	return loss > cloak ? cloak : loss;
}

float
yt_sector_mine_missile_loss(float missiles, float batch, float draw)
{
	volatile float range = batch * missiles;
	volatile float product = draw * range;
	volatile float loss = floorf(product) + 1.0f;

	return loss > missiles ? missiles : loss;
}

bool
yt_sector_mine_missile_step(float missiles, float batch,
    yt_sector_mine_draw_fn draw, void *context,
    struct yt_sector_mine_missile_result *result, struct yt_error *error)
{
	struct yt_sector_mine_missile_result next;
	float sampled;
	volatile float remaining;

	if (draw == NULL || result == NULL)
		return false;
	if (missiles == 0.0f) {
		next.remaining = missiles;
		next.loss = 0.0f;
		next.applied = false;
		*result = next;
		return true;
	}
	if (!draw(context, &sampled, error))
		return false;
	next.loss = yt_sector_mine_missile_loss(missiles, batch, sampled);
	remaining = missiles - next.loss;
	next.remaining = remaining;
	next.applied = true;
	*result = next;
	return true;
}

float
yt_sector_mine_empty_holds(const struct yt_player *player)
{
	volatile float empty;

	if (player == NULL)
		return 0.0f;
	empty = player->holds - player->equipment;
	empty = empty - player->organics;
	empty = empty - player->ore;
	return empty;
}

void
yt_sector_mine_sector_overlay(struct yt_sector *sector, float mines_after)
{
	if (sector == NULL)
		return;
	sector->mines = mines_after;
	(void)yt_record_set_number(&sector->record, YT_F129, mines_after);
}

void
yt_sector_mine_player_overlay(struct yt_player *fresh,
    const struct yt_player *working, unsigned fields)
{
	static const uint8_t scanner_zero[4] = {0x00, 0x00, 0x48, 0x00};

	if (fresh == NULL || working == NULL)
		return;
#define MINE_OVERLAY(flag, member, offset) do { \
	if ((fields & (flag)) != 0U) { \
		fresh->member = working->member; \
		(void)yt_record_set_number(&fresh->record, (offset), \
		    working->member); \
	} \
} while (0)
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_SHIELDS, shields, YT_F53);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_FIGHTERS, fighters, YT_F61);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_HOLDS, holds, YT_F65);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORE, ore, YT_F69);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_ORGANICS, organics, YT_F73);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_EQUIPMENT, equipment, YT_F77);
	if ((fields & YT_SECTOR_MINE_DAMAGE_SCANNER) != 0U) {
		fresh->danger_scanner = working->danger_scanner;
		if (working->danger_scanner == 0.0f)
			(void)yt_record_set_raw_number(&fresh->record, YT_F93,
			    scanner_zero);
		else
			(void)yt_record_set_number(&fresh->record, YT_F93,
			    working->danger_scanner);
	}
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_MISSILES, missiles, YT_F97);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CLOAK, cloak, YT_F125);
	MINE_OVERLAY(YT_SECTOR_MINE_DAMAGE_CARRIED_MINES, mines, YT_F129);
#undef MINE_OVERLAY
}

bool
yt_sector_mine_explosion_row(float mines_before, float batch,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "There are";
	static const uint8_t middle[] = " mines here!";
	static const uint8_t suffix[] = " EXPLODE!";
	char before_text[64];
	char batch_text[64];
	int before_length = qb_str_single(before_text, sizeof(before_text),
	    mines_before);
	int batch_length = qb_str_single(batch_text, sizeof(batch_text), batch);

	if (before_length < 0 || batch_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)before_text, (size_t)before_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)batch_text, (size_t)batch_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_sector_mine_shields_row(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields down to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_loss_row(enum yt_sector_mine_loss_kind kind, float loss,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Lost";
	static const char *const suffixes[] = {
		" fighters!", "% cloak!", " Missiles!", " mines!",
		" holds of ore!", " holds of organics!",
		" holds of equipment!", " empty holds!",
	};
	char number[64];
	int number_length;
	const char *suffix;

	if (kind < YT_SECTOR_MINE_LOSS_FIGHTERS
	    || kind > YT_SECTOR_MINE_LOSS_EMPTY_HOLDS)
		return false;
	suffix = suffixes[kind];
	number_length = qb_str_single(number, sizeof(number), loss);
	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    (const uint8_t *)suffix, strlen(suffix), NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_entry_news(const uint8_t *player_name,
    size_t player_name_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] = " hit sector mines in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), sector);

	if (number_length < 0)
		return false;
	return move_join_parts(player_name, player_name_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_sector_mine_final_news(float shields, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Shields reduced to";
	static const uint8_t suffix[] = " units!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), shields);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_direct_fighter_mine_warning(const uint8_t *victim_name,
    size_t victim_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t suffix[] =
	    " had sector mines! They EXPLODED!";

	return move_join_parts(prefix, sizeof(prefix) - 1U,
	    victim_name, victim_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

bool
yt_common_fatal_run(struct yt_common_fatal_state *state,
    const struct yt_common_fatal_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Your ship has been destroyed!";
	struct yt_player player;

	if (state == NULL || ops == NULL || ops->set_foreground == NULL
	    || ops->present == NULL || ops->read_player == NULL
	    || ops->sound == NULL || ops->death == NULL || ops->wait == NULL)
		return false;
	state->foreground = 3.0f;
	state->pager_foreground = 3;
	state->wait_complete = false;
	state->normal_exit = false;
	ops->set_foreground(context, state->foreground,
	    state->pager_foreground);
	if (!ops->present(context, notice, sizeof(notice) - 1U, error)
	    || !ops->read_player(context, state->current_player_record,
	    &player, error))
		return false;
	state->field_player = player;
	state->field_valid = true;
	state->target_record = (float)state->current_player_record;
	if (!ops->sound(context, error)
	    || !ops->death(context, state->current_player_record,
	    state->target_record, error)
	    || !ops->wait(context, 5.0f, error))
		return false;
	state->wait_complete = true;
	state->normal_exit = true;
	return true;
}

bool
yt_direct_fighter_kill_run(struct yt_direct_fighter_kill_state *state,
    const struct yt_direct_fighter_kill_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_player target;
	struct yt_sector sector;
	uint8_t warning[128];
	size_t warning_length;
	bool terminal;
	volatile float deployed;

	if (state == NULL || ops == NULL)
		return false;
	if (state->target_shields > 0.0f) {
		state->route = YT_DIRECT_FIGHTER_NO_KILL;
		return true;
	}
	if (ops->sound == NULL || ops->read_player == NULL
	    || ops->name_length == NULL || ops->death == NULL
	    || ops->salvage == NULL)
		return false;
	if (!ops->sound(context, error)
	    || !ops->read_player(context, state->target_record, &target, error))
		return false;
	state->saved_mines = target.mines;
	if (!ops->name_length(context, target.name_length,
	    &state->saved_name_length, error))
		return false;
	if (state->saved_name_length > sizeof(state->saved_name))
		return false;
	if (state->saved_name_length != 0U)
		memcpy(state->saved_name, target.record.bytes,
		    state->saved_name_length);
	if (!ops->death(context, state->target_record,
	    (float)state->current_player_record, error)
	    || !ops->salvage(context, state->target_record,
	    (float)state->current_player_record, error))
		return false;
	if (!(state->saved_mines > 0.0f)) {
		state->route = YT_DIRECT_FIGHTER_FRESH_PROMPT;
		return true;
	}
	if (ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->present == NULL || ops->news == NULL || ops->mine == NULL)
		return false;
	if (!ops->read_sector(context, state->current_sector, &sector, error))
		return false;
	deployed = sector.mines + state->saved_mines;
	yt_sector_mine_sector_overlay(&sector, deployed);
	if (!ops->write_sector(context, state->current_sector, &sector, error)
	    || !yt_direct_fighter_mine_warning(state->saved_name,
	    state->saved_name_length, warning, sizeof(warning),
	    &warning_length)
	    || !ops->present(context, warning, warning_length, error)
	    || !ops->news(context, warning, warning_length, error))
		return false;
	terminal = false;
	if (!ops->mine(context, &terminal, state->destroyed_raw, error))
		return false;
	if (terminal) {
		state->route = YT_DIRECT_FIGHTER_MINE_TERMINAL;
		return true;
	}
	if (!qb_mbf32_truth(state->destroyed_raw)) {
		state->route = YT_DIRECT_FIGHTER_FRESH_PROMPT;
		return true;
	}
	state->route = YT_DIRECT_FIGHTER_COMMON_FATAL;
	if (ops->fatal == NULL)
		return false;
	return ops->fatal(context, error);
}

float
yt_emergency_warp_duration(float first, float second)
{
	volatile float first_part = first * 70.0f;
	volatile float second_part = second * 70.0f;
	volatile float result = first_part + second_part;

	return result;
}

float
yt_emergency_warp_destination(float draw, float sector_count)
{
	volatile float product = draw * sector_count;
	volatile float integral = floorf(product);
	volatile float result = integral + 1.0f;

	return result;
}

float
yt_emergency_warp_cost(float heat, float draw, float turns, bool meltdown)
{
	volatile float heat_cost = heat * 4.0f;
	volatile float jitter_product = draw * 4.0f;
	volatile float jitter = floorf(jitter_product);
	volatile float result = heat_cost + jitter;

	if (result > turns)
		result = turns;
	if (meltdown)
		result = turns;
	return result;
}

void
yt_emergency_warp_player_overlay(struct yt_player *player,
    float destination, float cost)
{
	volatile float remaining;

	if (player == NULL)
		return;
	remaining = player->turns - cost;
	player->sector = destination;
	player->turns = remaining;
	(void)yt_record_set_number(&player->record, YT_F57, destination);
	(void)yt_record_set_number(&player->record, YT_F49, remaining);
}

bool
yt_emergency_warp_result_row(float destination, float cost,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "sector";
	static const uint8_t middle[] =
	    ". However, it takes you";
	static const uint8_t suffix[] =
	    " turns to recharge your engines!";
	char destination_text[64];
	char cost_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), cost);

	if (destination_length < 0 || cost_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_emergency_warp_stranded_row(float destination, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "You are stranded in sector";
	static const uint8_t suffix[] = ".";
	char destination_text[64];
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), destination);

	if (destination_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_movement_warp_row(const float warps[6], uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t heading[] = "Warps lead to";
	size_t used = sizeof(heading) - 1U;
	size_t slot;

	if (warps == NULL || row == NULL || length == NULL
	    || used > capacity)
		return false;
	memcpy(row, heading, used);
	for (slot = 0U; slot < 6U; ++slot) {
		char number[64];
		int number_length;

		if (warps[slot] == 0.0f)
			continue;
		number_length = qb_str_single(number, sizeof(number), warps[slot]);
		if (number_length < 0 || used + 1U + (size_t)number_length
		    > capacity)
			return false;
		row[used++] = ',';
		memcpy(row + used, number, (size_t)number_length);
		used += (size_t)number_length;
	}
	*length = used;
	return true;
}

bool
yt_movement_confirmation_prompt(float target, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t first[] = "Move into sector";
	static const uint8_t suffix[] = "? [y/N] ";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), target);

	if (number_length < 0)
		return false;
	return move_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

void
yt_movement_player_overlay(struct yt_player *player, float target)
{
	if (player == NULL)
		return;
	player->sector = target;
	(void)yt_record_set_number(&player->record, YT_F57, target);
}

size_t
yt_port_trade_schedule(const float factors[3], size_t order[3])
{
	size_t count = 0;
	size_t index;

	if (factors == NULL || order == NULL)
		return 0;
	for (index = 0; index < 3; ++index)
		if (factors[index] < 0.0f)
			order[count++] = index;
	for (index = 0; index < 3; ++index)
		if (factors[index] > 0.0f)
			order[count++] = index;
	return count;
}

int
yt_computer_selector_position(const char *command)
{
	static const char selector[] = "+!LMP?123459";
	const char *match;

	if (command == NULL)
		return 0;
	match = strstr(selector, command);
	return match == NULL ? 0 : (int)(match - selector) + 1;
}

void
yt_trade_treasury_overlay(struct yt_port *port, float receipt)
{
	volatile float updated;

	if (port == NULL)
		return;
	updated = port->treasury + receipt;
	port->treasury = updated;
	(void)yt_record_set_number(&port->record, YT_F89, updated);
}

void
yt_trade_credit_overlay(struct yt_player *player, float delta)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = player->credits + delta;
	updated = floorf(updated);
	player->credits = updated;
	(void)yt_record_set_number(&player->record, YT_F81, updated);
}

void
yt_trade_holds_overlay(struct yt_player *player, size_t commodity,
    float quantity, float direction)
{
	float *selected;
	volatile float single_delta;
	volatile double updated;

	if (player == NULL || commodity >= 3U)
		return;
	selected = commodity == 0U ? &player->ore
	    : commodity == 1U ? &player->organics : &player->equipment;
	single_delta = quantity * direction;
	updated = (double)*selected + (double)single_delta;
	*selected = (float)updated;
	(void)yt_record_set_number(&player->record, YT_F69, player->ore);
	(void)yt_record_set_number(&player->record, YT_F73, player->organics);
	(void)yt_record_set_number(&player->record, YT_F77, player->equipment);
}

void
yt_trade_stock_overlay(struct yt_port *port, size_t commodity,
    double cached_quantity, float quantity)
{
	volatile double updated;

	if (port == NULL || commodity >= 3U)
		return;
	updated = cached_quantity - (double)quantity;
	port->stock[commodity] = (float)updated;
	(void)yt_record_set_number(&port->record,
	    YT_F49 + commodity * 4U, port->stock[commodity]);
}

static float *
take_all_player_item(struct yt_player *player, int item)
{
	switch (item) {
	case 1: return &player->ore;
	case 2: return &player->organics;
	case 3: return &player->equipment;
	case 4: return &player->fighters;
	case 5: return &player->missiles;
	case 6: return &player->mines;
	case 9: return &player->plasma;
	default: return NULL;
	}
}

static float *
take_all_planet_item(struct yt_planet *planet, int item)
{
	switch (item) {
	case 1: return &planet->stock[0];
	case 2: return &planet->stock[1];
	case 3: return &planet->stock[2];
	case 4: return &planet->fighters;
	case 5: return &planet->missiles;
	case 6: return &planet->mines;
	case 9: return &planet->plasma;
	default: return NULL;
	}
}

static float
take_all_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static double
take_all_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
take_all_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

static float
take_all_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
take_all_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
take_all_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

static double
take_all_double_div(double left, double right)
{
	volatile double result = left / right;

	return result;
}

const char *
yt_planet_take_one_title(int item)
{
	static const char *const titles[7] = {
		"<Take Ore>", "<Take Organics)", "<Take Equipment)",
		"<Take Fighters)", "<Take Missiles)", "<Take Mines)",
		"<Take Plasma Bolts>"
	};

	if (item >= 1 && item <= 6)
		return titles[item - 1];
	return item == 9 ? titles[6] : NULL;
}

void
yt_planet_take_one_player_overlay(struct yt_player *player, int item,
    float amount)
{
	float *selected;

	if (player == NULL)
		return;
	selected = take_all_player_item(player, item);
	if (selected == NULL)
		return;
	if (item == 9)
		*selected = take_all_single_add(*selected, amount);
	else
		*selected = (float)take_all_double_add((double)*selected,
		    (double)amount);
}

void
yt_planet_take_one_planet_overlay(struct yt_planet *planet, int item,
    double cached_quantity, float amount)
{
	float *selected;

	if (planet == NULL)
		return;
	selected = take_all_planet_item(planet, item);
	if (selected == NULL)
		return;
	*selected = (float)take_all_double_sub(cached_quantity,
	    (double)amount);
}

void
yt_planet_take_all_weapon_player_overlay(struct yt_player *player,
    const double cached_quantity[10], double amount[10])
{
	static const int items[4] = {4, 5, 6, 9};
	size_t index;

	if (player == NULL || cached_quantity == NULL || amount == NULL)
		return;
	memset(amount, 0, 10U * sizeof(*amount));
	amount[4] = floor(cached_quantity[4]);
	for (index = 1; index < 4U; ++index)
		amount[items[index]] = (double)(float)floor(
		    cached_quantity[items[index]]);
	player->fighters = (float)take_all_double_add(
	    (double)player->fighters, amount[4]);
	player->missiles = take_all_single_add(player->missiles,
	    (float)amount[5]);
	player->mines = take_all_single_add(player->mines, (float)amount[6]);
	player->plasma = take_all_single_add(player->plasma, (float)amount[9]);
}

void
yt_planet_take_all_weapon_planet_overlay(struct yt_planet *planet,
    const double cached_quantity[10], const double amount[10])
{
	static const int items[4] = {4, 5, 6, 9};
	size_t index;

	if (planet == NULL || cached_quantity == NULL || amount == NULL)
		return;
	for (index = 0; index < 4U; ++index) {
		float *selected = take_all_planet_item(planet, items[index]);

		*selected = (float)take_all_double_sub(
		    cached_quantity[items[index]], amount[items[index]]);
	}
}

float
yt_planet_take_all_commodity_player_overlay(struct yt_player *player,
    int item, double cached_quantity)
{
	float *selected;
	float amount;
	float free_holds;
	double free_double;

	if (player == NULL || item < 1 || item > 3)
		return 0.0f;
	free_double = take_all_double_sub((double)player->holds,
	    (double)player->ore);
	free_double = take_all_double_sub(free_double,
	    (double)player->organics);
	free_double = take_all_double_sub(free_double,
	    (double)player->equipment);
	free_holds = (float)free_double;
	amount = (float)floor(cached_quantity);
	if (free_holds < amount)
		amount = free_holds;
	selected = take_all_player_item(player, item);
	*selected = (float)take_all_double_add((double)*selected,
	    (double)amount);
	return amount;
}

void
yt_planet_take_all_commodity_planet_overlay(struct yt_planet *planet,
    int item, double cached_quantity, float amount)
{
	float *selected;

	if (planet == NULL || item < 1 || item > 3)
		return;
	selected = take_all_planet_item(planet, item);
	*selected = (float)take_all_double_sub(cached_quantity,
	    (double)amount);
}

void
yt_planet_transfer_cargo_cache(float rate[10], double quantity[10],
    const double held[3])
{
	size_t index;

	if (rate == NULL || quantity == NULL || held == NULL)
		return;
	for (index = 0; index < 3U; ++index) {
		int item = (int)index + 1;
		float threshold = take_all_single_mul(rate[item], 10.0f);
		double total = take_all_double_add(quantity[item], held[index]);

		if (total > (double)threshold)
			rate[item] = (float)take_all_double_add(
			    take_all_double_div(floor(total), 10.0), 1.0);
		quantity[item] = take_all_double_add(quantity[item], held[index]);
	}
}

void
yt_planet_transfer_cargo_player_overlay(struct yt_player *player)
{
	if (player == NULL)
		return;
	player->ore = 0.0f;
	player->organics = 0.0f;
	player->equipment = 0.0f;
}

void
yt_planet_transfer_cargo_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10])
{
	size_t index;

	if (planet == NULL || rate == NULL || quantity == NULL
	    || contribution == NULL)
		return;
	for (index = 0; index < 3U; ++index) {
		int item = (int)index + 1;

		planet->production[index] = take_all_single_sub(rate[item],
		    contribution[item]);
		planet->stock[index] = (float)quantity[item];
	}
}

void
yt_planet_transfer_direct_player_overlay(struct yt_player *player, int item)
{
	float *selected;

	if (player == NULL)
		return;
	selected = take_all_player_item(player, item);
	if (selected != NULL)
		*selected = 0.0f;
}

void
yt_planet_transfer_direct_planet_overlay(struct yt_planet *planet, int item,
    double cached_quantity, float cached_amount)
{
	float *selected;

	if (planet == NULL)
		return;
	selected = take_all_planet_item(planet, item);
	if (selected != NULL)
		*selected = (float)take_all_double_add(cached_quantity,
		    (double)cached_amount);
}

void
yt_planet_transfer_fighter_player_overlay(struct yt_player *player,
    float cached_fighters, float amount)
{
	if (player != NULL)
		player->fighters = (float)take_all_double_sub(
		    (double)cached_fighters, (double)amount);
}

void
yt_planet_transfer_fighter_planet_overlay(struct yt_planet *planet,
    double cached_quantity, float amount)
{
	if (planet != NULL)
		planet->fighters = (float)take_all_double_add(cached_quantity,
		    (double)amount);
}

int
yt_planet_transfer_selector_position(const char *command)
{
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr("CSFMB", command);
	return position == NULL ? 0 : (int)(position - "CSFMB") + 1;
}

int
yt_planet_menu_selector_position(const char *command)
{
	static const char selector[] = "F!MPC1234569LTAB$";
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr(selector, command);
	return position == NULL ? 0 : (int)(position - selector) + 1;
}

bool
yt_planet_transfer_cargo_empty(const double held[3])
{
	return held != NULL && held[0] == 0.0 && held[1] == 0.0
	    && held[2] == 0.0;
}

bool
yt_planet_transfer_fighter_rejected(float amount, float cached_fighters)
{
	return amount < 0.0f || (double)amount > (double)cached_fighters;
}

double
yt_planet_bank_available(float cached_credits, float cached_bank)
{
	return take_all_double_add((double)cached_credits,
	    (double)cached_bank);
}

double
yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target)
{
	double after_target = take_all_double_sub((double)cached_credits,
	    target);

	return take_all_double_add(after_target, (double)cached_bank);
}

void
yt_planet_bank_planet_overlay(struct yt_planet *planet, double target)
{
	if (planet != NULL)
		planet->bank = (float)target;
}

float
yt_planet_bank_credit_argument(float cached_bank, double target)
{
	return (float)take_all_double_sub((double)cached_bank, target);
}

void
yt_planet_bank_credit_overlay(struct yt_player *player, float argument)
{
	if (player != NULL)
		player->credits = floorf(take_all_single_add(player->credits,
		    argument));
}

bool
yt_credit_mutation_run(struct yt_credit_mutation_state *state,
    const struct yt_credit_mutation_ops *ops, void *context,
    struct yt_error *error)
{
	struct yt_player *player;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || state->hydration.player == NULL)
		return startup_configuration_error(error, YT_RANGE,
		    "credit mutation arguments");
	state->fresh_credits = 0.0f;
	state->summed_credits = 0.0f;
	state->result_credits = 0.0f;
	memset(state->argument_raw, 0, sizeof(state->argument_raw));
	memset(state->fresh_credits_raw, 0,
	    sizeof(state->fresh_credits_raw));
	memset(state->summed_credits_raw, 0,
	    sizeof(state->summed_credits_raw));
	memset(state->result_credits_raw, 0,
	    sizeof(state->result_credits_raw));
	state->hydrated = false;
	state->overlay_applied = false;
	state->write_attempted = false;
	state->written = false;
	if (qb_mbf32_encode(state->argument, state->argument_raw)
	    == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "credit mutation argument MBF32");
	if (!yt_current_player_hydrate_run(&state->hydration,
	    ops->read_player, context, error))
		return false;
	state->hydrated = true;
	player = state->hydration.player;
	state->fresh_credits = player->credits;
	memcpy(state->fresh_credits_raw,
	    player->record.bytes + YT_F81, sizeof(state->fresh_credits_raw));
	state->summed_credits = take_all_single_add(player->credits,
	    state->argument);
	state->result_credits = floorf(state->summed_credits);
	if (qb_mbf32_encode(state->summed_credits,
	    state->summed_credits_raw) == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(state->result_credits,
	    state->result_credits_raw) == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "credit mutation result MBF32");
	player->credits = qb_mbf32_decode(state->result_credits_raw);
	if (!yt_record_set_raw_number(&player->record, YT_F81,
	    state->result_credits_raw))
		return startup_configuration_error(error, YT_RANGE,
		    "credit mutation overlay");
	state->overlay_applied = true;
	state->write_attempted = true;
	if (!ops->write_player(context, state->hydration.player_record,
	    &player->record, error))
		return false;
	state->written = true;
	return true;
}

double
yt_planet_productivity_units(double spend)
{
	return take_all_double_div(spend, 250.0);
}

void
yt_planet_productivity_cache(float rate[10], double units, float delta[4])
{
	static const float plasma_multiplier = 0x1.0c6f7ap-18f;
	float old_sum;
	float new_sum;
	float old_value;
	float new_value;
	size_t index;

	if (rate == NULL || delta == NULL)
		return;
	old_sum = take_all_single_add(take_all_single_add(rate[1], rate[2]),
	    rate[3]);
	for (index = 1; index <= 3U; ++index)
		rate[index] = (float)take_all_double_add((double)rate[index],
		    units);
	new_sum = take_all_single_add(take_all_single_add(rate[1], rate[2]),
	    rate[3]);
	delta[0] = take_all_single_sub(floorf(new_sum), floorf(old_sum));
	new_value = floorf(take_all_single_div(new_sum, 2500.0f));
	old_value = floorf(take_all_single_div(old_sum, 2500.0f));
	delta[1] = take_all_single_sub(new_value, old_value);
	new_value = floorf(take_all_single_div(new_sum, 25000.0f));
	old_value = floorf(take_all_single_div(old_sum, 25000.0f));
	delta[2] = take_all_single_sub(new_value, old_value);
	new_value = floorf(take_all_single_mul(new_sum, plasma_multiplier));
	old_value = floorf(take_all_single_mul(old_sum, plasma_multiplier));
	delta[3] = take_all_single_sub(new_value, old_value);
}

float
yt_planet_productivity_credit_argument(double units)
{
	volatile double cost = units * 250.0;
	volatile float single_cost = (float)cost;

	return -single_cost;
}

void
yt_planet_productivity_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10])
{
	yt_planet_transfer_cargo_planet_overlay(planet, rate, quantity,
	    contribution);
}

bool
yt_planet_rename_protected(float current_record, float planet_offset,
    float total_record_marker)
{
	volatile float relative = current_record - planet_offset;
	volatile float marker_minus_one = total_record_marker + -1.0f;

	return relative == 1.0f || current_record == total_record_marker
	    || current_record == marker_minus_one;
}

enum yt_planet_rename_name_result
yt_planet_rename_prepare_name(char *name, size_t *length)
{
	size_t normalized;

	if (name == NULL || length == NULL)
		return YT_PLANET_RENAME_EMPTY;
	normalized = qb_title_case_n((uint8_t *)name, strlen(name));
	name[normalized] = '\0';
	if (normalized == 0U) {
		*length = 0U;
		return YT_PLANET_RENAME_EMPTY;
	}
	if (strcmp(name, "The Wanderer") == 0
	    || strcmp(name, "Xannoron") == 0
	    || strcmp(name, "Mercenary Base") == 0) {
		*length = normalized;
		return YT_PLANET_RENAME_RESERVED;
	}
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	name[normalized] = '\0';
	*length = normalized;
	return YT_PLANET_RENAME_ACCEPTED;
}

void
yt_planet_rename_overlay(struct yt_planet *planet, const char *name,
    size_t length)
{
	if (planet == NULL || name == NULL)
		return;
	if (length > YT_TEXT_FIELD_SIZE)
		length = YT_TEXT_FIELD_SIZE;
	memcpy(planet->name, name, length);
	planet->name[length] = '\0';
	planet->name_length = (float)length;
}

bool
yt_clearance_candidate_needed(size_t item, float trigger_draw,
    float discount, bool create)
{
	static const float trigger[4] = {
		0.7900000214576721f, 0.7900000214576721f,
		0.8399999737739563f, 0.8899999856948853f
	};

	return item < 4U && trigger_draw > trigger[item]
	    && discount == 0.0f && create;
}

bool
yt_clearance_normalize(size_t item, float *discount)
{
	static const float maximum[4] = {
		0.9509999752044678f, 0.9800000190734863f,
		0.800000011920929f, 0.8999999761581421f
	};

	if (item >= 4U || discount == NULL)
		return false;
	if (*discount < 0.10000000149011612f
	    || *discount > maximum[item]) {
		*discount = 0.0f;
		return false;
	}
	return true;
}

float
yt_clearance_percentage(float discount)
{
	return floorf(take_all_single_mul(100.0f, discount));
}

void
yt_earth_prices(const float discount[4], float price[4])
{
	if (discount == NULL || price == NULL)
		return;
	price[0] = floorf(take_all_single_sub(250.0f,
	    take_all_single_mul(250.0f, discount[0])));
	price[1] = floorf(take_all_single_sub(50.0f,
	    take_all_single_mul(50.0f, discount[1])));
	price[2] = floorf(take_all_single_mul(50.0f,
	    take_all_single_sub(1.0f, discount[2])));
	price[3] = floorf(take_all_single_mul(200.5f,
	    take_all_single_sub(1.0f, discount[3])));
}

double
yt_earth_affordable(float credits, float price)
{
	return floor(take_all_double_div((double)credits, (double)price));
}

int
yt_earth_selector_position(const char *command)
{
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr("LM0C", command);
	return position == NULL ? 0 : (int)(position - "LM0C") + 1;
}

float
yt_earth_purchase_quantity(double value)
{
	return (float)floor(value);
}

float
yt_earth_receipt_amount(float owner, int buyer_record, float cost)
{
	if (owner == 0.0f)
		return 0.0f;
	if (owner == (float)buyer_record)
		return floorf(take_all_single_mul(0.009999999776482582f, cost));
	return cost;
}

float
yt_earth_cloak_points(float cloak)
{
	return floorf(take_all_single_mul(50.0f, cloak));
}

float
yt_earth_cloak_default(float deficit, float credits)
{
	if (take_all_single_mul(deficit, 1000.0f) > credits)
		return (float)yt_earth_affordable(credits, 1000.0f);
	return deficit;
}

float
yt_earth_cloak_overlay(float points, float quantity)
{
	return take_all_single_div(floorf(take_all_single_add(points, quantity)),
	    50.0f);
}

void
yt_earth_supply_overlay(struct yt_player *player, int choice, float quantity)
{
	if (player == NULL)
		return;
	if (choice == 3)
		player->fighters = take_all_single_add(player->fighters, quantity);
	else if (choice == 7)
		player->ground_forces = floorf(take_all_single_add(
		    player->ground_forces, quantity));
	else if (choice == 8)
		player->shields = floorf(take_all_single_add(
		    player->shields, quantity));
}

bool
yt_earth_anti_cloak_run(struct yt_earth_anti_cloak_state *state,
    const struct yt_earth_anti_cloak_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t activation[] =
	    "ti-Cloaking device activated!\xd4" "D";
	static const uint8_t waves[] =
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl";
	static const uint8_t uncloaked[] = " is uncloaked!";
	static const uint8_t none[] = "Too bad noone was cloaked anyhow!";
	static const uint8_t fade[] = "...the effect fades.";
	uint8_t row[YT_TEXT_FIELD_SIZE + sizeof(uncloaked) - 1U];

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->mutate_credits == NULL || ops->present == NULL
	    || ops->sound == NULL || state->cloak_cache == NULL)
		return false;
	state->counter = 2.0f;
	state->reported = false;
	state->field_record = 0.0f;
	state->credit_argument = 0.0f;
	state->credit_loaded = false;
	if (!ops->present(context, activation, sizeof(activation) - 1U,
	    state->foreground, false, error)
	    || !ops->present(context, NULL, 0U, state->foreground, false,
	    error))
		return false;
	state->foreground = 2.0f;
	if (!ops->present(context, waves, sizeof(waves) - 1U,
	    state->foreground, true, error)
	    || !ops->present(context, NULL, 0U, state->foreground, false,
	    error)
	    || !ops->sound(context, 2.0f, error))
		return false;
	state->foreground = 6.0f;
	while (state->counter <= state->player_terminal) {
		bool overflow;
		int32_t converted = qb_cint_mode((double)state->counter,
		    state->conversion_mode, &overflow);
		size_t index;

		if (overflow || converted < 0
		    || (size_t)converted >= state->cloak_cache_count) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "Anti-Cloak cache index");
			}
			return false;
		}
		index = (size_t)converted;
		if (state->cloak_cache[index] > 0.0f) {
			int32_t converted_length;
			size_t name_length;

			state->cloak_cache[index] = 0.0f;
			if (!ops->read_player(context, state->counter,
			    &state->field_player, error))
				return false;
			state->field_record = state->counter;
			if (state->field_player.killed_by == 0.0f) {
				converted_length = qb_cint_mode(
				    (double)state->field_player.name_length,
				    state->conversion_mode, &overflow);
				if (overflow || converted_length < 0) {
					if (error != NULL) {
						error->status = YT_RANGE;
						(void)snprintf(error->operation,
						    sizeof(error->operation), "%s",
						    "Anti-Cloak player name length");
					}
					return false;
				}
				name_length = (size_t)converted_length;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				memcpy(row, state->field_player.record.bytes,
				    name_length);
				memcpy(row + name_length, uncloaked,
				    sizeof(uncloaked) - 1U);
				if (!ops->present(context, row,
				    name_length + sizeof(uncloaked) - 1U,
				    state->foreground, true, error)
				    || !ops->sound(context, 1.0f, error))
					return false;
				state->reported = true;
			}
		}
		state->counter = take_all_single_add(state->counter, 1.0f);
	}
	state->foreground = 2.0f;
	if (!state->reported
	    && (!ops->present(context, NULL, 0U, state->foreground, false,
	    error)
	    || !ops->present(context, none, sizeof(none) - 1U,
	    state->foreground, false, error)))
		return false;
	if (!ops->present(context, NULL, 0U, state->foreground, false, error)
	    || !ops->present(context, fade, sizeof(fade) - 1U,
	    state->foreground, true, error)
	    || !ops->sound(context, 5.0f, error))
		return false;
	state->credit_argument = -state->price;
	{
		bool completed = ops->mutate_credits(context,
		    state->current_record, state->credit_argument,
		    &state->field_player, &state->credit_loaded, error);

		if (state->credit_loaded)
			state->field_record = state->current_record;
		if (!completed)
			return false;
	}
	if (!state->credit_loaded)
		return false;
	state->foreground = 3.0f;
	return true;
}

int
yt_lottery_match_count(const int winning[6], const char ticket[6],
    bool matched_winning[6])
{
	bool used_winning[6] = {0};
	bool used_ticket[6] = {0};
	int matches = 0;
	int index;

	if (winning == NULL || ticket == NULL || matched_winning == NULL)
		return 0;
	memset(matched_winning, 0, 6U * sizeof(*matched_winning));
	for (index = 0; index < 6; ++index) {
		int candidate;

		for (candidate = 0; candidate < 6; ++candidate) {
			if (!used_winning[index] && !used_ticket[candidate]
			    && winning[index] == ticket[candidate] - '0') {
				used_winning[index] = true;
				used_ticket[candidate] = true;
				matched_winning[index] = true;
				++matches;
				break;
			}
		}
	}
	return matches;
}

float
yt_lottery_award(int matches)
{
	static const float awards[6] = {
		100.0f, 1000.0f, 10000.0f, 100000.0f,
		1000000.0f, 100000000.0f
	};

	return matches < 1 || matches > 6 ? 0.0f : awards[matches - 1];
}

bool
yt_player_stored_name(const struct yt_player *player,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint(player->name_length, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "player name LEFT$ length");
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
stored_record_name(const struct yt_record *record, float raw_length,
    const char *operation, uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint(raw_length, &overflow);
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
	return stored_record_name(&port->record, port->name_length,
	    "port name LEFT$ length", name, length, error);
}

bool
yt_planet_stored_name(const struct yt_planet *planet,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int requested = (int)qb_cint(planet->name_length, &overflow);
	size_t stored;

	if (length != NULL)
		*length = 0U;
	if (overflow || requested < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "planet name LEFT$ length");
		}
		return false;
	}
	stored = (size_t)requested;
	if (stored > YT_TEXT_FIELD_SIZE)
		stored = YT_TEXT_FIELD_SIZE;
	if (stored > 0U && name != NULL)
		memcpy(name, planet->record.bytes, stored);
	if (length != NULL)
		*length = stored;
	return true;
}

struct sector_row_builder {
	uint8_t *row;
	size_t capacity;
	size_t length;
};

static bool
sector_row_append(struct sector_row_builder *builder, const void *data,
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

static bool
sector_row_number(struct sector_row_builder *builder, float value,
    bool promoted)
{
	char number[64];
	int length = promoted
	    ? qb_str_double(number, sizeof(number), (double)value)
	    : qb_str_single(number, sizeof(number), value);

	return length >= 0 && sector_row_append(builder, number, (size_t)length);
}

bool
yt_sector_mine_warning_row(float mines, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "** WARNING! SECTOR HAS";
	static const uint8_t suffix[] = " MINES! **";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder, mines, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
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
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (port == NULL || !yt_port_stored_name(port, name, &name_length,
	    error))
		return false;
	if (port->commodity_class == 1.0f)
		commodity = equipment;
	else if (port->commodity_class == 2.0f)
		commodity = organics;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_append(&builder, name, name_length)
	    || !sector_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !sector_row_append(&builder, commodity, sizeof(ore) - 1U))
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
	struct sector_row_builder builder = {row, capacity, 0U};
	float forces;

	if (length != NULL)
		*length = 0U;
	if (planet == NULL || !yt_planet_stored_name(planet, name,
	    &name_length, error))
		return false;
	forces = (float)qb_int((double)planet->ground_forces);
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_append(&builder, name, name_length)
	    || !sector_row_append(&builder, separator, sizeof(separator) - 1U)
	    || !sector_row_number(&builder, forces, false))
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
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (player == NULL || !yt_player_stored_name(player, name,
	    &name_length, error))
		return false;
	if (!sector_row_append(&builder, indent, sizeof(indent) - 1U)
	    || !sector_row_append(&builder, name, name_length))
		return false;
	if (player->team > 0.0f
	    && (!sector_row_append(&builder, team, sizeof(team) - 1U)
	    || !sector_row_number(&builder, player->team, false)))
		return false;
	if (!sector_row_append(&builder, fighters, sizeof(fighters) - 1U)
	    || !sector_row_number(&builder, player->fighters, true)
	    || !sector_row_append(&builder, shields, sizeof(shields) - 1U)
	    || !sector_row_number(&builder, player->shields, false))
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
	struct sector_row_builder builder = {row, capacity, 0U};
	struct sector_row_builder scratch_builder = {
		scratch, scratch_capacity, 0U
	};
	bool changed = false;

	if (length != NULL)
		*length = 0U;
	if (scratch_changed != NULL)
		*scratch_changed = false;
	if (sector == NULL
	    || !sector_row_number(&builder, sector->fighters, true)
	    || !sector_row_append(&builder, belonging,
	    sizeof(belonging) - 1U))
		return false;
	if (sector->fighter_owner == (float)current_player_record) {
		if (!sector_row_append(&builder, self, sizeof(self) - 1U))
			return false;
	}
	else {
		if (sector->fighter_owner == -1.0f) {
			if (!sector_row_append(&scratch_builder, xannor,
			    sizeof(xannor) - 1U))
				return false;
			changed = true;
		}
		else if (sector->fighter_owner == -2.0f) {
			if (!sector_row_append(&scratch_builder, mercenaries,
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
			    || !sector_row_append(&scratch_builder, owner_name,
			    owner_name_length))
				return false;
			changed = true;
			if (owner->team != 0.0f) {
				number_length = qb_str_single(number, sizeof(number),
				    owner->team);
				if (number_length < 1 || team_overlay == NULL
				    || !sector_row_append(&scratch_builder,
				    team_prefix, sizeof(team_prefix) - 1U)
				    || !sector_row_append(&scratch_builder,
				    number + 1, (size_t)number_length - 1U)
				    || !sector_row_append(&scratch_builder, "]", 1U))
					return false;
				team_name_length = (int)qb_cint(
				    yt_record_get_number(&team_overlay->record,
				    YT_F73), &overflow);
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
				    && (!sector_row_append(&scratch_builder,
				    overlay_prefix, sizeof(overlay_prefix) - 1U)
				    || !sector_row_append(&scratch_builder,
				    team_overlay->record.bytes, stored_team_length)
				    || !sector_row_append(&scratch_builder, "]", 1U)))
					return false;
			}
		}
		if (!sector_row_append(&builder, scratch_builder.row,
		    scratch_builder.length)
		    || !sector_row_append(&builder, ")", 1U))
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

bool
yt_projectile_defense_row(float sector, const uint8_t *owner,
    size_t owner_length, double fighters, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t prefix[] = "Sector:";
	static const uint8_t owner_prefix[] = " defended by ";
	static const uint8_t fighter_prefix[] = " with";
	static const uint8_t suffix[] = " fighters.";
	char sector_text[64];
	char fighter_text[64];
	int sector_length;
	int fighter_length;
	size_t needed;
	size_t position = 0U;

	if (length == NULL || (owner == NULL && owner_length != 0U))
		return false;
	*length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	fighter_length = qb_str_double(fighter_text, sizeof(fighter_text),
	    fighters);
	if (sector_length < 0 || fighter_length < 0)
		return false;
	needed = sizeof(prefix) - 1U + (size_t)sector_length
	    + sizeof(owner_prefix) - 1U + owner_length
	    + sizeof(fighter_prefix) - 1U + (size_t)fighter_length
	    + sizeof(suffix) - 1U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	memcpy(row + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(row + position, owner_prefix, sizeof(owner_prefix) - 1U);
	position += sizeof(owner_prefix) - 1U;
	if (owner_length != 0U)
		memcpy(row + position, owner, owner_length);
	position += owner_length;
	memcpy(row + position, fighter_prefix, sizeof(fighter_prefix) - 1U);
	position += sizeof(fighter_prefix) - 1U;
	memcpy(row + position, fighter_text, (size_t)fighter_length);
	position += (size_t)fighter_length;
	memcpy(row + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*length = position;
	return true;
}

bool
yt_projectile_candidate_admitted(int candidate, float cached_cloak,
    int xannor_provoker)
{
	return (cached_cloak == 0.0f || candidate == xannor_provoker)
	    && (xannor_provoker == 0 || candidate == xannor_provoker);
}

enum yt_projectile_candidate_route
yt_projectile_candidate_route(int candidate, int shooter,
    float cached_sector, float sector, float remaining)
{
	if (cached_sector != sector)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	if (remaining <= 0.0f)
		return YT_PROJECTILE_CANDIDATE_TERMINATE;
	if (candidate == shooter)
		return YT_PROJECTILE_CANDIDATE_SKIP;
	return YT_PROJECTILE_CANDIDATE_FRIENDSHIP;
}

bool
yt_projectile_player_survives(float shields)
{
	return shields >= 1.0f;
}

bool
yt_projectile_salvage_admitted(int counterattack, int xannor_provoker)
{
	return counterattack == 0 && xannor_provoker == 0;
}

enum yt_projectile_death_route
yt_projectile_death_continuation(float remaining, float saved_mines)
{
	if (remaining > 0.0f && saved_mines > 0.0f)
		return YT_PROJECTILE_DEATH_REENTER_MINES;
	if (remaining < 1.0f)
		return YT_PROJECTILE_DEATH_RETURN;
	return YT_PROJECTILE_DEATH_NEXT_PLAYER;
}

bool
yt_projectile_survivor_sets_counterattack(int shooter)
{
	return shooter != -1;
}

bool
yt_projectile_damage_iteration(float counter, float saved_missiles)
{
	return counter <= saved_missiles;
}

static float
projectile_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
projectile_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
projectile_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

bool
yt_projectile_is_black_hole(float hop, float first, float second)
{
	return hop == first || hop == second;
}

bool
yt_projectile_cruise_reroute_run(
    struct yt_projectile_cruise_reroute_state *state,
    const struct yt_projectile_cruise_reroute_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prefix[] =
	    "The missiles are deflected by a black hole in sector";
	static const uint8_t suffix[] = "!";
	uint8_t row[160];
	char number[64];
	int number_length;
	size_t row_length;
	float draw;
	float span;
	float selected;

	if (state == NULL || ops == NULL || state->origin == NULL
	    || state->destination == NULL || ops->line == NULL
	    || ops->attention == NULL || ops->random == NULL)
		return false;
	if (!ops->line(context, NULL, 0U, error))
		return false;
	number_length = qb_str_single(number, sizeof(number), state->hop);
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length + sizeof(suffix) - 1U
	    > sizeof(row))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	row_length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	if (!ops->attention(context, row, row_length, error))
		return false;
	*state->origin = state->hop;
	if (!ops->random(context, &draw, error))
		return false;
	span = projectile_single_sub(state->port_record_offset,
	    state->sector_record_offset);
	selected = floorf(projectile_single_mul(draw, span));
	*state->destination = projectile_single_add(selected, 1.0f);
	return true;
}

bool
yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker)
{
	return hop < 8.0f && destination < 8.0f
	    && counterattack == 0 && xannor_provoker == 0;
}

bool
yt_projectile_union_police_run(
    struct yt_projectile_union_police_state *state,
    yt_projectile_cruise_reroute_output_fn present, void *context,
    struct yt_error *error)
{
	static const uint8_t row[] =
	    "The Union Police have destroyed the Missiles!";

	if (state == NULL || present == NULL)
		return false;
	state->intercepted = false;
	if (!yt_projectile_union_police_admitted(state->hop,
	    state->destination, state->counterattack, state->xannor_provoker))
		return true;
	state->intercepted = true;
	return present(context, row, sizeof(row) - 1U, error);
}

bool
yt_projectile_sector_probe_run(struct yt_projectile_sector_probe_state *state,
    struct yt_error *error)
{
	bool overflow;

	if (state == NULL || state->sector == NULL
	    || state->sector_cache == NULL || state->cloak_cache == NULL)
		return false;
	state->presence = 0.0f;
	state->matched_player = 0.0f;
	if (state->sector->mines > 0.0f || state->sector->fighters > 0.0f
	    || state->sector->port > 0.0f || state->sector->planet > 0.0f)
		state->presence = 1.0f;
	state->counter = 2.0f;
	while (state->counter <= state->player_terminal) {
		int candidate = (int)qb_cint(state->counter, &overflow);

		if (overflow || candidate < 0
		    || (size_t)candidate >= state->cache_count) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "projectile sector-probe cache index");
			}
			return false;
		}
		if (state->sector_cache[candidate] == state->hop
		    && (state->cloak_cache[candidate] == 0.0f
		    || state->counter == state->xannor_provoker)) {
			state->presence = 1.0f;
			state->matched_player = state->counter;
			break;
		}
		state->counter = projectile_single_add(state->counter, 1.0f);
	}
	return true;
}

bool
yt_projectile_plasma_fighter_run(
    struct yt_projectile_plasma_fighter_state *state,
    const struct yt_projectile_plasma_fighter_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t you[] = "YOU";
	static const uint8_t direct_prefix[] =
	    "The plasma bolts destroyed";
	static const uint8_t direct_suffix[] = " fighters!";
	static const uint8_t news_infix[] =
	    "'s plasma bolts destroyed";
	static const uint8_t news_sector[] = " fighters in sector";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x10, 0x00};
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[256];
	char destroyed_text[64];
	char sector_text[64];
	size_t owner_length = sizeof(xannor) - 1U;
	size_t row_length;
	int destroyed_length;
	int sector_length;

	if (state == NULL || ops == NULL || state->energy == NULL
	    || state->bold == NULL
	    || (state->attacker == NULL && state->attacker_length != 0U)
	    || ops->owner == NULL || ops->present == NULL || ops->sound == NULL
	    || ops->random == NULL || ops->news == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->victory == NULL)
		return false;
	state->destroyed = 0.0;
	state->remaining_fighters = state->fighters;
	state->victory_called = false;
	state->route = YT_PROJECTILE_PLASMA_FIGHTER_CONTINUE_SECTOR;
	if (!(state->fighters > 0.0))
		return true;

	memcpy(owner_name, xannor, owner_length);
	if (state->owner == -2.0f) {
		memcpy(owner_name, mercenaries, sizeof(mercenaries) - 1U);
		owner_length = sizeof(mercenaries) - 1U;
	}
	else if (state->owner > 1.0f) {
		if (!ops->owner(context, state->owner, owner_name, &owner_length,
		    error) || owner_length > sizeof(owner_name))
			return false;
	}
	if (state->owner == (float)state->shooter) {
		memcpy(owner_name, you, sizeof(you) - 1U);
		owner_length = sizeof(you) - 1U;
	}
	if (!yt_projectile_defense_row(state->sector, owner_name, owner_length,
	    state->fighters, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_PROJECTILE_PLASMA_FIGHTER_ENCOUNTER, error))
		return false;
	*state->bold = 1.0f;
	if (!ops->sound(context, 2.0f, error))
		return false;
	if (!(*state->energy > 0.0))
		return true;

	while (*state->energy > 0.0
	    && state->destroyed < state->fighters) {
		float draw;

		state->destroyed += floor(*state->energy / 5000.0) + 1.0;
		if (!ops->random(context, &draw, error))
			return false;
		*state->energy -= (double)projectile_single_mul(draw, 25000.0f);
	}
	if (*state->energy < 0.0)
		*state->energy = 0.0;
	if (state->destroyed > state->fighters)
		state->destroyed = state->fighters;
	destroyed_length = qb_str_double(destroyed_text,
	    sizeof(destroyed_text), state->destroyed);
	if (destroyed_length < 0
	    || sizeof(direct_prefix) - 1U + (size_t)destroyed_length
	    + sizeof(direct_suffix) - 1U > sizeof(row))
		return false;
	memcpy(row, direct_prefix, sizeof(direct_prefix) - 1U);
	memcpy(row + sizeof(direct_prefix) - 1U, destroyed_text,
	    (size_t)destroyed_length);
	row_length = sizeof(direct_prefix) - 1U + (size_t)destroyed_length;
	memcpy(row + row_length, direct_suffix, sizeof(direct_suffix) - 1U);
	row_length += sizeof(direct_suffix) - 1U;
	if (!ops->present(context, row, row_length,
	    YT_PROJECTILE_PLASMA_FIGHTER_DAMAGE, error))
		return false;

	state->remaining_fighters = state->fighters - state->destroyed;
	if (state->destroyed > 9.0) {
		sector_length = qb_str_single(sector_text, sizeof(sector_text),
		    state->sector);
		if (sector_length < 0
		    || state->attacker_length + sizeof(news_infix) - 1U
		    + (size_t)destroyed_length + sizeof(news_sector) - 1U
		    + (size_t)sector_length + 1U > sizeof(row))
			return false;
		row_length = 0U;
		if (state->attacker_length != 0U) {
			memcpy(row, state->attacker, state->attacker_length);
			row_length = state->attacker_length;
		}
		memcpy(row + row_length, news_infix, sizeof(news_infix) - 1U);
		row_length += sizeof(news_infix) - 1U;
		memcpy(row + row_length, destroyed_text,
		    (size_t)destroyed_length);
		row_length += (size_t)destroyed_length;
		memcpy(row + row_length, news_sector, sizeof(news_sector) - 1U);
		row_length += sizeof(news_sector) - 1U;
		memcpy(row + row_length, sector_text, (size_t)sector_length);
		row_length += (size_t)sector_length;
		row[row_length++] = '!';
		if (!ops->news(context, row, row_length, error))
			return false;
	}

	if (!ops->read_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	state->persistence.fighters = (float)state->remaining_fighters;
	if (!yt_record_set_number(&state->persistence.record, YT_F81,
	    state->persistence.fighters))
		return false;
	if (state->remaining_fighters == 0.0) {
		state->persistence.fighters = 0.0f;
		state->persistence.fighter_owner = 0.0f;
		if (!yt_record_set_raw_number(&state->persistence.record, YT_F81,
		    dirty_zero)
		    || !yt_record_set_raw_number(&state->persistence.record, YT_F85,
		    dirty_zero))
			return false;
	}
	if (!ops->write_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	if (state->remaining_fighters == 0.0
	    && state->sector == state->headquarters) {
		state->victory_called = true;
		if (!ops->victory(context, error))
			return false;
	}
	if (*state->energy < 1.0)
		state->route = YT_PROJECTILE_PLASMA_FIGHTER_FOOTER;
	return true;
}

bool
yt_projectile_plasma_mine_run(struct yt_projectile_plasma_mine_state *state,
    const struct yt_projectile_plasma_mine_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t entry_infix[] =
	    "'s Plasma Bolts hit sector mines in sector";
	static const uint8_t news_infix[] =
	    "'s plasma bolts destroyed";
	static const uint8_t direct_prefix[] =
	    "The plasma bolts destroyed";
	static const uint8_t result_infix[] = " mines in sector";
	uint8_t row[256];
	char destroyed_text[64];
	char sector_text[64];
	size_t row_length;
	int destroyed_length;
	int sector_length;

	if (state == NULL || ops == NULL || state->energy == NULL
	    || (state->attacker == NULL && state->attacker_length != 0U)
	    || ops->sound == NULL || ops->news == NULL || ops->random == NULL
	    || ops->present == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL)
		return false;
	state->destroyed = 0.0f;
	state->remaining_mines = (float)state->mines;
	state->route = YT_PROJECTILE_PLASMA_MINE_CONTINUE_PLAYERS;
	if (!(state->mines > 0.0))
		return true;
	if (!ops->sound(context, 5.0f, error))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    state->sector);
	if (sector_length < 0
	    || state->attacker_length + sizeof(entry_infix) - 1U
	    + (size_t)sector_length + 1U > sizeof(row))
		return false;
	row_length = 0U;
	if (state->attacker_length != 0U) {
		memcpy(row, state->attacker, state->attacker_length);
		row_length = state->attacker_length;
	}
	memcpy(row + row_length, entry_infix, sizeof(entry_infix) - 1U);
	row_length += sizeof(entry_infix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	if (!ops->news(context, row, row_length, error))
		return false;

	while (*state->energy > 0.0
	    && (double)state->destroyed < state->mines) {
		float draw;
		volatile double quantum = floor(*state->energy * 0.000001);
		volatile double accumulated = (double)state->destroyed + quantum;

		state->destroyed = (float)(accumulated + 1.0);
		if (!ops->random(context, &draw, error))
			return false;
		*state->energy -= (double)projectile_single_mul(draw, 25000.0f);
	}
	if (*state->energy < 0.0)
		*state->energy = 0.0;
	if ((double)state->destroyed > state->mines)
		state->destroyed = (float)state->mines;
	destroyed_length = qb_str_single(destroyed_text,
	    sizeof(destroyed_text), state->destroyed);
	if (destroyed_length < 0
	    || state->attacker_length + sizeof(news_infix) - 1U
	    + (size_t)destroyed_length + sizeof(result_infix) - 1U
	    + (size_t)sector_length + 1U > sizeof(row))
		return false;
	row_length = 0U;
	if (state->attacker_length != 0U) {
		memcpy(row, state->attacker, state->attacker_length);
		row_length = state->attacker_length;
	}
	memcpy(row + row_length, news_infix, sizeof(news_infix) - 1U);
	row_length += sizeof(news_infix) - 1U;
	memcpy(row + row_length, destroyed_text, (size_t)destroyed_length);
	row_length += (size_t)destroyed_length;
	memcpy(row + row_length, result_infix, sizeof(result_infix) - 1U);
	row_length += sizeof(result_infix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	if (!ops->news(context, row, row_length, error))
		return false;
	if (sizeof(direct_prefix) - 1U + (size_t)destroyed_length
	    + sizeof(result_infix) - 1U + (size_t)sector_length + 1U
	    > sizeof(row))
		return false;
	memcpy(row, direct_prefix, sizeof(direct_prefix) - 1U);
	memcpy(row + sizeof(direct_prefix) - 1U, destroyed_text,
	    (size_t)destroyed_length);
	row_length = sizeof(direct_prefix) - 1U + (size_t)destroyed_length;
	memcpy(row + row_length, result_infix, sizeof(result_infix) - 1U);
	row_length += sizeof(result_infix) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	if (!ops->present(context, row, row_length, error))
		return false;

	if (!ops->read_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	state->remaining_mines = projectile_single_sub((float)state->mines,
	    state->destroyed);
	state->persistence.mines = state->remaining_mines;
	if (!yt_record_set_number(&state->persistence.record, YT_F129,
	    state->persistence.mines)
	    || !ops->write_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	if (*state->energy < 1.0)
		state->route = YT_PROJECTILE_PLASMA_MINE_FOOTER;
	return true;
}

bool
yt_projectile_plasma_dispatch_run(
    struct yt_projectile_plasma_dispatch_state *state,
    struct yt_error *error)
{
	bool overflow;

	if (state == NULL || state->sector_cache == NULL)
		return false;
	state->selected_player = 0;
	if (state->resume_after_player && state->energy < 1.0) {
		state->route = YT_PROJECTILE_PLASMA_DISPATCH_FOOTER;
		return true;
	}
	if (state->resume_after_player)
		state->counter = projectile_single_add(state->counter, 1.0f);
	else
		state->counter = 2.0f;
	while (state->counter <= state->player_terminal) {
		int candidate = qb_cint(state->counter, &overflow);

		if (overflow || candidate < 0
		    || (size_t)candidate >= state->cache_count) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma player-dispatch cache index");
			}
			return false;
		}
		if (state->sector_cache[candidate] == state->sector
		    && state->energy > 0.0) {
			state->selected_player = candidate;
			state->route = YT_PROJECTILE_PLASMA_DISPATCH_PLAYER;
			return true;
		}
		state->counter = projectile_single_add(state->counter, 1.0f);
	}
	if (!(state->energy > 0.0)) {
		state->route = YT_PROJECTILE_PLASMA_DISPATCH_FOOTER;
		return true;
	}
	if (qb_cint(state->planet_link, &overflow) != 0) {
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma player-dispatch planet CINT");
			}
			return false;
		}
		state->route = YT_PROJECTILE_PLASMA_DISPATCH_PLANET;
	}
	else {
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma player-dispatch planet CINT");
			}
			return false;
		}
		state->route = YT_PROJECTILE_PLASMA_DISPATCH_NEXT_HOP;
	}
	return true;
}

bool
yt_projectile_plasma_player_run(
    struct yt_projectile_plasma_player_state *state,
    const struct yt_projectile_plasma_player_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t second_prefix[] = "shields to";
	static const uint8_t second_infix[] = " units and destroying";
	static const uint8_t second_suffix[] = " fighters!";
	struct yt_player player;
	uint8_t victim[YT_TEXT_FIELD_SIZE];
	uint8_t news_row[256];
	uint8_t direct_row[256];
	uint8_t second_row[256];
	char shield_text[64];
	char fighter_text[64];
	size_t victim_length;
	size_t news_length;
	size_t direct_length;
	size_t second_length;
	int shield_length;
	int fighter_length;

	if (state == NULL || ops == NULL || state->energy == NULL
	    || (state->attacker == NULL && state->attacker_length != 0U)
	    || ops->read_player == NULL || ops->write_player == NULL
	    || ops->color == NULL || ops->sound == NULL || ops->random == NULL
	    || ops->news == NULL || ops->present == NULL)
		return false;
	memset(&state->persistence, 0, sizeof(state->persistence));
	state->saved_foreground = state->foreground;
	state->destroyed_fighters = 0.0;
	state->destroyed_shields = 0.0f;
	state->remaining_fighters = 0.0;
	state->remaining_shields = 0.0f;
	state->route = YT_PROJECTILE_PLASMA_PLAYER_CONTINUE_DISPATCH;
	if (!ops->read_player(context, state->target, &player, error))
		return false;
	state->original_fighters = (double)player.fighters;
	state->original_shields = player.shields;
	ops->color(context, 5.0f);
	if (!ops->sound(context, 2.0f, error))
		return false;
	while (*state->energy > 0.0
	    && state->destroyed_fighters < state->original_fighters) {
		float draw;
		volatile double quantum = floor(*state->energy / 5000.0);
		volatile double accumulated = state->destroyed_fighters + quantum;

		state->destroyed_fighters = accumulated + 1.0;
		if (!ops->random(context, &draw, error))
			return false;
		*state->energy -= (double)projectile_single_mul(draw, 25000.0f);
	}
	while (*state->energy > 0.0
	    && state->destroyed_shields < state->original_shields) {
		float draw;
		volatile double quantum = floor(*state->energy / 10000.0);
		volatile double accumulated =
		    (double)state->destroyed_shields + quantum;

		state->destroyed_shields = (float)(accumulated + 1.0);
		if (!ops->random(context, &draw, error))
			return false;
		*state->energy -= (double)projectile_single_mul(draw, 25000.0f);
	}
	if (state->destroyed_fighters > state->original_fighters)
		state->destroyed_fighters = state->original_fighters;
	if (state->destroyed_shields > state->original_shields)
		state->destroyed_shields = state->original_shields;
	state->remaining_fighters = state->original_fighters
	    - state->destroyed_fighters;
	state->remaining_shields = projectile_single_sub(state->original_shields,
	    state->destroyed_shields);

	if (!ops->read_player(context, state->target, &player, error)
	    || !yt_player_stored_name(&player, victim, &victim_length, error)
	    || !yt_projectile_attack_first_rows(true,
	    state->attacker, state->attacker_length, victim, victim_length,
	    state->sector, news_row, sizeof(news_row), &news_length,
	    direct_row, sizeof(direct_row), &direct_length)
	    || !ops->news(context, news_row, news_length, error)
	    || !ops->present(context, direct_row, direct_length,
	    YT_PROJECTILE_PLASMA_PLAYER_FIRST_ROW, error))
		return false;

	shield_length = qb_str_single(shield_text, sizeof(shield_text),
	    state->remaining_shields);
	fighter_length = qb_str_double(fighter_text, sizeof(fighter_text),
	    state->destroyed_fighters);
	if (shield_length < 0 || fighter_length < 0
	    || sizeof(second_prefix) - 1U + (size_t)shield_length
	    + sizeof(second_infix) - 1U + (size_t)fighter_length
	    + sizeof(second_suffix) - 1U > sizeof(second_row))
		return false;
	memcpy(second_row, second_prefix, sizeof(second_prefix) - 1U);
	second_length = sizeof(second_prefix) - 1U;
	memcpy(second_row + second_length, shield_text, (size_t)shield_length);
	second_length += (size_t)shield_length;
	memcpy(second_row + second_length, second_infix,
	    sizeof(second_infix) - 1U);
	second_length += sizeof(second_infix) - 1U;
	memcpy(second_row + second_length, fighter_text,
	    (size_t)fighter_length);
	second_length += (size_t)fighter_length;
	memcpy(second_row + second_length, second_suffix,
	    sizeof(second_suffix) - 1U);
	second_length += sizeof(second_suffix) - 1U;
	if (!ops->news(context, second_row, second_length, error)
	    || !ops->present(context, second_row, second_length,
	    YT_PROJECTILE_PLASMA_PLAYER_SECOND_ROW, error))
		return false;
	ops->color(context, state->saved_foreground);

	if (state->remaining_shields < 1.0f) {
		state->route = YT_PROJECTILE_PLASMA_PLAYER_KILLED;
		return true;
	}
	if (!ops->read_player(context, state->target, &state->persistence,
	    error))
		return false;
	state->persistence.shields = state->remaining_shields;
	state->persistence.fighters = (float)state->remaining_fighters;
	if (!yt_record_set_number(&state->persistence.record, YT_F53,
	    state->persistence.shields)
	    || !yt_record_set_number(&state->persistence.record, YT_F61,
	    state->persistence.fighters)
	    || !ops->write_player(context, state->target, &state->persistence,
	    error))
		return false;
	if (*state->energy < 1.0)
		state->route = YT_PROJECTILE_PLASMA_PLAYER_FOOTER;
	return true;
}

bool
yt_projectile_plasma_killed_run(
    struct yt_projectile_plasma_killed_state *state,
    const struct yt_projectile_plasma_killed_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t self_row[] = "YOU were destroyed!";
	struct yt_player victim;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t destroyed_row[128];
	uint8_t warning_row[160];
	size_t victim_name_length = 0U;
	size_t destroyed_length = 0U;
	size_t warning_length = 0U;
	bool rows_ready = false;

	if (state == NULL || ops == NULL || state->energy == NULL
	    || state->blink == NULL || state->destroyed == NULL
	    || state->sector_cache == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->death == NULL || ops->sound == NULL || ops->salvage == NULL)
		return false;
	state->self_hit = state->victim == state->shooter;
	state->saved_mines = 0.0f;
	memset(&state->victim_persistence, 0,
	    sizeof(state->victim_persistence));
	memset(&state->mine_persistence, 0, sizeof(state->mine_persistence));
	state->route = YT_PROJECTILE_PLASMA_KILLED_CONTINUE_DISPATCH;
	if (!ops->read_player(context, state->victim, &victim, error))
		return false;
	if (!state->self_hit) {
		if (!yt_player_stored_name(&victim, victim_name,
		    &victim_name_length, error)
		    || !yt_projectile_destroyed_rows(victim_name,
		    victim_name_length, destroyed_row, sizeof(destroyed_row),
		    &destroyed_length, warning_row, sizeof(warning_row),
		    &warning_length))
			return false;
		rows_ready = true;
	}
	state->saved_mines = victim.mines;
	victim.mines = 0.0f;
	victim.danger_scanner = 0.0f;
	if (!yt_record_set_number(&victim.record, YT_F129, 0.0f)
	    || !yt_record_set_number(&victim.record, YT_F93, 0.0f))
		return false;
	state->victim_persistence = victim;
	if (!ops->write_player(context, state->victim,
	    &state->victim_persistence, error))
		return false;

	*state->blink = 1.0f;
	if (state->self_hit) {
		if (!ops->present(context, self_row, sizeof(self_row) - 1U,
		    YT_PROJECTILE_PLASMA_KILLED_SELF_DESTROYED_ROW, error))
			return false;
	}
	else if (!ops->present(context, destroyed_row, destroyed_length,
	    YT_PROJECTILE_PLASMA_KILLED_DESTROYED_ROW, error))
		return false;

	if (state->saved_mines != 0.0f) {
		if (!rows_ready) {
			if (!yt_player_stored_name(&victim, victim_name,
			    &victim_name_length, error)
			    || !yt_projectile_destroyed_rows(victim_name,
			    victim_name_length, destroyed_row, sizeof(destroyed_row),
			    &destroyed_length, warning_row, sizeof(warning_row),
			    &warning_length))
				return false;
		}
		*state->blink = 1.0f;
		if (!ops->present(context, warning_row, warning_length,
		    YT_PROJECTILE_PLASMA_KILLED_WARNING_ROW, error)
		    || !ops->read_sector(context, state->sector,
		    &state->mine_persistence, error))
			return false;
		state->mine_persistence.mines = projectile_single_add(
		    state->mine_persistence.mines, state->saved_mines);
		if (!yt_record_set_number(&state->mine_persistence.record, YT_F129,
		    state->mine_persistence.mines)
		    || !ops->write_sector(context, state->sector,
		    &state->mine_persistence, error))
			return false;
	}

	if (state->self_hit) {
		if (state->shooter < 0
		    || (size_t)state->shooter >= state->cache_count) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma killed self cache index");
			}
			return false;
		}
		*state->destroyed = true;
		state->sector_cache[state->shooter] = 0.0f;
	}
	else {
		if (!ops->death(context, state->victim, state->shooter, error)
		    || !ops->sound(context, 3.0f, error)
		    || !ops->salvage(context, state->victim, state->shooter,
		    error))
			return false;
	}

	if (*state->energy > 0.0 && state->saved_mines > 0.0f)
		state->route = YT_PROJECTILE_PLASMA_KILLED_RELOAD_SECTOR;
	else if (*state->energy < 1.0)
		state->route = YT_PROJECTILE_PLASMA_KILLED_FOOTER;
	return true;
}

bool
yt_projectile_plasma_planet_run(
    struct yt_projectile_plasma_planet_state *state,
    const struct yt_projectile_plasma_planet_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t destroyed_row[] = "The planet was destroyed!!";
	static const uint8_t ground_prefix[] = "Ground forces reduced by";
	static const uint8_t ground_middle[] = " units to";
	struct yt_planet planet;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	uint8_t row[256];
	char first_number[64];
	char second_number[64];
	size_t planet_name_length;
	size_t direct_length;
	size_t news_length;
	size_t row_length;
	int first_length;
	int second_length;
	size_t index;

	if (state == NULL || ops == NULL || state->energy == NULL
	    || (state->attacker == NULL && state->attacker_length != 0U)
	    || ops->update == NULL || ops->read_planet == NULL
	    || ops->write_planet == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->present == NULL
	    || ops->news == NULL || ops->sound == NULL || ops->random == NULL)
		return false;
	state->destroyed = false;
	state->route = YT_PROJECTILE_PLASMA_PLANET_NEXT_HOP;
	memset(&state->persistence, 0, sizeof(state->persistence));
	memset(&state->destruction, 0, sizeof(state->destruction));
	memset(&state->unlink, 0, sizeof(state->unlink));
	if (!ops->update(context, state->planet, &state->stale_ore, error)
	    || !ops->read_planet(context, state->planet, &planet, error))
		return false;
	for (index = 0U; index < 3U; ++index) {
		state->production[index] = planet.production[index];
		state->stock[index] = planet.stock[index];
	}
	state->original_ground = planet.ground_forces;
	state->remaining_ground = state->original_ground;
	if (!yt_planet_stored_name(&planet, planet_name, &planet_name_length,
	    error)
	    || !yt_projectile_planet_attack_rows(true,
	    state->attacker, state->attacker_length,
	    planet_name, planet_name_length, (float)state->sector,
	    direct_row, sizeof(direct_row), &direct_length,
	    news_row, sizeof(news_row), &news_length)
	    || !ops->present(context, direct_row, direct_length,
	    YT_PROJECTILE_PLASMA_PLANET_HIT_ROW, error)
	    || !ops->news(context, news_row, news_length, error)
	    || !ops->sound(context, 2.0f, error))
		return false;

	state->original_productivity = projectile_single_add(
	    projectile_single_add(state->production[0], state->production[1]),
	    state->production[2]);
	while ((state->stale_ore > 0.0f || state->production[1] > 0.0f
	    || state->production[2] > 0.0f) && *state->energy > 0.0) {
		float draw;
		volatile double product = *state->energy * 0.000004;
		float quantity = (float)product;

		state->remaining_ground = projectile_single_sub(
		    state->remaining_ground, quantity);
		for (index = 0U; index < 3U; ++index)
			state->production[index] = projectile_single_sub(
			    state->production[index], quantity);
		if (!ops->random(context, &draw, error))
			return false;
		*state->energy -= (double)projectile_single_mul(draw, 25000.0f);
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (state->production[index] < 0.0f)
			state->production[index] = 0.0f;
		cap = projectile_single_mul(state->production[index], 10.0f);
		if (state->stock[index] > cap)
			state->stock[index] = cap;
	}
	state->remaining_productivity = projectile_single_add(
	    projectile_single_add(state->production[0], state->production[1]),
	    state->production[2]);
	if (!yt_projectile_planet_productivity_row(
	    state->original_productivity, state->remaining_productivity,
	    row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_PROJECTILE_PLASMA_PLANET_PRODUCTIVITY_ROW, error)
	    || !ops->news(context, row, row_length, error)
	    || !ops->read_planet(context, state->planet, &state->persistence,
	    error)
	    || !yt_projectile_planet_productivity_overlay(&state->persistence,
	    state->production, state->stock))
		return false;
	state->remaining_ground = floorf(state->remaining_ground);
	if (state->remaining_ground < 1.0f) {
		state->remaining_ground = 0.0f;
		state->persistence.owner = 0.0f;
		if (!yt_record_set_number(&state->persistence.record, YT_F73,
		    0.0f))
			return false;
	}
	state->persistence.ground_forces = state->remaining_ground;
	if (!yt_record_set_number(&state->persistence.record, YT_F77,
	    state->remaining_ground)
	    || !ops->write_planet(context, state->planet, &state->persistence,
	    error))
		return false;

	if (state->production[0] == 0.0f
	    && state->production[1] == 0.0f
	    && state->production[2] == 0.0f) {
		state->destroyed = true;
		if (!ops->read_planet(context, state->planet,
		    &state->destruction, error))
			return false;
		state->destruction.name_length = 0.0f;
		if (!yt_record_set_number(&state->destruction.record, YT_F85, 0.0f)
		    || !ops->write_planet(context, state->planet,
		    &state->destruction, error)
		    || !ops->read_sector(context, state->sector, &state->unlink,
		    error))
			return false;
		state->unlink.planet = 0.0f;
		if (!yt_record_set_number(&state->unlink.record, YT_F93, 0.0f)
		    || !ops->write_sector(context, state->sector, &state->unlink,
		    error)
		    || !ops->present(context, destroyed_row,
		    sizeof(destroyed_row) - 1U,
		    YT_PROJECTILE_PLASMA_PLANET_DESTROYED_ROW, error)
		    || !ops->sound(context, 3.0f, error)
		    || !ops->news(context, destroyed_row,
		    sizeof(destroyed_row) - 1U, error))
			return false;
	}
	else if (state->original_ground != 0.0f) {
		first_length = qb_str_single(first_number, sizeof(first_number),
		    projectile_single_sub(state->original_ground,
		    state->remaining_ground));
		second_length = qb_str_single(second_number, sizeof(second_number),
		    state->remaining_ground);
		if (first_length < 0 || second_length < 0
		    || sizeof(ground_prefix) - 1U + (size_t)first_length
		    + sizeof(ground_middle) - 1U + (size_t)second_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, ground_prefix, sizeof(ground_prefix) - 1U);
		row_length = sizeof(ground_prefix) - 1U;
		memcpy(row + row_length, first_number, (size_t)first_length);
		row_length += (size_t)first_length;
		memcpy(row + row_length, ground_middle,
		    sizeof(ground_middle) - 1U);
		row_length += sizeof(ground_middle) - 1U;
		memcpy(row + row_length, second_number, (size_t)second_length);
		row_length += (size_t)second_length;
		row[row_length++] = '!';
		if (!ops->present(context, row, row_length,
		    YT_PROJECTILE_PLASMA_PLANET_GROUND_ROW, error)
		    || !ops->news(context, row, row_length, error))
			return false;
	}
	if (*state->energy < 1.0)
		state->route = YT_PROJECTILE_PLASMA_PLANET_FOOTER;
	return true;
}

bool
yt_projectile_plasma_footer_run(
    const struct yt_projectile_plasma_footer_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t dissipated[] = "Plasma bolts dissipated.";

	if (ops == NULL || ops->present == NULL)
		return false;
	return ops->present(context, NULL, 0U,
	    YT_PROJECTILE_PLASMA_FOOTER_LEADING_BLANK, error)
	    && ops->present(context, dissipated, sizeof(dissipated) - 1U,
	    YT_PROJECTILE_PLASMA_FOOTER_TEXT, error)
	    && ops->present(context, NULL, 0U,
	    YT_PROJECTILE_PLASMA_FOOTER_TRAILING_BLANK, error);
}

bool
yt_projectile_defense_front_run(
    struct yt_projectile_defense_front_state *state,
    const struct yt_projectile_defense_front_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t you[] = "YOU";
	static const uint8_t them[] = "THEM";
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[256];
	const uint8_t *initial = xannor;
	size_t owner_length = sizeof(xannor) - 1U;
	size_t row_length;
	bool friendly = false;

	if (state == NULL || ops == NULL || ops->owner == NULL
	    || ops->friendship == NULL || ops->present == NULL
	    || ops->sound == NULL)
		return false;
	state->route = YT_PROJECTILE_DEFENSE_NO_DEFENSE;
	if (!(state->fighters > 0.0))
		return true;
	if (state->owner == -2.0f) {
		initial = mercenaries;
		owner_length = sizeof(mercenaries) - 1U;
	}
	memcpy(owner_name, initial, owner_length);
	if (state->owner > 1.0f) {
		if (!ops->owner(context, state->owner, owner_name, &owner_length,
		    error)
		    || owner_length > sizeof(owner_name)
		    || !ops->friendship(context, state->owner, &friendly, error))
			return false;
	}
	if (state->owner == (float)state->shooter) {
		const uint8_t *replacement = state->shooter == -1 ? them : you;
		size_t replacement_length = state->shooter == -1
		    ? sizeof(them) - 1U : sizeof(you) - 1U;

		memcpy(owner_name, replacement, replacement_length);
		owner_length = replacement_length;
		friendly = true;
	}
	if (!yt_projectile_defense_row(state->sector, owner_name, owner_length,
	    state->fighters, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length, error))
		return false;
	if (friendly) {
		state->route = YT_PROJECTILE_DEFENSE_FRIENDLY;
		return true;
	}
	state->route = YT_PROJECTILE_DEFENSE_HOSTILE;
	return ops->sound(context, 2.0f, error);
}

bool
yt_projectile_defense_combat_run(
    struct yt_projectile_defense_combat_state *state,
    const struct yt_projectile_defense_combat_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t direct_prefix[] = "The Missiles destroyed";
	static const uint8_t direct_suffix[] = " fighters!";
	static const uint8_t news_infix[] = "'s Missiles destroyed";
	static const uint8_t news_sector[] = " fighters in sector";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x10, 0x00};
	uint8_t row[256];
	char destroyed_text[64];
	char sector_text[64];
	int destroyed_length;
	int sector_length;
	size_t row_length;

	if (state == NULL || ops == NULL || state->missiles == NULL
	    || state->xannor_provoker == NULL || ops->random == NULL
	    || ops->present == NULL || ops->news == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->victory == NULL)
		return false;
	state->saved_missiles = 0.0f;
	state->destroyed = 0.0f;
	state->counter = 1.0f;
	state->remaining_fighters = state->fighters;
	state->victory_called = false;
	state->route = YT_PROJECTILE_DEFENSE_CONTINUE_MINES;
	if (!(*state->missiles > 0.0f))
		return true;
	state->saved_missiles = *state->missiles;
	while (state->counter <= state->saved_missiles
	    && (double)state->destroyed < state->fighters) {
		float draw;

		if (!ops->random(context, &draw, error))
			return false;
		state->destroyed = floorf(projectile_single_add(
		    projectile_single_mul(draw, 5000.0f), state->destroyed));
		*state->missiles = projectile_single_sub(*state->missiles, 1.0f);
		if ((double)state->destroyed >= state->fighters) {
			state->destroyed = (float)state->fighters;
			break;
		}
		state->counter = projectile_single_add(state->counter, 1.0f);
	}
	destroyed_length = qb_str_single(destroyed_text,
	    sizeof(destroyed_text), state->destroyed);
	if (destroyed_length < 0
	    || sizeof(direct_prefix) - 1U + (size_t)destroyed_length
	    + sizeof(direct_suffix) - 1U > sizeof(row))
		return false;
	memcpy(row, direct_prefix, sizeof(direct_prefix) - 1U);
	memcpy(row + sizeof(direct_prefix) - 1U, destroyed_text,
	    (size_t)destroyed_length);
	row_length = sizeof(direct_prefix) - 1U + (size_t)destroyed_length;
	memcpy(row + row_length, direct_suffix, sizeof(direct_suffix) - 1U);
	row_length += sizeof(direct_suffix) - 1U;
	if (!ops->present(context, row, row_length, error))
		return false;
	state->remaining_fighters = state->fighters
	    - (double)state->destroyed;
	if (state->destroyed > 9.0f) {
		if (state->shooter_name == NULL && state->shooter_name_length != 0U)
			return false;
		sector_length = qb_str_single(sector_text, sizeof(sector_text),
		    state->sector);
		if (sector_length < 0
		    || state->shooter_name_length + sizeof(news_infix) - 1U
		    + (size_t)destroyed_length + sizeof(news_sector) - 1U
		    + (size_t)sector_length + 1U > sizeof(row))
			return false;
		row_length = 0U;
		if (state->shooter_name_length != 0U) {
			memcpy(row, state->shooter_name,
			    state->shooter_name_length);
			row_length = state->shooter_name_length;
		}
		memcpy(row + row_length, news_infix, sizeof(news_infix) - 1U);
		row_length += sizeof(news_infix) - 1U;
		memcpy(row + row_length, destroyed_text,
		    (size_t)destroyed_length);
		row_length += (size_t)destroyed_length;
		memcpy(row + row_length, news_sector, sizeof(news_sector) - 1U);
		row_length += sizeof(news_sector) - 1U;
		memcpy(row + row_length, sector_text, (size_t)sector_length);
		row_length += (size_t)sector_length;
		row[row_length++] = '!';
		if (!ops->news(context, row, row_length, error))
			return false;
	}
	if (!ops->read_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	state->persistence.fighters = (float)state->remaining_fighters;
	if (!yt_record_set_number(&state->persistence.record, YT_F81,
	    state->persistence.fighters))
		return false;
	if (state->remaining_fighters == 0.0) {
		state->persistence.fighters = 0.0f;
		state->persistence.fighter_owner = 0.0f;
		if (!yt_record_set_raw_number(&state->persistence.record, YT_F81,
		    dirty_zero)
		    || !yt_record_set_raw_number(&state->persistence.record, YT_F85,
		    dirty_zero))
			return false;
	}
	else if (state->owner == -1.0f)
		*state->xannor_provoker = state->shooter;
	if (!ops->write_sector(context, state->sector, &state->persistence,
	    error))
		return false;
	if (state->remaining_fighters == 0.0
	    && state->sector == state->headquarters && state->owner == -1.0f) {
		state->victory_called = true;
		if (!ops->victory(context, error))
			return false;
	}
	if (*state->missiles < 1.0f)
		state->route = YT_PROJECTILE_DEFENSE_RETURN;
	return true;
}

bool
yt_projectile_sector_mine_run(
    struct yt_projectile_sector_mine_state *state,
    const struct yt_projectile_sector_mine_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t hit_prefix[] = "The missiles hit";
	static const uint8_t hit_middle[] = " SECTOR MINES in sector";
	static const uint8_t news_middle[] =
	    "'s Missiles hit sector mines in sector";
	static const uint8_t destroyed_prefix[] = "The missile";
	static const uint8_t destroyed_middle[] = " destroyed";
	static const uint8_t destroyed_mine[] = " mine";
	static const uint8_t plural[] = "s";
	uint8_t row[256];
	char mine_text[64];
	char sector_text[64];
	int mine_length;
	int sector_length;
	size_t row_length;

	if (state == NULL || ops == NULL || state->missiles == NULL
	    || state->last_news_sector == NULL || ops->read_sector == NULL
	    || ops->present == NULL || ops->sound == NULL || ops->news == NULL
	    || ops->write_sector == NULL
	    || (state->shooter_name == NULL
	    && state->shooter_name_length != 0U))
		return false;
	state->observed_mines = 0.0;
	state->destroyed = 0.0f;
	state->route = YT_PROJECTILE_SECTOR_MINE_CONTINUE_PLAYERS;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    state->sector);
	if (sector_length < 0)
		return false;
	for (;;) {
		double remaining_mines;
		const uint8_t *suffix;
		size_t suffix_length;

		if (!ops->read_sector(context, state->sector,
		    &state->persistence, error))
			return false;
		state->observed_mines = (double)state->persistence.mines;
		if (!(state->observed_mines > 0.0))
			return true;
		mine_length = qb_str_double(mine_text, sizeof(mine_text),
		    state->observed_mines);
		if (mine_length < 0
		    || sizeof(hit_prefix) - 1U + (size_t)mine_length
		    + sizeof(hit_middle) - 1U + (size_t)sector_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, hit_prefix, sizeof(hit_prefix) - 1U);
		memcpy(row + sizeof(hit_prefix) - 1U, mine_text,
		    (size_t)mine_length);
		row_length = sizeof(hit_prefix) - 1U + (size_t)mine_length;
		memcpy(row + row_length, hit_middle, sizeof(hit_middle) - 1U);
		row_length += sizeof(hit_middle) - 1U;
		memcpy(row + row_length, sector_text, (size_t)sector_length);
		row_length += (size_t)sector_length;
		row[row_length++] = '!';
		if (!ops->present(context, row, row_length, error)
		    || !ops->sound(context, 5.0f, error))
			return false;
		if (*state->last_news_sector != state->sector) {
			if (state->shooter_name_length + sizeof(news_middle) - 1U
			    + (size_t)sector_length + 1U > sizeof(row))
				return false;
			row_length = 0U;
			if (state->shooter_name_length != 0U) {
				memcpy(row, state->shooter_name,
				    state->shooter_name_length);
				row_length = state->shooter_name_length;
			}
			memcpy(row + row_length, news_middle,
			    sizeof(news_middle) - 1U);
			row_length += sizeof(news_middle) - 1U;
			memcpy(row + row_length, sector_text,
			    (size_t)sector_length);
			row_length += (size_t)sector_length;
			row[row_length++] = '!';
			if (!ops->news(context, row, row_length, error))
				return false;
			*state->last_news_sector = state->sector;
		}
		state->destroyed = (double)*state->missiles
		    < state->observed_mines ? *state->missiles
		    : (float)state->observed_mines;
		suffix = state->destroyed > 1.0f ? plural : NULL;
		suffix_length = suffix == NULL ? 0U : sizeof(plural) - 1U;
		mine_length = qb_str_single(mine_text, sizeof(mine_text),
		    state->destroyed);
		if (mine_length < 0
		    || sizeof(destroyed_prefix) - 1U + suffix_length
		    + sizeof(destroyed_middle) - 1U + (size_t)mine_length
		    + sizeof(destroyed_mine) - 1U + suffix_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, destroyed_prefix, sizeof(destroyed_prefix) - 1U);
		row_length = sizeof(destroyed_prefix) - 1U;
		if (suffix_length != 0U)
			row[row_length++] = *suffix;
		memcpy(row + row_length, destroyed_middle,
		    sizeof(destroyed_middle) - 1U);
		row_length += sizeof(destroyed_middle) - 1U;
		memcpy(row + row_length, mine_text, (size_t)mine_length);
		row_length += (size_t)mine_length;
		memcpy(row + row_length, destroyed_mine,
		    sizeof(destroyed_mine) - 1U);
		row_length += sizeof(destroyed_mine) - 1U;
		if (suffix_length != 0U)
			row[row_length++] = *suffix;
		row[row_length++] = '!';
		if (!ops->present(context, row, row_length, error)
		    || !ops->read_sector(context, state->sector,
		    &state->persistence, error))
			return false;
		remaining_mines = state->observed_mines
		    - (double)state->destroyed;
		state->persistence.mines = (float)remaining_mines;
		if (!yt_record_set_number(&state->persistence.record, YT_F129,
		    state->persistence.mines)
		    || !ops->write_sector(context, state->sector,
		    &state->persistence, error))
			return false;
		*state->missiles = projectile_single_sub(*state->missiles,
		    state->destroyed);
		if (*state->missiles < 1.0f) {
			state->route = YT_PROJECTILE_SECTOR_MINE_RETURN;
			return true;
		}
	}
}

bool
yt_projectile_survivor_overlay(struct yt_player *player, float shields,
    double fighters, float scanner, bool scanner_disabled)
{
	static const uint8_t scanner_zero[4] = {
		0x00, 0x00, 0x48, 0x00
	};

	if (player == NULL)
		return false;
	player->shields = shields;
	player->fighters = (float)fighters;
	player->danger_scanner = scanner_disabled ? 0.0f : scanner;
	return yt_record_set_number(&player->record, YT_F53, shields)
	    && yt_record_set_number(&player->record, YT_F61,
	    player->fighters)
	    && (scanner_disabled
	    ? yt_record_set_raw_number(&player->record, YT_F93, scanner_zero)
	    : yt_record_set_number(&player->record, YT_F93, scanner));
}

bool
yt_projectile_victim_mines_overlay(struct yt_player *player,
    float *saved_mines)
{
	if (player == NULL || saved_mines == NULL)
		return false;
	*saved_mines = player->mines;
	player->mines = 0.0f;
	return yt_record_set_number(&player->record, YT_F129, 0.0f);
}

bool
yt_projectile_sector_mines_overlay(struct yt_sector *sector,
    float carried_mines)
{
	if (sector == NULL)
		return false;
	sector->mines = projectile_single_add(sector->mines, carried_mines);
	return yt_record_set_number(&sector->record, YT_F129, sector->mines);
}

uint32_t
yt_projectile_physical_record(float offset, float logical)
{
	return qb_brun_random_record_number(projectile_single_add(offset,
	    logical));
}

bool
yt_projectile_planet_ground_overlay(struct yt_planet *planet,
    float ground, float owner)
{
	if (planet == NULL)
		return false;
	planet->ground_forces = ground;
	planet->owner = owner;
	return yt_record_set_number(&planet->record, YT_F77, ground)
	    && yt_record_set_number(&planet->record, YT_F73, owner);
}

bool
yt_projectile_planet_productivity_overlay(struct yt_planet *planet,
    const float production[3], const float stock[3])
{
	size_t index;

	if (planet == NULL || production == NULL || stock == NULL)
		return false;
	for (index = 0U; index < 3U; ++index) {
		planet->production[index] = production[index];
		planet->stock[index] = stock[index];
		if (!yt_record_set_number(&planet->record, YT_F45 + index * 4U,
		    production[index])
		    || !yt_record_set_number(&planet->record,
		    YT_F57 + index * 4U, stock[index]))
			return false;
	}
	return true;
}

bool
yt_projectile_planet_destroy_overlay(struct yt_planet *planet)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (planet == NULL)
		return false;
	planet->name_length = 0.0f;
	return yt_record_set_raw_number(&planet->record, YT_F85, link_zero);
}

bool
yt_projectile_sector_unlink_overlay(struct yt_sector *sector)
{
	static const uint8_t link_zero[4] = {
		0x00, 0x00, 0x20, 0x00
	};

	if (sector == NULL)
		return false;
	sector->planet = 0.0f;
	return yt_record_set_raw_number(&sector->record, YT_F93, link_zero);
}

bool
yt_projectile_planet_ground_damage(float ground, float owner,
    float *remaining, yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_ground_result *result, struct yt_error *error)
{
	size_t iterations = 0U;

	if (remaining == NULL || draw == NULL || result == NULL)
		return false;
	result->ground = ground;
	result->owner = owner;
	result->iterations = 0U;
	while (ground > 0.0f && *remaining > 0.0f) {
		float value;

		if (!draw(context, &value, error))
			return false;
		ground = projectile_single_sub(ground,
		    projectile_single_mul(value, 25.0f));
		*remaining = projectile_single_sub(*remaining, 1.0f);
		++iterations;
		result->ground = ground;
		result->iterations = iterations;
	}
	ground = floorf(ground);
	if (ground < 1.0f) {
		ground = 0.0f;
		owner = 0.0f;
	}
	result->ground = ground;
	result->owner = owner;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_productivity_damage(float updater_ore,
    float production[3], float stock[3], float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_productivity_result *result,
    struct yt_error *error)
{
	float old_total;
	float new_total;
	size_t iterations = 0U;
	size_t index;

	if (production == NULL || stock == NULL || remaining == NULL
	    || draw == NULL || result == NULL)
		return false;
	old_total = projectile_single_add(projectile_single_add(production[0],
	    production[1]), production[2]);
	while ((updater_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *remaining > 0.0f) {
		for (index = 0U; index < 3U; ++index) {
			float value;

			if (!draw(context, &value, error))
				return false;
			production[index] = projectile_single_sub(
			    production[index], projectile_single_mul(value,
			    2000.0f));
		}
		*remaining = projectile_single_sub(*remaining, 1.0f);
		++iterations;
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = projectile_single_mul(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	new_total = projectile_single_add(projectile_single_add(production[0],
	    production[1]), production[2]);
	result->old_total = old_total;
	result->new_total = new_total;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_planet_ground_row(float ground, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Ground forces reduced to";
	static const uint8_t suffix[] = "!";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder, ground, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_projectile_planet_productivity_row(float old_total, float new_total,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Productivity reduced by";
	static const uint8_t middle[] = " units to";
	static const uint8_t suffix[] = " units!";
	struct sector_row_builder builder = {row, capacity, 0U};

	if (length != NULL)
		*length = 0U;
	if (!sector_row_append(&builder, prefix, sizeof(prefix) - 1U)
	    || !sector_row_number(&builder,
	    projectile_single_sub(old_total, new_total), false)
	    || !sector_row_append(&builder, middle, sizeof(middle) - 1U)
	    || !sector_row_number(&builder, new_total, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
	return true;
}

bool
yt_projectile_planet_impact_run(
    struct yt_projectile_planet_impact_state *state,
    const struct yt_projectile_planet_impact_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = "The planet was destroyed!!";
	uint8_t row[256];
	size_t row_length;

	if (state == NULL || ops == NULL || state->planet == NULL
	    || state->remaining == NULL || ops->random == NULL
	    || ops->read_planet == NULL || ops->write_planet == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->present == NULL || ops->append_news == NULL
	    || ops->sound == NULL)
		return false;
	state->early_return = false;

	if (state->planet->ground_forces != 0.0f) {
		struct yt_projectile_ground_result impact;
		struct yt_planet persistence;

		if (!yt_projectile_planet_ground_damage(
		    state->planet->ground_forces, state->planet->owner,
		    state->remaining, ops->random, context, &impact, error)) {
			state->planet->ground_forces = impact.ground;
			state->planet->owner = impact.owner;
			return false;
		}
		state->planet->ground_forces = impact.ground;
		state->planet->owner = impact.owner;
		if (!ops->read_planet(context, state->physical_planet,
		    &persistence, error)
		    || !yt_projectile_planet_ground_overlay(&persistence,
		    impact.ground, impact.owner)
		    || !ops->write_planet(context, state->physical_planet,
		    &persistence, error)
		    || !yt_projectile_planet_ground_row(impact.ground, row,
		    sizeof(row), &row_length)
		    || !ops->present(context, row, row_length, error)
		    || !ops->append_news(context, row, row_length, error))
			return false;
		if (*state->remaining < 1.0f) {
			state->early_return = true;
			return true;
		}
	}

	{
		struct yt_projectile_productivity_result impact;
		struct yt_planet persistence;

		if (!yt_projectile_planet_productivity_damage(state->updater_ore,
		    state->planet->production, state->planet->stock,
		    state->remaining, ops->random, context, &impact, error)
		    || !yt_projectile_planet_productivity_row(impact.old_total,
		    impact.new_total, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length, error)
		    || !ops->append_news(context, row, row_length, error)
		    || !ops->read_planet(context, state->physical_planet,
		    &persistence, error)
		    || !yt_projectile_planet_productivity_overlay(&persistence,
		    state->planet->production, state->planet->stock)
		    || !ops->write_planet(context, state->physical_planet,
		    &persistence, error))
			return false;
	}

	if (state->planet->production[0] == 0.0f
	    && state->planet->production[1] == 0.0f
	    && state->planet->production[2] == 0.0f) {
		struct yt_planet destruction;
		struct yt_sector unlink;

		if (!ops->read_planet(context, state->physical_planet,
		    &destruction, error)
		    || !yt_projectile_planet_destroy_overlay(&destruction)
		    || !ops->write_planet(context, state->physical_planet,
		    &destruction, error)
		    || !ops->read_sector(context, state->physical_sector, &unlink,
		    error)
		    || !yt_projectile_sector_unlink_overlay(&unlink)
		    || !ops->write_sector(context, state->physical_sector, &unlink,
		    error)
		    || !ops->present(context, destroyed, sizeof(destroyed) - 1U,
		    error)
		    || !ops->sound(context, 3.0f, error)
		    || !ops->append_news(context, destroyed,
		    sizeof(destroyed) - 1U, error))
			return false;
	}
	if (*state->remaining < 1.0f)
		state->early_return = true;
	return true;
}

enum yt_projectile_post_impact_route
yt_projectile_post_impact_route(float remaining)
{
	return remaining > 0.0f ? YT_PROJECTILE_POST_IMPACT_NEXT_HOP
	    : YT_PROJECTILE_POST_IMPACT_FOOTER;
}

bool
yt_projectile_route_has_next(int16_t next_hop)
{
	return next_hop != 0;
}

bool
yt_projectile_route_avoid_enabled(bool plasma, int counterattack, int shooter)
{
	return !plasma && counterattack == 0 && shooter != -1;
}

bool
yt_projectile_route_failure_row(bool caller_suffix, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t helper[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t suffix[] = "Missles self destructed!";
	const uint8_t *source = caller_suffix ? suffix : helper;
	size_t source_length = caller_suffix
	    ? sizeof(suffix) - 1U : sizeof(helper) - 1U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < source_length || row == NULL)
		return false;
	memcpy(row, source, source_length);
	*length = source_length;
	return true;
}

bool
yt_projectile_footer_row(uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t footer[] = "*** End of Report ***";

	if (length == NULL)
		return false;
	*length = 0U;
	if (capacity < sizeof(footer) - 1U || row == NULL)
		return false;
	memcpy(row, footer, sizeof(footer) - 1U);
	*length = sizeof(footer) - 1U;
	return true;
}

bool
yt_projectile_player_damage(struct yt_player *target, float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_damage_result *result, struct yt_error *error)
{
	double original_fighters;
	double fighter_damage = 0.0;
	float original_shields;
	float shield_damage = 0.0f;
	float saved_missiles;
	float counter = 1.0f;
	bool scanner_disabled = false;
	size_t iterations = 0U;

	if (target == NULL || remaining == NULL || draw == NULL
	    || result == NULL)
		return false;
	original_fighters = (double)target->fighters;
	original_shields = target->shields;
	saved_missiles = *remaining;
	while (yt_projectile_damage_iteration(counter, saved_missiles)) {
		bool overflow;
		float value;
		float scanner_product;
		int32_t scanner;

		++iterations;
		*remaining = projectile_single_sub(*remaining, 1.0f);
		if (!draw(context, &value, error))
			return false;
		scanner_product = projectile_single_mul(value, *remaining);
		scanner = qb_cint(target->danger_scanner, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "cruise missile scanner CINT");
			}
			return false;
		}
		if (scanner_product > 100.0f && scanner != 0) {
			target->danger_scanner = 0.0f;
			scanner_disabled = true;
		}
		if (!draw(context, &value, error))
			return false;
		fighter_damage = floor((double)projectile_single_mul(value,
		    4001.0f) + fighter_damage);
		if (!draw(context, &value, error))
			return false;
		/* The SINGLE draw is promoted for the DOUBLE fighter operand. */
		if ((double)value * original_fighters < fighter_damage) {
			if (!draw(context, &value, error))
				return false;
			shield_damage = projectile_single_add(shield_damage,
			    floorf(projectile_single_mul(value, 1001.0f)));
		}
		if (fighter_damage >= original_fighters
		    && shield_damage >= original_shields)
			break;
		counter = projectile_single_add(counter, 1.0f);
	}
	if (fighter_damage > original_fighters)
		fighter_damage = original_fighters;
	if (shield_damage > original_shields)
		shield_damage = original_shields;
	target->fighters = (float)(original_fighters - fighter_damage);
	target->shields = projectile_single_sub(original_shields,
	    shield_damage);
	result->fighters = fighter_damage;
	result->shields = shield_damage;
	result->scanner_disabled = scanner_disabled;
	result->iterations = iterations;
	return true;
}

bool
yt_projectile_attack_first_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *victim, size_t victim_length, float sector,
    uint8_t *news, size_t news_capacity, size_t *news_length,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length)
{
	static const uint8_t missile_news[] = "'s missiles attacked ";
	static const uint8_t missile_direct[] = "The missiles attacked ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit ";
	static const uint8_t middle[] = " in";
	static const uint8_t suffix[] = " reducing";
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	char sector_text[64];
	int sector_length;
	size_t news_needed;
	size_t direct_needed;
	size_t position;

	if (news_length == NULL || direct_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	*news_length = 0U;
	*direct_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0)
		return false;
	news_needed = attacker_length + news_infix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	direct_needed = direct_prefix_length + victim_length
	    + sizeof(middle) - 1U + (size_t)sector_length
	    + sizeof(suffix) - 1U;
	if (news_needed > news_capacity || direct_needed > direct_capacity
	    || (news_needed != 0U && news == NULL)
	    || (direct_needed != 0U && direct == NULL))
		return false;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (victim_length != 0U)
		memcpy(news + position, victim, victim_length);
	position += victim_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(news + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*news_length = position;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (victim_length != 0U)
		memcpy(direct + position, victim, victim_length);
	position += victim_length;
	memcpy(direct + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	memcpy(direct + position, suffix, sizeof(suffix) - 1U);
	position += sizeof(suffix) - 1U;
	*direct_length = position;
	return true;
}

bool
yt_projectile_destroyed_rows(const uint8_t *victim,
    size_t victim_length, uint8_t *destroyed, size_t destroyed_capacity,
    size_t *destroyed_length, uint8_t *warning, size_t warning_capacity,
    size_t *warning_length)
{
	static const uint8_t destroyed_suffix[] = " was destroyed!";
	static const uint8_t warning_prefix[] = "*** WARNING, ";
	static const uint8_t warning_suffix[] = " had sector mines!";
	size_t destroyed_needed;
	size_t warning_needed;

	if (destroyed_length == NULL || warning_length == NULL
	    || (victim == NULL && victim_length != 0U))
		return false;
	*destroyed_length = 0U;
	*warning_length = 0U;
	destroyed_needed = victim_length + sizeof(destroyed_suffix) - 1U;
	warning_needed = sizeof(warning_prefix) - 1U + victim_length
	    + sizeof(warning_suffix) - 1U;
	if (destroyed_needed > destroyed_capacity
	    || warning_needed > warning_capacity
	    || (destroyed_needed != 0U && destroyed == NULL)
	    || (warning_needed != 0U && warning == NULL))
		return false;
	if (victim_length != 0U)
		memcpy(destroyed, victim, victim_length);
	memcpy(destroyed + victim_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U);
	memcpy(warning, warning_prefix, sizeof(warning_prefix) - 1U);
	if (victim_length != 0U)
		memcpy(warning + sizeof(warning_prefix) - 1U,
		    victim, victim_length);
	memcpy(warning + sizeof(warning_prefix) - 1U + victim_length,
	    warning_suffix, sizeof(warning_suffix) - 1U);
	*destroyed_length = destroyed_needed;
	*warning_length = warning_needed;
	return true;
}

bool
yt_projectile_friendly_planet_row(const uint8_t *planet,
    size_t planet_length, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking friendly planet \"";
	static const uint8_t suffix[] = "\"!";
	size_t needed = sizeof(prefix) - 1U + planet_length
	    + sizeof(suffix) - 1U;

	if (length == NULL || (planet == NULL && planet_length != 0U))
		return false;
	*length = 0U;
	if (needed > capacity || (needed != 0U && row == NULL))
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	if (planet_length != 0U)
		memcpy(row + sizeof(prefix) - 1U, planet, planet_length);
	memcpy(row + sizeof(prefix) - 1U + planet_length,
	    suffix, sizeof(suffix) - 1U);
	*length = needed;
	return true;
}

bool
yt_projectile_planet_attack_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *planet, size_t planet_length, float sector,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t missile_direct[] = "The Missiles attacked planet ";
	static const uint8_t missile_news[] = "'s Missiles attacked planet ";
	static const uint8_t plasma_direct[] = "The plasma bolts hit planet ";
	static const uint8_t plasma_news[] = "'s plasma bolts hit planet ";
	static const uint8_t sector_prefix[] = " in sector";
	const uint8_t *direct_prefix = plasma ? plasma_direct : missile_direct;
	size_t direct_prefix_length = plasma
	    ? sizeof(plasma_direct) - 1U : sizeof(missile_direct) - 1U;
	const uint8_t *news_infix = plasma ? plasma_news : missile_news;
	size_t news_infix_length = plasma
	    ? sizeof(plasma_news) - 1U : sizeof(missile_news) - 1U;
	char sector_text[64];
	int sector_length;
	size_t direct_needed;
	size_t news_needed;
	size_t position;

	if (direct_length == NULL || news_length == NULL
	    || (attacker == NULL && attacker_length != 0U)
	    || (planet == NULL && planet_length != 0U))
		return false;
	*direct_length = 0U;
	*news_length = 0U;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0)
		return false;
	direct_needed = direct_prefix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	news_needed = attacker_length + news_infix_length + planet_length
	    + sizeof(sector_prefix) - 1U + (size_t)sector_length + 1U;
	if (direct_needed > direct_capacity || news_needed > news_capacity
	    || (direct_needed != 0U && direct == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	memcpy(direct + position, direct_prefix, direct_prefix_length);
	position += direct_prefix_length;
	if (planet_length != 0U)
		memcpy(direct + position, planet, planet_length);
	position += planet_length;
	memcpy(direct + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(direct + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	direct[position++] = '!';
	*direct_length = position;
	position = 0U;
	if (attacker_length != 0U)
		memcpy(news + position, attacker, attacker_length);
	position += attacker_length;
	memcpy(news + position, news_infix, news_infix_length);
	position += news_infix_length;
	if (planet_length != 0U)
		memcpy(news + position, planet, planet_length);
	position += planet_length;
	memcpy(news + position, sector_prefix, sizeof(sector_prefix) - 1U);
	position += sizeof(sector_prefix) - 1U;
	memcpy(news + position, sector_text, (size_t)sector_length);
	position += (size_t)sector_length;
	news[position++] = '!';
	*news_length = position;
	return true;
}

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
	if (!yt_player_stored_name(player, prompt, &stored, error))
		return false;
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
yt_direct_attack_radio_text(const uint8_t *name, size_t name_length,
    double defender_loss, uint8_t *text, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " destroyed";
	static const uint8_t suffix[] = " of your fighters!";
	uint8_t raw_double[8];
	char aliased[64];
	int aliased_length;
	size_t total;

	if (length != NULL)
		*length = 0;
	if ((name == NULL && name_length != 0U) || text == NULL
	    || length == NULL)
		return false;
	if (qb_mbf64_encode(defender_loss, raw_double) != QB_MBF_OK)
		return false;
	aliased_length = qb_str_mbf32(aliased, sizeof(aliased), raw_double);
	if (aliased_length < 0)
		return false;
	total = name_length + sizeof(middle) - 1U + (size_t)aliased_length
	    + sizeof(suffix) - 1U;
	if (total > capacity)
		return false;
	if (name_length != 0U)
		memcpy(text, name, name_length);
	memcpy(text + name_length, middle, sizeof(middle) - 1U);
	memcpy(text + name_length + sizeof(middle) - 1U, aliased,
	    (size_t)aliased_length);
	memcpy(text + name_length + sizeof(middle) - 1U
	    + (size_t)aliased_length, suffix, sizeof(suffix) - 1U);
	*length = total;
	return true;
}

static bool
direct_attack_append(uint8_t *output, size_t capacity, size_t *position,
    const void *data, size_t length)
{
	if (output == NULL || position == NULL || (data == NULL && length != 0U)
	    || *position > capacity || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(output + *position, data, length);
	*position += length;
	return true;
}

bool
yt_direct_attack_team_row(const uint8_t *name, size_t name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking team member ";
	static const uint8_t suffix[] = "!";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_candidate_prompt(const uint8_t *name,
    size_t name_length, uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Attack ";
	static const uint8_t suffix[] = " (Y/N)[Y]? ";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_commitment_prompt(double fighters, uint8_t *prompt,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = ". Use how many fighters? [0] ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_too_many_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You only have";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_result_rows(double attacker_loss,
    double cached_reserve, double defender_loss, double defenders,
    uint8_t *attacker_row, size_t attacker_capacity,
    size_t *attacker_length, uint8_t *defender_row,
    size_t defender_capacity, size_t *defender_length)
{
	static const uint8_t attacker_prefix[] = "You lost";
	static const uint8_t attacker_middle[] = " fighter(s),";
	static const uint8_t remain[] = " remain.";
	static const uint8_t defender_prefix[] = "You destroyed";
	static const uint8_t defender_middle[] = " enemy fighters,";
	char attacker_loss_text[64];
	char reserve_text[64];
	char defender_loss_text[64];
	char defenders_text[64];
	int attacker_loss_length;
	int reserve_length;
	int defender_loss_length;
	int defenders_length;
	size_t attacker_position = 0U;
	size_t defender_position = 0U;

	if (attacker_length == NULL || defender_length == NULL)
		return false;
	*attacker_length = 0U;
	*defender_length = 0U;
	attacker_loss_length = qb_str_double(attacker_loss_text,
	    sizeof(attacker_loss_text), attacker_loss);
	reserve_length = qb_str_double(reserve_text, sizeof(reserve_text),
	    cached_reserve);
	defender_loss_length = qb_str_double(defender_loss_text,
	    sizeof(defender_loss_text), defender_loss);
	defenders_length = qb_str_double(defenders_text,
	    sizeof(defenders_text), defenders);
	if (attacker_loss_length < 0 || reserve_length < 0
	    || defender_loss_length < 0 || defenders_length < 0
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_prefix, sizeof(attacker_prefix) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_loss_text,
	    (size_t)attacker_loss_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_middle, sizeof(attacker_middle) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, reserve_text, (size_t)reserve_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, remain, sizeof(remain) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_prefix, sizeof(defender_prefix) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_loss_text,
	    (size_t)defender_loss_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_middle, sizeof(defender_middle) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defenders_text, (size_t)defenders_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, remain, sizeof(remain) - 1U))
		return false;
	*attacker_length = attacker_position;
	*defender_length = defender_position;
	return true;
}

void
yt_direct_attack_fighter_overlay(struct yt_player *player, float fighters)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_direct_attack_shield_overlay(struct yt_player *player, float shields)
{
	if (player == NULL)
		return;
	player->shields = shields;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
}

void
yt_deployed_attack_player_overlay(struct yt_player *player,
    float shields, float fighters)
{
	if (player == NULL)
		return;
	player->shields = shields;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_deployed_attack_sector_overlay(struct yt_sector *sector, float fighters)
{
	if (sector == NULL)
		return;
	sector->fighters = fighters;
	(void)yt_record_set_number(&sector->record, YT_F81, fighters);
	if (fighters < 1.0f) {
		sector->fighter_owner = 0.0f;
		(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	}
}

void
yt_death_player_overlay(struct yt_player *player, float killer)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x7a, 0x00};

	if (player == NULL)
		return;
	player->killed_by = killer;
	player->sector = 0.0f;
	player->ports_owned = 0.0f;
	(void)yt_record_set_number(&player->record, YT_F45, killer);
	(void)yt_record_set_raw_number(&player->record, YT_F57, dirty_zero);
	(void)yt_record_set_raw_number(&player->record, YT_F117, dirty_zero);
}

bool
yt_death_sector_overlay(struct yt_sector *sector, float victim)
{
	if (sector == NULL || sector->fighter_owner != victim)
		return false;
	sector->fighter_owner = -2.0f;
	(void)yt_record_set_number(&sector->record, YT_F85, -2.0f);
	return true;
}

void
yt_death_team_roster_overlay(struct yt_record *record, float victim)
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	if (record == NULL)
		return;
	for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); ++index)
		if (yt_record_get_number(record, offsets[index]) == victim)
			(void)yt_record_set_number(record, offsets[index], 0.0f);
}

enum yt_death_port_route
yt_death_port_overlay(struct yt_port *port, float victim, float killer,
    float last_player)
{
	bool valid;

	if (port == NULL || port->owner != victim)
		return YT_DEATH_PORT_UNMATCHED;
	valid = (killer != victim) & (killer > 1.0f)
	    & (killer <= last_player);
	if (valid) {
		port->owner = killer;
		port->last_minute = killer;
		(void)yt_record_set_number(&port->record, YT_F97, killer);
		(void)yt_record_set_number(&port->record, YT_F101, killer);
		return YT_DEATH_PORT_TRANSFERRED;
	}
	port->owner = 0.0f;
	port->treasury = 0.0f;
	(void)yt_record_set_number(&port->record, YT_F97, 0.0f);
	(void)yt_record_set_number(&port->record, YT_F89, 0.0f);
	return YT_DEATH_PORT_CLEARED;
}

void
yt_death_killer_credit_overlay(struct yt_player *player, float ports)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = player->ports_owned + ports;
	player->ports_owned = updated;
	(void)yt_record_set_number(&player->record, YT_F117, updated);
}

bool
yt_death_title_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The titles to";
	static const uint8_t middle[] = " ports of ";
	static const uint8_t suffix[] = "'s are now yours!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_death_kill_news_row(const uint8_t *killer, size_t killer_length,
    const uint8_t *victim, size_t victim_length, bool self,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t self_suffix[] = " was killed!";
	static const uint8_t other_infix[] = " killed ";
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (killer == NULL && killer_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, killer,
	    killer_length)
	    || !direct_attack_append(row, capacity, &position,
	    self ? self_suffix : other_infix,
	    self ? sizeof(self_suffix) - 1U : sizeof(other_infix) - 1U)
	    || (!self && !direct_attack_append(row, capacity, &position,
	    victim, victim_length)))
		return false;
	*length = position;
	return true;
}

bool
yt_death_port_news_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  Took";
	static const uint8_t middle[] = " ports from ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length))
		return false;
	*length = position;
	return true;
}

void
yt_bribe_sector_overlay(struct yt_sector *sector)
{
	if (sector == NULL)
		return;
	sector->fighter_owner = 0.0f;
	(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	sector->fighters = 0.0f;
	(void)yt_record_set_number(&sector->record, YT_F81, 0.0f);
}

void
yt_bribe_player_overlay(struct yt_player *player, float fighters,
    float credits)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	player->credits = credits;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
	(void)yt_record_set_number(&player->record, YT_F81, credits);
}

bool
yt_player_killer_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, bool *emit, struct yt_error *error)
{
	static const uint8_t suffix[] = " destroyed your ship!";
	size_t prefix;

	if (length != NULL)
		*length = 0;
	if (emit != NULL)
		*emit = false;
	if (player == NULL || row == NULL || length == NULL || emit == NULL)
		return false;
	if (player->name_length == 0.0f)
		return true;
	if (!yt_player_stored_name(player, row, &prefix, error))
		return false;
	if (prefix + sizeof(suffix) - 1U > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "killer row capacity");
		}
		return false;
	}
	memcpy(row + prefix, suffix, sizeof(suffix) - 1U);
	*length = prefix + sizeof(suffix) - 1U;
	*emit = true;
	return true;
}

bool
yt_player_name_matches(const struct yt_player *player, const uint8_t *name,
    size_t length, bool *matches, struct yt_error *error)
{
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	size_t stored;

	if (matches != NULL)
		*matches = false;
	if (!yt_player_stored_name(player, stored_name, &stored, error))
		return false;
	if (matches != NULL)
		*matches = length == stored
		    && (stored == 0 || memcmp(stored_name, name,
		    stored) == 0);
	return true;
}

void
yt_player_construct(struct yt_player *player, const struct yt_config *config,
    float today)
{
	player->last_active = today;
	player->killed_by = 0;
	player->turns = config->turns_per_day;
	player->shields = 100;
	player->sector = 1;
	player->fighters = config->initial_fighters;
	player->holds = config->initial_holds;
	player->ore = 0;
	player->organics = 0;
	player->equipment = 0;
	player->credits = config->initial_credits;
	player->team = 0;
	player->danger_scanner = 0;
	player->missiles = 1;
	yt_record_set_number(&player->record, YT_F101, 0);
	player->lottery_plays = 0;
	player->plasma = 0;
	player->ports_owned = 0;
	player->ground_forces = 0;
	player->cloak = 1;
	player->mines = 0;
}
