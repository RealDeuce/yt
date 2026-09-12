#include "yt_game.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_pager.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
	if (!ops->play_file(context, "XANNORHQ.TXT", error)
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
yt_xannor_victory_mks_internal_fatal_run(uint16_t module_segment,
    bool redirected_stdin, bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	return yt_brun_internal_fatal_run(YT_BRUN_INTERNAL_FATAL_GC,
	    "YT-SUB  ", true, 64006, module_segment, 0xA995U,
	    redirected_stdin, function_bar, cursor_shape_known,
	    process_entry_cursor_shape, ops, context, state);
}

bool
yt_main_startup_internal_fatal_run(uint16_t module_segment,
    bool redirected_stdin, bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	return yt_brun_internal_fatal_run(YT_BRUN_INTERNAL_FATAL_OWNER,
	    "YT      ", true, 3, module_segment, 0x0136U, redirected_stdin,
	    function_bar, cursor_shape_known, process_entry_cursor_shape,
	    ops, context, state);
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
	    || state->player_cache == NULL || ops->close_data == NULL
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
	path_count = qb_cint_mbf32(config->record.bytes + YT_F41, 0U,
	    &overflow);
	if (overflow || path_count < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "startup scoreboard LEFT$");
	state->scoreboard_path_length = (size_t)path_count;
	if (state->scoreboard_path_length > YT_TEXT_FIELD_SIZE)
		state->scoreboard_path_length = YT_TEXT_FIELD_SIZE;
	memcpy(config->scoreboard, config->record.bytes,
	    state->scoreboard_path_length);
	if (ops->store_local_screen != NULL)
		ops->store_local_screen(context, config->record.bytes + YT_F85);
	qb_compat_upper_n((uint8_t *)config->scoreboard,
	    state->scoreboard_path_length);
	config->scoreboard[state->scoreboard_path_length] = '\0';

	if (config->headquarters == 0.0f) {
		static const uint8_t headquarters_default[4] = {
			0x00, 0x40, 0x37, 0x8a
		};

		if (!yt_record_set_raw_number(&config->record, YT_F117,
		    headquarters_default)
		    || !ops->store_config(context, config, error))
			return false;
		config->headquarters = 733.0f;
	}
	if (config->genesis_ports < 20.0f) {
		config->genesis_ports = 200.0f;
	}
	if (state->scoreboard_path_length == 0U) {
		static const char default_path[] = "YTSCORE.ASC";

		memcpy(config->scoreboard, default_path, sizeof(default_path));
		state->scoreboard_path_length = sizeof(default_path) - 1U;
	}
	local_mode = qb_cint(state->local_mode, &overflow);
	if (overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "startup local-mode CINT");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode != 0) {
		static const uint8_t local_default[4] = {
			0x00, 0x00, 0x80, 0x81
		};

		config->local_screen = -1.0f;
		if (ops->store_local_screen != NULL)
			ops->store_local_screen(context, local_default);
	}
	if (config->lottery_plays < 0.0f || config->lottery_plays > 9.0f) {
		config->lottery_plays = 3.0f;
	}
	if (config->maximum_planets == 0.0f) {
		config->maximum_planets = 100.0f;
	}
	if (config->maximum_holds < 5.0f || config->maximum_holds > 1000.0f) {
		config->maximum_holds = 1000.0f;
	}
	if (config->turns_per_day < 100.0f
	    || config->turns_per_day > 2500.0f) {
		config->turns_per_day = 500.0f;
	}

	counter = 2.0f;
	while (counter <= config->sector_offset) {
		struct yt_player player;
		int32_t basic = qb_cint(counter, &overflow);

		if (overflow || !yt_player_cache_contains(basic))
			return startup_configuration_error(error, YT_RANGE,
			    "startup player-cache index");
		if (!ops->read_player(context, basic, &player, error))
			return false;
		(void)yt_player_cache_set_raw(state->player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR, player.record.bytes + YT_F57);
		(void)yt_player_cache_set_raw(state->player_cache, basic,
		    YT_PLAYER_CACHE_CLOAK, player.record.bytes + YT_F125);
		if (player.cloak < 0.0f || player.cloak > 1.0f) {
			static const uint8_t one[4] = {
				0x00U, 0x00U, 0x00U, 0x81U
			};

			player.cloak = 1.0f;
			(void)yt_player_cache_set_raw(state->player_cache, basic,
			    YT_PLAYER_CACHE_CLOAK, one);
			if (!yt_record_set_number(&player.record, YT_F125, 1.0f)
			    || !ops->write_player(context, basic, &player, error))
				return false;
		}
		counter = startup_single_add(counter, 1.0f);
	}
	for (index = 0U; index < 2U; ++index) {
		float draw;
		float difference;
		float span;
		float product;
		float integral;
		uint8_t raw[4];

		if (!ops->random(context, &draw, error))
			return false;
		difference = startup_single_subtract(config->port_offset,
		    config->sector_offset);
		span = startup_single_subtract(difference, 2.0f);
		product = startup_single_multiply(draw, span);
		integral = floorf(product);
		state->black_hole[index] = startup_single_add(integral, 2.0f);
		if (integral == -2.0f) {
			static const uint8_t dirty_zero[4] = {
				0x00U, 0x00U, 0x80U, 0x00U
			};

			memcpy(raw, dirty_zero, sizeof(raw));
		} else if (qb_mbf32_encode(state->black_hole[index], raw)
		    != QB_MBF_OK) {
			return startup_configuration_error(error, YT_RANGE,
			    "startup disruption result");
		}
		if (ops->store_disruption != NULL)
			ops->store_disruption(context, index, raw);
	}
	return true;
}

static float projectile_single_add(float left, float right);
static float projectile_single_sub(float left, float right);
static float projectile_single_mul(float left, float right);

static void
team_loader_cache_store_raw(float *value, uint8_t destination[4],
    const uint8_t source[4])
{
	memcpy(destination, source, 4U);
	*value = qb_mbf32_decode(source);
}

static void
team_loader_cache_store_value(float *destination,
    uint8_t destination_raw[4], float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) != QB_MBF_OK)
		return;
	team_loader_cache_store_raw(destination, destination_raw, raw);
}

void
yt_team_loader_cache_sync_raw(struct yt_team_loader_cache *cache)
{
	size_t index;

	if (cache == NULL)
		return;
	if (!cache->raw_valid
	    || qb_mbf32_decode(cache->available_raw) != cache->available)
		team_loader_cache_store_value(&cache->available,
		    cache->available_raw, cache->available);
	for (index = 0U; index < YT_ARRAY_LEN(cache->roster); ++index) {
		if (!cache->raw_valid
		    || qb_mbf32_decode(cache->roster_raw[index])
		    != cache->roster[index])
			team_loader_cache_store_value(&cache->roster[index],
			    cache->roster_raw[index], cache->roster[index]);
	}
	if (!cache->raw_valid
	    || qb_mbf32_decode(cache->captain_raw) != cache->captain)
		team_loader_cache_store_value(&cache->captain,
		    cache->captain_raw, cache->captain);
	if (!cache->raw_valid
	    || qb_mbf32_decode(cache->captain_flag_raw)
	    != cache->captain_flag)
		team_loader_cache_store_value(&cache->captain_flag,
		    cache->captain_flag_raw, cache->captain_flag);
	if (!cache->raw_valid
	    || qb_mbf32_decode(cache->counter_raw) != cache->counter)
		team_loader_cache_store_value(&cache->counter,
		    cache->counter_raw, cache->counter);
	cache->raw_valid = true;
}

void
yt_team_loader_begin(float team_id, struct yt_team_loader_cache *cache,
    bool *needs_overlay)
{
	static const uint8_t zero[4] = {0x00U, 0x00U, 0x00U, 0x00U};
	static const uint8_t true_raw[4] = {0x00U, 0x00U, 0x80U, 0x81U};
	size_t index;

	if (cache == NULL)
		return;
	yt_team_loader_cache_sync_raw(cache);
	team_loader_cache_store_raw(&cache->available, cache->available_raw,
	    true_raw);
	for (index = 0U; index < YT_ARRAY_LEN(cache->roster); ++index) {
		team_loader_cache_store_value(&cache->counter,
		    cache->counter_raw, (float)(index + 1U));
		team_loader_cache_store_raw(&cache->roster[index],
		    cache->roster_raw[index], zero);
	}
	team_loader_cache_store_value(&cache->counter, cache->counter_raw, 5.0f);
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
	static const uint8_t live_zero[4] = {0x00U, 0x00U, 0x48U, 0x00U};
	static const uint8_t true_raw[4] = {0x00U, 0x00U, 0x80U, 0x81U};
	float roster[4];
	bool overflow;
	int32_t converted_length;
	size_t name_length;
	size_t index;
	bool live = false;

	if (overlay == NULL || cache == NULL || route == NULL)
		return false;
	yt_team_loader_cache_sync_raw(cache);
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

	team_loader_cache_store_raw(&cache->available, cache->available_raw,
	    live_zero);
	converted_length = qb_cint_mbf32(overlay->bytes + YT_F73,
	    conversion_mode, &overflow);
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
	team_loader_cache_store_raw(&cache->captain, cache->captain_raw,
	    overlay->bytes + YT_F77);
	if (cache->captain == current_player) {
		team_loader_cache_store_raw(&cache->captain_flag,
		    cache->captain_flag_raw, true_raw);
	}
	for (index = 0U; index < YT_ARRAY_LEN(cache->roster); ++index) {
		team_loader_cache_store_raw(&cache->roster[index],
		    cache->roster_raw[index],
		    overlay->bytes + roster_offsets[index]);
	}
	*route = YT_TEAM_LOADER_LIVE;
	return true;
}

bool
yt_team_loader_run(struct yt_team_loader_state *state,
    yt_team_loader_read_record_fn read_record, void *context,
    struct yt_error *error)
{
	bool needs_overlay;
	float expression;

	if (state == NULL || state->cache == NULL || read_record == NULL)
		return false;
	state->route = YT_TEAM_LOADER_OUT_OF_RANGE;
	state->overlay_loaded = false;
	state->complete = false;
	yt_team_loader_begin(state->team_id, state->cache, &needs_overlay);
	if (!needs_overlay) {
		state->complete = true;
		return true;
	}
	expression = startup_single_add(state->sector_record_offset,
	    state->team_id);
	state->physical_record = qb_brun_random_record_number(expression);
	if (!read_record(context, state->physical_record, &state->overlay,
	    error))
		return false;
	state->overlay_loaded = true;
	if (!yt_team_loader_finish(&state->overlay,
	    state->current_player_record, state->conversion_mode,
	    state->cache, &state->route, error))
		return false;
	state->complete = true;
	return true;
}

static bool
team_audit_error(struct yt_error *error, const char *operation)
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
team_audit_append(uint8_t *destination, size_t capacity, size_t *length,
    const uint8_t *source, size_t source_length, struct yt_error *error)
{
	if (source_length > capacity - *length)
		return team_audit_error(error, "team audit message length");
	if (source_length != 0U)
		memcpy(destination + *length, source, source_length);
	*length += source_length;
	return true;
}

bool
yt_team_audit_run(struct yt_team_audit_state *state,
    const struct yt_team_audit_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t separator[] = " *** ";
	static const uint8_t quit_text[] = " QUIT your team on ";
	static const uint8_t join_text[] = " joined your team on ";
	static const uint8_t invalid_text[] =
	    " entered invalid password for your team: ";
	static const uint8_t at_text[] = " at ";
	static const uint8_t suffix[] = "!";
	static const uint8_t sender_raw[4] = {0x00U, 0x00U, 0x80U, 0x82U};
	uint8_t replacement[YT_TEAM_AUDIT_MESSAGE_MAX];
	uint8_t clock_text[32];
	size_t replacement_length = 0U;
	size_t clock_length;
	size_t index;
	bool construct;
	bool overflow;

	if (state == NULL || ops == NULL || ops->load_team == NULL
	    || ops->write_radio == NULL || state->cache == NULL
	    || state->message == NULL
	    || state->message_length > state->message_capacity
	    || state->message_capacity > YT_TEAM_AUDIT_MESSAGE_MAX
	    || (state->current_player_name == NULL
	    && state->current_player_name_length != 0U)
	    || (state->attempted_password == NULL
	    && state->attempted_password_length != 0U))
		return team_audit_error(error, "team audit state");
	state->complete = false;
	construct = state->event_type == 0.0f || state->event_type == 1.0f
	    || state->event_type == 2.0f;
	if (construct) {
		if (!team_audit_append(replacement, sizeof(replacement),
		    &replacement_length, state->current_player_name,
		    state->current_player_name_length, error)
		    || !team_audit_append(replacement, sizeof(replacement),
		    &replacement_length, separator, sizeof(separator) - 1U,
		    error))
			return false;
		if (state->event_type == 2.0f) {
			if (!team_audit_append(replacement, sizeof(replacement),
			    &replacement_length, quit_text,
			    sizeof(quit_text) - 1U, error))
				return false;
		} else if (state->event_type == 1.0f) {
			if (!team_audit_append(replacement, sizeof(replacement),
			    &replacement_length, join_text,
			    sizeof(join_text) - 1U, error))
				return false;
		} else if (!team_audit_append(replacement,
		    sizeof(replacement), &replacement_length, invalid_text,
		    sizeof(invalid_text) - 1U, error))
			return false;
		if (state->event_type == 1.0f || state->event_type == 2.0f) {
			if (ops->clock == NULL)
				return team_audit_error(error,
				    "team audit clock adapter");
			clock_length = 0U;
			if (!ops->clock(context, YT_TEAM_AUDIT_DATE, clock_text,
			    sizeof(clock_text), &clock_length, error)
			    || clock_length > sizeof(clock_text)
			    || !team_audit_append(replacement,
			    sizeof(replacement), &replacement_length, clock_text,
			    clock_length, error)
			    || !team_audit_append(replacement,
			    sizeof(replacement), &replacement_length, at_text,
			    sizeof(at_text) - 1U, error))
				return false;
			clock_length = 0U;
			if (!ops->clock(context, YT_TEAM_AUDIT_TIME, clock_text,
			    sizeof(clock_text), &clock_length, error)
			    || clock_length > sizeof(clock_text)
			    || !team_audit_append(replacement,
			    sizeof(replacement), &replacement_length, clock_text,
			    clock_length, error))
				return false;
		} else if (!team_audit_append(replacement,
		    sizeof(replacement), &replacement_length,
		    state->attempted_password,
		    state->attempted_password_length, error))
			return false;
		if (!team_audit_append(replacement, sizeof(replacement),
		    &replacement_length, suffix, sizeof(suffix) - 1U, error)
		    || replacement_length > state->message_capacity)
			return team_audit_error(error,
			    "team audit message capacity");
		memcpy(state->message, replacement, replacement_length);
		state->message_length = replacement_length;
	}
	if (!ops->load_team(context, state->team_id, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(state->cache->roster_raw);
	    ++index) {
		float recipient = qb_mbf32_decode(state->cache->roster_raw[index]);
		int32_t converted = qb_cint_mbf32(
		    state->cache->roster_raw[index], state->conversion_mode,
		    &overflow);
		int32_t unequal = recipient != state->current_player_record
		    ? -1 : 0;

		if (overflow)
			return team_audit_error(error,
			    "team audit recipient CINT");
		if ((converted & unequal) != 0) {
			if (!ops->write_radio(context, state->message,
			    state->message_length, sender_raw,
			    state->cache->roster_raw[index], error))
				return false;
		}
	}
	state->complete = true;
	return true;
}

bool
yt_death_team_remove_run(struct yt_death_team_remove_state *state,
    const struct yt_death_team_remove_ops *ops, void *context,
    struct yt_error *error)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct yt_player victim;
	struct yt_record overlay;
	struct yt_team_loader_state loader;
	float expression;
	size_t index;

	if (state == NULL || state->cache == NULL || ops == NULL
	    || ops->read_player == NULL || ops->write_player == NULL
	    || ops->read_record == NULL || ops->write_record == NULL)
		return false;
	state->complete = false;
	state->loader_route = YT_TEAM_LOADER_OUT_OF_RANGE;
	if (!ops->read_player(context, state->victim_record, &victim, error))
		return false;
	state->raw_team_id = victim.team;
	if (state->raw_team_id == 0.0f) {
		state->complete = true;
		return true;
	}

	loader = (struct yt_team_loader_state){
		.team_id = state->raw_team_id,
		.current_player_record = state->current_player_record,
		.sector_record_offset = state->sector_record_offset,
		.conversion_mode = state->conversion_mode,
		.cache = state->cache,
	};
	if (!yt_team_loader_run(&loader, ops->read_record, context, error))
		return false;
	state->loader_route = loader.route;
	if (loader.overlay_loaded)
		state->overlay_physical_record = loader.physical_record;
	for (index = 0U; index < YT_ARRAY_LEN(state->cache->roster);
	    ++index) {
		if (state->cache->roster[index]
		    == (float)state->victim_record) {
			static const uint8_t zero[4] = {0};

			team_loader_cache_store_raw(&state->cache->roster[index],
			    state->cache->roster_raw[index], zero);
		}
	}

	expression = startup_single_add(state->sector_record_offset,
	    state->raw_team_id);
	state->overlay_physical_record =
	    qb_brun_random_record_number(expression);
	if (!ops->read_record(context, state->overlay_physical_record,
	    &overlay, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(roster_offsets); ++index)
		(void)yt_record_set_raw_number(&overlay, roster_offsets[index],
		    state->cache->roster_raw[index]);
	if (!ops->write_record(context, state->overlay_physical_record,
	    &overlay, error)
	    || !ops->read_player(context, state->victim_record, &victim,
	    error))
		return false;
	victim.team = 0.0f;
	(void)yt_record_set_number(&victim.record, YT_F89, 0.0f);
	if (!ops->write_player(context, state->victim_record, &victim, error))
		return false;
	state->complete = true;
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
	    || ops->store_team_id == NULL
	    || ops->store_captain == NULL
	    || ops->promote_cache == NULL
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
	ops->store_team_id(context,
	    &state->current_player.record.bytes[YT_F89]);
	state->team_id = qb_mbf32_decode(
	    &state->current_player.record.bytes[YT_F89]);
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
	ops->store_captain(context, &state->team.overlay.record.bytes[YT_F77]);
	state->captain_record = qb_mbf32_decode(
	    &state->team.overlay.record.bytes[YT_F77]);
	if (state->captain_record >= 2.0f
	    && state->captain_record <= state->sector_offset) {
		struct yt_player captain;

		if (!ops->read_player(context, state->captain_record, &captain,
		    error))
			return false;
		if (captain.name_length > 0.0f) {
			name_length = qb_cint_mbf32(captain.record.bytes + YT_F85,
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

		ops->store_captain(context, state->current_record_raw);
		ops->promote_cache(context, state->current_record_raw);
		state->captain_record = qb_mbf32_decode(state->current_record_raw);
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
	if (state->found)
		return true;
	state->found = true;
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

static void
spy_cache_cloak_zero(struct yt_spy_sweep_state *state, int player_record)
{
	static const uint8_t zero[4] = {0};

	(void)yt_player_cache_set_raw(state->player_cache, player_record,
	    YT_PLAYER_CACHE_CLOAK, zero);
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
	int spy_index;

	if (state == NULL || ops == NULL || state->spy_sectors == NULL
	    || state->last_reported_sectors == NULL
	    || state->player_cache == NULL
	    || ops->read_sector == NULL || ops->update_planet == NULL
	    || ops->read_planet == NULL || ops->read_player == NULL
	    || ops->read_team == NULL || ops->random == NULL
	    || ops->sound == NULL || ops->present == NULL
	    || ops->pause == NULL)
		return false;
	if (state->active_spies == 0)
		return true;
	for (spy_index = 0; spy_index < state->active_spies; ++spy_index) {
		struct yt_sector sector;
		bool overflow;
		size_t spy = (size_t)spy_index;
		int sector_number;
		bool first_ship = true;
		int candidate;

		sector_number = state->spy_sectors[spy];
		state->foreground = 7.0f;
		if (!(sector_number == state->last_reported_sectors[spy]
		    && sector_number != 0)) {
			state->found = false;
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

				if (!yt_player_cache_contains(candidate))
					return startup_configuration_error(error, YT_RANGE,
					    "last player cache aliases adjacent memory");
				if (!yt_sector_candidate_eligible(candidate,
				    state->current_player_record,
				    yt_player_cache_value(state->player_cache, candidate,
				    YT_PLAYER_CACHE_SECTOR),
				    (float)sector_number))
					continue;
				if (!ops->random(context, &draw, error))
					return false;
				cloak = yt_player_cache_value(state->player_cache, candidate,
				    YT_PLAYER_CACHE_CLOAK);
				detected = yt_sector_cloak_revealed(draw, cloak);
				if (detected) {
					if (!spy_first_finding(state, ops, context,
					    spy, sector_number, error)
					    || !spy_present(state, ops, context,
					    cloak_notice, sizeof(cloak_notice) - 1U,
					    YT_SPY_BOLD_LINE, error))
						return false;
					spy_cache_cloak_zero(state, candidate);
					if (!ops->sound(context, 4.0f, error))
						return false;
				}
				if (yt_player_cache_value(state->player_cache, candidate,
				    YT_PLAYER_CACHE_CLOAK) != 0.0f
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
		if (state->found) {
			if (!spy_present(state, ops, context, NULL, 0U,
			    YT_SPY_LINE, error)
			    || !ops->pause(context, state, error))
				return false;
		}
		if (!ops->read_sector(context, sector_number, &sector, error))
			return false;
		{
			int32_t warps[6];
			size_t slot;

			for (slot = 0U; slot < YT_ARRAY_LEN(warps); ++slot) {
				warps[slot] = qb_cint_mbf32(sector.record.bytes + YT_F105
				    + 4U * slot, 0U, &overflow);
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
				if (warps[selected] != 0) {
					state->spy_sectors[spy] = warps[selected];
					break;
				}
			}
		}
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
	uint8_t saved_cloak_raw[4];
	int saved_record;
	int target_candidate;
	float target;
	float projectile_amount;
	int amount;
	int ignored_counterattack = 0;
	char amount_text[64];
	char target_text[64];
	char row[192];
	bool valid_cache;
	bool cache_cleared = false;

	if (state == NULL || ops == NULL || state->player == NULL
	    || state->player_record == NULL || state->destroyed == NULL
	    || state->provoker == NULL || state->headquarters == NULL
	    || state->player_cache == NULL
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
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		yt_player_cache_raw(state->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
		if (*state->provoker != 0) {
			static const uint8_t cloak_zero[4] = {
				0x00U, 0x00U, 0x40U, 0x00U
			};

			(void)yt_player_cache_set_raw(state->player_cache,
			    saved_record, YT_PLAYER_CACHE_CLOAK, cloak_zero);
			cache_cleared = true;
		}
	}
	(void)snprintf(state->player->name, sizeof(state->player->name), "%s",
	    "The Xannor");
	*state->player_record = -1;

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
	projectile_amount = (float)amount;
	if (!ops->present(context, (const uint8_t *)row, strlen(row), true,
	    error)
	    || !ops->projectile(context, state->headquarters, &target,
	    &projectile_amount, false, &ignored_counterattack, state->provoker,
	    error))
		return false;

	*state->player_record = saved_record;
	*state->player = saved_player;
	if (valid_cache) {
		if (cache_cleared)
			(void)yt_player_cache_set_raw(state->player_cache,
			    saved_record, YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
	}
	if (!ops->read_player(context, saved_record, state->player, error))
		return false;
	if (qb_mbf32_truth(state->player->record.bytes + YT_F45)) {
		*state->destroyed = true;
	}
	if (!ops->wait(context, 4.0f, error))
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

static bool
projectile_command_error(struct yt_error *error, enum yt_status status,
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
yt_projectile_command_run(struct yt_projectile_command_state *state,
    const struct yt_projectile_command_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t no_turns[] =
	    "Sorry but you have no turns left.";
	static const uint8_t no_ammunition[] = "You dont have any!";
	static const uint8_t invalid_sector[] = "Invalid Sector number!";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	static const uint8_t too_many[] = "You dont have that many!";
	uint8_t prompt[192];
	char response[4096];
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	size_t prompt_length;
	double integral;

	if (state == NULL || ops == NULL || state->destroyed == NULL
	    || state->current_player_record < 1 || state->maximum_sector < 1.0f
	    || ops->hydrate == NULL || ops->present == NULL
	    || ops->input == NULL || ops->finalize == NULL
	    || ops->write_player == NULL || ops->flush == NULL
	    || ops->resolve == NULL || ops->counterlaunch == NULL
	    || ops->xannor == NULL || ops->fatal == NULL)
		return projectile_command_error(error, YT_INVALID,
		    "projectile-command state");
	memset(&state->first_hydration, 0, sizeof(state->first_hydration));
	memset(&state->live_hydration, 0, sizeof(state->live_hydration));
	memset(&state->post_finalizer, 0, sizeof(state->post_finalizer));
	state->turn_gate_result_stores = 0U;
	state->attempts = 0U;
	state->hydrations = 0U;
	state->available = 0.0f;
	state->target = 0.0f;
	memset(state->target_raw, 0, sizeof(state->target_raw));
	state->target_stored = false;
	state->amount = 0.0f;
	memset(state->amount_raw, 0, sizeof(state->amount_raw));
	state->amount_stored = false;
	state->origin = 0.0f;
	memset(state->origin_raw, 0, sizeof(state->origin_raw));
	state->counterattack = 0;
	state->xannor_provoker = 0;
	state->finalizer_called = false;
	state->player_written = false;
	state->player_flushed = false;
	state->destruction_cleared = false;
	state->resolver_called = false;
	state->counterlaunch_called = false;
	state->xannor_called = false;
	state->fatal_called = false;
	state->route = YT_PROJECTILE_COMMAND_INCOMPLETE;
	state->complete = false;

	for (;;) {
		state->attempts++;
		if (!ops->present(context, NULL, 0U,
		    YT_PROJECTILE_COMMAND_OPENING_BLANK, error)
		    || !ops->hydrate(context, state->current_player_record,
		    &state->first_hydration, error))
			return false;
		state->hydrations++;
		if (!ops->hydrate(context, state->current_player_record,
		    &state->live_hydration, error))
			return false;
		state->hydrations++;
		yt_no_turn_gate_result_raw(false, state->turn_gate_result_raw);
		state->turn_gate_result_stores++;
		if (ops->store_turn_gate_result != NULL)
			ops->store_turn_gate_result(context,
			    state->turn_gate_result_raw);
		if (state->live_hydration.turns <= 0.0f) {
			yt_no_turn_gate_result_raw(true, state->turn_gate_result_raw);
			state->turn_gate_result_stores++;
			if (ops->store_turn_gate_result != NULL)
				ops->store_turn_gate_result(context,
				    state->turn_gate_result_raw);
			state->route = YT_PROJECTILE_COMMAND_NO_TURNS;
			if (!ops->present(context, no_turns,
			    sizeof(no_turns) - 1U,
			    YT_PROJECTILE_COMMAND_NO_TURNS_ROW, error))
				return false;
			state->complete = true;
			return true;
		}
		state->available = state->plasma
		    ? state->live_hydration.plasma
		    : state->live_hydration.missiles;
		if (state->available < 1.0f) {
			state->route = YT_PROJECTILE_COMMAND_NO_AMMUNITION;
			if (!ops->present(context, no_ammunition,
			    sizeof(no_ammunition) - 1U,
			    YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW, error))
				return false;
			state->complete = true;
			return true;
		}
		if (!yt_projectile_target_prompt(state->plasma, state->displayed,
		    state->maximum_sector, prompt, sizeof(prompt), &prompt_length)
		    || !ops->present(context, prompt, prompt_length,
		    YT_PROJECTILE_COMMAND_TARGET_PROMPT, error)
		    || !ops->input(context, response, sizeof(response), error))
			return false;
		if (response[0] == '\0') {
			state->route = YT_PROJECTILE_COMMAND_TARGET_CANCELLED;
			state->complete = true;
			return true;
		}
		parsed = qb_val(response);
		if (parsed.overflow)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target VAL");
		state->target = (float)(parsed.valid ? parsed.value : 0.0);
		conversion = qb_mbf32_encode(state->target, state->target_raw);
		if (conversion == QB_MBF_OVERFLOW)
			return projectile_command_error(error, YT_RANGE,
			    "projectile target CSNG");
		state->target = qb_mbf32_decode(state->target_raw);
		state->target_stored = true;
		if (state->target >= 1.0f
		    && state->target <= state->maximum_sector)
			break;
		if (!ops->present(context, invalid_sector,
		    sizeof(invalid_sector) - 1U,
		    YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW, error))
			return false;
	}

	if (!ops->present(context, quantity_prompt,
	    sizeof(quantity_prompt) - 1U,
	    YT_PROJECTILE_COMMAND_QUANTITY_PROMPT, error)
	    || !ops->input(context, response, sizeof(response), error))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity VAL");
	integral = floor(parsed.valid ? parsed.value : 0.0);
	state->amount = (float)integral;
	conversion = qb_mbf32_encode(state->amount, state->amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return projectile_command_error(error, YT_RANGE,
		    "projectile quantity CSNG");
	state->amount = qb_mbf32_decode(state->amount_raw);
	state->amount_stored = true;
	if (state->amount < 1.0f) {
		state->route = YT_PROJECTILE_COMMAND_QUANTITY_CANCELLED;
		state->complete = true;
		return true;
	}
	if (state->amount > state->available) {
		state->route = YT_PROJECTILE_COMMAND_TOO_MANY;
		if (!ops->present(context, too_many, sizeof(too_many) - 1U,
		    YT_PROJECTILE_COMMAND_TOO_MANY_ROW, error))
			return false;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U,
	    YT_PROJECTILE_COMMAND_ACCEPTED_BLANK, error))
		return false;
	state->finalizer_called = true;
	if (!ops->finalize(context, &state->post_finalizer, error)) {
		if (error == NULL || error->status == YT_OK) {
			state->route = YT_PROJECTILE_COMMAND_FINALIZER_TERMINAL;
			state->complete = true;
			return true;
		}
		return false;
	}
	state->origin = state->post_finalizer.sector;
	memcpy(state->origin_raw,
	    state->post_finalizer.record.bytes + YT_F57,
	    sizeof(state->origin_raw));
	yt_projectile_debit_overlay(&state->post_finalizer, state->plasma,
	    state->amount);
	if (!ops->write_player(context, state->current_player_record,
	    &state->post_finalizer, error))
		return false;
	state->player_written = true;
	if (!ops->flush(context, error))
		return false;
	state->player_flushed = true;
	*state->destroyed = false;
	state->destruction_cleared = true;
	state->resolver_called = true;
	if (!ops->resolve(context, &state->origin, state->origin_raw,
	    &state->target, state->target_raw, &state->amount,
	    state->amount_raw, state->plasma, &state->counterattack,
	    &state->xannor_provoker, error))
		return false;
	if (ops->counterattack_truth != NULL
	    ? ops->counterattack_truth(context) : state->counterattack != 0) {
		state->counterlaunch_called = true;
		if (!ops->counterlaunch(context, &state->counterattack,
		    &state->xannor_provoker, error))
			return false;
	}
	if (ops->xannor_truth != NULL
	    ? ops->xannor_truth(context) : state->xannor_provoker != 0) {
		state->xannor_called = true;
		if (!ops->xannor(context, &state->xannor_provoker, error))
			return false;
	}
	if (ops->destroyed_truth != NULL
	    ? ops->destroyed_truth(context) : *state->destroyed) {
		state->route = YT_PROJECTILE_COMMAND_FATAL;
		state->fatal_called = true;
		if (!ops->fatal(context, error))
			return false;
	}
	else {
		state->route = YT_PROJECTILE_COMMAND_RETURNED;
	}
	state->complete = true;
	return true;
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
	bool process_route;

	if (state == NULL || ops == NULL || state->origin == NULL
	    || state->destination == NULL || state->energy == NULL
	    || state->step_limit == 0U || ops->build_route == NULL
	    || ops->line == NULL || ops->attention == NULL
	    || ops->wait == NULL || ops->random == NULL
	    || ops->impact == NULL || ops->footer == NULL)
		return false;
	process_route = ops->read_route != NULL || ops->write_route != NULL;
	if ((process_route && (ops->read_route == NULL
	    || ops->write_route == NULL)) || (!process_route
	    && (state->route == NULL || state->route_capacity == 0U)))
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
			if (overflow || (!process_route && (destination_index < 0
			    || (size_t)destination_index >= state->route_capacity)))
				return false;
			*state->origin = 0.0f;
			if (ops->arguments_changed != NULL)
				ops->arguments_changed(context, *state->origin,
				    *state->destination,
				    YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO);
			if (process_route) {
				ops->write_route(context, 0,
				    (int16_t)destination_index);
				ops->write_route(context,
				    (int16_t)destination_index, 0);
			}
			else {
				state->route[0] = (int16_t)destination_index;
				state->route[destination_index] = 0;
			}
		}
		else {
			state->route_status = 0.0f;
			++state->route_calls;
			if (!ops->build_route(context, state->origin,
			    state->destination, state->route,
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
			if (overflow || (!process_route && (current_index < 0
			    || (size_t)current_index >= state->route_capacity)))
				return false;
			next_hop = process_route
			    ? ops->read_route(context, (int16_t)current_index)
			    : state->route[current_index];
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
				if (ops->arguments_changed != NULL)
					ops->arguments_changed(context, *state->origin,
					    *state->destination,
					    YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN);
				if (!ops->random(context, &draw, error))
					return false;
				span = projectile_single_sub(state->port_record_offset,
				    state->sector_record_offset);
				*state->destination = floorf(projectile_single_add(
				    projectile_single_mul(draw, span), 1.0f));
				if (ops->arguments_changed != NULL)
					ops->arguments_changed(context, *state->origin,
					    *state->destination,
					    YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION);
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
	return resolver(resolver_context, origin, &target, &amount, plasma,
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
	uint8_t saved_cloak_raw[4];

	if (state == NULL || ops == NULL || state->player == NULL
	    || state->player_record == NULL || state->destroyed == NULL
	    || state->retained_count == NULL || state->counterattacker == NULL
	    || state->xannor_provoker == NULL || state->player_cache == NULL
	    || ops->read_player == NULL
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
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		static const uint8_t zero[4] = {0};

		yt_player_cache_raw(state->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
		(void)yt_player_cache_set_raw(state->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, zero);
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
	if (!ops->projectile(context, &origin, &target,
	    state->retained_count, false, state->counterattacker,
	    state->xannor_provoker, error))
		return false;

	*state->counterattacker = 0;
	*state->player_record = saved_record;
	*state->player = saved_player;
	if (valid_cache)
		(void)yt_player_cache_set_raw(state->player_cache, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
	if (!ops->read_player(context, saved_record, &final_player, error))
		return false;
	if (qb_mbf32_truth(final_player.record.bytes + YT_F45)) {
		*state->destroyed = true;
	}
	return ops->wait(context, 4.0f, error);
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

static bool
nearest_front_present(struct yt_nearest_front_state *state,
    const struct yt_nearest_front_ops *ops, void *context,
    enum yt_nearest_front_output_kind kind, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	if (!ops->present(context, kind, text, length, error))
		return false;
	++state->outputs;
	return true;
}

static bool
nearest_front_input(struct yt_nearest_front_state *state,
    const struct yt_nearest_front_ops *ops, void *context, uint8_t *text,
    size_t capacity, size_t *length, struct yt_error *error)
{
	*length = 0U;
	if (!ops->input(context, text, capacity, length, error))
		return false;
	if (*length > capacity)
		return startup_configuration_error(error, YT_INVALID,
		    "nearest filter input length");
	++state->inputs;
	return true;
}

static int
nearest_front_selector(const uint8_t *response, size_t length)
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
yt_nearest_front_run(struct yt_nearest_front_state *state,
    const struct yt_nearest_front_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t first_line[] =
	    "Show buying/selling [1] Equ, [2] Org, [3] Ore,";
	static const uint8_t second_line[] =
	    "[Y] Your Ports, [T] Team's Ports, [E] Enemy Ports";
	static const uint8_t filter_prompt[] =
	    "[U] Un-owned Ports OR [A] All Ports ? -=> [A] ";
	static const uint8_t no_team[] = "You dont belong to a team!";
	static const uint8_t no_ports[] = "You dont own any!";
	static const char *const commodities[3] = {
		"Equipment", "Organics", "Ore"
	};
	uint8_t prompt[80];
	int written;

	if (state == NULL || ops == NULL || ops->hydrate == NULL
	    || ops->present == NULL || ops->input == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "nearest filter arguments");
	state->current_team = 0.0f;
	state->ports_owned = 0.0f;
	state->selector = 0;
	state->direction = 0U;
	state->filter_length = 0U;
	state->direction_length = 0U;
	state->hydrations = 0U;
	state->outputs = 0U;
	state->inputs = 0U;
	state->result = YT_NEAREST_FRONT_INCOMPLETE;
	memset(state->filter_response, 0, sizeof(state->filter_response));
	memset(state->direction_response, 0,
	    sizeof(state->direction_response));

	if (!ops->hydrate(context, &state->current_team,
	    &state->ports_owned, error))
		return false;
	++state->hydrations;
	if (!nearest_front_present(state, ops, context,
	    YT_NEAREST_FRONT_FILTER_FIRST, first_line,
	    sizeof(first_line) - 1U, error)
	    || !nearest_front_present(state, ops, context,
	    YT_NEAREST_FRONT_FILTER_SECOND, second_line,
	    sizeof(second_line) - 1U, error)
	    || !nearest_front_present(state, ops, context,
	    YT_NEAREST_FRONT_FILTER_PROMPT, filter_prompt,
	    sizeof(filter_prompt) - 1U, error)
	    || !nearest_front_input(state, ops, context,
	    state->filter_response, sizeof(state->filter_response),
	    &state->filter_length, error))
		return false;
	state->selector = nearest_front_selector(state->filter_response,
	    state->filter_length);
	if (state->selector == 0) {
		state->result = YT_NEAREST_FRONT_REPROMPT;
		return true;
	}
	if (state->selector == 5 && state->current_team == 0.0f) {
		if (!nearest_front_present(state, ops, context,
		    YT_NEAREST_FRONT_NO_TEAM, no_team,
		    sizeof(no_team) - 1U, error))
			return false;
		state->result = YT_NEAREST_FRONT_REPROMPT;
		return true;
	}
	if (state->selector == 6 && state->ports_owned == 0.0f) {
		if (!nearest_front_present(state, ops, context,
		    YT_NEAREST_FRONT_NO_PORTS, no_ports,
		    sizeof(no_ports) - 1U, error))
			return false;
		state->result = YT_NEAREST_FRONT_REPROMPT;
		return true;
	}
	if (state->selector >= 1 && state->selector <= 3) {
		if (!nearest_front_present(state, ops, context,
		    YT_NEAREST_FRONT_DIRECTION_BLANK, NULL, 0U, error))
			return false;
		written = snprintf((char *)prompt, sizeof(prompt),
		    "Find ports [B] Buying or [S] Selling %s -=> ",
		    commodities[state->selector - 1]);
		if (written < 0 || (size_t)written >= sizeof(prompt))
			return startup_configuration_error(error, YT_RANGE,
			    "nearest direction prompt");
		if (!nearest_front_present(state, ops, context,
		    YT_NEAREST_FRONT_DIRECTION_PROMPT, prompt,
		    (size_t)written, error)
		    || !nearest_front_input(state, ops, context,
		    state->direction_response,
		    sizeof(state->direction_response),
		    &state->direction_length, error))
			return false;
		if (state->direction_length != 1U
		    || (state->direction_response[0] != 'B'
		    && state->direction_response[0] != 'S')) {
			state->result = YT_NEAREST_FRONT_REPROMPT;
			return true;
		}
		state->direction = state->direction_response[0];
	}
	state->result = YT_NEAREST_FRONT_HANDOFF;
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
    float current_day, float timer_seconds, struct yt_error *error)
{
	float day_delta;
	float minute_delta;
	size_t index;

	if (market == NULL || port == NULL || base_price == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "nearest market arguments");
	nearest_market_copy(market, port);
	if (!nearest_div(timer_seconds, 60.0f, &market->minute, error,
	    "nearest market minute")
	    || !nearest_sub(current_day, market->stored_day, &day_delta, error,
	    "nearest market day delta")
	    || !nearest_sub(market->minute, market->stored_minute, &minute_delta,
	    error, "nearest market minute delta")
	    || !nearest_div(minute_delta, 1440.0f, &minute_delta, error,
	    "nearest market minute fraction")
	    || !nearest_add(day_delta, minute_delta, &market->elapsed, error,
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
		    &growth, error, "nearest market growth")
		    || !nearest_add(port->stock[index], growth,
		    &market->stock[index], error, "nearest market stock")
		    || !nearest_div(market->stock[index], 10.0f, &candidate,
		    error, "nearest market production comparison"))
			return false;
		if (candidate > market->production[index]
		    && !nearest_div(market->stock[index], 10.0f,
		    &market->production[index], error,
		    "nearest market production replacement"))
			return false;
		if (!nearest_mul(market->factor[index], market->stock[index],
		    &numerator, error, "nearest market numerator")
		    || !nearest_mul(market->production[index], 1000.0f,
		    &denominator, error, "nearest market denominator")
		    || !nearest_div(numerator, denominator, &ratio, error,
		    "nearest market ratio")
		    || !nearest_sub(1.0f, ratio, &scale, error,
		    "nearest market scale")
		    || !nearest_mul(base_price[index], scale, &raw, error,
		    "nearest market raw price")
		    || !nearest_add(raw, 0.5f, &rounded, error,
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

static bool
nearest_cint(const struct yt_nearest_state *state, float value, int *result,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    state->conversion_mode, &overflow);

	if (overflow)
		return nearest_error(error, operation);
	*result = (int)converted;
	return true;
}

static bool
nearest_present(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    enum yt_nearest_output_kind kind, enum yt_nearest_present_mode mode,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	if (mode == YT_NEAREST_PRESENT_BOLD_LINE
	    || mode == YT_NEAREST_PRESENT_BOLD_RAW)
		state->style.bold = 1.0f;
	if (!ops->present(context, kind, mode, text, length, &state->style,
	    error))
		return false;
	++state->outputs;
	return true;
}

static bool
nearest_read(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    enum yt_nearest_field_kind kind, float expression,
    struct yt_record *record, struct yt_error *error)
{
	uint32_t physical;

	if (!nearest_single(expression, &state->current_record_expression,
	    error, "nearest record expression"))
		return false;
	physical = qb_brun_random_record_number(
	    state->current_record_expression);
	if (!ops->read_record(context, kind, state->current_record_expression,
	    physical, record, error))
		return false;
	state->field = *record;
	state->field_kind = kind;
	state->field_record = physical;
	state->field_valid = true;
	++state->reads;
	return true;
}

static int
nearest_descending_compare(const void *left, const void *right)
{
	int a = *(const int *)left;
	int b = *(const int *)right;

	return a < b ? 1 : a > b ? -1 : 0;
}

static bool
nearest_filter(const struct yt_nearest_state *state, bool member)
{
	float klass = state->port.commodity_class;
	float owner = state->port.owner;
	bool accepted = false;

	if (state->selector >= 1 && state->selector <= 3) {
		accepted = state->direction == 'S'
		    ? klass == (float)state->selector
		    : state->direction == 'B'
		    && klass != (float)state->selector;
	}
	else if (state->selector == 4)
		accepted = true;
	else if (state->selector == 5)
		accepted = owner > 0.0f && owner != state->actor_number
		    && member;
	else if (state->selector == 6)
		accepted = owner == state->actor_number;
	else if (state->selector == 7)
		accepted = owner > 0.0f && owner != state->actor_number
		    && (state->current_team == 0.0f
		    || (state->current_team > 0.0f && !member));
	else if (state->selector == 8)
		accepted = owner == 0.0f;
	return accepted && klass != 0.0f;
}

static bool
nearest_price_cell(float price, char result[4])
{
	char number[64];
	char source[65];
	size_t length;
	size_t amount;
	size_t padding;
	int rendered = qb_str_single(number, sizeof(number), price);

	if (rendered < 0)
		return false;
	source[0] = ' ';
	memcpy(source + 1, number, (size_t)rendered);
	length = (size_t)rendered + 1U;
	amount = length < 3U ? length : 3U;
	padding = 3U - amount;
	memset(result, ' ', padding);
	memcpy(result + padding, source + length - amount, amount);
	result[3] = '\0';
	return true;
}

static bool
nearest_stock_cell(const struct yt_nearest_market *market, uint8_t result[7],
    struct yt_error *error)
{
	char number[64];
	char source[96];
	float total;
	float scaled;
	float rounded;
	int length;
	size_t source_length;

	if (!nearest_add(market->stock[0], market->stock[1], &total, error,
	    "nearest stock total one")
	    || !nearest_add(total, market->stock[2], &total, error,
	    "nearest stock total two")
	    || !nearest_div(total, 10000.0f, &scaled, error,
	    "nearest stock scale")
	    || !nearest_add(scaled, 0.5f, &rounded, error,
	    "nearest stock rounding"))
		return false;
	rounded = floorf(rounded);
	if (!nearest_single(rounded, &rounded, error, "nearest stock INT"))
		return false;
	length = qb_str_double(number, sizeof(number), (double)rounded);
	if (length < 0 || snprintf(source, sizeof(source), "      %sK  ",
	    number) < 0)
		return nearest_error(error, "nearest stock formatting");
	source_length = strlen(source);
	if (source_length < 7U)
		return nearest_error(error, "nearest stock width");
	memcpy(result, source + source_length - 7U, 7U);
	return true;
}

static bool
nearest_page_count(struct yt_nearest_state *state, float amount,
    struct yt_error *error)
{
	if (!nearest_add(state->page_count, amount, &state->page_count, error,
	    "nearest pager count"))
		return false;
	if (qb_mbf32_encode(state->page_count, state->page_count_raw)
	    == QB_MBF_OVERFLOW)
		return nearest_error(error, "nearest pager count raw");
	return true;
}

static bool
nearest_page(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context, bool *stop,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "More? [Y]es [N]o [+] Continuous [Y] ";
	uint8_t key;
	bool available;

	*stop = false;
	state->page_count = 0.0f;
	memcpy(state->page_count_raw, "\x00\x00\x30\x00", 4U);
	state->style.foreground = 3.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_PAGER_PROMPT,
	    YT_NEAREST_PRESENT_BOLD_RAW, prompt, sizeof(prompt) - 1U, error))
		return false;
	for (;;) {
		available = false;
		key = 0U;
		if (!ops->input(context, &key, &available, error))
			return false;
		if (!available)
			continue;
		if (key == '\r')
			key = 'Y';
		ops->uppercase(context, &key, 1U);
		if (key != 'Y' && key != 'N' && key != '+')
			continue;
		if (!nearest_present(state, ops, context, YT_NEAREST_PAGER_ECHO,
		    YT_NEAREST_PRESENT_LINE, &key, 1U, error))
			return false;
		if (key == '+')
			state->continuous = true;
		*stop = key == 'N';
		return true;
	}
}

bool
yt_nearest_run(struct yt_nearest_state *state,
    const struct yt_nearest_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t scanning[] = "Scanning Starmap Database...";
	static const uint8_t instruction[] =
	    "Owned ports show the name of the owner preceeded by a \">\".";
	static const uint8_t earth[] = "** Earth **";
	bool visited[3001] = {false};
	int current_layer[3001];
	int next_layer[3001];
	size_t current_count = 0U;
	struct yt_record raw;
	bool first_sector = true;

	if (state == NULL || ops == NULL || ops->read_record == NULL
	    || ops->observe_day == NULL || ops->observe_timer == NULL
	    || ops->present == NULL || ops->input == NULL
	    || ops->uppercase == NULL
	    || state->selector < 1 || state->selector > 8
	    || (state->selector <= 3 && state->direction != 'B'
	    && state->direction != 'S'))
		return startup_configuration_error(error, YT_INVALID,
		    "nearest arguments");
	state->current_record_expression = 0.0f;
	state->current_team = 0.0f;
	state->start_sector_raw = 0.0f;
	state->display_sector = 0.0f;
	state->timer_seconds = 0.0f;
	state->page_count = 4.0f;
	if (qb_mbf32_encode(4.0f, state->page_count_raw) == QB_MBF_OVERFLOW)
		return nearest_error(error, "nearest initial pager count");
	state->current_sector = 0;
	state->distance = 0;
	state->rows = 0U;
	state->outputs = 0U;
	state->reads = 0U;
	state->day_observations = 0U;
	state->timer_observations = 0U;
	state->roster_comparisons = 0U;
	state->continuous = false;
	state->stopped = false;
	state->complete = false;
	state->result = YT_NEAREST_INCOMPLETE;

	if (!nearest_single(state->actor_number, &state->actor_number, error,
	    "nearest actor number")
	    || !nearest_present(state, ops, context, YT_NEAREST_ENTRY_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->style.foreground = 3.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_SCANNING,
	    YT_NEAREST_PRESENT_BOLD_LINE, scanning, sizeof(scanning) - 1U,
	    error)
	    || !nearest_present(state, ops, context, YT_NEAREST_SCAN_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->style.foreground = 7.0f;
	if (!nearest_present(state, ops, context, YT_NEAREST_OWNER_INSTRUCTION,
	    YT_NEAREST_PRESENT_BOLD_LINE, instruction,
	    sizeof(instruction) - 1U, error)
	    || !nearest_present(state, ops, context, YT_NEAREST_OWNER_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;

	if (!nearest_read(state, ops, context, YT_NEAREST_FIELD_PLAYER,
	    state->actor_number, &raw, error))
		return false;
	yt_player_decode(&state->player, &raw);
	state->current_team = state->player.team;
	state->start_sector_raw = state->player.sector;
	if (!nearest_cint(state, state->start_sector_raw,
	    &state->current_sector, error, "nearest start-sector CINT"))
		return false;
	if (state->current_sector < 0 || state->current_sector > 3000)
		return nearest_error(error, "nearest start workspace");
	if (state->start_sector_raw != 0.0f) {
		visited[state->current_sector] = true;
		current_layer[current_count++] = state->current_sector;
	}

	while (current_count != 0U) {
		size_t layer_index;
		size_t next_count = 0U;
		bool heading_emitted = false;

		for (layer_index = 0U; layer_index < current_count; ++layer_index) {
			int sector_number = current_layer[layer_index];
			float sector_operand = first_sector
			    ? state->start_sector_raw : (float)sector_number;
			float expression;
			float raw_port;
			size_t slot;

			first_sector = false;
			state->current_sector = sector_number;
			state->display_sector = sector_operand;
			if (!nearest_add(state->sector_record_offset, sector_operand,
			    &expression, error, "nearest sector record expression")
			    || !nearest_read(state, ops, context,
			    YT_NEAREST_FIELD_SECTOR, expression, &raw, error))
				return false;
			yt_sector_decode(&state->sector, &raw);
			for (slot = 0U; slot < 6U; ++slot) {
				int target;
				float warp = state->sector.warps[slot];

				if (warp == 0.0f)
					continue;
				if (!nearest_cint(state, warp, &target, error,
				    "nearest warp CINT"))
					return false;
				if (target < 0 || target > 3000)
					return nearest_error(error,
					    "nearest warp workspace");
				if (visited[target])
					continue;
				visited[target] = true;
				next_layer[next_count++] = target;
			}
			raw_port = state->sector.port;
			if (raw_port == 0.0f)
				continue;
			if (!ops->observe_day(context, &state->current_day, error))
				return false;
			++state->day_observations;
			if (!nearest_single(state->current_day, &state->current_day,
			    error, "nearest current day")
			    || !nearest_add(state->port_record_offset, raw_port,
			    &expression, error, "nearest port record expression")
			    || !nearest_read(state, ops, context,
			    YT_NEAREST_FIELD_PORT, expression, &raw, error))
				return false;
			yt_port_decode(&state->port, &raw);
			{
				bool member = false;

				if (state->current_team != 0.0f) {
					for (slot = 0U; slot < 4U; ++slot) {
						++state->roster_comparisons;
						if (state->cached_roster[slot]
						    == state->port.owner)
							member = true;
					}
				}
				if (!nearest_filter(state, member))
					continue;
			}
			nearest_market_copy(&state->market, &state->port);
			if (!ops->observe_timer(context, &state->timer_seconds, error))
				return false;
			++state->timer_observations;
			if (!nearest_single(state->timer_seconds,
			    &state->timer_seconds, error, "nearest TIMER")
			    || !yt_nearest_market_project(&state->market, &state->port,
			    state->base_price, state->current_day,
			    state->timer_seconds, error))
				return false;

			if (!heading_emitted) {
				char number[64];
				uint8_t heading[96];
				int length = qb_str_single(number, sizeof(number),
				    (float)state->distance);

				if (length < 0 || 9U + (size_t)length > sizeof(heading))
					return nearest_error(error,
					    "nearest distance formatting");
				memcpy(heading, "Distance:", 9U);
				memcpy(heading + 9U, number, (size_t)length);
				state->style.foreground = 1.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_DISTANCE, YT_NEAREST_PRESENT_BOLD_LINE,
				    heading, 9U + (size_t)length, error)
				    || !nearest_page_count(state, 1.0f, error))
					return false;
				heading_emitted = true;
			}
			{
				uint8_t name[43];
				uint8_t sector_cell[13];
				uint8_t ore[13];
				uint8_t organics[13];
				uint8_t equipment[11];
				uint8_t stock[7];
				char number[64];
				char price[4];
				size_t name_length;
				int rendered;
				int owner_record;
				bool stop;

				rendered = qb_str_single(number, sizeof(number),
				    state->display_sector);
				if (rendered < 0)
					return nearest_error(error,
					    "nearest sector formatting");
				memcpy(sector_cell, "Sector:", 7U);
				memset(sector_cell + 7U, ' ', 6U);
				memcpy(sector_cell + 7U, number,
				    (size_t)rendered < 6U ? (size_t)rendered : 6U);
				if (!nearest_price_cell(state->market.price[0], price))
					return nearest_error(error,
					    "nearest ore formatting");
				(void)snprintf((char *)ore, sizeof(ore),
				    " Ore @%c%s  ", state->port.commodity_class
				    == 3.0f ? 'S' : 'B', price);
				if (!nearest_price_cell(state->market.price[1], price))
					return nearest_error(error,
					    "nearest organics formatting");
				(void)snprintf((char *)organics,
				    sizeof(organics), " Org @%c%s  ",
				    state->port.commodity_class == 2.0f ? 'S' : 'B',
				    price);
				if (!nearest_price_cell(state->market.price[2], price))
					return nearest_error(error,
					    "nearest equipment formatting");
				(void)snprintf((char *)equipment,
				    sizeof(equipment), " Equ @%c%s",
				    state->port.commodity_class == 1.0f ? 'S' : 'B',
				    price);
				if (!nearest_stock_cell(&state->market, stock, error)
				    || !nearest_cint(state, state->port.name_length,
				    &rendered, error, "nearest name-length CINT"))
					return false;
				if (rendered < 0)
					return nearest_error(error,
					    "nearest name LEFT$ length");
				name_length = (size_t)rendered;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				memcpy(name, state->port.record.bytes, name_length);
				if (state->display_sector == 1.0f) {
					name_length = sizeof(earth) - 1U;
					memcpy(name, earth, name_length);
					memset(ore, 0, sizeof(ore));
					memset(organics, 0, sizeof(organics));
					memset(equipment, 0, sizeof(equipment));
					memset(stock, 0, sizeof(stock));
				}
				if (state->port.owner != 0.0f)
					state->style.bold = 1.0f;
				state->style.foreground = 2.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_SECTOR, YT_NEAREST_PRESENT_RAW,
				    sector_cell, sizeof(sector_cell), error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 3.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context, YT_NEAREST_ORE,
				    YT_NEAREST_PRESENT_BOLD_RAW, ore,
				    state->display_sector == 1.0f
				    ? 0U : sizeof(ore) - 1U,
				    error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 2.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_ORGANICS, YT_NEAREST_PRESENT_BOLD_RAW,
				    organics, state->display_sector == 1.0f
				    ? 0U : sizeof(organics) - 1U, error))
					return false;
				state->style.foreground =
				    state->port.commodity_class == 1.0f ? 7.0f : 6.0f;
				if (!nearest_present(state, ops, context,
				    YT_NEAREST_EQUIPMENT, YT_NEAREST_PRESENT_BOLD_RAW,
				    equipment, state->display_sector == 1.0f
				    ? 0U : sizeof(equipment) - 1U, error))
					return false;
				state->style.foreground = 2.0f;
				if (!nearest_present(state, ops, context, YT_NEAREST_STOCK,
				    YT_NEAREST_PRESENT_RAW, stock,
				    state->display_sector == 1.0f ? 0U : sizeof(stock),
				    error))
					return false;
				state->style.foreground = 3.0f;
				if (!nearest_cint(state, state->port.owner, &owner_record,
				    error, "nearest owner CINT"))
					return false;
				if (state->display_sector != 1.0f
				    && owner_record != 0) {
					if (!nearest_read(state, ops, context,
					    YT_NEAREST_FIELD_OWNER, state->port.owner,
					    &raw, error))
						return false;
					yt_player_decode(&state->owner, &raw);
					memmove(name + 2U, state->owner.record.bytes,
					    YT_TEXT_FIELD_SIZE);
					memcpy(name, "> ", 2U);
					name_length = qb_title_case_n(name,
					    YT_TEXT_FIELD_SIZE + 2U);
					if (name_length > 26U)
						name_length = 26U;
					if (state->port.owner == state->actor_number)
						state->style.foreground = 5.0f;
				}
				if (state->display_sector == 1.0f) {
					state->style.foreground = 3.0f;
					state->style.blink = 1.0f;
				}
				if (!nearest_present(state, ops, context, YT_NEAREST_NAME,
				    YT_NEAREST_PRESENT_BOLD_LINE, name, name_length,
				    error))
					return false;
				++state->rows;
				if (!nearest_page_count(state, 1.0f, error))
					return false;
				if (state->continuous) {
					state->page_count = 0.0f;
					memset(state->page_count_raw, 0, 4U);
				}
				if (state->page_count > 22.0f) {
					if (!nearest_page(state, ops, context, &stop,
					    error))
						return false;
					if (stop) {
						if (!nearest_present(state, ops, context,
						    YT_NEAREST_FINAL_BLANK,
						    YT_NEAREST_PRESENT_LINE, NULL, 0U,
						    error))
							return false;
						state->stopped = true;
						state->complete = true;
						state->result = YT_NEAREST_PAGE_STOP;
						return true;
					}
				}
			}
		}
		qsort(next_layer, next_count, sizeof(next_layer[0]),
		    nearest_descending_compare);
		memcpy(current_layer, next_layer,
		    next_count * sizeof(current_layer[0]));
		current_count = next_count;
		++state->distance;
	}
	if (!nearest_present(state, ops, context, YT_NEAREST_FINAL_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		return false;
	state->complete = true;
	state->result = YT_NEAREST_COMPLETE;
	return true;
}

static bool
profit_present(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, enum yt_profit_output_kind kind,
    enum yt_profit_present_mode mode, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	if (!ops->present(context, kind, mode, text, length, &state->style,
	    error))
		return false;
	++state->outputs;
	return true;
}

static bool
profit_checkpoint(const struct yt_profit_ops *ops, void *context,
    enum yt_profit_checkpoint checkpoint, struct yt_error *error)
{
	return ops->checkpoint(context, checkpoint, error);
}

static bool
profit_read(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, enum yt_profit_field_kind kind, float expression,
    struct yt_record *record, struct yt_error *error)
{
	uint32_t physical;

	if (!nearest_single(expression, &state->current_record_expression,
	    error, "profit record expression"))
		return false;
	physical = qb_brun_random_record_number(state->current_record_expression);
	if (!ops->read_record(context, kind, state->current_record_expression,
	    physical, record, error))
		return false;
	state->field = *record;
	state->field_kind = kind;
	state->field_record = physical;
	state->field_valid = true;
	++state->reads;
	return true;
}

static bool
profit_cint(const struct yt_profit_state *state, float value, int *result,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    state->conversion_mode, &overflow);

	if (overflow)
		return nearest_error(error, operation);
	*result = (int)converted;
	return true;
}

static bool
profit_raw_memory_boundary(struct yt_profit_state *state,
    struct yt_error *error)
{
	state->result = YT_PROFIT_UNRESOLVED_RAW_MEMORY;
	if (error != NULL) {
		error->status = YT_INVALID;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "profit unresolved price-array index");
		error->path[0] = '\0';
	}
	return false;
}

static bool
profit_coerce_warps(const struct yt_profit_state *state,
    const struct yt_sector *sector, int warps[6], struct yt_error *error)
{
	size_t slot;

	for (slot = 0U; slot < 6U; ++slot) {
		if (!profit_cint(state, sector->warps[slot], &warps[slot], error,
		    "profit warp target CINT"))
			return false;
	}
	return true;
}

static bool
profit_project(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, const struct yt_port *port,
    struct yt_nearest_market *market, float prices[4],
    struct yt_error *error)
{
	if (!ops->observe_day(context, &state->current_day, error))
		return false;
	++state->day_observations;
	if (!nearest_single(state->current_day, &state->current_day, error,
	    "profit current day")
	    || !ops->observe_timer(context, &state->timer_seconds, error))
		return false;
	++state->timer_observations;
	if (!nearest_single(state->timer_seconds, &state->timer_seconds, error,
	    "profit TIMER")
	    || !yt_nearest_market_project(market, port, state->base_price,
	    state->current_day, state->timer_seconds, error))
		return false;
	prices[0] = 0.0f;
	prices[1] = market->price[0];
	prices[2] = market->price[1];
	prices[3] = market->price[2];
	return true;
}

static void
profit_pair_color(struct yt_profit_state *state, float source, float target)
{
	if ((source == 1.0f && target == 2.0f)
	    || (source == 2.0f && target == 1.0f))
		state->style.foreground = 3.0f;
	else if ((source == 1.0f && target == 3.0f)
	    || (source == 3.0f && target == 1.0f))
		state->style.foreground = 2.0f;
	else if ((source == 2.0f && target == 3.0f)
	    || (source == 3.0f && target == 2.0f))
		state->style.foreground = 1.0f;
}

static bool
profit_right_four(uint8_t result[4], float value, bool integer)
{
	char number[64];
	uint8_t source[68];
	int rendered = integer
	    ? qb_str_integer(number, sizeof(number), (int16_t)(int)value)
	    : qb_str_single(number, sizeof(number), value);
	size_t length;
	size_t amount;

	if (rendered < 0)
		return false;
	source[0] = ' ';
	source[1] = ' ';
	memcpy(source + 2U, number, (size_t)rendered);
	length = 2U + (size_t)rendered;
	amount = length < 4U ? length : 4U;
	memset(result, ' ', 4U - amount);
	memcpy(result + 4U - amount, source + length - amount, amount);
	return true;
}

static bool
profit_compose_row(struct yt_profit_state *state,
    const struct yt_profit_ops *ops, void *context, float source_number,
    int target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], uint8_t row[36],
    struct yt_error *error)
{
	static const uint8_t arrows[3][7] = {
		{'E', 'q', 'u', ' ', '-', '>', ' '},
		{'O', 'r', 'g', ' ', '-', '>', ' '},
		{'O', 'r', 'e', ' ', '-', '>', ' '},
	};
	static const uint8_t labels[3][3] = {
		{'E', 'q', 'u'}, {'O', 'r', 'g'}, {'O', 'r', 'e'},
	};
	float source_subscript;
	float target_subscript;
	float source_leg;
	float target_leg;
	float profit;
	int source_index;
	int target_index;
	int source_text = source_port->commodity_class == 1.0f ? 0
	    : source_port->commodity_class == 2.0f ? 1 : 2;
	int target_text = target_port->commodity_class == 1.0f ? 0
	    : target_port->commodity_class == 2.0f ? 1 : 2;
	char number[64];
	uint8_t field[68];
	int rendered;
	size_t length = 0U;

	if (!nearest_sub(4.0f, source_port->commodity_class,
	    &source_subscript, error, "profit source class subtraction")
	    || !profit_cint(state, source_subscript, &source_index, error,
	    "profit source class CINT")
	    || !nearest_sub(4.0f, target_port->commodity_class,
	    &target_subscript, error, "profit target class subtraction")
	    || !profit_cint(state, target_subscript, &target_index, error,
	    "profit target class CINT"))
		return false;
	if (source_index < 0 || source_index > 3
	    || target_index < 0 || target_index > 3)
		return profit_raw_memory_boundary(state, error);
	if (!nearest_sub(source_price[source_index],
	    target_price[source_index], &source_leg, error,
	    "profit source leg")
	    || !nearest_single(fabsf(source_leg), &source_leg, error,
	    "profit source ABS")
	    || !nearest_sub(target_price[target_index],
	    source_price[target_index], &target_leg, error,
	    "profit target leg")
	    || !nearest_single(fabsf(target_leg), &target_leg, error,
	    "profit target ABS")
	    || !nearest_add(source_leg, target_leg, &profit, error,
	    "profit spread"))
		return false;
	profit_pair_color(state, source_port->commodity_class,
	    target_port->commodity_class);
	if (!profit_checkpoint(ops, context, YT_PROFIT_ROW_CONSTRUCTION, error)
	    || !profit_right_four(row + length, source_number, false))
		return false;
	length += 4U;
	row[length++] = ',';
	if (!profit_right_four(row + length, (float)target_number, true))
		return nearest_error(error, "profit target formatting");
	length += 4U;
	row[length++] = ' ';
	memcpy(row + length, arrows[source_text], 7U);
	length += 7U;
	memcpy(row + length, labels[target_text], 3U);
	length += 3U;
	memcpy(row + length, " @ Profit of", 12U);
	length += 12U;
	rendered = qb_str_single(number, sizeof(number), profit);
	if (rendered < 0 || (size_t)rendered + 3U > sizeof(field))
		return nearest_error(error, "profit value formatting");
	memcpy(field, number, (size_t)rendered);
	memset(field + (size_t)rendered, ' ', 3U);
	memcpy(row + length, field, 4U);
	length += 4U;
	if (length != 36U)
		return nearest_error(error, "profit row width");
	return true;
}

static bool
profit_page(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, bool *keep_going, struct yt_error *error)
{
	static const uint8_t prompt[] = "More? [Y/n] ";
	uint8_t response[8];
	size_t length;
	bool available;

	for (;;) {
		if (!profit_present(state, ops, context, YT_PROFIT_PAGER_PROMPT,
		    YT_PROFIT_PRESENT_RAW, prompt, sizeof(prompt) - 1U, error))
			return false;
		++state->pager_prompts;
		for (;;) {
			length = 0U;
			available = false;
			if (!ops->input(context, response, sizeof(response), &length,
			    &available, error))
				return false;
			if (available)
				break;
		}
		if (length == 1U && response[0] == '\r')
			response[0] = 'Y';
		ops->uppercase(context, response, length);
		if (!profit_present(state, ops, context, YT_PROFIT_PAGER_ECHO,
		    YT_PROFIT_PRESENT_LINE, response, length, error))
			return false;
		if (length == 1U && (response[0] == 'Y' || response[0] == 'N')) {
			*keep_going = response[0] == 'Y';
			return true;
		}
	}
}

static bool
profit_emit_pair(struct yt_profit_state *state,
    const struct yt_profit_ops *ops, void *context, float source_number,
    int target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], bool *keep_going, struct yt_error *error)
{
	static const uint8_t separator[] = {' ', 0xba, ' '};
	uint8_t row[36];
	int count;

	*keep_going = true;
	if (state->global) {
		if (!nearest_add(state->result_count, 1.0f,
		    &state->result_count, error, "profit result counter")
		    || qb_mbf32_encode(state->result_count,
		    state->result_count_raw) == QB_MBF_OVERFLOW)
			return nearest_error(error, "profit result counter raw");
	}
	if (!profit_compose_row(state, ops, context, source_number,
	    target_number, source_port, target_port, source_price, target_price,
	    row, error))
		return false;
	if (!state->global) {
		if (!profit_present(state, ops, context, YT_PROFIT_ROW,
		    YT_PROFIT_PRESENT_BOLD_LINE, row, sizeof(row), error))
			return false;
		++state->rows;
		return true;
	}
	if (!profit_present(state, ops, context, YT_PROFIT_ROW,
	    YT_PROFIT_PRESENT_BOLD_RAW, row, sizeof(row), error))
		return false;
	++state->rows;
	state->style.foreground = 6.0f;
	if (!profit_cint(state, state->result_count, &count, error,
	    "profit parity CINT"))
		return false;
	if ((count & 1) != 0) {
		if (!profit_present(state, ops, context,
		    YT_PROFIT_COLUMN_SEPARATOR, YT_PROFIT_PRESENT_BOLD_RAW,
		    separator, sizeof(separator), error))
			return false;
	}
	else if (!profit_present(state, ops, context, YT_PROFIT_ROW_END,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error))
		return false;
	if (!profit_cint(state, state->result_count, &count, error,
	    "profit pager CINT"))
		return false;
	if (count % 44 == 0 && !profit_page(state, ops, context,
	    keep_going, error))
		return false;
	return true;
}

bool
yt_profit_run(struct yt_profit_state *state, const struct yt_profit_ops *ops,
    void *context, struct yt_error *error)
{
	static const uint8_t title[] =
	    "Profits of a two way trade to ports in adjacent sectors.";
	static const uint8_t no_port[] = "NO trading port in your sector!";
	static const uint8_t no_results[] =
	    "No ports you can trade with in adjacent sectors!";
	static const uint8_t end_banner[] = " *-[ End of List ]-*";
	float source;

	if (state == NULL || ops == NULL || ops->read_record == NULL
	    || ops->observe_day == NULL || ops->observe_timer == NULL
	    || ops->present == NULL || ops->input == NULL
	    || ops->uppercase == NULL || ops->checkpoint == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "profit arguments");
	state->current_record_expression = 0.0f;
	state->timer_seconds = 0.0f;
	state->maximum_sector = 0.0f;
	state->result_count = 0.0f;
	memcpy(state->initial_result_raw,
	    state->global ? "\x00\x00\x04\x00" : "\x00\x00\x20\x00", 4U);
	memset(state->result_count_raw, 0, sizeof(state->result_count_raw));
	if (state->global)
		memcpy(state->result_count_raw, state->initial_result_raw,
		    sizeof(state->result_count_raw));
	state->rows = 0U;
	state->outputs = 0U;
	state->reads = 0U;
	state->day_observations = 0U;
	state->timer_observations = 0U;
	state->pager_prompts = 0U;
	state->stopped = false;
	state->complete = false;
	state->result = YT_PROFIT_INCOMPLETE;

	if (!state->global)
		state->style.foreground = 7.0f;
	if (!profit_present(state, ops, context, YT_PROFIT_LEADING_BLANK,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error))
		return false;
	if (!state->global
	    && (!profit_present(state, ops, context, YT_PROFIT_TITLE,
	    YT_PROFIT_PRESENT_BOLD_LINE, title, sizeof(title) - 1U, error)
	    || !profit_present(state, ops, context, YT_PROFIT_TITLE_BLANK,
	    YT_PROFIT_PRESENT_LINE, NULL, 0U, error)))
		return false;

	if (state->global) {
		if (!nearest_sub(state->port_record_offset,
		    state->sector_record_offset, &state->maximum_sector, error,
		    "profit maximum sector")
		    || !profit_checkpoint(ops, context,
		    YT_PROFIT_MAXIMUM_READY, error))
			return false;
		source = 2.0f;
	}
	else {
		struct yt_record raw;
		struct yt_sector sector;
		struct yt_port source_port;
		struct yt_nearest_market source_market;
		float source_prices[4];
		float display_source;
		int warps[6];
		size_t slot;

		if (!nearest_single(state->current_sector_record,
		    &state->current_sector_record, error,
		    "profit current sector record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_SECTOR,
		    state->current_sector_record, &raw, error))
			return false;
		yt_sector_decode(&sector, &raw);
		if (sector.port == 0.0f || sector.port == 1.0f) {
			if (!profit_present(state, ops, context,
			    YT_PROFIT_NO_CURRENT_PORT,
			    YT_PROFIT_PRESENT_BOLD_LINE, no_port,
			    sizeof(no_port) - 1U, error))
				return false;
			state->complete = true;
			state->result = YT_PROFIT_NO_CURRENT_PORT_RESULT;
			return true;
		}
		if (!profit_coerce_warps(state, &sector, warps, error)
		    || !nearest_add(state->port_record_offset, sector.port,
		    &source, error, "profit current port record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_PORT,
		    source, &raw, error))
			return false;
		yt_port_decode(&source_port, &raw);
		if (!profit_project(state, ops, context, &source_port,
		    &source_market, source_prices, error)
		    || !profit_checkpoint(ops, context,
		    YT_PROFIT_SOURCE_ARROW_READY, error))
			return false;
		for (slot = 0U; slot < 6U; ++slot) {
			struct yt_sector target_sector;
			struct yt_port target_port;
			struct yt_nearest_market target_market;
			float target_prices[4];
			int target;
			bool keep_going;

			target = warps[slot];
			if (target <= 1)
				continue;
			if (!nearest_add(state->sector_record_offset, (float)target,
			    &source, error, "profit target sector record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_SECTOR, source, &raw, error))
				return false;
			yt_sector_decode(&target_sector, &raw);
			if (target_sector.port == 0.0f)
				continue;
			if (!nearest_add(state->port_record_offset,
			    target_sector.port, &source, error,
			    "profit target port record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_PORT, source, &raw, error))
				return false;
			yt_port_decode(&target_port, &raw);
			if (target_port.commodity_class
			    == source_port.commodity_class)
				continue;
			if (!profit_project(state, ops, context, &target_port,
			    &target_market, target_prices, error)
			    || !nearest_sub(state->current_sector_record,
			    state->sector_record_offset, &display_source, error,
			    "profit display sector")
			    || !profit_emit_pair(state, ops, context, display_source,
			    target, &source_port, &target_port, source_prices,
			    target_prices, &keep_going, error))
				return false;
		}
		if (state->rows == 0U) {
			if (!profit_present(state, ops, context, YT_PROFIT_NO_RESULTS,
			    YT_PROFIT_PRESENT_BOLD_LINE, no_results,
			    sizeof(no_results) - 1U, error))
				return false;
			state->result = YT_PROFIT_NO_RESULTS_RESULT;
		}
		else
			state->result = YT_PROFIT_COMPLETE;
		state->complete = true;
		return true;
	}

	while (source <= state->maximum_sector) {
		struct yt_record raw;
		struct yt_sector sector;
		struct yt_port source_port;
		struct yt_nearest_market source_market;
		float source_prices[4];
		float expression;
		int warps[6];
		size_t slot;

		if (!nearest_add(state->sector_record_offset, source,
		    &expression, error, "profit source sector record")
		    || !profit_read(state, ops, context, YT_PROFIT_FIELD_SECTOR,
		    expression, &raw, error))
			return false;
		yt_sector_decode(&sector, &raw);
		if (!profit_coerce_warps(state, &sector, warps, error))
			return false;
		if (sector.port > 0.0f) {
			if (!nearest_add(state->port_record_offset, sector.port,
			    &expression, error, "profit source port record")
			    || !profit_read(state, ops, context,
			    YT_PROFIT_FIELD_PORT, expression, &raw, error))
				return false;
			yt_port_decode(&source_port, &raw);
			if (!profit_project(state, ops, context, &source_port,
			    &source_market, source_prices, error)
			    || !profit_checkpoint(ops, context,
			    YT_PROFIT_SOURCE_ARROW_READY, error))
				return false;
			for (slot = 0U; slot < 6U; ++slot) {
				struct yt_sector target_sector;
				struct yt_port target_port;
				struct yt_nearest_market target_market;
				float target_prices[4];
				int target;
				bool keep_going;

				target = warps[slot];
				if (target <= 1)
					continue;
				if (!nearest_add(state->sector_record_offset,
				    (float)target, &expression, error,
				    "profit target sector record")
				    || !profit_read(state, ops, context,
				    YT_PROFIT_FIELD_SECTOR, expression, &raw, error))
					return false;
				yt_sector_decode(&target_sector, &raw);
				if (target_sector.port == 0.0f
				    || (float)target <= source)
					continue;
				if (!nearest_add(state->port_record_offset,
				    target_sector.port, &expression, error,
				    "profit target port record")
				    || !profit_read(state, ops, context,
				    YT_PROFIT_FIELD_PORT, expression, &raw, error))
					return false;
				yt_port_decode(&target_port, &raw);
				if (target_port.commodity_class
				    == source_port.commodity_class)
					continue;
				if (!profit_project(state, ops, context, &target_port,
				    &target_market, target_prices, error)
				    || !profit_emit_pair(state, ops, context, source,
				    target, &source_port, &target_port, source_prices,
				    target_prices, &keep_going, error))
					return false;
				if (!keep_going) {
					state->stopped = true;
					state->complete = true;
					state->result = YT_PROFIT_STOPPED_BY_N;
					return true;
				}
			}
		}
		if (!nearest_add(source, 1.0f, &source, error,
		    "profit source increment"))
			return false;
	}
	state->style.foreground = 7.0f;
	if (!profit_present(state, ops, context, YT_PROFIT_END_BANNER,
	    YT_PROFIT_PRESENT_BOLD_LINE, end_banner,
	    sizeof(end_banner) - 1U, error))
		return false;
	state->complete = true;
	state->result = YT_PROFIT_COMPLETE;
	return true;
}

static bool
current_player_hydration_fault(struct yt_error *error,
    enum yt_basic_fault_site site, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
		(void)yt_error_attach_basic_fault_number(error, site, 6U);
	}
	return false;
}

static enum qb_mbf_status
current_player_add_single_raw(const uint8_t left[4], const uint8_t right[4],
    const uint8_t dirty_zero_source[4], uint8_t result[4])
{
	volatile float sum;
	enum qb_mbf_status status;

	if (right[3] == 0U) {
		memcpy(result, left, 4U);
		return QB_MBF_OK;
	}
	if (left[3] == 0U) {
		memcpy(result, right, 4U);
		return QB_MBF_OK;
	}
	sum = qb_mbf32_decode(left) + qb_mbf32_decode(right);
	status = qb_mbf32_encode(sum, result);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return status;
	if (status == QB_MBF_UNDERFLOW || sum == 0.0f) {
		memcpy(result, dirty_zero_source, 3U);
		result[3] = 0U;
	}
	return status;
}

bool
yt_current_player_hydrate_run(
    struct yt_current_player_hydration_state *state,
    yt_current_player_read_fn read_player, void *context,
    struct yt_error *error)
{
	struct yt_player fresh;
	uint8_t current_sector_raw[8] = {0};
	enum qb_mbf_status add_status;
	int record;

	if (state == NULL || state->player == NULL || read_player == NULL
	    || state->current_sector_record == NULL)
		return false;
	record = state->player_record;
	if (!read_player(context, record, &fresh, error))
		return false;

	state->player->record = fresh.record;
	state->player->sector = fresh.sector;
	state->player->fighters = fresh.fighters;
	add_status = current_player_add_single_raw(state->sector_record_offset_raw,
	    fresh.record.bytes + YT_F57, fresh.record.bytes + YT_F61,
	    current_sector_raw);
	if (add_status == QB_MBF_OVERFLOW || add_status == QB_MBF_DOMAIN)
		return current_player_hydration_fault(error,
		    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_SECTOR_ADD,
		    "current-player A41C sector ADD_FLOAT");
	*state->current_sector_record = qb_mbf32_decode(current_sector_raw);
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
	if (!state->anti_cloak_enabled) {
		if (state->player_cache != NULL)
			(void)yt_player_cache_set_raw(state->player_cache, record,
			    YT_PLAYER_CACHE_CLOAK,
			    fresh.record.bytes + YT_F125);
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
yt_game_construct_player(struct yt_game *game, int basic_record,
    const uint8_t today_raw[4], const uint8_t turns_raw[4],
    struct yt_player *player, struct yt_player_constructor_state *state,
    struct yt_error *error)
{
	static const uint8_t first_zero[4] = {0x00, 0x00, 0x0a, 0x00};
	static const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
	static const uint8_t one[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t hundred[4] = {0x00, 0x00, 0x48, 0x87};
	struct yt_record config_record;
	struct yt_record constructed;
	struct yt_player_constructor_state local_state;

	if (game == NULL || today_raw == NULL || turns_raw == NULL
	    || player == NULL)
		return false;
	if (state == NULL)
		state = &local_state;
	memset(state, 0, sizeof(*state));

	if (!yt_database_read(&game->database, 1, &config_record, error))
		return false;
	state->config_hydrated = true;
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	state->player_hydrated = true;
	constructed = player->record;
	(void)yt_record_set_raw_number(&constructed, YT_F41, today_raw);
	(void)yt_record_set_raw_number(&constructed, YT_F45, first_zero);
	(void)yt_record_set_raw_number(&constructed, YT_F49, turns_raw);
	(void)yt_record_set_raw_number(&constructed, YT_F53, hundred);
	(void)yt_record_set_raw_number(&constructed, YT_F57, one);
	(void)yt_record_set_raw_number(&constructed, YT_F61,
	    config_record.bytes + YT_F65);
	(void)yt_record_set_raw_number(&constructed, YT_F65,
	    config_record.bytes + YT_F73);
	(void)yt_record_set_raw_number(&constructed, YT_F69, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F73, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F77, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F81,
	    config_record.bytes + YT_F69);
	(void)yt_record_set_raw_number(&constructed, YT_F93, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F97, one);
	(void)yt_record_set_raw_number(&constructed, YT_F101, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F89, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F105, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F113, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F125, one);
	(void)yt_record_set_raw_number(&constructed, YT_F117, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F121, zero);
	(void)yt_record_set_raw_number(&constructed, YT_F129, zero);
	yt_player_decode(player, &constructed);
	state->put_attempted = true;
	return yt_database_write(&game->database, (size_t)basic_record,
	    &constructed, error);
}

bool
yt_game_set_player_identity(struct yt_game *game, int basic_record,
    const uint8_t *name, size_t length, struct yt_player *player,
    struct yt_error *error)
{
	static const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
	struct yt_record identity;
	uint8_t length_raw[4];

	if ((name == NULL && length != 0)
	    || !yt_game_read_player(game, basic_record, player, error))
		return false;
	identity = player->record;
	yt_record_set_text(&identity, name, length);
	(void)qb_mbf32_encode((float)length, length_raw);
	(void)yt_record_set_raw_number(&identity, YT_F85, length_raw);
	(void)yt_record_set_raw_number(&identity, YT_F89, zero);
	yt_player_decode(player, &identity);
	return yt_database_write(&game->database, (size_t)basic_record,
	    &identity, error);
}

bool
yt_game_post_login_repairs(struct yt_game *game, int basic_record,
    const uint8_t one_raw[4], const uint8_t zero_raw[4],
    const uint8_t maximum_holds_raw[4], struct yt_player *player,
    struct yt_post_login_repairs *repairs, struct yt_error *error)
{
	struct yt_post_login_repairs applied = {false, false, 0};
	struct yt_record repaired;
	float maximum_holds;

	if (repairs != NULL)
		*repairs = applied;
	if (game == NULL || one_raw == NULL || zero_raw == NULL
	    || maximum_holds_raw == NULL || player == NULL)
		return false;
	maximum_holds = qb_mbf32_decode(maximum_holds_raw);
	if (!yt_game_read_player(game, basic_record, player, error))
		return false;
	if (qb_mbf32_decode(player->record.bytes + YT_F57)
	    < qb_mbf32_decode(one_raw)) {
		repaired = player->record;
		(void)yt_record_set_raw_number(&repaired, YT_F57, one_raw);
		yt_player_decode(player, &repaired);
		applied.sector = true;
		if (!yt_database_write(&game->database, (size_t)basic_record,
		    &repaired, error)
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
	if ((double)player->holds > (double)maximum_holds) {
		repaired = player->record;
		(void)yt_record_set_raw_number(&repaired, YT_F69, zero_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F73, zero_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F77,
		    maximum_holds_raw);
		(void)yt_record_set_raw_number(&repaired, YT_F65,
		    maximum_holds_raw);
		yt_player_decode(player, &repaired);
		applied.holds = true;
		if (!yt_database_write(&game->database, (size_t)basic_record,
		    &repaired, error)
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

bool
yt_main_prompt_run(struct yt_main_prompt_state *state,
    const struct yt_main_prompt_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Main Command (?=Help)? ";
	uint8_t prompt[512];
	size_t prompt_length;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || state->response == NULL || state->response_capacity == 0U
	    || (state->time_text_length != 0U && state->time_text == NULL)
	    || ops->effect == NULL || ops->hydrate == NULL
	    || ops->present == NULL || ops->edit == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	state->response[0] = '\0';
	state->response_length = 0U;
	state->route = YT_MAIN_SHELL_DISPLAY;
	state->player_hydrated = false;
	state->prompt_presented = false;
	state->input_available = false;
	state->complete = false;

	ops->effect(context, YT_MAIN_PROMPT_RESET_PAGER);
	if (!ops->hydrate(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->player_hydrated = true;
	ops->effect(context, YT_MAIN_PROMPT_SET_FOREGROUND);
	if (!ops->present(context, NULL, 0U, YT_MAIN_PROMPT_LEADING_BLANK,
	    error))
		return false;
	ops->effect(context, YT_MAIN_PROMPT_RESET_SCANNER);
	if (state->time_text_length > state->time_text_capacity
	    || state->time_text_length > sizeof(prompt) - (sizeof(prefix) - 1U)
	    - (sizeof(suffix) - 1U))
		return startup_configuration_error(error, YT_RANGE,
		    "main prompt time capacity");
	prompt_length = 0U;
	memcpy(prompt + prompt_length, prefix, sizeof(prefix) - 1U);
	prompt_length += sizeof(prefix) - 1U;
	if (state->time_text_length != 0U) {
		memcpy(prompt + prompt_length, state->time_text,
		    state->time_text_length);
		prompt_length += state->time_text_length;
	}
	memcpy(prompt + prompt_length, suffix, sizeof(suffix) - 1U);
	prompt_length += sizeof(suffix) - 1U;
	if (!ops->present(context, prompt, prompt_length, YT_MAIN_PROMPT_TEXT,
	    error))
		return false;
	state->prompt_presented = true;
	if (!ops->edit(context, state->response, state->response_capacity,
	    &state->response_length, &state->input_available, error))
		return false;
	if (state->response_length >= state->response_capacity)
		return startup_configuration_error(error, YT_RANGE,
		    "main prompt response capacity");
	state->response[state->response_length] = '\0';
	if (state->input_available)
		state->route = yt_main_shell_dispatch(state->response);
	state->complete = true;
	return true;
}

bool
yt_computer_prompt_run(struct yt_computer_prompt_state *state,
    const struct yt_computer_prompt_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "Time:";
	static const uint8_t suffix[] = "Computer command (?=help)? ";
	uint8_t prompt[512];
	size_t prompt_length;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || state->response == NULL || state->response_capacity < 3U
	    || (state->time_text_length != 0U && state->time_text == NULL)
	    || ops->effect == NULL || ops->hydrate == NULL
	    || ops->present == NULL || ops->edit == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	state->response[0] = '\0';
	state->response_length = 0U;
	state->player_hydrated = false;
	state->prompt_presented = false;
	state->input_available = false;
	state->complete = false;

	if (!ops->hydrate(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->player_hydrated = true;
	ops->effect(context, YT_COMPUTER_PROMPT_RESET_SCANNER);
	if (!ops->present(context, NULL, 0U,
	    YT_COMPUTER_PROMPT_LEADING_BLANK, error))
		return false;
	ops->effect(context, YT_COMPUTER_PROMPT_SET_FOREGROUND);
	if (state->time_text_length > state->time_text_capacity
	    || state->time_text_length > sizeof(prompt) - (sizeof(prefix) - 1U)
	    - (sizeof(suffix) - 1U))
		return startup_configuration_error(error, YT_RANGE,
		    "computer prompt time capacity");
	prompt_length = 0U;
	memcpy(prompt + prompt_length, prefix, sizeof(prefix) - 1U);
	prompt_length += sizeof(prefix) - 1U;
	if (state->time_text_length != 0U) {
		memcpy(prompt + prompt_length, state->time_text,
		    state->time_text_length);
		prompt_length += state->time_text_length;
	}
	memcpy(prompt + prompt_length, suffix, sizeof(suffix) - 1U);
	prompt_length += sizeof(suffix) - 1U;
	if (!ops->present(context, prompt, prompt_length,
	    YT_COMPUTER_PROMPT_TEXT, error))
		return false;
	state->prompt_presented = true;
	if (!ops->edit(context, state->response, state->response_capacity,
	    &state->response_length, &state->input_available, error))
		return false;
	if (state->response_length >= state->response_capacity)
		return startup_configuration_error(error, YT_RANGE,
		    "computer prompt response capacity");
	state->response[state->response_length] = '\0';
	if (state->input_available) {
		if (state->response_length == 0U) {
			state->response[0] = '?';
			state->response[1] = '\0';
			state->response_length = 1U;
		}
		if (state->response_length > 2U) {
			state->response[2] = '\0';
			state->response_length = 2U;
		}
	}
	state->complete = true;
	return true;
}

bool
yt_computer_activation_run(struct yt_computer_activation_state *state,
    const struct yt_computer_activation_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t notice[] = "<Computer activated>";

	if (state == NULL || ops == NULL || ops->effect == NULL
	    || ops->present == NULL || ops->sound == NULL)
		return false;
	state->notice_presented = false;
	state->complete = false;
	ops->effect(context, YT_COMPUTER_ACTIVATION_SET_FOREGROUND);
	if (!ops->present(context, notice, sizeof(notice) - 1U, error))
		return false;
	state->notice_presented = true;
	if (!ops->sound(context, 4.0f, error))
		return false;
	state->complete = true;
	return true;
}

bool
yt_computer_spy_run(struct yt_computer_spy_state *state,
    const struct yt_computer_spy_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t none[] = "You do not have any spies!";
	size_t index;

	if (state == NULL || ops == NULL || ops->read_target == NULL
	    || ops->present == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "active-spy arguments");
	state->counter = 0.0f;
	state->current_target = 0;
	state->target_reads = 0U;
	state->outputs = 0U;
	state->rows = 0U;
	state->target_valid = false;
	state->complete = false;
	if (state->count != 0.0f && state->count != 1.0f
	    && state->count != 2.0f && state->count != 3.0f)
		return startup_configuration_error(error, YT_RANGE,
		    "active-spy count");
	if (state->count == 0.0f) {
		if (!ops->present(context, none, sizeof(none) - 1U,
		    YT_COMPUTER_SPY_NONE, error))
			return false;
		state->outputs = 1U;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U,
	    YT_COMPUTER_SPY_LEADING_BLANK, error))
		return false;
	state->outputs = 1U;
	for (index = 0U; index < (size_t)state->count; ++index) {
		char counter[64];
		char target[64];
		uint8_t row[160];
		int counter_length;
		int target_length;
		int row_length;

		state->counter = (float)(index + 1U);
		state->target_valid = false;
		if (!ops->read_target(context, index, &state->current_target,
		    error))
			return false;
		++state->target_reads;
		state->target_valid = true;
		counter_length = qb_str_single(counter, sizeof(counter),
		    state->counter);
		target_length = qb_str_integer(target, sizeof(target),
		    state->current_target);
		if (counter_length < 0 || target_length < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "active-spy numeric row");
		row_length = snprintf((char *)row, sizeof(row),
		    "Spy #%.*s will hunt in sector%.*s.", counter_length,
		    counter, target_length, target);
		if (row_length < 0 || (size_t)row_length >= sizeof(row))
			return startup_configuration_error(error, YT_RANGE,
			    "active-spy row");
		if (!ops->present(context, row, (size_t)row_length,
		    YT_COMPUTER_SPY_ROW, error))
			return false;
		++state->outputs;
		++state->rows;
	}
	state->counter = state->count + 1.0f;
	state->complete = true;
	return true;
}

bool
yt_computer_scoreboard_run(struct yt_computer_scoreboard_state *state,
    const struct yt_computer_scoreboard_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x04, 0x00};
	char edited[YT_COMPUTER_SCOREBOARD_RESPONSE_SIZE];
	size_t length = 0U;
	bool available = false;

	if (state == NULL || ops == NULL || state->pathname == NULL
	    || ops->clear_pager_key == NULL || ops->present == NULL
	    || ops->edit == NULL || ops->reset_pager == NULL
	    || ops->generate == NULL || ops->view == NULL)
		return false;
	memset(state->raw_response, 0, sizeof(state->raw_response));
	memset(state->response, 0, sizeof(state->response));
	memcpy(state->pager_line_count_raw, dirty_zero, sizeof(dirty_zero));
	state->raw_response_length = 0U;
	state->response_length = 0U;
	state->pager_key_cleared = false;
	state->selector_presented = false;
	state->input_available = false;
	state->pager_reset = false;
	state->updated = false;
	state->generator_called = false;
	state->generator_complete = false;
	state->viewer_called = false;
	state->complete = false;

	ops->clear_pager_key(context);
	state->pager_key_cleared = true;
	if (!ops->present(context, NULL, 0U,
	    YT_COMPUTER_SCOREBOARD_LEADING_BLANK, error)
	    || !ops->present(context, prompt, sizeof(prompt) - 1U,
	    YT_COMPUTER_SCOREBOARD_SELECTOR_PROMPT, error))
		return false;
	state->selector_presented = true;
	if (!ops->edit(context, edited, sizeof(edited), &length,
	    &available, error))
		return false;
	state->input_available = available;
	if (!available)
		return false;
	if (length >= sizeof(edited))
		return startup_configuration_error(error, YT_RANGE,
		    "scoreboard selector response capacity");
	edited[length] = '\0';
	memcpy(state->raw_response, edited, length + 1U);
	state->raw_response_length = length;
	qb_compat_upper_n((uint8_t *)edited, length);
	memcpy(state->response, edited, length + 1U);
	state->response_length = length;

	ops->reset_pager(context, state->pager_line_count_raw);
	state->pager_reset = true;
	if (!ops->present(context, NULL, 0U,
	    YT_COMPUTER_SCOREBOARD_TRAILING_BLANK, error))
		return false;
	state->updated = !(length == 1U && edited[0] == 'O');
	if (state->updated) {
		if (!ops->present(context, heading, sizeof(heading) - 1U,
		    YT_COMPUTER_SCOREBOARD_UPDATED_HEADING, error))
			return false;
		state->generator_called = true;
		if (!ops->generate(context, error))
			return false;
		state->generator_complete = true;
		if (!ops->present(context, NULL, 0U,
		    YT_COMPUTER_SCOREBOARD_POST_GENERATOR_BLANK, error))
			return false;
	}
	state->viewer_called = true;
	if (!ops->view(context, state->pathname, error))
		return false;
	state->complete = true;
	return true;
}

bool
yt_computer_newspaper_run(struct yt_computer_newspaper_state *state,
    const struct yt_computer_newspaper_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	static const char today_path[] = "YTNEWS.DAT";
	static const char yesterday_path[] = "YTYNEWS.DAT";
	char edited[YT_COMPUTER_NEWSPAPER_RESPONSE_SIZE];

	if (state == NULL || ops == NULL || ops->checkpoint == NULL
	    || ops->present == NULL
	    || ops->edit == NULL || ops->view == NULL)
		return false;
	memset(state->raw_response, 0, sizeof(state->raw_response));
	memset(state->response, 0, sizeof(state->response));
	state->raw_response_length = 0U;
	state->response_length = 0U;
	state->attempts = 0U;
	state->selected_pathname = NULL;
	state->choice = YT_COMPUTER_NEWSPAPER_NONE;
	state->checkpoint_reached = false;
	state->checkpoint_resumed = false;
	state->leading_blank_presented = false;
	state->input_available = false;
	state->viewer_called = false;
	state->complete = false;

	{
		bool resume = false;

		state->checkpoint_reached = true;
		if (!ops->checkpoint(context, &resume, error) || !resume)
			return false;
		state->checkpoint_resumed = true;
	}
	if (!ops->present(context, NULL, 0U,
	    YT_COMPUTER_NEWSPAPER_LEADING_BLANK, error))
		return false;
	state->leading_blank_presented = true;
	for (;;) {
		size_t length = 0U;
		bool available = false;

		if (!ops->present(context, prompt, sizeof(prompt) - 1U,
		    YT_COMPUTER_NEWSPAPER_SELECTOR_PROMPT, error)
		    || !ops->edit(context, edited, sizeof(edited), &length,
		    &available, error))
			return false;
		++state->attempts;
		state->input_available = available;
		if (!available)
			return false;
		if (length >= sizeof(edited))
			return startup_configuration_error(error, YT_RANGE,
			    "newspaper selector response capacity");
		edited[length] = '\0';
		memcpy(state->raw_response, edited, length + 1U);
		state->raw_response_length = length;
		qb_compat_upper_n((uint8_t *)edited, length);
		memcpy(state->response, edited, length + 1U);
		state->response_length = length;
		if (length == 1U && edited[0] == 'T') {
			state->choice = YT_COMPUTER_NEWSPAPER_TODAY;
			state->selected_pathname = today_path;
			break;
		}
		if (length == 1U && edited[0] == 'Y') {
			state->choice = YT_COMPUTER_NEWSPAPER_YESTERDAY;
			state->selected_pathname = yesterday_path;
			break;
		}
	}
	state->viewer_called = true;
	if (!ops->view(context, state->selected_pathname, error))
		return false;
	state->complete = true;
	return true;
}

bool
yt_radio_read_run(struct yt_radio_read_state *state,
    const struct yt_radio_read_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t log_heading[] =
	    "Log of messages sent/recieved.";
	static const uint8_t pause[] = "[Pause]";
	static const uint8_t none[] = "None Found.";
	struct yt_radio_pager_state pager;
	uint32_t record_number;

	if (state == NULL || ops == NULL || ops->open == NULL
	    || ops->size == NULL || ops->get == NULL || ops->name == NULL
	    || ops->present == NULL || ops->wait == NULL || ops->put == NULL
	    || ops->close == NULL)
		return false;
	state->byte_length = 0U;
	state->probe_count = 0U;
	state->record_number = 0U;
	memset(&state->radio_field, 0, sizeof(state->radio_field));
	state->previous_recipient = 0.0f;
	state->previous_sender = 0.0f;
	state->player_field_record = 0.0f;
	state->private_line_count = 0.0f;
	state->visible_records = 0U;
	state->name_accesses = 0U;
	state->positive_name_gets = 0U;
	state->writes = 0U;
	state->waits = 0U;
	state->player_field_role = YT_RADIO_READ_NAME_NONE;
	state->radio_field_valid = false;
	state->player_field_valid = false;
	state->file_open = false;
	state->close_attempted = false;
	state->visible = false;
	state->complete = false;

	if (!ops->present(context, NULL, 0U, YT_RADIO_READ_OPENING_BLANK,
	    error)
	    || !ops->present(context,
	    state->reader_mode != 0.0f ? log_heading : automatic_heading,
	    state->reader_mode != 0.0f ? sizeof(log_heading) - 1U
	    : sizeof(automatic_heading) - 1U, YT_RADIO_READ_HEADING, error))
		return false;
	yt_radio_pager_begin(&pager);
	if (!ops->open(context, error))
		return false;
	state->file_open = true;
	if (!ops->size(context, &state->byte_length, error))
		goto abort;
	state->probe_count = state->byte_length / YT_RADIO_RECORD_SIZE + 1U;
	if (state->probe_count > 0xFFFFFFU) {
		(void)startup_configuration_error(error, YT_RANGE,
		    "radio scan bound");
		goto abort;
	}
	for (record_number = 1U; record_number <= state->probe_count;
	    ++record_number) {
		struct yt_radio_record record;
		struct yt_radio_reader_decision decision;
		float counter;
		float recipient;
		float sender;

		state->record_number = record_number;
		if (!ops->get(context, record_number, &record, error))
			goto abort;
		state->radio_field = record;
		state->radio_field_valid = true;
		counter = yt_radio_get_number(&record, 0U);
		recipient = yt_radio_get_number(&record, 4U);
		sender = yt_radio_get_number(&record, 8U);
		if (!yt_radio_reader_decide(counter, recipient, sender,
		    state->current_player, state->reader_mode, &decision, error))
			goto abort;
		if (decision.visible) {
			uint8_t from[YT_TEXT_FIELD_SIZE];
			uint8_t to[YT_TEXT_FIELD_SIZE];
			uint8_t header[2U * YT_TEXT_FIELD_SIZE + 32U];
			size_t from_length;
			size_t to_length;
			size_t header_length;

			state->visible = true;
			++state->visible_records;
			++state->name_accesses;
			if (!ops->name(context, recipient, false, to, sizeof(to),
			    &to_length, error))
				goto abort;
			if (recipient > 0.0f) {
				state->player_field_valid = true;
				state->player_field_record = recipient;
				state->player_field_role =
				    YT_RADIO_READ_NAME_RECIPIENT;
				++state->positive_name_gets;
			}
			++state->name_accesses;
			if (!ops->name(context, sender, true, from, sizeof(from),
			    &from_length, error))
				goto abort;
			if (sender > 0.0f) {
				state->player_field_valid = true;
				state->player_field_record = sender;
				state->player_field_role = YT_RADIO_READ_NAME_SENDER;
				++state->positive_name_gets;
			}
			if (sender != state->previous_sender
			    || recipient != state->previous_recipient) {
				if (!yt_radio_reader_header(to, to_length, from,
				    from_length, header, sizeof(header),
				    &header_length)
				    || !ops->present(context, NULL, 0U,
				    YT_RADIO_READ_PAIR_BLANK, error)
				    || !ops->present(context, header, header_length,
				    YT_RADIO_READ_PAIR_HEADER, error))
					goto abort;
				yt_radio_pager_add_pair(&pager);
			}
			if (!ops->present(context, record.bytes + 12U, 74U,
			    YT_RADIO_READ_BODY, error))
				goto abort;
			state->previous_sender = sender;
			state->previous_recipient = recipient;
			if (yt_radio_pager_add_body(&pager)) {
				if (!ops->present(context, pause, sizeof(pause) - 1U,
				    YT_RADIO_READ_PAUSE, error)
				    || !ops->wait(context, 99.0, error))
					goto abort;
				++state->waits;
				if (!ops->present(context, NULL, 0U,
				    YT_RADIO_READ_PAUSE_BLANK, error))
					goto abort;
			}
			state->private_line_count = pager.line_count;
			if (decision.automatic_write) {
				if (!yt_radio_reader_mutate(&record, counter)
				    || !ops->put(context, record_number, &record,
				    error))
					goto abort;
				++state->writes;
			}
		}
	}
	if (!state->visible && !ops->present(context, none,
	    sizeof(none) - 1U, YT_RADIO_READ_NONE, error))
		goto abort;
	state->close_attempted = true;
	if (!ops->close(context, error))
		return false;
	state->file_open = false;
	state->complete = true;
	return true;

abort:
	if (state->file_open) {
		state->close_attempted = true;
		if (ops->close(context, NULL))
			state->file_open = false;
	}
	return false;
}

bool
yt_radio_send_run(struct yt_radio_send_state *state,
    const struct yt_radio_send_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t header_prefix[] = "  -  Message from: ";
	static const uint8_t line_prefix[] = "  -  ";
	uint8_t news[sizeof(line_prefix) - 1U + 75U];
	size_t recipient;
	size_t line;

	if (state == NULL || ops == NULL || ops->append_news == NULL
	    || ops->append_radio == NULL || ops->present_success == NULL
	    || (state->recipient_count != 1U
	    && state->recipient_count != YT_RADIO_SEND_RECIPIENTS)
	    || state->line_count == 0U || state->line_count > YT_RADIO_SEND_LINES)
		return false;
	for (line = 0U; line < state->line_count; ++line) {
		if ((state->lines[line].data == NULL
		    && state->lines[line].length != 0U)
		    || state->lines[line].length > 75U)
			return false;
	}
	state->recipient_index = 0U;
	state->line_index = 0U;
	state->news_completed = 0U;
	state->radio_completed = 0U;
	state->broadcast = state->recipients[0] == -2.0f;
	state->success_presented = false;
	state->draft_erased = false;
	state->complete = false;
	if (state->broadcast) {
		size_t length;

		if ((state->sender_name == NULL
		    && state->sender_name_length != 0U)
		    || state->sender_name_length > YT_TEXT_FIELD_SIZE)
			return false;
		memcpy(news, header_prefix, sizeof(header_prefix) - 1U);
		if (state->sender_name_length != 0U)
			memcpy(news + sizeof(header_prefix) - 1U,
			    state->sender_name, state->sender_name_length);
		length = sizeof(header_prefix) - 1U + state->sender_name_length;
		if (!ops->append_news(context, news, length, error))
			return false;
		++state->news_completed;
	}
	for (recipient = 0U; recipient < state->recipient_count; ++recipient) {
		state->recipient_index = recipient;
		if (state->recipients[recipient] == 0.0f)
			continue;
		for (line = 0U; line < state->line_count; ++line) {
			state->line_index = line;
			if (state->broadcast) {
				size_t length = sizeof(line_prefix) - 1U
				    + state->lines[line].length;

				memcpy(news, line_prefix, sizeof(line_prefix) - 1U);
				if (state->lines[line].length != 0U)
					memcpy(news + sizeof(line_prefix) - 1U,
					    state->lines[line].data,
					    state->lines[line].length);
				if (!ops->append_news(context, news, length, error))
					return false;
				++state->news_completed;
			}
			if (!ops->append_radio(context, state->lines[line].data,
			    state->lines[line].length, state->sender,
			    state->recipients[recipient], error))
				return false;
			++state->radio_completed;
		}
	}
	if (!ops->present_success(context, error))
		return false;
	state->success_presented = true;
	state->draft_erased = true;
	state->complete = true;
	return true;
}

bool
yt_radio_team_target_run(struct yt_radio_team_target_state *state,
    yt_team_loader_read_record_fn read_record, void *context,
    struct yt_error *error)
{
	struct yt_team_loader_state loader;
	size_t index;
	bool loaded;

	if (state == NULL || state->cache == NULL || read_record == NULL)
		return false;
	memset(state->recipients, 0, sizeof(state->recipients));
	state->recipient_count = 0U;
	state->physical_record = 0U;
	state->loader_route = YT_TEAM_LOADER_OUT_OF_RANGE;
	state->teamless = state->raw_team_id == 0.0f;
	state->overlay_loaded = false;
	state->complete = false;
	if (state->teamless) {
		state->complete = true;
		return true;
	}

	loader = (struct yt_team_loader_state){
		.team_id = state->raw_team_id,
		.current_player_record = state->current_player_record,
		.sector_record_offset = state->sector_record_offset,
		.conversion_mode = state->conversion_mode,
		.cache = state->cache,
	};
	loaded = yt_team_loader_run(&loader, read_record, context, error);
	state->physical_record = loader.physical_record;
	state->loader_route = loader.route;
	state->overlay_loaded = loader.overlay_loaded;
	if (!loaded)
		return false;
	for (index = 0U; index < YT_RADIO_SEND_RECIPIENTS; ++index)
		state->recipients[index] = state->cache->roster[index];
	state->recipient_count = YT_RADIO_SEND_RECIPIENTS;
	state->complete = true;
	return true;
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
yt_fighter_shield_spill_run(
    struct yt_fighter_shield_spill_state *state,
    const struct yt_fighter_shield_spill_ops *ops, void *context,
    struct yt_error *error)
{
	uint8_t fighter_row[128];
	uint8_t shield_row[128];
	size_t fighter_length;
	size_t shield_length;

	if (state == NULL || ops == NULL || ops->random == NULL
	    || ops->present == NULL)
		return false;
	state->iterations = 0U;
	state->fighter_row_presented = false;
	state->shield_row_presented = false;
	state->complete = false;
	while (state->fighters > 0.0 && state->shields > 0.0f) {
		float sampled;

		if (!ops->random(context, &sampled, error)
		    || !yt_fighter_shield_spill_step(&state->fighters,
		    &state->shields, sampled))
			return false;
		if (ops->store != NULL)
			ops->store(context, sampled >= 0.5f
			    ? YT_FIGHTER_SHIELD_SPILL_STORE_FIGHTERS
			    : YT_FIGHTER_SHIELD_SPILL_STORE_SHIELDS,
			    state->fighters, state->shields);
		++state->iterations;
	}
	if (!yt_fighter_shield_spill_rows(state->fighters, state->shields,
	    fighter_row, sizeof(fighter_row), &fighter_length,
	    shield_row, sizeof(shield_row), &shield_length)
	    || !ops->present(context, fighter_row, fighter_length,
	    YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW, error))
		return false;
	state->fighter_row_presented = true;
	if (!ops->present(context, shield_row, shield_length,
	    YT_FIGHTER_SHIELD_SPILL_SHIELD_ROW, error))
		return false;
	state->shield_row_presented = true;
	state->complete = true;
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
yt_bribe_ordinary_forces(float owner, double defenders,
    double ship_fighters, float draw)
{
	return owner == -1.0f || (defenders > ship_fighters
	    && draw < 0.33000001311302185f);
}

bool
yt_bribe_mercenary_forces(double defenders, double ship_fighters,
    float first, float second, bool sticky)
{
	return first < 0.05000000074505806f
	    || (ship_fighters < defenders
	    && second > 0.8999999761581421f) || sticky;
}

double
yt_bribe_offer_threshold(double defenders, float draw)
{
	volatile double product = defenders * (double)draw;
	volatile double doubled = product * 2.0;
	volatile double threshold = doubled + defenders;

	return threshold;
}

bool
yt_bribe_offer_accepted(float offer, double credits, double threshold)
{
	return (double)offer <= credits && (double)offer >= threshold;
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

static bool
drop_mines_error(struct yt_error *error, enum yt_status status,
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
drop_mines_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
drop_mines_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

bool
yt_drop_mines_run(struct yt_drop_mines_state *state,
    const struct yt_drop_mines_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t no_mines[] = "You don't HAVE any!";
	static const uint8_t union_refusal[] =
	    "The Union doesnt like the home 7 sectors mined!";
	static const char prompt_suffix[] =
	    " mines. Drop how many? [0] -=>";
	static const char success_suffix[] = " is now mined!";
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	enum yt_sector_mine_admission admission;
	char number[64];
	uint8_t row[192];
	char response[4096] = {0};
	int number_length;
	size_t row_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->flush == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->present == NULL || ops->amount == NULL
	    || ops->suppress == NULL || ops->sound == NULL
	    || state->current_player_record < 1)
		return drop_mines_error(error, YT_INVALID, "drop-mines state");
	memset(&state->current, 0, sizeof(state->current));
	memset(&state->sector, 0, sizeof(state->sector));
	state->current_sector = 0;
	state->carried = 0.0f;
	state->amount = 0.0f;
	memset(state->amount_raw, 0, sizeof(state->amount_raw));
	state->player_mines_after = 0.0f;
	state->sector_mines_before = 0.0f;
	state->sector_mines_after = 0.0f;
	state->player_read = false;
	state->negative_repair = false;
	state->repair_written = false;
	state->repair_flushed = false;
	state->amount_stored = false;
	state->suppression_set = false;
	state->player_written = false;
	state->player_flushed = false;
	state->sector_read = false;
	state->sector_written = false;
	state->sector_flushed = false;
	state->route = YT_DROP_MINES_INCOMPLETE;
	state->complete = false;

	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_read = true;
	state->carried = state->current.mines;
	state->current_sector = (int)state->current.sector;
	if (state->carried < 0.0f) {
		state->negative_repair = true;
		state->current.mines = 0.0f;
		if (!yt_record_set_number(&state->current.record, YT_F129, 0.0f)
		    || !ops->write_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->repair_written = true;
		if (!ops->flush(context, error))
			return false;
		state->repair_flushed = true;
	}
	if (state->carried < 1.0f) {
		state->route = YT_DROP_MINES_NO_MINES;
		if (!ops->present(context, no_mines, sizeof(no_mines) - 1U,
		    YT_DROP_MINES_NO_MINES_ROW, error))
			return false;
		state->complete = true;
		return true;
	}
	if (state->current.sector < 8.0f) {
		state->route = YT_DROP_MINES_UNION_REFUSAL;
		if (!ops->present(context, union_refusal,
		    sizeof(union_refusal) - 1U, YT_DROP_MINES_UNION_ROW, error))
			return false;
		state->complete = true;
		return true;
	}

	number_length = qb_str_single(number, sizeof(number), state->carried);
	if (number_length < 0
	    || sizeof("You have") - 1U + (size_t)number_length
	    + sizeof(prompt_suffix) - 1U > sizeof(row))
		return drop_mines_error(error, YT_RANGE, "drop-mines prompt");
	memcpy(row, "You have", sizeof("You have") - 1U);
	row_length = sizeof("You have") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, prompt_suffix, sizeof(prompt_suffix) - 1U);
	row_length += sizeof(prompt_suffix) - 1U;
	if (!ops->present(context, NULL, 0U, YT_DROP_MINES_PROMPT_BLANK,
	    error)
	    || !ops->present(context, row, row_length, YT_DROP_MINES_PROMPT,
	    error)
	    || !ops->amount(context, response, sizeof(response), error))
		return false;
	if (response[0] == '\0') {
		state->amount = 0.0f;
	}
	else {
		parsed = qb_val(response);
		if (!parsed.valid || parsed.overflow)
			return drop_mines_error(error, YT_RANGE, "drop-mines:VAL");
		state->amount = (float)parsed.value;
	}
	conversion = qb_mbf32_encode(state->amount, state->amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return drop_mines_error(error, YT_RANGE,
		    "drop-mines:amount-csng");
	state->amount = qb_mbf32_decode(state->amount_raw);
	state->amount_stored = true;
	admission = yt_sector_mine_admit(state->carried, state->amount);
	if (admission != YT_SECTOR_MINE_ACCEPTED) {
		state->route = YT_DROP_MINES_CANCELLED;
		state->complete = true;
		return true;
	}

	ops->suppress(context);
	state->suppression_set = true;
	state->player_mines_after = drop_mines_single_sub(state->carried,
	    state->amount);
	state->current.mines = state->player_mines_after;
	if (!yt_record_set_number(&state->current.record, YT_F129,
	    state->player_mines_after)
	    || !ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	if (!ops->flush(context, error))
		return false;
	state->player_flushed = true;
	if (!ops->read_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_read = true;
	state->sector_mines_before = state->sector.mines;
	state->sector_mines_after = drop_mines_single_add(
	    state->sector_mines_before, state->amount);
	state->sector.mines = state->sector_mines_after;
	if (!yt_record_set_number(&state->sector.record, YT_F129,
	    state->sector_mines_after)
	    || !ops->write_sector(context, state->current_sector,
	    &state->sector, error))
		return false;
	state->sector_written = true;
	if (!ops->flush(context, error))
		return false;
	state->sector_flushed = true;

	number_length = qb_str_single(number, sizeof(number),
	    state->current.sector);
	if (number_length < 0
	    || sizeof("Sector") - 1U + (size_t)number_length
	    + sizeof(success_suffix) - 1U > sizeof(row))
		return drop_mines_error(error, YT_RANGE, "drop-mines success row");
	memcpy(row, "Sector", sizeof("Sector") - 1U);
	row_length = sizeof("Sector") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, success_suffix, sizeof(success_suffix) - 1U);
	row_length += sizeof(success_suffix) - 1U;
	state->route = YT_DROP_MINES_ACCEPTED;
	if (!ops->present(context, NULL, 0U, YT_DROP_MINES_SUCCESS_BLANK,
	    error)
	    || !ops->present(context, row, row_length,
	    YT_DROP_MINES_SUCCESS_ROW, error)
	    || !ops->sound(context, 4.0f, error))
		return false;
	state->complete = true;
	return true;
}

bool
yt_no_turn_gate_denied(float turns)
{
	return turns <= 0.0f;
}

void
yt_no_turn_gate_result_raw(bool denied, uint8_t raw[4])
{
	static const uint8_t false_value[4] = {0x00, 0x00, 0x7d, 0x00};
	static const uint8_t true_value[4] = {0x00, 0x00, 0x00, 0x81};

	if (raw != NULL)
		memcpy(raw, denied ? true_value : false_value, 4U);
}

bool
yt_action_finalizer_turn_raw(const uint8_t before[4], uint8_t after[4])
{
	volatile float updated;

	if (before == NULL || after == NULL)
		return false;
	updated = qb_mbf32_decode(before) - 1.0f;
	return qb_mbf32_encode(updated, after) != QB_MBF_OVERFLOW;
}

bool
yt_action_finalizer_cloak_raw(const uint8_t before[4],
    uint8_t arithmetic[4], uint8_t result[4], bool *clamped)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0xa3, 0x00};
	volatile float updated;

	if (before == NULL || arithmetic == NULL || result == NULL
	    || clamped == NULL)
		return false;
	updated = qb_mbf32_decode(before) - 0.009999999776482582f;
	if (qb_mbf32_encode(updated, arithmetic) == QB_MBF_OVERFLOW)
		return false;
	if (updated < 0.0f) {
		memcpy(result, dirty_zero, sizeof(dirty_zero));
		*clamped = true;
	}
	else {
		memcpy(result, arithmetic, 4U);
		*clamped = false;
	}
	return true;
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
yt_team_membership_apply_player(struct yt_player *player, float team)
{
	if (player == NULL)
		return;
	player->team = team;
	(void)yt_record_set_number(&player->record, YT_F89, team);
}

void
yt_team_banish_apply_player(struct yt_player *player)
{
	yt_team_membership_apply_player(player, 0.0f);
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

void
yt_team_inactive_overlay(struct yt_record *record)
{
	static const size_t offsets[5] = {
		YT_F77, YT_F109, YT_F117, YT_F121, YT_F125
	};
	static const uint8_t zero[4] = {0, 0, 0, 0};
	size_t index;

	if (record == NULL)
		return;
	for (index = 0; index < YT_ARRAY_LEN(offsets); ++index)
		(void)yt_record_set_raw_number(record, offsets[index], zero);
	memset(record->bytes + YT_F113, ' ', 4);
}

bool
yt_port_link_missing(float link)
{
	return link == 0.0f;
}

float
yt_port_selected_expression(float port_offset, float logical_link)
{
	volatile float expression = port_offset + logical_link;

	return expression;
}

bool
yt_computer_port_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer port maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_port_select(const char *response, float maximum,
    float *selected, enum yt_computer_port_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t raw[4];
	volatile float candidate;
	bool above;
	bool below;
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer port selection arguments");
	*selected = 0.0f;
	if (response[0] == '\0') {
		*route = YT_COMPUTER_PORT_SELECTION_EMPTY;
		return true;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port sector VAL");
	candidate = (float)qb_int(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port sector CSNG");
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	above = *selected > maximum;
	below = *selected < 1.0f;
	*route = (above | below) ? YT_COMPUTER_PORT_SELECTION_INVALID
	    : YT_COMPUTER_PORT_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_path_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_path_parse(const char *response, float *selected,
    uint8_t selected_raw[4], struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t integer_raw[8];
	enum qb_mbf_status status;

	if (response == NULL || selected == NULL || selected_raw == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path parse arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector VAL");
	status = qb_mbf64_floor_raw(parsed.mbf, integer_raw);
	if (status != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector INT");
	status = qb_mbf32_from_mbf64_raw(integer_raw, selected_raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path sector CSNG");
	if (status == QB_MBF_UNDERFLOW)
		memset(selected_raw, 0, 4U);
	*selected = qb_mbf32_decode(selected_raw);
	return true;
}

bool
yt_computer_path_append_hop(char *scratch, size_t capacity,
    size_t *length, float next_sector, float *hop_count,
    uint8_t hop_count_raw[4], struct yt_error *error)
{
	char number[64];
	int number_length;
	volatile float incremented;
	enum qb_mbf_status status;

	if (scratch == NULL || capacity == 0U || length == NULL
	    || *length >= capacity || scratch[*length] != '\0'
	    || hop_count == NULL || hop_count_raw == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer path scratch arguments");
	number_length = qb_str_single(number, sizeof(number), next_sector);
	if (number_length < 0 || (size_t)number_length + 3U
	    >= capacity - *length)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path scratch append");
	scratch[(*length)++] = '\r';
	scratch[(*length)++] = 'M';
	scratch[(*length)++] = '\r';
	memcpy(scratch + *length, number, (size_t)number_length);
	*length += (size_t)number_length;
	scratch[*length] = '\0';
	incremented = *hop_count + 1.0f;
	status = qb_mbf32_encode(incremented, hop_count_raw);
	if (status != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "computer path hop increment");
	*hop_count = qb_mbf32_decode(hop_count_raw);
	return true;
}

bool
yt_computer_path_wrap_required(int local_column)
{
	return local_column > 74;
}

static bool
computer_avoid_csng(const struct qb_val_result *parsed, float *selected,
    struct yt_error *error, const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status;

	status = qb_mbf32_from_mbf64_raw(parsed->mbf, raw);
	if (status == QB_MBF_OVERFLOW || status == QB_MBF_DOMAIN)
		return startup_configuration_error(error, YT_RANGE, operation);
	*selected = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_avoid_maximum(float port_offset, float sector_offset,
    float *maximum, struct yt_error *error)
{
	uint8_t raw[4];
	volatile float difference = port_offset - sector_offset;
	enum qb_mbf_status status;

	if (maximum == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid maximum arguments");
	status = qb_mbf32_encode(difference, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid maximum subtraction");
	*maximum = status == QB_MBF_UNDERFLOW ? 0.0f
	    : qb_mbf32_decode(raw);
	return true;
}

bool
yt_computer_avoid_select_slot(const char *response, uint8_t conversion_mode,
    float *selected, int *index,
    enum yt_computer_avoid_selection_route *route, struct yt_error *error)
{
	struct qb_val_result parsed;
	bool overflow;

	if (response == NULL || selected == NULL || index == NULL
	    || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid slot arguments");
	*selected = 0.0f;
	*index = 0;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid slot VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid slot CSNG"))
		return false;
	if (*selected < 1.0f || *selected > 30.0f)
		return true;
	*index = (int)qb_cint_mode((double)*selected, conversion_mode,
	    &overflow);
	if (overflow || *index < 1 || *index > 30)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid slot CINT");
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

bool
yt_computer_avoid_select_sector(const char *response, float maximum,
    float *selected, enum yt_computer_avoid_selection_route *route,
    struct yt_error *error)
{
	struct qb_val_result parsed;

	if (response == NULL || selected == NULL || route == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "avoid sector arguments");
	*selected = 0.0f;
	*route = YT_COMPUTER_AVOID_SELECTION_INVALID;
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "avoid sector VAL");
	if (!computer_avoid_csng(&parsed, selected, error,
	    "avoid sector CSNG"))
		return false;
	if (*selected < 0.0f || *selected > maximum)
		return true;
	*route = YT_COMPUTER_AVOID_SELECTION_ACCEPTED;
	return true;
}

void
yt_computer_avoid_transition(float old_value, float new_value,
    bool *locked, bool *available)
{
	if (locked != NULL)
		*locked = new_value != 0.0f;
	if (available != NULL)
		*available = old_value != 0.0f && old_value != new_value;
}

static bool
computer_port_record(float value, uint32_t *record, const char *operation,
    struct yt_error *error)
{
	if (!isfinite(value) || value < 1.0f || floorf(value) != value
	    || value > (float)UINT32_MAX)
		return startup_configuration_error(error, YT_RANGE, operation);
	*record = (uint32_t)value;
	return true;
}

bool
yt_computer_port_visibility_run(
    struct yt_computer_port_visibility_state *state,
    yt_computer_port_read_player_fn read_player, void *context,
    struct yt_error *error)
{
	static const uint8_t marker_false[4] = {0x00, 0x00, 0x03, 0x00};
	static const uint8_t relation_false[4] = {0x00, 0x00, 0x80, 0x00};
	static const uint8_t relation_true[4] = {0x00, 0x00, 0x80, 0x81};
	struct yt_player player;
	float current_team = 0.0f;
	uint32_t record;
	uint8_t scratch_raw[4];
	volatile float scratch;
	bool candidate_below;
	bool candidate_above;
	bool current_below;
	bool current_above;
	bool no_port;
	bool fighters_positive;
	bool team_positive;
	bool not_relation;
	bool team_zero;
	bool owner_not_self;

	if (state == NULL || read_player == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "computer port visibility arguments");
	memcpy(state->marker_4d62_raw, marker_false,
	    sizeof(state->marker_4d62_raw));
	memcpy(state->relation_raw, relation_false,
	    sizeof(state->relation_raw));
	state->marker_4d62 = qb_mbf32_decode(state->marker_4d62_raw);
	state->relation = qb_mbf32_decode(state->relation_raw);
	state->scratch_19c4 = 0.0f;
	memset(state->scratch_19c4_raw, 0,
	    sizeof(state->scratch_19c4_raw));
	state->player_read_attempts = 0U;
	state->scratch_written = false;
	state->unavailable = false;
	state->complete = false;

	/* The shipped predicate evaluates all four comparisons eagerly. */
	candidate_below = state->fighter_owner < 2.0f;
	candidate_above = state->fighter_owner > state->last_player_record;
	current_below = state->current_player_record < 2.0f;
	current_above = state->current_player_record > state->last_player_record;
	if (!(candidate_below | candidate_above | current_below
	    | current_above)) {
		if (state->fighter_owner == state->current_player_record) {
			memcpy(state->relation_raw, relation_true,
			    sizeof(state->relation_raw));
			state->relation = qb_mbf32_decode(state->relation_raw);
		}
		else {
			if (!computer_port_record(state->current_player_record,
			    &record, "computer port current record coercion",
			    error))
				return false;
			++state->player_read_attempts;
			if (!read_player(context, record, &player, error))
				return false;
			state->field_kind = YT_COMPUTER_PORT_FIELD_PLAYER;
			state->field_record = record;
			state->field = player.record;
			state->field_valid = true;
			current_team = player.team;
			if (current_team != 0.0f) {
				if (!computer_port_record(state->fighter_owner,
				    &record,
				    "computer port candidate record coercion",
				    error))
					return false;
				++state->player_read_attempts;
				if (!read_player(context, record, &player, error))
					return false;
				state->field_kind = YT_COMPUTER_PORT_FIELD_PLAYER;
				state->field_record = record;
				state->field = player.record;
				state->field_valid = true;
				if (player.team == current_team) {
					memcpy(state->relation_raw, relation_true,
					    sizeof(state->relation_raw));
					state->relation = qb_mbf32_decode(
					    state->relation_raw);
				}
			}
		}
	}

	scratch = state->planet_record_offset + state->inherited_index;
	if (qb_mbf32_encode(scratch, scratch_raw) == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "computer port scratch addition");
	state->scratch_19c4 = qb_mbf32_decode(scratch_raw);
	memcpy(state->scratch_19c4_raw, scratch_raw,
	    sizeof(state->scratch_19c4_raw));
	state->scratch_written = true;

	/* Preserve all six eager source comparisons before combining them. */
	no_port = state->port_link == 0.0f;
	fighters_positive = state->fighter_count > 0.0f;
	team_positive = state->cached_current_team > 0.0f;
	not_relation = (~(int16_t)state->relation) != 0;
	team_zero = state->cached_current_team == 0.0f;
	owner_not_self = state->current_player_record != state->fighter_owner;
	state->unavailable = no_port
	    | (fighters_positive & team_positive & not_relation)
	    | (fighters_positive & team_zero & owner_not_self);
	state->complete = true;
	return true;
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

bool
yt_port_rename_run(struct yt_port_rename_state *state,
    const struct yt_port_rename_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t not_owner[] = "This isn't your port!";
	static const uint8_t earth[] = "Can't rename Earth!";
	bool conversion_overflow;
	int32_t converted_length;

	if (state == NULL || ops == NULL || ops->hydrate == NULL
	    || ops->read_sector == NULL || ops->read_port == NULL
	    || ops->present == NULL || ops->edit == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	memset(&state->sector, 0, sizeof(state->sector));
	memset(&state->port, 0, sizeof(state->port));
	state->hydration_record = (int)qb_brun_random_record_number(
	    state->current_player_record);
	state->logical_port = 0;
	state->relative_port = 0.0f;
	state->cached_name_length = 0U;
	state->player_hydrated = false;
	state->sector_read = false;
	state->port_read = false;
	state->editor_called = false;
	state->complete = false;
	state->route = YT_PORT_RENAME_INCOMPLETE;

	if (!ops->hydrate(context, state->hydration_record, &state->player,
	    error))
		return false;
	state->player_hydrated = true;
	if (!ops->read_sector(context, (int)state->player.sector,
	    &state->sector, error))
		return false;
	state->sector_read = true;
	if (!qb_mbf32_truth(state->sector.record.bytes + YT_F65)) {
		if (!ops->present(context, no_port, sizeof(no_port) - 1U,
		    YT_PORT_RENAME_NO_PORT, error))
			return false;
		state->route = YT_PORT_RENAME_NO_PORT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!yt_port_rename_record(state->port_offset, state->sector.port,
	    &state->logical_port, &state->relative_port))
		return startup_configuration_error(error, YT_RANGE,
		    "rename port record conversion");
	if (!ops->read_port(context, state->logical_port, &state->port, error))
		return false;
	state->port_read = true;
	if (state->port.owner != state->current_player_record) {
		if (!ops->present(context, not_owner, sizeof(not_owner) - 1U,
		    YT_PORT_RENAME_NOT_OWNER, error))
			return false;
		state->route = YT_PORT_RENAME_NOT_OWNER_ROUTE;
		state->complete = true;
		return true;
	}
	if (state->relative_port == 1.0f) {
		if (!ops->present(context, earth, sizeof(earth) - 1U,
		    YT_PORT_RENAME_EARTH, error))
			return false;
		state->route = YT_PORT_RENAME_EARTH_ROUTE;
		state->complete = true;
		return true;
	}
	conversion_overflow = false;
	converted_length = qb_cint_mbf32(state->port.record.bytes + YT_F85,
	    state->conversion_mode, &conversion_overflow);
	if (conversion_overflow || converted_length < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "port name length");
	state->cached_name_length = (size_t)converted_length;
	if (state->cached_name_length > YT_TEXT_FIELD_SIZE)
		state->cached_name_length = YT_TEXT_FIELD_SIZE;
	memcpy(state->cached_name, state->port.record.bytes,
	    state->cached_name_length);
	state->editor_called = true;
	if (!ops->edit(context, state->logical_port, state->cached_name,
	    state->cached_name_length, &state->port, error))
		return false;
	state->route = YT_PORT_RENAME_EDITED_ROUTE;
	state->complete = true;
	return true;
}

bool
yt_port_rename_cycle_run(struct yt_port_rename_cycle_state *state,
    const struct yt_port_rename_cycle_ops *ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || ops == NULL || ops->rename == NULL
	    || ops->scanner == NULL)
		return false;
	state->rename_complete = false;
	state->scanner_complete = false;
	state->complete = false;
	if (!ops->rename(context, error))
		return false;
	state->rename_complete = true;
	if (!ops->scanner(context, error))
		return false;
	state->scanner_complete = true;
	state->complete = true;
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
yt_port_purchase_seller_overlay(struct yt_player *seller, float treasury,
    double price)
{
	volatile float ports;

	if (seller == NULL)
		return false;
	seller->credits = yt_port_purchase_seller_credit(treasury,
	    seller->credits, price);
	ports = seller->ports_owned - 1.0f;
	seller->ports_owned = ports;
	return yt_record_set_number(&seller->record, YT_F81, seller->credits)
	    && yt_record_set_number(&seller->record, YT_F117,
	    seller->ports_owned);
}

bool
yt_port_purchase_title_overlay(struct yt_port *port, int buyer_record)
{
	if (port == NULL)
		return false;
	port->owner = (float)buyer_record;
	port->treasury = 0.0f;
	return yt_record_set_number(&port->record, YT_F97, port->owner)
	    && yt_record_set_number(&port->record, YT_F89, 0.0f);
}

bool
yt_port_purchase_buyer_overlay(struct yt_player *buyer, double price)
{
	volatile float ports;

	if (buyer == NULL)
		return false;
	buyer->credits = yt_port_purchase_buyer_credit(buyer->credits, price);
	ports = buyer->ports_owned + 1.0f;
	buyer->ports_owned = ports;
	return yt_record_set_number(&buyer->record, YT_F81, buyer->credits)
	    && yt_record_set_number(&buyer->record, YT_F117,
	    buyer->ports_owned);
}

int
yt_port_purchase_seller_record(float owner)
{
	return (int)qb_brun_random_record_number(owner);
}

static bool
port_purchase_error(struct yt_error *error, enum yt_status status,
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

static bool
port_purchase_append(uint8_t *buffer, size_t capacity, size_t *length,
    const uint8_t *text, size_t text_length)
{
	if (length == NULL || text_length > capacity - *length
	    || (text_length != 0U && (buffer == NULL || text == NULL)))
		return false;
	if (text_length != 0U)
		memcpy(buffer + *length, text, text_length);
	*length += text_length;
	return true;
}

bool
yt_port_purchase_accept_run(struct yt_port_purchase_accept_state *state,
    const struct yt_port_purchase_accept_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t sold[] = "Sold!";
	static const uint8_t transfer_prefix[] = "Credits transferred to ";
	static const uint8_t transfer_suffix[] = "'s account!";
	static const uint8_t radio_one[] = " bought your port \"";
	static const uint8_t radio_two[] = "\" in";
	static const uint8_t radio_three[] = " for";
	static const uint8_t radio_four[] = " credits";
	static const uint8_t success_prefix[] = "Congratulations ";
	static const uint8_t success_suffix[] =
	    "! When others trade at your port their CREDITS";
	static const uint8_t success_tail[] =
	    "will go into the port treasury for you to take out later!";
	uint8_t row[512];
	uint8_t message[512];
	char sector_text[64];
	char price_text[64];
	size_t length;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || state->logical_port < 0 || ops->present == NULL
	    || ops->read_port == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->radio == NULL
	    || ops->rename == NULL || ops->write_port == NULL
	    || ops->hydrate_buyer == NULL
	    || (state->cached_trader_length != 0U
	    && state->cached_trader == NULL)
	    || (state->old_name_length != 0U && state->old_name == NULL)
	    || (state->owner_name_length != 0U && state->owner_name == NULL)
	    || (state->first_name_length != 0U && state->first_name == NULL))
		return false;
	memset(&state->port, 0, sizeof(state->port));
	memset(&state->seller, 0, sizeof(state->seller));
	memset(&state->buyer, 0, sizeof(state->buyer));
	state->seller_record = 0;
	state->sold_presented = false;
	state->port_read = false;
	state->seller_read = false;
	state->seller_written = false;
	state->radio_written = false;
	state->port_reloaded_after_radio = false;
	state->rename_called = false;
	state->title_port_read = false;
	state->title_written = false;
	state->buyer_hydrated = false;
	state->buyer_written = false;
	state->success_presented = false;
	state->complete = false;

	if (!ops->present(context, NULL, 0U,
	    YT_PORT_PURCHASE_ACCEPT_SOLD_BLANK, error)
	    || !ops->present(context, sold, sizeof(sold) - 1U,
	    YT_PORT_PURCHASE_ACCEPT_SOLD_ROW, error))
		return false;
	state->sold_presented = true;
	if (!ops->read_port(context, state->logical_port, &state->port, error))
		return false;
	state->port_read = true;
	if (state->old_owner != 0.0f) {
		length = 0U;
		if (!port_purchase_append(row, sizeof(row), &length,
		    transfer_prefix, sizeof(transfer_prefix) - 1U)
		    || !port_purchase_append(row, sizeof(row), &length,
		    state->owner_name, state->owner_name_length)
		    || !port_purchase_append(row, sizeof(row), &length,
		    transfer_suffix, sizeof(transfer_suffix) - 1U)
		    || !ops->present(context, NULL, 0U,
		    YT_PORT_PURCHASE_ACCEPT_TRANSFER_BLANK, error)
		    || !ops->present(context, row, length,
		    YT_PORT_PURCHASE_ACCEPT_TRANSFER_ROW, error))
			return false;
		state->seller_record =
		    yt_port_purchase_seller_record(state->old_owner);
		if (!ops->read_player(context, state->seller_record,
		    &state->seller, error))
			return false;
		state->seller_read = true;
		if (!yt_port_purchase_seller_overlay(&state->seller,
		    state->port.treasury, state->price)
		    || !ops->write_player(context, state->seller_record,
		    &state->seller, error))
			return false;
		state->seller_written = true;
		if (qb_str_single(sector_text, sizeof(sector_text),
		    state->cached_buyer_sector) < 0
		    || qb_str_double(price_text, sizeof(price_text),
		    state->price) < 0)
			return false;
		length = 0U;
		if (!port_purchase_append(message, sizeof(message), &length,
		    state->cached_trader, state->cached_trader_length)
		    || !port_purchase_append(message, sizeof(message), &length,
		    radio_one, sizeof(radio_one) - 1U)
		    || !port_purchase_append(message, sizeof(message), &length,
		    state->old_name, state->old_name_length)
		    || !port_purchase_append(message, sizeof(message), &length,
		    radio_two, sizeof(radio_two) - 1U)
		    || !port_purchase_append(message, sizeof(message), &length,
		    (const uint8_t *)sector_text, strlen(sector_text))
		    || !port_purchase_append(message, sizeof(message), &length,
		    radio_three, sizeof(radio_three) - 1U)
		    || !port_purchase_append(message, sizeof(message), &length,
		    (const uint8_t *)price_text, strlen(price_text))
		    || !port_purchase_append(message, sizeof(message), &length,
		    radio_four, sizeof(radio_four) - 1U)
		    || !ops->radio(context, message, length, -2.0f,
		    state->old_owner, error))
			return false;
		state->radio_written = true;
		if (!ops->read_port(context, state->logical_port, &state->port,
		    error))
			return false;
		state->port_reloaded_after_radio = true;
	}
	if (state->relative_port > 1.0f) {
		state->rename_called = true;
		if (!ops->rename(context, state->logical_port, state->old_name,
		    state->old_name_length, &state->port, error))
			return false;
	}
	if (!ops->read_port(context, state->logical_port, &state->port, error))
		return false;
	state->title_port_read = true;
	if (!yt_port_purchase_title_overlay(&state->port,
	    state->current_player_record)
	    || !ops->write_port(context, state->logical_port, &state->port,
	    error))
		return false;
	state->title_written = true;
	if (!ops->hydrate_buyer(context, state->current_player_record,
	    &state->buyer, error))
		return false;
	state->buyer_hydrated = true;
	if (!yt_port_purchase_buyer_overlay(&state->buyer, state->price)
	    || !ops->write_player(context, state->current_player_record,
	    &state->buyer, error))
		return false;
	state->buyer_written = true;
	length = 0U;
	if (!port_purchase_append(row, sizeof(row), &length, success_prefix,
	    sizeof(success_prefix) - 1U)
	    || !port_purchase_append(row, sizeof(row), &length,
	    state->first_name, state->first_name_length)
	    || !port_purchase_append(row, sizeof(row), &length, success_suffix,
	    sizeof(success_suffix) - 1U)
	    || !ops->present(context, row, length,
	    YT_PORT_PURCHASE_ACCEPT_SUCCESS_FIRST, error)
	    || !ops->present(context, success_tail, sizeof(success_tail) - 1U,
	    YT_PORT_PURCHASE_ACCEPT_SUCCESS_TAIL, error))
		return false;
	state->success_presented = true;
	state->complete = true;
	return true;
}

bool
yt_port_purchase_run(struct yt_port_purchase_state *state,
    const struct yt_port_purchase_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t already_prefix[] = "You already OWN this port ";
	static const uint8_t unaffordable[] =
	    "Come back when you can afford it!";
	static const uint8_t offer_prefix[] = "You may buy it from ";
	static const uint8_t offer_suffix[] = " if you wish.";
	static const uint8_t prompt[] = "Do you wish to buy it? [y/N]";
	static const uint8_t declined[] = "What a shame.. it's a nice port!";
	static const uint8_t earth_name[] = "Earth";
	uint8_t row[512];
	char price_text[64];
	char credits_text[64];
	size_t length;
	int32_t converted_length;
	bool conversion_overflow;
	bool accepted;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || ops->hydrate_buyer == NULL || ops->read_sector == NULL
	    || ops->report == NULL || ops->owner == NULL
	    || ops->present == NULL || ops->confirm == NULL
	    || ops->accept == NULL
	    || (state->first_name_length != 0U && state->first_name == NULL))
		return false;
	memset(&state->buyer_entry, 0, sizeof(state->buyer_entry));
	memset(&state->sector, 0, sizeof(state->sector));
	memset(&state->early_port, 0, sizeof(state->early_port));
	memset(&state->terminal_port, 0, sizeof(state->terminal_port));
	memset(&state->accepted, 0, sizeof(state->accepted));
	state->logical_port = 0;
	state->relative_port = 0.0f;
	state->earth = false;
	state->cached_buyer_credits = 0.0f;
	state->cached_buyer_sector = 0.0f;
	state->cached_trader_length = 0U;
	state->old_owner = 0.0f;
	memset(state->purchase_production, 0,
	    sizeof(state->purchase_production));
	state->price = 0.0;
	state->old_name_length = 0U;
	state->owner_name_length = 0U;
	state->buyer_hydrated = false;
	state->sector_read = false;
	state->report_complete = false;
	state->owner_displayed = false;
	state->confirmation_read = false;
	state->accepted_called = false;
	state->complete = false;
	state->route = YT_PORT_PURCHASE_INCOMPLETE;

	if (!ops->hydrate_buyer(context, state->current_player_record,
	    &state->buyer_entry, error))
		return false;
	state->buyer_hydrated = true;
	state->cached_buyer_credits = state->buyer_entry.credits;
	state->cached_buyer_sector = state->buyer_entry.sector;
	if (!yt_player_stored_name(&state->buyer_entry, state->cached_trader,
	    &state->cached_trader_length, error)
	    || !ops->read_sector(context, (int)state->cached_buyer_sector,
	    &state->sector, error))
		return false;
	state->sector_read = true;
	if (!qb_mbf32_truth(state->sector.record.bytes + YT_F65)) {
		if (!ops->present(context, no_port, sizeof(no_port) - 1U,
		    YT_PORT_PURCHASE_NO_PORT, error))
			return false;
		state->route = YT_PORT_PURCHASE_NO_PORT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!yt_port_rename_record(state->port_offset, state->sector.port,
	    &state->logical_port, &state->relative_port))
		return port_purchase_error(error, YT_RANGE,
		    "buy port record conversion");
	state->earth = state->sector.port == 1.0f;
	if (!ops->report(context, state->logical_port, state->earth,
	    &state->early_port, &state->terminal_port,
	    state->purchase_production, error))
		return false;
	state->report_complete = true;
	state->old_owner = state->early_port.owner;
	if (state->earth) {
		state->price = 1000000000.0;
		memcpy(state->old_name, earth_name, sizeof(earth_name) - 1U);
		state->old_name_length = sizeof(earth_name) - 1U;
	}
	else {
		conversion_overflow = false;
		converted_length = qb_cint_mbf32(
		    state->terminal_port.record.bytes + YT_F85,
		    state->conversion_mode, &conversion_overflow);
		if (conversion_overflow || converted_length < 0)
			return port_purchase_error(error, YT_RANGE,
			    "buy old port name length");
		state->old_name_length = (size_t)converted_length;
		if (state->old_name_length > YT_TEXT_FIELD_SIZE)
			state->old_name_length = YT_TEXT_FIELD_SIZE;
		memcpy(state->old_name, state->terminal_port.record.bytes,
		    state->old_name_length);
		state->price = yt_port_purchase_price(
		    state->purchase_production);
	}
	if (state->old_owner == (float)state->current_player_record) {
		length = 0U;
		if (!port_purchase_append(row, sizeof(row), &length,
		    already_prefix, sizeof(already_prefix) - 1U)
		    || !port_purchase_append(row, sizeof(row), &length,
		    state->first_name, state->first_name_length)
		    || !port_purchase_append(row, sizeof(row), &length,
		    (const uint8_t *)"!", 1U)
		    || !ops->present(context, row, length,
		    YT_PORT_PURCHASE_ALREADY_OWNER, error))
			return false;
		state->route = YT_PORT_PURCHASE_ALREADY_OWNER_ROUTE;
		state->complete = true;
		return true;
	}
	if (qb_str_double(price_text, sizeof(price_text), state->price) < 0
	    || qb_str_double(credits_text, sizeof(credits_text),
	    (double)state->cached_buyer_credits) < 0)
		return port_purchase_error(error, YT_RANGE,
		    "buy price formatting");
	length = (size_t)snprintf((char *)row, sizeof(row),
	    "This port is for sale for%s credits. You have%s credits.",
	    price_text, credits_text);
	if (length >= sizeof(row)
	    || !ops->present(context, row, length, YT_PORT_PURCHASE_PRICE,
	    error))
		return false;
	if ((double)state->cached_buyer_credits < state->price) {
		if (!ops->present(context, unaffordable,
		    sizeof(unaffordable) - 1U, YT_PORT_PURCHASE_UNAFFORDABLE,
		    error))
			return false;
		state->route = YT_PORT_PURCHASE_UNAFFORDABLE_ROUTE;
		state->complete = true;
		return true;
	}
	if (state->old_owner != 0.0f) {
		struct yt_port display_port = state->terminal_port;

		display_port.owner = state->old_owner;
		if (!ops->owner(context, &display_port, state->owner_name,
		    sizeof(state->owner_name), &state->owner_name_length, error))
			return false;
		state->owner_displayed = true;
		length = 0U;
		if (!ops->present(context, NULL, 0U,
		    YT_PORT_PURCHASE_OFFER_LEADING_BLANK, error)
		    || !port_purchase_append(row, sizeof(row), &length,
		    offer_prefix, sizeof(offer_prefix) - 1U)
		    || !port_purchase_append(row, sizeof(row), &length,
		    state->owner_name, state->owner_name_length)
		    || !port_purchase_append(row, sizeof(row), &length,
		    offer_suffix, sizeof(offer_suffix) - 1U)
		    || !ops->present(context, row, length,
		    YT_PORT_PURCHASE_OFFER_ROW, error)
		    || !ops->present(context, NULL, 0U,
		    YT_PORT_PURCHASE_OFFER_TRAILING_BLANK, error))
			return false;
	}
	accepted = false;
	if (!ops->confirm(context, prompt, sizeof(prompt) - 1U, &accepted,
	    error))
		return false;
	state->confirmation_read = true;
	if (!accepted) {
		if (!ops->present(context, declined, sizeof(declined) - 1U,
		    YT_PORT_PURCHASE_DECLINED, error))
			return false;
		state->route = YT_PORT_PURCHASE_DECLINED_ROUTE;
		state->complete = true;
		return true;
	}
	state->accepted = (struct yt_port_purchase_accept_state){
		.current_player_record = state->current_player_record,
		.logical_port = state->logical_port,
		.relative_port = state->relative_port,
		.old_owner = state->old_owner,
		.price = state->price,
		.cached_buyer_sector = state->cached_buyer_sector,
		.cached_trader = state->cached_trader,
		.cached_trader_length = state->cached_trader_length,
		.old_name = state->old_name,
		.old_name_length = state->old_name_length,
		.owner_name = state->owner_name,
		.owner_name_length = state->owner_name_length,
		.first_name = state->first_name,
		.first_name_length = state->first_name_length,
	};
	state->accepted_called = true;
	if (!ops->accept(context, &state->accepted, error))
		return false;
	state->route = YT_PORT_PURCHASE_ACCEPTED_ROUTE;
	state->complete = true;
	return true;
}

bool
yt_port_purchase_cycle_run(struct yt_port_purchase_cycle_state *state,
    const struct yt_port_purchase_cycle_ops *ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || ops == NULL || ops->purchase == NULL
	    || ops->scanner == NULL)
		return false;
	state->purchase_complete = false;
	state->scanner_complete = false;
	state->complete = false;
	if (!ops->purchase(context, error))
		return false;
	state->purchase_complete = true;
	if (!ops->scanner(context, error))
		return false;
	state->scanner_complete = true;
	state->complete = true;
	return true;
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

static float
main_fighters_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static double
main_fighters_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

bool
yt_main_fighters_sector_overlay(struct yt_sector *sector,
    const uint8_t desired_raw[4], int player_record)
{
	uint8_t owner_raw[4];

	if (sector == NULL || desired_raw == NULL
	    || qb_mbf32_encode((float)player_record, owner_raw)
	    == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&sector->record, YT_F81, desired_raw)
	    || !yt_record_set_raw_number(&sector->record, YT_F85, owner_raw))
		return false;
	sector->fighters = qb_mbf32_decode(desired_raw);
	sector->fighter_owner = qb_mbf32_decode(owner_raw);
	return true;
}

bool
yt_main_fighters_player_overlay(struct yt_player *player, float remaining)
{
	uint8_t remaining_raw[4];

	if (player == NULL
	    || qb_mbf32_encode(remaining, remaining_raw) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F61,
	    remaining_raw))
		return false;
	player->fighters = qb_mbf32_decode(remaining_raw);
	return true;
}

bool
yt_main_fighters_run(struct yt_main_fighters_state *state,
    const struct yt_main_fighters_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Drop/Take Fighters>";
	static const uint8_t union_refusal[] =
	    "You can't leave fighters in the Union (sectors 1-7)";
	static const uint8_t foreign_refusal[] =
	    "There are already fighters in this sector!";
	static const uint8_t prompt[] =
	    "Defend this sector with how many? ";
	static const uint8_t insufficient[] = "You don't have that many!";
	struct qb_val_result parsed;
	uint32_t logical_sector;
	char response[160];
	char number[64];
	char row[160];
	double desired_integer;
	int amount;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || ops->hydrate == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->input == NULL || ops->sound == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	memset(&state->first_sector, 0, sizeof(state->first_sector));
	memset(&state->accepted_sector, 0, sizeof(state->accepted_sector));
	memset(&state->accepted_player, 0, sizeof(state->accepted_player));
	state->logical_sector = 0;
	state->available = 0.0;
	state->desired = 0.0f;
	memset(state->desired_raw, 0, sizeof(state->desired_raw));
	state->delta = 0.0f;
	state->remaining = 0.0f;
	state->player_hydrated = false;
	state->first_sector_read = false;
	state->input_read = false;
	state->desired_stored = false;
	state->accepted_sector_read = false;
	state->sector_written = false;
	state->accepted_player_read = false;
	state->player_written = false;
	state->sound_called = false;
	state->complete = false;
	state->route = YT_MAIN_FIGHTERS_INCOMPLETE;

	if (!ops->present(context, title, sizeof(title) - 1U,
	    YT_MAIN_FIGHTERS_TITLE, error)
	    || !ops->hydrate(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->player_hydrated = true;
	if (state->player.sector < 8.0f) {
		if (!ops->present(context, union_refusal,
		    sizeof(union_refusal) - 1U, YT_MAIN_FIGHTERS_UNION_REFUSAL,
		    error))
			return false;
		state->route = YT_MAIN_FIGHTERS_UNION_ROUTE;
		state->complete = true;
		return true;
	}
	logical_sector = qb_brun_random_record_number(state->player.sector);
	if (logical_sector > INT_MAX)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter sector record conversion");
	state->logical_sector = (int)logical_sector;
	if (!ops->read_sector(context, state->logical_sector,
	    &state->first_sector, error))
		return false;
	state->first_sector_read = true;
	if (state->first_sector.fighters > 0.0f
	    && state->first_sector.fighter_owner
	    != (float)state->current_player_record) {
		if (!ops->present(context, foreign_refusal,
		    sizeof(foreign_refusal) - 1U,
		    YT_MAIN_FIGHTERS_FOREIGN_REFUSAL, error))
			return false;
		state->route = YT_MAIN_FIGHTERS_FOREIGN_ROUTE;
		state->complete = true;
		return true;
	}
	state->available = main_fighters_double_add(
	    (double)state->first_sector.fighters,
	    (double)state->player.fighters);
	amount = qb_str_double(number, sizeof(number), state->available);
	if (amount < 0 || snprintf(row, sizeof(row),
	    "You have%s fighters available.", number) < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter available row composition");
	if (!ops->present(context, (const uint8_t *)row, strlen(row),
	    YT_MAIN_FIGHTERS_AVAILABLE, error)
	    || !ops->present(context, prompt, sizeof(prompt) - 1U,
	    YT_MAIN_FIGHTERS_PROMPT, error))
		return false;
	memset(response, 0, sizeof(response));
	if (!ops->input(context, response, sizeof(response), error))
		return false;
	state->input_read = true;
	if (memchr(response, '\0', sizeof(response)) == NULL)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter desired-count response");
	if (response[0] == '\0') {
		state->route = YT_MAIN_FIGHTERS_CANCELLED_ROUTE;
		state->complete = true;
		return true;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter desired-count VAL");
	desired_integer = floor(parsed.valid ? parsed.value : 0.0);
	state->desired = (float)desired_integer;
	if (qb_mbf32_encode(state->desired, state->desired_raw)
	    == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter desired-count CSNG");
	state->desired = qb_mbf32_decode(state->desired_raw);
	state->desired_stored = true;
	if (state->desired < 0.0f) {
		state->route = YT_MAIN_FIGHTERS_CANCELLED_ROUTE;
		state->complete = true;
		return true;
	}
	state->delta = main_fighters_single_sub(
	    state->first_sector.fighters, state->desired);
	state->remaining = (float)main_fighters_double_add(
	    (double)state->player.fighters, (double)state->delta);
	if (state->remaining < 0.0f) {
		if (!ops->present(context, insufficient,
		    sizeof(insufficient) - 1U, YT_MAIN_FIGHTERS_INSUFFICIENT,
		    error))
			return false;
		state->route = YT_MAIN_FIGHTERS_INSUFFICIENT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!ops->read_sector(context, state->logical_sector,
	    &state->accepted_sector, error))
		return false;
	state->accepted_sector_read = true;
	if (!yt_main_fighters_sector_overlay(&state->accepted_sector,
	    state->desired_raw, state->current_player_record))
		return startup_configuration_error(error, YT_RANGE,
		    "fighter sector overlay");
	if (!ops->write_sector(context, state->logical_sector,
	    &state->accepted_sector, error))
		return false;
	state->sector_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &state->accepted_player, error))
		return false;
	state->accepted_player_read = true;
	if (!yt_main_fighters_player_overlay(&state->accepted_player,
	    state->remaining))
		return startup_configuration_error(error, YT_RANGE,
		    "fighter player overlay");
	if (!ops->write_player(context, state->current_player_record,
	    &state->accepted_player, error))
		return false;
	state->player_written = true;
	amount = qb_str_single(number, sizeof(number), state->remaining);
	if (amount < 0 || snprintf(row, sizeof(row),
	    "Done.  You have%s fighters left.", number) < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "fighter success row composition");
	if (!ops->present(context, (const uint8_t *)row, strlen(row),
	    YT_MAIN_FIGHTERS_SUCCESS, error))
		return false;
	if (!ops->sound(context, 4.0f, error))
		return false;
	state->sound_called = true;
	state->route = YT_MAIN_FIGHTERS_ACCEPTED_ROUTE;
	state->complete = true;
	return true;
}

bool
yt_genesis_handoff_run(struct yt_genesis_handoff_state *state,
    const struct yt_genesis_handoff_ops *ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || ops == NULL || ops->close_file5 == NULL
	    || ops->open_output == NULL || ops->print_command == NULL
	    || ops->close_all == NULL || ops->run == NULL)
		return false;
	memset(state, 0, sizeof(*state));
	if (!ops->close_file5(context, error)) {
		state->failed_operation = YT_GENESIS_HANDOFF_CLOSE_FILE5;
		return false;
	}
	state->file5_closed = true;
	if (!ops->open_output(context, error)) {
		state->failed_operation = YT_GENESIS_HANDOFF_OPEN_OUTPUT;
		return false;
	}
	state->output_opened = true;
	if (!ops->print_command(context, error)) {
		state->failed_operation = YT_GENESIS_HANDOFF_PRINT_COMMAND;
		return false;
	}
	state->command_printed = true;
	if (!ops->close_all(context, error)) {
		state->failed_operation = YT_GENESIS_HANDOFF_CLOSE_ALL;
		return false;
	}
	state->close_all_completed = true;
	state->run_invoked = true;
	if (!ops->run(context, error)) {
		state->failed_operation = YT_GENESIS_HANDOFF_RUN;
		return false;
	}
	state->complete = true;
	return true;
}

bool
yt_genesis_run(struct yt_genesis_state *state,
    const struct yt_genesis_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prophecy_first[] =
	    "It has been written that one day a Trader Baron will rise up";
	static const uint8_t prophecy_second[] =
	    "and wipe the universe clean of the evil that infests it.";
	static const uint8_t disabled[] = "*FUNCTION DISABLED*";
	static const uint8_t declined[] =
	    "Alas, today is not the day that the prophesy will be fullfilled.";
	static const uint8_t success_first[] =
	    "...and so it was written, that one day a trader baron would emerge who";
	static const uint8_t success_second[] =
	    "would wipe away the all of the evil in the universe.....";
	uint8_t prompt[512];
	uint8_t first[256];
	uint8_t second[256];
	size_t prompt_length;
	size_t first_length;
	size_t second_length;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || (state->cached_trader_length != 0U
	    && state->cached_trader == NULL)
	    || ops->hydrate == NULL || ops->present == NULL
	    || ops->confirm == NULL || ops->handoff == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	state->answer = false;
	state->player_hydrated = false;
	state->confirmation_read = false;
	state->disabled_presented = false;
	state->handoff_called = false;
	state->complete = false;
	state->route = YT_GENESIS_INCOMPLETE;

	if (!ops->hydrate(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->player_hydrated = true;
	if (!ops->present(context, prophecy_first, sizeof(prophecy_first) - 1U,
	    YT_GENESIS_PROPHECY_FIRST, error)
	    || !ops->present(context, prophecy_second,
	    sizeof(prophecy_second) - 1U, YT_GENESIS_PROPHECY_SECOND, error)
	    || !ops->present(context, NULL, 0U, YT_GENESIS_PROMPT_BLANK,
	    error))
		return false;
	if (!yt_genesis_confirmation_prompt(state->cached_trader,
	    state->cached_trader_length, prompt, sizeof(prompt),
	    &prompt_length))
		return startup_configuration_error(error, YT_RANGE,
		    "Genesis confirmation prompt");
	if (!ops->confirm(context, prompt, prompt_length, &state->answer,
	    error))
		return false;
	state->confirmation_read = true;
	if (state->required_ports > 300.0f) {
		if (!ops->present(context, disabled, sizeof(disabled) - 1U,
		    YT_GENESIS_DISABLED, error))
			return false;
		state->disabled_presented = true;
		state->answer = false;
	}
	if (!state->answer) {
		if (!ops->present(context, declined, sizeof(declined) - 1U,
		    YT_GENESIS_DECLINED, error))
			return false;
		state->route = state->disabled_presented
		    ? YT_GENESIS_DISABLED_ROUTE : YT_GENESIS_DECLINED_ROUTE;
		state->complete = true;
		return true;
	}
	if (state->player.ports_owned < state->required_ports) {
		if (!yt_genesis_insufficient_rows(state->required_ports,
		    state->player.ports_owned, first, sizeof(first), &first_length,
		    second, sizeof(second), &second_length))
			return startup_configuration_error(error, YT_RANGE,
			    "Genesis insufficient row composition");
		if (!ops->present(context, first, first_length,
		    YT_GENESIS_INSUFFICIENT_FIRST, error)
		    || !ops->present(context, second, second_length,
		    YT_GENESIS_INSUFFICIENT_SECOND, error))
			return false;
		state->route = YT_GENESIS_INSUFFICIENT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U, YT_GENESIS_SUCCESS_BLANK,
	    error)
	    || !ops->present(context, success_first, sizeof(success_first) - 1U,
	    YT_GENESIS_SUCCESS_FIRST, error)
	    || !ops->present(context, success_second,
	    sizeof(success_second) - 1U, YT_GENESIS_SUCCESS_SECOND, error))
		return false;
	state->handoff_called = true;
	if (!ops->handoff(context, error))
		return false;
	state->route = YT_GENESIS_HANDOFF_ROUTE;
	state->complete = true;
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

static void
sector_mine_apply_style(struct yt_sector_mine_state *state,
    const struct yt_sector_mine_ops *ops, void *context)
{
	if (ops->style != NULL)
		ops->style(context, state->foreground, state->background,
		    state->blink, state->pager_foreground);
}

static bool
sector_mine_stock_loss(const struct yt_sector_mine_ops *ops, void *context,
    float batch, float *stock, float *loss, struct yt_error *error)
{
	float sampled;

	if (!ops->shrink(context, projectile_single_mul(batch, *stock),
	    &sampled, error))
		return false;
	if (sampled > *stock)
		sampled = *stock;
	*stock = projectile_single_sub(*stock, sampled);
	*loss = sampled;
	return true;
}

bool
yt_sector_mine_run(struct yt_sector_mine_state *state,
    const struct yt_sector_mine_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t warning[] = "** Sector is Mined!! **";
	static const uint8_t shields_destroyed[] = "Shields disintegrated!";
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";
	uint8_t row[300];
	size_t row_length;
	bool overflow;
	int32_t converted;
	int current;

	if (state == NULL || state->destroyed == NULL || ops == NULL
	    || ops->read_current == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->news == NULL || ops->random == NULL
	    || ops->shrink == NULL || ops->emergency_warp == NULL)
		return false;
	state->terminal = false;
	state->complete = false;
	state->batches = 0U;
	converted = qb_cint_mode((double)state->current_sector,
	    state->conversion_mode, &overflow);
	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "sector mine sector record CINT");
		}
		return false;
	}
	current = (int)converted;
	if (!ops->present(context, NULL, 0U, YT_SECTOR_MINE_OUTPUT_LINE,
	    error))
		return false;
	state->blink = 1.0f;
	sector_mine_apply_style(state, ops, context);
	if (!ops->present(context, warning, sizeof(warning) - 1U,
	    YT_SECTOR_MINE_OUTPUT_LINE, error)
	    || !ops->sound(context, 5.0f, error)
	    || !ops->read_current(context, &state->player, error))
		return false;
	converted = qb_cint_mode((double)state->player.name_length,
	    state->conversion_mode, &overflow);
	if (overflow || converted < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "sector mine player name length");
		}
		return false;
	}
	if ((size_t)converted > YT_TEXT_FIELD_SIZE)
		converted = YT_TEXT_FIELD_SIZE;
	if (!yt_sector_mine_entry_news(state->player.record.bytes,
	    (size_t)converted, state->current_sector, row, sizeof(row),
	    &row_length)
	    || !ops->news(context, row, row_length, error))
		return false;

	for (;;) {
		struct yt_player working;
		struct yt_player persisted;
		float saved_foreground;
		float draw;
		float loss;
		float empty;

		state->touched = 0U;
		if (!ops->read_sector(context, current, &state->sector, error))
			return false;
		state->mines_before = state->sector.mines;
		state->batch = yt_sector_mine_batch(state->mines_before);
		yt_sector_mine_sector_overlay(&state->sector,
		    projectile_single_sub(state->mines_before, state->batch));
		if (!ops->write_sector(context, current, &state->sector, error))
			return false;
		++state->batches;
		saved_foreground = state->foreground;
		state->foreground = 3.0f;
		state->background = 0.0f;
		state->blink = 0.0f;
		state->pager_foreground = 3;
		sector_mine_apply_style(state, ops, context);
		if (!yt_sector_mine_explosion_row(state->mines_before,
		    state->batch, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_SECTOR_MINE_OUTPUT_BOLD_RAW, error))
			return false;
		state->background = 1.0f;
		sector_mine_apply_style(state, ops, context);
		if (!ops->present(context, NULL, 0U,
		    YT_SECTOR_MINE_OUTPUT_LINE, error)
		    || !ops->read_current(context, &state->player, error))
			return false;
		working = state->player;
		if (working.shields > 0.0f) {
			if (!ops->random(context, &draw, error))
				return false;
			working.shields = yt_sector_mine_shield_result(
			    working.shields, state->batch, draw);
			state->touched |= YT_SECTOR_MINE_DAMAGE_SHIELDS;
			if (working.shields == 0.0f) {
				state->foreground = 7.0f;
				state->blink = 1.0f;
				state->pager_foreground = 7;
				sector_mine_apply_style(state, ops, context);
				if (!ops->present(context, shields_destroyed,
				    sizeof(shields_destroyed) - 1U,
				    YT_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
					return false;
				state->foreground = saved_foreground;
				state->pager_foreground = (int)saved_foreground;
				sector_mine_apply_style(state, ops, context);
			} else {
				if (!yt_sector_mine_shields_row(working.shields,
				    row, sizeof(row), &row_length)
				    || !ops->present(context, row, row_length,
				    YT_SECTOR_MINE_OUTPUT_BOLD_LINE, error)
				    || !ops->random(context, &draw, error))
					return false;
				if (working.danger_scanner != 0.0f
				    && draw > 0.949999988079071f) {
					working.danger_scanner = 0.0f;
					state->touched |=
					    YT_SECTOR_MINE_DAMAGE_SCANNER;
					state->foreground = 7.0f;
					state->blink = 1.0f;
					state->pager_foreground = 7;
					sector_mine_apply_style(state, ops, context);
					if (!ops->present(context, scanner_destroyed,
					    sizeof(scanner_destroyed) - 1U,
					    YT_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
						return false;
					state->foreground = saved_foreground;
					state->pager_foreground =
					    (int)saved_foreground;
					sector_mine_apply_style(state, ops, context);
				}
			}
		} else {
#define MINE_LOSS_ROW(kind, operation) do { \
	if (!yt_sector_mine_loss_row((kind), loss, row, sizeof(row), \
	    &row_length) || !ops->present(context, row, row_length, \
	    YT_SECTOR_MINE_OUTPUT_LINE, error)) \
		return false; \
} while (0)
			if (working.fighters != 0.0f) {
				if (!ops->shrink(context,
				    projectile_single_mul(40000.0f, state->batch),
				    &loss, error))
					return false;
				if (loss > working.fighters)
					loss = working.fighters;
				working.fighters = projectile_single_sub(
				    working.fighters, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_FIGHTERS;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_FIGHTERS,
				    "sector mine fighter loss");
			}
			if (working.cloak != 0.0f) {
				if (!ops->random(context, &draw, error))
					return false;
				loss = yt_sector_mine_cloak_loss(working.cloak,
				    state->batch, draw);
				working.cloak = projectile_single_sub(
				    working.cloak, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_CLOAK;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_CLOAK,
				    "sector mine cloak loss");
			}
			if (working.missiles != 0.0f) {
				if (!ops->random(context, &draw, error))
					return false;
				loss = yt_sector_mine_missile_loss(working.missiles,
				    state->batch, draw);
				working.missiles = projectile_single_sub(
				    working.missiles, loss);
				state->touched |= YT_SECTOR_MINE_DAMAGE_MISSILES;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_MISSILES,
				    "sector mine missile loss");
			}
			if (working.danger_scanner != 0.0f) {
				working.danger_scanner = 0.0f;
				state->touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
				state->foreground = 7.0f;
				state->blink = 1.0f;
				state->pager_foreground = 7;
				sector_mine_apply_style(state, ops, context);
				if (!ops->present(context, scanner_destroyed,
				    sizeof(scanner_destroyed) - 1U,
				    YT_SECTOR_MINE_OUTPUT_BOLD_LINE, error))
					return false;
				state->foreground = saved_foreground;
				state->pager_foreground = (int)saved_foreground;
				sector_mine_apply_style(state, ops, context);
			}
#define MINE_STOCK(member, flag, kind) do { \
	if (working.member != 0.0f) { \
		if (!sector_mine_stock_loss(ops, context, state->batch, \
		    &working.member, &loss, error)) \
			return false; \
		state->touched |= (flag); \
		MINE_LOSS_ROW((kind), "sector mine stock loss"); \
	} \
} while (0)
			MINE_STOCK(mines, YT_SECTOR_MINE_DAMAGE_CARRIED_MINES,
			    YT_SECTOR_MINE_LOSS_MINES);
			MINE_STOCK(ore, YT_SECTOR_MINE_DAMAGE_ORE,
			    YT_SECTOR_MINE_LOSS_ORE);
			MINE_STOCK(organics, YT_SECTOR_MINE_DAMAGE_ORGANICS,
			    YT_SECTOR_MINE_LOSS_ORGANICS);
			MINE_STOCK(equipment, YT_SECTOR_MINE_DAMAGE_EQUIPMENT,
			    YT_SECTOR_MINE_LOSS_EQUIPMENT);
#undef MINE_STOCK
			empty = yt_sector_mine_empty_holds(&working);
			if (empty > 0.0f) {
				if (!ops->shrink(context, empty, &loss, error))
					return false;
				loss = projectile_single_mul(loss, state->batch);
				if (loss > empty)
					loss = empty;
			working.holds = projectile_single_sub(working.holds,
			    loss);
			if (working.holds < 1.0f) {
				working.holds = 0.0f;
				*state->destroyed = true;
			}
				state->touched |= YT_SECTOR_MINE_DAMAGE_HOLDS;
				MINE_LOSS_ROW(YT_SECTOR_MINE_LOSS_EMPTY_HOLDS,
				    "sector mine empty hold loss");
			}
#undef MINE_LOSS_ROW
		}
		if (!ops->read_player(context, state->current_player_record,
		    &persisted, error))
			return false;
		yt_sector_mine_player_overlay(&persisted, &working,
		    state->touched);
		if (!ops->write_player(context, state->current_player_record,
		    &persisted, error))
			return false;
		working.record = persisted.record;
		state->player = working;
		if (ops->set_current != NULL)
			ops->set_current(context, &working);
		if (!ops->sound(context, 2.0f, error)
		    || !ops->random(context, &draw, error))
			return false;
		if (draw > 0.800000011920929f && working.holds < 10.0f) {
			if (!ops->emergency_warp(context, error))
				return false;
			state->terminal = true;
			state->complete = true;
			return true;
		}
		if (state->sector.mines > 0.0f && !*state->destroyed)
			continue;
		break;
	}
	if (!yt_sector_mine_final_news(state->player.shields, row,
	    sizeof(row), &row_length)
	    || !ops->news(context, row, row_length, error)
	    || !ops->read_sector(context, current, &state->sector, error))
		return false;
	state->complete = true;
	return true;
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
	if (!ops->sound(context, 3.0f, error)
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
	    state->current_player_record, error))
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
	if (!ops->mine(context, &terminal, &state->destroyed, error))
		return false;
	if (terminal) {
		state->route = YT_DIRECT_FIGHTER_MINE_TERMINAL;
		return true;
	}
	if (!state->destroyed) {
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
yt_gameplay_hazard_error_project(unsigned error_number, unsigned saved_ip,
    struct yt_gameplay_hazard_error_request *request)
{
	if (request == NULL || error_number == 0U || error_number > UINT8_MAX
	    || saved_ip > UINT16_MAX)
		return false;
	request->error_number = (uint8_t)error_number;
	request->saved_ip = (uint16_t)saved_ip;
	request->handler = 0x45F7U;
	return true;
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

static bool
danger_scan_append(uint8_t *row, size_t capacity, size_t *length,
    const void *data, size_t data_length)
{
	if (*length > capacity || data_length > capacity - *length)
		return false;
	if (data_length != 0U)
		memcpy(row + *length, data, data_length);
	*length += data_length;
	return true;
}

static bool
danger_scan_present(struct yt_danger_scan_state *state,
    const struct yt_danger_scan_ops *ops, void *context,
    const uint8_t *text, size_t length,
    enum yt_danger_scan_output_kind kind, struct yt_error *error)
{
	state->attempted = YT_DANGER_SCAN_PRESENT;
	state->attempted_output = kind;
	if (!ops->present(context, text, length, kind, error))
		return false;
	++state->output_count;
	return true;
}

static void
danger_scan_store_relationship(struct yt_danger_scan_state *state,
    const struct yt_danger_scan_ops *ops, void *context,
    const uint8_t raw[4])
{
	memcpy(state->relationship_raw, raw, 4U);
	state->relationship = qb_mbf32_decode(raw);
	ops->store_relationship(context, raw);
}

static void
danger_scan_set_finding(struct yt_danger_scan_state *state)
{
	static const uint8_t one[4] = {0x00U, 0x00U, 0x00U, 0x81U};

	memcpy(state->finding_flag_raw, one, sizeof(one));
	state->finding_flag = 1.0f;
}

static bool
danger_scan_checkpoint(struct yt_danger_scan_state *state,
    const struct yt_danger_scan_ops *ops, void *context,
    enum yt_danger_scan_checkpoint checkpoint,
    enum yt_danger_scan_step step, struct yt_error *error)
{
	state->attempted = step;
	return ops->checkpoint(context, checkpoint, error);
}

static bool
danger_scan_first_warning(struct yt_danger_scan_state *state,
    const struct yt_danger_scan_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t warning[] = "*** WARNING! ***";
	static const uint8_t suffix[] =
	    " Danger Scanner has detected the following in sector!";
	uint8_t row[160];
	char number[64];
	int number_length;
	size_t length = 0U;

	if (state->finding_flag != 0.0f)
		return true;
	state->attempted = YT_DANGER_SCAN_MUSIC;
	if (!ops->sound(context, 8.0f, error)
	    || !danger_scan_present(state, ops, context, NULL, 0U,
	    YT_DANGER_SCAN_LEADING_BLANK, error))
		return false;
	ops->set_blink(context, 1.0f);
	if (!danger_scan_present(state, ops, context, warning,
	    sizeof(warning) - 1U, YT_DANGER_SCAN_WARNING_RAW, error))
		return false;
	number_length = qb_str_single(number, sizeof(number), state->target);
	if (number_length < 0
	    || !danger_scan_append(row, sizeof(row), &length, number,
	    (size_t)number_length)
	    || !danger_scan_append(row, sizeof(row), &length, suffix,
	    sizeof(suffix) - 1U))
		return startup_configuration_error(error, YT_RANGE,
		    "danger warning target row");
	return danger_scan_present(state, ops, context, row, length,
	    YT_DANGER_SCAN_WARNING_TARGET, error)
	    && danger_scan_present(state, ops, context, NULL, 0U,
	    YT_DANGER_SCAN_WARNING_BLANK, error);
}

bool
yt_danger_scan_run(struct yt_danger_scan_state *state,
    const struct yt_danger_scan_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t dirty_false[4] = {0x00U, 0x00U, 0x80U, 0x00U};
	static const uint8_t relationship_true[4] =
	    {0x00U, 0x00U, 0x80U, 0x81U};
	static const uint8_t disruption[] = "** Space-time disruption! **";
	static const uint8_t mine_prefix[] = "**";
	static const uint8_t mine_suffix[] = " SECTOR MINES! **";
	static const uint8_t fighter_prefix[] = "***";
	static const uint8_t fighter_middle[] = " Fighters Belonging to ";
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t team_prefix[] = " * Team [";
	static const uint8_t team_name_prefix[] = " [";
	static const uint8_t closing_bracket[] = "]";
	static const uint8_t deactivated[] =
	    "*** WARP DRIVE DEACTIVATED ***";
	uint8_t row[256];
	char number[80];
	size_t row_length;
	bool overflow;

	if (state == NULL || ops == NULL || ops->read_sector == NULL
	    || ops->read_player == NULL || ops->restore_current == NULL
	    || ops->checkpoint == NULL
	    || ops->sound == NULL || ops->present == NULL
	    || ops->foreground == NULL || ops->set_foreground == NULL
	    || ops->set_background == NULL || ops->set_blink == NULL
	    || ops->store_relationship == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "danger scanner transaction");
	memcpy(state->finding_flag_raw, dirty_false, sizeof(dirty_false));
	state->finding_flag = 0.0f;
	state->relationship = qb_mbf32_decode(state->relationship_raw);
	state->saved_foreground = 0.0f;
	memset(&state->target_sector, 0, sizeof(state->target_sector));
	memset(&state->owner_player, 0, sizeof(state->owner_player));
	memset(&state->team_overlay, 0, sizeof(state->team_overlay));
	state->attempted = YT_DANGER_SCAN_NONE;
	state->attempted_output = YT_DANGER_SCAN_LEADING_BLANK;
	state->output_count = 0U;
	state->target_read = false;
	state->owner_read = false;
	state->friendship_current_read = false;
	state->friendship_owner_read = false;
	state->team_read = false;
	state->current_player_restored = false;
	state->complete = false;
	if (state->target < 1.0f || state->target > state->sector_count) {
		state->complete = true;
		return true;
	}
	state->saved_foreground = ops->foreground(context);
	ops->set_foreground(context, 3.0f);
	ops->set_background(context, 4.0f);
	state->attempted = YT_DANGER_SCAN_TARGET_GET;
	if (!ops->read_sector(context, state->target,
	    &state->target_sector, error))
		return false;
	state->target_read = true;
	if (state->target == state->disruption_sectors[0]
	    || state->target == state->disruption_sectors[1]) {
		if (!danger_scan_first_warning(state, ops, context, error)
		    || !danger_scan_present(state, ops, context, disruption,
		    sizeof(disruption) - 1U, YT_DANGER_SCAN_DISRUPTION, error))
			return false;
		danger_scan_set_finding(state);
	}
	if (state->target_sector.mines != 0.0f) {
		int number_length;

		if (!danger_scan_first_warning(state, ops, context, error))
			return false;
		number_length = qb_str_single(number, sizeof(number),
		    state->target_sector.mines);
		row_length = 0U;
		if (number_length < 0
		    || !danger_scan_append(row, sizeof(row), &row_length,
		    mine_prefix, sizeof(mine_prefix) - 1U)
		    || !danger_scan_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length)
		    || !danger_scan_append(row, sizeof(row), &row_length,
		    mine_suffix, sizeof(mine_suffix) - 1U))
			return startup_configuration_error(error, YT_RANGE,
			    "danger mines row");
		if (!danger_scan_present(state, ops, context, row, row_length,
		    YT_DANGER_SCAN_MINES, error))
			return false;
		danger_scan_set_finding(state);
	}
	if (state->target_sector.fighters > 0.0f) {
		float owner = state->target_sector.fighter_owner;
		bool hostile;
		int number_length = qb_str_double(number, sizeof(number),
		    (double)state->target_sector.fighters);

		row_length = 0U;
		if (number_length < 0
		    || !danger_scan_append(row, sizeof(row), &row_length,
		    fighter_prefix, sizeof(fighter_prefix) - 1U)
		    || !danger_scan_append(row, sizeof(row), &row_length, number,
		    (size_t)number_length)
		    || !danger_scan_append(row, sizeof(row), &row_length,
		    fighter_middle, sizeof(fighter_middle) - 1U))
			return startup_configuration_error(error, YT_RANGE,
			    "danger fighters row");
		if (owner == -1.0f) {
			if (!danger_scan_append(row, sizeof(row), &row_length,
			    xannor, sizeof(xannor) - 1U))
				return false;
		}
		else if (owner == -2.0f) {
			if (!danger_scan_append(row, sizeof(row), &row_length,
			    mercenaries, sizeof(mercenaries) - 1U))
				return false;
		}
		else {
			int name_length;

			state->attempted = YT_DANGER_SCAN_OWNER_GET;
			if (!ops->read_player(context, owner,
			    &state->owner_player, error))
				return false;
			state->owner_read = true;
			if (!danger_scan_checkpoint(state, ops, context,
			    YT_DANGER_CHECK_OWNER_NAME_LEFT,
			    YT_DANGER_SCAN_OWNER_NAME_LEFT, error))
				return false;
			name_length = qb_cint_mbf32(
			    state->owner_player.record.bytes + YT_F85, 0U,
			    &overflow);
			if (overflow || name_length < 0)
				return startup_configuration_error(error, YT_RANGE,
				    "danger owner name length");
			if ((size_t)name_length > YT_TEXT_FIELD_SIZE)
				name_length = (int)YT_TEXT_FIELD_SIZE;
			if (!danger_scan_append(row, sizeof(row), &row_length,
			    state->owner_player.record.bytes, (size_t)name_length))
				return startup_configuration_error(error, YT_RANGE,
				    "danger owner name row");
			if (state->owner_player.team != 0.0f) {
				struct yt_player current;
				struct yt_player candidate;
				int team_name_length;

				if (!danger_scan_checkpoint(state, ops, context,
				    YT_DANGER_CHECK_FRIENDSHIP_HELPER,
				    YT_DANGER_SCAN_FRIENDSHIP_HELPER, error))
					return false;
				danger_scan_store_relationship(state, ops, context,
				    dirty_false);
				if (owner >= 2.0f && owner <= state->sector_offset
				    && state->current_player_record >= 2.0f
				    && state->current_player_record <= state->sector_offset) {
					if (owner == state->current_player_record)
						danger_scan_store_relationship(state, ops,
						    context, relationship_true);
					else {
						state->attempted =
						    YT_DANGER_SCAN_FRIEND_CURRENT_GET;
						if (!ops->read_player(context,
						    state->current_player_record, &current,
						    error))
							return false;
						state->friendship_current_read = true;
						if (current.team != 0.0f) {
							state->attempted =
							    YT_DANGER_SCAN_FRIEND_OWNER_GET;
							if (!ops->read_player(context, owner,
							    &candidate, error))
								return false;
							state->friendship_owner_read = true;
							if (candidate.team == current.team)
								danger_scan_store_relationship(state,
								    ops, context, relationship_true);
						}
					}
				}
				number_length = qb_str_single(number, sizeof(number),
				    state->owner_player.team);
				if (number_length < 1
				    || !danger_scan_append(row, sizeof(row), &row_length,
				    team_prefix, sizeof(team_prefix) - 1U)
				    || !danger_scan_append(row, sizeof(row), &row_length,
				    number + 1, (size_t)number_length - 1U)
				    || !danger_scan_append(row, sizeof(row), &row_length,
				    closing_bracket, sizeof(closing_bracket) - 1U))
					return startup_configuration_error(error, YT_RANGE,
					    "danger team number row");
				state->attempted = YT_DANGER_SCAN_TEAM_GET;
				if (!ops->read_sector(context, state->owner_player.team,
				    &state->team_overlay, error))
					return false;
				state->team_read = true;
				team_name_length = qb_cint_mbf32(
				    state->team_overlay.record.bytes + YT_F73, 0U,
				    &overflow);
				if (overflow || team_name_length < 0)
					return startup_configuration_error(error, YT_RANGE,
					    "danger team name length");
				if (team_name_length > 0) {
					size_t amount = (size_t)team_name_length;

					if (!danger_scan_checkpoint(state, ops, context,
					    YT_DANGER_CHECK_TEAM_NAME_LEFT,
					    YT_DANGER_SCAN_TEAM_NAME_LEFT, error))
						return false;
					if (amount > YT_TEXT_FIELD_SIZE)
						amount = YT_TEXT_FIELD_SIZE;
					if (!danger_scan_append(row, sizeof(row), &row_length,
					    team_name_prefix,
					    sizeof(team_name_prefix) - 1U)
					    || !danger_scan_append(row, sizeof(row), &row_length,
					    state->team_overlay.record.bytes, amount)
					    || !danger_scan_append(row, sizeof(row), &row_length,
					    closing_bracket,
					    sizeof(closing_bracket) - 1U))
						return startup_configuration_error(error,
						    YT_RANGE, "danger team name row");
				}
			}
		}
		hostile = owner < 0.0f;
		if (!hostile && owner > 1.0f && owner <= state->sector_offset
		    && owner != state->current_player_record) {
			int relationship;

			if (!danger_scan_checkpoint(state, ops, context,
			    YT_DANGER_CHECK_RELATIONSHIP_CINT,
			    YT_DANGER_SCAN_RELATIONSHIP_CINT, error))
				return false;
			relationship = qb_cint_mbf32(state->relationship_raw, 0U,
			    &overflow);
			if (overflow)
				return startup_configuration_error(error, YT_RANGE,
				    "danger relationship CINT");
			hostile = ~relationship != 0;
		}
		if (hostile) {
			if (!danger_scan_first_warning(state, ops, context, error))
				return false;
			danger_scan_set_finding(state);
			if (!danger_scan_present(state, ops, context, row, row_length,
			    YT_DANGER_SCAN_FIGHTERS, error))
				return false;
		}
	}
	state->attempted = YT_DANGER_SCAN_RESTORE_CURRENT;
	if (!ops->restore_current(context, error))
		return false;
	state->current_player_restored = true;
	if (state->finding_flag != 0.0f) {
		if (!danger_scan_present(state, ops, context, NULL, 0U,
		    YT_DANGER_SCAN_FINAL_BLANK, error))
			return false;
		ops->set_blink(context, 1.0f);
		if (!danger_scan_present(state, ops, context, deactivated,
		    sizeof(deactivated) - 1U, YT_DANGER_SCAN_DEACTIVATED, error))
			return false;
	}
	ops->set_foreground(context, state->saved_foreground);
	ops->set_background(context, 0.0f);
	state->complete = true;
	return true;
}

static float
movement_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

bool
yt_movement_run(struct yt_movement_state *state,
    const struct yt_movement_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t prompt[] = "Move to which sector? ";
	static const uint8_t same_sector[] =
	    "That was quick! Felt like we didn't even move!";
	static const uint8_t not_adjacent[] =
	    "You can't get there from here.";
	uint8_t row[256];
	size_t row_length;
	char response[4096];
	struct qb_val_result parsed;
	size_t slot;

	if (state == NULL || ops == NULL || state->current_player_record < 1
	    || ops->turn_gate == NULL || ops->present == NULL
	    || ops->input == NULL || ops->danger == NULL
	    || ops->clear_queue == NULL || ops->confirm == NULL
	    || ops->finalize == NULL
	    || ops->clear_self_mine_suppression == NULL
	    || ops->hydrate == NULL || ops->write_player == NULL
	    || ops->flush_player == NULL || ops->update_cache == NULL)
		return false;
	memset(&state->player, 0, sizeof(state->player));
	memset(&state->accepted_player, 0, sizeof(state->accepted_player));
	state->maximum = 0.0f;
	state->target = 0.0f;
	memset(state->target_raw, 0, sizeof(state->target_raw));
	state->attempts = 0U;
	state->matching_warp = 0U;
	state->turn_gate_called = false;
	state->turn_denied = false;
	state->target_stored = false;
	state->adjacent = false;
	state->danger_called = false;
	state->dangerous = false;
	state->confirmation_read = false;
	state->finalizer_called = false;
	state->self_mine_suppression_cleared = false;
	state->player_hydrated = false;
	state->player_written = false;
	state->player_flushed = false;
	state->cache_updated = false;
	state->complete = false;
	state->route = YT_MOVEMENT_INCOMPLETE;

	state->turn_gate_called = true;
	if (!ops->turn_gate(context, state->current_player_record,
	    &state->player, &state->turn_denied, error))
		return false;
	if (state->turn_denied) {
		state->route = YT_MOVEMENT_TURN_DENIED;
		state->complete = true;
		return true;
	}
	if (!yt_movement_warp_row(state->warps, row, sizeof(row),
	    &row_length))
		return startup_configuration_error(error, YT_RANGE,
		    "movement warp row");
	if (!ops->present(context, row, row_length, YT_MOVEMENT_WARP_ROW,
	    error)
	    || !ops->present(context, NULL, 0U,
	    YT_MOVEMENT_POST_WARP_BLANK, error))
		return false;
	for (;;) {
		if (!ops->present(context, prompt, sizeof(prompt) - 1U,
		    YT_MOVEMENT_DESTINATION_PROMPT, error))
			return false;
		memset(response, 0, sizeof(response));
		if (!ops->input(context, response, sizeof(response), error))
			return false;
		++state->attempts;
		if (memchr(response, '\0', sizeof(response)) == NULL)
			return startup_configuration_error(error, YT_RANGE,
			    "movement destination response");
		if (strcmp(response, "M") != 0)
			break;
	}
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "movement destination VAL");
	state->target = (float)(parsed.valid ? parsed.value : 0.0);
	if (qb_mbf32_encode(state->target, state->target_raw)
	    == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "movement destination CSNG");
	state->target = qb_mbf32_decode(state->target_raw);
	state->target_stored = true;
	state->maximum = movement_single_sub(state->port_offset,
	    state->sector_offset);
	if (state->target < 1.0f || state->target > state->maximum) {
		state->route = YT_MOVEMENT_BOUNDS_CANCELLED;
		state->complete = true;
		return true;
	}
	if (state->target == state->player.sector) {
		if (!ops->present(context, same_sector,
		    sizeof(same_sector) - 1U, YT_MOVEMENT_SAME_SECTOR, error))
			return false;
		state->route = YT_MOVEMENT_SAME_SECTOR_ROUTE;
		state->complete = true;
		return true;
	}
	for (slot = 0U; slot < YT_ARRAY_LEN(state->warps); ++slot) {
		if (state->warps[slot] == state->target) {
			state->adjacent = true;
			state->matching_warp = slot + 1U;
			break;
		}
	}
	if (!state->adjacent) {
		if (!ops->present(context, not_adjacent,
		    sizeof(not_adjacent) - 1U, YT_MOVEMENT_NOT_ADJACENT, error))
			return false;
		state->route = YT_MOVEMENT_NOT_ADJACENT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U, YT_MOVEMENT_ACCEPTED_BLANK,
	    error))
		return false;
	if (state->player.danger_scanner != 0.0f) {
		state->danger_called = true;
		if (!ops->danger(context, state->target, &state->dangerous,
		    error))
			return false;
		if (state->dangerous) {
			bool accepted;

			if (!ops->present(context, NULL, 0U,
			    YT_MOVEMENT_CONFIRMATION_BLANK, error))
				return false;
			ops->clear_queue(context);
			if (!yt_movement_confirmation_prompt(state->target, row,
			    sizeof(row), &row_length))
				return startup_configuration_error(error, YT_RANGE,
				    "movement confirmation prompt");
			if (!ops->confirm(context, row, row_length, &accepted,
			    error))
				return false;
			state->confirmation_read = true;
			if (!accepted) {
				state->route = YT_MOVEMENT_DANGER_DECLINED;
				state->complete = true;
				return true;
			}
		}
	}
	state->finalizer_called = true;
	if (!ops->finalize(context, error)) {
		if (error != NULL && error->status != YT_OK)
			return false;
		state->route = YT_MOVEMENT_FINALIZER_TERMINAL;
		state->complete = true;
		return true;
	}
	ops->clear_self_mine_suppression(context);
	state->self_mine_suppression_cleared = true;
	if (!ops->hydrate(context, state->current_player_record,
	    &state->accepted_player, error))
		return false;
	state->player_hydrated = true;
	yt_movement_player_overlay(&state->accepted_player, state->target);
	if (!ops->write_player(context, state->current_player_record,
	    &state->accepted_player, error))
		return false;
	state->player_written = true;
	if (!ops->flush_player(context, error))
		return false;
	state->player_flushed = true;
	{
		uint8_t target_raw[4];

		if (qb_mbf32_encode(state->target, target_raw) != QB_MBF_OK
		    || !ops->update_cache(context, state->current_player_record,
		    target_raw, error))
			return false;
	}
	state->cache_updated = true;
	state->route = YT_MOVEMENT_MOVED;
	state->complete = true;
	return true;
}

static float
treasury_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
treasury_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static bool
treasury_format_single(const char *prefix, float value, char *text,
    size_t capacity, struct yt_error *error, const char *operation)
{
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), value);
	int result;

	if (number_length < 0)
		return startup_configuration_error(error, YT_RANGE, operation);
	result = snprintf(text, capacity, "%s%s", prefix, number);
	if (result < 0 || (size_t)result >= capacity)
		return startup_configuration_error(error, YT_RANGE, operation);
	return true;
}

static bool
treasury_format_double(const char *prefix, const uint8_t raw[8],
    const char *suffix, char *text, size_t capacity,
    struct yt_error *error, const char *operation)
{
	char number[96];
	int number_length = qb_str_mbf64(number, sizeof(number), raw);
	int result;

	if (number_length < 0)
		return startup_configuration_error(error, YT_RANGE, operation);
	result = snprintf(text, capacity, "%s%s%s", prefix, number,
	    suffix == NULL ? "" : suffix);
	if (result < 0 || (size_t)result >= capacity)
		return startup_configuration_error(error, YT_RANGE, operation);
	return true;
}

static void
treasury_promote_single(const uint8_t single[4], uint8_t raw[8])
{
	memset(raw, 0, 8U);
	if (single[3] != 0U)
		memcpy(raw + 4U, single, 4U);
}

static bool
treasury_add_single_to_total(struct yt_treasury_state *state,
    const uint8_t value_raw[4],
    struct yt_error *error)
{
	uint8_t promoted[8];
	uint8_t sum[8];

	treasury_promote_single(value_raw, promoted);
	if (qb_mbf64_add_raw(state->total_raw, promoted, sum) != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "treasury MBF56 accumulation");
	memcpy(state->total_raw, sum, sizeof(state->total_raw));
	state->total = qb_mbf64_decode(state->total_raw);
	return true;
}

static bool
treasury_player_overlay(struct yt_player *player, float owned,
    const uint8_t total_raw[8], struct yt_error *error)
{
	uint8_t fresh_credits[8];
	uint8_t summed_credits[8];
	uint8_t stored_credits[4];
	uint8_t stored_owned[4];

	treasury_promote_single(player->record.bytes + YT_F81, fresh_credits);
	if (qb_mbf64_add_raw(fresh_credits, total_raw, summed_credits)
	    != QB_MBF_OK
	    || qb_mbf32_from_mbf64_raw(summed_credits, stored_credits)
	    == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(owned, stored_owned) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F117,
	    stored_owned)
	    || !yt_record_set_raw_number(&player->record, YT_F81,
	    stored_credits))
		return startup_configuration_error(error, YT_RANGE,
		    "treasury player overlay");
	player->ports_owned = qb_mbf32_decode(stored_owned);
	player->credits = qb_mbf32_decode(stored_credits);
	return true;
}

bool
yt_treasury_run(struct yt_treasury_state *state,
    const struct yt_treasury_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	static const uint8_t no_ports[] = "You don't OWN any ports!!!";
	static const uint8_t collect_prefix[] =
	    "Sending out armored cargo ships to";
	static const uint8_t report_prefix[] =
	    "Checking galactic bank statement for";
	static const uint8_t heading_suffix[] = " ports with credits...";
	char text[192];

	if (state == NULL || ops == NULL
	    || ops->read_player == NULL || ops->read_port == NULL
	    || ops->write_port == NULL || ops->present == NULL
	    || ops->write_player == NULL || ops->flush_player == NULL
	    || ops->update_cache == NULL)
		return false;
	memset(&state->initial_player, 0, sizeof(state->initial_player));
	memset(&state->final_player, 0, sizeof(state->final_player));
	memset(&state->current_port, 0, sizeof(state->current_port));
	state->loop_bound = 0.0f;
	state->counter = 0.0f;
	state->owned = 0.0f;
	state->credited = 0.0f;
	state->barren = 0.0f;
	memset(state->total_raw, 0, sizeof(state->total_raw));
	state->total = 0.0;
	state->current_record_expression = 0.0f;
	state->current_player_physical_record = 0U;
	state->current_physical_record = 0U;
	state->records_read = 0U;
	state->records_written = 0U;
	state->initial_player_read = false;
	state->final_player_read = false;
	state->player_written = false;
	state->player_flushed = false;
	state->cache_updated = false;
	state->complete = false;
	state->route = YT_TREASURY_INCOMPLETE;

	if (!ops->present(context, NULL, 0U, YT_TREASURY_OPENING_BLANK,
	    error))
		return false;
	state->current_player_physical_record = qb_brun_random_record_number(
	    state->current_player_record);
	if (state->current_player_physical_record == 0U)
		return startup_configuration_error(error, YT_RANGE,
		    "treasury player record conversion");
	if (!ops->read_player(context, state->current_player_physical_record,
	    &state->initial_player, error))
		return false;
	state->initial_player_read = true;
	if (state->initial_player.ports_owned < 1.0f) {
		if (!ops->present(context, no_ports, sizeof(no_ports) - 1U,
		    YT_TREASURY_NO_PORTS, error))
			return false;
		state->route = YT_TREASURY_NO_PORTS_ROUTE;
		state->complete = true;
		return true;
	}
	if (!ops->present(context,
	    state->collecting ? collect_prefix : report_prefix,
	    state->collecting ? sizeof(collect_prefix) - 1U
	    : sizeof(report_prefix) - 1U,
	    YT_TREASURY_HEADING_PREFIX, error)
	    || !ops->present(context, heading_suffix,
	    sizeof(heading_suffix) - 1U, YT_TREASURY_HEADING_SUFFIX, error)
	    || !ops->present(context, NULL, 0U, YT_TREASURY_SCAN_BLANK,
	    error))
		return false;

	state->loop_bound = treasury_single_sub(state->planet_offset,
	    state->port_offset);
	state->counter = 1.0f;
	while (state->counter <= state->loop_bound) {
		bool conversion_overflow;
		int32_t converted_length;
		size_t name_length;

		state->current_record_expression = treasury_single_add(
		    state->port_offset, state->counter);
		state->current_physical_record = qb_brun_random_record_number(
		    state->current_record_expression);
		if (state->current_physical_record == 0U)
			return startup_configuration_error(error, YT_RANGE,
			    "treasury port record conversion");
		if (!ops->read_port(context, state->current_physical_record,
		    &state->current_port, error))
			return false;
		++state->records_read;
		if (state->current_port.owner
		    == (float)state->current_player_record) {
			state->owned = treasury_single_add(state->owned, 1.0f);
			if (qb_mbf32_truth(state->current_port.record.bytes
			    + YT_F89)) {
				state->credited = treasury_single_add(
				    state->credited, 1.0f);
				if (!treasury_add_single_to_total(state,
				    state->current_port.record.bytes + YT_F89, error)
				    || !treasury_format_single("Sector:",
				    state->current_port.sector, text, sizeof(text), error,
				    "treasury sector field")
				    || !ops->present(context, (const uint8_t *)text,
				    strlen(text), YT_TREASURY_SECTOR_FIELD, error))
					return false;
				conversion_overflow = false;
				converted_length = qb_cint_mbf32(
				    state->current_port.record.bytes + YT_F85,
				    state->conversion_mode, &conversion_overflow);
				if (conversion_overflow || converted_length < 0)
					return startup_configuration_error(error,
					    YT_RANGE, "treasury port-name length");
				name_length = (size_t)converted_length;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				if (!ops->present(context,
				    state->current_port.record.bytes, name_length,
				    YT_TREASURY_NAME_FIELD, error)
				    || !treasury_format_single(" Credits:",
				    state->current_port.treasury, text,
				    sizeof(text), error, "treasury credit field")
				    || !ops->present(context, (const uint8_t *)text,
				    strlen(text), YT_TREASURY_CREDIT_FIELD, error)
				    || !treasury_format_double(" Total:",
				    state->total_raw, NULL, text, sizeof(text), error,
				    "treasury row total")
				    || !ops->present(context, (const uint8_t *)text,
				    strlen(text), YT_TREASURY_ROW_TOTAL, error))
					return false;
				if (state->collecting) {
					state->current_port.treasury = 0.0f;
					if (!yt_record_set_raw_number(
					    &state->current_port.record, YT_F89,
					    dirty_zero))
						return false;
					if (!ops->write_port(context,
					    state->current_physical_record,
					    &state->current_port, error))
						return false;
					++state->records_written;
				}
			}
		}
		state->counter = treasury_single_add(state->counter, 1.0f);
	}
	if (state->total_raw[7] != 0U
	    && !ops->present(context, NULL, 0U,
	    YT_TREASURY_NONZERO_BLANK, error))
		return false;
	if (!treasury_format_single("Total ports...:", state->owned, text,
	    sizeof(text), error, "treasury total ports")
	    || !ops->present(context, (const uint8_t *)text, strlen(text),
	    YT_TREASURY_TOTAL_PORTS, error)
	    || !treasury_format_single("With credits..:", state->credited,
	    text, sizeof(text), error, "treasury credited ports")
	    || !ops->present(context, (const uint8_t *)text, strlen(text),
	    YT_TREASURY_WITH_CREDITS, error))
		return false;
	state->barren = treasury_single_sub(state->owned, state->credited);
	if (!treasury_format_single("Barren ports..:", state->barren, text,
	    sizeof(text), error, "treasury barren ports")
	    || !ops->present(context, (const uint8_t *)text, strlen(text),
	    YT_TREASURY_BARREN_PORTS, error)
	    || !treasury_format_double("Total credits.:", state->total_raw,
	    NULL, text, sizeof(text), error, "treasury total credits")
	    || !ops->present(context, (const uint8_t *)text, strlen(text),
	    YT_TREASURY_TOTAL_CREDITS, error)
	    || !ops->present(context, NULL, 0U, YT_TREASURY_SUMMARY_BLANK,
	    error))
		return false;
	if (!state->collecting) {
		if (!treasury_format_double("You have", state->total_raw,
		    " credits in your port accounts.", text, sizeof(text), error,
		    "treasury report result")
		    || !ops->present(context, (const uint8_t *)text, strlen(text),
		    YT_TREASURY_REPORT_RESULT, error))
			return false;
		state->route = YT_TREASURY_REPORT_ROUTE;
		state->complete = true;
		return true;
	}
	if (!treasury_format_double("You collected a total of",
	    state->total_raw, " credits.", text, sizeof(text), error,
	    "treasury collection result")
	    || !ops->present(context, (const uint8_t *)text, strlen(text),
	    YT_TREASURY_COLLECTION_RESULT, error)
	    || !ops->read_player(context, state->current_player_physical_record,
	    &state->final_player, error))
		return false;
	state->final_player_read = true;
	if (!treasury_player_overlay(&state->final_player, state->owned,
	    state->total_raw, error))
		return false;
	if (!ops->write_player(context, state->current_player_physical_record,
	    &state->final_player, error))
		return false;
	state->player_written = true;
	if (!ops->flush_player(context, error))
		return false;
	state->player_flushed = true;
	if (!ops->update_cache(context, &state->final_player, error))
		return false;
	state->cache_updated = true;
	state->route = YT_TREASURY_COLLECTION_ROUTE;
	state->complete = true;
	return true;
}

static void
market_promote_single(const uint8_t single[4], uint8_t raw[8])
{
	memset(raw, 0, 8U);
	if (single[3] != 0U)
		memcpy(raw + 4U, single, 4U);
}

static bool
market_encode_single(float value, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return startup_configuration_error(error, YT_RANGE, operation);
}

typedef enum qb_mbf_status (*market_binary_fn)(const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8]);

static bool
market_binary(market_binary_fn operation, const uint8_t left[8],
    const uint8_t right[8], uint8_t result[8], struct yt_error *error,
    const char *label)
{
	enum qb_mbf_status status = operation(left, right, result);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return startup_configuration_error(error, YT_RANGE, label);
}

static int
market_compare_magnitude(const uint8_t left[8], const uint8_t right[8])
{
	int index;

	if (left[7] != right[7])
		return left[7] < right[7] ? -1 : 1;
	for (index = 6; index >= 0; --index) {
		uint8_t left_byte = left[index];
		uint8_t right_byte = right[index];

		if (index == 6) {
			left_byte &= 0x7fU;
			right_byte &= 0x7fU;
		}
		if (left_byte != right_byte)
			return left_byte < right_byte ? -1 : 1;
	}
	return 0;
}

static int
market_compare(const uint8_t left[8], const uint8_t right[8])
{
	bool left_negative;
	bool right_negative;
	int magnitude;

	if (left[7] == 0U)
		return right[7] == 0U ? 0
		    : ((right[6] & 0x80U) != 0U ? 1 : -1);
	if (right[7] == 0U)
		return (left[6] & 0x80U) != 0U ? -1 : 1;
	left_negative = (left[6] & 0x80U) != 0U;
	right_negative = (right[6] & 0x80U) != 0U;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	magnitude = market_compare_magnitude(left, right);
	return left_negative ? -magnitude : magnitude;
}

static void
market_negate(uint8_t raw[8])
{
	if (raw[7] != 0U)
		raw[6] ^= 0x80U;
}

static float
market_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
market_single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static float
market_single_mul(float left, float right)
{
	volatile float result = left * right;

	return result;
}

static float
market_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

bool
yt_port_market_update(struct yt_port_market_state *state,
    struct yt_error *error)
{
	uint8_t ten[8];
	uint8_t thousand[8];
	uint8_t one[8];
	uint8_t half[8];
	uint8_t zero[8] = {0};
	uint8_t current_day_raw[4];
	uint8_t current_minute_raw[4];
	uint8_t mutable_capacity[3][8];
	uint8_t mutable_production[3][4];
	uint8_t mutable_price[3][4];
	struct yt_record updated;
	bool raised[3] = {false, false, false};
	float minute;
	float elapsed;
	size_t index;

	if (state == NULL)
		return false;
	state->current_minute = 0.0f;
	state->elapsed = 0.0f;
	memset(state->capacity_raw, 0, sizeof(state->capacity_raw));
	memset(state->capacity, 0, sizeof(state->capacity));
	memset(state->production_raw, 0, sizeof(state->production_raw));
	memset(state->price_raw, 0, sizeof(state->price_raw));
	memset(state->price, 0, sizeof(state->price));
	memset(state->production_raised, 0,
	    sizeof(state->production_raised));
	state->completed_items = 0U;
	state->complete = false;

	if (qb_mbf64_from_u64(10U, ten) != QB_MBF_OK
	    || qb_mbf64_from_u64(1000U, thousand) != QB_MBF_OK
	    || qb_mbf64_from_u64(1U, one) != QB_MBF_OK
	    || qb_mbf64_encode(0.5, half) != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "ordinary port constants");
	minute = market_single_div(state->timer_seconds, 60.0f);
	elapsed = market_single_add(
	    market_single_sub(state->current_day, state->port.last_day),
	    market_single_div(market_single_sub(minute,
	    state->port.last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	if (!market_encode_single(state->current_day, current_day_raw, error,
	    "ordinary port current day")
	    || !market_encode_single(minute, current_minute_raw, error,
	    "ordinary port current minute"))
		return false;

	for (index = 0U; index < 3U; ++index) {
		uint8_t growth_raw[4];
		uint8_t growth[8];
		uint8_t quotient[8];
		uint8_t promoted_production[8];
		uint8_t base_raw[4];
		uint8_t base[8];
		uint8_t factor[8];
		uint8_t numerator[8];
		uint8_t denominator[8];
		uint8_t ratio[8];
		uint8_t scale[8];
		uint8_t raw_price[8];
		uint8_t rounded_source[8];
		uint8_t rounded[8];
		float growth_value;

		market_promote_single(state->port.record.bytes
		    + YT_F49 + index * 4U, mutable_capacity[index]);
		memcpy(mutable_production[index], state->port.record.bytes
		    + YT_F61 + index * 4U, 4U);
		growth_value = market_single_mul(
		    qb_mbf32_decode(mutable_production[index]), elapsed);
		if (!market_encode_single(growth_value, growth_raw, error,
		    "ordinary port growth"))
			return false;
		market_promote_single(growth_raw, growth);
		if (!market_binary(qb_mbf64_add_raw, mutable_capacity[index],
		    growth, mutable_capacity[index], error,
		    "ordinary port capacity")
		    || !market_binary(qb_mbf64_div_raw, mutable_capacity[index],
		    ten, quotient, error, "ordinary port production comparison"))
			return false;
		market_promote_single(mutable_production[index],
		    promoted_production);
		if (market_compare(quotient, promoted_production) > 0) {
			if (!market_binary(qb_mbf64_div_raw,
			    mutable_capacity[index], ten, quotient, error,
			    "ordinary port production replacement")
			    || qb_mbf32_from_mbf64_raw(quotient,
			    mutable_production[index]) == QB_MBF_OVERFLOW)
				return startup_configuration_error(error, YT_RANGE,
				    "ordinary port production CSNG");
			raised[index] = true;
			market_promote_single(mutable_production[index],
			    promoted_production);
		}
		if (!market_encode_single(state->base_price[index], base_raw,
		    error, "ordinary port base price"))
			return false;
		market_promote_single(base_raw, base);
		market_promote_single(state->port.record.bytes
		    + YT_F73 + index * 4U, factor);
		if (!market_binary(qb_mbf64_mul_raw, factor,
		    mutable_capacity[index], numerator, error,
		    "ordinary port price numerator")
		    || !market_binary(qb_mbf64_mul_raw, promoted_production,
		    thousand, denominator, error,
		    "ordinary port price denominator")
		    || !market_binary(qb_mbf64_div_raw, numerator, denominator,
		    ratio, error, "ordinary port price division"))
			return false;
		memcpy(scale, ratio, 8U);
		market_negate(scale);
		if (!market_binary(qb_mbf64_add_raw, one, scale, scale, error,
		    "ordinary port price scale")
		    || !market_binary(qb_mbf64_mul_raw, base, scale, raw_price,
		    error, "ordinary port raw price")
		    || !market_binary(qb_mbf64_add_raw, raw_price, half,
		    rounded_source, error, "ordinary port price rounding"))
			return false;
		if (market_compare(rounded_source, zero) <= 0)
			memset(rounded, 0, sizeof(rounded));
		else {
			enum qb_mbf_status status = qb_mbf64_floor_positive_raw(
			    rounded_source, rounded);

			if (status != QB_MBF_OK && status != QB_MBF_UNDERFLOW)
				return startup_configuration_error(error, YT_RANGE,
				    "ordinary port price INT");
		}
		if (qb_mbf32_from_mbf64_raw(rounded, mutable_price[index])
		    == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary port price CSNG");
		if (qb_mbf32_decode(mutable_price[index]) < 1.0f
		    && !market_encode_single(1.0f, mutable_price[index], error,
		    "ordinary port price floor"))
			return false;
		++state->completed_items;
	}

	updated = state->port.record;
	if (!yt_record_set_raw_number(&updated, YT_F45,
	    current_day_raw)
	    || !yt_record_set_raw_number(&updated, YT_F101,
	    current_minute_raw))
		return false;
	for (index = 0U; index < 3U; ++index) {
		uint8_t stored_capacity[4];

		if (qb_mbf32_from_mbf64_raw(mutable_capacity[index],
		    stored_capacity) == QB_MBF_OVERFLOW
		    || !yt_record_set_raw_number(&updated,
		    YT_F49 + index * 4U, stored_capacity)
		    || !yt_record_set_raw_number(&updated,
		    YT_F61 + index * 4U, mutable_production[index]))
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary port FIELD overlay");
		memcpy(state->capacity_raw[index], mutable_capacity[index], 8U);
		state->capacity[index] = qb_mbf64_decode(mutable_capacity[index]);
		memcpy(state->production_raw[index], mutable_production[index], 4U);
		memcpy(state->price_raw[index], mutable_price[index], 4U);
		state->price[index] = qb_mbf32_decode(mutable_price[index]);
		state->production_raised[index] = raised[index];
	}
	state->current_minute = minute;
	state->elapsed = elapsed;
	yt_port_decode(&state->port, &updated);
	state->complete = true;
	return true;
}

bool
yt_port_update_run(struct yt_port_update_state *state,
    const struct yt_port_update_ops *ops, void *context,
    struct yt_error *error)
{
	float logical_port;
	float record_expression;
	uint32_t physical_record;

	if (state == NULL || ops == NULL || ops->read_sector == NULL
	    || ops->observe_day == NULL || ops->read_port == NULL
	    || ops->observe_timer == NULL || ops->write_port == NULL)
		return false;
	state->sector_read = false;
	state->day_observed = false;
	state->port_read = false;
	state->timer_observed = false;
	state->port_write_attempted = false;
	state->port_written = false;
	state->complete = false;
	memset(&state->market, 0, sizeof(state->market));
	if (!state->sector_loaded) {
		memset(&state->sector, 0, sizeof(state->sector));
		if (!state->sector_record_supplied)
			state->sector_record_expression = market_single_add(
			    state->sector_record_offset,
			    (float)state->sector_number);
		state->sector_physical_record = qb_brun_random_record_number(
		    state->sector_record_expression);
		if (state->sector_physical_record == 0U)
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary port sector record conversion");
		if (!ops->read_sector(context, state->sector_physical_record,
		    &state->sector, error))
			return false;
		state->sector_read = true;
		state->sector_loaded = true;
	}
	logical_port = state->sector.port;
	record_expression = market_single_add(state->port_offset, logical_port);
	physical_record = qb_brun_random_record_number(record_expression);
	state->market.logical_port = logical_port;
	state->market.port_record_expression = record_expression;
	state->market.port_physical_record = physical_record;
	if (physical_record == 0U)
		return startup_configuration_error(error, YT_RANGE,
		    "ordinary port record conversion");
	if (!ops->observe_day(context, &state->market.current_day, error))
		return false;
	state->day_observed = true;
	if (!ops->read_port(context, physical_record, &state->market.port,
	    error))
		return false;
	state->port_read = true;
	if (!ops->observe_timer(context, &state->market.timer_seconds, error))
		return false;
	state->timer_observed = true;
	memcpy(state->market.base_price, state->base_price,
	    sizeof(state->base_price));
	if (!yt_port_market_update(&state->market, error))
		return false;
	state->port_write_attempted = true;
	if (!ops->write_port(context, physical_record, &state->market.port,
	    error))
		return false;
	state->port_written = true;
	state->complete = true;
	return true;
}

static void
port_ordinary_project_update(struct yt_port_ordinary_state *state)
{
	if (state->update.port_read) {
		state->field_kind = YT_PORT_ORDINARY_FIELD_PORT;
		state->field_record = state->update.market.port_physical_record;
		state->field = state->update.market.port.record;
		state->field_valid = true;
	}
	else if (state->update.sector_read) {
		state->field_kind = YT_PORT_ORDINARY_FIELD_SECTOR;
		state->field_record = state->update.sector_physical_record;
		state->field = state->update.sector.record;
		state->field_valid = true;
	}
	state->persistence_attempted = state->update.port_write_attempted;
	state->persistence_committed = state->update.port_written;
}

static void
port_ordinary_project_report(struct yt_port_ordinary_state *state)
{
	if (state->report.owner_player_read) {
		state->field_kind = YT_PORT_ORDINARY_FIELD_PLAYER;
		state->field_record = (uint32_t)state->report.owner_record;
		state->field = state->report.owner_player.record;
		state->field_valid = true;
	}
	if (state->report.current_player_read) {
		state->field_kind = YT_PORT_ORDINARY_FIELD_PLAYER;
		state->field_record =
		    (uint32_t)state->report.current_player_record;
		state->field = state->report.current_player.record;
		state->field_valid = true;
	}
	if (state->report.report_port_read) {
		state->field_kind = YT_PORT_ORDINARY_FIELD_PORT;
		state->field_record = state->report.port_physical_record;
		state->field = state->report.report_port.record;
		state->field_valid = true;
	}
}

bool
yt_port_ordinary_run(struct yt_port_ordinary_state *state,
    const struct yt_port_update_ops *update_ops,
    const struct yt_port_report_ops *report_ops, void *context,
    struct yt_error *error)
{
	if (state == NULL || update_ops == NULL || report_ops == NULL)
		return false;
	state->persistence_attempted = false;
	state->persistence_committed = false;
	state->report_started = false;
	state->complete = false;
	if (!yt_port_update_run(&state->update, update_ops, context, error)) {
		port_ordinary_project_update(state);
		return false;
	}
	port_ordinary_project_update(state);
	state->report.port_physical_record =
	    state->update.market.port_physical_record;
	state->report.market = state->update.market;
	state->report_started = true;
	if (!yt_port_report_run(&state->report, report_ops, context, error)) {
		port_ordinary_project_report(state);
		return false;
	}
	port_ordinary_project_report(state);
	state->complete = true;
	return true;
}

bool
yt_computer_port_earth_run(struct yt_computer_port_earth_state *state,
    yt_computer_port_earth_report_fn report, void *context,
    struct yt_error *error)
{
	if (state == NULL || report == NULL)
		return false;
	state->report_calls = 1U;
	state->report_returned = false;
	state->complete = false;
	if (!report(context, state, error))
		return false;
	state->report_returned = true;
	state->report_seen = false;
	state->complete = true;
	return true;
}

static bool
port_report_append(uint8_t *row, size_t capacity, size_t *position,
    const void *text, size_t length)
{
	if (*position > capacity || length > capacity - *position
	    || (length != 0U && text == NULL))
		return false;
	if (length != 0U)
		memcpy(row + *position, text, length);
	*position += length;
	return true;
}

static bool
port_report_field_length(const uint8_t raw[4], uint8_t conversion_mode,
    size_t *length, struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted;

	converted = qb_cint_mbf32(raw, conversion_mode, &overflow);
	if (overflow || converted < 0)
		return startup_configuration_error(error, YT_RANGE, operation);
	*length = (size_t)converted;
	if (*length > YT_TEXT_FIELD_SIZE)
		*length = YT_TEXT_FIELD_SIZE;
	return true;
}

static bool
port_report_present(struct yt_port_report_state *state,
    const struct yt_port_report_ops *ops, void *context,
    const uint8_t *text, size_t length,
    enum yt_port_report_output_kind kind, size_t item,
    struct yt_error *error)
{
	if (!ops->present(context, text, length, kind, item, error))
		return false;
	++state->output_count;
	return true;
}

static bool
port_report_right_raw(const uint8_t *source, size_t source_length,
    size_t width, uint8_t *rendered)
{
	size_t amount = source_length < width ? source_length : width;
	size_t padding = width - amount;

	if (source == NULL || rendered == NULL)
		return false;
	memset(rendered, ' ', padding);
	memcpy(rendered + padding, source + source_length - amount, amount);
	return true;
}

bool
yt_port_report_run(struct yt_port_report_state *state,
    const struct yt_port_report_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x01, 0x00};
	static const uint8_t title_prefix[] = "Commerce report for ";
	static const uint8_t title_separator[] = ": ";
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	static const uint8_t commodity[3][14] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	static const uint8_t buying[] = "  Buying ";
	static const uint8_t selling[] = "  Selling";
	static const uint8_t padding[] = "    ";
	static const size_t hold_offset[3] = {YT_F69, YT_F73, YT_F77};
	uint8_t date[10];
	uint8_t time_text[8];
	uint8_t row[256];
	uint8_t promoted_hold[8];
	uint8_t floored_capacity[8];
	uint8_t aligned[12];
	char number[96];
	int formatted_length;
	size_t position;
	size_t name_length;
	size_t number_length;
	size_t index;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->read_port == NULL || ops->observe_date == NULL
	    || ops->observe_time == NULL || ops->present == NULL
	    || ops->reset_pager == NULL || ops->set_bold == NULL
	    || ops->set_foreground == NULL)
		return false;
	memset(&state->owner_player, 0, sizeof(state->owner_player));
	memset(&state->current_player, 0, sizeof(state->current_player));
	memset(&state->report_port, 0, sizeof(state->report_port));
	memcpy(state->pager_line_count_raw, dirty_zero, sizeof(dirty_zero));
	state->owner_record = 0;
	state->owner_kind = YT_PORT_OWNER_SILENT;
	state->foreground = 0.0f;
	state->bold = 0.0f;
	state->output_count = 0U;
	state->completed_items = 0U;
	state->pager_reset = false;
	state->owner_player_read = false;
	state->current_player_read = false;
	state->report_port_read = false;
	state->date_observed = false;
	state->time_observed = false;
	state->complete = false;

	ops->reset_pager(context, state->pager_line_count_raw);
	state->pager_reset = true;
	state->owner_kind = yt_port_owner_classify(state->market.port.owner,
	    state->current_player_record, &state->owner_record);
	if (state->owner_kind == YT_PORT_OWNER_INVALID)
		return startup_configuration_error(error, YT_RANGE,
		    "port owner record conversion");
	if (state->owner_kind != YT_PORT_OWNER_SILENT) {
		const uint8_t *owner_name = NULL;
		size_t owner_name_length = 0U;

		if (state->owner_kind == YT_PORT_OWNER_OTHER) {
			if (!ops->read_player(context,
			    (uint32_t)state->owner_record, &state->owner_player,
			    error))
				return false;
			state->owner_player_read = true;
			if (!port_report_field_length(
			    state->owner_player.record.bytes + YT_F85,
			    state->conversion_mode, &owner_name_length, error,
			    "port owner name length"))
				return false;
			owner_name = state->owner_player.record.bytes;
		}
		if (!yt_port_owner_compose(state->owner_kind,
		    state->market.port.treasury, owner_name, owner_name_length,
		    row, sizeof(row), &position))
			return startup_configuration_error(error, YT_RANGE,
			    "port owner row composition");
		if (!port_report_present(state, ops, context, NULL, 0U,
		    YT_PORT_REPORT_OWNER_BLANK, SIZE_MAX, error)
		    || !port_report_present(state, ops, context, row, position,
		    YT_PORT_REPORT_OWNER_ROW, SIZE_MAX, error))
			return false;
	}
	if (!ops->read_player(context, (uint32_t)state->current_player_record,
	    &state->current_player, error))
		return false;
	state->current_player_read = true;
	if (!ops->read_port(context, state->port_physical_record,
	    &state->report_port, error))
		return false;
	state->report_port_read = true;
	if (!port_report_field_length(state->report_port.record.bytes + YT_F85,
	    state->conversion_mode, &name_length, error,
	    "port report name length"))
		return false;
	if (!ops->observe_date(context, date, error))
		return false;
	state->date_observed = true;
	if (!ops->observe_time(context, time_text, error))
		return false;
	state->time_observed = true;
	position = 0U;
	if (!port_report_append(row, sizeof(row), &position, title_prefix,
	    sizeof(title_prefix) - 1U)
	    || !port_report_append(row, sizeof(row), &position,
	    state->report_port.record.bytes, name_length)
	    || !port_report_append(row, sizeof(row), &position, title_separator,
	    sizeof(title_separator) - 1U)
	    || !port_report_append(row, sizeof(row), &position, date,
	    sizeof(date))
	    || !port_report_append(row, sizeof(row), &position, " ", 1U)
	    || !port_report_append(row, sizeof(row), &position, time_text,
	    sizeof(time_text)))
		return startup_configuration_error(error, YT_RANGE,
		    "port report title composition");
	if (!port_report_present(state, ops, context, NULL, 0U,
	    YT_PORT_REPORT_TITLE_BLANK, SIZE_MAX, error)
	    || !port_report_present(state, ops, context, row, position,
	    YT_PORT_REPORT_TITLE, SIZE_MAX, error)
	    || !port_report_present(state, ops, context, NULL, 0U,
	    YT_PORT_REPORT_HEADER_BLANK, SIZE_MAX, error)
	    || !port_report_present(state, ops, context, header,
	    sizeof(header) - 1U, YT_PORT_REPORT_HEADER, SIZE_MAX, error))
		return false;
	state->bold = 1.0f;
	ops->set_bold(context, state->bold);
	if (!port_report_present(state, ops, context, rule, sizeof(rule) - 1U,
	    YT_PORT_REPORT_RULE, SIZE_MAX, error))
		return false;
	for (index = 0U; index < 3U; ++index) {
		const uint8_t *status;

		if (state->market.port.factor[index] < 0.0f) {
			status = buying;
			state->foreground = 3.0f;
		}
		else {
			status = selling;
			state->foreground = 2.0f;
		}
		ops->set_foreground(context, state->foreground);
		position = 0U;
		if (!port_report_append(row, sizeof(row), &position,
		    commodity[index], sizeof(commodity[index]) - 1U)
		    || !port_report_append(row, sizeof(row), &position, status,
		    sizeof(buying) - 1U)
		    || !port_report_present(state, ops, context, row, position,
		    YT_PORT_REPORT_ITEM_NAME_STATUS, index, error))
			return false;
		if (qb_mbf64_floor_raw(state->market.capacity_raw[index],
		    floored_capacity) != QB_MBF_OK)
			return startup_configuration_error(error, YT_RANGE,
			    "port report stock INT");
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    floored_capacity);
		if (formatted_length < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "port report stock formatting");
		number_length = (size_t)formatted_length;
		if (!port_report_right_raw((const uint8_t *)number,
		    number_length, 12U, aligned)
		    || !port_report_present(state, ops, context, aligned, 12U,
		    YT_PORT_REPORT_ITEM_CAPACITY, index, error))
			return false;
		market_promote_single(state->current_player.record.bytes
		    + hold_offset[index], promoted_hold);
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    promoted_hold);
		if (formatted_length < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "port report hold formatting");
		number_length = (size_t)formatted_length;
		if (!port_report_right_raw((const uint8_t *)number,
		    number_length, 11U, aligned)
		    || !port_report_present(state, ops, context, aligned, 11U,
		    YT_PORT_REPORT_ITEM_HOLD, index, error))
			return false;
		formatted_length = qb_str_mbf32(number, sizeof(number),
		    state->market.price_raw[index]);
		if (formatted_length < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "port report price formatting");
		number_length = (size_t)formatted_length;
		position = 0U;
		if (!port_report_append(row, sizeof(row), &position, number,
		    number_length)
		    || !port_report_append(row, sizeof(row), &position, padding,
		    sizeof(padding) - 1U)
		    || !port_report_present(state, ops, context, row, position,
		    YT_PORT_REPORT_ITEM_PRICE, index, error))
			return false;
		++state->completed_items;
	}
	state->foreground = 3.0f;
	ops->set_foreground(context, state->foreground);
	state->complete = true;
	return true;
}

static bool
commodity_trade_present(struct yt_commodity_trade_state *state,
    const struct yt_commodity_trade_ops *ops, void *context,
    const void *text, size_t length, enum yt_commodity_trade_output_kind kind,
    struct yt_error *error)
{
	if (!ops->present(context, text, length, kind, error))
		return false;
	++state->output_count;
	return true;
}

static double
commodity_trade_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

static double
commodity_trade_double_div(double numerator, double denominator)
{
	volatile double result = numerator / denominator;

	return result;
}

static bool
commodity_trade_row(char *row, size_t capacity, const char *format,
    const char *first, const char *second, struct yt_error *error,
    const char *operation)
{
	int length = snprintf(row, capacity, format, first, second);

	if (length < 0 || (size_t)length >= capacity)
		return startup_configuration_error(error, YT_RANGE, operation);
	return true;
}

bool
yt_commodity_trade_run(struct yt_commodity_trade_state *state,
    const struct yt_commodity_trade_ops *ops, void *context,
    struct yt_error *error)
{
	static const char *const names[3] = {"Ore", "Organics", "Equipment"};
	static const uint8_t confirmation[] = "Do you agree? [Y/n] ";
	static const uint8_t never_mind[] = "Never mind!";
	static const uint8_t yours[] = "It's Yours!";
	static const uint8_t take[] = "We'll take them!";
	struct qb_val_result parsed;
	struct yt_player fresh_player;
	struct yt_port fresh_port;
	double credits;
	double free_double;
	uint8_t selected_quantity_raw[8];
	uint8_t floored_quantity_raw[8];
	uint8_t single_raw[4];
	uint8_t promoted_raw[8];
	float factor;
	float price;
	char first[64];
	char second[64];
	char row[256];
	char response[80];
	bool accepted;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->mutate_credits == NULL
	    || ops->read_port == NULL
	    || ops->write_port == NULL || ops->present == NULL
	    || ops->input == NULL || ops->confirm == NULL
	    || state->commodity >= 3U || state->current_player_record == 0U
	    || state->port_physical_record == 0U)
		return startup_configuration_error(error, YT_INVALID,
		    "commodity trade arguments");
	state->free_holds = 0.0f;
	state->maximum = 0.0f;
	state->quantity = 0.0f;
	state->total = 0.0f;
	state->direction = 0.0f;
	state->credit_delta = 0.0f;
	state->selected_quantity = 0.0;
	state->displayed_hold = 0.0;
	state->quantity_attempts = 0U;
	state->output_count = 0U;
	state->port_sells = false;
	state->prompt_reached = false;
	state->entry_player_read = false;
	state->treasury_port_read = false;
	state->treasury_port_written = false;
	state->credit_player_read = false;
	state->credit_player_written = false;
	state->hold_player_read = false;
	state->hold_player_written = false;
	state->stock_port_read = false;
	state->stock_port_written = false;
	state->complete = false;
	state->route = YT_COMMODITY_TRADE_INCOMPLETE;

	if (!ops->read_player(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->entry_player_read = true;
	memcpy(selected_quantity_raw,
	    state->market.capacity_raw[state->commodity],
	    sizeof(selected_quantity_raw));
	state->selected_quantity = qb_mbf64_decode(selected_quantity_raw);
	if (qb_mbf64_floor_raw(selected_quantity_raw, floored_quantity_raw)
	    != QB_MBF_OK)
		return startup_configuration_error(error, YT_RANGE,
		    "commodity trade cached quantity INT");
	state->displayed_hold = state->commodity == 0U
	    ? (double)state->player.ore : state->commodity == 1U
	    ? (double)state->player.organics : (double)state->player.equipment;
	factor = state->market.port.factor[state->commodity];
	price = state->market.price[state->commodity];
	credits = (double)state->player.credits;
	free_double = commodity_trade_double_sub((double)state->player.holds,
	    (double)state->player.ore);
	free_double = commodity_trade_double_sub(free_double,
	    (double)state->player.organics);
	free_double = commodity_trade_double_sub(free_double,
	    (double)state->player.equipment);
	state->free_holds = (float)free_double;
	state->port_sells = floorf(factor) > 0.0f;
	if (state->port_sells) {
		float required;

		state->maximum = state->free_holds;
		if (qb_mbf32_encode(state->maximum, single_raw) == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "commodity trade maximum MBF32");
		market_promote_single(single_raw, promoted_raw);
		if (market_compare(promoted_raw, floored_quantity_raw) > 0) {
			if (qb_mbf32_from_mbf64_raw(floored_quantity_raw, single_raw)
			    == QB_MBF_OVERFLOW)
				return startup_configuration_error(error, YT_RANGE,
				    "commodity trade cached quantity CSNG");
			state->maximum = qb_mbf32_decode(single_raw);
		}
		required = floorf(market_single_mul(price, state->maximum));
		if ((double)required > credits) {
			double affordable;

			if (price == 0.0f)
				return startup_configuration_error(error, YT_RANGE,
				    "commodity trade credit/price division");
			affordable = commodity_trade_double_div(credits,
			    (double)price);
			if (!isfinite(affordable) || floor(affordable) > FLT_MAX
			    || floor(affordable) < -FLT_MAX)
				return startup_configuration_error(error, YT_RANGE,
				    "commodity trade affordable CSNG");
			state->maximum = (float)floor(affordable);
		}
	}
	else {
		if (qb_mbf32_from_mbf64_raw(floored_quantity_raw, single_raw)
		    == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "commodity trade cached quantity CSNG");
		state->maximum = qb_mbf32_decode(single_raw);
		if ((double)state->maximum > floor(state->displayed_hold))
			state->maximum = (float)floor(state->displayed_hold);
	}
	if (state->maximum == 0.0f) {
		state->route = YT_COMMODITY_TRADE_MAXIMUM_ZERO;
		state->complete = true;
		return true;
	}

	if (qb_str_double(first, sizeof(first), credits) < 0
	    || qb_str_single(second, sizeof(second), state->free_holds) < 0
	    || !commodity_trade_row(row, sizeof(row),
	    "You have%s credits and%s empty cargo holds.", first, second,
	    error, "commodity trade status composition")
	    || !commodity_trade_present(state, ops, context, row, strlen(row),
	    YT_COMMODITY_TRADE_STATUS, error)
	    || qb_str_mbf64(first, sizeof(first), floored_quantity_raw) < 0
	    || qb_str_double(second, sizeof(second), state->displayed_hold) < 0
	    || !commodity_trade_row(row, sizeof(row),
	    state->port_sells
	    ? "We are selling up to%s.  You have%s in your holds."
	    : "We are buying up to%s.  You have%s in your holds.",
	    first, second, error, "commodity trade market composition")
	    || !commodity_trade_present(state, ops, context, row, strlen(row),
	    YT_COMMODITY_TRADE_MARKET, error))
		return false;

	for (;;) {
		if (qb_mbf64_from_u64(1U, state->caller_trade_flag_raw)
		    != QB_MBF_OK
		    || qb_str_single(first, sizeof(first), state->maximum) < 0
		    || snprintf(row, sizeof(row),
		    "How many holds of %s do you want to %s [%s ]? ",
		    names[state->commodity], state->port_sells ? "buy" : "sell",
		    first) < 0)
			return startup_configuration_error(error, YT_RANGE,
			    "commodity trade quantity prompt composition");
		state->prompt_reached = true;
		++state->quantity_attempts;
		if (!commodity_trade_present(state, ops, context, row, strlen(row),
		    YT_COMMODITY_TRADE_QUANTITY_PROMPT, error)
		    || !ops->input(context, response, sizeof(response), error))
			return false;
		if (strlen(response) > 4U)
			continue;
		if (response[0] == '\0')
			state->quantity = state->maximum;
		else {
			parsed = qb_val(response);
			if (parsed.overflow)
				return startup_configuration_error(error, YT_RANGE,
				    "commodity trade VAL");
			state->quantity = parsed.valid
			    ? (float)floor(parsed.value) : 0.0f;
		}
		if (state->quantity < 1.0f) {
			state->route = YT_COMMODITY_TRADE_QUANTITY_CANCEL;
			state->complete = true;
			return true;
		}
		if (qb_mbf32_encode(state->quantity, single_raw) == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "commodity trade quantity MBF32");
		market_promote_single(single_raw, promoted_raw);
		if (market_compare(promoted_raw, selected_quantity_raw) > 0) {
			const char *message = state->port_sells
			    ? "We don't have that much!"
			    : "We don't need that much!";

			if (!commodity_trade_present(state, ops, context, message,
			    strlen(message), YT_COMMODITY_TRADE_CAPACITY_ERROR,
			    error))
				return false;
			state->route = YT_COMMODITY_TRADE_CAPACITY_REJECTED;
			state->complete = true;
			return true;
		}
		if (state->port_sells && state->quantity > state->free_holds) {
			static const uint8_t message[] =
			    "You don't have enough cargo holds.";

			if (!commodity_trade_present(state, ops, context, message,
			    sizeof(message) - 1U,
			    YT_COMMODITY_TRADE_FREE_HOLDS_ERROR, error)
			    || !commodity_trade_present(state, ops, context, NULL, 0U,
			    YT_COMMODITY_TRADE_FREE_HOLDS_BLANK, error))
				return false;
			continue;
		}
		if (state->quantity > state->maximum) {
			const char *message = state->port_sells
			    ? "You can't afford that much!"
			    : "You don't have that much!";

			if (!commodity_trade_present(state, ops, context, message,
			    strlen(message), YT_COMMODITY_TRADE_MAXIMUM_ERROR,
			    error))
				return false;
			state->route = YT_COMMODITY_TRADE_MAXIMUM_REJECTED;
			state->complete = true;
			return true;
		}
		if (state->port_sells
		    && market_compare(promoted_raw, floored_quantity_raw) > 0) {
			static const uint8_t message[] =
			    "We're not selling that many.";

			if (!commodity_trade_present(state, ops, context, message,
			    sizeof(message) - 1U,
			    YT_COMMODITY_TRADE_NOT_SELLING_ERROR, error))
				return false;
			continue;
		}
		if (!state->port_sells
		    && market_compare(promoted_raw, floored_quantity_raw) > 0) {
			static const uint8_t message[] =
			    "We don't want that many.";

			if (!commodity_trade_present(state, ops, context, message,
			    sizeof(message) - 1U,
			    YT_COMMODITY_TRADE_DONT_WANT_ERROR, error))
				return false;
			continue;
		}
		if (!state->port_sells
		    && (double)state->quantity > state->displayed_hold) {
			static const uint8_t message[] =
			    "You don't have that much!";

			if (!commodity_trade_present(state, ops, context, message,
			    sizeof(message) - 1U,
			    YT_COMMODITY_TRADE_PLAYER_AMOUNT_ERROR, error))
				return false;
			continue;
		}
		break;
	}

	state->total = floorf(market_single_add(
	    market_single_mul(price, state->quantity), 0.5f));
	if (qb_str_single(first, sizeof(first), state->quantity) < 0
	    || snprintf(row, sizeof(row), "Agreed,%s units.", first) < 0
	    || !commodity_trade_present(state, ops, context, row, strlen(row),
	    YT_COMMODITY_TRADE_AGREED, error)
	    || qb_str_single(first, sizeof(first), state->total) < 0
	    || snprintf(row, sizeof(row), "We'll %s them for%s credits.",
	    state->port_sells ? "sell" : "buy", first) < 0
	    || !commodity_trade_present(state, ops, context, row, strlen(row),
	    YT_COMMODITY_TRADE_OFFER, error)
	    || !ops->confirm(context, confirmation, sizeof(confirmation) - 1U,
	    &accepted, error))
		return false;
	if (!accepted) {
		if (!commodity_trade_present(state, ops, context, never_mind,
		    sizeof(never_mind) - 1U, YT_COMMODITY_TRADE_DECLINED, error))
			return false;
		state->route = YT_COMMODITY_TRADE_DECLINED_ROUTE;
		state->complete = true;
		return true;
	}
	if (!commodity_trade_present(state, ops, context,
	    state->port_sells ? yours : take,
	    state->port_sells ? sizeof(yours) - 1U : sizeof(take) - 1U,
	    YT_COMMODITY_TRADE_SUCCESS, error))
		return false;

	if (state->port_sells && state->market.port.owner != 0.0f) {
		float receipt = state->total;

		if (state->market.port.owner == (float)state->current_player_record)
			receipt = floorf(market_single_mul(
			    0.009999999776482582f, state->total));
		if (!ops->read_port(context, state->port_physical_record,
		    &fresh_port, error))
			return false;
		state->treasury_port_read = true;
		yt_trade_treasury_overlay(&fresh_port, receipt);
		state->port = fresh_port;
		if (!ops->write_port(context, state->port_physical_record,
		    &state->port, error))
			return false;
		state->treasury_port_written = true;
	}
	factor = state->market.port.factor[state->commodity];
	state->direction = factor > 0.0f ? 1.0f
	    : factor < 0.0f ? -1.0f : 0.0f;
	state->credit_delta = -market_single_mul(state->total, state->direction);
	{
		bool hydrated = false;

		if (!ops->mutate_credits(context,
		    (float)state->current_player_record, state->credit_delta,
		    &fresh_player, &hydrated, error)) {
			state->credit_player_read = hydrated;
			if (hydrated)
				state->player = fresh_player;
			return false;
		}
		state->credit_player_read = true;
	}
	state->player = fresh_player;
	state->credit_player_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &fresh_player, error))
		return false;
	state->hold_player_read = true;
	yt_trade_holds_overlay(&fresh_player, state->commodity,
	    state->quantity, state->direction);
	state->player = fresh_player;
	if (!ops->write_player(context, state->current_player_record,
	    &state->player, error))
		return false;
	state->hold_player_written = true;
	if (!ops->read_port(context, state->port_physical_record,
	    &fresh_port, error))
		return false;
	state->stock_port_read = true;
	if (qb_mbf32_encode(state->quantity, single_raw) == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "commodity trade stock quantity MBF32");
	market_promote_single(single_raw, promoted_raw);
	market_negate(promoted_raw);
	if (qb_mbf64_add_raw(selected_quantity_raw, promoted_raw,
	    promoted_raw) != QB_MBF_OK
	    || qb_mbf32_from_mbf64_raw(promoted_raw, single_raw)
	    == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&fresh_port.record,
	    YT_F49 + state->commodity * 4U, single_raw))
		return startup_configuration_error(error, YT_RANGE,
		    "commodity trade stock overlay");
	fresh_port.stock[state->commodity] = qb_mbf32_decode(single_raw);
	state->port = fresh_port;
	if (!ops->write_port(context, state->port_physical_record,
	    &state->port, error))
		return false;
	state->stock_port_written = true;
	state->route = YT_COMMODITY_TRADE_ACCEPTED;
	state->complete = true;
	return true;
}

bool
yt_ordinary_commerce_run(struct yt_ordinary_commerce_state *state,
    const struct yt_ordinary_commerce_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t refusal_prefix[] =
	    "We don't want your goods and you can't buy ours ";
	static const uint8_t refusal_suffix[] = "!";
	char credits[64];
	char free_holds[64];
	uint8_t row[256];
	double free;
	int length;
	size_t index;

	if (state == NULL || ops == NULL || ops->update == NULL
	    || ops->report == NULL || ops->trade == NULL
	    || ops->read_player == NULL || ops->present == NULL
	    || ops->set_foreground == NULL
	    || state->current_player_record == 0U
	    || (state->first_name_length != 0U && state->first_name == NULL))
		return startup_configuration_error(error, YT_INVALID,
		    "ordinary commerce arguments");
	memset(&state->market, 0, sizeof(state->market));
	memset(&state->final_player, 0, sizeof(state->final_player));
	memset(state->schedule, 0, sizeof(state->schedule));
	state->scheduled_count = 0U;
	state->completed_trades = 0U;
	state->prompt_reached = false;
	state->update_complete = false;
	state->report_complete = false;
	state->refusal_presented = false;
	state->final_player_read = false;
	state->status_presented = false;
	state->complete = false;

	if (!ops->update(context, state->sector_number,
	    state->sector_record_expression, &state->market, error))
		return false;
	state->update_complete = true;
	if (!ops->report(context, &state->market, error))
		return false;
	state->report_complete = true;
	for (index = 0U; index < 3U; ++index) {
		bool reached = false;

		if (!(state->market.port.factor[index] < 0.0f))
			continue;
		state->schedule[state->scheduled_count++] = index;
		if (!ops->trade(context, &state->market, index, &reached, error))
			return false;
		if (reached)
			state->prompt_reached = true;
		++state->completed_trades;
	}
	for (index = 0U; index < 3U; ++index) {
		bool reached = false;

		if (!(state->market.port.factor[index] > 0.0f))
			continue;
		state->schedule[state->scheduled_count++] = index;
		if (!ops->trade(context, &state->market, index, &reached, error))
			return false;
		if (reached)
			state->prompt_reached = true;
		++state->completed_trades;
	}
	if (!state->prompt_reached) {
		size_t position = 0U;

		ops->set_foreground(context, 6.0f);
		if (!port_report_append(row, sizeof(row), &position,
		    refusal_prefix, sizeof(refusal_prefix) - 1U)
		    || !port_report_append(row, sizeof(row), &position,
		    state->first_name, state->first_name_length)
		    || !port_report_append(row, sizeof(row), &position,
		    refusal_suffix, sizeof(refusal_suffix) - 1U))
			return startup_configuration_error(error, YT_RANGE,
			    "ordinary commerce refusal composition");
		if (!ops->present(context, row, position,
		    YT_ORDINARY_COMMERCE_REFUSAL, error))
			return false;
		state->refusal_presented = true;
	}
	if (!ops->read_player(context, state->current_player_record,
	    &state->final_player, error))
		return false;
	state->final_player_read = true;
	free = commodity_trade_double_sub((double)state->final_player.holds,
	    (double)state->final_player.ore);
	free = commodity_trade_double_sub(free,
	    (double)state->final_player.organics);
	free = commodity_trade_double_sub(free,
	    (double)state->final_player.equipment);
	if (qb_str_double(credits, sizeof(credits),
	    (double)state->final_player.credits) < 0
	    || qb_str_double(free_holds, sizeof(free_holds), free) < 0)
		return startup_configuration_error(error, YT_RANGE,
		    "ordinary commerce status formatting");
	length = snprintf((char *)row, sizeof(row),
	    "You have%s credits and%s empty cargo holds.",
	    credits, free_holds);
	if (length < 0 || (size_t)length >= sizeof(row))
		return startup_configuration_error(error, YT_RANGE,
		    "ordinary commerce status composition");
	if (!ops->present(context, row, (size_t)length,
	    YT_ORDINARY_COMMERCE_STATUS, error))
		return false;
	state->status_presented = true;
	state->complete = true;
	return true;
}

bool
yt_port_docking_run(struct yt_port_docking_state *state,
    const struct yt_port_docking_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t label[] = "<Port>";
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t docking[] = "Docking, ";
	bool returned;

	if (state == NULL || ops == NULL || ops->present == NULL
	    || ops->set_foreground == NULL || ops->turn_gate == NULL
	    || ops->read_sector == NULL || ops->finalize == NULL
	    || ops->read_port == NULL || ops->earth == NULL
	    || ops->ordinary == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "port docking arguments");
	state->gate_sector = 0.0f;
	state->gate_sector_record_expression = 0.0f;
	state->gate_sector_physical_record = 0U;
	memset(&state->sector, 0, sizeof(state->sector));
	state->logical_port = 0.0f;
	state->selected_port_expression = 0.0f;
	state->selected_port_physical_record = 0U;
	memset(&state->selected_port, 0, sizeof(state->selected_port));
	state->post_finalizer_sector = 0.0f;
	state->post_finalizer_sector_record_expression = 0.0f;
	state->label_presented = false;
	state->foreground_selected = false;
	state->gate_complete = false;
	state->gate_denied = false;
	state->sector_read = false;
	state->no_port_presented = false;
	state->docking_blank_presented = false;
	state->docking_prefix_presented = false;
	state->finalizer_complete = false;
	state->selected_port_read = false;
	state->child_complete = false;
	state->reenter_sector = false;
	state->complete = false;
	state->route = YT_PORT_DOCKING_INCOMPLETE;

	if (!ops->present(context, label, sizeof(label) - 1U,
	    YT_PORT_DOCKING_LABEL, error))
		return false;
	state->label_presented = true;
	ops->set_foreground(context, 3.0f);
	state->foreground_selected = true;
	if (!ops->turn_gate(context, &state->gate_denied,
	    &state->gate_sector, &state->gate_sector_record_expression, error))
		return false;
	state->gate_complete = true;
	if (state->gate_denied) {
		state->route = YT_PORT_DOCKING_GATE_DENIED;
		state->reenter_sector = true;
		state->complete = true;
		return true;
	}
	state->gate_sector_physical_record = qb_brun_random_record_number(
	    state->gate_sector_record_expression);
	if (state->gate_sector_physical_record == 0U)
		return startup_configuration_error(error, YT_RANGE,
		    "port docking sector record conversion");
	if (!ops->read_sector(context, state->gate_sector_physical_record,
	    &state->sector, error))
		return false;
	state->sector_read = true;
	state->logical_port = state->sector.port;
	state->selected_port_expression = yt_port_selected_expression(
	    state->port_offset, state->logical_port);
	{
		uint8_t selected_raw[4];
		enum qb_mbf_status status = qb_mbf32_encode(
		    state->selected_port_expression, selected_raw);

		if (status == QB_MBF_OVERFLOW)
			return startup_configuration_error(error, YT_RANGE,
			    "port docking selected expression add");
		state->selected_port_expression = status == QB_MBF_UNDERFLOW
		    ? 0.0f : qb_mbf32_decode(selected_raw);
	}
	if (yt_port_link_missing(state->logical_port)) {
		if (!ops->present(context, no_port, sizeof(no_port) - 1U,
		    YT_PORT_DOCKING_NO_PORT, error))
			return false;
		state->no_port_presented = true;
		state->route = YT_PORT_DOCKING_NO_PORT_ROUTE;
		state->reenter_sector = true;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, NULL, 0U,
	    YT_PORT_DOCKING_LEADING_BLANK, error))
		return false;
	state->docking_blank_presented = true;
	if (!ops->present(context, docking, sizeof(docking) - 1U,
	    YT_PORT_DOCKING_PREFIX, error))
		return false;
	state->docking_prefix_presented = true;
	returned = false;
	if (!ops->finalize(context, &returned,
	    &state->post_finalizer_sector,
	    &state->post_finalizer_sector_record_expression, error))
		return false;
	state->finalizer_complete = true;
	if (!returned) {
		state->route = YT_PORT_DOCKING_FINALIZER_TERMINAL;
		state->complete = true;
		return true;
	}
	state->selected_port_physical_record = qb_brun_random_record_number(
	    state->selected_port_expression);
	if (state->selected_port_physical_record == 0U)
		return startup_configuration_error(error, YT_RANGE,
		    "port docking selected record conversion");
	if (!ops->read_port(context, state->selected_port_physical_record,
	    &state->selected_port, error))
		return false;
	state->selected_port_read = true;
	if (state->post_finalizer_sector == 1.0f) {
		if (!ops->earth(context, &state->reenter_sector, error))
			return false;
		state->route = YT_PORT_DOCKING_EARTH;
	}
	else {
		if (!ops->ordinary(context,
		    (int)state->post_finalizer_sector,
		    state->post_finalizer_sector_record_expression, error))
			return false;
		state->route = YT_PORT_DOCKING_ORDINARY;
		state->reenter_sector = true;
	}
	state->child_complete = true;
	state->complete = true;
	return true;
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

bool
yt_planet_transfer_fighter_amount(const char *response, float *amount,
    struct yt_error *error)
{
	struct qb_val_result parsed;
	uint8_t raw[4];
	volatile float candidate;
	enum qb_mbf_status status;

	if (response == NULL || amount == NULL)
		return startup_configuration_error(error, YT_INVALID,
		    "planet Transfer fighter amount arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return startup_configuration_error(error, YT_RANGE,
		    "planet Transfer fighter VAL");
	candidate = (float)(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return startup_configuration_error(error, YT_RANGE,
		    "planet Transfer fighter CSNG");
	*amount = status == QB_MBF_UNDERFLOW ? 0.0f : qb_mbf32_decode(raw);
	return true;
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

bool
yt_clearance_run(struct yt_clearance_state *state,
    const struct yt_clearance_ops *ops, void *context,
    struct yt_error *error)
{
	static const char *const name[4] = {
		"Holds", "Fighters", "Shields", "Ground Forces"
	};
	size_t index;

	if (state == NULL || ops == NULL || ops->random == NULL
	    || ops->present == NULL || ops->sound == NULL)
		return false;
	state->announced = false;
	state->current_item = 0U;
	state->items_completed = 0U;
	state->draws_consumed = 0U;
	state->announcements = 0U;
	state->leading_blank_presented = false;
	state->sound_called = false;
	state->trailing_blank_presented = false;
	state->complete = false;
	if (!ops->present(context, NULL, 0U, YT_CLEARANCE_LEADING_BLANK,
	    error))
		return false;
	state->leading_blank_presented = true;
	for (index = 0U; index < 4U; ++index) {
		float discount;
		float draw;
		char percent[64];
		char row[192];
		int row_length;

		state->current_item = index;
		if (!ops->random(context, &draw, error))
			return false;
		++state->draws_consumed;
		discount = state->discount[index];
		if (yt_clearance_candidate_needed(index, draw, discount,
		    state->create)) {
			if (!ops->random(context, &draw, error))
				return false;
			++state->draws_consumed;
			discount = draw;
			state->discount[index] = discount;
		}
		if (!yt_clearance_normalize(index, &discount)) {
			state->discount[index] = 0.0f;
		} else {
			state->discount[index] = discount;
			if (qb_str_single(percent, sizeof(percent),
			    yt_clearance_percentage(discount)) < 0)
				return false;
			row_length = snprintf(row, sizeof(row),
			    "Special clearance sale! The Trader's Guild is selling "
			    "%s for%s%% off!", name[index], percent);
			if (row_length < 0 || (size_t)row_length >= sizeof(row)
			    || !ops->present(context, (const uint8_t *)row,
			    (size_t)row_length, YT_CLEARANCE_ANNOUNCEMENT, error))
				return false;
			state->announced = true;
			++state->announcements;
		}
		state->items_completed = index + 1U;
	}
	if (state->announced) {
		if (!ops->sound(context, 1.0f, error))
			return false;
		state->sound_called = true;
		if (!ops->present(context, NULL, 0U,
		    YT_CLEARANCE_TRAILING_BLANK, error))
			return false;
		state->trailing_blank_presented = true;
	}
	state->complete = true;
	return true;
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
	    || ops->sound == NULL || state->player_cache == NULL)
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

		if (overflow || !yt_player_cache_contains((int)converted)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "Anti-Cloak cache index");
			}
			return false;
		}
		if (yt_player_cache_value(state->player_cache, converted,
		    YT_PLAYER_CACHE_CLOAK) > 0.0f) {
			int32_t converted_length;
			size_t name_length;

			static const uint8_t zero[4] = {0};

			(void)yt_player_cache_set_raw(state->player_cache, converted,
			    YT_PLAYER_CACHE_CLOAK, zero);
			if (!ops->read_player(context, state->counter,
			    &state->field_player, error))
				return false;
			state->field_record = state->counter;
			if (state->field_player.killed_by == 0.0f) {
				converted_length = qb_cint_mbf32(
				    state->field_player.record.bytes + YT_F85,
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
yt_projectile_survivor_store_counterattack(int shooter, int player_record,
    int *counterattack, uint8_t raw[4])
{
	if (counterattack == NULL || raw == NULL
	    || !yt_projectile_survivor_sets_counterattack(shooter))
		return false;
	if (qb_mbf32_encode((float)player_record, raw) != QB_MBF_OK)
		return false;
	*counterattack = player_record;
	return true;
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
yt_projectile_sector_probe_run(struct yt_projectile_sector_probe_state *state,
    struct yt_error *error)
{
	bool overflow;

	if (state == NULL || state->sector == NULL
	    || state->player_cache == NULL)
		return false;
	state->presence = 0.0f;
	state->matched_player = 0.0f;
	if (state->sector->mines > 0.0f || state->sector->fighters > 0.0f
	    || state->sector->port > 0.0f || state->sector->planet > 0.0f)
		state->presence = 1.0f;
	state->counter = 2.0f;
	while (state->counter <= state->player_terminal) {
		int candidate = (int)qb_cint(state->counter, &overflow);

		if (overflow || !yt_player_cache_contains(candidate)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "projectile sector-probe cache index");
			}
			return false;
		}
		if (yt_player_cache_value(state->player_cache, candidate,
		    YT_PLAYER_CACHE_SECTOR) == state->hop
		    && (yt_player_cache_value(state->player_cache, candidate,
		    YT_PLAYER_CACHE_CLOAK) == 0.0f
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

	if (state == NULL || state->player_cache == NULL)
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

		if (overflow || !yt_player_cache_contains(candidate)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma player-dispatch cache index");
			}
			return false;
		}
		if (yt_player_cache_value(state->player_cache, candidate,
		    YT_PLAYER_CACHE_SECTOR) == state->sector
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
	if (qb_cint_mbf32(state->planet_link_raw, state->conversion_mode,
	    &overflow) != 0) {
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
	    || ops->save_foreground == NULL || ops->color == NULL
	    || ops->sound == NULL || ops->random == NULL || ops->news == NULL
	    || ops->present == NULL
	    || ops->restore_foreground == NULL)
		return false;
	memset(&state->persistence, 0, sizeof(state->persistence));
	state->destroyed_fighters = 0.0;
	state->destroyed_shields = 0.0f;
	state->remaining_fighters = 0.0;
	state->remaining_shields = 0.0f;
	state->route = YT_PROJECTILE_PLASMA_PLAYER_CONTINUE_DISPATCH;
	if (!ops->read_player(context, state->target, &player, error))
		return false;
	state->original_fighters = (double)player.fighters;
	state->original_shields = player.shields;
	ops->save_foreground(context, &state->saved_foreground);
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
	ops->restore_foreground(context, state->saved_foreground);

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
	static const uint8_t cache_zero[4] = {0x00, 0x00, 0x80, 0x00};
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
	    || state->player_cache == NULL
	    || ops->read_player == NULL
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
		if (!yt_player_cache_contains(state->shooter)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma killed self cache index");
			}
			return false;
		}
		*state->destroyed = true;
		(void)yt_player_cache_set_raw(state->player_cache, state->shooter,
		    YT_PLAYER_CACHE_SECTOR, cache_zero);
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
	else if (state->owner == -1.0f) {
		*state->xannor_provoker = state->shooter;
	}
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
	uint8_t scanner_raw[4];
	float counter = 1.0f;
	bool scanner_disabled = false;
	size_t iterations = 0U;

	if (target == NULL || remaining == NULL || draw == NULL
	    || result == NULL)
		return false;
	original_fighters = (double)target->fighters;
	original_shields = target->shields;
	saved_missiles = *remaining;
	memcpy(scanner_raw, target->record.bytes + YT_F113,
	    sizeof(scanner_raw));
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
		scanner = qb_cint_mbf32(scanner_raw, 0U, &overflow);
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
			memset(scanner_raw, 0, sizeof(scanner_raw));
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
	if (!yt_player_stored_name(player, stored, &stored_length, error))
		return false;
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

static float
direct_attack_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
direct_attack_single_div(float left, float right)
{
	volatile float result = left / right;

	return result;
}

static double
direct_attack_double_add(double left, double right)
{
	volatile double result = left + right;

	return result;
}

static double
direct_attack_double_sub(double left, double right)
{
	volatile double result = left - right;

	return result;
}

bool
yt_hostile_attack_surrender_run(struct yt_hostile_surrender_state *state,
    const struct yt_hostile_surrender_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t radio[] = "RADIO MESSAGE COMING IN!";
	static const uint8_t captain_prefix[] =
	    "This is the captain of the fighter group in sector";
	static const uint8_t wish[] = "WE WISH TO SURRENDER!!!";
	static const uint8_t prompt[] =
	    "Will you accept our surrender? [Y]/N -=>";
	static const uint8_t joined[] = " We join your forces!";
	static const uint8_t xannor_refusal[] =
	    "Whee fyte to the deeth hoo-man slyme!";
	static const uint8_t mercenary_prefix[] =
	    "We'll DIE before joining with a slyme like you ";
	static const uint8_t mercenary_suffix[] = "!";
	static const uint8_t news_middle_one[] = " fighters in sector";
	static const uint8_t news_middle_two[] = " surrendered to ";
	static const uint8_t count_suffix[] = " fighters surrendered!";
	uint8_t captain[160];
	uint8_t refusal[256];
	uint8_t news[320];
	uint8_t count[128];
	char sector_number[64];
	char surrendered_number[64];
	size_t position;
	size_t sector_length;
	size_t surrendered_length;
	enum yt_hostile_surrender_answer answer =
	    YT_HOSTILE_SURRENDER_ANSWER_NO;
	bool accepted = false;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->present == NULL || ops->sound == NULL
	    || ops->prompt == NULL || ops->append_news == NULL
	    || ops->cache_forces == NULL
	    || ops->mark_checked == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL))
		return false;
	state->fighter_owner = state->old_owner;
	state->deployed_remaining = state->deployed_fighters;
	state->checked = false;
	state->accepted = false;
	state->complete = false;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->ship_fighters = (double)state->current.fighters;
	state->owner_route = yt_hostile_surrender_route(state->old_owner);
	if (!ops->present(context, radio, sizeof(radio) - 1U,
	    YT_HOSTILE_SURRENDER_RADIO_ROW, error))
		return false;
	if (!ops->sound(context, YT_HOSTILE_SURRENDER_RADIO_SOUND, 4.0f,
	    error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    state->current.sector) < 0)
		return false;
	sector_length = strlen(sector_number);
	position = 0U;
	if (!direct_attack_append(captain, sizeof(captain), &position,
	    captain_prefix, sizeof(captain_prefix) - 1U)
	    || !direct_attack_append(captain, sizeof(captain), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !ops->present(context, captain, position,
	    YT_HOSTILE_SURRENDER_CAPTAIN_ROW, error))
		return false;

	switch (state->owner_route) {
	case YT_HOSTILE_SURRENDER_PLAYER:
		if (!ops->present(context, wish, sizeof(wish) - 1U,
		    YT_HOSTILE_SURRENDER_WISH_ROW, error)
		    || !ops->present(context, NULL, 0U,
		    YT_HOSTILE_SURRENDER_PROMPT_BLANK, error)
		    || !ops->prompt(context, prompt, sizeof(prompt) - 1U,
		    &answer, error))
			return false;
		if (answer != YT_HOSTILE_SURRENDER_ANSWER_NO
		    && answer != YT_HOSTILE_SURRENDER_ANSWER_YES
		    && answer != YT_HOSTILE_SURRENDER_ANSWER_EMPTY)
			return false;
		accepted = answer == YT_HOSTILE_SURRENDER_ANSWER_YES
		    || answer == YT_HOSTILE_SURRENDER_ANSWER_EMPTY;
		break;
	case YT_HOSTILE_SURRENDER_XANNOR:
		if (!ops->present(context, xannor_refusal,
		    sizeof(xannor_refusal) - 1U,
		    YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW, error))
			return false;
		if (!ops->sound(context, YT_HOSTILE_SURRENDER_XANNOR_SOUND, 5.0f,
		    error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_MERCENARY:
		position = 0U;
		if (!direct_attack_append(refusal, sizeof(refusal), &position,
		    mercenary_prefix, sizeof(mercenary_prefix) - 1U)
		    || !direct_attack_append(refusal, sizeof(refusal), &position,
		    state->real_first_name, state->real_first_name_length)
		    || !direct_attack_append(refusal, sizeof(refusal), &position,
		    mercenary_suffix, sizeof(mercenary_suffix) - 1U)
		    || !ops->present(context, refusal, position,
		    YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW, error))
			return false;
		if (!ops->sound(context, YT_HOSTILE_SURRENDER_MERCENARY_SOUND,
		    5.0f, error))
			return false;
		break;
	case YT_HOSTILE_SURRENDER_QUIET:
		break;
	}
	ops->mark_checked(context);
	state->checked = true;
	state->accepted = accepted;
	if (!accepted) {
		state->complete = true;
		return true;
	}
	if (!ops->present(context, joined, sizeof(joined) - 1U,
	    YT_HOSTILE_SURRENDER_JOINED_ROW, error))
		return false;
	if (!ops->sound(context, YT_HOSTILE_SURRENDER_JOINED_SOUND, 1.0f,
	    error))
		return false;
	state->surrendered_fighters = direct_attack_double_sub(
	    state->deployed_fighters, state->defender_loss);
	if (qb_str_double(surrendered_number, sizeof(surrendered_number),
	    state->surrendered_fighters) < 0)
		return false;
	surrendered_length = strlen(surrendered_number);
	position = 0U;
	if (!direct_attack_append(news, sizeof(news), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !direct_attack_append(news, sizeof(news), &position,
	    news_middle_one, sizeof(news_middle_one) - 1U)
	    || !direct_attack_append(news, sizeof(news), &position,
	    (const uint8_t *)sector_number, sector_length)
	    || !direct_attack_append(news, sizeof(news), &position,
	    news_middle_two, sizeof(news_middle_two) - 1U)
	    || !direct_attack_append(news, sizeof(news), &position,
	    state->cached_player_name, state->cached_player_name_length)
	    || !ops->append_news(context, news, position, error))
		return false;
	state->ship_fighters = direct_attack_double_add(
	    direct_attack_double_sub(direct_attack_double_sub(
	    (double)state->current.fighters, state->attacker_loss),
	    state->defender_loss), state->deployed_fighters);
	state->current.fighters = (float)state->ship_fighters;
	state->deployed_remaining = 0.0;
	state->fighter_owner = 0.0f;
	ops->cache_forces(context, state->ship_fighters,
	    state->deployed_remaining);
	position = 0U;
	if (!direct_attack_append(count, sizeof(count), &position,
	    (const uint8_t *)surrendered_number, surrendered_length)
	    || !direct_attack_append(count, sizeof(count), &position,
	    count_suffix, sizeof(count_suffix) - 1U)
	    || !ops->present(context, count, position,
	    YT_HOSTILE_SURRENDER_COUNT_ROW, error))
		return false;
	state->complete = true;
	return true;
}

bool
yt_hostile_attack_persistence_run(
    struct yt_hostile_attack_persistence_state *state,
    const struct yt_hostile_attack_persistence_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t destroyed[] = " destroyed";
	static const uint8_t belonging[] = " fighters belonging to ";
	uint8_t news[320];
	char loss_number[64];
	size_t position = 0U;
	int loss_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->present_blank == NULL
	    || ops->append_news == NULL || ops->fatal == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->owner_label_length != 0U
	    && state->owner_label == NULL))
		return false;
	state->route = YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL;
	state->player_written = false;
	state->sector_written = false;
	state->post_loss_read = false;
	state->news_written = false;
	state->mercenaries_hurt = false;
	state->complete = false;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	yt_deployed_attack_player_overlay(&state->current, state->shields,
	    (float)state->ship_fighters);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	if (!ops->read_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	yt_deployed_attack_sector_overlay(&state->sector,
	    (float)state->deployed_fighters);
	if (!ops->write_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_written = true;
	if (state->ship_fighters < 1.0 && state->shields < 1.0f) {
		state->route = YT_HOSTILE_ATTACK_PERSISTENCE_FATAL;
		if (!ops->fatal(context, error))
			return false;
		state->complete = true;
		return true;
	}
	if (!ops->present_blank(context, error))
		return false;
	if (state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->post_loss_read = true;
		state->ship_fighters = (double)state->current.fighters;
		loss_length = qb_str_double(loss_number, sizeof(loss_number),
		    state->defender_loss);
		if (loss_length < 0
		    || !direct_attack_append(news, sizeof(news), &position,
		    state->cached_player_name,
		    state->cached_player_name_length)
		    || !direct_attack_append(news, sizeof(news), &position,
		    destroyed, sizeof(destroyed) - 1U)
		    || !direct_attack_append(news, sizeof(news), &position,
		    (const uint8_t *)loss_number, (size_t)loss_length)
		    || !direct_attack_append(news, sizeof(news), &position,
		    belonging, sizeof(belonging) - 1U)
		    || !direct_attack_append(news, sizeof(news), &position,
		    state->owner_label, state->owner_label_length)
		    || !ops->append_news(context, news, position, error))
			return false;
		state->news_written = true;
		state->mercenaries_hurt = state->old_owner == -2.0f;
	}
	state->complete = true;
	return true;
}

bool
yt_hostile_attack_tail_run(struct yt_hostile_attack_tail_state *state,
    const struct yt_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error)
{
	uint8_t display[240];
	uint8_t news[300];
	uint8_t defeated[160];
	size_t display_length;
	size_t news_length;
	size_t defeated_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->append_news == NULL || ops->clearance == NULL
	    || ops->random == NULL || ops->victory == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL))
		return false;
	state->bonus = 0.0f;
	state->player_read = false;
	state->player_written = false;
	state->reward_presented = false;
	state->reward_news_written = false;
	state->clearance_called = false;
	state->draw_consumed = false;
	state->defeated_presented = false;
	state->victory_called = false;
	state->complete = false;
	if (state->old_owner == -1.0f && state->defender_loss > 0.0) {
		if (!ops->read_player(context, state->current_player_record,
		    &state->current, error))
			return false;
		state->player_read = true;
		state->ship_fighters = (double)state->current.fighters;
		state->bonus = yt_xannor_attack_bonus(state->defender_loss,
		    state->current.turns, state->turns_per_day);
		if (state->bonus >= 1.0f) {
			state->current.turns = direct_attack_single_add(
			    state->current.turns, state->bonus);
			(void)yt_record_set_number(&state->current.record, YT_F49,
			    state->current.turns);
			if (!ops->write_player(context, state->current_player_record,
			    &state->current, error))
				return false;
			state->player_written = true;
			if (!yt_xannor_attack_reward_rows(
			    state->cached_player_name,
			    state->cached_player_name_length, state->bonus,
			    state->defender_loss, display, sizeof(display),
			    &display_length, news, sizeof(news), &news_length)
			    || !ops->present(context, display, display_length,
			    YT_HOSTILE_ATTACK_TAIL_REWARD_ROW, error))
				return false;
			state->reward_presented = true;
			if (!ops->append_news(context, news, news_length, error))
				return false;
			state->reward_news_written = true;
			if (state->deployed_fighters < 1.0) {
				if (!ops->clearance(context, error))
					return false;
				state->clearance_called = true;
			}
		}
	}
	if (!ops->random(context, &state->dominated_draw, error))
		return false;
	state->draw_consumed = true;
	if (state->deployed_fighters <= 0.0) {
		if (!yt_hostile_defeated_row(state->ship_fighters, defeated,
		    sizeof(defeated), &defeated_length)
		    || !ops->present(context, defeated, defeated_length,
		    YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW, error))
			return false;
		state->defeated_presented = true;
		if (state->old_owner == -1.0f
		    && state->current.sector == state->headquarters) {
			if (!ops->victory(context, error))
				return false;
			state->victory_called = true;
		}
	}
	state->complete = true;
	return true;
}

bool
yt_hostile_attack_combat_run(struct yt_hostile_attack_combat_state *state,
    const struct yt_hostile_attack_combat_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t lost_prefix[] = " You lost";
	static const uint8_t lost_suffix[] = " fighter(s)";
	static const uint8_t destroyed_prefix[] = " You destroyed";
	static const uint8_t destroyed_suffix[] = " enemy fighters.";
	static const uint8_t exposed[] =
	    "Fighters gone! Enemy attacking your ship!";
	uint8_t lost_row[128];
	uint8_t destroyed_row[128];
	char attacker_number[64];
	char defender_number[64];
	size_t lost_length = 0U;
	size_t destroyed_length = 0U;
	int attacker_length;
	int defender_length;
	bool child_result;

	if (state == NULL || ops == NULL || ops->read_sector == NULL
	    || ops->read_player == NULL || ops->sound == NULL
	    || ops->random == NULL || ops->store_ship == NULL
	    || ops->surrender == NULL
	    || ops->present == NULL || ops->cache_player == NULL
	    || ops->cache_sector == NULL || ops->spill == NULL
	    || ops->persistence == NULL || ops->tail == NULL
	    || (state->cached_player_name_length != 0U
	    && state->cached_player_name == NULL)
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL)
	    || (state->owner_label_length != 0U
	    && state->owner_label == NULL))
		return false;
	state->route = YT_HOSTILE_ATTACK_COMBAT_NORMAL;
	state->complete = false;
	state->old_count = state->cached_defenders;
	state->old_ship = 0.0;
	state->attacker_loss = 0.0;
	state->defender_loss = 0.0;
	state->ship_fighters = 0.0;
	state->deployed_remaining = state->old_count;
	state->quantum = 0.0f;
	state->last_draw = 0.0f;
	state->iterations = 0U;
	state->surrender_checked = false;
	state->surrendered = false;
	state->spill_called = false;
	memset(&state->surrender, 0, sizeof(state->surrender));
	memset(&state->persistence, 0, sizeof(state->persistence));
	memset(&state->tail, 0, sizeof(state->tail));
	if (!ops->read_sector(context, state->current_sector,
	    &state->opened_sector, error))
		return false;
	state->old_owner = qb_mbf32_decode(
	    &state->opened_sector.record.bytes[YT_F85]);
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->old_ship = (double)state->current.fighters;
	if (!ops->sound(context, 2.0f, error))
		return false;
	do {
		double remaining_attacker = direct_attack_double_sub(
		    state->commitment, state->attacker_loss);
		double remaining_defender = direct_attack_double_sub(
		    state->old_count, state->defender_loss);
		volatile double ratio;

		state->quantum = yt_hostile_attack_quantum(remaining_attacker,
		    remaining_defender);
		if (remaining_defender == 0.0) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "attack:surrender-ratio-divide");
			}
			return false;
		}
		ratio = remaining_attacker / remaining_defender;
		if (!state->surrender_checked && state->allow_surrender
		    && ratio > 10.0) {
			state->surrender = (struct yt_hostile_surrender_state){
				.current_player_record = state->current_player_record,
				.old_owner = state->old_owner,
				.attacker_loss = state->attacker_loss,
				.defender_loss = state->defender_loss,
				.deployed_fighters = state->old_count,
				.cached_player_name = state->cached_player_name,
				.cached_player_name_length =
				    state->cached_player_name_length,
				.real_first_name = state->real_first_name,
				.real_first_name_length = state->real_first_name_length,
				.current = state->current,
			};
			child_result = ops->surrender(context, &state->surrender,
			    error);
			state->current = state->surrender.current;
			state->old_ship = state->surrender.ship_fighters;
			state->surrender_checked = state->surrender.checked;
			state->surrendered = state->surrender.accepted;
			ops->cache_player(context, &state->current);
			if (!child_result)
				return false;
			if (state->surrendered) {
				state->ship_fighters =
				    state->surrender.ship_fighters;
				state->deployed_remaining =
				    state->surrender.deployed_remaining;
				state->sector.fighter_owner =
				    state->surrender.fighter_owner;
				state->current.fighters =
				    (float)state->ship_fighters;
				ops->cache_player(context, &state->current);
				ops->cache_sector(context, &state->sector,
				    state->deployed_remaining);
				break;
			}
		}
		if (!ops->random(context, &state->last_draw, error))
			return false;
		if (yt_hostile_attack_loses_attacker(state->current.cloak,
		    state->last_draw)) {
			state->attacker_loss = direct_attack_double_add(
			    state->attacker_loss, (double)state->quantum);
		} else {
			state->defender_loss = direct_attack_double_add(
			    state->defender_loss, (double)state->quantum);
		}
		++state->iterations;
	} while (state->attacker_loss < state->commitment
	    && state->defender_loss < state->old_count);
	if (state->attacker_loss > state->commitment) {
		state->attacker_loss = state->commitment;
	}
	if (state->defender_loss > state->old_count) {
		state->defender_loss = state->old_count;
	}
	if (!state->surrendered) {
		state->ship_fighters = direct_attack_double_sub(state->old_ship,
		    state->attacker_loss);
		state->deployed_remaining = direct_attack_double_sub(
		    state->old_count, state->defender_loss);
		state->current.fighters = (float)state->ship_fighters;
		ops->store_ship(context, state->ship_fighters);
		ops->cache_player(context, &state->current);
	}
	attacker_length = qb_str_double(attacker_number,
	    sizeof(attacker_number), state->attacker_loss);
	defender_length = qb_str_double(defender_number,
	    sizeof(defender_number), state->defender_loss);
	if (attacker_length < 0 || defender_length < 0
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_prefix, sizeof(lost_prefix) - 1U)
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    attacker_number, (size_t)attacker_length)
	    || !direct_attack_append(lost_row, sizeof(lost_row), &lost_length,
	    lost_suffix, sizeof(lost_suffix) - 1U)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, destroyed_prefix,
	    sizeof(destroyed_prefix) - 1U)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, defender_number, (size_t)defender_length)
	    || !direct_attack_append(destroyed_row, sizeof(destroyed_row),
	    &destroyed_length, destroyed_suffix,
	    sizeof(destroyed_suffix) - 1U)
	    || !ops->present(context, NULL, 0U,
	    YT_HOSTILE_ATTACK_COMBAT_RESULT_BLANK, error)
	    || !ops->present(context, lost_row, lost_length,
	    YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW, error)
	    || !ops->present(context, destroyed_row, destroyed_length,
	    YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW, error))
		return false;
	if (state->ship_fighters < 1.0 && state->deployed_remaining > 0.0) {
		if (!ops->present(context, exposed, sizeof(exposed) - 1U,
		    YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW, error)
		    || !ops->present(context, NULL, 0U,
		    YT_HOSTILE_ATTACK_COMBAT_SPILL_BLANK, error))
			return false;
		if (!ops->spill(context, &state->deployed_remaining,
		    &state->current.shields, error)) {
			ops->cache_player(context, &state->current);
			return false;
		}
		state->spill_called = true;
		ops->cache_player(context, &state->current);
	}
	state->sector.fighters = (float)state->deployed_remaining;
	ops->cache_sector(context, &state->sector, state->deployed_remaining);
	state->persistence =
	    (struct yt_hostile_attack_persistence_state){
		.current_player_record = state->current_player_record,
		.current_sector = state->current_sector,
		.ship_fighters = state->ship_fighters,
		.shields = state->current.shields,
		.deployed_fighters = state->deployed_remaining,
		.defender_loss = state->defender_loss,
		.old_owner = state->old_owner,
		.cached_player_name = state->cached_player_name,
		.cached_player_name_length = state->cached_player_name_length,
		.owner_label = state->owner_label,
		.owner_label_length = state->owner_label_length,
		.current = state->current,
		.sector = state->sector,
	};
	child_result = ops->persistence(context, &state->persistence, error);
	state->current = state->persistence.current;
	state->ship_fighters = state->persistence.ship_fighters;
	if (state->persistence.sector_written) {
		state->sector = state->persistence.sector;
		ops->cache_sector(context, &state->sector,
		    state->deployed_remaining);
	}
	if (state->persistence.route == YT_HOSTILE_ATTACK_PERSISTENCE_FATAL)
		state->route = YT_HOSTILE_ATTACK_COMBAT_FATAL;
	if (!child_result)
		return false;
	if (state->route == YT_HOSTILE_ATTACK_COMBAT_FATAL) {
		state->complete = true;
		return true;
	}
	state->tail = (struct yt_hostile_attack_tail_state){
		.current_player_record = state->current_player_record,
		.old_owner = state->old_owner,
		.defender_loss = state->defender_loss,
		.deployed_fighters = state->deployed_remaining,
		.ship_fighters = state->ship_fighters,
		.turns_per_day = state->turns_per_day,
		.headquarters = state->headquarters,
		.cached_player_name = state->cached_player_name,
		.cached_player_name_length = state->cached_player_name_length,
		.current = state->current,
	};
	child_result = ops->tail(context, &state->tail, error);
	state->current = state->tail.current;
	state->ship_fighters = state->tail.ship_fighters;
	if (!child_result)
		return false;
	state->complete = true;
	return true;
}

bool
yt_hostile_bribe_accept_run(struct yt_hostile_bribe_accept_state *state,
    const struct yt_hostile_bribe_accept_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t deal[] = "Good Deal! We join up with you!";
	volatile double fighters;
	volatile double credits;

	if (state == NULL || ops == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->read_sector == NULL
	    || ops->write_sector == NULL || ops->read_player == NULL
	    || ops->write_player == NULL)
		return false;
	state->persisted_fighters = 0.0f;
	state->persisted_credits = 0.0f;
	state->deal_presented = false;
	state->sound_played = false;
	state->sector_read = false;
	state->sector_written = false;
	state->player_read = false;
	state->player_written = false;
	state->complete = false;
	memset(&state->sector, 0, sizeof(state->sector));
	memset(&state->current, 0, sizeof(state->current));
	if (!ops->present(context, deal, sizeof(deal) - 1U, error))
		return false;
	state->deal_presented = true;
	if (!ops->sound(context, 1.0f, error))
		return false;
	state->sound_played = true;
	if (!ops->read_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_read = true;
	yt_bribe_sector_overlay(&state->sector);
	if (!ops->write_sector(context, state->current_sector, &state->sector,
	    error))
		return false;
	state->sector_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_read = true;
	fighters = direct_attack_double_add((double)state->current.fighters,
	    (double)state->cached_defenders);
	credits = direct_attack_double_sub((double)state->current.credits,
	    (double)state->offer);
	state->persisted_fighters = (float)fighters;
	state->persisted_credits = (float)credits;
	yt_bribe_player_overlay(&state->current, state->persisted_fighters,
	    state->persisted_credits);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->player_written = true;
	state->complete = true;
	return true;
}

static bool
hostile_bribe_name_row(const uint8_t *prefix, size_t prefix_length,
    const uint8_t *name, size_t name_length, const uint8_t *suffix,
    size_t suffix_length, uint8_t *row, size_t capacity, size_t *length)
{
	size_t used = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (!direct_attack_append(row, capacity, &used, prefix, prefix_length)
	    || !direct_attack_append(row, capacity, &used, name, name_length)
	    || !direct_attack_append(row, capacity, &used, suffix,
	    suffix_length))
		return false;
	*length = used;
	return true;
}

static bool
hostile_bribe_force_run(struct yt_hostile_bribe_state *state,
    const struct yt_hostile_bribe_ops *ops, void *context,
    bool mercenary_fatal_gate, struct yt_error *error)
{
	enum qb_mbf_status conversion;
	float commitment = (float)state->ship_fighters;
	uint8_t raw[4];

	state->forced_attack = true;
	conversion = qb_mbf32_encode(commitment, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:commitment-csng");
		}
		return false;
	}
	state->commitment = qb_mbf32_decode(raw);
	switch (yt_bribe_forced_admit(state->ship_fighters, state->shields,
	    mercenary_fatal_gate, state->commitment)) {
	case YT_BRIBE_FORCED_FATAL:
		state->route = YT_HOSTILE_BRIBE_FATAL;
		state->fatal_called = true;
		if (!ops->fatal(context, error))
			return false;
		break;
	case YT_BRIBE_FORCED_LESS_THAN_ONE:
		state->direct_hostile_menu = true;
		state->route = YT_HOSTILE_BRIBE_HOSTILE_MENU;
		break;
	case YT_BRIBE_FORCED_ATTACK:
		state->route = YT_HOSTILE_BRIBE_COMBAT;
		state->combat_called = true;
		if (!ops->combat(context, (double)state->commitment, error))
			return false;
		break;
	default:
		return false;
	}
	state->complete = true;
	return true;
}

bool
yt_hostile_bribe_run(struct yt_hostile_bribe_state *state,
    const struct yt_hostile_bribe_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t ordinary_prefix[] =
	    "We don't accept no Bribes ";
	static const uint8_t planet_prefix[] = "Scram ";
	static const uint8_t planet_suffix[] = ", This planet is OURS!";
	static const uint8_t life_prefix[] =
	    "We just want your miserable life ";
	static const uint8_t introduction_prefix[] =
	    "We MAY join up if you pay us enough ";
	static const uint8_t rejected_prefix[] = "You insult us ";
	static const uint8_t rejected_suffix[] = "! Prepare to DIE!";
	static const uint8_t bang[] = "!";
	static const uint8_t prompt_prefix[] = "You have";
	static const uint8_t prompt_suffix[] =
	    " credits. How much do you offer? -+>";
	uint8_t row[512];
	uint8_t prompt[512];
	char credits[64];
	char response[4096] = {0};
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	uint8_t raw[4];
	size_t row_length;
	size_t prompt_length = 0U;
	int credits_length;
	bool force_attack;

	if (state == NULL || ops == NULL || ops->present == NULL
	    || ops->random == NULL || ops->amount == NULL
	    || ops->accept == NULL || ops->combat == NULL
	    || ops->fatal == NULL
	    || (state->real_first_name_length != 0U
	    && state->real_first_name == NULL))
		return false;
	memset(state->draws, 0, sizeof(state->draws));
	state->draws_consumed = 0U;
	state->offer = 0.0f;
	state->above_credits = false;
	state->threshold = 0.0;
	state->commitment = 0.0f;
	state->forced_attack = false;
	state->direct_hostile_menu = false;
	state->accepted_called = false;
	state->combat_called = false;
	state->fatal_called = false;
	memset(&state->accepted, 0, sizeof(state->accepted));
	state->branch = YT_HOSTILE_BRIBE_BRANCH_INCOMPLETE;
	state->route = YT_HOSTILE_BRIBE_INCOMPLETE;
	state->complete = false;

	if (state->owner != -2.0f) {
		state->branch = YT_HOSTILE_BRIBE_ORDINARY_REFUSAL;
		if (!hostile_bribe_name_row(ordinary_prefix,
		    sizeof(ordinary_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, bang, sizeof(bang) - 1U,
		    row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_ORDINARY_REFUSAL_ROW, error))
			return false;
		if (!ops->random(context, &state->draws[0], error))
			return false;
		state->draws_consumed = 1U;
		force_attack = yt_bribe_ordinary_forces(state->owner,
		    state->cached_defenders, state->ship_fighters,
		    state->draws[0]);
		if (!force_attack) {
			state->route = YT_HOSTILE_BRIBE_SCANNER;
			state->complete = true;
			return true;
		}
		return hostile_bribe_force_run(state, ops, context, false, error);
	}

	if (state->planet_link != 0.0f) {
		state->branch = YT_HOSTILE_BRIBE_PLANET_REFUSAL;
		if (!hostile_bribe_name_row(planet_prefix,
		    sizeof(planet_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, planet_suffix,
		    sizeof(planet_suffix) - 1U, row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW, error))
			return false;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}

	if (!ops->random(context, &state->draws[0], error))
		return false;
	state->draws_consumed = 1U;
	if (!ops->random(context, &state->draws[1], error))
		return false;
	state->draws_consumed = 2U;
	force_attack = yt_bribe_mercenary_forces(state->cached_defenders,
	    state->ship_fighters, state->draws[0], state->draws[1],
	    state->mercenaries_hurt);
	if (force_attack) {
		state->branch = YT_HOSTILE_BRIBE_LIFE_DEMAND;
		if (!hostile_bribe_name_row(life_prefix,
		    sizeof(life_prefix) - 1U, state->real_first_name,
		    state->real_first_name_length, bang, sizeof(bang) - 1U,
		    row, sizeof(row), &row_length)
		    || !ops->present(context, row, row_length,
		    YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW, error))
			return false;
		return hostile_bribe_force_run(state, ops, context, true, error);
	}

	if (!hostile_bribe_name_row(introduction_prefix,
	    sizeof(introduction_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, bang, sizeof(bang) - 1U,
	    row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_INTRODUCTION_ROW, error))
		return false;
	credits_length = qb_str_double(credits, sizeof(credits), state->credits);
	if (credits_length < 0
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_prefix, sizeof(prompt_prefix) - 1U)
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
	    credits, (size_t)credits_length)
	    || !direct_attack_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_suffix, sizeof(prompt_suffix) - 1U)
	    || !ops->present(context, prompt, prompt_length,
	    YT_HOSTILE_BRIBE_OFFER_PROMPT, error)
	    || !ops->amount(context, response, sizeof(response), error))
		return false;
	if (response[0] == '\0') {
		state->branch = YT_HOSTILE_BRIBE_EMPTY_OFFER;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}
	parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:VAL");
		}
		return false;
	}
	state->offer = (float)parsed.value;
	conversion = qb_mbf32_encode(state->offer, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:offer-csng");
		}
		state->offer = 0.0f;
		return false;
	}
	state->offer = qb_mbf32_decode(raw);
	state->above_credits = (double)state->offer > state->credits;
	if (!ops->random(context, &state->draws[2], error))
		return false;
	state->draws_consumed = 3U;
	state->threshold = yt_bribe_offer_threshold(state->cached_defenders,
	    state->draws[2]);
	if (!state->above_credits && yt_bribe_offer_accepted(state->offer,
	    state->credits, state->threshold)) {
		state->branch = YT_HOSTILE_BRIBE_ACCEPTED;
		state->accepted = (struct yt_hostile_bribe_accept_state){
			.current_player_record = state->current_player_record,
			.current_sector = state->current_sector,
			.cached_defenders = state->cached_defenders,
			.offer = state->offer,
		};
		state->accepted_called = true;
		if (!ops->accept(context, &state->accepted, error))
			return false;
		state->route = YT_HOSTILE_BRIBE_SCANNER;
		state->complete = true;
		return true;
	}

	state->branch = YT_HOSTILE_BRIBE_REJECTED;
	if (!hostile_bribe_name_row(rejected_prefix,
	    sizeof(rejected_prefix) - 1U, state->real_first_name,
	    state->real_first_name_length, rejected_suffix,
	    sizeof(rejected_suffix) - 1U, row, sizeof(row), &row_length)
	    || !ops->present(context, row, row_length,
	    YT_HOSTILE_BRIBE_REJECTED_ROW, error))
		return false;
	return hostile_bribe_force_run(state, ops, context, true, error);
}

bool
yt_direct_attack_attrition_run(
    struct yt_direct_attack_attrition_state *state,
    yt_direct_attack_attrition_draw_fn draw, void *context,
    struct yt_error *error)
{
	if (state == NULL || draw == NULL)
		return false;
	state->attacker_loss = 0.0;
	state->defender_loss = 0.0;
	state->quantum = 0.0f;
	state->iterations = 0U;
	state->complete = false;
	while (state->attacker_loss < state->committed
	    && state->defender_loss < state->defenders) {
		double remaining_attacker = state->committed
		    - state->attacker_loss;
		double remaining_defender = state->defenders
		    - state->defender_loss;
		double minimum = remaining_attacker < remaining_defender
		    ? remaining_attacker : remaining_defender;
		volatile double integral = floor(minimum / 20.0);
		float sampled;

		state->quantum = (float)integral;
		if (state->quantum < 1.0f)
			state->quantum = 1.0f;
		if (!draw(context, &sampled, error))
			return false;
		if (direct_attack_single_add(direct_attack_single_div(
		    state->cloak, 10.0f), sampled) < 0.44999998807907104f)
			state->attacker_loss = direct_attack_double_add(
			    state->attacker_loss, (double)state->quantum);
		else
			state->defender_loss = direct_attack_double_add(
			    state->defender_loss, (double)state->quantum);
		++state->iterations;
	}
	state->complete = true;
	return true;
}

bool
yt_direct_attack_combat_run(struct yt_direct_attack_combat_state *state,
    const struct yt_direct_attack_combat_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t eliminated[] =
	    "Fighters eliminated! Attacking the ship!";
	uint8_t row[300];
	uint8_t second[300];
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t radio[160];
	size_t row_length;
	size_t second_length;
	size_t name_length;
	size_t radio_length;
	float remaining_shields;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->write_player == NULL || ops->present == NULL
	    || ops->sound == NULL || ops->radio == NULL
	    || ops->random == NULL || ops->spill == NULL
	    || ops->kill == NULL)
		return false;
	state->route = YT_DIRECT_ATTACK_COMBAT_INCOMPLETE;
	state->reserve_written = false;
	state->current_casualty_written = false;
	state->target_casualty_written = false;
	state->target_shield_written = false;
	state->current_final_written = false;
	state->complete = false;
	state->defenders = 0.0;
	state->cached_reserve = 0.0;
	state->attacking = 0.0;
	state->target_shields = 0.0f;
	state->current_sector = 0.0f;
	memset(&state->attrition, 0, sizeof(state->attrition));

	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->defenders = (double)state->target.fighters;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	if (state->committed > (double)state->current.fighters) {
		if (!yt_direct_attack_too_many_row(
		    (double)state->current.fighters, row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length,
		    YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW, error))
			return false;
		state->route = YT_DIRECT_ATTACK_COMBAT_TOO_MANY;
		state->complete = true;
		return true;
	}

	state->cached_reserve = direct_attack_double_sub(
	    (double)state->current.fighters, state->committed);
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)state->cached_reserve);
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->reserve_written = true;
	if (!ops->sound(context, 2.0f, error))
		return false;

	state->attrition = (struct yt_direct_attack_attrition_state){
		.committed = state->committed,
		.defenders = state->defenders,
		.cloak = state->current.cloak,
	};
	if (!yt_direct_attack_attrition_run(&state->attrition, ops->random,
	    context, error))
		return false;
	if (state->attrition.defender_loss > 0.0) {
		if (!yt_player_stored_name(&state->current, stored_name,
		    &name_length, error)
		    || !yt_direct_attack_radio_text(stored_name, name_length,
		    state->attrition.defender_loss, radio, sizeof(radio),
		    &radio_length)
		    || !ops->radio(context, radio, radio_length,
		    (float)state->target_record, error))
			return false;
	}

	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->cached_reserve = (double)state->current.fighters;
	state->attacking = direct_attack_double_sub(state->committed,
	    state->attrition.attacker_loss);
	state->defenders = direct_attack_double_sub(state->defenders,
	    state->attrition.defender_loss);
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)direct_attack_double_add(state->cached_reserve,
	    state->attacking));
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->current_casualty_written = true;
	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->current_sector = state->current.sector;
	state->target_shields = state->target.shields;
	yt_direct_attack_fighter_overlay(&state->target,
	    (float)state->defenders);
	if (!ops->write_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->target_casualty_written = true;
	if (!yt_direct_attack_result_rows(state->attrition.attacker_loss,
	    state->cached_reserve, state->attrition.defender_loss,
	    state->defenders, row, sizeof(row), &row_length,
	    second, sizeof(second), &second_length)
	    || !ops->present(context, row, row_length,
	    YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW, error)
	    || !ops->present(context, second, second_length,
	    YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW, error))
		return false;
	if (state->defenders > 0.0 || state->attacking < 1.0) {
		state->route = YT_DIRECT_ATTACK_COMBAT_CASUALTY_RETURN;
		state->complete = true;
		return true;
	}
	if (!ops->present(context, eliminated, sizeof(eliminated) - 1U,
	    YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW, error))
		return false;
	if (state->target_shields > 0.0f
	    && !ops->spill(context, &state->attacking,
	    &state->target_shields, error))
		return false;

	remaining_shields = state->target_shields;
	if (!ops->read_player(context, state->target_record, &state->target,
	    error))
		return false;
	yt_direct_attack_shield_overlay(&state->target, remaining_shields);
	if (!ops->write_player(context, state->target_record, &state->target,
	    error))
		return false;
	state->target_shield_written = true;
	if (!ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	yt_direct_attack_fighter_overlay(&state->current,
	    (float)direct_attack_double_add(state->cached_reserve,
	    state->attacking));
	if (!ops->write_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	state->current_final_written = true;
	if (state->target_shields > 0.0f) {
		state->route = YT_DIRECT_ATTACK_COMBAT_SHIELD_RETURN;
		state->complete = true;
		return true;
	}
	if (!ops->kill(context, state->target_record,
	    state->current_player_record, state->current_sector,
	    state->target_shields, error))
		return false;
	state->route = YT_DIRECT_ATTACK_COMBAT_KILL_RETURN;
	state->complete = true;
	return true;
}

static bool
direct_attack_candidate_record(struct yt_direct_attack_state *state,
    int *record, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)state->candidate,
	    state->conversion_mode, &overflow);

	if (!overflow && yt_player_cache_contains((int)converted)) {
		*record = (int)converted;
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "direct Attack candidate cache CINT");
	}
	return false;
}

bool
yt_direct_attack_run(struct yt_direct_attack_state *state,
    const struct yt_direct_attack_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t no_fighters[] =
	    "You don't have any fighters.";
	static const uint8_t none_visible[] = "There's no one here!";
	static const uint8_t none_selected[] =
	    "There are no other ships in this sector.";
	uint8_t row[300];
	uint8_t target_name[YT_TEXT_FIELD_SIZE];
	char response[4096];
	size_t row_length;
	size_t target_name_length;

	if (state == NULL || ops == NULL || ops->read_player == NULL
	    || ops->present == NULL || ops->confirm == NULL
	    || ops->amount == NULL || ops->combat == NULL
	    || state->player_cache == NULL)
		return false;
	state->candidate = 2.0f;
	state->target_record_cell = 0.0f;
	state->committed = 0.0;
	state->encountered = false;
	state->enter_sector = false;
	state->route = YT_DIRECT_ATTACK_INCOMPLETE;
	state->complete = false;
	if (!ops->present(context, title, sizeof(title) - 1U,
	    YT_DIRECT_ATTACK_TITLE_ROW, error)
	    || !ops->read_player(context, state->current_player_record,
	    &state->current, error))
		return false;
	if (state->current.fighters < 1.0f) {
		if (!ops->present(context, no_fighters,
		    sizeof(no_fighters) - 1U,
		    YT_DIRECT_ATTACK_NO_FIGHTERS_ROW, error))
			return false;
		state->route = YT_DIRECT_ATTACK_NO_FIGHTERS;
		state->complete = true;
		return true;
	}

	while (state->candidate <= state->last_player_record) {
		enum yt_direct_attack_confirmation answer;
		struct qb_val_result parsed;
		float cached_sector;
		float cached_cloak;
		bool sector_mismatch;
		bool self;
		bool cloaked;
		bool positive_team;
		bool same_team;
		int record;

		if (!direct_attack_candidate_record(state, &record, error))
			return false;
		cached_sector = yt_player_cache_value(state->player_cache, record,
		    YT_PLAYER_CACHE_SECTOR);
		cached_cloak = yt_player_cache_value(state->player_cache, record,
		    YT_PLAYER_CACHE_CLOAK);
		sector_mismatch = cached_sector != state->current.sector;
		self = record == state->current_player_record;
		cloaked = cached_cloak > 0.0f;
		if (sector_mismatch || self || cloaked) {
			state->candidate = direct_attack_single_add(
			    state->candidate, 1.0f);
			continue;
		}

		state->target_record_cell = state->candidate;
		if (ops->store_target_record != NULL) {
			uint8_t target_record_raw[4];

			(void)qb_mbf32_encode(state->candidate,
			    target_record_raw);
			ops->store_target_record(context, target_record_raw);
		}
		if (!ops->read_player(context, record,
		    &state->candidate_player, error)
		    || !yt_player_stored_name(&state->candidate_player,
		    target_name, &target_name_length, error))
			return false;
		positive_team = state->candidate_player.team > 0.0f;
		same_team = state->candidate_player.team == state->current.team;
		if (positive_team && same_team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !ops->present(context, row, row_length,
			    YT_DIRECT_ATTACK_TEAM_ROW, error))
				return false;
			state->encountered = true;
			state->candidate = direct_attack_single_add(
			    state->candidate, 1.0f);
			continue;
		}

		state->encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !ops->confirm(context, row, row_length, &answer, error))
			return false;
		if (answer == YT_DIRECT_ATTACK_CONFIRM_NO) {
			state->candidate = direct_attack_single_add(
			    state->candidate, 1.0f);
			continue;
		}
		if (answer != YT_DIRECT_ATTACK_CONFIRM_YES
		    && answer != YT_DIRECT_ATTACK_CONFIRM_EMPTY)
			return false;
		if (!yt_direct_attack_commitment_prompt(
		    (double)state->current.fighters, row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length,
		    YT_DIRECT_ATTACK_COMMITMENT_PROMPT, error)
		    || !ops->amount(context, response, sizeof(response), error))
			return false;
		parsed = qb_val(response);
		state->committed = parsed.valid ? parsed.value : 0.0;
		if (state->committed < 1.0
		    || state->target_record_cell < 1.0f) {
			state->route = YT_DIRECT_ATTACK_CANCELLED;
			state->complete = true;
			return true;
		}
		if (!ops->combat(context, record, state->committed, error))
			return false;
		state->route = YT_DIRECT_ATTACK_COMBAT_RETURN;
		state->complete = true;
		return true;
	}

	state->enter_sector = true;
	if (!ops->present(context,
	    state->encountered ? none_selected : none_visible,
	    state->encountered ? sizeof(none_selected) - 1U
	    : sizeof(none_visible) - 1U,
	    state->encountered ? YT_DIRECT_ATTACK_NONE_SELECTED_ROW
	    : YT_DIRECT_ATTACK_NONE_VISIBLE_ROW, error))
		return false;
	state->route = YT_DIRECT_ATTACK_EXHAUSTED;
	state->complete = true;
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

bool
yt_player_death_run(struct yt_player_death_state *state,
    const struct yt_player_death_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t active_cache_zero[4] = {
		0x00U, 0x00U, 0x7aU, 0x00U
	};
	struct yt_player player;
	uint8_t row[300];
	size_t row_length;
	int logical;
	bool self;
	bool valid_killer;
	float matched;

	if (state == NULL || ops == NULL || ops->clear_active_cache == NULL
	    || ops->read_player == NULL || ops->write_player == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->remove_team == NULL || ops->read_port == NULL
	    || ops->write_port == NULL || ops->present == NULL
	    || ops->news == NULL || ops->set_current_player == NULL
	    || ops->flush == NULL
	    || (state->current_name == NULL && state->current_name_length != 0U))
		return false;
	state->complete = false;
	state->matched_ports = 0;
	ops->clear_active_cache(context, state->victim_record,
	    active_cache_zero);
	if (!ops->read_player(context, state->victim_record, &player, error)
	    || !yt_player_stored_name(&player, state->victim_name,
	    &state->victim_name_length, error))
		return false;
	state->old_ports_owned = player.ports_owned;
	yt_death_player_overlay(&player, state->killer);
	state->victim = player;
	if (!ops->write_player(context, state->victim_record, &player, error))
		return false;
	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!ops->read_sector(context, logical, &sector, error))
			return false;
		if (yt_death_sector_overlay(&sector,
		    (float)state->victim_record)
		    && !ops->write_sector(context, logical, &sector, error))
			return false;
	}
	if (!ops->remove_team(context, state->victim_record, error))
		return false;
	if (state->old_ports_owned != 0.0f) {
		for (logical = 1; logical <= state->port_count; ++logical) {
			struct yt_port port;
			enum yt_death_port_route route;

			if (!ops->read_port(context, logical, &port, error))
				return false;
			route = yt_death_port_overlay(&port,
			    (float)state->victim_record, state->killer,
			    state->last_player_record);
			if (route == YT_DEATH_PORT_UNMATCHED)
				continue;
			++state->matched_ports;
			if (!ops->write_port(context, logical, &port, error))
				return false;
		}
	}
	valid_killer = (state->killer != (float)state->victim_record)
	    & (state->killer > 1.0f)
	    & (state->killer <= state->last_player_record);
	matched = (float)state->matched_ports;
	if (valid_killer && state->matched_ports != 0) {
		if (!yt_death_title_row(state->victim_name,
		    state->victim_name_length, matched, row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length, error)
		    || !ops->read_player(context, (int)state->killer, &player,
		    error))
			return false;
		yt_death_killer_credit_overlay(&player, matched);
		if (!ops->write_player(context, (int)state->killer, &player,
		    error))
			return false;
	}
	self = state->killer == (float)state->victim_record;
	if (!self
	    && !ops->read_player(context, state->victim_record, &player, error))
		return false;
	if (!yt_death_kill_news_row(state->current_name,
	    state->current_name_length, state->victim_name,
	    state->victim_name_length, self, row, sizeof(row), &row_length)
	    || !ops->news(context, row, row_length, error))
		return false;
	if (!self && state->matched_ports != 0) {
		if (!yt_death_port_news_row(state->victim_name,
		    state->victim_name_length, matched, row, sizeof(row),
		    &row_length)
		    || !ops->news(context, row, row_length, error))
			return false;
	}
	if (state->victim_record == state->current_player_record)
		ops->set_current_player(context, &state->victim);
	if (!ops->flush(context, error))
		return false;
	state->complete = true;
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
