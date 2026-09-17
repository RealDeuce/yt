#ifndef YT_MAINT_H
#define YT_MAINT_H

#include "yt_game.h"
#include "yt_names.h"
#include "yt_platform.h"

#define YT_MAINTENANCE_OUTPUT_ROWS 13U
#define YT_MAINTENANCE_OUTPUT_ROW_SIZE 256U
#define YT_MAINTENANCE_OUTPUT_SIZE 2048U

struct yt_maintenance_text {
	const uint8_t *data;
	size_t length;
};

enum yt_maintenance_output_row_id {
	YT_MAINT_ROW_WRAPPER_BLANK,
	YT_MAINT_ROW_WRAPPER_COMPLETED,
	YT_MAINT_ROW_ENTRY_SAME_DAY_BLANK,
	YT_MAINT_ROW_ENTRY_SAME_DAY_MESSAGE,
	YT_MAINT_ROW_ENTRY_BANNER_BLANK,
	YT_MAINT_ROW_ENTRY_TITLE,
	YT_MAINT_ROW_ENTRY_BYLINE,
	YT_MAINT_ROW_ENTRY_REVISION_LEADING_BLANK,
	YT_MAINT_ROW_ENTRY_REVISION_INDENT,
	YT_MAINT_ROW_ENTRY_REVISION,
	YT_MAINT_ROW_ENTRY_WARNING_BLANK,
	YT_MAINT_ROW_ENTRY_WARNING,
	YT_MAINT_ROW_ENTRY_PLAYER_PHASE_BLANK,
	YT_MAINT_ROW_ENTRY_PLAYER_PHASE,
	YT_MAINT_ROW_ENTRY_PLAYER_PHASE_TRAILING_BLANK,
	YT_MAINT_ROW_PLAYER_CLOAK_EXPIRED,
	YT_MAINT_ROW_PLAYER_DELETED,
	YT_MAINT_ROW_PORT_PHASE_BLANK,
	YT_MAINT_ROW_PORT_PHASE_HEADER,
	YT_MAINT_ROW_PORT_PLAGUE_BLANK,
	YT_MAINT_ROW_PORT_PLAGUE_REPORT,
	YT_MAINT_ROW_PLANET_PHASE_BLANK,
	YT_MAINT_ROW_PLANET_PHASE_HEADER,
	YT_MAINT_ROW_PLANET_EVENT_BLANK,
	YT_MAINT_ROW_PLANET_EVENT_SUMMARY,
	YT_MAINT_ROW_PLANET_EVENT_PRODUCTION,
	YT_MAINT_ROW_PLANET_EVENT_GROUND,
	YT_MAINT_ROW_PLANET_EVENT_EXPENSE,
	YT_MAINT_ROW_WANDERER_PHASE_BLANK,
	YT_MAINT_ROW_WANDERER_PHASE_HEADER,
	YT_MAINT_ROW_WANDERER_MISSING,
	YT_MAINT_ROW_WANDERER_REGENERATED,
	YT_MAINT_ROW_WANDERER_RESULT_BLANK,
	YT_MAINT_ROW_WANDERER_WARPED,
	YT_MAINT_ROW_XANNOR_HOME_PHASE_BLANK,
	YT_MAINT_ROW_XANNOR_HOME_PHASE_HEADER,
	YT_MAINT_ROW_XANNOR_HOME_REBUILD_BLANK,
	YT_MAINT_ROW_XANNOR_HOME_CREATED,
	YT_MAINT_ROW_XANNOR_HOME_LINKED,
	YT_MAINT_ROW_XANNOR_HUNT_PHASE_BLANK,
	YT_MAINT_ROW_XANNOR_HUNT_PROCESSING,
	YT_MAINT_ROW_XANNOR_HUNT_SEPARATOR,
	YT_MAINT_ROW_XANNOR_HUNT_LOCATING,
	YT_MAINT_ROW_XANNOR_HUNT_TARGET_BLANK,
	YT_MAINT_ROW_XANNOR_HUNT_TARGET,
	YT_MAINT_ROW_XANNOR_REGENERATION_BLANK,
	YT_MAINT_ROW_XANNOR_REGENERATION_REPORT,
	YT_MAINT_ROW_XANNOR_REGENERATION_TRAILING_BLANK,
	YT_MAINT_ROW_XANNOR_RECLAIM_ATTEMPT,
	YT_MAINT_ROW_XANNOR_RECLAIM_RESULT,
	YT_MAINT_ROW_XANNOR_RELOCATION_MESSAGE,
	YT_MAINT_ROW_XANNOR_RELOCATION_BLANK,
	YT_MAINT_ROW_XANNOR_REVENGE_BLANK,
	YT_MAINT_ROW_XANNOR_REVENGE_MESSAGE,
	YT_MAINT_ROW_XANNOR_REVENGE_TRAILING_BLANK,
	YT_MAINT_ROW_XANNOR_ROAMING_MESSAGE,
	YT_MAINT_ROW_XANNOR_ROAMING_BLANK,
	YT_MAINT_ROW_XANNOR_GROUP_REPORT,
	YT_MAINT_ROW_XANNOR_PATH_ERROR,
	YT_MAINT_ROW_MERCENARY_START_BLANK,
	YT_MAINT_ROW_MERCENARY_START_SEPARATOR,
	YT_MAINT_ROW_MERCENARY_TAX_REPORT,
	YT_MAINT_ROW_MERCENARY_PHASE_BLANK,
	YT_MAINT_ROW_MERCENARY_PHASE_HEADER,
	YT_MAINT_ROW_MERCENARY_PHASE_SEPARATOR,
	YT_MAINT_ROW_MERCENARY_BASE_CHECK,
	YT_MAINT_ROW_MERCENARY_REBUILD_BLANK,
	YT_MAINT_ROW_MERCENARY_REBUILT,
	YT_MAINT_ROW_MERCENARY_HIRED,
	YT_MAINT_ROW_MERCENARY_MOVEMENT,
	YT_MAINT_ROW_MESSAGE_COMPACTION_BLANK,
	YT_MAINT_ROW_MESSAGE_COMPACTION_HEADER,
};

struct yt_maintenance_output_row {
	enum yt_maintenance_output_row_id id;
	bool newline;
	uint8_t data[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t length;
};

struct yt_maintenance_output_result {
	struct yt_maintenance_output_row rows[YT_MAINTENANCE_OUTPUT_ROWS];
	size_t row_count;
	uint8_t output[YT_MAINTENANCE_OUTPUT_SIZE];
	size_t output_length;
	size_t final_column;
};

struct yt_maintenance_player_aging_result {
	float cached_cloak;
	float persisted_cloak;
	float cutoff;
	bool cloak_written;
	bool cloak_expired;
	bool delete_player;
};

struct yt_maintenance_player_output_result {
	struct yt_maintenance_output_result screen;
	uint8_t radio_message[256];
	size_t radio_length;
	bool deletion_reached;
};

enum yt_maintenance_planet_event {
	YT_MAINTENANCE_PLANET_NO_EVENT,
	YT_MAINTENANCE_PLANET_PLAGUE,
	YT_MAINTENANCE_PLANET_CIVIL_WAR
};

struct yt_maintenance_planet_result {
	float elapsed;
	enum yt_maintenance_planet_event event;
	float old_event_total;
	float new_event_total;
	float old_event_ground;
	float new_event_ground;
	float civil_war_expense;
	bool emit_ground_line;
	uint64_t draws_consumed;
};

bool yt_maintenance_xannor_should_retarget(float group_location,
    float group_size);
bool yt_maintenance_xannor_route_complete(float group_location,
    int target_sector);
bool yt_maintenance_xannor_bypass_initial_arrival(int group_number,
    float group_location, float top_player_target);
bool yt_maintenance_xannor_should_attack_hunt_player(int group_number,
    int hunt_player);
bool yt_maintenance_xannor_advance_group(int group_number,
    int *next_group);
bool yt_maintenance_xannor_post_planet_exhausted(float group_location,
    float group_size);
bool yt_maintenance_xannor_player_scan_admit(int group_number,
    float group_location, float player_location, float cached_cloak,
    float cloak_draw);
bool yt_maintenance_xannor_player_scan_continue(int player_record,
    int player_count);

struct yt_maintenance_route_cache {
	float *warps;
	int *successors;
	int sector_count;
};

struct yt_maintenance_xannor_player_result {
	float player_fighter_losses;
	float xannor_losses;
};

typedef bool (*yt_maintenance_score_line_fn)(void *context,
    const uint8_t *line, size_t length, struct yt_error *error);

bool yt_maintenance_run(struct yt_error *error);
bool yt_maintenance_default_headquarters(float *headquarters);
bool yt_maintenance_clear_protected_mines(struct yt_game *game,
    struct yt_error *error);
bool yt_maintenance_write_header(const struct yt_clock *clock,
    struct yt_error *error);
bool yt_maintenance_same_day(float stored_marker, float computed_serial);
bool yt_maintenance_compose_entry(bool same_day,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_wrapper(
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_message_compaction(
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_port_phase(const uint8_t *blank,
    size_t blank_length, int plagued_count,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_mercenary_phase(const uint8_t *blank,
    size_t blank_length, float tax_pool, bool rebuilt_base,
    float hired_fighters, struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_mercenary_movement(double moving_fighters,
    float origin_sector, struct yt_maintenance_output_result *result);
bool yt_maintenance_collect_mercenary_tax(struct yt_game *game,
    int port_count, float *tax_pool, float *fleet_strength,
    struct yt_error *error);
bool yt_maintenance_maintain_mercenary_base(struct yt_game *game,
    int sector_count, int planet_number, bool *rebuilt,
    struct yt_error *error);
bool yt_maintenance_place_mercenary_fleets(struct yt_game *game,
    int sector_count, float strength, float *hired_fighters,
    struct yt_error *error);
bool yt_maintenance_mercenary_defections(struct yt_game *game,
    int sector_count, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error);
bool yt_maintenance_mercenary_mines(struct yt_game *game,
    int sector_number, float *moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector,
    struct yt_error *error);
bool yt_maintenance_mercenary_planet_absorption(struct yt_game *game,
    int sector_number, int selected_destination, double moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, bool *absorbed,
    struct yt_error *error);
bool yt_maintenance_super_lottery(struct yt_game *game, int player_count,
    int planet_count, int sector_count, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error);
bool yt_maintenance_store_final_marker(struct yt_game *game, float serial,
    struct yt_error *error);
bool yt_maintenance_age_player(float cloak, float last_active,
    float killer_status, float today, float retention_days,
    struct yt_maintenance_player_aging_result *result);
bool yt_maintenance_compose_player_aging(
    const struct yt_maintenance_text *name,
    const struct yt_maintenance_text *time_text,
    const struct yt_maintenance_text *date_text, bool cloak_expired,
    bool delete_player, struct yt_maintenance_player_output_result *result);
bool yt_maintenance_update_port(struct yt_random *random,
    struct yt_port *port, float current_day, float current_minute,
    bool *plagued, struct yt_error *error);
bool yt_maintenance_maintain_ports(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_compose_planet_phase(const uint8_t *blank,
    size_t blank_length, const struct yt_maintenance_text *planet_name,
    const struct yt_maintenance_planet_result *mutation,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_update_planet(struct yt_random *random,
    struct yt_planet *planet, float current_day, float current_minute,
    struct yt_maintenance_planet_result *result, struct yt_error *error);
bool yt_maintenance_maintain_planets(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_compose_wanderer_phase(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_maintain_wanderer(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_compose_xannor_home(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_maintain_xannor_home(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_compose_xannor_hunt(const uint8_t *blank,
    size_t blank_length, const struct yt_maintenance_text *hunt_name,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_regeneration(
    const uint8_t *blank, size_t blank_length,
    double regeneration, struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_reclaim_attempt(
    const struct yt_maintenance_text *opponent,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_reclaim_result(bool successful,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_relocation(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_revenge(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_roaming(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_group(int group_number, float group_size,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_compose_xannor_path_error(float source, float target,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_xannor_hunt(struct yt_game *game,
    const float *player_sector, const float *player_cloak, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    int *hunt_player, float *top_score, int *target_sector,
    struct yt_error *error);
bool yt_maintenance_xannor_target(struct yt_random *random, int sector_count,
    int *hunt_player, int *target_sector,
    struct yt_error *error);
bool yt_maintenance_xannor_groups_extract(struct yt_game *game,
    float location[21], float size[21], struct yt_error *error);
bool yt_maintenance_xannor_groups_persist(struct yt_game *game,
    const float location[21], const float size[21], struct yt_error *error);
bool yt_maintenance_xannor_group_twenty_finish(struct yt_game *game,
    int group_number, float group_location, float group_size,
    struct yt_error *error);
bool yt_maintenance_xannor_regeneration(float top_score,
    float size[21], double *regeneration);
bool yt_maintenance_xannor_headquarters_reclaim(struct yt_game *game,
    float location[21], float size[21], yt_maintenance_score_line_fn line_output,
    void *line_context, bool *original_hostile,
    struct yt_error *error);
bool yt_maintenance_xannor_headquarters_relocate(struct yt_game *game,
    float location[21], bool original_hostile, float group_one,
    double regeneration, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_error *error);
bool yt_maintenance_xannor_revenge_slot(struct yt_game *game,
    const float *player_sector, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    int *live_sector, int *cached_target,
    struct yt_error *error);
bool yt_maintenance_xannor_roaming_split(struct yt_random *random,
    int group_number, float *group_one, float *group_size,
    float *group_location, float top_score, float headquarters,
    bool *skip_group,
    struct yt_error *error);
bool yt_maintenance_xannor_candidate_discovery(struct yt_game *game,
    const float *player_sector, const float *player_cloak,
    size_t cache_count, int current_sector, int revenge_live_sector,
    int revenge_cached_target, int *target_sector,
    struct yt_error *error);
bool yt_maintenance_xannor_target_override(int group_number,
    int discovered_target, float group_one, float top_score,
    int headquarters, int revenge_live_sector, int top_player_target,
    int *target, struct yt_error *error);
bool yt_maintenance_xannor_defense(struct yt_random *random,
    float *group_size, float *defense_fighters, float *defense_owner,
    struct yt_error *error);
bool yt_maintenance_xannor_sector_arrival(struct yt_game *game,
    int sector_number, float *group_size, struct yt_sector *sector,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);

bool yt_maintenance_mercenary_stays(float planet_link, float draw);

bool yt_maintenance_mercenary_attacks(float defense_owner, float draw);
bool yt_maintenance_xannor_player_line_bytes(const uint8_t *player_name,
    size_t player_name_length,
    const struct yt_maintenance_xannor_player_result *result,
    float xannor_fighters, float player_shields, bool player_killed,
    uint8_t *line, size_t line_size, size_t *line_length);
bool yt_maintenance_xannor_player_arrival(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count,
    int player_record, float *xannor_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_xannor_target_finish(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count,
    bool reached_target, int group_number, int hunt_player,
    float *group_location, float *group_size,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_xannor_planet_arrival(struct yt_game *game,
    float *group_location, float *group_size, struct yt_sector *sector,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_route_next_hop(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, int source, int target,
    int *next_hop, struct yt_error *error);
void yt_maintenance_route_cache_free(
    struct yt_maintenance_route_cache *cache);
bool yt_maintenance_scoreboard(struct yt_game *game,
    yt_maintenance_score_line_fn line_output, void *context,
    struct yt_error *error);
bool yt_maintenance_remove_alias(const char *player_name,
    struct yt_error *error);
bool yt_maintenance_expire_player(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count,
    int player_record, struct yt_player *player, struct yt_error *error);
bool yt_maintenance_immediate_death(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count,
    int victim_record, float killer, struct yt_player *victim,
    struct yt_error *error);
bool yt_radio_append_maintenance_bytes(const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error);
bool yt_radio_append_maintenance(const char *text, float sender,
    float recipient, struct yt_error *error);
bool yt_radio_compact(struct yt_error *error);
bool yt_news_rotate(struct yt_error *error);
bool yt_news_append(const char *text, struct yt_error *error);
bool yt_news_append_bytes(const uint8_t *text, size_t length,
    struct yt_error *error);
bool yt_news_append_login_bytes(const uint8_t *time_text,
    size_t time_length, const uint8_t *player_name, size_t player_length,
    struct yt_error *error);
bool yt_news_append_new_player(const char *date_text, const char *player_name,
    struct yt_error *error);
bool yt_news_append_game_full(const char *date_text, const char *player_name,
    struct yt_error *error);

#endif
