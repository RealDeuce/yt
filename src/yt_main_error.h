#ifndef YT_MAIN_ERROR_H
#define YT_MAIN_ERROR_H

#include "yt_common.h"

#define YT_MAIN_ERROR_TEXT 1024U

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

bool yt_main_error_compose(int16_t error_number, int32_t source_line,
    const uint8_t *pathname, size_t pathname_length,
    const uint8_t *date_text, size_t date_length,
    const uint8_t *time_text, size_t time_length,
    struct yt_main_error_result *result);

bool yt_shared_error_compose(int16_t error_number, int32_t source_line,
    struct yt_shared_error_result *result);

#endif
