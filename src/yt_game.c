#include "yt_game.h"

#include <string.h>

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
