#ifndef YT_CONFIG_H
#define YT_CONFIG_H

#include "yt_file.h"
#include "yt_platform.h"

struct yt_config {
	struct yt_record record;
	char scoreboard[42];
	float scoreboard_length;
	float epoch_year;
	float turns_per_day;
	float sector_offset;
	float port_offset;
	float planet_offset;
	float initial_fighters;
	float initial_credits;
	float initial_holds;
	float retention_days;
	float last_maintenance;
	float local_screen;
	float total_records;
	float lottery_plays;
	float genesis_ports;
	float headquarters;
	float maximum_holds;
	float marker;
	float maximum_planets;
};

bool yt_config_decode(struct yt_config *config,
    const struct yt_record *record, struct yt_error *error);
bool yt_config_load(struct yt_database *database, struct yt_config *config,
    struct yt_error *error);
bool yt_config_store(struct yt_database *database,
    const struct yt_config *config, struct yt_error *error);
void yt_config_encode(struct yt_config *config);
void yt_config_normalize_game(struct yt_config *config, bool local_mode);
void yt_config_normalize_maintenance(struct yt_config *config);

int yt_date_serial(const struct yt_clock_value *date, int epoch_year,
    int *adjusted_year);
bool yt_current_date_serial(float epoch, int *serial, int *adjusted_year,
    struct yt_error *error);
void yt_format_date(const struct yt_clock_value *value, char dest[11]);
void yt_format_time(const struct yt_clock_value *value, char dest[9]);

int yt_player_basic_record(int logical_player);
int yt_sector_basic_record(const struct yt_config *config, int logical_sector);
int yt_port_basic_record(const struct yt_config *config, int logical_port);
int yt_planet_basic_record(const struct yt_config *config, int logical_planet);

#endif
