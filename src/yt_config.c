#include "yt_config.h"
#include "qb.h"

#include <math.h>
#include <string.h>

static bool
config_decode_unsigned(const struct yt_record *record, size_t offset,
    uint32_t maximum, uint32_t *result, struct yt_error *error)
{
	float value = yt_record_get_number(record, offset);

	if (!isfinite(value) || value < 0.0f || value > (float)maximum
	    || floorf(value) != value) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "decode configuration integer");
			(void)snprintf(error->path, sizeof(error->path), "%s",
			    "YTDATA.DAT");
		}
		return false;
	}
	*result = (uint32_t)value;
	return true;
}

bool
yt_config_decode(struct yt_config *config, const struct yt_record *record,
    struct yt_error *error)
{
	size_t stored_length;
	uint32_t integer;

	memset(config, 0, sizeof(*config));
	config->record = *record;
	(void)error;
	stored_length = (size_t)yt_record_get_number(record, YT_F41);
	if (stored_length > 41U)
		stored_length = 41U;
	config->scoreboard_length = stored_length;
	memcpy(config->scoreboard, record->bytes, stored_length);
	config->scoreboard[stored_length] = '\0';
	config->epoch_year = yt_record_get_number(record, YT_F45);
	config->turns_per_day = yt_record_get_number(record, YT_F49);
	if (!config_decode_unsigned(record, YT_F53, UINT8_MAX, &integer,
	    error))
		return false;
	config->sector_offset = (uint8_t)integer;
	if (!config_decode_unsigned(record, YT_F57, UINT16_MAX, &integer,
	    error))
		return false;
	config->port_offset = (uint16_t)integer;
	if (!config_decode_unsigned(record, YT_F61, UINT16_MAX, &integer,
	    error))
		return false;
	config->planet_offset = (uint16_t)integer;
	config->initial_fighters = yt_record_get_number(record, YT_F65);
	config->initial_credits = yt_record_get_number(record, YT_F69);
	config->initial_holds = yt_record_get_number(record, YT_F73);
	config->retention_days = yt_record_get_number(record, YT_F77);
	config->last_maintenance = yt_record_get_number(record, YT_F81);
	config->local_screen = yt_record_get_number(record, YT_F85) != 0.0f;
	if (!config_decode_unsigned(record, YT_F93, UINT16_MAX, &integer,
	    error))
		return false;
	config->total_records = (uint16_t)integer;
	config->lottery_plays = yt_record_get_number(record, YT_F101);
	config->genesis_ports = yt_record_get_number(record, YT_F105);
	config->headquarters = yt_record_get_number(record, YT_F117);
	config->maximum_holds = yt_record_get_number(record, YT_F121);
	if (!config_decode_unsigned(record, YT_F125, UINT16_MAX, &integer,
	    error))
		return false;
	config->marker = (uint16_t)integer;
	if (!config_decode_unsigned(record, YT_F129, UINT8_MAX, &integer,
	    error))
		return false;
	config->maximum_planets = (uint8_t)integer;
	return true;
}

bool
yt_config_load(struct yt_database *database, struct yt_config *config,
    struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(database, 1, &record, error))
		return false;
	return yt_config_decode(config, &record, error);
}

static bool
config_hq_error(struct yt_error *error, enum yt_status status,
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
yt_config_headquarters_relocate(struct yt_database *database,
    const struct yt_config *config, float candidate,
    enum yt_config_hq_route *route, struct yt_record *result,
    struct yt_error *error)
{
	static const uint8_t raw_clear[4] = {0x00, 0x00, 0x80, 0x00};
	struct yt_record candidate_initial;
	struct yt_record field;
	uint8_t candidate_raw[4];
	bool overflow;
	int candidate_logical;
	int old_logical;
	int32_t converted;
	float captured_candidate_fighters;
	float candidate_planet;
	float candidate_fighter_owner;
	float merged_fighters;
	float planet_link;

	if (database == NULL || config == NULL || route == NULL || result == NULL)
		return config_hq_error(error, YT_INVALID,
		    "YTCONFIG Headquarters transaction");
	*route = YT_CONFIG_HQ_ROUTE_INCOMPLETE;
	if (qb_mbf32_encode(candidate, candidate_raw) == QB_MBF_OVERFLOW)
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters candidate");
	converted = qb_cint((double)candidate, &overflow);
	if (overflow)
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters candidate");
	candidate_logical = (int)converted;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, candidate_logical),
	    &candidate_initial, error))
		return false;
	candidate_planet = yt_record_get_number(&candidate_initial, YT_F93);
	converted = qb_cint((double)candidate_planet, &overflow);
	if (overflow)
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters planet link");
	if (converted != 0) {
		*route = YT_CONFIG_HQ_ROUTE_OCCUPIED;
		return true;
	}
	captured_candidate_fighters = yt_record_get_number(&candidate_initial,
	    YT_F81);
	converted = qb_cint((double)captured_candidate_fighters, &overflow);
	if (overflow)
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters fighters");
	candidate_fighter_owner = yt_record_get_number(&candidate_initial,
	    YT_F85);
	if (converted != 0 && candidate_fighter_owner != -1.0f) {
		*route = YT_CONFIG_HQ_ROUTE_OCCUPIED;
		return true;
	}
	converted = qb_cint((double)config->headquarters, &overflow);
	if (overflow)
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG old Headquarters");
	old_logical = (int)converted;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, old_logical), &field,
	    error))
		return false;
	merged_fighters = qb_single_add(captured_candidate_fighters,
	    yt_record_get_number(&field, YT_F81));
	(void)yt_record_set_raw_number(&field, YT_F93, raw_clear);
	(void)yt_record_set_raw_number(&field, YT_F85, raw_clear);
	(void)yt_record_set_raw_number(&field, YT_F81, raw_clear);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config, old_logical), &field, error))
		return false;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, candidate_logical), &field,
	    error))
		return false;
	planet_link = qb_single_subtract((float)config->total_records,
	    (float)config->planet_offset);
	if (!yt_record_set_number(&field, YT_F85, -1.0f))
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters candidate overlay");
	if (!yt_record_set_number(&field, YT_F81, merged_fighters))
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters candidate overlay");
	if (!yt_record_set_number(&field, YT_F93, planet_link))
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG Headquarters candidate overlay");
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config, candidate_logical), &field,
	    error))
		return false;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, 1), &field, error))
		return false;
	(void)yt_record_set_raw_number(&field, YT_F105, candidate_raw);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config, 1), &field, error))
		return false;
	if (!yt_database_read(database, 1U, &field, error))
		return false;
	(void)yt_record_set_raw_number(&field, YT_F117, candidate_raw);
	if (!yt_database_write(database, 1U, &field, error))
		return false;
	if (!yt_database_flush(database, error))
		return false;
	*result = field;
	*route = YT_CONFIG_HQ_ROUTE_RELOCATED;
	return true;
}

bool
yt_config_toggle_local_screen(struct yt_database *database,
    struct yt_record *result, bool *toggled, struct yt_error *error)
{
	struct yt_record field;
	bool current;

	if (database == NULL || result == NULL || toggled == NULL)
		return config_hq_error(error, YT_INVALID,
		    "YTCONFIG local-screen transaction");
	if (!yt_database_read(database, 1U, &field, error))
		return false;
	current = yt_record_get_number(&field, YT_F85) != 0.0f;
	*toggled = !current;
	if (!yt_record_set_number(&field, YT_F85,
	    *toggled ? -1.0f : 0.0f))
		return config_hq_error(error, YT_RANGE,
		    "YTCONFIG local-screen overlay");
	if (!yt_database_write(database, 1U, &field, error))
		return false;
	if (!yt_database_flush(database, error))
		return false;
	*result = field;
	return true;
}

static bool
config_apply_overlay_values(struct yt_record *field,
    const struct yt_config_overlay *overlays, size_t overlay_count,
    const char *operation, struct yt_error *error)
{
	size_t index;

	if (field == NULL || (overlays == NULL && overlay_count != 0U))
		return config_hq_error(error, YT_INVALID, operation);
	for (index = 0U; index < overlay_count; ++index) {
		if ((overlays[index].data == NULL && overlays[index].length != 0U)
		    || overlays[index].offset > YT_RECORD_SIZE
		    || overlays[index].length >
		    YT_RECORD_SIZE - overlays[index].offset)
			return config_hq_error(error, YT_INVALID, operation);
	}
	for (index = 0U; index < overlay_count; ++index) {
		if (overlays[index].length != 0U)
			memcpy(field->bytes + overlays[index].offset,
			    overlays[index].data, overlays[index].length);
	}
	return true;
}

bool
yt_config_apply_overlays(struct yt_database *database,
    const struct yt_config_overlay *overlays, size_t overlay_count,
    struct yt_record *result, struct yt_error *error)
{
	struct yt_record field;

	if (database == NULL || result == NULL
	    || (overlays == NULL && overlay_count != 0U))
		return config_hq_error(error, YT_INVALID,
		    "YTCONFIG overlay transaction");
	if (!yt_database_read(database, 1U, &field, error))
		return false;
	if (!config_apply_overlay_values(&field, overlays, overlay_count,
	    "YTCONFIG overlay range", error))
		return false;
	if (!yt_database_write(database, 1U, &field, error))
		return false;
	if (!yt_database_flush(database, error))
		return false;
	*result = field;
	return true;
}

bool
yt_config_apply_loaded_overlays(struct yt_database *database,
    const struct yt_record *field,
    const struct yt_config_overlay *overlays, size_t overlay_count,
    struct yt_record *result, struct yt_error *error)
{
	struct yt_record updated;

	if (database == NULL || field == NULL || result == NULL
	    || (overlays == NULL && overlay_count != 0U))
		return config_hq_error(error, YT_INVALID,
		    "YTCONFIG loaded overlay transaction");
	updated = *field;
	if (!config_apply_overlay_values(&updated, overlays, overlay_count,
	    "YTCONFIG loaded overlay range", error))
		return false;
	if (!yt_database_write(database, 1U, &updated, error))
		return false;
	if (!yt_database_flush(database, error))
		return false;
	*result = updated;
	return true;
}

bool
yt_config_redraw_repairs(struct yt_database *database,
    const struct yt_record *field, float working_maximum_holds,
    struct yt_record *result, struct yt_error *error)
{
	struct yt_record updated;
	uint8_t raw[4];

	if (database == NULL || field == NULL || result == NULL)
		return config_hq_error(error, YT_INVALID,
		    "YTCONFIG redraw-repair transaction");
	updated = *field;
	if (yt_record_get_number(&updated, YT_F73) > working_maximum_holds) {
		if (qb_mbf32_encode(working_maximum_holds, raw)
		    == QB_MBF_OVERFLOW)
			return config_hq_error(error, YT_RANGE,
			    "YTCONFIG redraw holds repair");
		(void)yt_record_set_raw_number(&updated, YT_F73, raw);
		if (!yt_database_write(database, 1U, &updated, error))
			return false;
		if (!yt_database_flush(database, error))
			return false;
	}
	if (!yt_database_read(database, 1U, &updated, error))
		return false;
	if (yt_record_get_number(&updated, YT_F117) == 0.0f) {
		if (qb_mbf32_encode(85.0f, raw) == QB_MBF_OVERFLOW)
			return config_hq_error(error, YT_RANGE,
			    "YTCONFIG redraw Headquarters repair");
		(void)yt_record_set_raw_number(&updated, YT_F117, raw);
		if (!yt_database_write(database, 1U, &updated, error))
			return false;
		if (!yt_database_flush(database, error))
			return false;
		if (!yt_database_read(database, 1U, &updated, error))
			return false;
	}
	*result = updated;
	return true;
}

void
yt_config_normalize_maintenance(struct yt_config *config)
{
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "NUL");
	if (config->lottery_plays < 1.0f)
		config->lottery_plays = 1.0f;
	if (config->maximum_holds < 10.0f || config->maximum_holds > 250.0f)
		config->maximum_holds = 250.0f;
}

static int
date_serial_epoch(const struct yt_clock_value *date, float epoch,
    int *adjusted_year)
{
	static const int days_before[] =
	    {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int year = date->year % 100;
	int serial;
	float prior;

	if ((float)year < epoch)
		year += 100;
	serial = date->day + days_before[date->month];
	if (year % 4 == 0 && date->month > 2)
		++serial;
	prior = (float)year - 1.0f;
	if ((float)year != epoch && prior >= epoch) {
		float quarter = epoch * 0.25f;

		serial += 365;
		if (quarter == floorf(quarter))
			++serial;
	}
	if (adjusted_year != NULL)
		*adjusted_year = year;
	return serial;
}

int
yt_date_serial(const struct yt_clock_value *date, float epoch_year,
    int *adjusted_year)
{
	return date_serial_epoch(date, epoch_year, adjusted_year);
}

bool
yt_current_date_serial(const struct yt_clock *clock, float epoch, int *serial,
    int *adjusted_year, struct yt_error *error)
{
	struct yt_clock_value current;

	if (!yt_clock_read(clock, &current, error))
		return false;
	*serial = date_serial_epoch(&current, epoch, adjusted_year);
	return true;
}

void
yt_format_date(const struct yt_clock_value *value, char dest[11])
{
	dest[0] = (char)('0' + value->month / 10);
	dest[1] = (char)('0' + value->month % 10);
	dest[2] = '-';
	dest[3] = (char)('0' + value->day / 10);
	dest[4] = (char)('0' + value->day % 10);
	dest[5] = '-';
	dest[6] = (char)('0' + value->year / 1000);
	dest[7] = (char)('0' + value->year / 100 % 10);
	dest[8] = (char)('0' + value->year / 10 % 10);
	dest[9] = (char)('0' + value->year % 10);
	dest[10] = '\0';
}

void
yt_format_time(const struct yt_clock_value *value, char dest[9])
{
	int hour = value->hour;
	int minute = value->minute;
	int second = value->second;

	if (value->hundredth >= 50 && ++second == 60) {
		second = 0;
		if (++minute == 60) {
			minute = 0;
			++hour;
		}
	}
	dest[0] = (char)('0' + hour / 10);
	dest[1] = (char)('0' + hour % 10);
	dest[2] = ':';
	dest[3] = (char)('0' + minute / 10);
	dest[4] = (char)('0' + minute % 10);
	dest[5] = ':';
	dest[6] = (char)('0' + second / 10);
	dest[7] = (char)('0' + second % 10);
	dest[8] = '\0';
}

int
yt_sector_basic_record(const struct yt_config *config, int logical_sector)
{
	return (int)config->sector_offset + logical_sector;
}

int
yt_port_basic_record(const struct yt_config *config, int logical_port)
{
	return (int)config->port_offset + logical_port;
}

int
yt_planet_basic_record(const struct yt_config *config, int logical_planet)
{
	return (int)config->planet_offset + logical_planet;
}
