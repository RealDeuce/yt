#include "yt_game.h"
#include "yt_game_internal.h"
#include "yt_port_math.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_pager.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
yt_game_load_startup_configuration(struct yt_game *game, const char *path,
    bool local_mode, struct yt_player_cache *player_cache,
    float disruption_sectors[2], float *local_screen, struct yt_error *error)
{
	struct yt_config *config;
	bool overflow;
	int32_t path_count;
	float counter;
	size_t index;

	if (game == NULL || path == NULL || player_cache == NULL
	    || disruption_sectors == NULL || local_screen == NULL)
		return false;
	config = &game->config;
	if (!yt_database_random_close(&game->database, error)
	    || !yt_database_open(&game->database, path,
	    YT_OPEN_UPDATE_CREATE, error)
	    || !yt_config_load(&game->database, config, error))
		return false;
	path_count = qb_cint_mbf32(config->record.bytes + YT_F41, 0U,
	    &overflow);
	if (overflow || path_count < 0)
		return yt_game_error(error, YT_RANGE,
		    "startup scoreboard LEFT$");
	config->scoreboard_length = (float)path_count;
	if (config->scoreboard_length > YT_TEXT_FIELD_SIZE)
		config->scoreboard_length = YT_TEXT_FIELD_SIZE;
	memcpy(config->scoreboard, config->record.bytes,
	    (size_t)config->scoreboard_length);
	*local_screen = config->local_screen;
	qb_compat_upper_n((uint8_t *)config->scoreboard,
	    (size_t)config->scoreboard_length);
	config->scoreboard[(size_t)config->scoreboard_length] = '\0';

	if (config->headquarters == 0.0f) {
		static const uint8_t headquarters_default[4] = {
			0x00, 0x40, 0x37, 0x8a
		};

		if (!yt_record_set_raw_number(&config->record, YT_F117,
		    headquarters_default)
		    || !yt_database_write_durable(&game->database, 1U,
		    &config->record, error))
			return false;
		config->headquarters = 733.0f;
	}
	if (config->genesis_ports < 20.0f) {
		config->genesis_ports = 200.0f;
	}
	if (config->scoreboard_length == 0.0f) {
		static const char default_path[] = "YTSCORE.ASC";

		memcpy(config->scoreboard, default_path, sizeof(default_path));
		config->scoreboard_length = sizeof(default_path) - 1U;
	}
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode) {
		config->local_screen = -1.0f;
		*local_screen = -1.0f;
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
			return yt_game_error(error, YT_RANGE,
			    "startup player-cache index");
		if (!yt_game_read_player(game, basic, &player, error))
			return false;
		(void)yt_player_cache_set_raw(player_cache, basic,
		    YT_PLAYER_CACHE_SECTOR, player.record.bytes + YT_F57);
		(void)yt_player_cache_set_raw(player_cache, basic,
		    YT_PLAYER_CACHE_CLOAK, player.record.bytes + YT_F125);
		if (player.cloak < 0.0f || player.cloak > 1.0f) {
			static const uint8_t one[4] = {
				0x00U, 0x00U, 0x00U, 0x81U
			};

			player.cloak = 1.0f;
			(void)yt_player_cache_set_raw(player_cache, basic,
			    YT_PLAYER_CACHE_CLOAK, one);
			if (!yt_record_set_number(&player.record, YT_F125, 1.0f)
			    || !yt_database_write_durable(&game->database,
			    (size_t)basic, &player.record, error))
				return false;
		}
		counter = qb_single_add(counter, 1.0f);
	}
	for (index = 0U; index < 2U; ++index) {
		float draw;
		float difference;
		float span;
		float product;
		float integral;
		uint8_t raw[4];

		if (!yt_random_next(&game->random, &draw, error))
			return false;
		difference = qb_single_subtract(config->port_offset,
		    config->sector_offset);
		span = qb_single_subtract(difference, 2.0f);
		product = qb_single_multiply(draw, span);
		integral = floorf(product);
		disruption_sectors[index] = qb_single_add(integral, 2.0f);
		if (integral == -2.0f) {
			static const uint8_t dirty_zero[4] = {
				0x00U, 0x00U, 0x80U, 0x00U
			};

			memcpy(raw, dirty_zero, sizeof(raw));
		} else if (qb_mbf32_encode(disruption_sectors[index], raw)
		    != QB_MBF_OK) {
			return yt_game_error(error, YT_RANGE,
			    "startup disruption result");
		}
		disruption_sectors[index] = qb_mbf32_decode(raw);
	}
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
	return yt_game_join_parts(player_name, player_name_length,
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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

	return yt_game_join_parts(prefix, sizeof(prefix) - 1U,
	    victim_name, victim_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
	return yt_game_join_parts(first, sizeof(first) - 1U,
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
market_encode_single(float value, uint8_t raw[4], struct yt_error *error,
    const char *operation)
{
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (status == QB_MBF_OK || status == QB_MBF_UNDERFLOW)
		return true;
	return yt_game_error(error, YT_RANGE, operation);
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
	return yt_game_error(error, YT_RANGE, label);
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
		return yt_game_error(error, YT_RANGE,
		    "ordinary port constants");
	minute = qb_single_divide(state->timer_seconds, 60.0f);
	elapsed = qb_single_add(
	    qb_single_subtract(state->current_day, state->port.last_day),
	    qb_single_divide(qb_single_subtract(minute,
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

		yt_port_mbf64_promote_single(state->port.record.bytes
		    + YT_F49 + index * 4U, mutable_capacity[index]);
		memcpy(mutable_production[index], state->port.record.bytes
		    + YT_F61 + index * 4U, 4U);
		growth_value = qb_single_multiply(
		    qb_mbf32_decode(mutable_production[index]), elapsed);
		if (!market_encode_single(growth_value, growth_raw, error,
		    "ordinary port growth"))
			return false;
		yt_port_mbf64_promote_single(growth_raw, growth);
		if (!market_binary(qb_mbf64_add_raw, mutable_capacity[index],
		    growth, mutable_capacity[index], error,
		    "ordinary port capacity")
		    || !market_binary(qb_mbf64_div_raw, mutable_capacity[index],
		    ten, quotient, error, "ordinary port production comparison"))
			return false;
		yt_port_mbf64_promote_single(mutable_production[index],
		    promoted_production);
		if (yt_port_mbf64_compare(quotient, promoted_production) > 0) {
			if (!market_binary(qb_mbf64_div_raw,
			    mutable_capacity[index], ten, quotient, error,
			    "ordinary port production replacement")
			    || qb_mbf32_from_mbf64_raw(quotient,
			    mutable_production[index]) == QB_MBF_OVERFLOW)
				return yt_game_error(error, YT_RANGE,
				    "ordinary port production CSNG");
			raised[index] = true;
			yt_port_mbf64_promote_single(mutable_production[index],
			    promoted_production);
		}
		if (!market_encode_single(state->base_price[index], base_raw,
		    error, "ordinary port base price"))
			return false;
		yt_port_mbf64_promote_single(base_raw, base);
		yt_port_mbf64_promote_single(state->port.record.bytes
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
		yt_port_mbf64_negate(scale);
		if (!market_binary(qb_mbf64_add_raw, one, scale, scale, error,
		    "ordinary port price scale")
		    || !market_binary(qb_mbf64_mul_raw, base, scale, raw_price,
		    error, "ordinary port raw price")
		    || !market_binary(qb_mbf64_add_raw, raw_price, half,
		    rounded_source, error, "ordinary port price rounding"))
			return false;
		if (yt_port_mbf64_compare(rounded_source, zero) <= 0)
			memset(rounded, 0, sizeof(rounded));
		else {
			enum qb_mbf_status status = qb_mbf64_floor_positive_raw(
			    rounded_source, rounded);

			if (status != QB_MBF_OK && status != QB_MBF_UNDERFLOW)
				return yt_game_error(error, YT_RANGE,
				    "ordinary port price INT");
		}
		if (qb_mbf32_from_mbf64_raw(rounded, mutable_price[index])
		    == QB_MBF_OVERFLOW)
			return yt_game_error(error, YT_RANGE,
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
			return yt_game_error(error, YT_RANGE,
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
		return yt_game_error(error, YT_RANGE, operation);
	*length = (size_t)converted;
	if (*length > YT_TEXT_FIELD_SIZE)
		*length = YT_TEXT_FIELD_SIZE;
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
yt_port_report_compose(const struct yt_port_market_state *market,
    const struct yt_player *current_player,
    const struct yt_port *report_port, uint8_t conversion_mode,
    const uint8_t date[10], const uint8_t time_text[8],
    struct yt_port_report_text *report, struct yt_error *error)
{
	static const uint8_t title_prefix[] = "Commerce report for ";
	static const uint8_t title_separator[] = ": ";
	static const uint8_t commodity[3][14] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	static const uint8_t buying[] = "  Buying ";
	static const uint8_t selling[] = "  Selling";
	static const uint8_t padding[] = "    ";
	static const size_t hold_offset[3] = {YT_F69, YT_F73, YT_F77};
	uint8_t promoted_hold[8];
	uint8_t floored_capacity[8];
	char number[96];
	int formatted_length;
	size_t name_length;
	size_t number_length;
	size_t index;

	if (market == NULL || current_player == NULL || report_port == NULL
	    || date == NULL || time_text == NULL || report == NULL)
		return yt_game_error(error, YT_INVALID,
		    "port report arguments");
	memset(report, 0, sizeof(*report));
	if (!port_report_field_length(report_port->record.bytes + YT_F85,
	    conversion_mode, &name_length, error, "port report name length"))
		return false;
	if (!port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_prefix, sizeof(title_prefix) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, report_port->record.bytes, name_length)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, title_separator,
	    sizeof(title_separator) - 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, date, 10U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, " ", 1U)
	    || !port_report_append(report->title, sizeof(report->title),
	    &report->title_length, time_text, 8U))
		return yt_game_error(error, YT_RANGE,
		    "port report title composition");

	for (index = 0U; index < 3U; ++index) {
		struct yt_port_report_item *item = &report->item[index];
		const uint8_t *status;
		size_t position = 0U;

		if (market->port.factor[index] < 0.0f) {
			status = buying;
			item->foreground = 3.0f;
		}
		else {
			status = selling;
			item->foreground = 2.0f;
		}
		if (!port_report_append(item->name_status,
		    sizeof(item->name_status), &position, commodity[index],
		    sizeof(commodity[index]) - 1U)
		    || !port_report_append(item->name_status,
		    sizeof(item->name_status), &position, status,
		    sizeof(buying) - 1U))
			return yt_game_error(error, YT_RANGE,
			    "port report item composition");
		if (qb_mbf64_floor_raw(market->capacity_raw[index],
		    floored_capacity) != QB_MBF_OK)
			return yt_game_error(error, YT_RANGE,
			    "port report stock INT");
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    floored_capacity);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->capacity),
		    item->capacity))
			return yt_game_error(error, YT_RANGE,
			    "port report stock formatting");
		yt_port_mbf64_promote_single(current_player->record.bytes
		    + hold_offset[index], promoted_hold);
		formatted_length = qb_str_mbf64(number, sizeof(number),
		    promoted_hold);
		if (formatted_length < 0
		    || !port_report_right_raw((const uint8_t *)number,
		    (size_t)formatted_length, sizeof(item->hold), item->hold))
			return yt_game_error(error, YT_RANGE,
			    "port report hold formatting");
		formatted_length = qb_str_mbf32(number, sizeof(number),
		    market->price_raw[index]);
		if (formatted_length < 0)
			return yt_game_error(error, YT_RANGE,
			    "port report price formatting");
		number_length = (size_t)formatted_length;
		position = 0U;
		if (!port_report_append(item->price, sizeof(item->price),
		    &position, number, number_length)
		    || !port_report_append(item->price, sizeof(item->price),
		    &position, padding, sizeof(padding) - 1U))
			return yt_game_error(error, YT_RANGE,
			    "port report price composition");
		item->price_length = position;
	}
	return true;
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
		*selected = qb_single_add(*selected, amount);
	else
		*selected = (float)qb_double_add((double)*selected,
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
	*selected = (float)qb_double_subtract(cached_quantity,
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
	player->fighters = (float)qb_double_add(
	    (double)player->fighters, amount[4]);
	player->missiles = qb_single_add(player->missiles,
	    (float)amount[5]);
	player->mines = qb_single_add(player->mines, (float)amount[6]);
	player->plasma = qb_single_add(player->plasma, (float)amount[9]);
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

		*selected = (float)qb_double_subtract(
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
	free_double = qb_double_subtract((double)player->holds,
	    (double)player->ore);
	free_double = qb_double_subtract(free_double,
	    (double)player->organics);
	free_double = qb_double_subtract(free_double,
	    (double)player->equipment);
	free_holds = (float)free_double;
	amount = (float)floor(cached_quantity);
	if (free_holds < amount)
		amount = free_holds;
	selected = take_all_player_item(player, item);
	*selected = (float)qb_double_add((double)*selected,
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
	*selected = (float)qb_double_subtract(cached_quantity,
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
		float threshold = qb_single_multiply(rate[item], 10.0f);
		double total = qb_double_add(quantity[item], held[index]);

		if (total > (double)threshold)
			rate[item] = (float)qb_double_add(
			    qb_double_divide(floor(total), 10.0), 1.0);
		quantity[item] = qb_double_add(quantity[item], held[index]);
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

		planet->production[index] = qb_single_subtract(rate[item],
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
		*selected = (float)qb_double_add(cached_quantity,
		    (double)cached_amount);
}

void
yt_planet_transfer_fighter_player_overlay(struct yt_player *player,
    float cached_fighters, float amount)
{
	if (player != NULL)
		player->fighters = (float)qb_double_subtract(
		    (double)cached_fighters, (double)amount);
}

void
yt_planet_transfer_fighter_planet_overlay(struct yt_planet *planet,
    double cached_quantity, float amount)
{
	if (planet != NULL)
		planet->fighters = (float)qb_double_add(cached_quantity,
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
		return yt_game_error(error, YT_INVALID,
		    "planet Transfer fighter amount arguments");
	parsed = qb_val(response);
	if (parsed.overflow)
		return yt_game_error(error, YT_RANGE,
		    "planet Transfer fighter VAL");
	candidate = (float)(parsed.valid ? parsed.value : 0.0);
	status = qb_mbf32_encode(candidate, raw);
	if (status == QB_MBF_OVERFLOW)
		return yt_game_error(error, YT_RANGE,
		    "planet Transfer fighter CSNG");
	*amount = status == QB_MBF_UNDERFLOW ? 0.0f : qb_mbf32_decode(raw);
	return true;
}

double
yt_planet_bank_available(float cached_credits, float cached_bank)
{
	return qb_double_add((double)cached_credits,
	    (double)cached_bank);
}

double
yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target)
{
	double after_target = qb_double_subtract((double)cached_credits,
	    target);

	return qb_double_add(after_target, (double)cached_bank);
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
	return (float)qb_double_subtract((double)cached_bank, target);
}

void
yt_planet_bank_credit_overlay(struct yt_player *player, float argument)
{
	if (player != NULL)
		player->credits = floorf(qb_single_add(player->credits,
		    argument));
}

double
yt_planet_productivity_units(double spend)
{
	return qb_double_divide(spend, 250.0);
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
	old_sum = qb_single_add(qb_single_add(rate[1], rate[2]),
	    rate[3]);
	for (index = 1; index <= 3U; ++index)
		rate[index] = (float)qb_double_add((double)rate[index],
		    units);
	new_sum = qb_single_add(qb_single_add(rate[1], rate[2]),
	    rate[3]);
	delta[0] = qb_single_subtract(floorf(new_sum), floorf(old_sum));
	new_value = floorf(qb_single_divide(new_sum, 2500.0f));
	old_value = floorf(qb_single_divide(old_sum, 2500.0f));
	delta[1] = qb_single_subtract(new_value, old_value);
	new_value = floorf(qb_single_divide(new_sum, 25000.0f));
	old_value = floorf(qb_single_divide(old_sum, 25000.0f));
	delta[2] = qb_single_subtract(new_value, old_value);
	new_value = floorf(qb_single_multiply(new_sum, plasma_multiplier));
	old_value = floorf(qb_single_multiply(old_sum, plasma_multiplier));
	delta[3] = qb_single_subtract(new_value, old_value);
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
	return floorf(qb_single_multiply(100.0f, discount));
}

void
yt_earth_prices(const float discount[4], float price[4])
{
	if (discount == NULL || price == NULL)
		return;
	price[0] = floorf(qb_single_subtract(250.0f,
	    qb_single_multiply(250.0f, discount[0])));
	price[1] = floorf(qb_single_subtract(50.0f,
	    qb_single_multiply(50.0f, discount[1])));
	price[2] = floorf(qb_single_multiply(50.0f,
	    qb_single_subtract(1.0f, discount[2])));
	price[3] = floorf(qb_single_multiply(200.5f,
	    qb_single_subtract(1.0f, discount[3])));
}

double
yt_earth_affordable(float credits, float price)
{
	return floor(qb_double_divide((double)credits, (double)price));
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
		return floorf(qb_single_multiply(0.009999999776482582f, cost));
	return cost;
}

float
yt_earth_cloak_points(float cloak)
{
	return floorf(qb_single_multiply(50.0f, cloak));
}

float
yt_earth_cloak_default(float deficit, float credits)
{
	if (qb_single_multiply(deficit, 1000.0f) > credits)
		return (float)yt_earth_affordable(credits, 1000.0f);
	return deficit;
}

float
yt_earth_cloak_overlay(float points, float quantity)
{
	return qb_single_divide(floorf(qb_single_add(points, quantity)),
	    50.0f);
}

void
yt_earth_supply_overlay(struct yt_player *player, int choice, float quantity)
{
	if (player == NULL)
		return;
	if (choice == 3)
		player->fighters = qb_single_add(player->fighters, quantity);
	else if (choice == 7)
		player->ground_forces = floorf(qb_single_add(
		    player->ground_forces, quantity));
	else if (choice == 8)
		player->shields = floorf(qb_single_add(
		    player->shields, quantity));
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

bool
yt_projectile_is_black_hole(float hop, float first, float second)
{
	return hop == first || hop == second;
}

bool
yt_projectile_cruise_reroute_row(float hop, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] =
	    "The missiles are deflected by a black hole in sector";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), hop);
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length + sizeof(suffix) - 1U
	    > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	memcpy(row + sizeof(prefix) - 1U, number, (size_t)number_length);
	row_length = sizeof(prefix) - 1U + (size_t)number_length;
	memcpy(row + row_length, suffix, sizeof(suffix) - 1U);
	row_length += sizeof(suffix) - 1U;
	*length = row_length;
	return true;
}

float
yt_projectile_cruise_reroute_destination(float draw,
    float sector_record_offset, float port_record_offset)
{
	float span = qb_single_subtract(port_record_offset,
	    sector_record_offset);
	float selected = floorf(qb_single_multiply(draw, span));

	return qb_single_add(selected, 1.0f);
}

bool
yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker)
{
	return hop < 8.0f && destination < 8.0f
	    && counterattack == 0 && xannor_provoker == 0;
}

bool
yt_projectile_sector_has_presence(const struct yt_sector *sector,
    int sector_number, int last_player,
    const struct yt_player_cache *player_cache, int xannor_provoker)
{
	bool present;
	int player;

	if (sector == NULL || player_cache == NULL)
		return false;
	present = sector->mines > 0.0f || sector->fighters > 0.0f
	    || sector->port > 0.0f || sector->planet > 0.0f;
	for (player = YT_PLAYER_FIRST_RECORD; player <= last_player; ++player) {
		if (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_SECTOR) == (float)sector_number
		    && (yt_player_cache_value(player_cache, player,
		    YT_PLAYER_CACHE_CLOAK) == 0.0f
		    || player == xannor_provoker))
			return true;
	}
	return present;
}




bool
yt_projectile_sector_mine_hit_row(double mines, float sector,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t hit_prefix[] = "The missiles hit";
	static const uint8_t hit_middle[] = " SECTOR MINES in sector";
	char mine_text[64];
	char sector_text[64];
	int mine_length;
	int sector_length;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text),
	    sector);
	mine_length = qb_str_double(mine_text, sizeof(mine_text), mines);
	if (sector_length < 0 || mine_length < 0
	    || sizeof(hit_prefix) - 1U + (size_t)mine_length
	    + sizeof(hit_middle) - 1U + (size_t)sector_length + 1U
	    > capacity)
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
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_news_row(const uint8_t *shooter,
    size_t shooter_length, float sector, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t middle[] =
	    "'s Missiles hit sector mines in sector";
	char sector_text[64];
	int sector_length;
	size_t row_length = 0U;

	if (row == NULL || length == NULL
	    || (shooter == NULL && shooter_length != 0U))
		return false;
	sector_length = qb_str_single(sector_text, sizeof(sector_text), sector);
	if (sector_length < 0 || shooter_length + sizeof(middle) - 1U
	    + (size_t)sector_length + 1U > capacity)
		return false;
	if (shooter_length != 0U) {
		memcpy(row, shooter, shooter_length);
		row_length = shooter_length;
	}
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, sector_text, (size_t)sector_length);
	row_length += (size_t)sector_length;
	row[row_length++] = '!';
	*length = row_length;
	return true;
}

bool
yt_projectile_sector_mine_destroyed_row(float destroyed,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The missile";
	static const uint8_t middle[] = " destroyed";
	static const uint8_t mine[] = " mine";
	char number[64];
	int number_length;
	size_t plural = destroyed > 1.0f ? 1U : 0U;
	size_t row_length;

	if (row == NULL || length == NULL)
		return false;
	number_length = qb_str_single(number, sizeof(number), destroyed);
	if (number_length < 0 || sizeof(prefix) - 1U + plural
	    + sizeof(middle) - 1U + (size_t)number_length
	    + sizeof(mine) - 1U + plural + 1U > capacity)
		return false;
	memcpy(row, prefix, sizeof(prefix) - 1U);
	row_length = sizeof(prefix) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	memcpy(row + row_length, middle, sizeof(middle) - 1U);
	row_length += sizeof(middle) - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, mine, sizeof(mine) - 1U);
	row_length += sizeof(mine) - 1U;
	if (plural != 0U)
		row[row_length++] = 's';
	row[row_length++] = '!';
	*length = row_length;
	return true;
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
	sector->mines = qb_single_add(sector->mines, carried_mines);
	return yt_record_set_number(&sector->record, YT_F129, sector->mines);
}

uint32_t
yt_projectile_physical_record(float offset, float logical)
{
	return qb_brun_random_record_number(qb_single_add(offset,
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
    float *remaining, struct yt_random *random,
    struct yt_projectile_ground_result *result, struct yt_error *error)
{
	size_t iterations = 0U;

	if (remaining == NULL || random == NULL || result == NULL)
		return false;
	result->ground = ground;
	result->owner = owner;
	result->iterations = 0U;
	while (ground > 0.0f && *remaining > 0.0f) {
		float value;

		if (!yt_random_next(random, &value, error))
			return false;
		ground = qb_single_subtract(ground,
		    qb_single_multiply(value, 25.0f));
		*remaining = qb_single_subtract(*remaining, 1.0f);
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
    struct yt_random *random,
    struct yt_projectile_productivity_result *result,
    struct yt_error *error)
{
	float old_total;
	float new_total;
	size_t iterations = 0U;
	size_t index;

	if (production == NULL || stock == NULL || remaining == NULL
	    || random == NULL || result == NULL)
		return false;
	old_total = qb_single_add(qb_single_add(production[0],
	    production[1]), production[2]);
	while ((updater_ore > 0.0f || production[1] > 0.0f
	    || production[2] > 0.0f) && *remaining > 0.0f) {
		for (index = 0U; index < 3U; ++index) {
			float value;

			if (!yt_random_next(random, &value, error))
				return false;
			production[index] = qb_single_subtract(
			    production[index], qb_single_multiply(value,
			    2000.0f));
		}
		*remaining = qb_single_subtract(*remaining, 1.0f);
		++iterations;
	}
	for (index = 0U; index < 3U; ++index) {
		float cap;

		if (production[index] < 0.0f)
			production[index] = 0.0f;
		cap = qb_single_multiply(production[index], 10.0f);
		if (stock[index] > cap)
			stock[index] = cap;
	}
	new_total = qb_single_add(qb_single_add(production[0],
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
	    qb_single_subtract(old_total, new_total), false)
	    || !sector_row_append(&builder, middle, sizeof(middle) - 1U)
	    || !sector_row_number(&builder, new_total, false)
	    || !sector_row_append(&builder, suffix, sizeof(suffix) - 1U))
		return false;
	if (length != NULL)
		*length = builder.length;
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
    struct yt_random *random,
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

	if (target == NULL || remaining == NULL || random == NULL
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
		*remaining = qb_single_subtract(*remaining, 1.0f);
		if (!yt_random_next(random, &value, error))
			return false;
		scanner_product = qb_single_multiply(value, *remaining);
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
		if (!yt_random_next(random, &value, error))
			return false;
		fighter_damage = floor((double)qb_single_multiply(value,
		    4001.0f) + fighter_damage);
		if (!yt_random_next(random, &value, error))
			return false;
		/* The SINGLE draw is promoted for the DOUBLE fighter operand. */
		if ((double)value * original_fighters < fighter_damage) {
			if (!yt_random_next(random, &value, error))
				return false;
			shield_damage = qb_single_add(shield_damage,
			    floorf(qb_single_multiply(value, 1001.0f)));
		}
		if (fighter_damage >= original_fighters
		    && shield_damage >= original_shields)
			break;
		counter = qb_single_add(counter, 1.0f);
	}
	if (fighter_damage > original_fighters)
		fighter_damage = original_fighters;
	if (shield_damage > original_shields)
		shield_damage = original_shields;
	target->fighters = (float)(original_fighters - fighter_damage);
	target->shields = qb_single_subtract(original_shields,
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
