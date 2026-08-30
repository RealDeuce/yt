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
struct yt_projectile_cruise_opening_ops {
	yt_projectile_opening_sound_fn sound;
	yt_projectile_opening_present_fn present;
};
bool yt_projectile_cruise_opening_run(int *last_mine_news_sector,
    const struct yt_projectile_cruise_opening_ops *ops, void *context,
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
bool yt_planet_transfer_cargo_empty(const double held[3]);
bool yt_planet_transfer_fighter_rejected(float amount,
    float cached_fighters);
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
