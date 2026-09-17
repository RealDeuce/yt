#include "yt_game.h"

#include "qb.h"
#include "yt_main_error.h"

#include <stdio.h>
#include <string.h>

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
yt_current_player_hydrate(struct yt_player *player,
    const struct yt_player *fresh, int player_record,
    float sector_record_offset, bool anti_cloak_enabled,
    float *current_sector_record, struct yt_player_cache *player_cache,
    struct yt_error *error)
{
	uint8_t sector_record_offset_raw[4];
	uint8_t current_sector_raw[8] = {0};
	enum qb_mbf_status add_status;

	if (player == NULL || fresh == NULL || current_sector_record == NULL)
		return false;
	if (qb_mbf32_encode(sector_record_offset, sector_record_offset_raw)
	    != QB_MBF_OK)
		return false;

	player->record = fresh->record;
	player->sector = fresh->sector;
	player->fighters = fresh->fighters;
	add_status = current_player_add_single_raw(sector_record_offset_raw,
	    fresh->record.bytes + YT_F57, fresh->record.bytes + YT_F61,
	    current_sector_raw);
	if (add_status == QB_MBF_OVERFLOW || add_status == QB_MBF_DOMAIN)
		return current_player_hydration_fault(error,
		    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_SECTOR_ADD,
		    "current-player A41C sector ADD_FLOAT");
	*current_sector_record = qb_mbf32_decode(current_sector_raw);
	player->turns = fresh->turns;
	player->credits = fresh->credits;
	player->danger_scanner = fresh->danger_scanner;
	player->missiles = fresh->missiles;
	player->mines = fresh->mines;
	player->team = fresh->team;
	player->holds = fresh->holds;
	player->ore = fresh->ore;
	player->organics = fresh->organics;
	player->equipment = fresh->equipment;
	player->plasma = fresh->plasma;
	player->score = fresh->score;
	player->ports_owned = fresh->ports_owned;
	player->ground_forces = fresh->ground_forces;
	player->cloak = fresh->cloak;
	if (!anti_cloak_enabled) {
		if (player_cache != NULL)
			(void)yt_player_cache_set(player_cache, player_record,
			    YT_PLAYER_CACHE_CLOAK, fresh->cloak);
	}
	player->shields = fresh->shields;
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
	player->name_length = (size_t)yt_record_get_number(record, YT_F85);
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
	    (float)player->name_length);
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
yt_team_decode(struct yt_team *team, int id, const struct yt_record *record)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125,
	};
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = id;
	yt_sector_decode(&team->overlay, record);
	team->full = true;
	for (index = 0U; index < YT_ARRAY_LEN(team->roster); ++index) {
		team->roster[index] = (int)yt_record_get_number(record,
		    roster_offsets[index]);
		if (team->roster[index] != 0)
			team->live = true;
		else
			team->full = false;
	}
	if (!team->live) {
		team->full = false;
		return;
	}
	team->name_length = (size_t)yt_record_get_number(record, YT_F73);
	if (team->name_length > YT_TEXT_FIELD_SIZE)
		team->name_length = YT_TEXT_FIELD_SIZE;
	memcpy(team->name, record->bytes, team->name_length);
	team->name[team->name_length] = '\0';
	memcpy(team->password, record->bytes + YT_F113, 4U);
	team->password[4] = '\0';
	team->captain = (int)yt_record_get_number(record, YT_F77);
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
	port->name_length = (size_t)yt_record_get_number(record, YT_F85);
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
	    (float)port->name_length);
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
	planet->name_length = (size_t)yt_record_get_number(record, YT_F85);
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
	    (float)planet->name_length);
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
    const struct yt_clock *clock, struct yt_error *error)
{
	memset(game, 0, sizeof(*game));
	if (clock != NULL)
		game->clock = *clock;
	yt_random_init(&game->random);
	if (!yt_database_open(&game->database, "YTDATA.DAT", mode, error)
	    || !yt_config_load(&game->database, &game->config, error)) {
		yt_game_close(game);
		return false;
	}
	return yt_current_date_serial(&game->clock, game->config.epoch_year,
	    &game->today, &game->adjusted_year, error);
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
yt_game_read_team(struct yt_game *game, int id, struct yt_team *team,
    struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, id), &record, error))
		return false;
	yt_team_decode(team, id, &record);
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
