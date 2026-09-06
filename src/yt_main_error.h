#ifndef YT_MAIN_ERROR_H
#define YT_MAIN_ERROR_H

#include "yt_common.h"

#define YT_MAIN_ERROR_TEXT 1024U

enum yt_basic_fault_module {
	YT_BASIC_FAULT_MAIN,
	YT_BASIC_FAULT_SHARED,
};

enum yt_basic_fault_site {
	YT_BASIC_FAULT_PORT_SELECTED_SECTOR_GET,
	YT_BASIC_FAULT_PORT_FRIENDSHIP_CURRENT_GET,
	YT_BASIC_FAULT_PORT_FRIENDSHIP_CANDIDATE_GET,
	YT_BASIC_FAULT_PORT_UPDATER_SECTOR_GET,
	YT_BASIC_FAULT_PORT_UPDATER_PORT_GET,
	YT_BASIC_FAULT_PORT_UPDATER_PORT_PUT,
	YT_BASIC_FAULT_PORT_OWNER_PLAYER_GET,
	YT_BASIC_FAULT_CURRENT_PLAYER_A41C_GET,
	YT_BASIC_FAULT_CURRENT_PLAYER_A41C_SECTOR_ADD,
	YT_BASIC_FAULT_CURRENT_PLAYER_A41C_ANTI_CLOAK_CINT,
	YT_BASIC_FAULT_CURRENT_PLAYER_A41C_PLAYER_INDEX_CINT,
	YT_BASIC_FAULT_PORT_REPORT_PORT_GET,
	YT_BASIC_FAULT_PORT_EARTH_GET,
	YT_BASIC_FAULT_ROUTE_START_FIFO_CINT,
	YT_BASIC_FAULT_ROUTE_START_PREDECESSOR_CINT,
	YT_BASIC_FAULT_ROUTE_AVOID_PREDECESSOR_CINT,
	YT_BASIC_FAULT_ROUTE_AVOID_ENDPOINT_CINT,
	YT_BASIC_FAULT_ROUTE_FIFO_HEAD_CINT,
	YT_BASIC_FAULT_ROUTE_FIFO_NODE_CINT,
	YT_BASIC_FAULT_ROUTE_DESTINATION_PREDECESSOR_CINT,
	YT_BASIC_FAULT_ROUTE_EXPANDED_NODE_CINT,
	YT_BASIC_FAULT_ROUTE_SECTOR_GET,
	YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_CHILD_CINT,
	YT_BASIC_FAULT_ROUTE_RECONSTRUCTION_PARENT_CINT,
	YT_BASIC_FAULT_ROUTE_NEXT_HOP_CINT,
	YT_BASIC_FAULT_ROUTE_DISPLAY_VERTEX_CINT,
	YT_BASIC_FAULT_ROUTE_PROGRAM_VERTEX_CINT,
	YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET,
	YT_BASIC_FAULT_NORMAL_EXIT_REGISTERED_CINT,
	YT_BASIC_FAULT_RETURNING_DAILY_GET,
	YT_BASIC_FAULT_RETURNING_DAILY_PUT,
	YT_BASIC_FAULT_RETURNING_KILLER_GET,
	YT_BASIC_FAULT_RETURNING_KILLER_CINT,
	YT_BASIC_FAULT_RETURNING_KILLER_LEFT,
	YT_BASIC_FAULT_CONSTRUCTOR_CONFIG_GET,
	YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_GET,
	YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_PUT,
	YT_BASIC_FAULT_IDENTITY_PLAYER_GET,
	YT_BASIC_FAULT_IDENTITY_PLAYER_PUT,
	YT_BASIC_FAULT_POST_LOGIN_SECTOR_PUT,
	YT_BASIC_FAULT_POST_LOGIN_CARGO_PUT,
	YT_BASIC_FAULT_GENESIS_OPEN_OUTPUT,
	YT_BASIC_FAULT_GENESIS_COMMAND_MATERIALIZE,
	YT_BASIC_FAULT_GENESIS_PRINT_VALUE,
	YT_BASIC_FAULT_GENESIS_PRINT_COMPLETION,
	YT_BASIC_FAULT_GENESIS_CLOSE_ALL,
	YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW,
	YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW,
	YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE,
	YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE,
	YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE,
	YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE,
	YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK,
	YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE,
	YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK,
	YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE,
	YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE,
	YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE,
	YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE,
	YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE,
	YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE,
	YT_BASIC_FAULT_UPPER_FRAME_STACK,
	YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE,
	YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE,
	YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE,
	YT_BASIC_FAULT_SITE_COUNT,
};

#define YT_BASIC_FAULT_ERRORS 7U

struct yt_basic_fault_identity {
	const char *name;
	enum yt_basic_fault_module module;
	uint16_t instruction;
	uint16_t saved_ip;
	uint16_t retry_statement;
	int32_t source_line;
	uint16_t handler;
	uint8_t errors[YT_BASIC_FAULT_ERRORS];
	size_t error_count;
};

const struct yt_basic_fault_identity *yt_basic_fault_identity(
	enum yt_basic_fault_site site);
bool yt_basic_fault_admits(enum yt_basic_fault_site site,
	uint8_t error_number);
bool yt_error_attach_basic_fault(struct yt_error *error,
	enum yt_basic_fault_site site);
bool yt_error_attach_basic_fault_number(struct yt_error *error,
	enum yt_basic_fault_site site, uint16_t error_number);

enum yt_basic_fault_disposition {
	YT_BASIC_FAULT_RETRY_STATEMENT,
	YT_BASIC_FAULT_RESUME_MISSING_FILE,
	YT_BASIC_FAULT_RESUME_GAMEPLAY,
	YT_BASIC_FAULT_END,
};

enum yt_main_error_route {
	YT_MAIN_ERROR_RETRY_CURRENT,
	YT_MAIN_ERROR_MISSING_FILE,
	YT_MAIN_ERROR_GAMEPLAY,
	YT_MAIN_ERROR_FATAL,
};

struct yt_main_error_result {
	enum yt_main_error_route route;
	uint8_t debug[YT_MAIN_ERROR_TEXT];
	size_t debug_length;
	uint8_t action[YT_MAIN_ERROR_TEXT];
	size_t action_length;
};

typedef bool (*yt_main_error_present_fn)(void *context,
    const uint8_t *text, size_t length, struct yt_error *error);

enum yt_shared_error_route {
	YT_SHARED_ERROR_RETRY_CURRENT,
	YT_SHARED_ERROR_AUTOPILOT_MEMORY,
	YT_SHARED_ERROR_DATA_OPEN,
	YT_SHARED_ERROR_ANSI_OPEN,
	YT_SHARED_ERROR_RANKINGS_FILESPEC,
	YT_SHARED_ERROR_DORINFO_COM,
	YT_SHARED_ERROR_ALIAS_FILE,
	YT_SHARED_ERROR_GENERIC,
};

enum yt_shared_error_destination {
	YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
	YT_SHARED_ERROR_SESSION_AND_NEWS,
	YT_SHARED_ERROR_NEWS,
};

#define YT_SHARED_ERROR_EVENTS 3U

struct yt_shared_error_event {
	enum yt_shared_error_destination destination;
	uint8_t data[YT_MAIN_ERROR_TEXT];
	size_t length;
};

struct yt_shared_error_result {
	enum yt_shared_error_route route;
	uint8_t debug[YT_MAIN_ERROR_TEXT];
	size_t debug_length;
	struct yt_shared_error_event events[YT_SHARED_ERROR_EVENTS];
	size_t event_count;
	bool ends;
};

struct yt_basic_fault_projection {
	enum yt_basic_fault_site site;
	uint16_t error_number;
	const struct yt_basic_fault_identity *identity;
	enum yt_basic_fault_disposition disposition;
	struct yt_main_error_result main;
	struct yt_shared_error_result shared;
};

bool yt_basic_fault_project(const struct yt_error *error,
	const uint8_t *pathname, size_t pathname_length,
	const uint8_t *date_text, size_t date_length,
	const uint8_t *time_text, size_t time_length,
	struct yt_basic_fault_projection *projection);

bool yt_main_error_compose(int16_t error_number, int32_t source_line,
    const uint8_t *pathname, size_t pathname_length,
    const uint8_t *date_text, size_t date_length,
    const uint8_t *time_text, size_t time_length,
    struct yt_main_error_result *result);

bool yt_main_error_commit_fatal_to(const char *path,
    const struct yt_main_error_result *result,
    yt_main_error_present_fn present, void *present_context,
    struct yt_error *error);
bool yt_main_error_commit_fatal(const struct yt_main_error_result *result,
    yt_main_error_present_fn present, void *present_context,
    struct yt_error *error);

bool yt_shared_error_compose(int16_t error_number, int32_t source_line,
    struct yt_shared_error_result *result);

#endif
