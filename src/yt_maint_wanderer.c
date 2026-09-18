#include "yt_maint.h"

#include <errno.h>
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

static bool
maintenance_write_wanderer_sector(struct yt_game *game, int logical,
    struct yt_sector *sector, struct yt_error *error)
{
	if (!yt_record_set_number(&sector->record, YT_F93,
	    (float)sector->planet)) {
		set_error(error, YT_RANGE, "encode Wanderer sector", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_sector_basic_record(&game->config, logical),
	    &sector->record, error);
}

static bool
maintenance_write_wanderer_rebuild(struct yt_game *game,
    struct yt_planet *planet, int today, struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	yt_record_set_text(&planet->record,
	    (const uint8_t *)"The Wanderer", 12U);
	if (!yt_record_set_number(&planet->record, YT_F41,
	    (float)(today - 10)))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], 5000.0f))
			goto range;
		if (!yt_record_set_number(&planet->record,
		    stock_offsets[index], 0.0f))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, 0.0f))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F73, 0.0f))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F77, 0.0f))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F85, 12.0f))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F117, 250000.0f))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F125, 0.0f))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, 1),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode Wanderer rebuild", "YTDATA.DAT");
	return false;
}

static bool
maintenance_write_wanderer_planet(struct yt_game *game,
    struct yt_planet *planet, struct yt_error *error)
{
	if (!yt_record_set_number(&planet->record, YT_F73,
	    (float)planet->owner)) {
		set_error(error, YT_RANGE, "encode Wanderer planet", "YTDATA.DAT");
		return false;
	}
	if (!yt_record_set_number(&planet->record, YT_F117, planet->bank)) {
		set_error(error, YT_RANGE, "encode Wanderer planet", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, 1),
	    &planet->record, error);
}

bool
yt_maintenance_maintain_wanderer(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	struct yt_sector sector;
	struct yt_planet planet;
	int sector_count;
	int logical;
	int removed_sector = 0;
	int today;
	size_t row;

	if (game == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "maintain Wanderer", "YTDATA.DAT");
		return false;
	}
	sector_count = (int)game->config.port_offset
	    - (int)game->config.sector_offset;
	if (sector_count < 1
	    || game->config.total_records <= game->config.planet_offset) {
		set_error(error, YT_RANGE, "maintain Wanderer", "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_compose_wanderer_phase(blank,
	    blank_length, false, &output)) {
		set_error(error, YT_RANGE, "maintain Wanderer", "YTDATA.DAT");
		return false;
	}
	for (row = 0U; row < 2U; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	for (logical = 1; logical <= sector_count; ++logical) {
		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		if (sector.planet == 1) {
			removed_sector = logical;
			sector.planet = 0;
			if (!maintenance_write_wanderer_sector(game, logical,
			    &sector, error))
				return false;
			break;
		}
	}
	if (removed_sector == 0) {
		if (!yt_current_date_serial(&game->clock,
		    (float)game->config.epoch_year,
		    &today, NULL, error))
			return false;
		if (!yt_maintenance_compose_wanderer_phase(blank,
		    blank_length, true, &output))
			return false;
		if (!line_output(line_context, output.rows[2].data,
		    output.rows[2].length, error))
			return false;
		if (!yt_news_append_bytes(output.rows[2].data,
		    output.rows[2].length, error))
			return false;
		if (!yt_game_read_planet(game, 1, &planet, error))
			return false;
		if (!maintenance_write_wanderer_rebuild(game, &planet, today,
		    error))
			return false;
		if (!line_output(line_context, output.rows[3].data,
		    output.rows[3].length, error))
			return false;
		if (!yt_news_append_bytes(output.rows[3].data,
		    output.rows[3].length, error))
			return false;
	}
	for (row = output.row_count - 2U; row < output.row_count; ++row) {
		if (!line_output(line_context, output.rows[row].data,
		    output.rows[row].length, error))
			return false;
	}
	for (;;) {
		if (!yt_random_integer(&game->random, sector_count,
		    &logical, error))
			return false;
		if (!yt_game_read_sector(game, logical, &sector, error))
			return false;
		if (sector.planet == 0)
			break;
	}
	sector.planet = 1;
	if (!maintenance_write_wanderer_sector(game, logical, &sector, error))
		return false;
	if (!yt_game_read_planet(game, 1, &planet, error))
		return false;
	planet.owner = 0;
	if (planet.bank == 0.0f)
		planet.bank = 250000.0f;
	if (!maintenance_write_wanderer_planet(game, &planet, error))
		return false;
	return true;
}
