#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

bool
yt_maintenance_mercenary_stays(float planet_link, float draw)
{
	return planet_link > 0.0f && draw < 0.6600000262260437f;
}

bool
yt_maintenance_mercenary_attacks(float defense_owner, float draw)
{
	return defense_owner < 0.0f
	    || draw > 0.949999988079071f;
}

static bool
maintenance_news_output_row(const struct yt_maintenance_output_result *output,
    uint16_t address, struct yt_error *error)
{
	const struct yt_maintenance_output_row *row =
	    maintenance_find_output_row(output, address);
	char line[YT_MAINTENANCE_OUTPUT_ROW_SIZE + 1U];

	if (row == NULL || row->length > YT_MAINTENANCE_OUTPUT_ROW_SIZE) {
		set_error(error, YT_INVALID, "maintenance news row", "");
		return false;
	}
	memcpy(line, row->data, row->length);
	line[row->length] = '\0';
	return yt_news_append(line, error);
}

static bool
maintenance_write_mercenary_rebuild(struct yt_game *game,
    int planet_number, struct yt_planet *planet, struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	yt_record_set_text(&planet->record,
	    (const uint8_t *)"Mercenary Base", 14U);
	if (!yt_record_set_number(&planet->record, YT_F41, planet->last_day))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], planet->production[index])
		    || !yt_record_set_number(&planet->record,
		    stock_offsets[index], planet->stock[index]))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, planet->missiles)
	    || !yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces)
	    || !yt_record_set_number(&planet->record, YT_F85,
	    planet->name_length)
	    || !yt_record_set_number(&planet->record, YT_F117, planet->bank)
	    || !yt_record_set_number(&planet->record, YT_F125, planet->mines))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode Mercenary Base rebuild",
	    "YTDATA.DAT");
	return false;
}

static bool
maintenance_write_mercenary_daily(struct yt_game *game, int planet_number,
    struct yt_planet *planet, bool ground_changed, bool bank_changed,
    struct yt_error *error)
{
	if (!yt_record_set_number(&planet->record, YT_F73, planet->owner)
	    || (ground_changed && !yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces))
	    || (bank_changed && !yt_record_set_number(&planet->record, YT_F117,
	    planet->bank))) {
		set_error(error, YT_RANGE, "encode Mercenary Base daily",
		    "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet->record, error);
}

bool
yt_maintenance_maintain_mercenary_base(struct yt_game *game,
    int sector_count, int planet_number, bool *rebuilt,
    struct yt_error *error)
{
	int linked_sector = 0;
	int sector_number;
	struct yt_planet planet;
	bool ground_changed;
	bool bank_changed;

	if (game == NULL || rebuilt == NULL || sector_count < 1
	    || planet_number < 1) {
		set_error(error, YT_INVALID, "Mercenary Base result", "");
		return false;
	}
	*rebuilt = false;
	for (sector_number = 1; sector_number <= sector_count; ++sector_number) {
		struct yt_sector sector;

		if (!yt_game_read_sector(game, sector_number, &sector, error))
			return false;
		if (sector.planet == (float)planet_number) {
			linked_sector = sector_number;
			break;
		}
	}
	if (linked_sector == 0) {
		struct yt_sector sector;
		int today;

		if (!yt_current_date_serial(game->config.epoch_year, &today, NULL,
		    error)
		    || !yt_game_read_planet(game, planet_number, &planet, error))
			return false;
		yt_record_set_text(&planet.record,
		    (const uint8_t *)"Mercenary Base", 14);
		strcpy(planet.name, "Mercenary Base");
		planet.name_length = 14.0f;
		planet.last_day = (float)(today - 10);
		planet.production[0] = 100000.0f;
		planet.production[1] = 100000.0f;
		planet.production[2] = 100000.0f;
		planet.stock[0] = planet.stock[1] = planet.stock[2] = 0.0f;
		planet.missiles = 0.0f;
		planet.owner = -2.0f;
		planet.ground_forces = 150000.0f;
		planet.bank = 25000000.0f;
		planet.mines = 0.0f;
		if (!maintenance_write_mercenary_rebuild(game, planet_number,
		    &planet, error))
			return false;
		do {
			if (!yt_random_integer(&game->random,
			    sector_count, &sector_number, error)
			    || !yt_game_read_sector(game, sector_number, &sector,
			    error))
				return false;
		} while (sector.planet > 0.0f);
		sector.planet = (float)planet_number;
		if (!yt_record_set_number(&sector.record, YT_F93, sector.planet)) {
			set_error(error, YT_RANGE, "encode Mercenary Base link",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, sector_number),
		    &sector.record, error))
			return false;
		*rebuilt = true;
	}
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	planet.owner = -2.0f;
	ground_changed = planet.ground_forces < 1.0f;
	if (ground_changed)
		planet.ground_forces = 150000.0f;
	bank_changed = planet.bank == 0.0f;
	if (bank_changed)
		planet.bank = 25000000.0f;
	return maintenance_write_mercenary_daily(game, planet_number, &planet,
	    ground_changed, bank_changed, error);
}

bool
yt_maintenance_collect_mercenary_tax(struct yt_game *game, int port_count,
    struct yt_maintenance_mercenary_tax_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_tax_result local = {0};
	int port_number;

	if (game == NULL || result == NULL || port_count < 0) {
		set_error(error, YT_INVALID, "Mercenary port tax", "");
		return false;
	}
	for (port_number = 1; port_number <= port_count; ++port_number) {
		struct yt_port port;
		float tax;

		if (!yt_game_read_port(game, port_number, &port, error))
			return false;
		if (port.treasury == 0.0f)
			continue;
		tax = yt_maintenance_sint(
		    qb_single_divide(port.treasury, 10.0f));
		local.tax_pool = qb_single_add(local.tax_pool, tax);
		port.treasury = qb_single_subtract(port.treasury,
		    yt_maintenance_sint(
		    qb_single_divide(port.treasury, 10.0f)));
		if (!yt_game_write_port(game, port_number, &port, error))
			return false;
		++local.taxed_ports;
	}
	local.fleet_strength = yt_maintenance_sint(
	    qb_single_divide(local.tax_pool, 10.0f));
	*result = local;
	return true;
}

bool
yt_maintenance_place_mercenary_fleets(struct yt_game *game,
    int sector_count, float strength,
    float *hired_fighters, struct yt_error *error)
{
	int fleet;

	if (game == NULL || hired_fighters == NULL || sector_count < 2) {
		set_error(error, YT_INVALID, "Mercenary funding result", "");
		return false;
	}
	*hired_fighters = 0.0f;
	if (strength <= 0.0f)
		return true;
	for (fleet = 0; fleet < 10; ++fleet) {
		int sector_number;
		struct yt_sector sector;

		do {
			if (!yt_random_integer(&game->random,
			    sector_count - 1, &sector_number, error))
				return false;
			++sector_number;
			if (!yt_game_read_sector(game, sector_number,
			    &sector, error))
				return false;
		} while (sector.fighters > 0.0f);
		sector.fighters = strength;
		sector.fighter_owner = -2.0f;
		if (!yt_record_set_number(&sector.record, YT_F81,
		    sector.fighters)
		    || !yt_record_set_number(&sector.record, YT_F85,
		    sector.fighter_owner)) {
			set_error(error, YT_RANGE, "encode Mercenary fleet",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_database_write(&game->database,
		    (size_t)yt_sector_basic_record(&game->config, sector_number),
		    &sector.record, error))
			return false;
	}
	*hired_fighters = qb_single_multiply(strength, 10.0f);
	return true;
}

bool
yt_maintenance_mercenary_defections(struct yt_game *game, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_mercenary_defection_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_defection_result local = {0};
	uint64_t starting_draws;
	int logical;

	if (game == NULL || sector_count < 1 || line_output == NULL
	    || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary defections", "YTDATA.DAT");
		return false;
	}
	starting_draws = game->random.draws;
	for (logical = 1; logical <= sector_count; ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		if (sector.fighters > 0.0f) {
			float sample;

			if (!yt_random_next(&game->random, &sample, error))
				return false;
			if (sector.fighter_owner > 1.0f
			    && qb_single_multiply(sample, 100.0f) > sector.fighters) {
				char amount[64];
				char sector_text[64];
				uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
				size_t line_length = 0U;
				size_t name_length;
				int amount_length;
				int sector_length;
				int32_t owner_record;
				int32_t stored_length;
				bool overflow;
				struct yt_player owner;
				static const uint8_t prefix[] = "  - ";
				static const uint8_t fighters[] =
				    " fighters in sector";
				static const uint8_t belonging[] = " belonging to ";
				static const uint8_t suffix[] = " joined the mercs!";

				owner_record = qb_cint_mbf32(sector.record.bytes + YT_F85,
				    0U, &overflow);
				if (overflow || owner_record < 1) {
					set_error(error, YT_RANGE,
					    "Mercenary defection owner", "YTDATA.DAT");
					return false;
				}
				sector.fighter_owner = -2.0f;
				if (!yt_record_set_number(&sector.record, YT_F85,
				    sector.fighter_owner)) {
					set_error(error, YT_RANGE,
					    "encode Mercenary defection", "YTDATA.DAT");
					return false;
				}
				if (!yt_database_write(&game->database,
				    (size_t)yt_sector_basic_record(&game->config, logical),
				    &sector.record, error)
				    || !yt_game_read_player(game, owner_record, &owner,
				    error))
					return false;
				stored_length = qb_cint_mbf32(owner.record.bytes + YT_F85, 0U,
				    &overflow);
				if (overflow || stored_length < 0) {
					set_error(error, YT_RANGE,
					    "Mercenary defection owner name", "YTDATA.DAT");
					return false;
				}
				name_length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
				    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
				amount_length = qb_str_single(amount, sizeof(amount),
				    sector.fighters);
				sector_length = qb_str_single(sector_text,
				    sizeof(sector_text), (float)logical);
				if (amount_length < 0 || sector_length < 0
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, prefix, sizeof(prefix) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, (const uint8_t *)amount,
				    (size_t)amount_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, fighters, sizeof(fighters) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, (const uint8_t *)sector_text,
				    (size_t)sector_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, belonging, sizeof(belonging) - 1U)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, owner.record.bytes, name_length)
				    || !maintenance_copy_part(line, sizeof(line),
				    &line_length, suffix, sizeof(suffix) - 1U)
				    || !line_output(line_context, line, line_length, error)
				    || !yt_news_append_bytes(line, line_length,
				    error))
					return false;
				++local.defections;
			}
		}
	}
	local.draws_consumed = game->random.draws - starting_draws;
	*result = local;
	return true;
}

static bool
mercenary_mine_line(float value, int sector_number, int kind,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t hit_prefix[] = " ***";
	static const uint8_t hit_middle[] =
	    " mercenaries hit sector mines in sector";
	static const uint8_t lost_prefix[] = " *** Lost a total of";
	static const uint8_t lost_suffix[] = " fighters!";
	static const uint8_t killed[] = " *** The mercenariers were killed!";
	char number[64];
	char sector[64];
	int number_length;
	int sector_length;

	*line_length = 0U;
	if (kind == 1)
		return maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, killed,
		    sizeof(killed) - 1U);
	number_length = qb_str_single(number, sizeof(number), value);
	if (number_length < 0)
		return false;
	if (kind == 2)
		return maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, lost_prefix,
		    sizeof(lost_prefix) - 1U)
		    && maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length,
		    (const uint8_t *)number, (size_t)number_length)
		    && maintenance_copy_part(line,
		    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, lost_suffix,
		    sizeof(lost_suffix) - 1U);
	sector_length = qb_str_single(sector, sizeof(sector),
	    (float)sector_number);
	return sector_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, hit_prefix, sizeof(hit_prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)number, (size_t)number_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, hit_middle, sizeof(hit_middle) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

bool
yt_maintenance_mercenary_mines(struct yt_game *game, int sector_number,
    float moving_fighters, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_mine_result *result,
    struct yt_error *error)
{
	struct yt_maintenance_mercenary_mine_result local = {0};
	uint64_t starting_draws;
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length;
	int damage;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary mine arrival",
		    "YTDATA.DAT");
		return false;
	}
	local.moving_before = moving_fighters;
	local.survivors = moving_fighters;
	starting_draws = game->random.draws;
	if (!yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	if (arrival_sector->mines <= 0.0f || moving_fighters <= 0.0f) {
		*result = local;
		return true;
	}
	if (!yt_random_nested_integer(&game->random, 2, 10000, &damage,
	    error))
		return false;
	if ((float)damage > moving_fighters)
		damage = (int)moving_fighters;
	local.draws_consumed = game->random.draws - starting_draws;
	if (damage <= 0) {
		*result = local;
		return true;
	}
	local.losses = (float)damage;
	local.survivors = qb_single_subtract(moving_fighters, local.losses);
	local.mine_hit = true;
	local.killed = local.survivors == 0.0f;
	if (!yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	arrival_sector->mines = qb_single_subtract(arrival_sector->mines, 1.0f);
	if (!yt_record_set_number(&arrival_sector->record, YT_F129,
	    arrival_sector->mines)) {
		set_error(error, YT_RANGE, "encode Mercenary mine",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, sector_number),
	    &arrival_sector->record, error)
	    || !mercenary_mine_line(local.moving_before, sector_number, 0,
	    line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	if (!mercenary_mine_line(local.killed ? 0.0f : local.losses,
	    sector_number, local.killed ? 1 : 2, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	*result = local;
	return true;
}

static bool
mercenary_planet_line(double mercenaries, float planet_fighters,
    const uint8_t *name, size_t name_length, bool taking,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t prefix[] = "  - ";
	static const uint8_t captured[] = " Mercenaries captured planet ";
	static const uint8_t taking_text[] = " Mercenaries taking";
	static const uint8_t fighters[] = " fighters from planet ";
	char first[64];
	char second[64];
	int first_length;
	int second_length = 0;

	*line_length = 0U;
	first_length = qb_str_double(first, sizeof(first), mercenaries);
	if (taking)
		second_length = qb_str_single(second, sizeof(second),
		    planet_fighters);
	return first_length >= 0 && second_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)first, (size_t)first_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, taking ? taking_text : captured,
	    taking ? sizeof(taking_text) - 1U : sizeof(captured) - 1U)
	    && (!taking || maintenance_copy_part(line,
	    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length,
	    (const uint8_t *)second, (size_t)second_length))
	    && (!taking || maintenance_copy_part(line,
	    YT_MAINTENANCE_OUTPUT_ROW_SIZE, line_length, fighters,
	    sizeof(fighters) - 1U))
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, name, name_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

bool
yt_maintenance_mercenary_planet_absorption(struct yt_game *game,
    int sector_number, int selected_destination, double moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_planet_result *result,
    struct yt_error *error)
{
	static const uint8_t planet_fighter_zero[4] = {0x00, 0x00, 0x80, 0x00};
	struct yt_maintenance_mercenary_planet_result local = {0};
	struct yt_planet planet;
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length;
	int32_t planet_number;
	int32_t name_length;
	bool overflow;
	double incoming;
	double existing;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary planet arrival",
		    "YTDATA.DAT");
		return false;
	}
	planet_number = qb_cint_mbf32(arrival_sector->record.bytes + YT_F93,
	    0U, &overflow);
	if (overflow) {
		set_error(error, YT_RANGE, "Mercenary planet link", "YTDATA.DAT");
		return false;
	}
	if ((arrival_sector->fighter_owner != -2.0f
	    && arrival_sector->fighter_owner != 0.0f)
	    || planet_number == 0) {
		*result = local;
		return true;
	}
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	name_length = qb_cint_mbf32(planet.record.bytes + YT_F85, 0U,
	    &overflow);
	if (overflow || name_length < 0 || name_length > 41) {
		set_error(error, YT_RANGE, "Mercenary planet name", "YTDATA.DAT");
		return false;
	}
	local.planet_fighters = (float)qb_int((double)planet.fighters);
	planet.fighters = 0.0f;
	if (!yt_record_set_raw_number(&planet.record, YT_F129,
	    planet_fighter_zero)) {
		set_error(error, YT_RANGE, "encode Mercenary planet fighters",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, planet_number),
	    &planet.record, error)
	    || !yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	existing = (double)arrival_sector->fighters;
	incoming = (double)local.planet_fighters + moving_fighters;
	local.sector_fighters = (float)(incoming + existing);
	arrival_sector->fighters = local.sector_fighters;
	arrival_sector->fighter_owner = -2.0f;
	if (!yt_record_set_number(&arrival_sector->record, YT_F81,
	    arrival_sector->fighters)
	    || !yt_record_set_number(&arrival_sector->record, YT_F85, -2.0f)) {
		set_error(error, YT_RANGE, "encode Mercenary planet sector",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, sector_number),
	    &arrival_sector->record, error))
		return false;
	local.absorbed = true;
	local.capture_report = selected_destination != sector_number
	    && moving_fighters != 0.0;
	local.taking_report = local.planet_fighters > 0.0f;
	if (local.capture_report
	    && (!mercenary_planet_line(incoming, local.planet_fighters,
	    planet.record.bytes, (size_t)name_length, false, line, &line_length)
	    || !line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error)))
		return false;
	if (local.taking_report
	    && (!mercenary_planet_line(moving_fighters + existing,
	    local.planet_fighters, planet.record.bytes, (size_t)name_length,
	    true, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error)))
		return false;
	*result = local;
	return true;
}

static bool
mercenary_destination_attack_line(double moving, double defenders,
    const uint8_t *owner, size_t owner_length,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length)
{
	static const uint8_t prefix[] = " ***";
	static const uint8_t attack[] = " Mercenaries attacking";
	static const uint8_t belonging[] = " fighters beloning to ";
	char first[64];
	char second[64];
	int first_length = qb_str_double(first, sizeof(first), moving);
	int second_length = qb_str_double(second, sizeof(second), defenders);

	*line_length = 0U;
	return first_length >= 0 && second_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)first, (size_t)first_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, attack, sizeof(attack) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)second, (size_t)second_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, belonging, sizeof(belonging) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U);
}

static bool
mercenary_destination_join_lines(double moving, int sector_number,
    const uint8_t *owner, size_t owner_length,
    uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *line_length,
    uint8_t radio[YT_MAINTENANCE_OUTPUT_ROW_SIZE], size_t *radio_length)
{
	static const uint8_t prefix[] = " ***";
	static const uint8_t joined[] = " Mercenaries joined ";
	static const uint8_t defense[] = "'s Defense force in Sector";
	static const uint8_t radio_middle[] =
	    " *** of our boys joined your defense force in Sector";
	char count[64];
	char sector[64];
	int count_length = qb_str_double(count, sizeof(count), moving);
	int sector_length = qb_str_single(sector, sizeof(sector),
	    (float)sector_number);

	*line_length = 0U;
	*radio_length = 0U;
	return count_length >= 0 && sector_length >= 0
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, prefix, sizeof(prefix) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)count, (size_t)count_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, joined, sizeof(joined) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, owner, owner_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, defense, sizeof(defense) - 1U)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(line, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    line_length, (const uint8_t *)"!", 1U)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)count, (size_t)count_length)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, radio_middle, sizeof(radio_middle) - 1U)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)sector, (size_t)sector_length)
	    && maintenance_copy_part(radio, YT_MAINTENANCE_OUTPUT_ROW_SIZE,
	    radio_length, (const uint8_t *)"!", 1U);
}

static bool
mercenary_destination_impl(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, float *moving_after,
    bool *continues, struct yt_error *error)
{
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t won[] = " *** The Mercenaries Won!";
	static const uint8_t lost[] = " *** The Mercenaries Lost!";
	struct yt_sector fresh;
	struct yt_player owner_player;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	uint8_t radio[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t owner_length = 0U;
	size_t line_length;
	size_t radio_length;
	double defenders;
	double moving;
	float original_owner;
	int32_t owner_record = 0;
	int32_t stored_length;
	bool overflow;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || moving_after == NULL
	    || continues == NULL) {
		set_error(error, YT_INVALID, "Mercenary destination", "YTDATA.DAT");
		return false;
	}
	*moving_after = moving_fighters;
	*continues = false;
	original_owner = arrival_sector->fighter_owner;
	moving = (double)moving_fighters;
	if (original_owner == -2.0f || original_owner == 0.0f) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2.0f;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
		*arrival_sector = fresh;
		*moving_after = fresh.fighters;
		*continues = true;
		return true;
	}
	if (original_owner == -1.0f) {
		memcpy(owner_name, xannor, sizeof(xannor) - 1U);
		owner_length = sizeof(xannor) - 1U;
	}
	else {
		owner_record = qb_cint_mbf32(arrival_sector->record.bytes + YT_F85,
		    0U, &overflow);
		if (overflow || owner_record < 1
		    || !yt_game_read_player(game, owner_record, &owner_player,
		    error)) {
			if (!overflow && owner_record < 1)
				set_error(error, YT_RANGE,
				    "Mercenary destination owner", "YTDATA.DAT");
			return false;
		}
		stored_length = qb_cint_mbf32(owner_player.record.bytes + YT_F85, 0U,
		    &overflow);
		if (overflow || stored_length < 0) {
			set_error(error, YT_RANGE,
			    "Mercenary destination owner name", "YTDATA.DAT");
			return false;
		}
		owner_length = (size_t)stored_length < YT_TEXT_FIELD_SIZE
		    ? (size_t)stored_length : YT_TEXT_FIELD_SIZE;
		memcpy(owner_name, owner_player.record.bytes, owner_length);
	}
	{
		float selector;
		bool attack;

		if (!yt_random_next(&game->random, &selector, error))
			return false;
		attack = yt_maintenance_mercenary_attacks(original_owner, selector);
		if (!attack) {
			if (!mercenary_destination_join_lines(moving, sector_number,
			    owner_name, owner_length, line, &line_length, radio,
			    &radio_length)) {
				set_error(error, YT_RANGE,
				    "compose Mercenary join", "YTDATA.DAT");
				return false;
			}
			if (!line_output(line_context, line, line_length, error)
			    || !yt_news_append_bytes(line, line_length, error)
			    || !yt_radio_append_maintenance_bytes(radio, radio_length,
			    -2.0f, original_owner, error)
			    || !yt_game_read_sector(game, sector_number, &fresh, error))
				return false;
			fresh.fighters = (float)((double)fresh.fighters + moving);
			if (!yt_game_write_sector(game, sector_number, &fresh, error))
				return false;
			*arrival_sector = fresh;
			*moving_after = 0.0f;
			return true;
		}
	}
	defenders = (double)arrival_sector->fighters;
	if (!mercenary_destination_attack_line(moving, defenders, owner_name,
	    owner_length, line, &line_length)) {
		set_error(error, YT_RANGE, "compose Mercenary attack", "YTDATA.DAT");
		return false;
	}
	if (!line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error))
		return false;
	while (defenders > 0.0 && moving > 0.0) {
		float sample;
		double quantum = defenders > 200.0 && moving > 200.0
		    ? 150.0 : 1.0;

		if (!yt_random_next(&game->random, &sample, error))
			return false;
		if (sample < 0.5f)
			defenders -= quantum;
		else
			moving -= quantum;
	}
	{
		const uint8_t *result_line = moving > 0.0 ? won : lost;
		size_t result_length = moving > 0.0
		    ? sizeof(won) - 1U : sizeof(lost) - 1U;

		if (!line_output(line_context, result_line, result_length, error)
		    || !yt_news_append_bytes(result_line, result_length, error)
		    || !yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
	}
	if (defenders > 0.0) {
		fresh.fighters = (float)defenders;
		fresh.fighter_owner = original_owner;
	}
	else {
		fresh.fighters = 0.0f;
		fresh.fighter_owner = 0.0f;
	}
	if (!yt_game_write_sector(game, sector_number, &fresh, error))
		return false;
	if (moving > 0.0) {
		if (!yt_game_read_sector(game, sector_number, &fresh, error))
			return false;
		fresh.fighters = (float)((double)fresh.fighters + moving);
		fresh.fighter_owner = -2.0f;
		if (!yt_game_write_sector(game, sector_number, &fresh, error))
			return false;
	}
	*arrival_sector = fresh;
	*moving_after = (float)moving;
	*continues = moving > 0.0;
	return true;
}

bool
yt_maintenance_mercenary_destination(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, struct yt_error *error)
{
	float moving_after;
	bool continues;

	return mercenary_destination_impl(game, sector_number, moving_fighters,
	    line_output, line_context, arrival_sector, &moving_after, &continues,
	    error);
}

enum mercenary_arrival_result {
	MERCENARY_ARRIVAL_TERMINAL,
	MERCENARY_ARRIVAL_CONTINUE
};

static bool
mercenary_arrival(struct yt_game *game, int sector_number,
    int selected_destination, float *moving,
    yt_maintenance_score_line_fn line_output, void *line_context,
    enum mercenary_arrival_result *arrival_result, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_maintenance_mercenary_mine_result mines;
	struct yt_maintenance_mercenary_planet_result planet;

	if (moving == NULL || arrival_result == NULL) {
		set_error(error, YT_INVALID, "Mercenary routed arrival",
		    "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_mercenary_mines(game, sector_number,
	    *moving, line_output, line_context, &sector, &mines, error))
		return false;
	*moving = mines.survivors;
	if (mines.killed) {
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (!yt_maintenance_mercenary_planet_absorption(game,
	    sector_number, selected_destination, (double)*moving,
	    line_output, line_context, &sector, &planet, error))
		return false;
	if (planet.absorbed) {
		*moving = 0.0f;
		*arrival_result = MERCENARY_ARRIVAL_TERMINAL;
		return true;
	}
	if (*moving <= 0.0f || sector.planet != 0.0f) {
		*arrival_result = MERCENARY_ARRIVAL_CONTINUE;
		return true;
	}
	{
		bool continues;

		if (!mercenary_destination_impl(game, sector_number, *moving,
		    line_output, line_context, &sector, moving, &continues, error))
			return false;
		*arrival_result = continues ? MERCENARY_ARRIVAL_CONTINUE
		    : MERCENARY_ARRIVAL_TERMINAL;
	}
	return true;
}

static bool
move_mercenaries_impl(struct yt_game *game, int sector_count,
    struct yt_maintenance_route_cache *route_cache,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	int origin;

	for (origin = sector_count; origin >= 1; --origin) {
		struct yt_sector sector;
		struct yt_maintenance_output_result output;
		int target;
		int next;
		int cursor;
		int hops;
		float moving;

		if (!yt_game_read_sector(game, origin, &sector, error))
			return false;
		if (sector.fighter_owner != -2.0f || sector.fighters <= 0.0f)
			continue;
		{
			float hold;

			if (!yt_random_next(&game->random, &hold, error))
				return false;
			if (yt_maintenance_mercenary_stays(sector.planet, hold))
				continue;
		}
		moving = sector.fighters;
		sector.fighters = 0.0f;
		sector.fighter_owner = 0.0f;
		if (!yt_game_write_sector(game, origin, &sector, error))
			return false;
		do {
			if (!yt_random_integer(&game->random,
			    sector_count, &target, error))
				return false;
		} while (target == origin);
		if (!yt_maintenance_route_next_hop(game,
		    route_cache, origin, target, &next, error)
		    || !yt_maintenance_compose_mercenary_movement((double)moving,
		    (float)origin, &output)
		    || !maintenance_emit_output_row(&output, 0x4911U,
		    line_output, line_context, error))
			return false;
		if (route_cache->successors == NULL) {
			set_error(error, YT_INVALID, "Mercenary route workspace",
			    "YTDATA.DAT");
			return false;
		}
		cursor = origin;
		for (hops = 0; hops <= sector_count; ++hops) {
			enum mercenary_arrival_result arrival_result;

			next = route_cache->successors[cursor];
			if (next == 0) {
				if (moving > 0.0f) {
					struct yt_sector destination;

					if (!yt_game_read_sector(game, target,
					    &destination, error))
						return false;
					destination.fighters = moving;
					destination.fighter_owner = -2.0f;
					if (!yt_game_write_sector(game, target,
					    &destination, error))
						return false;
				}
				break;
			}
			if (next < 1 || next > sector_count) {
				set_error(error, YT_RANGE,
				    "Mercenary route successor", "YTDATA.DAT");
				return false;
			}
			cursor = next;
			if (!mercenary_arrival(game, cursor, target, &moving,
			    line_output, line_context, &arrival_result, error))
				return false;
			if (arrival_result == MERCENARY_ARRIVAL_TERMINAL)
				break;
		}
		if (hops > sector_count) {
			set_error(error, YT_RANGE, "Mercenary route cycle",
			    "YTDATA.DAT");
			return false;
		}
	}
	return true;
}

bool
yt_maintenance_move_mercenaries(struct yt_game *game, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_route_cache route_cache = {0};
	bool result;

	if (game == NULL || sector_count < 1 || line_output == NULL) {
		set_error(error, YT_INVALID, "Mercenary movement", "YTDATA.DAT");
		return false;
	}
	result = move_mercenaries_impl(game, sector_count, &route_cache,
	    line_output, line_context, error);
	yt_maintenance_route_cache_free(&route_cache);
	return result;
}

bool
yt_maintenance_mercenaries_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const char report[] = "  -  Mercenary Report:";
	struct yt_maintenance_mercenary_tax_result tax;
	struct yt_maintenance_mercenary_defection_result defections;
	struct yt_maintenance_output_result output;
	bool rebuilt;
	float hired;

	if (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    0.0f, false, 0.0f, &output)
	    || !maintenance_emit_output_row(&output, 0x3F86U,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x3F95U,
	    line_output, line_context, error)
	    || !yt_maintenance_collect_mercenary_tax(&state->game,
	    state->port_count, &tax, error)
	    || !yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, false, 0.0f, &output))
		return false;
	if (tax.tax_pool != 0.0f
	    && (!maintenance_emit_output_row(&output, 0x40BCU,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output, 0x40BCU, error)))
		return false;
	if (!maintenance_emit_output_row(&output, 0x40DEU,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x40F2U,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output, 0x4103U,
	    line_output, line_context, error)
	    || !yt_news_append(report, error)
	    || !maintenance_emit_output_row(&output, 0x4130U,
	    line_output, line_context, error)
	    || !yt_maintenance_maintain_mercenary_base(&state->game,
	    state->sector_count, state->planet_count - 1, &rebuilt, error))
		return false;
	if (rebuilt) {
		if (!yt_maintenance_compose_mercenary_phase(
		    NULL, 0U, tax.tax_pool, true, 0.0f,
		    &output)
		    || !maintenance_emit_output_row(&output, 0x41DEU,
		    line_output, line_context, error)
		    || !maintenance_emit_output_row(&output, 0x41FAU,
		    line_output, line_context, error)
		    || !maintenance_news_output_row(&output, 0x41FAU, error))
			return false;
	}
	if (!yt_maintenance_place_mercenary_fleets(&state->game,
	    state->sector_count, tax.fleet_strength, &hired, error))
		return false;
	if (hired != 0.0f
	    && (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, rebuilt, hired, &output)
	    || !maintenance_emit_output_row(&output, 0x4631U,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output, 0x4631U, error)))
		return false;
	if (!yt_maintenance_mercenary_defections(&state->game,
	    state->sector_count, line_output, line_context, &defections,
	    error)
	    || !move_mercenaries_impl(&state->game, state->sector_count,
	    &state->route_cache, line_output, line_context, error))
		return false;
	return true;
}

bool
yt_maintenance_maintain_mercenaries(struct yt_game *game,
    struct yt_maintenance_route_cache *cache,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct maint_state state;
	bool success;

	if (game == NULL || cache == NULL || line_output == NULL) {
		set_error(error, YT_INVALID, "Mercenary maintenance",
		    "YTDATA.DAT");
		return false;
	}
	memset(&state, 0, sizeof(state));
	state.game = *game;
	state.route_cache = *cache;
	state.sector_count = (int)(game->config.port_offset
	    - game->config.sector_offset);
	state.port_count = (int)(game->config.planet_offset
	    - game->config.port_offset);
	state.planet_count = (int)(game->config.total_records
	    - game->config.planet_offset);
	if (state.sector_count < 2 || state.port_count < 0
	    || state.planet_count < 2) {
		set_error(error, YT_RANGE, "Mercenary maintenance layout",
		    "YTDATA.DAT");
		return false;
	}
	success = yt_maintenance_mercenaries_run(&state, line_output, line_context,
	    error);
	game->random = state.game.random;
	*cache = state.route_cache;
	return success;
}
