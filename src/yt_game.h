#ifndef YT_GAME_H
#define YT_GAME_H

#include "yt_config.h"
#include "yt_random.h"

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

struct yt_startup_configuration_state {
	struct yt_config *config;
	uint16_t installed_handler;
	bool handler_installed;
	float local_mode;
	float cache_guard;
	float *sector_cache;
	float *cloak_cache;
	size_t cache_count;
	float black_hole[2];
	size_t scoreboard_path_length;
};

typedef bool (*yt_startup_configuration_open_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_startup_configuration_close_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_startup_configuration_load_fn)(void *context,
	struct yt_config *config, struct yt_error *error);
typedef bool (*yt_startup_configuration_store_fn)(void *context,
	const struct yt_config *config, struct yt_error *error);
typedef bool (*yt_startup_configuration_read_player_fn)(void *context,
	int basic_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_startup_configuration_write_player_fn)(void *context,
	int basic_record, const struct yt_player *player,
	struct yt_error *error);
typedef bool (*yt_startup_configuration_random_fn)(void *context,
	float *value, struct yt_error *error);

struct yt_startup_configuration_ops {
	yt_startup_configuration_close_fn close_data;
	yt_startup_configuration_open_fn open_data;
	yt_startup_configuration_load_fn load_config;
	yt_startup_configuration_store_fn store_config;
	yt_startup_configuration_read_player_fn read_player;
	yt_startup_configuration_write_player_fn write_player;
	yt_startup_configuration_random_fn random;
};

bool yt_startup_configuration_run(
	struct yt_startup_configuration_state *state,
	const struct yt_startup_configuration_ops *ops, void *context,
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
	float captain;
	float roster[4];
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

struct yt_owned_planets_state {
	int maximum_sector;
	float planet_record_base;
	float current_player;
	int current_sector;
	float current_link;
	float current_record_expression;
	uint32_t current_planet_record;
	float foreground;
	float blink;
	bool found;
};

typedef bool (*yt_owned_planets_read_sector_fn)(void *context,
    int logical_sector, struct yt_sector *sector, struct yt_error *error);
typedef bool (*yt_owned_planets_read_planet_fn)(void *context,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_error *error);
typedef bool (*yt_owned_planets_present_fn)(void *context,
    const uint8_t *text, size_t length, bool bold, const char *operation,
    struct yt_error *error);
typedef void (*yt_owned_planets_color_fn)(void *context, int foreground);
typedef void (*yt_owned_planets_blink_fn)(void *context, float blink);

struct yt_owned_planets_ops {
	yt_owned_planets_read_sector_fn read_sector;
	yt_owned_planets_read_planet_fn read_planet;
	yt_owned_planets_present_fn present;
	yt_owned_planets_color_fn set_color;
	yt_owned_planets_blink_fn set_blink;
};

bool yt_owned_planets_run(struct yt_owned_planets_state *state,
    const struct yt_owned_planets_ops *ops, void *context,
    struct yt_error *error);

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

struct yt_post_login_repairs {
	bool turns;
	bool holds;
	unsigned writes;
};

enum yt_team_loader_route {
	YT_TEAM_LOADER_OUT_OF_RANGE,
	YT_TEAM_LOADER_ROSTER_DEAD,
	YT_TEAM_LOADER_LIVE,
};

struct yt_team_loader_cache {
	float available;
	float roster[4];
	float captain;
	float captain_flag;
	char name[YT_TEXT_FIELD_SIZE + 1U];
	size_t name_length;
	char password[5];
	float counter;
};
struct yt_team_loader_state {
	float team_id;
	float current_player_record;
	float sector_record_offset;
	uint8_t conversion_mode;
	struct yt_team_loader_cache *cache;
	struct yt_record overlay;
	uint32_t physical_record;
	enum yt_team_loader_route route;
	bool overlay_loaded;
	bool complete;
};
typedef bool (*yt_team_loader_read_record_fn)(void *context,
    uint32_t physical_record, struct yt_record *record,
    struct yt_error *error);
bool yt_team_loader_run(struct yt_team_loader_state *state,
    yt_team_loader_read_record_fn read_record, void *context,
    struct yt_error *error);

struct yt_death_team_remove_state {
	int victim_record;
	float current_player_record;
	float sector_record_offset;
	uint8_t conversion_mode;
	struct yt_team_loader_cache *cache;
	float raw_team_id;
	uint32_t overlay_physical_record;
	enum yt_team_loader_route loader_route;
	bool complete;
};
struct yt_death_team_remove_ops {
	bool (*read_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_player)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_record)(void *context, uint32_t physical_record,
	    struct yt_record *record, struct yt_error *error);
	bool (*write_record)(void *context, uint32_t physical_record,
	    const struct yt_record *record, struct yt_error *error);
};
bool yt_death_team_remove_run(struct yt_death_team_remove_state *state,
    const struct yt_death_team_remove_ops *ops, void *context,
    struct yt_error *error);

enum yt_info_team_route {
	YT_INFO_TEAM_NONE,
	YT_INFO_TEAM_SELF_CAPTAIN,
	YT_INFO_TEAM_OTHER_CAPTAIN,
	YT_INFO_TEAM_PROMOTED,
};
struct yt_info_team_state {
	float current_record;
	float sector_offset;
	uint8_t conversion_mode;
	struct yt_player current_player;
	float team_id;
	struct yt_team team;
	float captain_flag;
	float captain_record;
	uint8_t captain_name[YT_TEXT_FIELD_SIZE];
	size_t captain_name_length;
	bool current_is_captain;
	enum yt_info_team_route route;
};
typedef bool (*yt_info_team_read_player_fn)(void *context, float record,
    struct yt_player *player, struct yt_error *error);
typedef bool (*yt_info_team_load_team_fn)(void *context, float team_id,
    float current_record, float *captain_flag, struct yt_team *team,
    struct yt_error *error);
typedef bool (*yt_info_team_read_overlay_fn)(void *context, float team_id,
    struct yt_sector *overlay, struct yt_error *error);
typedef bool (*yt_info_team_write_overlay_fn)(void *context, float team_id,
    const struct yt_sector *overlay, struct yt_error *error);
typedef bool (*yt_info_team_present_fn)(void *context, const uint8_t *text,
    size_t length, struct yt_error *error);
struct yt_info_team_ops {
	yt_info_team_read_player_fn read_player;
	yt_info_team_load_team_fn load_team;
	yt_info_team_read_overlay_fn read_overlay;
	yt_info_team_write_overlay_fn write_overlay;
	yt_info_team_present_fn present;
};
bool yt_info_team_resolver_run(struct yt_info_team_state *state,
    const struct yt_info_team_ops *ops, void *context,
    struct yt_error *error);

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
	float active_spies;
	int *spy_sectors;
	int *last_reported_sectors;
	size_t spy_capacity;
	int current_player_record;
	float last_player_record;
	float disruption_sectors[2];
	float *sector_cache;
	float *cloak_cache;
	size_t cache_count;
	float found_scratch;
	float dead_counter_scratch;
	float warp_destination_scratch;
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
struct yt_projectile_cruise_opening_ops {
	yt_projectile_opening_sound_fn sound;
	yt_projectile_opening_present_fn present;
};
bool yt_projectile_cruise_opening_run(float *last_mine_news_sector,
    const struct yt_projectile_cruise_opening_ops *ops, void *context,
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
    float origin, float destination, int16_t *route, size_t route_capacity,
    float *status, struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_output_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_impact_fn)(void *context, int hop,
    double *energy, enum yt_projectile_plasma_impact_route *route,
    struct yt_error *error);
typedef bool (*yt_projectile_plasma_route_random_fn)(void *context,
    float *value, struct yt_error *error);
struct yt_projectile_plasma_route_ops {
	yt_projectile_plasma_route_build_fn build_route;
	yt_projectile_plasma_route_output_fn line;
	yt_projectile_plasma_route_output_fn attention;
	yt_projectile_opening_wait_fn wait;
	yt_projectile_plasma_route_random_fn random;
	yt_projectile_plasma_route_impact_fn impact;
	yt_projectile_plasma_route_output_fn footer;
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

struct yt_projectile_union_police_state {
	float hop;
	float destination;
	int counterattack;
	int xannor_provoker;
	bool intercepted;
};
bool yt_projectile_union_police_admitted(float hop, float destination,
    int counterattack, int xannor_provoker);
bool yt_projectile_union_police_run(
    struct yt_projectile_union_police_state *state,
    yt_projectile_cruise_reroute_output_fn present, void *context,
    struct yt_error *error);

struct yt_projectile_sector_probe_state {
	const struct yt_sector *sector;
	float hop;
	float player_terminal;
	const float *sector_cache;
	const float *cloak_cache;
	size_t cache_count;
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
	float player_terminal;
	const float *sector_cache;
	size_t cache_count;
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
typedef bool (*yt_projectile_plasma_player_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_player_output_kind kind,
    struct yt_error *error);
struct yt_projectile_plasma_player_ops {
	yt_projectile_plasma_player_read_fn read_player;
	yt_projectile_plasma_player_write_fn write_player;
	yt_projectile_plasma_player_color_fn color;
	yt_projectile_plasma_fighter_sound_fn sound;
	yt_projectile_plasma_fighter_random_fn random;
	yt_projectile_plasma_fighter_news_fn news;
	yt_projectile_plasma_player_present_fn present;
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
	float *sector_cache;
	size_t cache_count;
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

enum yt_projectile_plasma_footer_output_kind {
	YT_PROJECTILE_PLASMA_FOOTER_LEADING_BLANK,
	YT_PROJECTILE_PLASMA_FOOTER_TEXT,
	YT_PROJECTILE_PLASMA_FOOTER_TRAILING_BLANK,
};
typedef bool (*yt_projectile_plasma_footer_present_fn)(void *context,
    const uint8_t *text, size_t length,
    enum yt_projectile_plasma_footer_output_kind kind,
    struct yt_error *error);
struct yt_projectile_plasma_footer_ops {
	yt_projectile_plasma_footer_present_fn present;
};
bool yt_projectile_plasma_footer_run(
    const struct yt_projectile_plasma_footer_ops *ops, void *context,
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
	float *sector_cache;
	float *cloak_cache;
	size_t cache_count;
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
    float *origin, float target, float amount, bool plasma,
    int *counterattack, int *xannor_provoker, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_xannor_retaliation_wait_fn)(void *context, double seconds,
    struct yt_error *error);

struct yt_xannor_retaliation_ops {
	yt_xannor_retaliation_read_sector_fn read_sector;
	yt_xannor_retaliation_random_fn random;
	yt_xannor_retaliation_present_fn present;
	yt_xannor_retaliation_projectile_fn projectile;
	yt_xannor_retaliation_read_player_fn read_player;
	yt_xannor_retaliation_wait_fn wait;
};

struct yt_counterlaunch_state {
	struct yt_player *player;
	int *player_record;
	float *sector_cache;
	float *cloak_cache;
	size_t cache_count;
	bool *destroyed;
	float *retained_count;
	int *counterattacker;
	int *xannor_provoker;
	int last_player_record;
};

typedef bool (*yt_counterlaunch_read_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_counterlaunch_random_fn)(void *context, float *value,
    struct yt_error *error);
typedef bool (*yt_counterlaunch_write_player_fn)(void *context,
    int player_record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_counterlaunch_present_fn)(void *context,
    const uint8_t *text, size_t length, bool bold, struct yt_error *error);
typedef bool (*yt_counterlaunch_news_fn)(void *context, const uint8_t *text,
    size_t length, struct yt_error *error);
typedef bool (*yt_counterlaunch_projectile_fn)(void *context, float *origin,
    float target, float *amount, bool plasma, int *counterattack,
    int *xannor_provoker, struct yt_error *error);
typedef bool (*yt_counterlaunch_wait_fn)(void *context, double seconds,
    struct yt_error *error);

struct yt_counterlaunch_ops {
	yt_counterlaunch_read_player_fn read_player;
	yt_counterlaunch_random_fn random;
	yt_counterlaunch_write_player_fn write_player;
	yt_counterlaunch_present_fn present;
	yt_counterlaunch_news_fn append_news;
	yt_counterlaunch_projectile_fn projectile;
	yt_counterlaunch_wait_fn wait;
};

struct yt_salvage_cargo_state {
	float requested;
	float stock[3];
	float remaining;
	float awards[4];
};

struct yt_salvage_state {
	int victim_record;
	float killer_record;
	float last_player_record;
	float maximum_holds;
	const uint8_t *current_name;
	size_t current_name_length;
	struct yt_player victim;
	struct yt_player killer;
	float opening_draws[6];
	float awards[6];
	float requested_holds;
	struct yt_salvage_cargo_state cargo;
	bool admitted;
	bool emitted;
	bool complete;
};
struct yt_salvage_ops {
	bool (*read_victim)(void *context, int player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*read_killer)(void *context, float player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*write_killer)(void *context, float player_record,
	    struct yt_player *player, struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*cargo_draw)(void *context, float range, float *one_based,
	    struct yt_error *error);
	bool (*wait)(void *context, float duration, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    bool bold, struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
};
bool yt_salvage_run(struct yt_salvage_state *state,
    const struct yt_salvage_ops *ops, void *context,
    struct yt_error *error);

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

typedef bool (*yt_salvage_cargo_draw_fn)(void *context, float range,
    float *one_based, struct yt_error *error);

struct yt_current_player_hydration_state {
	struct yt_player *player;
	int player_record;
	int last_player_record;
	float sector_record_offset;
	float *current_sector_record;
	float *sector_cache;
	float *cloak_cache;
	size_t cache_count;
	bool anti_cloak;
};

typedef bool (*yt_current_player_read_fn)(void *context, int player_record,
    struct yt_player *player, struct yt_error *error);

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

struct yt_planet_updater_state {
	uint8_t logical_planet_raw[4];
	uint8_t planet_offset_raw[4];
	uint8_t current_day_raw[4];
	uint8_t timer_seconds_raw[4];
	uint32_t physical_record;
	struct yt_record field;
	struct yt_planet_updater_cache cache;
	enum yt_planet_updater_stage stage;
	size_t effect_count;
	size_t completed_effects;
	bool field_loaded;
	bool field_dirty;
	bool written;
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
bool yt_game_construct_player(struct yt_game *game, int basic_record,
    float today, struct yt_player *player, struct yt_error *error);
bool yt_game_set_player_identity(struct yt_game *game, int basic_record,
    const uint8_t *name, size_t length, struct yt_player *player,
    struct yt_error *error);
void yt_team_loader_begin(float team_id, struct yt_team_loader_cache *cache,
    bool *needs_overlay);
bool yt_team_loader_finish(const struct yt_record *overlay,
    float current_player, uint8_t conversion_mode,
    struct yt_team_loader_cache *cache, enum yt_team_loader_route *route,
    struct yt_error *error);
bool yt_game_post_login_repairs(struct yt_game *game, int basic_record,
    float maximum_holds, struct yt_player *player,
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
bool yt_hostile_defeated_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_xannor_attack_reward_rows(const uint8_t *name, size_t name_length,
    float bonus, double defenders_destroyed,
    uint8_t *display, size_t display_capacity, size_t *display_length,
    uint8_t *news, size_t news_capacity, size_t *news_length);
float yt_xannor_attack_bonus(double defenders_destroyed, float turns,
    float turns_per_day);
bool yt_bribe_ordinary_forces(float owner, float defenders,
    float ship_fighters, float draw);
bool yt_bribe_mercenary_forces(float defenders, float ship_fighters,
    float first, float second, bool sticky);
double yt_bribe_offer_threshold(float defenders, float draw);
bool yt_bribe_offer_accepted(float offer, float credits, double threshold);
enum yt_bribe_forced_admission yt_bribe_forced_admit(
    double ship_fighters, float shields, bool mercenary_fatal_gate,
    float commitment);
enum yt_sector_mine_admission yt_sector_mine_admit(
    float carried, float amount);
bool yt_no_turn_gate_denied(float turns);
bool yt_team_choice_rejected(float choice, float raw_team,
    int32_t captain_cint, int32_t team_cint);
void yt_team_transfer_apply_sector(struct yt_sector *sector,
    double initial_fighters, float amount);
void yt_team_transfer_apply_player(struct yt_player *player, float amount);
void yt_team_banish_apply_player(struct yt_player *player);
void yt_team_roster_overlay(struct yt_record *record, const float roster[4]);
void yt_team_name_overlay(struct yt_record *record, const uint8_t *name,
    size_t length);
bool yt_team_prepare_name(char *name, size_t *length);
void yt_team_password_overlay(struct yt_record *record,
    const uint8_t password[4]);
bool yt_port_link_missing(float link);
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
double yt_port_purchase_price(const float production[3]);
float yt_port_purchase_seller_credit(float treasury, float credits,
    double price);
float yt_port_purchase_buyer_credit(float credits, double price);
bool yt_genesis_confirmation_prompt(const uint8_t *trader,
    size_t trader_length, uint8_t *prompt, size_t capacity, size_t *length);
bool yt_genesis_insufficient_rows(float required, float owned,
    uint8_t *first, size_t first_capacity, size_t *first_length,
    uint8_t *second, size_t second_capacity, size_t *second_length);
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
	bool (*sound)(void *context, struct yt_error *error);
	bool (*death)(void *context, int victim_record, float killer,
	    struct yt_error *error);
	bool (*wait)(void *context, float duration, struct yt_error *error);
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
	uint8_t destroyed_raw[4];
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
	bool (*salvage)(void *context, int victim_record, float killer,
	    struct yt_error *error);
	bool (*read_sector)(void *context, float logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*write_sector)(void *context, float logical_sector,
	    struct yt_sector *sector, struct yt_error *error);
	bool (*present)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*news)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*mine)(void *context, bool *terminal, uint8_t destroyed_raw[4],
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
bool yt_emergency_warp_result_row(float destination, float cost,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_emergency_warp_stranded_row(float destination, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_movement_warp_row(const float warps[6], uint8_t *row,
    size_t capacity, size_t *length);
bool yt_movement_confirmation_prompt(float target, uint8_t *row,
    size_t capacity, size_t *length);
void yt_movement_player_overlay(struct yt_player *player, float target);
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
double yt_planet_bank_available(float cached_credits, float cached_bank);
double yt_planet_bank_remaining(float cached_credits, float cached_bank,
    double target);
void yt_planet_bank_planet_overlay(struct yt_planet *planet, double target);
float yt_planet_bank_credit_argument(float cached_bank, double target);
void yt_planet_bank_credit_overlay(struct yt_player *player, float argument);

typedef bool (*yt_credit_mutation_write_fn)(void *context,
    int player_record, const struct yt_record *record,
    struct yt_error *error);

struct yt_credit_mutation_ops {
	yt_current_player_read_fn read_player;
	yt_credit_mutation_write_fn write_player;
};

struct yt_credit_mutation_state {
	struct yt_current_player_hydration_state hydration;
	float argument;
	float fresh_credits;
	float summed_credits;
	float result_credits;
	uint8_t argument_raw[4];
	uint8_t fresh_credits_raw[4];
	uint8_t summed_credits_raw[4];
	uint8_t result_credits_raw[4];
	bool hydrated;
	bool overlay_applied;
	bool write_attempted;
	bool written;
};

bool yt_credit_mutation_run(struct yt_credit_mutation_state *state,
    const struct yt_credit_mutation_ops *ops, void *context,
    struct yt_error *error);
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
struct yt_earth_anti_cloak_state {
	float price;
	float current_record;
	float player_terminal;
	uint8_t conversion_mode;
	float *cloak_cache;
	size_t cloak_cache_count;
	float foreground;
	float counter;
	bool reported;
	struct yt_player field_player;
	float field_record;
	float credit_argument;
	bool credit_loaded;
};
typedef bool (*yt_earth_anti_cloak_read_player_fn)(void *context,
    float record, struct yt_player *player, struct yt_error *error);
typedef bool (*yt_earth_anti_cloak_present_fn)(void *context,
    const uint8_t *text, size_t length, float foreground, bool bold,
    struct yt_error *error);
typedef bool (*yt_earth_anti_cloak_sound_fn)(void *context, float selector,
    struct yt_error *error);
struct yt_earth_anti_cloak_ops {
	yt_earth_anti_cloak_read_player_fn read_player;
	yt_credit_mutation_apply_fn mutate_credits;
	yt_earth_anti_cloak_present_fn present;
	yt_earth_anti_cloak_sound_fn sound;
};
bool yt_earth_anti_cloak_run(struct yt_earth_anti_cloak_state *state,
    const struct yt_earth_anti_cloak_ops *ops, void *context,
    struct yt_error *error);
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
bool yt_direct_attack_radio_text(const uint8_t *name, size_t name_length,
    double defender_loss, uint8_t *text, size_t capacity, size_t *length);
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
	void (*clear_active_cache)(void *context, int victim_record);
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
enum yt_projectile_target_result yt_projectile_target_response(
    const char *response, float maximum, float *target);
float yt_projectile_quantity_response(const char *response);
void yt_projectile_debit_overlay(struct yt_player *player, bool plasma,
    float amount);
typedef bool (*yt_projectile_resolver_fn)(void *context, float *origin,
    float target, float amount, bool plasma, int *counterattack,
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
bool yt_counterlaunch_run(struct yt_counterlaunch_state *state,
    const struct yt_counterlaunch_ops *ops, void *context,
    struct yt_error *error);
bool yt_salvage_cargo_sample(struct yt_salvage_cargo_state *state,
    yt_salvage_cargo_draw_fn draw, void *context, struct yt_error *error);
bool yt_salvage_header_row(const uint8_t *salvor, size_t salvor_length,
    const uint8_t *victim, size_t victim_length, uint8_t *row,
    size_t capacity, size_t *length);
bool yt_salvage_simple_row(enum yt_salvage_simple_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_salvage_cargo_row(enum yt_salvage_cargo_kind kind, float amount,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_current_player_hydrate_run(
    struct yt_current_player_hydration_state *state,
    yt_current_player_read_fn read_player, void *context,
    struct yt_error *error);

void yt_player_construct(struct yt_player *player,
    const struct yt_config *config, float today);

#endif
