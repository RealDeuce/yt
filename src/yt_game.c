#include "yt_game.h"

#include "qb.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

void
yt_current_player_cache_overlay(float *sector_cache, float *cloak_cache,
    size_t cache_count, int player_record, bool anti_cloak,
    const struct yt_player *player)
{
	if (sector_cache == NULL || cloak_cache == NULL || player == NULL
	    || player_record < 0 || (size_t)player_record >= cache_count)
		return;
	sector_cache[player_record] = player->sector;
	if (!anti_cloak)
		cloak_cache[player_record] = player->cloak;
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
