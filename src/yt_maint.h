#ifndef YT_MAINT_H
#define YT_MAINT_H

#include "yt_game.h"

#define YT_MAINTENANCE_OUTPUT_ROWS 13U
#define YT_MAINTENANCE_OUTPUT_ROW_SIZE 256U
#define YT_MAINTENANCE_OUTPUT_SIZE 2048U

struct yt_maintenance_text {
	const uint8_t *data;
	size_t length;
};

struct yt_maintenance_output_row {
	uint16_t address;
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

struct yt_maintenance_port_result {
	float elapsed;
	bool plagued;
	int selected_stock_index;
	uint64_t draws_consumed;
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

struct yt_maintenance_wanderer_result {
	int scanned_sectors;
	int removed_sector;
	int candidate_attempts;
	int target_sector;
	bool rebuilt;
	float bank_after;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_home_result {
	float planet_link_before;
	bool rebuilt;
	float ground_before_daily_update;
	float ground_after_daily_update;
	float bank_after;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_hunt_result {
	int top_record;
	float top_score;
	bool selected;
	bool used_cached_sector;
	int target_sector;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_target_result {
	int hunt_player;
	int target_sector;
	bool replaced;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_regeneration_result {
	float total_before;
	float ceiling;
	double regeneration;
	float group_one_after;
};

struct yt_maintenance_xannor_reclaim_result {
	bool original_hostile;
	bool attempted;
	bool successful;
	double defenders_after;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_relocation_result {
	bool triggered;
	int old_headquarters;
	int target_sector;
	int attempts;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_revenge_result {
	bool eligible;
	int live_sector;
	int cached_target;
};

struct yt_maintenance_xannor_split_result {
	bool split;
	bool skip_group;
	float group_one_after;
	float group_size_after;
	float group_location_after;
	uint64_t draws_consumed;
};

struct yt_maintenance_xannor_discovery_result {
	int initial_target;
	int discovery_target;
	int target_sector;
	int initial_draws;
	int attempts;
	int player_draws;
	int selected_player_record;
	uint64_t draws_consumed;
};

struct yt_maintenance_mercenary_tax_result {
	float tax_pool;
	float fleet_strength;
	int taxed_ports;
};

struct yt_maintenance_mercenary_defection_result {
	int defections;
	uint64_t draws_consumed;
};

struct yt_maintenance_mercenary_mine_result {
	float moving_before;
	float survivors;
	float losses;
	bool mine_hit;
	bool killed;
	uint64_t draws_consumed;
};

struct yt_maintenance_mercenary_planet_result {
	bool absorbed;
	bool capture_report;
	bool taking_report;
	float planet_fighters;
	float sector_fighters;
};

enum yt_maintenance_lottery_failure {
	YT_MAINTENANCE_LOTTERY_SUCCESS,
	YT_MAINTENANCE_LOTTERY_COIN,
	YT_MAINTENANCE_LOTTERY_BLANK_PLAYER,
	YT_MAINTENANCE_LOTTERY_OCCUPIED_PLANET,
	YT_MAINTENANCE_LOTTERY_OCCUPIED_SECTOR
};

struct yt_maintenance_lottery_result {
	enum yt_maintenance_lottery_failure failure;
	int player_record;
	int planet_number;
	int sector_number;
	uint64_t draws_consumed;
};

enum yt_news_rotate_step {
	YT_NEWS_ROTATE_NONE,
	YT_NEWS_ROTATE_OPEN_CURRENT,
	YT_NEWS_ROTATE_CLOSE_CURRENT,
	YT_NEWS_ROTATE_OPEN_YESTERDAY,
	YT_NEWS_ROTATE_CLOSE_YESTERDAY,
	YT_NEWS_ROTATE_KILL_YESTERDAY,
	YT_NEWS_ROTATE_RENAME_CURRENT,
};

struct yt_news_rotate_state {
	enum yt_news_rotate_step attempted;
	size_t completed_steps;
	bool complete;
};

struct yt_news_rotate_ops {
	bool (*open_append)(void *context, const char *path,
	    struct yt_error *error);
	bool (*close)(void *context, struct yt_error *error);
	bool (*kill)(void *context, const char *path,
	    struct yt_error *error);
	bool (*rename)(void *context, const char *old_path,
	    const char *new_path, struct yt_error *error);
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

enum yt_maintenance_enqueue_result {
	YT_MAINTENANCE_ENQUEUE_INVALID,
	YT_MAINTENANCE_ENQUEUE_SKIPPED,
	YT_MAINTENANCE_ENQUEUE_ADDED
};

struct yt_maintenance_route_cache {
	float *warps;
	int *successors;
	int sector_count;
};

struct yt_maintenance_xannor_route_result {
	int hops;
	bool reached_target;
	bool route_missing;
	bool exhausted;
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
bool yt_maintenance_write_header(struct yt_error *error);
bool yt_maintenance_maintain_players(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count, int today,
    yt_maintenance_score_line_fn line_output, void *line_context,
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
bool yt_maintenance_move_mercenaries(struct yt_game *game, int sector_count,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_collect_mercenary_tax(struct yt_game *game,
    int port_count, struct yt_maintenance_mercenary_tax_result *result,
    struct yt_error *error);
bool yt_maintenance_maintain_mercenary_base(struct yt_game *game,
    int sector_count, int planet_number, bool *rebuilt,
    struct yt_error *error);
bool yt_maintenance_place_mercenary_fleets(struct yt_game *game,
    int sector_count, float strength, float *hired_fighters,
    struct yt_error *error);
bool yt_maintenance_mercenary_defections(struct yt_game *game,
    int sector_count, yt_maintenance_score_line_fn line_output,
    void *line_context,
    struct yt_maintenance_mercenary_defection_result *result,
    struct yt_error *error);
bool yt_maintenance_mercenary_mines(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_mine_result *result,
    struct yt_error *error);
bool yt_maintenance_mercenary_planet_absorption(struct yt_game *game,
    int sector_number, int selected_destination, double moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector,
    struct yt_maintenance_mercenary_planet_result *result,
    struct yt_error *error);
bool yt_maintenance_mercenary_destination(struct yt_game *game,
    int sector_number, float moving_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_sector *arrival_sector, struct yt_error *error);
bool yt_maintenance_super_lottery(struct yt_game *game, int player_count,
    int planet_count, int sector_count, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_lottery_result *result,
    struct yt_error *error);
bool yt_maintenance_store_final_marker(struct yt_game *game, float serial,
    struct yt_error *error);
bool yt_maintenance_age_player(float cloak, float last_active,
    float killer_status, float today, float retention_days,
    struct yt_maintenance_player_aging_result *result);
bool yt_maintenance_player_name(const struct yt_player *player,
    bool *occupied, struct yt_maintenance_text *name,
    struct yt_error *error);
bool yt_maintenance_compose_player_aging(
    const struct yt_maintenance_text *name,
    const struct yt_maintenance_text *time_text,
    const struct yt_maintenance_text *date_text, bool cloak_expired,
    bool delete_player, struct yt_maintenance_player_output_result *result);
bool yt_maintenance_update_port(struct yt_random *random,
    struct yt_port *port, float current_day, float current_minute,
    struct yt_maintenance_port_result *result, struct yt_error *error);
bool yt_maintenance_maintain_ports(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    int *plagued_count, struct yt_error *error);
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
    int *event_count, struct yt_error *error);
bool yt_maintenance_compose_wanderer_phase(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_maintain_wanderer(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_wanderer_result *result, struct yt_error *error);
bool yt_maintenance_compose_xannor_home(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result);
bool yt_maintenance_maintain_xannor_home(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_home_result *result,
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
    struct yt_maintenance_xannor_hunt_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_target(struct yt_random *random, int sector_count,
    int hunt_player, int target_sector,
    struct yt_maintenance_xannor_target_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_groups_extract(struct yt_game *game,
    float location[21], float size[21], struct yt_error *error);
bool yt_maintenance_xannor_groups_persist(struct yt_game *game,
    const float location[21], const float size[21], struct yt_error *error);
bool yt_maintenance_xannor_group_twenty_finish(struct yt_game *game,
    int group_number, float group_location, float group_size,
    struct yt_error *error);
bool yt_maintenance_xannor_regeneration(float top_score,
    const float size[21], struct yt_maintenance_xannor_regeneration_result *result);
bool yt_maintenance_xannor_headquarters_reclaim(struct yt_game *game,
    float location[21], float size[21], yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_xannor_reclaim_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_headquarters_relocate(struct yt_game *game,
    float location[21], bool original_hostile, float group_one,
    double regeneration, const uint8_t *blank,
    size_t blank_length, yt_maintenance_score_line_fn line_output,
    void *line_context, struct yt_maintenance_xannor_relocation_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_revenge_slot(struct yt_game *game,
    const float *player_sector, size_t cache_count,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_maintenance_xannor_revenge_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_roaming_split(struct yt_random *random,
    int group_number, float *group_one, float *group_size,
    float *group_location, float top_score, float headquarters,
    struct yt_maintenance_xannor_split_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_candidate_discovery(struct yt_game *game,
    const float *player_sector, const float *player_cloak,
    size_t cache_count, int current_sector, int revenge_live_sector,
    int revenge_cached_target,
    struct yt_maintenance_xannor_discovery_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_target_override(int group_number,
    int discovered_target, float group_one, float top_score,
    int headquarters, int revenge_live_sector, int top_player_target,
    int *target, struct yt_error *error);
bool yt_maintenance_random_integer(struct yt_random *random, int range,
    int *value, struct yt_error *error);
bool yt_maintenance_nested_integer(struct yt_random *random, int count,
    int range, int *value, struct yt_error *error);
bool yt_maintenance_xannor_defense(struct yt_random *random,
    float *group_size, float *defense_fighters, float *defense_owner,
    struct yt_error *error);

bool yt_maintenance_mercenary_stays(float planet_link, float draw);

bool yt_maintenance_mercenary_attacks(float defense_owner, float draw);
bool yt_maintenance_xannor_player_combat(struct yt_random *random,
    float *player_fighters, float *player_shields, float *xannor_fighters,
    struct yt_maintenance_xannor_player_result *result,
    struct yt_error *error);
bool yt_maintenance_xannor_player_line(const char *player_name,
    const struct yt_maintenance_xannor_player_result *result,
    float xannor_fighters, float player_shields, bool player_killed,
    char *line, size_t line_size);
bool yt_maintenance_xannor_player_arrival(struct yt_game *game,
    float *player_sector, float *player_cloak, size_t cache_count,
    int player_record, float *xannor_fighters,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
bool yt_maintenance_xannor_planet_arrival(struct yt_game *game,
    float *group_location, float *group_size, struct yt_sector *sector,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error);
enum yt_maintenance_enqueue_result yt_maintenance_route_enqueue(int neighbor,
    int predecessor, int sector_count, int *queue, size_t queue_capacity,
    size_t *tail, int *previous);
bool yt_maintenance_route_next_hop(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, int source, int target,
    int *next_hop, struct yt_error *error);
void yt_maintenance_route_cache_free(
    struct yt_maintenance_route_cache *cache);
bool yt_maintenance_xannor_route_arrivals(struct yt_game *game,
    struct yt_maintenance_route_cache *cache, float *player_sector,
    float *player_cloak, size_t cache_count, int group_number,
    int target_sector, float location[21], float size[21],
    struct yt_maintenance_xannor_route_result *result,
    struct yt_error *error);
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
bool yt_radio_append_maintenance(const char *text, float sender,
    float recipient, struct yt_error *error);
bool yt_radio_compact(struct yt_error *error);
bool yt_news_rotate_run(struct yt_news_rotate_state *state,
	const struct yt_news_rotate_ops *ops, void *context,
	struct yt_error *error);
bool yt_news_rotate(struct yt_error *error);
bool yt_news_append(const char *text, struct yt_error *error);
bool yt_news_append_bytes(const uint8_t *text, size_t length,
    struct yt_error *error);
bool yt_news_append_login_bytes(const uint8_t *time_text,
    size_t time_length, const uint8_t *player_name, size_t player_length,
    struct yt_error *error);
bool yt_news_append_login(const char *time_text, const char *player_name,
    struct yt_error *error);
bool yt_news_append_new_player(const char *date_text, const char *player_name,
    struct yt_error *error);
bool yt_news_append_game_full(const char *date_text, const char *player_name,
    struct yt_error *error);

#endif
