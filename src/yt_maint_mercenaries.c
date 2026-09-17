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
    enum yt_maintenance_output_row_id id, struct yt_error *error)
{
	const struct yt_maintenance_output_row *row =
	    maintenance_find_output_row(output, id);
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
	    (float)planet->name_length)
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

		if (!yt_current_date_serial(&game->clock, game->config.epoch_year,
		    &today, NULL,
		    error)
		    || !yt_game_read_planet(game, planet_number, &planet, error))
			return false;
		yt_record_set_text(&planet.record,
		    (const uint8_t *)"Mercenary Base", 14);
		strcpy(planet.name, "Mercenary Base");
		planet.name_length = 14U;
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
    struct yt_error *error)
{
	int logical;

	if (game == NULL || sector_count < 1 || line_output == NULL) {
		set_error(error, YT_INVALID, "Mercenary defections", "YTDATA.DAT");
		return false;
	}
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
				int owner_record;
				struct yt_player owner;
				static const uint8_t prefix[] = "  - ";
				static const uint8_t fighters[] =
				    " fighters in sector";
				static const uint8_t belonging[] = " belonging to ";
				static const uint8_t suffix[] = " joined the mercs!";

				owner_record = (int)sector.fighter_owner;
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
				name_length = owner.name_length < YT_TEXT_FIELD_SIZE
				    ? owner.name_length : YT_TEXT_FIELD_SIZE;
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
			}
		}
	}
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
    float *moving_fighters, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_sector *arrival_sector,
    struct yt_error *error)
{
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length;
	float moving_before;
	float survivors;
	float losses;
	bool killed;
	int damage;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || moving_fighters == NULL) {
		set_error(error, YT_INVALID, "Mercenary mine arrival",
		    "YTDATA.DAT");
		return false;
	}
	moving_before = *moving_fighters;
	if (!yt_game_read_sector(game, sector_number, arrival_sector, error))
		return false;
	if (arrival_sector->mines <= 0.0f || moving_before <= 0.0f)
		return true;
	if (!yt_random_nested_integer(&game->random, 2, 10000, &damage,
	    error))
		return false;
	if ((float)damage > moving_before)
		damage = (int)moving_before;
	if (damage <= 0)
		return true;
	losses = (float)damage;
	survivors = qb_single_subtract(moving_before, losses);
	killed = survivors == 0.0f;
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
	    || !mercenary_mine_line(moving_before, sector_number, 0,
	    line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	if (!mercenary_mine_line(killed ? 0.0f : losses,
	    sector_number, killed ? 1 : 2, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error))
		return false;
	*moving_fighters = survivors;
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
	int planet_number;
	size_t name_length;
	double incoming;
	double existing;

	if (game == NULL || sector_number < 1 || line_output == NULL
	    || arrival_sector == NULL || result == NULL) {
		set_error(error, YT_INVALID, "Mercenary planet arrival",
		    "YTDATA.DAT");
		return false;
	}
	planet_number = (int)arrival_sector->planet;
	if ((arrival_sector->fighter_owner != -2.0f
	    && arrival_sector->fighter_owner != 0.0f)
	    || planet_number == 0) {
		*result = local;
		return true;
	}
	if (!yt_game_read_planet(game, planet_number, &planet, error))
		return false;
	name_length = planet.name_length;
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
	    planet.record.bytes, name_length, false, line, &line_length)
	    || !line_output(line_context, line, line_length, error)
	    || !yt_news_append_bytes(line, line_length, error)))
		return false;
	if (local.taking_report
	    && (!mercenary_planet_line(moving_fighters + existing,
	    local.planet_fighters, planet.record.bytes, name_length,
	    true, line, &line_length)
	    || !yt_news_append_bytes(line, line_length, error)
	    || !line_output(line_context, line, line_length, error)))
		return false;
	*result = local;
	return true;
}

bool
yt_maintenance_mercenaries_run(struct maint_state *state,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	static const char report[] = "  -  Mercenary Report:";
	struct yt_maintenance_mercenary_tax_result tax;
	struct yt_maintenance_output_result output;
	bool rebuilt;
	float hired;

	if (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    0.0f, false, 0.0f, &output)
	    || !maintenance_emit_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_START_BLANK,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_START_SEPARATOR,
	    line_output, line_context, error)
	    || !yt_maintenance_collect_mercenary_tax(&state->game,
	    state->port_count, &tax, error)
	    || !yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, false, 0.0f, &output))
		return false;
	if (tax.tax_pool != 0.0f
	    && (!maintenance_emit_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_TAX_REPORT,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_TAX_REPORT, error)))
		return false;
	if (!maintenance_emit_output_row(&output, YT_MAINT_ROW_MERCENARY_PHASE_BLANK,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_PHASE_HEADER,
	    line_output, line_context, error)
	    || !maintenance_emit_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_PHASE_SEPARATOR,
	    line_output, line_context, error)
	    || !yt_news_append(report, error)
	    || !maintenance_emit_output_row(&output, YT_MAINT_ROW_MERCENARY_BASE_CHECK,
	    line_output, line_context, error)
	    || !yt_maintenance_maintain_mercenary_base(&state->game,
	    state->sector_count, state->planet_count - 1, &rebuilt, error))
		return false;
	if (rebuilt) {
		if (!yt_maintenance_compose_mercenary_phase(
		    NULL, 0U, tax.tax_pool, true, 0.0f,
		    &output)
		    || !maintenance_emit_output_row(&output,
		    YT_MAINT_ROW_MERCENARY_REBUILD_BLANK,
		    line_output, line_context, error)
		    || !maintenance_emit_output_row(&output, YT_MAINT_ROW_MERCENARY_REBUILT,
		    line_output, line_context, error)
		    || !maintenance_news_output_row(&output,
		    YT_MAINT_ROW_MERCENARY_REBUILT, error))
			return false;
	}
	if (!yt_maintenance_place_mercenary_fleets(&state->game,
	    state->sector_count, tax.fleet_strength, &hired, error))
		return false;
	if (hired != 0.0f
	    && (!yt_maintenance_compose_mercenary_phase(
	    NULL, 0U,
	    tax.tax_pool, rebuilt, hired, &output)
	    || !maintenance_emit_output_row(&output, YT_MAINT_ROW_MERCENARY_HIRED,
	    line_output, line_context, error)
	    || !maintenance_news_output_row(&output,
	    YT_MAINT_ROW_MERCENARY_HIRED, error)))
		return false;
	if (!yt_maintenance_mercenary_defections(&state->game,
	    state->sector_count, line_output, line_context, error)
	    || !yt_maintenance_move_mercenaries(&state->game,
	    state->sector_count,
	    &state->route_cache, line_output, line_context, error))
		return false;
	return true;
}
