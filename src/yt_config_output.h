#ifndef YT_CONFIG_OUTPUT_H
#define YT_CONFIG_OUTPUT_H

#include "yt_config.h"

#define YT_CONFIG_OUTPUT_SIZE 1024U

struct yt_config_menu_working {
	const uint8_t *scoreboard_path;
	size_t scoreboard_path_length;
	float local_screen;
	float lottery_plays;
	float maximum_holds;
};

struct yt_config_output_result {
	uint8_t output[YT_CONFIG_OUTPUT_SIZE];
	size_t output_length;
	size_t final_column;
	unsigned local_beeps;
};

enum yt_config_hq_diagnostic {
	YT_CONFIG_HQ_INVALID,
	YT_CONFIG_HQ_OCCUPIED
};

enum yt_config_scalar_key {
	YT_CONFIG_SCALAR_MAXIMUM_HOLDS = 'A',
	YT_CONFIG_SCALAR_TURNS = 'B',
	YT_CONFIG_SCALAR_FIGHTERS = 'C',
	YT_CONFIG_SCALAR_CREDITS = 'D',
	YT_CONFIG_SCALAR_INITIAL_HOLDS = 'E',
	YT_CONFIG_SCALAR_DEAD_DAYS = 'F',
	YT_CONFIG_SCALAR_MAINTENANCE = 'G',
	YT_CONFIG_SCALAR_LOTTERY = 'K'
};

bool yt_config_prepare_menu_working(const struct yt_config *config,
    uint8_t scoreboard_path[41], struct yt_config_menu_working *working);
bool yt_config_compose_menu_prompt(const struct yt_config *config,
    const struct yt_config_menu_working *working, int today,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_command_echo(uint8_t command,
    size_t initial_column, uint8_t *folded,
    struct yt_config_output_result *result);
bool yt_config_compose_exit(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_missing_data(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_genesis_prompt(const uint8_t *current_value,
    size_t current_value_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_local_beep(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_genesis_valid(float threshold);
bool yt_config_compose_hq_prompt(float current_hq, float upper_bound,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_hq_diagnostic(enum yt_config_hq_diagnostic diagnostic,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_hq_in_range(float candidate, float upper_bound);
bool yt_config_compose_scoreboard_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_scoreboard_too_long(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_scalar_prompt(enum yt_config_scalar_key key,
    float working_maximum, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_scalar_rejection(enum yt_config_scalar_key key,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_scalar_blank_unchanged(enum yt_config_scalar_key key);
bool yt_config_scalar_valid(enum yt_config_scalar_key key, float value);
bool yt_config_compose_planet_entry(unsigned active_count,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_planet_menu(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_key_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result);
bool yt_config_compose_planet_number_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_response_echo(uint8_t key,
    size_t initial_column, uint8_t *folded,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_list_header(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_list_row(int logical, const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_pause(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_blank(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_invalid(const uint8_t *entered,
    size_t entered_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_protected(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_edit(const uint8_t *name, size_t name_length,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_planet_confirmation(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_planet_cancel(const uint8_t *name, size_t name_length,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_planet_saved(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_planet_selection_in_range(float selection);
bool yt_config_planet_selection_protected(float selection);
bool yt_config_planet_pause_after(int logical, unsigned active_count);
bool yt_config_compose_port_search_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_search_echo(const uint8_t *search,
    size_t search_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_match_prompt(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_response_echo(uint8_t key,
    size_t initial_column, uint8_t *folded,
    struct yt_config_output_result *result);
bool yt_config_compose_port_not_found(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_end_list(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_wait_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_replacement_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_confirmation(const uint8_t *name,
    size_t name_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_cancel(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_port_saved(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_entry(unsigned player_count,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_alias_menu(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_key_echo(uint8_t key, size_t initial_column,
    uint8_t *folded, struct yt_config_output_result *result);
bool yt_config_compose_alias_list_header(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_list_row(int logical,
    const uint8_t *real_first, size_t real_first_length,
    const uint8_t *real_last, size_t real_last_length,
    const uint8_t *alias_first, size_t alias_first_length,
    const uint8_t *alias_last, size_t alias_last_length,
    size_t initial_column, struct yt_config_output_result *result);
bool yt_config_compose_alias_pause(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_blank(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_number_prompt(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_invalid(const uint8_t *entered,
    size_t entered_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_edit(const uint8_t *real_first,
    size_t real_first_length, const uint8_t *real_last,
    size_t real_last_length, const uint8_t *alias_first,
    size_t alias_first_length, const uint8_t *alias_last,
    size_t alias_last_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_confirmation(const uint8_t *alias,
    size_t alias_length, size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_response_echo(uint8_t key,
    size_t initial_column, uint8_t *folded,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_cancel(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_compose_alias_saved(size_t initial_column,
    struct yt_config_output_result *result);
bool yt_config_alias_selection_in_range(float selection,
    unsigned player_count);
bool yt_config_alias_pause_after(int logical, unsigned player_count);

#endif
