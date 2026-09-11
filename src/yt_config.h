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

enum yt_config_hq_operation {
	YT_CONFIG_HQ_NONE,
	YT_CONFIG_HQ_READ_CANDIDATE_INITIAL,
	YT_CONFIG_HQ_READ_OLD,
	YT_CONFIG_HQ_WRITE_OLD,
	YT_CONFIG_HQ_READ_CANDIDATE_FRESH,
	YT_CONFIG_HQ_WRITE_CANDIDATE,
	YT_CONFIG_HQ_READ_SECTOR_ONE,
	YT_CONFIG_HQ_WRITE_SECTOR_ONE,
	YT_CONFIG_HQ_READ_CONFIG,
	YT_CONFIG_HQ_WRITE_CONFIG,
};

enum yt_config_hq_route {
	YT_CONFIG_HQ_ROUTE_INCOMPLETE,
	YT_CONFIG_HQ_ROUTE_OCCUPIED,
	YT_CONFIG_HQ_ROUTE_RELOCATED,
};

struct yt_config_hq_state {
	enum yt_config_hq_operation attempted;
	enum yt_config_hq_route route;
	int candidate_logical;
	int old_logical;
	size_t current_basic_record;
	unsigned reads_completed;
	unsigned writes_completed;
	float captured_candidate_fighters;
	float merged_fighters;
	struct yt_record candidate_initial;
	struct yt_record field;
	bool field_loaded;
	bool complete;
};

struct yt_config_record_ops {
	bool (*read_record)(void *context, size_t basic_record,
	    struct yt_record *record, struct yt_error *error);
	bool (*write_record)(void *context, size_t basic_record,
	    const struct yt_record *record, struct yt_error *error);
};

enum yt_config_local_screen_operation {
	YT_CONFIG_LOCAL_SCREEN_NONE,
	YT_CONFIG_LOCAL_SCREEN_READ,
	YT_CONFIG_LOCAL_SCREEN_WRITE,
};

struct yt_config_local_screen_state {
	enum yt_config_local_screen_operation attempted;
	struct yt_record field;
	float stored;
	float toggled;
	bool field_loaded;
	bool overlay_complete;
	bool write_complete;
	bool complete;
};

enum yt_config_overlay_operation {
	YT_CONFIG_OVERLAY_NONE,
	YT_CONFIG_OVERLAY_READ,
	YT_CONFIG_OVERLAY_COPY,
	YT_CONFIG_OVERLAY_WRITE,
};

struct yt_config_overlay {
	size_t offset;
	const uint8_t *data;
	size_t length;
};

struct yt_config_overlay_state {
	enum yt_config_overlay_operation attempted;
	struct yt_record field;
	size_t overlay_index;
	size_t overlays_completed;
	bool field_loaded;
	bool write_complete;
	bool complete;
};

enum yt_config_redraw_repair_operation {
	YT_CONFIG_REDRAW_REPAIR_NONE,
	YT_CONFIG_REDRAW_REPAIR_WRITE_HOLDS,
	YT_CONFIG_REDRAW_REPAIR_READ_MENU,
	YT_CONFIG_REDRAW_REPAIR_WRITE_HEADQUARTERS,
	YT_CONFIG_REDRAW_REPAIR_READ_HEADQUARTERS,
};

struct yt_config_redraw_repair_state {
	enum yt_config_redraw_repair_operation attempted;
	struct yt_record field;
	unsigned reads_completed;
	unsigned writes_completed;
	bool holds_repaired;
	bool headquarters_repaired;
	bool field_loaded;
	bool complete;
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
bool yt_config_headquarters_relocate(struct yt_config_hq_state *state,
	const struct yt_config *config, float candidate,
	const struct yt_config_record_ops *ops, void *context,
	struct yt_error *error);
bool yt_config_toggle_local_screen(
	struct yt_config_local_screen_state *state,
	const struct yt_config_record_ops *ops, void *context,
	struct yt_error *error);
bool yt_config_apply_overlays(struct yt_config_overlay_state *state,
	const struct yt_config_overlay *overlays, size_t overlay_count,
	const struct yt_config_record_ops *ops, void *context,
	struct yt_error *error);
bool yt_config_apply_loaded_overlays(struct yt_config_overlay_state *state,
	const struct yt_record *field,
	const struct yt_config_overlay *overlays, size_t overlay_count,
	const struct yt_config_record_ops *ops, void *context,
	struct yt_error *error);
bool yt_config_redraw_repairs(struct yt_config_redraw_repair_state *state,
	const struct yt_record *field, float working_maximum_holds,
	const struct yt_config_record_ops *ops, void *context,
	struct yt_error *error);

int yt_date_serial(const struct yt_clock_value *date, float epoch_year,
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
