#include "yt_config.h"

#include <math.h>
#include <string.h>

bool
yt_config_decode(struct yt_config *config, const struct yt_record *record,
    struct yt_error *error)
{
	int stored_length;
	bool overflow;

	memset(config, 0, sizeof(*config));
	config->record = *record;
	config->scoreboard_length = yt_record_get_number(record, YT_F41);
	stored_length = (int)qb_cint(config->scoreboard_length, &overflow);
	if (overflow || stored_length < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "configuration scoreboard length");
		}
		return false;
	}
	if (stored_length > 41)
		stored_length = 41;
	memcpy(config->scoreboard, record->bytes, (size_t)stored_length);
	config->scoreboard[stored_length] = '\0';
	config->epoch_year = yt_record_get_number(record, YT_F45);
	config->turns_per_day = yt_record_get_number(record, YT_F49);
	config->sector_offset = yt_record_get_number(record, YT_F53);
	config->port_offset = yt_record_get_number(record, YT_F57);
	config->planet_offset = yt_record_get_number(record, YT_F61);
	config->initial_fighters = yt_record_get_number(record, YT_F65);
	config->initial_credits = yt_record_get_number(record, YT_F69);
	config->initial_holds = yt_record_get_number(record, YT_F73);
	config->retention_days = yt_record_get_number(record, YT_F77);
	config->last_maintenance = yt_record_get_number(record, YT_F81);
	config->local_screen = yt_record_get_number(record, YT_F85);
	config->total_records = yt_record_get_number(record, YT_F93);
	config->lottery_plays = yt_record_get_number(record, YT_F101);
	config->genesis_ports = yt_record_get_number(record, YT_F105);
	config->headquarters = yt_record_get_number(record, YT_F117);
	config->maximum_holds = yt_record_get_number(record, YT_F121);
	config->marker = yt_record_get_number(record, YT_F125);
	config->maximum_planets = yt_record_get_number(record, YT_F129);
	return true;
}

bool
yt_config_load(struct yt_database *database, struct yt_config *config,
    struct yt_error *error)
{
	struct yt_record record;

	return yt_database_read(database, 1, &record, error)
	    && yt_config_decode(config, &record, error);
}

void
yt_config_encode(struct yt_config *config)
{
	size_t length = strlen(config->scoreboard);

	if (length > 41)
		length = 41;
	yt_record_set_text_if_changed(&config->record,
	    (const uint8_t *)config->scoreboard, length);
	yt_record_set_number_if_changed(&config->record, YT_F41, (float)length);
	yt_record_set_number_if_changed(&config->record, YT_F45,
	    config->epoch_year);
	yt_record_set_number_if_changed(&config->record, YT_F49,
	    config->turns_per_day);
	yt_record_set_number_if_changed(&config->record, YT_F53,
	    config->sector_offset);
	yt_record_set_number_if_changed(&config->record, YT_F57,
	    config->port_offset);
	yt_record_set_number_if_changed(&config->record, YT_F61,
	    config->planet_offset);
	yt_record_set_number_if_changed(&config->record, YT_F65,
	    config->initial_fighters);
	yt_record_set_number_if_changed(&config->record, YT_F69,
	    config->initial_credits);
	yt_record_set_number_if_changed(&config->record, YT_F73,
	    config->initial_holds);
	yt_record_set_number_if_changed(&config->record, YT_F77,
	    config->retention_days);
	yt_record_set_number_if_changed(&config->record, YT_F81,
	    config->last_maintenance);
	yt_record_set_number_if_changed(&config->record, YT_F85,
	    config->local_screen);
	yt_record_set_number_if_changed(&config->record, YT_F93,
	    config->total_records);
	yt_record_set_number_if_changed(&config->record, YT_F101,
	    config->lottery_plays);
	yt_record_set_number_if_changed(&config->record, YT_F105,
	    config->genesis_ports);
	yt_record_set_number_if_changed(&config->record, YT_F117,
	    config->headquarters);
	yt_record_set_number_if_changed(&config->record, YT_F121,
	    config->maximum_holds);
	yt_record_set_number_if_changed(&config->record, YT_F125,
	    config->marker);
	yt_record_set_number_if_changed(&config->record, YT_F129,
	    config->maximum_planets);
}

bool
yt_config_store(struct yt_database *database, const struct yt_config *config,
    struct yt_error *error)
{
	struct yt_config encoded = *config;

	yt_config_encode(&encoded);
	return yt_database_write(database, 1, &encoded.record, error);
}

void
yt_config_normalize_game(struct yt_config *config, bool local_mode)
{
	if (config->genesis_ports < 20.0f)
		config->genesis_ports = 200.0f;
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "ytscore.asc");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f)
		config->local_screen = -1.0f;
	if (local_mode)
		config->local_screen = -1.0f;
	if (config->lottery_plays < 0.0f || config->lottery_plays > 9.0f)
		config->lottery_plays = 3.0f;
	if (config->maximum_planets == 0.0f)
		config->maximum_planets = 100.0f;
	if (config->maximum_holds < 5.0f || config->maximum_holds > 1000.0f)
		config->maximum_holds = 1000.0f;
	if (config->turns_per_day < 100.0f || config->turns_per_day > 2500.0f)
		config->turns_per_day = 500.0f;
}

void
yt_config_normalize_maintenance(struct yt_config *config)
{
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "NUL");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f)
		config->local_screen = -1.0f;
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
yt_date_serial(const struct yt_clock_value *date, int epoch_year,
    int *adjusted_year)
{
	return date_serial_epoch(date, (float)epoch_year, adjusted_year);
}

bool
yt_current_date_serial(float epoch, int *serial, int *adjusted_year,
    struct yt_error *error)
{
	struct yt_clock_value current;

	if (!yt_platform_clock(&current, error))
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
yt_player_basic_record(int logical_player)
{
	return logical_player + 1;
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
