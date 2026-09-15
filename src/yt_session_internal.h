#ifndef YT_SESSION_INTERNAL_H
#define YT_SESSION_INTERNAL_H

#include "yt_input.h"
#include "yt_maint.h"
#include "yt_pager.h"
#include "yt_route.h"
#include "yt_session.h"
#include "yt_team.h"

#define YT_COMMAND_SIZE 4096U

enum session_present_text_kind {
	SESSION_PRESENT_LINE,
	SESSION_PRESENT_RAW,
	SESSION_PRESENT_BOLD_LINE,
	SESSION_PRESENT_BOLD_RAW
};

enum session_fault_disposition {
	SESSION_FAULT_UNHANDLED,
	SESSION_FAULT_RESUME_GAMEPLAY,
	SESSION_FAULT_ENDED,
	SESSION_FAULT_HANDLER_FAILED,
};

struct session_projectile_state {
	int pending_counterattack_player;
	int pending_xannor_provoker;
	float retained_counterlaunch_missiles;
};

struct session_route_plan {
	int16_t next_hop[YT_ROUTE_CAPACITY];
};

struct session_combat_state {
	double ship_fighters;
	float ship_shields;
	double deployed_fighters;
	float hostile_owner;
	uint8_t hostile_owner_label[160];
	size_t hostile_owner_label_length;
	bool mercenaries_hurt;
};

struct session_navigation_state {
	float current_sector_physical_record;
	float current_warps[6];
	float route_marker;
	float route_start_sector;
	float avoided_sectors[YT_ROUTE_AVOID_COUNT];
	bool self_mines_suppressed;
};

struct yt_session {
	struct yt_door *door;
	struct yt_error *error;
	const char *executable_path;
	int active_player_record;
	struct session_projectile_state projectile;
	struct session_navigation_state navigation;
	bool destroyed;
	bool earth_report_seen;
	bool anti_cloak_enabled;
	float low_time_remembered;
	float inherited_loop_index;
	float planet_record_expression;
	float shared_target_record;
	float shared_status;
	int spy_count;
	int spy_sectors[3];
	int spy_markers[3];
	bool spy_found;
	struct yt_player player;
	struct session_combat_state combat;
	float market_bases[3];
	float clearance_discounts[4];
	struct yt_planet_economy planet_economy;
	float disruption_sectors[2];
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	size_t cached_player_name_length;
	struct yt_player_cache player_cache;
	char queue[YT_COMMAND_SIZE];
	size_t queue_length;
	size_t queue_position;
	char command_accumulator[YT_COMMAND_SIZE];
	char paged_text[YT_COMMAND_SIZE];
	char output_source[YT_COMMAND_SIZE];
	struct yt_input input;
	char saved_command[YT_COMMAND_SIZE];
	bool running;
	bool terminated;
	bool registered;
	bool fatal_wait_complete;
	struct yt_present_state presentation;
	char planet_name[42];
	struct yt_present_time_state time;
	struct yt_pager_state pager;
	struct yt_input_value input_residue;
	struct yt_team_cache team_cache;
};

int session_record(const struct yt_session *session);
bool session_current_date_serial(struct yt_session *session, int *serial,
    int *adjusted_year, struct yt_error *error);
float session_sector_offset(const struct yt_session *session);
float session_port_offset(const struct yt_session *session);
float session_planet_offset(const struct yt_session *session);
uint32_t session_port_basic_record(const struct yt_session *session,
    float logical_port);
int session_sector_count(const struct yt_session *session);
bool session_is_disruption_sector(const struct yt_session *session,
    float sector);
bool yt_session_players_are_friendly(struct yt_session *session,
    int candidate_record, bool *friendly, struct yt_error *error);
bool yt_session_destination_is_dangerous(struct yt_session *session,
    float target, bool *dangerous, struct yt_error *error);
bool yt_session_build_route(struct yt_session *session, float start,
    float destination, struct session_route_plan *plan, bool use_avoid,
    bool *found, enum yt_route_outcome *route_outcome,
    float *returned_status, struct yt_error *error);
bool yt_session_store_move(struct yt_session *session, float target,
    struct yt_error *error);
bool yt_session_command_move(struct yt_session *session, bool *moved,
    struct yt_error *error);
uint32_t session_planet_basic_record(const struct yt_session *session,
    float logical_planet);
uint32_t session_sector_basic_record(const struct yt_session *session,
    float logical_sector);
bool session_read_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool session_read_sector_at_fault(struct yt_session *session,
    int logical_sector, struct yt_sector *sector,
    enum yt_basic_fault_site site, struct yt_error *error);
bool session_read_player_at_fault(struct yt_session *session,
    int player_record, struct yt_player *player,
    enum yt_basic_fault_site site, struct yt_error *error);
bool session_read_player_expression(struct yt_session *session,
    float record_expression, struct yt_player *player,
    struct yt_error *error);
bool session_write_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error);
bool session_read_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool session_read_port_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_port *port, struct yt_error *error);
bool session_write_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error);
bool session_read_port_at_fault(struct yt_session *session, int logical_port,
    struct yt_port *port, enum yt_basic_fault_site site,
    struct yt_error *error);
bool session_read_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);
bool session_write_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error);
bool session_write_player(struct yt_session *session, struct yt_error *error);
void session_set_foreground(struct yt_session *session, float value);
void session_set_color(struct yt_session *session, int logical);
bool session_present_text(struct yt_session *session, const uint8_t *text,
    size_t length, enum session_present_text_kind kind,
    const char *operation, struct yt_error *error);
bool session_present_paged_fragment(struct yt_session *session,
    const uint8_t *text, size_t length);
bool session_present_paged_row(struct yt_session *session,
    const uint8_t *text, size_t length);
bool session_present_paged_line(struct yt_session *session,
    const uint8_t *text, size_t length, const char *operation,
    struct yt_error *error);
bool session_present_timed_paged_row(struct yt_session *session,
    const uint8_t *text, size_t length, const char *operation,
    struct yt_error *error);
bool session_right_aligned(struct yt_session *session, const char *text,
    float width, const char *operation, struct yt_error *error);
bool session_wait(struct yt_session *session, double seconds,
    const char *operation, struct yt_error *error);
void session_close_game(struct yt_session *session);
bool session_present_forced_local_line(const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error);
void session_set_pager_line_count_raw(struct yt_session *session,
    const uint8_t raw[4]);
void session_set_pager_line_count(struct yt_session *session, float value);
bool session_read_command(struct yt_session *session, char *text,
    size_t size);
bool session_read_upper_command(struct yt_session *session, char *text,
    size_t size);
bool session_read_number_command(struct yt_session *session, char *text,
    size_t size);
bool session_confirm(struct yt_session *session, const uint8_t *prompt,
    size_t prompt_length, enum yt_yes_no_answer *answer,
    struct yt_error *error);
bool session_present_alert(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error);
bool session_range_error(struct yt_error *error, const char *operation);
bool session_computer_error(struct yt_error *error, enum yt_status status,
    const char *operation);
bool session_buffer_append(uint8_t *buffer, size_t capacity, size_t *length,
    const void *data, size_t data_length);
void attach_database_get_fault(struct yt_session *session,
    struct yt_error *error, enum yt_basic_fault_site site);
void attach_database_put_fault(struct yt_session *session,
    struct yt_error *error, enum yt_basic_fault_site site);
bool session_commit_shared_terminal(struct yt_session *session,
    const struct yt_shared_error_result *result, struct yt_error *error);
enum session_fault_disposition session_route_basic_fault(
    struct yt_session *session, struct yt_error *error);
bool session_handle_gameplay_fault(struct yt_session *session,
    struct yt_error *error, bool *resume_gameplay);
bool session_attention_bytes(struct yt_session *session,
    const uint8_t *text, size_t length, const char *operation,
    struct yt_error *error);
bool session_press_any_key(struct yt_session *session, bool drain,
    struct yt_error *error);
bool session_sound(struct yt_session *session, float selector,
    const char *operation, struct yt_error *error);
bool yt_session_display_sector(struct yt_session *session, bool adjacent,
    struct yt_error *error);
bool yt_session_display_current_sector_cached(struct yt_session *session,
    struct yt_error *error);
bool yt_session_sector_entry(struct yt_session *session,
    struct yt_error *error);
bool yt_session_sector_force_is_friendly(struct yt_session *session,
    const struct yt_sector *sector, struct yt_error *error);
bool session_quit_confirm(struct yt_session *session, bool *confirmed,
    struct yt_error *error);
bool yt_session_quit(struct yt_session *session, struct yt_error *error);
bool session_display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error);
bool session_reload_player(struct yt_session *session,
    struct yt_error *error);
bool session_read_combat_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error);
bool session_write_combat_player(struct yt_session *session,
    int player_record, const struct yt_player *player,
    struct yt_error *error);
bool yt_session_fighter_shield_spill(struct yt_session *session,
    double *fighters, float *shields, bool bind_hostile_cells,
    struct yt_error *error);
bool yt_session_common_fatal_self(struct yt_session *session,
    struct yt_error *error);
bool session_mutate_player_credits(struct yt_session *session, float argument,
    bool *hydrated, struct yt_error *error);
bool read_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error);
bool write_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, const struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error);
bool session_load_team(struct yt_session *session, int id,
    struct yt_team *team, struct yt_error *error);
bool yt_session_load_team_cache(struct yt_session *session, int team_id,
    int current_player_record, struct yt_record *overlay,
    bool *overlay_loaded, bool *live, struct yt_error *error);
bool read_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_error *error);
bool session_write_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet, bool encode,
    struct yt_error *error);
bool yt_session_update_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_planet_economy *economy, struct yt_error *error);
bool yt_session_update_planet(struct yt_session *session,
    int logical_planet, struct yt_planet *planet,
    struct yt_planet_economy *economy, struct yt_error *error);
bool yt_session_update_port(struct yt_session *session, int sector_number,
    const float *sector_record_expression,
    const struct yt_sector *loaded_sector,
    struct yt_port_market_state *market, struct yt_error *error);
bool yt_session_port_report(struct yt_session *session, int logical_port,
    const struct yt_port_market_state *market,
    struct yt_port *terminal_port, struct yt_error *error);
bool yt_session_trade_commodity(struct yt_session *session,
    const struct yt_port_market_state *market, size_t commodity,
    bool *prompt_reached, struct yt_error *error);
bool yt_session_treasury(struct yt_session *session, bool collecting,
    struct yt_error *error);
bool yt_session_edit_port_name(struct yt_session *session, int logical_port,
    const uint8_t *cached, size_t cached_length, struct yt_port *port,
    struct yt_error *error);
bool yt_session_command_rename_port(struct yt_session *session,
    struct yt_error *error);
bool yt_session_command_buy_port(struct yt_session *session,
    struct yt_error *error);
bool yt_session_ordinary_commerce(struct yt_session *session,
    int sector_number, float sector_record_expression,
    struct yt_error *error);
bool yt_session_command_trade(struct yt_session *session,
    bool *enter_sector, struct yt_error *error);
bool yt_session_fresh_no_turn_gate(struct yt_session *session, bool *denied,
    struct yt_error *error);
bool yt_session_finalize_action(struct yt_session *session,
    struct yt_error *error);
bool yt_session_earth_store(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
bool session_earth_receipt(struct yt_session *session,
    const struct yt_port *cached_earth, float cost, struct yt_error *error);
bool session_earth_credit_error(struct yt_session *session, const char *text,
    struct yt_error *error);
bool session_earth_lottery(struct yt_session *session,
    const struct yt_port *cached_earth, struct yt_error *error);
void session_clear_queue(struct yt_session *session);
bool session_append_radio_bytes(const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error);
int session_radio_body_key(struct yt_session *session);
bool session_port_owner_row_capture(struct yt_session *session,
    const struct yt_port *port, uint8_t *captured_name,
    size_t captured_capacity, size_t *captured_length,
    struct yt_error *error);
bool session_earth_report(struct yt_session *session, struct yt_port *earth,
    float price[4], struct yt_error *error);
bool yt_session_salvage_player(struct yt_session *session, int victim_record,
    int killer_record, struct yt_error *error);
bool yt_session_kill_player(struct yt_session *session, int victim_record,
    float killer, bool wait_for_current, struct yt_error *error);
bool yt_session_command_attack(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
bool yt_session_attack_player(struct yt_session *session, int target_record,
    double committed, struct yt_error *error);
bool yt_session_attack_deployed(struct yt_session *session,
    struct yt_sector *sector, double commitment, bool allow_surrender,
    struct yt_error *error);
bool yt_session_bribe_deployed(struct yt_session *session,
    struct yt_sector *sector, bool *direct_hostile_menu,
    bool *forced_attack, struct yt_error *error);
bool yt_session_clearance(struct yt_session *session, bool create,
    struct yt_error *error);
bool yt_session_command_mines(struct yt_session *session,
    struct yt_error *error);
bool yt_session_mine_encounter(struct yt_session *session, bool *terminal,
    struct yt_error *error);
bool yt_session_emergency_warp(struct yt_session *session,
    struct yt_error *error);
bool yt_session_direct_emergency_warp(struct yt_session *session,
    struct yt_error *error);
bool yt_session_xannor_victory(struct yt_session *session,
    struct yt_error *error);
bool yt_session_launch_xannor_retaliation(struct yt_session *session,
    int *provoking_player, struct yt_error *error);
bool session_launch_projectile(struct yt_session *session, float *origin,
    float *target, float *amount, bool plasma, int *counterattack,
    int *xannor_provoker, struct yt_error *error);
bool yt_session_command_projectile(struct yt_session *session, bool plasma,
    struct yt_error *error);
bool yt_session_show_ship(struct yt_session *session,
    struct yt_error *error);
bool yt_session_info_team_lines(struct yt_session *session,
    struct yt_team *resolved_team, bool *current_is_captain,
    struct yt_error *error);
bool yt_session_spy_sweep(struct yt_session *session,
    struct yt_error *error);
bool yt_session_list_spies(struct yt_session *session,
    struct yt_error *error);
bool yt_session_registration(struct yt_session *session,
    struct yt_error *error);
bool yt_session_check_lockout(struct yt_session *session,
    struct yt_error *error);
bool yt_session_planet_permission(struct yt_session *session,
    int logical_planet, bool *denied, struct yt_error *error);
bool yt_session_planet_inventory(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_assault(struct yt_session *session,
    uint32_t physical_planet, float commitment, bool *defeated,
    struct yt_error *error);
bool yt_session_planet_take_one(struct yt_session *session,
    int logical_planet, int item, struct yt_error *error);
bool yt_session_planet_take_all(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_garrison(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_bank(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_rename(struct yt_session *session,
    int logical_planet, bool *renamed, struct yt_error *error);
bool yt_session_planet_transfer(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_productivity(struct yt_session *session,
    int logical_planet, struct yt_error *error);
bool yt_session_planet_menu(struct yt_session *session, int logical_planet,
    bool *enter_sector, struct yt_error *error);
bool yt_session_planet_move(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
bool yt_session_command_land(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
bool session_fixed_width_bytes(struct yt_session *session,
    const uint8_t *text, size_t text_length, float width,
    const char *operation, struct yt_error *error);

bool yt_session_computer_owned_fighters(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_owned_planets(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_nearest_ports(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_profit(struct yt_session *session, bool global,
    struct yt_error *error);
bool yt_session_computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error);
bool yt_session_computer_planet_report(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_port_report(struct yt_session *session,
    bool *enter_sector, struct yt_error *error);
bool yt_session_computer_avoid(struct yt_session *session,
    struct yt_error *error);
bool yt_session_radio_compose(struct yt_session *session,
    struct yt_error *error);
bool yt_session_radio_read(struct yt_session *session, bool log_mode,
    struct yt_error *error);
bool yt_session_command_team(struct yt_session *session,
    struct yt_error *error);
bool yt_session_generate_scoreboard(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_scoreboard(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_newspaper(struct yt_session *session,
    struct yt_error *error);
bool yt_session_computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
bool yt_session_command_genesis(struct yt_session *session,
    struct yt_error *error);
bool yt_session_command_fighters(struct yt_session *session,
    struct yt_error *error);
bool yt_session_instruction_offer(struct yt_session *session,
    struct yt_error *error);
bool yt_session_command_shell(struct yt_session *session,
    struct yt_error *error);

#endif
