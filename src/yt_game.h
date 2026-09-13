#ifndef YT_GAME_H
#define YT_GAME_H

#include "qb.h"
#include "yt_brun_fatal.h"
#include "yt_config.h"
#include "yt_player_cache.h"
#include "yt_random.h"

struct yt_game;

struct yt_player {
	struct yt_record record;
	char name[42];
	float last_active;
	float killed_by;
	float turns;
	float shields;
	float sector;
	float fighters;
	float holds;
	float ore;
	float organics;
	float equipment;
	float credits;
	float name_length;
	float team;
	float danger_scanner;
	float missiles;
	float lottery_plays;
	float score;
	float plasma;
	float ports_owned;
	float ground_forces;
	float cloak;
	float mines;
};

typedef bool (*yt_credit_mutation_apply_fn)(void *context,
	float player_record, float argument, struct yt_player *player,
	bool *hydrated, struct yt_error *error);
typedef void (*yt_player_record_store_fn)(void *context,
	const uint8_t raw[4]);
typedef bool (*yt_destroyed_truth_fn)(void *context);

bool yt_game_load_startup_configuration(struct yt_game *game,
	const char *path, bool local_mode, struct yt_player_cache *player_cache,
	float disruption_sectors[2], float *local_screen,
	struct yt_error *error);

struct yt_sector {
	struct yt_record record;
	float warps[6];
	float port;
	float fighters;
	float fighter_owner;
	float planet;
	float metadata;
	float mines;
};

struct yt_team {
	int id;
	struct yt_sector overlay;
	char name[42];
	size_t name_length;
	char password[5];
	int captain;
	int roster[4];
	bool live;
	bool full;
};

struct yt_port {
	struct yt_record record;
	char name[42];
	float commodity_class;
	float last_day;
	float stock[3];
	float production[3];
	float factor[3];
	float name_length;
	float treasury;
	float sector;
	float owner;
	float last_minute;
};

struct yt_planet {
	struct yt_record record;
	char name[42];
	float last_day;
	float production[3];
	float stock[3];
	float missiles;
	float owner;
	float ground_forces;
	float name_length;
	float last_minute;
	float plasma;
	float bank;
	float mines;
	float fighters;
};

enum yt_xannor_victory_output_kind {
	YT_XANNOR_VICTORY_RAW,
	YT_XANNOR_VICTORY_LINE,
	YT_XANNOR_VICTORY_BOLD_LINE,
};

struct yt_xannor_victory_state {
	float current_player;
	struct yt_player player;
	struct yt_sector sector;
	float foreground;
	float pager_foreground;
	float blink;
	float awarded_credits;
	uint8_t winner[128];
	size_t winner_length;
	unsigned sounds_completed;
	unsigned news_completed;
	unsigned radio_completed;
};

typedef bool (*yt_xannor_victory_file_fn)(void *context, const char *path,
	struct yt_error *error);
typedef bool (*yt_xannor_victory_present_fn)(void *context,
	const uint8_t *text, size_t length,
	enum yt_xannor_victory_output_kind kind, const char *operation,
	struct yt_error *error);
typedef bool (*yt_xannor_victory_wait_fn)(void *context, double seconds,
	const char *operation, struct yt_error *error);
typedef void (*yt_xannor_victory_foreground_fn)(void *context,
	float foreground);
typedef void (*yt_xannor_victory_blink_fn)(void *context, float blink);
typedef void (*yt_xannor_victory_clear_queue_fn)(void *context);
typedef bool (*yt_xannor_victory_sound_fn)(void *context, float selector,
	const char *operation, struct yt_error *error);
typedef bool (*yt_xannor_victory_news_fn)(void *context,
	const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_xannor_victory_radio_fn)(void *context,
	const uint8_t *text, size_t length, float sender, float recipient,
	struct yt_error *error);
typedef bool (*yt_xannor_victory_read_sector_fn)(void *context,
	int logical_sector, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_xannor_victory_write_sector_fn)(void *context,
	int logical_sector, struct yt_sector *sector, struct yt_error *error);

struct yt_xannor_victory_ops {
	yt_xannor_victory_file_fn play_file;
	yt_xannor_victory_present_fn present;
	yt_xannor_victory_wait_fn wait;
	yt_xannor_victory_foreground_fn set_foreground;
	yt_xannor_victory_blink_fn set_blink;
	yt_xannor_victory_clear_queue_fn clear_queue;
	yt_credit_mutation_apply_fn mutate_credits;
	yt_xannor_victory_sound_fn sound;
	yt_xannor_victory_news_fn append_news;
	yt_xannor_victory_radio_fn append_radio;
	yt_xannor_victory_read_sector_fn read_sector;
	yt_xannor_victory_write_sector_fn write_sector;
};

bool yt_xannor_victory_run(struct yt_xannor_victory_state *state,
	const struct yt_xannor_victory_ops *ops, void *context,
	struct yt_error *error);

/*
 * Joins the documented MKS$ allocator-owner corruption at YT-SUB:A995 to
 * the shared BRUN 0AC9 terminal.  Public ERR 14/16 results remain in the
 * ordinary shared error-router domain and are not accepted here.
 */
bool yt_xannor_victory_mks_internal_fatal_run(uint16_t module_segment,
	bool redirected_stdin, bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_internal_fatal_state *state);
bool yt_main_startup_internal_fatal_run(uint16_t module_segment,
	bool redirected_stdin, bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_internal_fatal_state *state);

struct yt_post_login_repairs {
	bool sector;
	bool holds;
	unsigned writes;
};

enum yt_info_panel_output_kind {
	YT_INFO_PANEL_LINE,
	YT_INFO_PANEL_FIXED,
};

struct yt_info_panel_state {
	const uint8_t *cached_name;
	size_t cached_name_length;
	float anti_cloak;
	float foreground;
	float background;
	float bold;
	uint8_t time_text[64];
	size_t time_text_length;
	struct yt_player player;
};

typedef bool (*yt_info_panel_refresh_fn)(void *context, uint8_t *text,
	size_t capacity, size_t *length, struct yt_error *error);
typedef bool (*yt_info_panel_team_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_info_panel_read_player_fn)(void *context,
	struct yt_player *player, struct yt_error *error);
typedef bool (*yt_info_panel_present_fn)(void *context,
	const uint8_t *text, size_t length, enum yt_info_panel_output_kind kind,
	float width, struct yt_info_panel_state *state,
	struct yt_error *error);

struct yt_info_panel_ops {
	yt_info_panel_refresh_fn refresh_time;
	yt_info_panel_team_fn team;
	yt_info_panel_read_player_fn read_player;
	yt_info_panel_present_fn present;
};

bool yt_info_panel_run(struct yt_info_panel_state *state,
	const struct yt_info_panel_ops *ops, void *context,
	struct yt_error *error);

enum yt_spy_output_kind {
	YT_SPY_LINE,
	YT_SPY_BOLD_LINE,
	YT_SPY_BOLD_RAW,
	YT_SPY_ATTENTION,
};

struct yt_spy_sweep_state {
	int active_spies;
	int *spy_sectors;
	int *last_reported_sectors;
	int current_player_record;
	float last_player_record;
	float disruption_sectors[2];
	struct yt_player_cache *player_cache;
	bool found;
	float foreground;
	float background;
	float bold;
	float blink;
};

typedef bool (*yt_spy_read_sector_fn)(void *context, int logical_sector,
	struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_spy_update_planet_fn)(void *context, float planet_link,
	struct yt_error *error);
typedef bool (*yt_spy_read_planet_fn)(void *context, float planet_link,
	struct yt_planet *planet, struct yt_error *error);
typedef bool (*yt_spy_read_player_fn)(void *context, float player_record,
	struct yt_player *player, struct yt_error *error);
typedef bool (*yt_spy_read_team_fn)(void *context, float team_id,
	struct yt_sector *overlay, struct yt_error *error);
typedef bool (*yt_spy_random_fn)(void *context, float *value,
	struct yt_error *error);
typedef bool (*yt_spy_sound_fn)(void *context, float selector,
	struct yt_error *error);
typedef bool (*yt_spy_present_fn)(void *context, const uint8_t *text,
	size_t length, enum yt_spy_output_kind kind,
	struct yt_spy_sweep_state *state, struct yt_error *error);
typedef bool (*yt_spy_pause_fn)(void *context,
	struct yt_spy_sweep_state *state, struct yt_error *error);

struct yt_spy_sweep_ops {
	yt_spy_read_sector_fn read_sector;
	yt_spy_update_planet_fn update_planet;
	yt_spy_read_planet_fn read_planet;
	yt_spy_read_player_fn read_player;
	yt_spy_read_team_fn read_team;
	yt_spy_random_fn random;
	yt_spy_sound_fn sound;
	yt_spy_present_fn present;
	yt_spy_pause_fn pause;
};

bool yt_spy_sweep_run(struct yt_spy_sweep_state *state,
	const struct yt_spy_sweep_ops *ops, void *context,
	struct yt_error *error);

enum yt_sector_force_route {
	YT_SECTOR_FORCE_FRIENDLY,
	YT_SECTOR_FORCE_HOSTILE,
	YT_SECTOR_FORCE_OWNER_GET,
};

enum yt_port_owner_kind {
	YT_PORT_OWNER_SILENT,
	YT_PORT_OWNER_SELF,
	YT_PORT_OWNER_OTHER,
	YT_PORT_OWNER_INVALID,
};

enum yt_death_port_route {
	YT_DEATH_PORT_UNMATCHED,
	YT_DEATH_PORT_TRANSFERRED,
	YT_DEATH_PORT_CLEARED,
};

enum yt_hostile_menu_route {
	YT_HOSTILE_MENU_HELP,
	YT_HOSTILE_MENU_SECTOR,
	YT_HOSTILE_MENU_INFO,
	YT_HOSTILE_MENU_INVALID,
	YT_HOSTILE_MENU_ATTACK,
	YT_HOSTILE_MENU_QUIT,
	YT_HOSTILE_MENU_BRIBE,
	YT_HOSTILE_MENU_MINE,
	YT_HOSTILE_MENU_WARP,
	YT_HOSTILE_MENU_TEAM,
};

enum yt_main_shell_route {
	YT_MAIN_SHELL_SOUND,
	YT_MAIN_SHELL_SENSORS,
	YT_MAIN_SHELL_DISPLAY,
	YT_MAIN_SHELL_WARP,
	YT_MAIN_SHELL_MISSILE,
	YT_MAIN_SHELL_PLASMA,
	YT_MAIN_SHELL_ATTACK,
	YT_MAIN_SHELL_BUY_PORT,
	YT_MAIN_SHELL_COMPUTER,
	YT_MAIN_SHELL_FIGHTERS,
	YT_MAIN_SHELL_LAND,
	YT_MAIN_SHELL_MOVE,
	YT_MAIN_SHELL_TRADE,
	YT_MAIN_SHELL_QUIT,
	YT_MAIN_SHELL_TEAM,
	YT_MAIN_SHELL_MINES,
	YT_MAIN_SHELL_COLLECT,
	YT_MAIN_SHELL_GENESIS,
	YT_MAIN_SHELL_RENAME_PORT,
	YT_MAIN_SHELL_VERSION,
	YT_MAIN_SHELL_INFO,
	YT_MAIN_SHELL_INSTRUCTIONS,
	YT_MAIN_SHELL_HELP,
	YT_MAIN_SHELL_INVALID,
};

enum yt_computer_newspaper_choice {
	YT_COMPUTER_NEWSPAPER_NONE,
	YT_COMPUTER_NEWSPAPER_TODAY,
	YT_COMPUTER_NEWSPAPER_YESTERDAY,
};

enum yt_computer_newspaper_choice yt_computer_newspaper_select(
	const char *response);

#define YT_RADIO_SEND_RECIPIENTS 4U

enum yt_hostile_attack_admission {
	YT_HOSTILE_ATTACK_NO_FIGHTERS,
	YT_HOSTILE_ATTACK_TOO_MANY,
	YT_HOSTILE_ATTACK_LESS_THAN_ONE,
	YT_HOSTILE_ATTACK_ADMITTED,
};

enum yt_hostile_surrender_route {
	YT_HOSTILE_SURRENDER_PLAYER,
	YT_HOSTILE_SURRENDER_XANNOR,
	YT_HOSTILE_SURRENDER_MERCENARY,
	YT_HOSTILE_SURRENDER_QUIET,
};

enum yt_hostile_surrender_answer {
	YT_HOSTILE_SURRENDER_ANSWER_NO,
	YT_HOSTILE_SURRENDER_ANSWER_YES,
	YT_HOSTILE_SURRENDER_ANSWER_EMPTY,
};

enum yt_hostile_surrender_output_kind {
	YT_HOSTILE_SURRENDER_RADIO_ROW,
	YT_HOSTILE_SURRENDER_CAPTAIN_ROW,
	YT_HOSTILE_SURRENDER_WISH_ROW,
	YT_HOSTILE_SURRENDER_PROMPT_BLANK,
	YT_HOSTILE_SURRENDER_JOINED_ROW,
	YT_HOSTILE_SURRENDER_COUNT_ROW,
	YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW,
	YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW,
};

enum yt_bribe_forced_admission {
	YT_BRIBE_FORCED_FATAL,
	YT_BRIBE_FORCED_LESS_THAN_ONE,
	YT_BRIBE_FORCED_ATTACK,
};

enum yt_sector_mine_admission {
	YT_SECTOR_MINE_BELOW_ONE,
	YT_SECTOR_MINE_ABOVE_CARRIED,
	YT_SECTOR_MINE_ACCEPTED,
};

enum yt_drop_mines_route {
	YT_DROP_MINES_INCOMPLETE,
	YT_DROP_MINES_NO_MINES,
	YT_DROP_MINES_UNION_REFUSAL,
	YT_DROP_MINES_CANCELLED,
	YT_DROP_MINES_ACCEPTED,
};

enum yt_drop_mines_output_kind {
	YT_DROP_MINES_NO_MINES_ROW,
	YT_DROP_MINES_UNION_ROW,
	YT_DROP_MINES_PROMPT_BLANK,
	YT_DROP_MINES_PROMPT,
	YT_DROP_MINES_SUCCESS_BLANK,
	YT_DROP_MINES_SUCCESS_ROW,
};

struct yt_drop_mines_state {
	int current_player_record;
	int current_sector;
	struct yt_player current;
	struct yt_sector sector;
	float carried;
	float amount;
	uint8_t amount_raw[4];
	float player_mines_after;
	float sector_mines_before;
	float sector_mines_after;
	bool player_read;
	bool negative_repair;
	bool repair_written;
	bool repair_flushed;
	bool amount_stored;
	bool suppression_set;
	bool player_written;
	bool player_flushed;
	bool sector_read;
	bool sector_written;
	bool sector_flushed;
	enum yt_drop_mines_route route;
	bool complete;
};

typedef bool (*yt_drop_mines_read_player_fn)(void *context,
	int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_drop_mines_write_player_fn)(void *context,
	int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_drop_mines_flush_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_drop_mines_read_sector_fn)(void *context,
	int sector_number, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_drop_mines_write_sector_fn)(void *context,
	int sector_number, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_drop_mines_present_fn)(void *context,
	const uint8_t *text, size_t length,
	enum yt_drop_mines_output_kind kind, struct yt_error *error);
typedef bool (*yt_drop_mines_amount_fn)(void *context, char *response,
	size_t capacity, struct yt_error *error);
typedef void (*yt_drop_mines_suppress_fn)(void *context);
typedef bool (*yt_drop_mines_sound_fn)(void *context, float selector,
	struct yt_error *error);

struct yt_drop_mines_ops {
	yt_drop_mines_read_player_fn read_player;
	yt_drop_mines_write_player_fn write_player;
	yt_drop_mines_flush_fn flush;
	yt_drop_mines_read_sector_fn read_sector;
	yt_drop_mines_write_sector_fn write_sector;
	yt_drop_mines_present_fn present;
	yt_drop_mines_amount_fn amount;
	yt_drop_mines_suppress_fn suppress;
	yt_drop_mines_sound_fn sound;
};

bool yt_drop_mines_run(struct yt_drop_mines_state *state,
	const struct yt_drop_mines_ops *ops, void *context,
	struct yt_error *error);

enum yt_sector_mine_damage_field {
	YT_SECTOR_MINE_DAMAGE_SHIELDS = 1U << 0,
	YT_SECTOR_MINE_DAMAGE_FIGHTERS = 1U << 1,
	YT_SECTOR_MINE_DAMAGE_HOLDS = 1U << 2,
	YT_SECTOR_MINE_DAMAGE_ORE = 1U << 3,
	YT_SECTOR_MINE_DAMAGE_ORGANICS = 1U << 4,
	YT_SECTOR_MINE_DAMAGE_EQUIPMENT = 1U << 5,
	YT_SECTOR_MINE_DAMAGE_SCANNER = 1U << 6,
	YT_SECTOR_MINE_DAMAGE_MISSILES = 1U << 7,
	YT_SECTOR_MINE_DAMAGE_CLOAK = 1U << 8,
	YT_SECTOR_MINE_DAMAGE_CARRIED_MINES = 1U << 9,
};

enum yt_sector_mine_loss_kind {
	YT_SECTOR_MINE_LOSS_FIGHTERS,
	YT_SECTOR_MINE_LOSS_CLOAK,
	YT_SECTOR_MINE_LOSS_MISSILES,
	YT_SECTOR_MINE_LOSS_MINES,
	YT_SECTOR_MINE_LOSS_ORE,
	YT_SECTOR_MINE_LOSS_ORGANICS,
	YT_SECTOR_MINE_LOSS_EQUIPMENT,
	YT_SECTOR_MINE_LOSS_EMPTY_HOLDS,
};

enum yt_sector_mine_output_kind {
	YT_SECTOR_MINE_OUTPUT_LINE,
	YT_SECTOR_MINE_OUTPUT_BOLD_LINE,
	YT_SECTOR_MINE_OUTPUT_BOLD_RAW,
};
struct yt_sector_mine_state {
	int current_player_record;
	float current_sector;
	uint8_t conversion_mode;
	float foreground;
	float background;
	float blink;
	int pager_foreground;
	bool *destroyed;
	struct yt_player player;
	struct yt_sector sector;
	float mines_before;
	float batch;
	unsigned touched;
	size_t batches;
	bool terminal;
	bool complete;
};
struct yt_sector_mine_ops {
	bool (*read_current)(void *context, struct yt_player *player,
	    struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_sector_mine_output_kind kind, struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*shrink)(void *context, float range, float *result,
	    struct yt_error *error);
	bool (*emergency_warp)(void *context, struct yt_error *error);
	void (*set_current)(void *context, const struct yt_player *player);
	void (*style)(void *context, float foreground, float background,
	    float blink, int pager_foreground);
};
bool yt_sector_mine_run(struct yt_sector_mine_state *state,
    const struct yt_sector_mine_ops *ops, void *context,
    struct yt_error *error);

enum yt_planet_rename_name_result {
	YT_PLANET_RENAME_EMPTY,
	YT_PLANET_RENAME_RESERVED,
	YT_PLANET_RENAME_ACCEPTED,
};

enum yt_projectile_target_result {
	YT_PROJECTILE_TARGET_CANCEL,
	YT_PROJECTILE_TARGET_RETRY,
	YT_PROJECTILE_TARGET_ACCEPT,
};

enum yt_projectile_opening_output_kind {
	YT_PROJECTILE_OPENING_DIRECT_LINE,
	YT_PROJECTILE_OPENING_RAW,
};
typedef bool (*yt_projectile_opening_sound_fn)(void *context,
    float selector, struct yt_error *error);
typedef bool (*yt_projectile_opening_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error);
typedef bool (*yt_projectile_opening_wait_fn)(void *context, float duration,
    struct yt_error *error);
#define YT_PROJECTILE_ATTACKER_CAPACITY 64U
struct yt_projectile_plasma_opening_state {
	float special_attacker;
	float bolts;
	const uint8_t *player_name;
	size_t player_name_length;
	uint8_t attacker[YT_PROJECTILE_ATTACKER_CAPACITY];
	size_t attacker_length;
	double energy;
	float hop_loss;
	float firing_counter;
};
struct yt_projectile_plasma_opening_ops {
	yt_projectile_opening_sound_fn sound;
	yt_projectile_opening_present_fn present;
	yt_projectile_opening_wait_fn wait;
};
bool yt_projectile_plasma_opening_run(
    struct yt_projectile_plasma_opening_state *state,
    const struct yt_projectile_plasma_opening_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_plasma_impact_route {
	YT_PROJECTILE_PLASMA_NEXT_HOP,
	YT_PROJECTILE_PLASMA_FOOTER,
};
typedef bool (*yt_projectile_plasma_route_build_fn)(void *context,
    float *origin, float *destination, int16_t *route,
    size_t route_capacity, float *status, struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_output_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_impact_fn)(void *context, int hop,
    double *energy, enum yt_projectile_plasma_impact_route *route,
    struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_random_fn)(void *context,
    float *value, struct yt_error *error);
typedef int16_t (*yt_projectile_plasma_route_read_fn)(void *context,
    int16_t index);
typedef void (*yt_projectile_plasma_route_write_fn)(void *context,
    int16_t index, int16_t value);
enum yt_projectile_plasma_argument_change {
	YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO,
	YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN,
	YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION,
};
typedef void (*yt_projectile_plasma_arguments_fn)(void *context,
    float origin, float destination,
    enum yt_projectile_plasma_argument_change change);
struct yt_projectile_plasma_route_ops {
	yt_projectile_plasma_route_build_fn build_route;
	yt_projectile_plasma_route_output_fn line;
	yt_projectile_plasma_route_output_fn attention;
	yt_projectile_opening_wait_fn wait;
	yt_projectile_plasma_route_random_fn random;
	yt_projectile_plasma_route_impact_fn impact;
	yt_projectile_plasma_route_output_fn footer;
	yt_projectile_plasma_route_read_fn read_route;
	yt_projectile_plasma_route_write_fn write_route;
	yt_projectile_plasma_arguments_fn arguments_changed;
};
struct yt_projectile_plasma_route_state {
	float *origin;
	float *destination;
	double *energy;
	float hop_loss;
	float black_hole[2];
	float sector_record_offset;
	float port_record_offset;
	int16_t *route;
	size_t route_capacity;
	size_t step_limit;
	float route_status;
	float current_hop;
	size_t route_calls;
	size_t hops;
};
bool yt_projectile_plasma_route_run(
    struct yt_projectile_plasma_route_state *state,
    const struct yt_projectile_plasma_route_ops *ops, void *context,
    struct yt_error *error);

typedef bool (*yt_projectile_route_entry_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
struct yt_projectile_route_entry_state {
	int shooter;
	float maximum_player_record;
	float start;
	float current_hop;
	float shooter_team;
};
bool yt_projectile_route_entry_run(
    struct yt_projectile_route_entry_state *state,
    yt_projectile_route_entry_read_player_fn read_player, void *context,
    struct yt_error *error);

typedef bool (*yt_projectile_cruise_reroute_output_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_cruise_reroute_random_fn)(void *context,
    float *value, struct yt_error *error);
struct yt_projectile_cruise_reroute_ops {
	yt_projectile_cruise_reroute_output_fn line;
	yt_projectile_cruise_reroute_output_fn attention;
	yt_projectile_cruise_reroute_random_fn random;
};
struct yt_projectile_cruise_reroute_state {
	float hop;
	float sector_record_offset;
	float port_record_offset;
	float *origin;
	float *destination;
};
bool yt_projectile_is_black_hole(float hop, float first, float second);
bool yt_projectile_cruise_reroute_run(
    struct yt_projectile_cruise_reroute_state *state,
    const struct yt_projectile_cruise_reroute_ops *ops, void *context,
    struct yt_error *error);

bool yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker);

struct yt_projectile_sector_probe_state {
	const struct yt_sector *sector;
	float hop;
	float player_terminal;
	const struct yt_player_cache *player_cache;
	float xannor_provoker;
	float presence;
	float matched_player;
	float counter;
};
bool yt_projectile_sector_probe_run(
    struct yt_projectile_sector_probe_state *state,
    struct yt_error *error);

enum yt_projectile_plasma_fighter_route {
	YT_PROJECTILE_PLASMA_FIGHTER_CONTINUE_SECTOR,
	YT_PROJECTILE_PLASMA_FIGHTER_FOOTER,
};
enum yt_projectile_plasma_fighter_output_kind {
	YT_PROJECTILE_PLASMA_FIGHTER_ENCOUNTER,
	YT_PROJECTILE_PLASMA_FIGHTER_DAMAGE,
};
struct yt_projectile_plasma_fighter_state {
	float sector;
	double fighters;
	float owner;
	int shooter;
	float headquarters;
	const uint8_t *attacker;
	size_t attacker_length;
	double *energy;
	float *bold;
	double destroyed;
	double remaining_fighters;
	struct yt_sector persistence;
	bool victory_called;
	enum yt_projectile_plasma_fighter_route route;
};
typedef bool (*yt_projectile_plasma_fighter_owner_fn)(void *context,
    float owner, uint8_t *name, size_t *name_length,
    struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_fighter_output_kind kind,
    struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_sound_fn)(void *context,
    float selector, struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_random_fn)(void *context,
    float *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_news_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_read_fn)(void *context,
    float sector, struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_write_fn)(void *context,
    float sector, const struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_fighter_victory_fn)(void *context,
    struct yt_error *error);
struct yt_projectile_plasma_fighter_ops {
	yt_projectile_plasma_fighter_owner_fn owner;
	yt_projectile_plasma_fighter_present_fn present;
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_fighter_random_fn random;
	yt_projectile_plasma_fighter_news_fn news;
	yt_projectile_plasma_fighter_read_fn read_sector;
	yt_projectile_plasma_fighter_write_fn write_sector;
	yt_projectile_plasma_fighter_victory_fn victory;
};
bool yt_projectile_plasma_fighter_run(
    struct yt_projectile_plasma_fighter_state *state,
    const struct yt_projectile_plasma_fighter_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_plasma_mine_route {
	YT_PROJECTILE_PLASMA_MINE_CONTINUE_PLAYERS,
	YT_PROJECTILE_PLASMA_MINE_FOOTER,
};
struct yt_projectile_plasma_mine_state {
	float sector;
	double mines;
	const uint8_t *attacker;
	size_t attacker_length;
	double *energy;
	float destroyed;
	float remaining_mines;
	struct yt_sector persistence;
	enum yt_projectile_plasma_mine_route route;
};
struct yt_projectile_plasma_mine_ops {
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_fighter_news_fn news;
	yt_projectile_plasma_fighter_random_fn random;
	yt_projectile_cruise_reroute_output_fn present;
	yt_projectile_plasma_fighter_read_fn read_sector;
	yt_projectile_plasma_fighter_write_fn write_sector;
};
bool yt_projectile_plasma_mine_run(
    struct yt_projectile_plasma_mine_state *state,
    const struct yt_projectile_plasma_mine_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_plasma_dispatch_route {
	YT_PROJECTILE_PLASMA_DISPATCH_PLAYER,
	YT_PROJECTILE_PLASMA_DISPATCH_PLANET,
	YT_PROJECTILE_PLASMA_DISPATCH_NEXT_HOP,
	YT_PROJECTILE_PLASMA_DISPATCH_FOOTER,
};
struct yt_projectile_plasma_dispatch_state {
	double energy;
	float sector;
	float planet_link;
	uint8_t planet_link_raw[4];
	uint8_t conversion_mode;
	float player_terminal;
	const struct yt_player_cache *player_cache;
	bool resume_after_player;
	float counter;
	int selected_player;
	enum yt_projectile_plasma_dispatch_route route;
};
bool yt_projectile_plasma_dispatch_run(
    struct yt_projectile_plasma_dispatch_state *state,
    struct yt_error *error);

enum yt_projectile_plasma_player_route {
	YT_PROJECTILE_PLASMA_PLAYER_KILLED,
	YT_PROJECTILE_PLASMA_PLAYER_CONTINUE_DISPATCH,
	YT_PROJECTILE_PLASMA_PLAYER_FOOTER,
};
enum yt_projectile_plasma_player_output_kind {
	YT_PROJECTILE_PLASMA_PLAYER_FIRST_ROW,
	YT_PROJECTILE_PLASMA_PLAYER_SECOND_ROW,
};
struct yt_projectile_plasma_player_state {
	int target;
	float sector;
	const uint8_t *attacker;
	size_t attacker_length;
	double *energy;
	float foreground;
	float saved_foreground;
	double original_fighters;
	float original_shields;
	double destroyed_fighters;
	float destroyed_shields;
	double remaining_fighters;
	float remaining_shields;
	struct yt_player persistence;
	enum yt_projectile_plasma_player_route route;
};
typedef bool (*yt_projectile_plasma_player_read_fn)(void *context,
    int player_record, struct yt_player *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_player_write_fn)(void *context,
    int player_record, const struct yt_player *value,
    struct yt_error *error);
typedef void (*yt_projectile_plasma_player_color_fn)(void *context,
    float foreground);
typedef void (*yt_projectile_plasma_player_save_foreground_fn)(void *context,
    float *saved_foreground);
typedef void (*yt_projectile_plasma_player_restore_foreground_fn)(
    void *context, float saved_foreground);
typedef bool (*yt_projectile_plasma_player_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_player_output_kind kind,
    struct yt_error *error);
struct yt_projectile_plasma_player_ops {
	yt_projectile_plasma_player_read_fn read_player;
	yt_projectile_plasma_player_write_fn write_player;
	yt_projectile_plasma_player_save_foreground_fn save_foreground;
	yt_projectile_plasma_player_color_fn color;
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_fighter_random_fn random;
	yt_projectile_plasma_fighter_news_fn news;
	yt_projectile_plasma_player_present_fn present;
	yt_projectile_plasma_player_restore_foreground_fn restore_foreground;
};
bool yt_projectile_plasma_player_run(
    struct yt_projectile_plasma_player_state *state,
    const struct yt_projectile_plasma_player_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_plasma_killed_route {
	YT_PROJECTILE_PLASMA_KILLED_RELOAD_SECTOR,
	YT_PROJECTILE_PLASMA_KILLED_CONTINUE_DISPATCH,
	YT_PROJECTILE_PLASMA_KILLED_FOOTER,
};
enum yt_projectile_plasma_killed_output_kind {
	YT_PROJECTILE_PLASMA_KILLED_DESTROYED_ROW,
	YT_PROJECTILE_PLASMA_KILLED_SELF_DESTROYED_ROW,
	YT_PROJECTILE_PLASMA_KILLED_WARNING_ROW,
};
struct yt_projectile_plasma_killed_state {
	int victim;
	int shooter;
	int sector;
	double *energy;
	float *blink;
	bool *destroyed;
	struct yt_player_cache *player_cache;
	bool self_hit;
	float saved_mines;
	struct yt_player victim_persistence;
	struct yt_sector mine_persistence;
	enum yt_projectile_plasma_killed_route route;
};
typedef bool (*yt_projectile_plasma_killed_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_killed_output_kind kind,
    struct yt_error *error);
typedef bool (*yt_projectile_plasma_killed_read_sector_fn)(void *context,
    int sector, struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_killed_write_sector_fn)(void *context,
    int sector, const struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_killed_child_fn)(void *context,
    int victim, int shooter, struct yt_error *error);
struct yt_projectile_plasma_killed_ops {
	yt_projectile_plasma_player_read_fn read_player;
	yt_projectile_plasma_player_write_fn write_player;
	yt_projectile_plasma_killed_present_fn present;
	yt_projectile_plasma_killed_read_sector_fn read_sector;
	yt_projectile_plasma_killed_write_sector_fn write_sector;
	yt_projectile_plasma_killed_child_fn death;
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_killed_child_fn salvage;
};
bool yt_projectile_plasma_killed_run(
    struct yt_projectile_plasma_killed_state *state,
    const struct yt_projectile_plasma_killed_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_plasma_planet_route {
	YT_PROJECTILE_PLASMA_PLANET_NEXT_HOP,
	YT_PROJECTILE_PLASMA_PLANET_FOOTER,
};
enum yt_projectile_plasma_planet_output_kind {
	YT_PROJECTILE_PLASMA_PLANET_HIT_ROW,
	YT_PROJECTILE_PLASMA_PLANET_PRODUCTIVITY_ROW,
	YT_PROJECTILE_PLASMA_PLANET_DESTROYED_ROW,
	YT_PROJECTILE_PLASMA_PLANET_GROUND_ROW,
};
struct yt_projectile_plasma_planet_state {
	int planet;
	int sector;
	const uint8_t *attacker;
	size_t attacker_length;
	double *energy;
	float stale_ore;
	float production[3];
	float stock[3];
	float original_productivity;
	float remaining_productivity;
	float original_ground;
	float remaining_ground;
	bool destroyed;
	struct yt_planet persistence;
	struct yt_planet destruction;
	struct yt_sector unlink;
	enum yt_projectile_plasma_planet_route route;
};
typedef bool (*yt_projectile_plasma_planet_update_fn)(void *context,
    int planet, float *stale_ore, struct yt_error *error);
typedef bool (*yt_projectile_plasma_planet_read_fn)(void *context,
    int planet, struct yt_planet *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_planet_write_fn)(void *context,
    int planet, const struct yt_planet *value, struct yt_error *error);
typedef bool (*yt_projectile_plasma_planet_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_planet_output_kind kind,
    struct yt_error *error);
struct yt_projectile_plasma_planet_ops {
	yt_projectile_plasma_planet_update_fn update;
	yt_projectile_plasma_planet_read_fn read_planet;
	yt_projectile_plasma_planet_write_fn write_planet;
	yt_projectile_plasma_killed_read_sector_fn read_sector;
	yt_projectile_plasma_killed_write_sector_fn write_sector;
	yt_projectile_plasma_planet_present_fn present;
	yt_projectile_plasma_fighter_news_fn news;
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_fighter_random_fn random;
};
bool yt_projectile_plasma_planet_run(
    struct yt_projectile_plasma_planet_state *state,
    const struct yt_projectile_plasma_planet_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_defense_front_route {
	YT_PROJECTILE_DEFENSE_NO_DEFENSE,
	YT_PROJECTILE_DEFENSE_FRIENDLY,
	YT_PROJECTILE_DEFENSE_HOSTILE,
};
struct yt_projectile_defense_front_state {
	float sector;
	double fighters;
	float owner;
	int shooter;
	enum yt_projectile_defense_front_route route;
};
typedef bool (*yt_projectile_defense_owner_fn)(void *context, float owner,
    uint8_t *name, size_t *name_length, struct yt_error *error);
typedef bool (*yt_projectile_defense_friendship_fn)(void *context,
    float owner, bool *friendly, struct yt_error *error);
typedef bool (*yt_projectile_defense_sound_fn)(void *context, float selector,
    struct yt_error *error);
struct yt_projectile_defense_front_ops {
	yt_projectile_defense_owner_fn owner;
	yt_projectile_defense_friendship_fn friendship;
	yt_projectile_cruise_reroute_output_fn present;
	yt_projectile_defense_sound_fn sound;
};
bool yt_projectile_defense_front_run(
    struct yt_projectile_defense_front_state *state,
    const struct yt_projectile_defense_front_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_defense_combat_route {
	YT_PROJECTILE_DEFENSE_CONTINUE_MINES,
	YT_PROJECTILE_DEFENSE_RETURN,
};
struct yt_projectile_defense_combat_state {
	float sector;
	double fighters;
	float owner;
	int shooter;
	float headquarters;
	const uint8_t *shooter_name;
	size_t shooter_name_length;
	float *missiles;
	int *xannor_provoker;
	float saved_missiles;
	float destroyed;
	float counter;
	double remaining_fighters;
	struct yt_sector persistence;
	bool victory_called;
	enum yt_projectile_defense_combat_route route;
};
typedef bool (*yt_projectile_defense_sector_read_fn)(void *context,
    float sector, struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_defense_sector_write_fn)(void *context,
    float sector, const struct yt_sector *value, struct yt_error *error);
typedef bool (*yt_projectile_defense_victory_fn)(void *context,
    struct yt_error *error);
struct yt_projectile_defense_combat_ops {
	yt_projectile_cruise_reroute_random_fn random;
	yt_projectile_cruise_reroute_output_fn present;
	yt_projectile_cruise_reroute_output_fn news;
	yt_projectile_defense_sector_read_fn read_sector;
	yt_projectile_defense_sector_write_fn write_sector;
	yt_projectile_defense_victory_fn victory;
};
bool yt_projectile_defense_combat_run(
    struct yt_projectile_defense_combat_state *state,
    const struct yt_projectile_defense_combat_ops *ops, void *context,
    struct yt_error *error);

enum yt_projectile_sector_mine_route {
	YT_PROJECTILE_SECTOR_MINE_CONTINUE_PLAYERS,
	YT_PROJECTILE_SECTOR_MINE_RETURN,
};
struct yt_projectile_sector_mine_state {
	float sector;
	const uint8_t *shooter_name;
	size_t shooter_name_length;
	float *missiles;
	float *last_news_sector;
	double observed_mines;
	float destroyed;
	struct yt_sector persistence;
	enum yt_projectile_sector_mine_route route;
};
struct yt_projectile_sector_mine_ops {
	yt_projectile_defense_sector_read_fn read_sector;
	yt_projectile_cruise_reroute_output_fn present;
	yt_projectile_defense_sound_fn sound;
	yt_projectile_cruise_reroute_output_fn news;
	yt_projectile_defense_sector_write_fn write_sector;
};
bool yt_projectile_sector_mine_run(
    struct yt_projectile_sector_mine_state *state,
    const struct yt_projectile_sector_mine_ops *ops, void *context,
    struct yt_error *error);

struct yt_xannor_retaliation_state {
	struct yt_player *player;
	int *player_record;
	struct yt_player_cache *player_cache;
	bool *destroyed;
	int *provoker;
	float *headquarters;
	int sector_count;
};

typedef bool (*yt_xannor_retaliation_read_sector_fn)(void *context,
    int logical_sector, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_random_fn)(void *context, int count,
    int range, int *value, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_present_fn)(void *context,
    const uint8_t *text, size_t length, bool bold, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_projectile_fn)(void *context,
    float *origin, float *target, float *amount, bool plasma,
    int *counterattack, int *xannor_provoker, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_wait_fn)(void *context,
	float duration, struct yt_error *error);

struct yt_xannor_retaliation_ops {
	yt_xannor_retaliation_read_sector_fn read_sector;
	yt_xannor_retaliation_random_fn random;
	yt_xannor_retaliation_present_fn present;
	yt_xannor_retaliation_projectile_fn projectile;
	yt_xannor_retaliation_read_player_fn read_player;
	yt_xannor_retaliation_wait_fn wait;
};

enum yt_salvage_simple_kind {
	YT_SALVAGE_CREDITS,
	YT_SALVAGE_MISSILES,
	YT_SALVAGE_PLASMA,
	YT_SALVAGE_GROUND_FORCES,
	YT_SALVAGE_MINES,
};

enum yt_salvage_cargo_kind {
	YT_SALVAGE_EMPTY_HOLDS,
	YT_SALVAGE_ORE,
	YT_SALVAGE_ORGANICS,
	YT_SALVAGE_EQUIPMENT,
};

enum yt_port_name_row_kind {
	YT_PORT_NAME_CURRENT_ROW,
	YT_PORT_NAME_KEEP_ROW,
	YT_PORT_NAME_INSTRUCTION_ROW,
};

struct yt_port_name_editor_state {
	const uint8_t *cached;
	size_t cached_length;
	int logical_port;
	struct yt_port *port;
};

typedef bool (*yt_port_name_row_fn)(void *context,
    enum yt_port_name_row_kind kind, const uint8_t *text, size_t length,
    struct yt_error *error);
typedef bool (*yt_port_name_prompt_fn)(void *context, const uint8_t *text,
    size_t length, struct yt_error *error);
typedef bool (*yt_port_name_edit_fn)(void *context, uint8_t *response,
    size_t capacity, size_t *length, struct yt_error *error);
typedef bool (*yt_port_name_blank_fn)(void *context,
    struct yt_error *error);
typedef bool (*yt_port_name_confirm_fn)(void *context,
    const uint8_t *prompt, size_t length, bool *accepted,
    struct yt_error *error);
typedef bool (*yt_port_name_write_fn)(void *context, int logical_port,
    const struct yt_record *record, struct yt_error *error);

struct yt_port_name_editor_ops {
	yt_port_name_row_fn row;
	yt_port_name_prompt_fn prompt;
	yt_port_name_edit_fn edit;
	yt_port_name_blank_fn blank;
	yt_port_name_confirm_fn confirm;
	yt_port_name_write_fn write;
};

struct yt_game {
	struct yt_database database;
	struct yt_config config;
	struct yt_random random;
	int today;
	int adjusted_year;
};

void yt_player_decode(struct yt_player *player, const struct yt_record *record);
void yt_player_encode(struct yt_player *player);
void yt_sector_decode(struct yt_sector *sector, const struct yt_record *record);
void yt_sector_encode(struct yt_sector *sector);
void yt_port_decode(struct yt_port *port, const struct yt_record *record);
void yt_port_encode(struct yt_port *port);
void yt_planet_decode(struct yt_planet *planet, const struct yt_record *record);
void yt_planet_encode(struct yt_planet *planet);

enum yt_planet_updater_stage {
	YT_PLANET_UPDATER_DATE_HELPER,
	YT_PLANET_UPDATER_OPENING_RECORD_EXPRESSION,
	YT_PLANET_UPDATER_GET,
	YT_PLANET_UPDATER_TIMER,
	YT_PLANET_UPDATER_LSET_DAY,
	YT_PLANET_UPDATER_LSET_BASE_ORE,
	YT_PLANET_UPDATER_LSET_BASE_ORGANICS,
	YT_PLANET_UPDATER_LSET_BASE_EQUIPMENT,
	YT_PLANET_UPDATER_LSET_STOCK_ORE,
	YT_PLANET_UPDATER_LSET_STOCK_ORGANICS,
	YT_PLANET_UPDATER_LSET_STOCK_EQUIPMENT,
	YT_PLANET_UPDATER_LSET_MISSILES,
	YT_PLANET_UPDATER_LSET_FORCES,
	YT_PLANET_UPDATER_LSET_MINUTE,
	YT_PLANET_UPDATER_LSET_PLASMA,
	YT_PLANET_UPDATER_LSET_BANK,
	YT_PLANET_UPDATER_LSET_MINES,
	YT_PLANET_UPDATER_LSET_FIGHTERS,
	YT_PLANET_UPDATER_CLOSING_RECORD_EXPRESSION,
	YT_PLANET_UPDATER_PUT,
	YT_PLANET_UPDATER_STAGE_COUNT
};

struct yt_planet_updater_cache {
	float current_day;
	float current_minute;
	float elapsed;
	float production[10];
	double quantity[10];
	float contribution[10];
	uint8_t quantity_raw[10][8];
};

struct yt_planet_updater_raw_cache {
	uint8_t quantity[10][8];
	uint8_t production[10][4];
	uint8_t contribution[10][4];
	uint8_t current_minute[4];
	uint8_t elapsed[4];
};

struct yt_planet_updater_state {
	uint8_t logical_planet_raw[4];
	uint8_t planet_offset_raw[4];
	uint8_t current_day_raw[4];
	uint8_t timer_seconds_raw[4];
	uint32_t physical_record;
	struct yt_record field;
	struct yt_planet_updater_cache cache;
	struct yt_planet_updater_raw_cache raw_cache;
	uint16_t raw_error_site;
	uint16_t raw_next_address;
	uint8_t raw_basic_error;
	enum yt_planet_updater_stage stage;
	size_t effect_count;
	size_t completed_effects;
	bool field_loaded;
	bool field_dirty;
	bool written;
	bool raw_error_valid;
};

typedef bool (*yt_planet_updater_date_fn)(void *context,
	uint8_t current_day_raw[4], struct yt_error *error);
typedef bool (*yt_planet_updater_record_expression_fn)(void *context,
	bool closing, struct yt_error *error);
typedef bool (*yt_planet_updater_get_fn)(void *context,
	uint32_t physical_record, struct yt_record *record,
	struct yt_error *error);
typedef bool (*yt_planet_updater_timer_fn)(void *context,
	uint8_t timer_seconds_raw[4], struct yt_error *error);
typedef bool (*yt_planet_updater_lset_fn)(void *context,
	enum yt_planet_updater_stage stage, size_t offset,
	const uint8_t raw[4], struct yt_error *error);
typedef bool (*yt_planet_updater_put_fn)(void *context,
	uint32_t physical_record, const struct yt_record *record,
	struct yt_error *error);

struct yt_planet_updater_ops {
	yt_planet_updater_date_fn date;
	yt_planet_updater_record_expression_fn record_expression;
	yt_planet_updater_get_fn get;
	yt_planet_updater_timer_fn timer;
	yt_planet_updater_lset_fn lset;
	yt_planet_updater_put_fn put;
};

const char *yt_planet_updater_stage_name(enum yt_planet_updater_stage stage);
const char *yt_planet_updater_stage_site(enum yt_planet_updater_stage stage);
bool yt_planet_updater_run(struct yt_planet_updater_state *state,
	const struct yt_planet_updater_ops *ops, void *context,
	struct yt_error *error);
bool yt_planet_updater_raw_run(struct yt_planet_updater_state *state,
	const struct yt_planet_updater_ops *ops, void *context,
	struct yt_error *error);

bool yt_game_open(struct yt_game *game, enum yt_open_mode mode,
    struct yt_error *error);
void yt_game_close(struct yt_game *game);
bool yt_game_read_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error);
bool yt_game_write_player(struct yt_game *game, int basic_record,
    struct yt_player *player, struct yt_error *error);
bool yt_game_read_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool yt_game_write_sector(struct yt_game *game, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool yt_game_read_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool yt_game_write_port(struct yt_game *game, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool yt_game_read_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);
bool yt_game_write_planet(struct yt_game *game, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);
struct yt_player_constructor_state {
	bool config_hydrated;
	bool player_hydrated;
	bool put_attempted;
};
bool yt_game_construct_player(struct yt_game *game, int basic_record,
    const uint8_t today_raw[4], const uint8_t turns_raw[4],
    struct yt_player *player, struct yt_player_constructor_state *state,
    struct yt_error *error);
bool yt_game_set_player_identity(struct yt_game *game, int basic_record,
    const uint8_t *name, size_t length, struct yt_player *player,
    struct yt_error *error);
bool yt_game_post_login_repairs(struct yt_game *game, int basic_record,
	const uint8_t one_raw[4], const uint8_t zero_raw[4],
	const uint8_t maximum_holds_raw[4], struct yt_player *player,
	struct yt_post_login_repairs *repairs, struct yt_error *error);
bool yt_sector_force_route(float fighters, float owner,
    int current_player_record, enum yt_sector_force_route *route,
    int *owner_record, struct yt_error *error);
bool yt_sector_is_black_hole(float current_sector, float first,
    float second);
bool yt_sector_mines_admitted(float mines, float suppression);
bool yt_sector_force_same_team(float current_team, float owner_team);
typedef bool (*yt_friendship_reader_fn)(void *context, int player_record,
    struct yt_player *player, struct yt_error *error);
bool yt_friendship_resolve(float candidate_record,
    float current_player_record, float last_player_record,
    yt_friendship_reader_fn reader, void *reader_context, bool *friendly,
    struct yt_error *error);
enum yt_port_owner_kind yt_port_owner_classify(float owner,
    int current_player_record, int *owner_record);
bool yt_port_owner_compose(enum yt_port_owner_kind kind, float treasury,
    const uint8_t *owner_name, size_t owner_name_length,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_hostile_menu_row(double ship_fighters, double deployed_fighters,
    uint8_t *row, size_t capacity, size_t *length);
enum yt_hostile_menu_route yt_hostile_menu_dispatch(const char *response);
enum yt_main_shell_route yt_main_shell_dispatch(const char *response);
bool yt_main_prompt_row(const uint8_t *time_text, size_t time_text_length,
	uint8_t *row, size_t capacity, size_t *length);
bool yt_computer_prompt_row(const uint8_t *time_text,
	size_t time_text_length, uint8_t *row, size_t capacity,
	size_t *length);
enum yt_hostile_attack_admission yt_hostile_attack_admit(
    float ship_fighters, float commitment);
float yt_hostile_attack_quantum(double remaining_attacker,
    double remaining_defender);
bool yt_hostile_attack_loses_attacker(float cloak, float draw);
enum yt_hostile_surrender_route yt_hostile_surrender_route(float owner);
bool yt_fighter_shield_spill_step(double *fighters, float *shields,
    float draw);
bool yt_fighter_shield_spill_rows(double fighters, float shields,
    uint8_t *fighter_row, size_t fighter_capacity, size_t *fighter_length,
    uint8_t *shield_row, size_t shield_capacity, size_t *shield_length);
enum yt_fighter_shield_spill_output_kind {
	YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW,
	YT_FIGHTER_SHIELD_SPILL_SHIELD_ROW,
};
struct yt_fighter_shield_spill_state {
	double fighters;
	float shields;
	size_t iterations;
	bool fighter_row_presented;
	bool shield_row_presented;
	bool complete;
};
typedef bool (*yt_fighter_shield_spill_draw_fn)(void *context,
    float *value, struct yt_error *error);
typedef bool (*yt_fighter_shield_spill_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_fighter_shield_spill_output_kind kind,
    struct yt_error *error);
enum yt_fighter_shield_spill_store_kind {
	YT_FIGHTER_SHIELD_SPILL_STORE_FIGHTERS,
	YT_FIGHTER_SHIELD_SPILL_STORE_SHIELDS,
};
typedef void (*yt_fighter_shield_spill_store_fn)(void *context,
    enum yt_fighter_shield_spill_store_kind kind, double fighters,
    float shields);
struct yt_fighter_shield_spill_ops {
	yt_fighter_shield_spill_draw_fn random;
	yt_fighter_shield_spill_present_fn present;
	yt_fighter_shield_spill_store_fn store;
};
bool yt_fighter_shield_spill_run(
    struct yt_fighter_shield_spill_state *state,
    const struct yt_fighter_shield_spill_ops *ops, void *context,
    struct yt_error *error);
bool yt_hostile_defeated_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_xannor_attack_reward_rows(const uint8_t *name, size_t name_length,
    float bonus, double defenders_destroyed,
    uint8_t *display, size_t display_capacity, size_t *display_length,
    uint8_t *news, size_t news_capacity, size_t *news_length);
float yt_xannor_attack_bonus(double defenders_destroyed, float turns,
    float turns_per_day);
bool yt_bribe_ordinary_forces(float owner, double defenders,
    double ship_fighters, float draw);
bool yt_bribe_mercenary_forces(double defenders, double ship_fighters,
    float first, float second, bool sticky);
double yt_bribe_offer_threshold(double defenders, float draw);
bool yt_bribe_offer_accepted(float offer, double credits, double threshold);
enum yt_bribe_forced_admission yt_bribe_forced_admit(
    double ship_fighters, float shields, bool mercenary_fatal_gate,
    float commitment);
enum yt_sector_mine_admission yt_sector_mine_admit(
    float carried, float amount);
bool yt_no_turn_gate_denied(float turns);
void yt_no_turn_gate_result_raw(bool denied, uint8_t raw[4]);
bool yt_action_finalizer_turn_raw(const uint8_t before[4], uint8_t after[4]);
bool yt_action_finalizer_cloak_raw(const uint8_t before[4],
    uint8_t arithmetic[4], uint8_t result[4], bool *clamped);
bool yt_port_link_missing(float link);
float yt_port_selected_expression(float port_offset, float logical_link);
enum yt_computer_port_selection_route {
	YT_COMPUTER_PORT_SELECTION_EMPTY,
	YT_COMPUTER_PORT_SELECTION_INVALID,
	YT_COMPUTER_PORT_SELECTION_ACCEPTED,
};
bool yt_computer_port_maximum(float port_offset, float sector_offset,
	float *maximum, struct yt_error *error);
bool yt_computer_port_select(const char *response, float maximum,
	float *selected, enum yt_computer_port_selection_route *route,
	struct yt_error *error);
bool yt_computer_path_maximum(float port_offset, float sector_offset,
	float *maximum, struct yt_error *error);
bool yt_computer_path_parse(const char *response, float *selected,
	uint8_t selected_raw[4], struct yt_error *error);
bool yt_computer_path_append_hop(char *scratch, size_t capacity,
	size_t *length, float next_sector, float *hop_count,
	uint8_t hop_count_raw[4], struct yt_error *error);
bool yt_computer_path_wrap_required(int local_column);
enum yt_computer_avoid_selection_route {
	YT_COMPUTER_AVOID_SELECTION_INVALID,
	YT_COMPUTER_AVOID_SELECTION_ACCEPTED,
};
bool yt_computer_avoid_maximum(float port_offset, float sector_offset,
	float *maximum, struct yt_error *error);
bool yt_computer_avoid_select_slot(const char *response,
	uint8_t conversion_mode, float *selected, int *index,
	enum yt_computer_avoid_selection_route *route,
	struct yt_error *error);
bool yt_computer_avoid_select_sector(const char *response, float maximum,
	float *selected, enum yt_computer_avoid_selection_route *route,
	struct yt_error *error);
void yt_computer_avoid_transition(float old_value, float new_value,
	bool *locked, bool *available);
enum yt_computer_port_field_kind {
	YT_COMPUTER_PORT_FIELD_CALLER,
	YT_COMPUTER_PORT_FIELD_SECTOR,
	YT_COMPUTER_PORT_FIELD_PLAYER,
};
struct yt_computer_port_visibility_state {
	float port_link;
	float fighter_count;
	float fighter_owner;
	float cached_current_team;
	float current_player_record;
	float last_player_record;
	float planet_record_offset;
	float inherited_index;
	uint8_t marker_4d62_raw[4];
	uint8_t relation_raw[4];
	float marker_4d62;
	float relation;
	float scratch_19c4;
	uint8_t scratch_19c4_raw[4];
	enum yt_computer_port_field_kind field_kind;
	uint32_t field_record;
	struct yt_record field;
	bool field_valid;
	size_t player_read_attempts;
	bool scratch_written;
	bool unavailable;
	bool complete;
};
typedef bool (*yt_computer_port_read_player_fn)(void *context,
	uint32_t physical_record, struct yt_player *player,
	struct yt_error *error);
bool yt_computer_port_visibility_run(
	struct yt_computer_port_visibility_state *state,
	yt_computer_port_read_player_fn read_player, void *context,
	struct yt_error *error);
bool yt_port_name_display_row(const uint8_t *cached, size_t cached_length,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_port_name_prepare_candidate(const uint8_t *entered,
    size_t entered_length, const uint8_t *cached, size_t cached_length,
    uint8_t *candidate, size_t capacity, size_t *candidate_length);
bool yt_port_name_confirmation_prompt(const uint8_t *candidate,
    size_t candidate_length, uint8_t *prompt, size_t capacity,
    size_t *length);
bool yt_port_name_overlay(struct yt_port *port, const uint8_t *candidate,
    size_t candidate_length);
bool yt_port_name_editor_run(struct yt_port_name_editor_state *state,
    const struct yt_port_name_editor_ops *ops, void *context,
    struct yt_error *error);
bool yt_port_rename_record(float port_offset, float sector_link,
    int *logical_port, float *relative_port);
enum yt_port_rename_output_kind {
	YT_PORT_RENAME_NO_PORT,
	YT_PORT_RENAME_NOT_OWNER,
	YT_PORT_RENAME_EARTH,
};
enum yt_port_rename_route {
	YT_PORT_RENAME_INCOMPLETE,
	YT_PORT_RENAME_NO_PORT_ROUTE,
	YT_PORT_RENAME_NOT_OWNER_ROUTE,
	YT_PORT_RENAME_EARTH_ROUTE,
	YT_PORT_RENAME_EDITED_ROUTE,
};
struct yt_port_rename_state {
	float current_player_record;
	float port_offset;
	uint8_t conversion_mode;
	int hydration_record;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	int logical_port;
	float relative_port;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
	size_t cached_name_length;
	bool player_hydrated;
	bool sector_read;
	bool port_read;
	bool editor_called;
	bool complete;
	enum yt_port_rename_route route;
};
struct yt_port_rename_ops {
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*read_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_port_rename_output_kind kind, struct yt_error *error);
	bool (*edit)(void *context, int logical_port,
	    const uint8_t *cached, size_t cached_length,
	    struct yt_port *port, struct yt_error *error);
};
bool yt_port_rename_run(struct yt_port_rename_state *state,
	const struct yt_port_rename_ops *ops, void *context,
	struct yt_error *error);
double yt_port_purchase_price(const float production[3]);
float yt_port_purchase_seller_credit(float treasury, float credits,
    double price);
float yt_port_purchase_buyer_credit(float credits, double price);
bool yt_port_purchase_seller_overlay(struct yt_player *seller,
	float treasury, double price);
bool yt_port_purchase_title_overlay(struct yt_port *port,
	int buyer_record);
bool yt_port_purchase_buyer_overlay(struct yt_player *buyer,
	double price);
int yt_port_purchase_seller_record(float owner);
enum yt_port_purchase_output_kind {
	YT_PORT_PURCHASE_NO_PORT,
	YT_PORT_PURCHASE_ALREADY_OWNER,
	YT_PORT_PURCHASE_PRICE,
	YT_PORT_PURCHASE_UNAFFORDABLE,
	YT_PORT_PURCHASE_OFFER_LEADING_BLANK,
	YT_PORT_PURCHASE_OFFER_ROW,
	YT_PORT_PURCHASE_OFFER_TRAILING_BLANK,
	YT_PORT_PURCHASE_DECLINED,
};
enum yt_port_purchase_route {
	YT_PORT_PURCHASE_INCOMPLETE,
	YT_PORT_PURCHASE_NO_PORT_ROUTE,
	YT_PORT_PURCHASE_ALREADY_OWNER_ROUTE,
	YT_PORT_PURCHASE_UNAFFORDABLE_ROUTE,
	YT_PORT_PURCHASE_DECLINED_ROUTE,
	YT_PORT_PURCHASE_ACCEPTED_ROUTE,
};
enum yt_port_purchase_accept_output_kind {
	YT_PORT_PURCHASE_ACCEPT_SOLD_BLANK,
	YT_PORT_PURCHASE_ACCEPT_SOLD_ROW,
	YT_PORT_PURCHASE_ACCEPT_TRANSFER_BLANK,
	YT_PORT_PURCHASE_ACCEPT_TRANSFER_ROW,
	YT_PORT_PURCHASE_ACCEPT_SUCCESS_FIRST,
	YT_PORT_PURCHASE_ACCEPT_SUCCESS_TAIL,
};
struct yt_port_purchase_accept_state {
	int current_player_record;
	int logical_port;
	float relative_port;
	float old_owner;
	double price;
	float cached_buyer_sector;
	const uint8_t *cached_trader;
	size_t cached_trader_length;
	const uint8_t *old_name;
	size_t old_name_length;
	const uint8_t *owner_name;
	size_t owner_name_length;
	const uint8_t *first_name;
	size_t first_name_length;
	struct yt_port port;
	struct yt_player seller;
	struct yt_player buyer;
	int seller_record;
	bool sold_presented;
	bool port_read;
	bool seller_read;
	bool seller_written;
	bool radio_written;
	bool port_reloaded_after_radio;
	bool rename_called;
	bool title_port_read;
	bool title_written;
	bool buyer_hydrated;
	bool buyer_written;
	bool success_presented;
	bool complete;
};
struct yt_port_purchase_accept_ops {
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_port_purchase_accept_output_kind kind,
	    struct yt_error *error);
	bool (*read_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*radio)(void *context, const uint8_t *text, size_t length,
	    float sender, float recipient, struct yt_error *error);
	bool (*rename)(void *context, int logical_port,
	    const uint8_t *cached, size_t cached_length,
	    struct yt_port *port, struct yt_error *error);
	bool (*write_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*hydrate_buyer)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
};
bool yt_port_purchase_accept_run(
	struct yt_port_purchase_accept_state *state,
	const struct yt_port_purchase_accept_ops *ops, void *context,
	struct yt_error *error);
struct yt_port_purchase_state {
	int current_player_record;
	float port_offset;
	uint8_t conversion_mode;
	const uint8_t *first_name;
	size_t first_name_length;
	struct yt_player buyer_entry;
	struct yt_sector sector;
	struct yt_port early_port;
	struct yt_port terminal_port;
	int logical_port;
	float relative_port;
	bool earth;
	float cached_buyer_credits;
	float cached_buyer_sector;
	uint8_t cached_trader[YT_TEXT_FIELD_SIZE];
	size_t cached_trader_length;
	float old_owner;
	float purchase_production[3];
	double price;
	uint8_t old_name[YT_TEXT_FIELD_SIZE];
	size_t old_name_length;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length;
	struct yt_port_purchase_accept_state accepted;
	bool buyer_hydrated;
	bool sector_read;
	bool report_complete;
	bool owner_displayed;
	bool confirmation_read;
	bool accepted_called;
	bool complete;
	enum yt_port_purchase_route route;
};
struct yt_port_purchase_ops {
	bool (*hydrate_buyer)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*report)(void *context, int logical_port, bool earth,
	    struct yt_port *early_port, struct yt_port *terminal_port,
	    float production[3], struct yt_error *error);
	bool (*owner)(void *context, const struct yt_port *port,
	    uint8_t *name, size_t capacity, size_t *length,
	    struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_port_purchase_output_kind kind, struct yt_error *error);
	bool (*confirm)(void *context, const uint8_t *prompt, size_t length,
	    bool *accepted, struct yt_error *error);
	bool (*accept)(void *context,
	    struct yt_port_purchase_accept_state *state,
	    struct yt_error *error);
};
bool yt_port_purchase_run(struct yt_port_purchase_state *state,
	const struct yt_port_purchase_ops *ops, void *context,
	struct yt_error *error);
bool yt_genesis_confirmation_prompt(const uint8_t *trader,
    size_t trader_length, uint8_t *prompt, size_t capacity, size_t *length);
bool yt_genesis_insufficient_rows(float required, float owned,
    uint8_t *first, size_t first_capacity, size_t *first_length,
    uint8_t *second, size_t second_capacity, size_t *second_length);
enum yt_main_fighters_output_kind {
	YT_MAIN_FIGHTERS_TITLE,
	YT_MAIN_FIGHTERS_UNION_REFUSAL,
	YT_MAIN_FIGHTERS_FOREIGN_REFUSAL,
	YT_MAIN_FIGHTERS_AVAILABLE,
	YT_MAIN_FIGHTERS_PROMPT,
	YT_MAIN_FIGHTERS_INSUFFICIENT,
	YT_MAIN_FIGHTERS_SUCCESS,
};
enum yt_main_fighters_route {
	YT_MAIN_FIGHTERS_INCOMPLETE,
	YT_MAIN_FIGHTERS_UNION_ROUTE,
	YT_MAIN_FIGHTERS_FOREIGN_ROUTE,
	YT_MAIN_FIGHTERS_CANCELLED_ROUTE,
	YT_MAIN_FIGHTERS_INSUFFICIENT_ROUTE,
	YT_MAIN_FIGHTERS_ACCEPTED_ROUTE,
};
struct yt_main_fighters_state {
	int current_player_record;
	int logical_sector;
	struct yt_player player;
	struct yt_sector first_sector;
	struct yt_sector accepted_sector;
	struct yt_player accepted_player;
	double available;
	float desired;
	uint8_t desired_raw[4];
	float delta;
	float remaining;
	bool player_hydrated;
	bool first_sector_read;
	bool input_read;
	bool desired_stored;
	bool accepted_sector_read;
	bool sector_written;
	bool accepted_player_read;
	bool player_written;
	bool sound_called;
	bool complete;
	enum yt_main_fighters_route route;
};
struct yt_main_fighters_ops {
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int sector_number,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_main_fighters_output_kind kind, struct yt_error *error);
	bool (*input)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
};
bool yt_main_fighters_sector_overlay(struct yt_sector *sector,
	const uint8_t desired_raw[4], int player_record);
bool yt_main_fighters_player_overlay(struct yt_player *player,
	float remaining);
bool yt_main_fighters_run(struct yt_main_fighters_state *state,
	const struct yt_main_fighters_ops *ops, void *context,
	struct yt_error *error);
enum yt_genesis_output_kind {
	YT_GENESIS_PROPHECY_FIRST,
	YT_GENESIS_PROPHECY_SECOND,
	YT_GENESIS_PROMPT_BLANK,
	YT_GENESIS_DISABLED,
	YT_GENESIS_DECLINED,
	YT_GENESIS_INSUFFICIENT_FIRST,
	YT_GENESIS_INSUFFICIENT_SECOND,
	YT_GENESIS_SUCCESS_BLANK,
	YT_GENESIS_SUCCESS_FIRST,
	YT_GENESIS_SUCCESS_SECOND,
};
enum yt_genesis_route {
	YT_GENESIS_INCOMPLETE,
	YT_GENESIS_DECLINED_ROUTE,
	YT_GENESIS_DISABLED_ROUTE,
	YT_GENESIS_INSUFFICIENT_ROUTE,
	YT_GENESIS_HANDOFF_ROUTE,
};
struct yt_genesis_state {
	int current_player_record;
	float required_ports;
	const uint8_t *cached_trader;
	size_t cached_trader_length;
	struct yt_player player;
	bool answer;
	bool player_hydrated;
	bool confirmation_read;
	bool disabled_presented;
	bool handoff_called;
	bool complete;
	enum yt_genesis_route route;
};
struct yt_genesis_ops {
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_genesis_output_kind kind, struct yt_error *error);
	bool (*confirm)(void *context, const uint8_t *prompt, size_t length,
	    bool *accepted, struct yt_error *error);
	bool (*handoff)(void *context, struct yt_error *error);
};
bool yt_genesis_run(struct yt_genesis_state *state,
	const struct yt_genesis_ops *ops, void *context,
	struct yt_error *error);

enum yt_genesis_handoff_operation {
	YT_GENESIS_HANDOFF_NONE,
	YT_GENESIS_HANDOFF_CLOSE_FILE5,
	YT_GENESIS_HANDOFF_OPEN_OUTPUT,
	YT_GENESIS_HANDOFF_PRINT_COMMAND,
	YT_GENESIS_HANDOFF_CLOSE_ALL,
	YT_GENESIS_HANDOFF_RUN,
};
struct yt_genesis_handoff_state {
	enum yt_genesis_handoff_operation failed_operation;
	bool file5_closed;
	bool output_opened;
	bool command_printed;
	bool close_all_completed;
	bool run_invoked;
	bool complete;
};
typedef bool (*yt_genesis_handoff_step_fn)(void *context,
	struct yt_error *error);
struct yt_genesis_handoff_ops {
	yt_genesis_handoff_step_fn close_file5;
	yt_genesis_handoff_step_fn open_output;
	yt_genesis_handoff_step_fn print_command;
	yt_genesis_handoff_step_fn close_all;
	yt_genesis_handoff_step_fn run;
};
bool yt_genesis_handoff_run(struct yt_genesis_handoff_state *state,
	const struct yt_genesis_handoff_ops *ops, void *context,
	struct yt_error *error);
bool yt_planet_garrison_prompt(float player_forces, float planet_forces,
    uint8_t *prompt, size_t capacity, size_t *length);
float yt_planet_garrison_after(float player_forces, float desired,
    float planet_forces);
void yt_planet_garrison_overlay(struct yt_planet *planet, float desired,
    int player_record);
void yt_planet_garrison_player_overlay(struct yt_player *player,
    float remaining);
bool yt_planet_garrison_success_row(float desired, uint8_t *row,
    size_t capacity, size_t *length);

enum yt_planet_permission_output_kind {
	YT_PLANET_PERMISSION_RAW,
	YT_PLANET_PERMISSION_LINE,
	YT_PLANET_PERMISSION_BOLD_LINE,
};

struct yt_planet_permission_state {
	float planet_record_value;
	float planet_offset;
	int current_player_record;
	int last_player_record;
	uint32_t physical_planet_record;
	float updater_logical;
	struct yt_planet planet;
	float cached_owner;
	float cached_ground_forces;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
	size_t cached_name_length;
	struct yt_player friendship_current;
	struct yt_player friendship_owner;
	struct yt_player vacancy_owner;
	int owner_record;
	bool friendly;
	bool vacant;
	bool allowed;
	bool denied;
	float draws[2];
	float reduced_ground_forces;
	float foreground;
	float blink;
};

typedef bool (*yt_planet_permission_update_fn)(void *context,
	float logical_planet, struct yt_error *error);
typedef bool (*yt_planet_permission_read_planet_fn)(void *context,
	uint32_t physical_record, struct yt_planet *planet,
	struct yt_error *error);
typedef bool (*yt_planet_permission_write_planet_fn)(void *context,
	uint32_t physical_record, struct yt_planet *planet,
	struct yt_error *error);
typedef bool (*yt_planet_permission_read_player_fn)(void *context,
	int physical_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_planet_permission_present_fn)(void *context,
	const uint8_t *text, size_t length,
	enum yt_planet_permission_output_kind kind, const char *operation,
	struct yt_error *error);
typedef bool (*yt_planet_permission_sound_fn)(void *context, float selector,
	const char *operation, struct yt_error *error);
typedef bool (*yt_planet_permission_wait_fn)(void *context, double seconds,
	const char *operation, struct yt_error *error);
typedef bool (*yt_planet_permission_random_fn)(void *context, float *value,
	struct yt_error *error);
typedef void (*yt_planet_permission_foreground_fn)(void *context,
	float foreground);
typedef void (*yt_planet_permission_blink_fn)(void *context, float blink);

struct yt_planet_permission_ops {
	yt_planet_permission_update_fn update_planet;
	yt_planet_permission_read_planet_fn read_planet;
	yt_planet_permission_write_planet_fn write_planet;
	yt_planet_permission_read_player_fn read_player;
	yt_planet_permission_present_fn present;
	yt_planet_permission_sound_fn sound;
	yt_planet_permission_wait_fn wait;
	yt_planet_permission_random_fn random;
	yt_planet_permission_foreground_fn set_foreground;
	yt_planet_permission_blink_fn set_blink;
};

bool yt_planet_permission_run(struct yt_planet_permission_state *state,
	const struct yt_planet_permission_ops *ops, void *context,
	struct yt_error *error);
bool yt_planet_landing_record(float planet_offset, float sector_link,
    uint32_t *physical_record, float *updater_logical);
bool yt_planet_landing_immediate_allow(float ground_forces, float owner,
    int current_player_record);
bool yt_planet_landing_valid_owner(float owner, int last_player_record,
    int *physical_owner_record);
bool yt_planet_landing_vacant(float owner, float owner_status,
    int last_player_record);
float yt_planet_landing_attrition(float first_draw, float second_draw,
    float cached_ground_forces);
void yt_planet_landing_vacancy_overlay(struct yt_planet *planet,
    float ground_forces, int current_player_record);
bool yt_planet_landing_traffic_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_landing_sensor_row(float fresh_ground_forces,
    float cached_carried_forces, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_landing_amount_prompt(float cached_carried_forces,
    uint8_t *prompt, size_t capacity, size_t *length);
float yt_planet_landing_commitment(const char *response);
bool yt_planet_landing_commitment_valid(float commitment,
    float cached_carried_forces);
bool yt_planet_landing_unrest_row(float reduced, float original,
    uint8_t *row, size_t capacity, size_t *length);
void yt_planet_assault_player_overlay(struct yt_player *player,
    float commitment);
void yt_planet_assault_victory_overlay(struct yt_planet *planet,
    float owner, float attackers);
void yt_planet_assault_failure_overlay(struct yt_planet *planet,
    float defenders);
void yt_planet_assault_round(bool attacker_damage, float amount,
    float *attackers, float *defenders);
bool yt_planet_assault_attack_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, float commitment, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_planet_assault_status_row(bool attacker_damage, float remaining,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_planet_assault_capture_news(const uint8_t *player_name,
    size_t player_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_assault_failure_row(float defenders, bool news,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_planet_creation_credit_row(double credits, uint8_t *row,
    size_t capacity, size_t *length);
void yt_planet_creation_overlay(struct yt_planet *planet,
    int current_player_record);
void yt_planet_creation_timestamp_overlay(struct yt_planet *planet,
    float day, float minute);
void yt_planet_creation_credit_overlay(struct yt_player *player,
    float price_argument);
bool yt_planet_creation_news(const uint8_t *trader_name,
    size_t trader_name_length, const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_creation_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
float yt_planet_move_destination(const char *response);
float yt_planet_move_maximum(float port_record_offset,
    float sector_record_offset);
float yt_planet_move_add_cost(float cost);
float yt_planet_move_fighter_loss(float fighters, float first_draw,
    float second_draw);
void yt_planet_move_sector_overlay(struct yt_sector *sector,
    float planet_link);
void yt_planet_move_explosion_overlay(struct yt_planet *planet);
void yt_planet_move_fighter_overlay(struct yt_player *player, float loss);
void yt_planet_move_success_overlay(struct yt_player *player,
    float requested_destination);
bool yt_planet_move_path_heading(float start, float destination,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_planet_move_summary(float cost, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_move_turns_row(float turns, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_move_explosion_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_move_explosion_news(const uint8_t *planet_name,
    size_t planet_name_length, const uint8_t *player_name,
    size_t player_name_length, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_planet_move_loss_row(const uint8_t *actor, size_t actor_length,
    float loss, uint8_t *row, size_t capacity, size_t *length);
bool yt_planet_move_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length);
float yt_sector_mine_batch(float mines_before);
float yt_sector_mine_shield_result(float shields, float batch, float draw);
float yt_sector_mine_cloak_loss(float cloak, float batch, float draw);
float yt_sector_mine_missile_loss(float missiles, float batch, float draw);
typedef bool (*yt_sector_mine_draw_fn)(void *context, float *value,
    struct yt_error *error);
struct yt_sector_mine_missile_result {
	float remaining;
	float loss;
	bool applied;
};
bool yt_sector_mine_missile_step(float missiles, float batch,
    yt_sector_mine_draw_fn draw, void *context,
    struct yt_sector_mine_missile_result *result, struct yt_error *error);
float yt_sector_mine_empty_holds(const struct yt_player *player);
void yt_sector_mine_sector_overlay(struct yt_sector *sector,
    float mines_after);
void yt_sector_mine_player_overlay(struct yt_player *fresh,
    const struct yt_player *working, unsigned fields);
bool yt_sector_mine_explosion_row(float mines_before, float batch,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_sector_mine_shields_row(float shields, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_sector_mine_loss_row(enum yt_sector_mine_loss_kind kind, float loss,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_sector_mine_entry_news(const uint8_t *player_name,
    size_t player_name_length, float sector, uint8_t *row, size_t capacity,
    size_t *length);
bool yt_sector_mine_final_news(float shields, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_direct_fighter_mine_warning(const uint8_t *victim_name,
    size_t victim_name_length, uint8_t *row, size_t capacity,
    size_t *length);
struct yt_common_fatal_state {
	int current_player_record;
	float foreground;
	int pager_foreground;
	float target_record;
	struct yt_player field_player;
	bool field_valid;
	bool wait_complete;
	bool normal_exit;
};
struct yt_common_fatal_ops {
	void (*set_foreground)(void *context, float foreground,
	    int pager_foreground);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*sound)(void *context, float selector,
	    struct yt_error *error);
	bool (*death)(void *context, int victim_record, float killer,
	    struct yt_error *error);
	bool (*wait)(void *context, float duration,
	    struct yt_error *error);
};
bool yt_common_fatal_run(struct yt_common_fatal_state *state,
    const struct yt_common_fatal_ops *ops, void *context,
    struct yt_error *error);
enum yt_direct_fighter_kill_route {
	YT_DIRECT_FIGHTER_NO_KILL,
	YT_DIRECT_FIGHTER_FRESH_PROMPT,
	YT_DIRECT_FIGHTER_MINE_TERMINAL,
	YT_DIRECT_FIGHTER_COMMON_FATAL,
};
struct yt_direct_fighter_kill_state {
	float target_shields;
	int target_record;
	int current_player_record;
	float current_sector;
	float saved_mines;
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	size_t saved_name_length;
	bool destroyed;
	enum yt_direct_fighter_kill_route route;
};
struct yt_direct_fighter_kill_ops {
	bool (*sound)(void *context, struct yt_error *error);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*name_length)(void *context, float raw_length, size_t *length,
	    struct yt_error *error);
	bool (*death)(void *context, int victim_record, float killer,
	    struct yt_error *error);
	bool (*salvage)(void *context, int victim_record, int killer,
	    struct yt_error *error);
	bool (*read_sector)(void *context, float logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, float logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*mine)(void *context, bool *terminal, bool *destroyed,
	    struct yt_error *error);
	bool (*fatal)(void *context, struct yt_error *error);
};
bool yt_direct_fighter_kill_run(struct yt_direct_fighter_kill_state *state,
    const struct yt_direct_fighter_kill_ops *ops, void *context,
    struct yt_error *error);
float yt_emergency_warp_duration(float first, float second);
float yt_emergency_warp_destination(float draw, float sector_count);
float yt_emergency_warp_cost(float heat, float draw, float turns,
    bool meltdown);
void yt_emergency_warp_player_overlay(struct yt_player *player,
    float destination, float cost);
struct yt_gameplay_hazard_error_request {
	uint8_t error_number;
	uint16_t saved_ip;
	uint16_t handler;
};
bool yt_gameplay_hazard_error_project(unsigned error_number,
    unsigned saved_ip, struct yt_gameplay_hazard_error_request *request);
bool yt_emergency_warp_result_row(float destination, float cost,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_emergency_warp_stranded_row(float destination, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_movement_warp_row(const float warps[6], uint8_t *row,
    size_t capacity, size_t *length);
bool yt_movement_confirmation_prompt(float target, uint8_t *row,
    size_t capacity, size_t *length);
void yt_movement_player_overlay(struct yt_player *player, float target);
enum yt_danger_scan_output_kind {
	YT_DANGER_SCAN_LEADING_BLANK,
	YT_DANGER_SCAN_WARNING_RAW,
	YT_DANGER_SCAN_WARNING_TARGET,
	YT_DANGER_SCAN_WARNING_BLANK,
	YT_DANGER_SCAN_DISRUPTION,
	YT_DANGER_SCAN_MINES,
	YT_DANGER_SCAN_FIGHTERS,
	YT_DANGER_SCAN_FINAL_BLANK,
	YT_DANGER_SCAN_DEACTIVATED,
};
enum yt_danger_scan_step {
	YT_DANGER_SCAN_NONE,
	YT_DANGER_SCAN_TARGET_GET,
	YT_DANGER_SCAN_OWNER_GET,
	YT_DANGER_SCAN_OWNER_NAME_LEFT,
	YT_DANGER_SCAN_FRIENDSHIP_HELPER,
	YT_DANGER_SCAN_FRIEND_CURRENT_GET,
	YT_DANGER_SCAN_FRIEND_OWNER_GET,
	YT_DANGER_SCAN_TEAM_GET,
	YT_DANGER_SCAN_TEAM_NAME_LEFT,
	YT_DANGER_SCAN_RELATIONSHIP_CINT,
	YT_DANGER_SCAN_MUSIC,
	YT_DANGER_SCAN_PRESENT,
	YT_DANGER_SCAN_RESTORE_CURRENT,
};
enum yt_danger_scan_checkpoint {
	YT_DANGER_CHECK_OWNER_NAME_LEFT,
	YT_DANGER_CHECK_FRIENDSHIP_HELPER,
	YT_DANGER_CHECK_TEAM_NAME_LEFT,
	YT_DANGER_CHECK_RELATIONSHIP_CINT,
};
struct yt_danger_scan_state {
	float target;
	float sector_count;
	float sector_offset;
	float current_player_record;
	float disruption_sectors[2];
	uint8_t relationship_raw[4];
	float relationship;
	uint8_t finding_flag_raw[4];
	float finding_flag;
	float saved_foreground;
	struct yt_sector target_sector;
	struct yt_player owner_player;
	struct yt_sector team_overlay;
	enum yt_danger_scan_step attempted;
	enum yt_danger_scan_output_kind attempted_output;
	size_t output_count;
	bool target_read;
	bool owner_read;
	bool friendship_current_read;
	bool friendship_owner_read;
	bool team_read;
	bool current_player_restored;
	bool complete;
};
struct yt_danger_scan_ops {
	bool (*read_sector)(void *context, float logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*read_player)(void *context, float record,
	    struct yt_player *player, struct yt_error *error);
	bool (*restore_current)(void *context, struct yt_error *error);
	bool (*checkpoint)(void *context,
	    enum yt_danger_scan_checkpoint checkpoint,
	    struct yt_error *error);
	bool (*sound)(void *context, float selector, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_danger_scan_output_kind kind, struct yt_error *error);
	float (*foreground)(void *context);
	void (*set_foreground)(void *context, float value);
	void (*set_background)(void *context, float value);
	void (*set_blink)(void *context, float value);
	void (*store_relationship)(void *context, const uint8_t raw[4]);
};
bool yt_danger_scan_run(struct yt_danger_scan_state *state,
	const struct yt_danger_scan_ops *ops, void *context,
	struct yt_error *error);
enum yt_movement_output_kind {
	YT_MOVEMENT_WARP_ROW,
	YT_MOVEMENT_POST_WARP_BLANK,
	YT_MOVEMENT_DESTINATION_PROMPT,
	YT_MOVEMENT_SAME_SECTOR,
	YT_MOVEMENT_NOT_ADJACENT,
	YT_MOVEMENT_ACCEPTED_BLANK,
	YT_MOVEMENT_CONFIRMATION_BLANK,
};
enum yt_movement_route {
	YT_MOVEMENT_INCOMPLETE,
	YT_MOVEMENT_TURN_DENIED,
	YT_MOVEMENT_BOUNDS_CANCELLED,
	YT_MOVEMENT_SAME_SECTOR_ROUTE,
	YT_MOVEMENT_NOT_ADJACENT_ROUTE,
	YT_MOVEMENT_DANGER_DECLINED,
	YT_MOVEMENT_FINALIZER_TERMINAL,
	YT_MOVEMENT_MOVED,
};
struct yt_movement_state {
	int current_player_record;
	float port_offset;
	float sector_offset;
	float warps[6];
	struct yt_player player;
	struct yt_player accepted_player;
	float maximum;
	float target;
	uint8_t target_raw[4];
	size_t attempts;
	size_t matching_warp;
	bool turn_gate_called;
	bool turn_denied;
	bool target_stored;
	bool adjacent;
	bool danger_called;
	bool dangerous;
	bool confirmation_read;
	bool finalizer_called;
	bool self_mine_suppression_cleared;
	bool player_hydrated;
	bool player_written;
	bool player_flushed;
	bool cache_updated;
	bool complete;
	enum yt_movement_route route;
};
struct yt_movement_ops {
	bool (*turn_gate)(void *context, int player_record,
	    struct yt_player *player, bool *denied, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_movement_output_kind kind, struct yt_error *error);
	bool (*input)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*danger)(void *context, float target, bool *dangerous,
	    struct yt_error *error);
	void (*clear_queue)(void *context);
	bool (*confirm)(void *context, const uint8_t *prompt, size_t length,
	    bool *accepted, struct yt_error *error);
	bool (*finalize)(void *context, struct yt_error *error);
	void (*clear_self_mine_suppression)(void *context);
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*flush_player)(void *context, struct yt_error *error);
	bool (*update_cache)(void *context, int player_record,
	    const uint8_t raw[4], struct yt_error *error);
};
bool yt_movement_run(struct yt_movement_state *state,
	const struct yt_movement_ops *ops, void *context,
	struct yt_error *error);
enum yt_treasury_output_kind {
	YT_TREASURY_OPENING_BLANK,
	YT_TREASURY_NO_PORTS,
	YT_TREASURY_HEADING_PREFIX,
	YT_TREASURY_HEADING_SUFFIX,
	YT_TREASURY_SCAN_BLANK,
	YT_TREASURY_SECTOR_FIELD,
	YT_TREASURY_NAME_FIELD,
	YT_TREASURY_CREDIT_FIELD,
	YT_TREASURY_ROW_TOTAL,
	YT_TREASURY_NONZERO_BLANK,
	YT_TREASURY_TOTAL_PORTS,
	YT_TREASURY_WITH_CREDITS,
	YT_TREASURY_BARREN_PORTS,
	YT_TREASURY_TOTAL_CREDITS,
	YT_TREASURY_SUMMARY_BLANK,
	YT_TREASURY_REPORT_RESULT,
	YT_TREASURY_COLLECTION_RESULT,
};
enum yt_treasury_route {
	YT_TREASURY_INCOMPLETE,
	YT_TREASURY_NO_PORTS_ROUTE,
	YT_TREASURY_REPORT_ROUTE,
	YT_TREASURY_COLLECTION_ROUTE,
};
enum yt_treasury_caller_kind {
	YT_TREASURY_CALLER_MAIN_COLLECT,
	YT_TREASURY_CALLER_COMPUTER_COLLECT,
	YT_TREASURY_CALLER_COMPUTER_REPORT,
};
struct yt_treasury_state {
	float current_player_record;
	uint32_t current_player_physical_record;
	float port_offset;
	float planet_offset;
	uint8_t conversion_mode;
	bool collecting;
	struct yt_player initial_player;
	struct yt_player final_player;
	struct yt_port current_port;
	float loop_bound;
	float counter;
	float owned;
	float credited;
	float barren;
	uint8_t total_raw[8];
	double total;
	float current_record_expression;
	uint32_t current_physical_record;
	size_t records_read;
	size_t records_written;
	bool initial_player_read;
	bool final_player_read;
	bool player_written;
	bool player_flushed;
	bool cache_updated;
	bool complete;
	enum yt_treasury_route route;
};
struct yt_treasury_ops {
	bool (*read_player)(void *context, uint32_t physical_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*write_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_treasury_output_kind kind, struct yt_error *error);
	bool (*write_player)(void *context, uint32_t physical_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*flush_player)(void *context, struct yt_error *error);
	bool (*update_cache)(void *context, const struct yt_player *player,
	    struct yt_error *error);
};
bool yt_treasury_run(struct yt_treasury_state *state,
	const struct yt_treasury_ops *ops, void *context,
	struct yt_error *error);
struct yt_port_market_state {
	struct yt_port port;
	float logical_port;
	float port_record_expression;
	uint32_t port_physical_record;
	float current_day;
	float timer_seconds;
	float base_price[3];
	float current_minute;
	float elapsed;
	uint8_t capacity_raw[3][8];
	double capacity[3];
	uint8_t production_raw[3][4];
	uint8_t price_raw[3][4];
	float price[3];
	bool production_raised[3];
	size_t completed_items;
	bool complete;
};
bool yt_port_market_update(struct yt_port_market_state *state,
	struct yt_error *error);
struct yt_port_update_state {
	int sector_number;
	float sector_record_offset;
	float sector_record_expression;
	uint32_t sector_physical_record;
	float port_offset;
	float base_price[3];
	struct yt_sector sector;
	struct yt_port_market_state market;
	bool sector_loaded;
	bool sector_record_supplied;
	bool sector_read;
	bool day_observed;
	bool port_read;
	bool timer_observed;
	bool port_write_attempted;
	bool port_written;
	bool complete;
};
struct yt_port_update_ops {
	bool (*read_sector)(void *context, uint32_t physical_record,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*observe_day)(void *context, float *current_day,
	    struct yt_error *error);
	bool (*read_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*observe_timer)(void *context, float *timer_seconds,
	    struct yt_error *error);
	bool (*write_port)(void *context, uint32_t physical_record,
	    const struct yt_port *port, struct yt_error *error);
};
bool yt_port_update_run(struct yt_port_update_state *state,
	const struct yt_port_update_ops *ops, void *context,
	struct yt_error *error);
enum yt_port_report_output_kind {
	YT_PORT_REPORT_OWNER_BLANK,
	YT_PORT_REPORT_OWNER_ROW,
	YT_PORT_REPORT_TITLE_BLANK,
	YT_PORT_REPORT_TITLE,
	YT_PORT_REPORT_HEADER_BLANK,
	YT_PORT_REPORT_HEADER,
	YT_PORT_REPORT_RULE,
	YT_PORT_REPORT_ITEM_NAME_STATUS,
	YT_PORT_REPORT_ITEM_CAPACITY,
	YT_PORT_REPORT_ITEM_HOLD,
	YT_PORT_REPORT_ITEM_PRICE,
};
struct yt_port_report_state {
	int current_player_record;
	uint32_t port_physical_record;
	uint8_t conversion_mode;
	struct yt_port_market_state market;
	struct yt_player owner_player;
	struct yt_player current_player;
	struct yt_port report_port;
	uint8_t pager_line_count_raw[4];
	int owner_record;
	enum yt_port_owner_kind owner_kind;
	float foreground;
	float bold;
	size_t output_count;
	size_t completed_items;
	bool pager_reset;
	bool owner_player_read;
	bool current_player_read;
	bool report_port_read;
	bool date_observed;
	bool time_observed;
	bool complete;
};
struct yt_port_report_ops {
	bool (*read_player)(void *context, uint32_t physical_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*observe_date)(void *context, uint8_t date[10],
	    struct yt_error *error);
	bool (*observe_time)(void *context, uint8_t time[8],
	    struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_port_report_output_kind kind, size_t item,
	    struct yt_error *error);
	void (*reset_pager)(void *context, const uint8_t raw[4]);
	void (*set_bold)(void *context, float bold);
	void (*set_foreground)(void *context, float foreground);
};
bool yt_port_report_run(struct yt_port_report_state *state,
	const struct yt_port_report_ops *ops, void *context,
	struct yt_error *error);
enum yt_port_ordinary_field_kind {
	YT_PORT_ORDINARY_FIELD_INHERITED,
	YT_PORT_ORDINARY_FIELD_SECTOR,
	YT_PORT_ORDINARY_FIELD_PORT,
	YT_PORT_ORDINARY_FIELD_PLAYER,
};
struct yt_port_ordinary_state {
	struct yt_port_update_state update;
	struct yt_port_report_state report;
	enum yt_port_ordinary_field_kind field_kind;
	uint32_t field_record;
	struct yt_record field;
	bool field_valid;
	bool persistence_attempted;
	bool persistence_committed;
	bool report_started;
	bool complete;
};
bool yt_port_ordinary_run(struct yt_port_ordinary_state *state,
	const struct yt_port_update_ops *update_ops,
	const struct yt_port_report_ops *report_ops, void *context,
	struct yt_error *error);
enum yt_commodity_trade_output_kind {
	YT_COMMODITY_TRADE_STATUS,
	YT_COMMODITY_TRADE_MARKET,
	YT_COMMODITY_TRADE_QUANTITY_PROMPT,
	YT_COMMODITY_TRADE_CAPACITY_ERROR,
	YT_COMMODITY_TRADE_FREE_HOLDS_ERROR,
	YT_COMMODITY_TRADE_FREE_HOLDS_BLANK,
	YT_COMMODITY_TRADE_MAXIMUM_ERROR,
	YT_COMMODITY_TRADE_NOT_SELLING_ERROR,
	YT_COMMODITY_TRADE_DONT_WANT_ERROR,
	YT_COMMODITY_TRADE_PLAYER_AMOUNT_ERROR,
	YT_COMMODITY_TRADE_AGREED,
	YT_COMMODITY_TRADE_OFFER,
	YT_COMMODITY_TRADE_DECLINED,
	YT_COMMODITY_TRADE_SUCCESS,
};
enum yt_commodity_trade_route {
	YT_COMMODITY_TRADE_INCOMPLETE,
	YT_COMMODITY_TRADE_MAXIMUM_ZERO,
	YT_COMMODITY_TRADE_QUANTITY_CANCEL,
	YT_COMMODITY_TRADE_CAPACITY_REJECTED,
	YT_COMMODITY_TRADE_MAXIMUM_REJECTED,
	YT_COMMODITY_TRADE_DECLINED_ROUTE,
	YT_COMMODITY_TRADE_ACCEPTED,
};
struct yt_commodity_trade_state {
	uint32_t current_player_record;
	uint32_t port_physical_record;
	size_t commodity;
	struct yt_port_market_state market;
	struct yt_player player;
	struct yt_port port;
	uint8_t caller_trade_flag_raw[8];
	float free_holds;
	float maximum;
	float quantity;
	float total;
	float direction;
	float credit_delta;
	double selected_quantity;
	double displayed_hold;
	size_t quantity_attempts;
	size_t output_count;
	bool port_sells;
	bool prompt_reached;
	bool entry_player_read;
	bool treasury_port_read;
	bool treasury_port_written;
	bool credit_player_read;
	bool credit_player_written;
	bool hold_player_read;
	bool hold_player_written;
	bool stock_port_read;
	bool stock_port_written;
	bool complete;
	enum yt_commodity_trade_route route;
};
struct yt_commodity_trade_ops {
	bool (*read_player)(void *context, uint32_t physical_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, uint32_t physical_record,
	    const struct yt_player *player, struct yt_error *error);
	yt_credit_mutation_apply_fn mutate_credits;
	bool (*read_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*write_port)(void *context, uint32_t physical_record,
	    const struct yt_port *port, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_commodity_trade_output_kind kind,
	    struct yt_error *error);
	bool (*input)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*confirm)(void *context, const uint8_t *prompt, size_t length,
	    bool *accepted, struct yt_error *error);
};
bool yt_commodity_trade_run(struct yt_commodity_trade_state *state,
	const struct yt_commodity_trade_ops *ops, void *context,
	struct yt_error *error);
enum yt_ordinary_commerce_output_kind {
	YT_ORDINARY_COMMERCE_REFUSAL,
	YT_ORDINARY_COMMERCE_STATUS,
};
struct yt_ordinary_commerce_state {
	int sector_number;
	float sector_record_expression;
	uint32_t current_player_record;
	const uint8_t *first_name;
	size_t first_name_length;
	struct yt_port_market_state market;
	struct yt_player final_player;
	size_t schedule[3];
	size_t scheduled_count;
	size_t completed_trades;
	bool prompt_reached;
	bool update_complete;
	bool report_complete;
	bool refusal_presented;
	bool final_player_read;
	bool status_presented;
	bool complete;
};
struct yt_ordinary_commerce_ops {
	bool (*update)(void *context, int sector_number,
	    float sector_record_expression,
	    struct yt_port_market_state *market, struct yt_error *error);
	bool (*report)(void *context, const struct yt_port_market_state *market,
	    struct yt_error *error);
	bool (*trade)(void *context, const struct yt_port_market_state *market,
	    size_t commodity, bool *prompt_reached, struct yt_error *error);
	bool (*read_player)(void *context, uint32_t physical_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_ordinary_commerce_output_kind kind,
	    struct yt_error *error);
	void (*set_foreground)(void *context, float foreground);
};
bool yt_ordinary_commerce_run(struct yt_ordinary_commerce_state *state,
	const struct yt_ordinary_commerce_ops *ops, void *context,
	struct yt_error *error);
enum yt_port_docking_output_kind {
	YT_PORT_DOCKING_LABEL,
	YT_PORT_DOCKING_NO_PORT,
	YT_PORT_DOCKING_LEADING_BLANK,
	YT_PORT_DOCKING_PREFIX,
};
enum yt_port_docking_route {
	YT_PORT_DOCKING_INCOMPLETE,
	YT_PORT_DOCKING_GATE_DENIED,
	YT_PORT_DOCKING_NO_PORT_ROUTE,
	YT_PORT_DOCKING_FINALIZER_TERMINAL,
	YT_PORT_DOCKING_EARTH,
	YT_PORT_DOCKING_ORDINARY,
};
struct yt_port_docking_state {
	float port_offset;
	float gate_sector;
	float gate_sector_record_expression;
	uint32_t gate_sector_physical_record;
	struct yt_sector sector;
	float logical_port;
	float selected_port_expression;
	uint32_t selected_port_physical_record;
	struct yt_port selected_port;
	float post_finalizer_sector;
	float post_finalizer_sector_record_expression;
	bool label_presented;
	bool foreground_selected;
	bool gate_complete;
	bool gate_denied;
	bool sector_read;
	bool no_port_presented;
	bool docking_blank_presented;
	bool docking_prefix_presented;
	bool finalizer_complete;
	bool selected_port_read;
	bool child_complete;
	bool reenter_sector;
	bool complete;
	enum yt_port_docking_route route;
};
struct yt_port_docking_ops {
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_port_docking_output_kind kind, struct yt_error *error);
	void (*set_foreground)(void *context, float foreground);
	bool (*turn_gate)(void *context, bool *denied, float *current_sector,
	    float *sector_record_expression, struct yt_error *error);
	bool (*read_sector)(void *context, uint32_t physical_record,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*finalize)(void *context, bool *returned, float *current_sector,
	    float *sector_record_expression,
	    struct yt_error *error);
	bool (*read_port)(void *context, uint32_t physical_record,
	    struct yt_port *port, struct yt_error *error);
	bool (*earth)(void *context, bool *reenter_sector,
	    struct yt_error *error);
	bool (*ordinary)(void *context, int sector_number,
	    float sector_record_expression,
	    struct yt_error *error);
};
bool yt_port_docking_run(struct yt_port_docking_state *state,
	const struct yt_port_docking_ops *ops, void *context,
	struct yt_error *error);
size_t yt_port_trade_schedule(const float factors[3], size_t order[3]);
int yt_computer_selector_position(const char *command);
void yt_trade_treasury_overlay(struct yt_port *port, float receipt);
void yt_trade_credit_overlay(struct yt_player *player, float delta);
void yt_trade_holds_overlay(struct yt_player *player, size_t commodity,
    float quantity, float direction);
void yt_trade_stock_overlay(struct yt_port *port, size_t commodity,
    double cached_quantity, float quantity);
const char *yt_planet_take_one_title(int item);
void yt_planet_take_one_player_overlay(struct yt_player *player, int item,
    float amount);
void yt_planet_take_one_planet_overlay(struct yt_planet *planet, int item,
    double cached_quantity, float amount);
void yt_planet_take_all_weapon_player_overlay(struct yt_player *player,
    const double cached_quantity[10], double amount[10]);
void yt_planet_take_all_weapon_planet_overlay(struct yt_planet *planet,
    const double cached_quantity[10], const double amount[10]);
float yt_planet_take_all_commodity_player_overlay(struct yt_player *player,
    int item, double cached_quantity);
void yt_planet_take_all_commodity_planet_overlay(struct yt_planet *planet,
    int item, double cached_quantity, float amount);
void yt_planet_transfer_cargo_cache(float rate[10], double quantity[10],
    const double held[3]);
void yt_planet_transfer_cargo_player_overlay(struct yt_player *player);
void yt_planet_transfer_cargo_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10]);
void yt_planet_transfer_direct_player_overlay(struct yt_player *player,
    int item);
void yt_planet_transfer_direct_planet_overlay(struct yt_planet *planet,
    int item, double cached_quantity, float cached_amount);
void yt_planet_transfer_fighter_player_overlay(struct yt_player *player,
    float cached_fighters, float amount);
void yt_planet_transfer_fighter_planet_overlay(struct yt_planet *planet,
    double cached_quantity, float amount);
int yt_planet_transfer_selector_position(const char *command);
int yt_planet_menu_selector_position(const char *command);
bool yt_planet_transfer_cargo_empty(const double held[3]);
bool yt_planet_transfer_fighter_rejected(float amount,
    float cached_fighters);
bool yt_planet_transfer_fighter_amount(const char *response, float *amount,
    struct yt_error *error);
double yt_planet_bank_available(float cached_credits, float cached_bank);
double yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target);
void yt_planet_bank_planet_overlay(struct yt_planet *planet, double target);
float yt_planet_bank_credit_argument(float cached_bank, double target);
void yt_planet_bank_credit_overlay(struct yt_player *player, float argument);

double yt_planet_productivity_units(double spend);
void yt_planet_productivity_cache(float rate[10], double units,
    float delta[4]);
float yt_planet_productivity_credit_argument(double units);
void yt_planet_productivity_planet_overlay(struct yt_planet *planet,
    const float rate[10], const double quantity[10],
    const float contribution[10]);
bool yt_planet_rename_protected(float current_record, float planet_offset,
    float total_record_marker);
enum yt_planet_rename_name_result yt_planet_rename_prepare_name(char *name,
    size_t *length);
void yt_planet_rename_overlay(struct yt_planet *planet, const char *name,
    size_t length);
bool yt_clearance_candidate_needed(size_t item, float trigger_draw,
    float discount, bool create);
bool yt_clearance_normalize(size_t item, float *discount);
float yt_clearance_percentage(float discount);

void yt_earth_prices(const float discount[4], float price[4]);
double yt_earth_affordable(float credits, float price);
int yt_earth_selector_position(const char *command);
float yt_earth_purchase_quantity(double value);
float yt_earth_receipt_amount(float owner, int buyer_record, float cost);
float yt_earth_cloak_points(float cloak);
float yt_earth_cloak_default(float deficit, float credits);
float yt_earth_cloak_overlay(float points, float quantity);
void yt_earth_supply_overlay(struct yt_player *player, int choice,
    float quantity);
int yt_lottery_match_count(const int winning[6], const char ticket[6],
    bool matched_winning[6]);
float yt_lottery_award(int matches);
bool yt_player_name_matches(const struct yt_player *player,
    const uint8_t *name, size_t length, bool *matches,
    struct yt_error *error);
bool yt_player_stored_name(const struct yt_player *player,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error);
bool yt_port_stored_name(const struct yt_port *port,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error);
bool yt_planet_stored_name(const struct yt_planet *planet,
    uint8_t name[YT_TEXT_FIELD_SIZE], size_t *length,
    struct yt_error *error);
bool yt_sector_mine_warning_row(float mines, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_sector_candidate_eligible(int candidate, int current_player_record,
    float cached_sector, float logical_sector);
bool yt_sector_cloak_revealed(float draw, float cached_cloak);
size_t yt_sector_sensor_targets(const float caller_warps[6],
    float targets[6]);
bool yt_sector_port_row(const struct yt_port *port, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error);
bool yt_sector_planet_row(const struct yt_planet *planet, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error);
bool yt_sector_player_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error);
bool yt_sector_fighter_row(const struct yt_sector *sector,
    int current_player_record, const struct yt_player *owner,
    const struct yt_sector *team_overlay, uint8_t *row, size_t capacity,
    size_t *length, uint8_t *scratch, size_t scratch_capacity,
    size_t *scratch_length, bool *scratch_changed, struct yt_error *error);
bool yt_projectile_defense_row(float sector, const uint8_t *owner,
    size_t owner_length, double fighters, uint8_t *row, size_t capacity,
    size_t *length);
enum yt_projectile_candidate_route {
	YT_PROJECTILE_CANDIDATE_SKIP,
	YT_PROJECTILE_CANDIDATE_TERMINATE,
	YT_PROJECTILE_CANDIDATE_FRIENDSHIP
};
enum yt_projectile_candidate_route yt_projectile_candidate_route(
    int candidate, int shooter, float cached_sector, float sector,
    float remaining);
bool yt_projectile_candidate_admitted(int candidate, float cached_cloak,
    int xannor_provoker);
enum yt_projectile_death_route {
	YT_PROJECTILE_DEATH_REENTER_MINES,
	YT_PROJECTILE_DEATH_RETURN,
	YT_PROJECTILE_DEATH_NEXT_PLAYER
};
bool yt_projectile_player_survives(float shields);
bool yt_projectile_salvage_admitted(int counterattack, int xannor_provoker);
enum yt_projectile_death_route yt_projectile_death_continuation(
    float remaining, float saved_mines);
bool yt_projectile_survivor_sets_counterattack(int shooter);
bool yt_projectile_survivor_store_counterattack(int shooter,
    int player_record, int *counterattack, uint8_t raw[4]);
typedef bool (*yt_projectile_damage_draw_fn)(void *context, float *value,
    struct yt_error *error);
struct yt_projectile_damage_result {
	double fighters;
	float shields;
	bool scanner_disabled;
	size_t iterations;
};
bool yt_projectile_damage_iteration(float counter, float saved_missiles);
bool yt_projectile_survivor_overlay(struct yt_player *player, float shields,
    double fighters, float scanner, bool scanner_disabled);
bool yt_projectile_victim_mines_overlay(struct yt_player *player,
    float *saved_mines);
bool yt_projectile_sector_mines_overlay(struct yt_sector *sector,
    float carried_mines);
uint32_t yt_projectile_physical_record(float offset, float logical);
bool yt_projectile_planet_ground_overlay(struct yt_planet *planet,
    float ground, float owner);
bool yt_projectile_planet_productivity_overlay(struct yt_planet *planet,
    const float production[3], const float stock[3]);
bool yt_projectile_planet_destroy_overlay(struct yt_planet *planet);
bool yt_projectile_sector_unlink_overlay(struct yt_sector *sector);
struct yt_projectile_ground_result {
	float ground;
	float owner;
	size_t iterations;
};
bool yt_projectile_planet_ground_damage(float ground, float owner,
    float *remaining, yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_ground_result *result, struct yt_error *error);
struct yt_projectile_productivity_result {
	float old_total;
	float new_total;
	size_t iterations;
};
bool yt_projectile_planet_productivity_damage(float updater_ore,
    float production[3], float stock[3], float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_productivity_result *result,
    struct yt_error *error);
typedef bool (*yt_projectile_planet_read_fn)(void *context,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_error *error);
typedef bool (*yt_projectile_planet_write_fn)(void *context,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_error *error);
typedef bool (*yt_projectile_sector_read_fn)(void *context,
    uint32_t physical_record, struct yt_sector *sector,
    struct yt_error *error);
typedef bool (*yt_projectile_sector_write_fn)(void *context,
    uint32_t physical_record, struct yt_sector *sector,
    struct yt_error *error);
typedef bool (*yt_projectile_planet_present_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_planet_news_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_planet_sound_fn)(void *context,
    float selector, struct yt_error *error);
struct yt_projectile_planet_impact_ops {
	yt_projectile_damage_draw_fn random;
	yt_projectile_planet_read_fn read_planet;
	yt_projectile_planet_write_fn write_planet;
	yt_projectile_sector_read_fn read_sector;
	yt_projectile_sector_write_fn write_sector;
	yt_projectile_planet_present_fn present;
	yt_projectile_planet_news_fn append_news;
	yt_projectile_planet_sound_fn sound;
};
struct yt_projectile_planet_impact_state {
	struct yt_planet *planet;
	float updater_ore;
	float *remaining;
	uint32_t physical_planet;
	uint32_t physical_sector;
	bool early_return;
};
bool yt_projectile_planet_ground_row(float ground, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_projectile_planet_productivity_row(float old_total, float new_total,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_projectile_planet_impact_run(
    struct yt_projectile_planet_impact_state *state,
    const struct yt_projectile_planet_impact_ops *ops, void *context,
    struct yt_error *error);
enum yt_projectile_post_impact_route {
	YT_PROJECTILE_POST_IMPACT_FOOTER,
	YT_PROJECTILE_POST_IMPACT_NEXT_HOP,
};
enum yt_projectile_post_impact_route yt_projectile_post_impact_route(
    float remaining);
bool yt_projectile_route_has_next(int16_t next_hop);
bool yt_projectile_route_avoid_enabled(bool plasma, int counterattack,
    int shooter);
bool yt_projectile_route_failure_row(bool caller_suffix, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_projectile_footer_row(uint8_t *row, size_t capacity,
    size_t *length);
bool yt_projectile_player_damage(struct yt_player *target, float *remaining,
    yt_projectile_damage_draw_fn draw, void *context,
    struct yt_projectile_damage_result *result, struct yt_error *error);
bool yt_projectile_attack_first_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *victim, size_t victim_length, float sector,
    uint8_t *news, size_t news_capacity, size_t *news_length,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length);
bool yt_projectile_destroyed_rows(const uint8_t *victim,
    size_t victim_length, uint8_t *destroyed, size_t destroyed_capacity,
    size_t *destroyed_length, uint8_t *warning, size_t warning_capacity,
    size_t *warning_length);
bool yt_projectile_friendly_planet_row(const uint8_t *planet,
    size_t planet_length, uint8_t *row, size_t capacity, size_t *length);
bool yt_projectile_planet_attack_rows(bool plasma,
    const uint8_t *attacker, size_t attacker_length,
    const uint8_t *planet, size_t planet_length, float sector,
    uint8_t *direct, size_t direct_capacity, size_t *direct_length,
    uint8_t *news, size_t news_capacity, size_t *news_length);
bool yt_xannor_victory_winner(const uint8_t *player, size_t player_length,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_fixed_text_contains(const uint8_t field[YT_TEXT_FIELD_SIZE],
    const uint8_t *needle, size_t needle_length);
bool yt_radio_player_prompt(const struct yt_player *player, uint8_t *prompt,
    size_t capacity, size_t *length, struct yt_error *error);
bool yt_radio_tuning_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, struct yt_error *error);
bool yt_direct_attack_radio_text(const uint8_t *name, size_t name_length,
    double defender_loss, uint8_t *text, size_t capacity, size_t *length);

struct yt_hostile_surrender_state {
	int current_player_record;
	float old_owner;
	double attacker_loss;
	double defender_loss;
	double deployed_fighters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	struct yt_player current;
	double surrendered_fighters;
	double ship_fighters;
	double deployed_remaining;
	float fighter_owner;
	enum yt_hostile_surrender_route owner_route;
	bool checked;
	bool accepted;
	bool complete;
};

typedef bool (*yt_hostile_surrender_read_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_hostile_surrender_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_hostile_surrender_output_kind kind, struct yt_error *error);
enum yt_hostile_surrender_sound_kind {
	YT_HOSTILE_SURRENDER_RADIO_SOUND,
	YT_HOSTILE_SURRENDER_XANNOR_SOUND,
	YT_HOSTILE_SURRENDER_MERCENARY_SOUND,
	YT_HOSTILE_SURRENDER_JOINED_SOUND,
};
typedef bool (*yt_hostile_surrender_sound_fn)(void *context,
    enum yt_hostile_surrender_sound_kind kind, float selector,
    struct yt_error *error);
typedef bool (*yt_hostile_surrender_prompt_fn)(void *context,
    const uint8_t *prompt, size_t length,
    enum yt_hostile_surrender_answer *answer, struct yt_error *error);
typedef bool (*yt_hostile_surrender_news_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef void (*yt_hostile_surrender_cache_fn)(void *context,
    double ship_fighters, double deployed_fighters);
typedef void (*yt_hostile_surrender_mark_checked_fn)(void *context);

struct yt_hostile_surrender_ops {
	yt_hostile_surrender_read_fn read_player;
	yt_hostile_surrender_present_fn present;
	yt_hostile_surrender_sound_fn sound;
	yt_hostile_surrender_prompt_fn prompt;
	yt_hostile_surrender_news_fn append_news;
	yt_hostile_surrender_cache_fn cache_forces;
	yt_hostile_surrender_mark_checked_fn mark_checked;
};

bool yt_hostile_attack_surrender_run(
    struct yt_hostile_surrender_state *state,
    const struct yt_hostile_surrender_ops *ops, void *context,
    struct yt_error *error);

enum yt_hostile_attack_persistence_route {
	YT_HOSTILE_ATTACK_PERSISTENCE_NORMAL,
	YT_HOSTILE_ATTACK_PERSISTENCE_FATAL,
};

struct yt_hostile_attack_persistence_state {
	int current_player_record;
	int current_sector;
	double ship_fighters;
	float shields;
	double deployed_fighters;
	double defender_loss;
	float old_owner;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *owner_label;
	size_t owner_label_length;
	struct yt_player current;
	struct yt_sector sector;
	enum yt_hostile_attack_persistence_route route;
	bool player_written;
	bool sector_written;
	bool post_loss_read;
	bool news_written;
	bool mercenaries_hurt;
	bool complete;
};

typedef bool (*yt_hostile_attack_persistence_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_write_player_fn)(void *context,
    int player_record, const struct yt_player *player,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_read_sector_fn)(void *context,
    int sector_number, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_write_sector_fn)(void *context,
    int sector_number, const struct yt_sector *sector,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_blank_fn)(void *context,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_news_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_hostile_attack_persistence_fatal_fn)(void *context,
    struct yt_error *error);

struct yt_hostile_attack_persistence_ops {
	yt_hostile_attack_persistence_read_player_fn read_player;
	yt_hostile_attack_persistence_write_player_fn write_player;
	yt_hostile_attack_persistence_read_sector_fn read_sector;
	yt_hostile_attack_persistence_write_sector_fn write_sector;
	yt_hostile_attack_persistence_blank_fn present_blank;
	yt_hostile_attack_persistence_news_fn append_news;
	yt_hostile_attack_persistence_fatal_fn fatal;
};

bool yt_hostile_attack_persistence_run(
    struct yt_hostile_attack_persistence_state *state,
    const struct yt_hostile_attack_persistence_ops *ops, void *context,
    struct yt_error *error);

enum yt_hostile_attack_tail_output_kind {
	YT_HOSTILE_ATTACK_TAIL_REWARD_ROW,
	YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW,
};

struct yt_hostile_attack_tail_state {
	int current_player_record;
	float old_owner;
	double defender_loss;
	double deployed_fighters;
	double ship_fighters;
	float turns_per_day;
	float headquarters;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	struct yt_player current;
	float bonus;
	float dominated_draw;
	bool player_read;
	bool player_written;
	bool reward_presented;
	bool reward_news_written;
	bool clearance_called;
	bool draw_consumed;
	bool defeated_presented;
	bool victory_called;
	bool complete;
};

typedef bool (*yt_hostile_attack_tail_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_hostile_attack_tail_output_kind kind, struct yt_error *error);
typedef bool (*yt_hostile_attack_tail_clearance_fn)(void *context,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_tail_random_fn)(void *context, float *value,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_tail_victory_fn)(void *context,
    struct yt_error *error);

struct yt_hostile_attack_tail_ops {
	yt_hostile_attack_persistence_read_player_fn read_player;
	yt_hostile_attack_persistence_write_player_fn write_player;
	yt_hostile_attack_tail_present_fn present;
	yt_hostile_attack_persistence_news_fn append_news;
	yt_hostile_attack_tail_clearance_fn clearance;
	yt_hostile_attack_tail_random_fn random;
	yt_hostile_attack_tail_victory_fn victory;
};

bool yt_hostile_attack_tail_run(struct yt_hostile_attack_tail_state *state,
    const struct yt_hostile_attack_tail_ops *ops, void *context,
    struct yt_error *error);

enum yt_hostile_attack_combat_route {
	YT_HOSTILE_ATTACK_COMBAT_NORMAL,
	YT_HOSTILE_ATTACK_COMBAT_FATAL,
};

enum yt_hostile_attack_combat_output_kind {
	YT_HOSTILE_ATTACK_COMBAT_RESULT_BLANK,
	YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW,
	YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW,
	YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW,
	YT_HOSTILE_ATTACK_COMBAT_SPILL_BLANK,
};

struct yt_hostile_attack_combat_state {
	int current_player_record;
	int current_sector;
	double commitment;
	bool allow_surrender;
	double cached_defenders;
	struct yt_sector sector;
	struct yt_sector opened_sector;
	struct yt_player current;
	const uint8_t *cached_player_name;
	size_t cached_player_name_length;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	const uint8_t *owner_label;
	size_t owner_label_length;
	float turns_per_day;
	float headquarters;
	float old_owner;
	double old_count;
	double old_ship;
	double attacker_loss;
	double defender_loss;
	double ship_fighters;
	double deployed_remaining;
	float quantum;
	float last_draw;
	size_t iterations;
	bool surrender_checked;
	bool surrendered;
	bool spill_called;
	struct yt_hostile_surrender_state surrender;
	struct yt_hostile_attack_persistence_state persistence;
	struct yt_hostile_attack_tail_state tail;
	enum yt_hostile_attack_combat_route route;
	bool complete;
};

typedef bool (*yt_hostile_attack_combat_read_sector_fn)(void *context,
    int sector_number, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_sound_fn)(void *context,
    float selector, struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_random_fn)(void *context,
    float *value, struct yt_error *error);
typedef void (*yt_hostile_attack_combat_store_ship_fn)(void *context,
    double ship_fighters);
typedef bool (*yt_hostile_attack_combat_surrender_fn)(void *context,
    struct yt_hostile_surrender_state *state, struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_hostile_attack_combat_output_kind kind,
    struct yt_error *error);
typedef void (*yt_hostile_attack_combat_cache_player_fn)(void *context,
    const struct yt_player *player);
typedef void (*yt_hostile_attack_combat_cache_sector_fn)(void *context,
	const struct yt_sector *sector, double deployed_fighters);
typedef bool (*yt_hostile_attack_combat_spill_fn)(void *context,
    double *fighters, float *shields, struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_persistence_fn)(void *context,
    struct yt_hostile_attack_persistence_state *state,
    struct yt_error *error);
typedef bool (*yt_hostile_attack_combat_tail_fn)(void *context,
    struct yt_hostile_attack_tail_state *state, struct yt_error *error);

struct yt_hostile_attack_combat_ops {
	yt_hostile_attack_combat_read_sector_fn read_sector;
	yt_hostile_attack_combat_read_player_fn read_player;
	yt_hostile_attack_combat_sound_fn sound;
	yt_hostile_attack_combat_random_fn random;
	yt_hostile_attack_combat_store_ship_fn store_ship;
	yt_hostile_attack_combat_surrender_fn surrender;
	yt_hostile_attack_combat_present_fn present;
	yt_hostile_attack_combat_cache_player_fn cache_player;
	yt_hostile_attack_combat_cache_sector_fn cache_sector;
	yt_hostile_attack_combat_spill_fn spill;
	yt_hostile_attack_combat_persistence_fn persistence;
	yt_hostile_attack_combat_tail_fn tail;
};

bool yt_hostile_attack_combat_run(
    struct yt_hostile_attack_combat_state *state,
    const struct yt_hostile_attack_combat_ops *ops, void *context,
    struct yt_error *error);

struct yt_hostile_bribe_accept_state {
	int current_player_record;
	int current_sector;
	double cached_defenders;
	float offer;
	struct yt_sector sector;
	struct yt_player current;
	float persisted_fighters;
	float persisted_credits;
	bool deal_presented;
	bool sound_played;
	bool sector_read;
	bool sector_written;
	bool player_read;
	bool player_written;
	bool complete;
};

typedef bool (*yt_hostile_bribe_accept_present_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_sound_fn)(void *context,
    float selector, struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_read_sector_fn)(void *context,
    int sector_number, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_write_sector_fn)(void *context,
    int sector_number, const struct yt_sector *sector,
    struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_write_player_fn)(void *context,
    int player_record, const struct yt_player *player,
    struct yt_error *error);

struct yt_hostile_bribe_accept_ops {
	yt_hostile_bribe_accept_present_fn present;
	yt_hostile_bribe_accept_sound_fn sound;
	yt_hostile_bribe_accept_read_sector_fn read_sector;
	yt_hostile_bribe_accept_write_sector_fn write_sector;
	yt_hostile_bribe_accept_read_player_fn read_player;
	yt_hostile_bribe_accept_write_player_fn write_player;
};

bool yt_hostile_bribe_accept_run(
    struct yt_hostile_bribe_accept_state *state,
    const struct yt_hostile_bribe_accept_ops *ops, void *context,
    struct yt_error *error);

enum yt_hostile_bribe_route {
	YT_HOSTILE_BRIBE_INCOMPLETE,
	YT_HOSTILE_BRIBE_SCANNER,
	YT_HOSTILE_BRIBE_HOSTILE_MENU,
	YT_HOSTILE_BRIBE_COMBAT,
	YT_HOSTILE_BRIBE_FATAL,
};

enum yt_hostile_bribe_branch {
	YT_HOSTILE_BRIBE_BRANCH_INCOMPLETE,
	YT_HOSTILE_BRIBE_ORDINARY_REFUSAL,
	YT_HOSTILE_BRIBE_PLANET_REFUSAL,
	YT_HOSTILE_BRIBE_LIFE_DEMAND,
	YT_HOSTILE_BRIBE_EMPTY_OFFER,
	YT_HOSTILE_BRIBE_ACCEPTED,
	YT_HOSTILE_BRIBE_REJECTED,
};

enum yt_hostile_bribe_output_kind {
	YT_HOSTILE_BRIBE_ORDINARY_REFUSAL_ROW,
	YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW,
	YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW,
	YT_HOSTILE_BRIBE_INTRODUCTION_ROW,
	YT_HOSTILE_BRIBE_OFFER_PROMPT,
	YT_HOSTILE_BRIBE_REJECTED_ROW,
};

struct yt_hostile_bribe_state {
	int current_player_record;
	int current_sector;
	float owner;
	double cached_defenders;
	double ship_fighters;
	float shields;
	double credits;
	float planet_link;
	bool mercenaries_hurt;
	const uint8_t *real_first_name;
	size_t real_first_name_length;
	float draws[3];
	size_t draws_consumed;
	float offer;
	bool above_credits;
	double threshold;
	float commitment;
	bool forced_attack;
	bool direct_hostile_menu;
	bool accepted_called;
	bool combat_called;
	bool fatal_called;
	struct yt_hostile_bribe_accept_state accepted;
	enum yt_hostile_bribe_branch branch;
	enum yt_hostile_bribe_route route;
	bool complete;
};

typedef bool (*yt_hostile_bribe_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_hostile_bribe_output_kind kind, struct yt_error *error);
typedef bool (*yt_hostile_bribe_random_fn)(void *context, float *value,
    struct yt_error *error);
typedef bool (*yt_hostile_bribe_amount_fn)(void *context, char *response,
    size_t capacity, struct yt_error *error);
typedef bool (*yt_hostile_bribe_accept_fn)(void *context,
    struct yt_hostile_bribe_accept_state *state, struct yt_error *error);
typedef bool (*yt_hostile_bribe_combat_fn)(void *context,
    double commitment, struct yt_error *error);
typedef bool (*yt_hostile_bribe_fatal_fn)(void *context,
    struct yt_error *error);

struct yt_hostile_bribe_ops {
	yt_hostile_bribe_present_fn present;
	yt_hostile_bribe_random_fn random;
	yt_hostile_bribe_amount_fn amount;
	yt_hostile_bribe_accept_fn accept;
	yt_hostile_bribe_combat_fn combat;
	yt_hostile_bribe_fatal_fn fatal;
};

bool yt_hostile_bribe_run(struct yt_hostile_bribe_state *state,
    const struct yt_hostile_bribe_ops *ops, void *context,
    struct yt_error *error);
bool yt_direct_attack_team_row(const uint8_t *name, size_t name_length,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_direct_attack_candidate_prompt(const uint8_t *name,
    size_t name_length, uint8_t *prompt, size_t capacity, size_t *length);
bool yt_direct_attack_commitment_prompt(double fighters, uint8_t *prompt,
    size_t capacity, size_t *length);
bool yt_direct_attack_too_many_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_direct_attack_result_rows(double attacker_loss,
    double cached_reserve, double defender_loss, double defenders,
    uint8_t *attacker_row, size_t attacker_capacity,
    size_t *attacker_length, uint8_t *defender_row,
    size_t defender_capacity, size_t *defender_length);
struct yt_direct_attack_attrition_state {
	double committed;
	double defenders;
	float cloak;
	double attacker_loss;
	double defender_loss;
	float quantum;
	size_t iterations;
	bool complete;
};
typedef bool (*yt_direct_attack_attrition_draw_fn)(void *context,
    float *value, struct yt_error *error);
bool yt_direct_attack_attrition_run(
    struct yt_direct_attack_attrition_state *state,
    yt_direct_attack_attrition_draw_fn draw, void *context,
    struct yt_error *error);
enum yt_direct_attack_combat_route {
	YT_DIRECT_ATTACK_COMBAT_INCOMPLETE,
	YT_DIRECT_ATTACK_COMBAT_TOO_MANY,
	YT_DIRECT_ATTACK_COMBAT_CASUALTY_RETURN,
	YT_DIRECT_ATTACK_COMBAT_SHIELD_RETURN,
	YT_DIRECT_ATTACK_COMBAT_KILL_RETURN,
};
enum yt_direct_attack_combat_output_kind {
	YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW,
	YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW,
	YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW,
	YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW,
};
struct yt_direct_attack_combat_state {
	int current_player_record;
	int target_record;
	double committed;
	double defenders;
	double cached_reserve;
	double attacking;
	float target_shields;
	float current_sector;
	struct yt_player current;
	struct yt_player target;
	struct yt_direct_attack_attrition_state attrition;
	enum yt_direct_attack_combat_route route;
	bool reserve_written;
	bool current_casualty_written;
	bool target_casualty_written;
	bool target_shield_written;
	bool current_final_written;
	bool complete;
};
typedef bool (*yt_direct_attack_combat_read_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_direct_attack_combat_write_fn)(void *context,
    int player_record, const struct yt_player *player,
    struct yt_error *error);
typedef bool (*yt_direct_attack_combat_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_direct_attack_combat_output_kind kind,
    struct yt_error *error);
typedef bool (*yt_direct_attack_combat_sound_fn)(void *context,
    float selector, struct yt_error *error);
typedef bool (*yt_direct_attack_combat_radio_fn)(void *context,
    const uint8_t *text, size_t length, float recipient,
    struct yt_error *error);
typedef bool (*yt_direct_attack_combat_spill_fn)(void *context,
    double *fighters, float *shields, struct yt_error *error);
typedef bool (*yt_direct_attack_combat_kill_fn)(void *context,
    int target_record, int current_player_record, float current_sector,
    float target_shields, struct yt_error *error);
struct yt_direct_attack_combat_ops {
	yt_direct_attack_combat_read_fn read_player;
	yt_direct_attack_combat_write_fn write_player;
	yt_direct_attack_combat_present_fn present;
	yt_direct_attack_combat_sound_fn sound;
	yt_direct_attack_combat_radio_fn radio;
	yt_direct_attack_attrition_draw_fn random;
	yt_direct_attack_combat_spill_fn spill;
	yt_direct_attack_combat_kill_fn kill;
};
bool yt_direct_attack_combat_run(
    struct yt_direct_attack_combat_state *state,
    const struct yt_direct_attack_combat_ops *ops, void *context,
    struct yt_error *error);
enum yt_direct_attack_route {
	YT_DIRECT_ATTACK_INCOMPLETE,
	YT_DIRECT_ATTACK_NO_FIGHTERS,
	YT_DIRECT_ATTACK_EXHAUSTED,
	YT_DIRECT_ATTACK_CANCELLED,
	YT_DIRECT_ATTACK_COMBAT_RETURN,
};
enum yt_direct_attack_output_kind {
	YT_DIRECT_ATTACK_TITLE_ROW,
	YT_DIRECT_ATTACK_NO_FIGHTERS_ROW,
	YT_DIRECT_ATTACK_TEAM_ROW,
	YT_DIRECT_ATTACK_COMMITMENT_PROMPT,
	YT_DIRECT_ATTACK_NONE_SELECTED_ROW,
	YT_DIRECT_ATTACK_NONE_VISIBLE_ROW,
};
enum yt_direct_attack_confirmation {
	YT_DIRECT_ATTACK_CONFIRM_NO,
	YT_DIRECT_ATTACK_CONFIRM_YES,
	YT_DIRECT_ATTACK_CONFIRM_EMPTY,
};
struct yt_direct_attack_state {
	int current_player_record;
	float last_player_record;
	uint8_t conversion_mode;
	const struct yt_player_cache *player_cache;
	struct yt_player current;
	struct yt_player candidate_player;
	float candidate;
	float target_record_cell;
	double committed;
	bool encountered;
	bool enter_sector;
	enum yt_direct_attack_route route;
	bool complete;
};
typedef bool (*yt_direct_attack_read_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_direct_attack_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_direct_attack_output_kind kind, struct yt_error *error);
typedef bool (*yt_direct_attack_confirm_fn)(void *context,
    const uint8_t *prompt, size_t length,
    enum yt_direct_attack_confirmation *answer, struct yt_error *error);
typedef bool (*yt_direct_attack_amount_fn)(void *context, char *response,
    size_t capacity, struct yt_error *error);
typedef bool (*yt_direct_attack_combat_fn)(void *context, int target_record,
    double committed, struct yt_error *error);
struct yt_direct_attack_ops {
	yt_direct_attack_read_fn read_player;
	yt_player_record_store_fn store_target_record;
	yt_direct_attack_present_fn present;
	yt_direct_attack_confirm_fn confirm;
	yt_direct_attack_amount_fn amount;
	yt_direct_attack_combat_fn combat;
};
bool yt_direct_attack_run(struct yt_direct_attack_state *state,
    const struct yt_direct_attack_ops *ops, void *context,
    struct yt_error *error);
void yt_direct_attack_fighter_overlay(struct yt_player *player,
    float fighters);
void yt_direct_attack_shield_overlay(struct yt_player *player,
    float shields);
void yt_deployed_attack_player_overlay(struct yt_player *player,
    float shields, float fighters);
void yt_deployed_attack_sector_overlay(struct yt_sector *sector,
    float fighters);
void yt_death_player_overlay(struct yt_player *player, float killer);
bool yt_death_sector_overlay(struct yt_sector *sector, float victim);
void yt_death_team_roster_overlay(struct yt_record *record, float victim);
enum yt_death_port_route yt_death_port_overlay(struct yt_port *port,
    float victim, float killer, float last_player);
void yt_death_killer_credit_overlay(struct yt_player *player, float ports);
bool yt_death_title_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length);
bool yt_death_kill_news_row(const uint8_t *killer, size_t killer_length,
    const uint8_t *victim, size_t victim_length, bool self,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_death_port_news_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length);
struct yt_player_death_state {
	int victim_record;
	int current_player_record;
	float killer;
	int sector_count;
	int port_count;
	float last_player_record;
	const uint8_t *current_name;
	size_t current_name_length;
	struct yt_player victim;
	float old_ports_owned;
	int matched_ports;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	size_t victim_name_length;
	bool complete;
};
struct yt_player_death_ops {
	void (*clear_active_cache)(void *context, int victim_record,
	    const uint8_t raw[4]);
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, int logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*remove_team)(void *context, int victim_record,
	    struct yt_error *error);
	bool (*read_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*write_port)(void *context, int logical_port,
	    struct yt_port *port, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	void (*set_current_player)(void *context,
	    const struct yt_player *player);
	bool (*flush)(void *context, struct yt_error *error);
};
bool yt_player_death_run(struct yt_player_death_state *state,
    const struct yt_player_death_ops *ops, void *context,
    struct yt_error *error);
void yt_bribe_sector_overlay(struct yt_sector *sector);
void yt_bribe_player_overlay(struct yt_player *player, float fighters,
    float credits);
bool yt_player_killer_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, bool *emit, struct yt_error *error);
bool yt_projectile_target_prompt(bool plasma, float displayed,
    float maximum, uint8_t *prompt, size_t capacity, size_t *length);
enum yt_projectile_command_route {
	YT_PROJECTILE_COMMAND_INCOMPLETE,
	YT_PROJECTILE_COMMAND_NO_TURNS,
	YT_PROJECTILE_COMMAND_NO_AMMUNITION,
	YT_PROJECTILE_COMMAND_TARGET_CANCELLED,
	YT_PROJECTILE_COMMAND_QUANTITY_CANCELLED,
	YT_PROJECTILE_COMMAND_TOO_MANY,
	YT_PROJECTILE_COMMAND_FINALIZER_TERMINAL,
	YT_PROJECTILE_COMMAND_RETURNED,
	YT_PROJECTILE_COMMAND_FATAL,
};
enum yt_projectile_command_output_kind {
	YT_PROJECTILE_COMMAND_OPENING_BLANK,
	YT_PROJECTILE_COMMAND_NO_TURNS_ROW,
	YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW,
	YT_PROJECTILE_COMMAND_TARGET_PROMPT,
	YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW,
	YT_PROJECTILE_COMMAND_QUANTITY_PROMPT,
	YT_PROJECTILE_COMMAND_TOO_MANY_ROW,
	YT_PROJECTILE_COMMAND_ACCEPTED_BLANK,
};
struct yt_projectile_command_state {
	int current_player_record;
	float maximum_sector;
	bool plasma;
	float displayed;
	struct yt_player first_hydration;
	struct yt_player live_hydration;
	struct yt_player post_finalizer;
	uint8_t turn_gate_result_raw[4];
	size_t turn_gate_result_stores;
	size_t attempts;
	size_t hydrations;
	float available;
	float target;
	uint8_t target_raw[4];
	bool target_stored;
	float amount;
	uint8_t amount_raw[4];
	bool amount_stored;
	float origin;
	uint8_t origin_raw[4];
	int counterattack;
	int xannor_provoker;
	bool finalizer_called;
	bool player_written;
	bool player_flushed;
	bool destruction_cleared;
	bool resolver_called;
	bool counterlaunch_called;
	bool xannor_called;
	bool fatal_called;
	bool *destroyed;
	enum yt_projectile_command_route route;
	bool complete;
};
struct yt_projectile_command_ops {
	bool (*hydrate)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    enum yt_projectile_command_output_kind kind,
	    struct yt_error *error);
	bool (*input)(void *context, char *response, size_t capacity,
	    struct yt_error *error);
	bool (*finalize)(void *context, struct yt_player *player,
	    struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*flush)(void *context, struct yt_error *error);
	bool (*resolve)(void *context, float *origin, uint8_t origin_raw[4],
	    float *target, uint8_t target_raw[4], float *amount,
	    uint8_t amount_raw[4], bool plasma, int *counterattack,
	    int *xannor_provoker, struct yt_error *error);
	bool (*counterlaunch)(void *context, int *counterattack,
	    int *xannor_provoker, struct yt_error *error);
	bool (*xannor)(void *context, int *xannor_provoker,
	    struct yt_error *error);
	bool (*fatal)(void *context, struct yt_error *error);
	yt_destroyed_truth_fn destroyed_truth;
	bool (*counterattack_truth)(void *context);
	bool (*xannor_truth)(void *context);
	void (*store_turn_gate_result)(void *context, const uint8_t raw[4]);
};
bool yt_projectile_command_run(struct yt_projectile_command_state *state,
	const struct yt_projectile_command_ops *ops, void *context,
	struct yt_error *error);
enum yt_projectile_target_result yt_projectile_target_response(
    const char *response, float maximum, float *target);
float yt_projectile_quantity_response(const char *response);
void yt_projectile_debit_overlay(struct yt_player *player, bool plasma,
    float amount);
typedef bool (*yt_projectile_resolver_fn)(void *context, float *origin,
    float *target, float *amount, bool plasma, int *counterattack,
    int *xannor_provoker, struct yt_error *error);
bool yt_projectile_commit(struct yt_game *game, int player_record,
    struct yt_player *player, bool plasma, float *origin, float target,
    float amount, bool *destroyed, int *counterattack, int *xannor_provoker,
    yt_projectile_resolver_fn resolver, void *resolver_context,
    struct yt_error *error);
float yt_counterlaunch_score_count(double cached_score, float retained);
void yt_counterlaunch_debit_overlay(struct yt_player *fresh_target,
    float first_available, float selected_count);
bool yt_counterlaunch_rows(const uint8_t *target_name,
    size_t target_name_length, float selected_count,
    const uint8_t *saved_name, size_t saved_name_length,
    uint8_t *terminal, size_t terminal_capacity, size_t *terminal_length,
    uint8_t *news, size_t news_capacity, size_t *news_length);
bool yt_xannor_retaliation_run(struct yt_xannor_retaliation_state *state,
    const struct yt_xannor_retaliation_ops *ops, void *context,
    struct yt_error *error);
bool yt_salvage_header_row(const uint8_t *salvor, size_t salvor_length,
    const uint8_t *victim, size_t victim_length, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_salvage_simple_row(enum yt_salvage_simple_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_salvage_cargo_row(enum yt_salvage_cargo_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length);

enum yt_nearest_front_output_kind {
	YT_NEAREST_FRONT_FILTER_FIRST,
	YT_NEAREST_FRONT_FILTER_SECOND,
	YT_NEAREST_FRONT_FILTER_PROMPT,
	YT_NEAREST_FRONT_NO_TEAM,
	YT_NEAREST_FRONT_NO_PORTS,
	YT_NEAREST_FRONT_DIRECTION_BLANK,
	YT_NEAREST_FRONT_DIRECTION_PROMPT,
};

enum yt_nearest_front_result {
	YT_NEAREST_FRONT_INCOMPLETE,
	YT_NEAREST_FRONT_HANDOFF,
	YT_NEAREST_FRONT_REPROMPT,
};

struct yt_nearest_front_state {
	float current_team;
	float ports_owned;
	int selector;
	uint8_t direction;
	uint8_t filter_response[80];
	size_t filter_length;
	uint8_t direction_response[80];
	size_t direction_length;
	size_t hydrations;
	size_t outputs;
	size_t inputs;
	enum yt_nearest_front_result result;
};

struct yt_nearest_front_ops {
	bool (*hydrate)(void *context, float *current_team,
	    float *ports_owned, struct yt_error *error);
	bool (*present)(void *context,
	    enum yt_nearest_front_output_kind kind, const uint8_t *text,
	    size_t length, struct yt_error *error);
	bool (*input)(void *context, uint8_t *text, size_t capacity,
	    size_t *length, struct yt_error *error);
};

bool yt_nearest_front_run(struct yt_nearest_front_state *state,
	const struct yt_nearest_front_ops *ops, void *context,
	struct yt_error *error);

enum yt_nearest_field_kind {
	YT_NEAREST_FIELD_NONE,
	YT_NEAREST_FIELD_PLAYER,
	YT_NEAREST_FIELD_SECTOR,
	YT_NEAREST_FIELD_PORT,
	YT_NEAREST_FIELD_OWNER,
};

enum yt_nearest_present_mode {
	YT_NEAREST_PRESENT_LINE,
	YT_NEAREST_PRESENT_RAW,
	YT_NEAREST_PRESENT_BOLD_LINE,
	YT_NEAREST_PRESENT_BOLD_RAW,
};

enum yt_nearest_output_kind {
	YT_NEAREST_ENTRY_BLANK,
	YT_NEAREST_SCANNING,
	YT_NEAREST_SCAN_BLANK,
	YT_NEAREST_OWNER_INSTRUCTION,
	YT_NEAREST_OWNER_BLANK,
	YT_NEAREST_DISTANCE,
	YT_NEAREST_SECTOR,
	YT_NEAREST_ORE,
	YT_NEAREST_ORGANICS,
	YT_NEAREST_EQUIPMENT,
	YT_NEAREST_STOCK,
	YT_NEAREST_NAME,
	YT_NEAREST_PAGER_PROMPT,
	YT_NEAREST_PAGER_ECHO,
	YT_NEAREST_FINAL_BLANK,
};

enum yt_nearest_result {
	YT_NEAREST_INCOMPLETE,
	YT_NEAREST_COMPLETE,
	YT_NEAREST_PAGE_STOP,
};

struct yt_nearest_style {
	float foreground;
	float bold;
	float blink;
};

struct yt_nearest_market {
	float minute;
	float elapsed;
	float stock[3];
	float production[3];
	float factor[3];
	float stored_day;
	float stored_minute;
	float price[3];
};

struct yt_nearest_state {
	int selector;
	uint8_t direction;
	uint8_t conversion_mode;
	float actor_number;
	float sector_record_offset;
	float port_record_offset;
	float base_price[3];
	float cached_roster[4];
	struct yt_nearest_style style;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_player owner;
	struct yt_nearest_market market;
	struct yt_record field;
	enum yt_nearest_field_kind field_kind;
	uint32_t field_record;
	float current_record_expression;
	float current_team;
	float start_sector_raw;
	float display_sector;
	float current_day;
	float timer_seconds;
	float page_count;
	uint8_t page_count_raw[4];
	int current_sector;
	int distance;
	size_t rows;
	size_t outputs;
	size_t reads;
	size_t day_observations;
	size_t timer_observations;
	size_t roster_comparisons;
	bool field_valid;
	bool continuous;
	bool stopped;
	bool complete;
	enum yt_nearest_result result;
};

struct yt_nearest_ops {
	bool (*read_record)(void *context, enum yt_nearest_field_kind kind,
	    float expression, uint32_t physical_record,
	    struct yt_record *record, struct yt_error *error);
	bool (*observe_day)(void *context, float *day,
	    struct yt_error *error);
	bool (*observe_timer)(void *context, float *seconds,
	    struct yt_error *error);
	bool (*present)(void *context, enum yt_nearest_output_kind kind,
	    enum yt_nearest_present_mode mode, const uint8_t *text,
	    size_t length, struct yt_nearest_style *style,
	    struct yt_error *error);
	bool (*input)(void *context, uint8_t *key, bool *available,
	    struct yt_error *error);
	void (*uppercase)(void *context, uint8_t *text, size_t length);
};

bool yt_nearest_market_project(struct yt_nearest_market *market,
	const struct yt_port *port, const float base_price[3],
	float current_day, float timer_seconds, struct yt_error *error);
bool yt_nearest_run(struct yt_nearest_state *state,
	const struct yt_nearest_ops *ops, void *context,
	struct yt_error *error);

enum yt_profit_field_kind {
	YT_PROFIT_FIELD_NONE,
	YT_PROFIT_FIELD_PLAYER,
	YT_PROFIT_FIELD_SECTOR,
	YT_PROFIT_FIELD_PORT,
};

enum yt_profit_present_mode {
	YT_PROFIT_PRESENT_LINE,
	YT_PROFIT_PRESENT_RAW,
	YT_PROFIT_PRESENT_BOLD_LINE,
	YT_PROFIT_PRESENT_BOLD_RAW,
};

enum yt_profit_output_kind {
	YT_PROFIT_LEADING_BLANK,
	YT_PROFIT_TITLE,
	YT_PROFIT_TITLE_BLANK,
	YT_PROFIT_NO_CURRENT_PORT,
	YT_PROFIT_ROW,
	YT_PROFIT_COLUMN_SEPARATOR,
	YT_PROFIT_ROW_END,
	YT_PROFIT_NO_RESULTS,
	YT_PROFIT_PAGER_PROMPT,
	YT_PROFIT_PAGER_ECHO,
	YT_PROFIT_END_BANNER,
};

enum yt_profit_result {
	YT_PROFIT_INCOMPLETE,
	YT_PROFIT_COMPLETE,
	YT_PROFIT_NO_CURRENT_PORT_RESULT,
	YT_PROFIT_NO_RESULTS_RESULT,
	YT_PROFIT_STOPPED_BY_N,
	YT_PROFIT_UNRESOLVED_RAW_MEMORY,
};

enum yt_profit_checkpoint {
	YT_PROFIT_MAXIMUM_READY,
	YT_PROFIT_SOURCE_ARROW_READY,
	YT_PROFIT_ROW_CONSTRUCTION,
};

struct yt_profit_state {
	bool global;
	uint8_t conversion_mode;
	float current_sector_record;
	float sector_record_offset;
	float port_record_offset;
	float base_price[3];
	struct yt_nearest_style style;
	struct yt_record field;
	enum yt_profit_field_kind field_kind;
	uint32_t field_record;
	float current_record_expression;
	float current_day;
	float timer_seconds;
	float maximum_sector;
	float result_count;
	uint8_t initial_result_raw[4];
	uint8_t result_count_raw[4];
	size_t rows;
	size_t outputs;
	size_t reads;
	size_t day_observations;
	size_t timer_observations;
	size_t pager_prompts;
	bool field_valid;
	bool stopped;
	bool complete;
	enum yt_profit_result result;
};

struct yt_profit_ops {
	bool (*read_record)(void *context, enum yt_profit_field_kind kind,
	    float expression, uint32_t physical_record,
	    struct yt_record *record, struct yt_error *error);
	bool (*observe_day)(void *context, float *day,
	    struct yt_error *error);
	bool (*observe_timer)(void *context, float *seconds,
	    struct yt_error *error);
	bool (*present)(void *context, enum yt_profit_output_kind kind,
	    enum yt_profit_present_mode mode, const uint8_t *text,
	    size_t length, struct yt_nearest_style *style,
	    struct yt_error *error);
	bool (*input)(void *context, uint8_t *text, size_t capacity,
	    size_t *length, bool *available, struct yt_error *error);
	void (*uppercase)(void *context, uint8_t *text, size_t length);
	bool (*checkpoint)(void *context, enum yt_profit_checkpoint checkpoint,
	    struct yt_error *error);
};

bool yt_profit_run(struct yt_profit_state *state,
	const struct yt_profit_ops *ops, void *context,
	struct yt_error *error);

bool yt_current_player_hydrate(struct yt_player *player,
    const struct yt_player *fresh, int player_record,
    float sector_record_offset, bool anti_cloak_enabled,
    float *current_sector_record, struct yt_player_cache *player_cache,
    struct yt_error *error);

void yt_player_construct(struct yt_player *player,
    const struct yt_config *config, float today);

#endif
