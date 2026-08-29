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

#endif
