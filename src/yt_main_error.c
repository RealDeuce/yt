#include "yt_main_error.h"

#include "qb.h"

#include <string.h>

struct text_builder {
	uint8_t *data;
	size_t capacity;
	size_t length;
};

static bool
append(struct text_builder *builder, const void *data, size_t length)
{
	if (length > builder->capacity - builder->length)
		return false;
	if (length != 0U)
		memcpy(builder->data + builder->length, data, length);
	builder->length += length;
	return true;
}

static bool
append_literal(struct text_builder *builder, const char *literal)
{
	return append(builder, literal, strlen(literal));
}

static bool
append_str_single(struct text_builder *builder, float value, bool print)
{
	char rendered[64];
	int length = print
	    ? qb_print_single(rendered, sizeof(rendered), value)
	    : qb_str_single(rendered, sizeof(rendered), value);

	return length >= 0
	    && append(builder, rendered, (size_t)length);
}

static bool
append_print_integer(struct text_builder *builder, int16_t value)
{
	char rendered[16];
	int length = qb_print_integer(rendered, sizeof(rendered), value);

	return length >= 0
	    && append(builder, rendered, (size_t)length);
}

static bool
append_str_integer(struct text_builder *builder, int16_t value)
{
	char rendered[16];
	int length = qb_str_integer(rendered, sizeof(rendered), value);

	return length >= 0
	    && append(builder, rendered, (size_t)length);
}

static bool
compose_debug(int16_t error_number, int32_t source_line,
    struct yt_main_error_result *result)
{
	struct text_builder builder = {
		result->debug, sizeof(result->debug), 0U
	};

	if (!append_literal(&builder, "YT DEBUG Error Trap Entry ERL= ")
	    || !append_str_single(&builder, (float)source_line, true)
	    || !append_literal(&builder, "  ERR= ")
	    || !append_print_integer(&builder, error_number))
		return false;
	result->debug_length = builder.length;
	return true;
}

static bool
compose_missing_file(const uint8_t *pathname, size_t pathname_length,
    struct yt_main_error_result *result)
{
	struct text_builder builder = {
		result->action, sizeof(result->action), 0U
	};

	if ((pathname == NULL && pathname_length != 0U)
	    || !append_literal(&builder, "*** GAME FILE [")
	    || !append(&builder, pathname, pathname_length)
	    || !append_literal(&builder, "] NOT FOUND! ***"))
		return false;
	result->action_length = builder.length;
	return true;
}

static bool
compose_fatal(int16_t error_number, int32_t source_line,
    const uint8_t *date_text, size_t date_length,
    const uint8_t *time_text, size_t time_length,
    struct yt_main_error_result *result)
{
	struct text_builder builder = {
		result->action, sizeof(result->action), 0U
	};

	if ((date_text == NULL && date_length != 0U)
	    || (time_text == NULL && time_length != 0U)
	    || !append_literal(&builder,
	    "YTMerg2 1.15 Untrapped Error ERL=")
	    || !append_str_single(&builder, (float)source_line, false)
	    || !append_literal(&builder, " ERR=")
	    || !append_str_single(&builder, (float)error_number, false)
	    || !append_literal(&builder, " Date >")
	    || !append(&builder, date_text, date_length)
	    || !append_literal(&builder, " ")
	    || !append(&builder, time_text, time_length))
		return false;
	result->action_length = builder.length;
	return true;
}

bool
yt_main_error_compose(int16_t error_number, int32_t source_line,
    const uint8_t *pathname, size_t pathname_length,
    const uint8_t *date_text, size_t date_length,
    const uint8_t *time_text, size_t time_length,
    struct yt_main_error_result *result)
{
	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	if (error_number == 24 || error_number == 57) {
		result->route = YT_MAIN_ERROR_RETRY_CURRENT;
		return true;
	}
	if (!compose_debug(error_number, source_line, result))
		return false;
	if (source_line == 40000) {
		result->route = YT_MAIN_ERROR_MISSING_FILE;
		return compose_missing_file(pathname, pathname_length, result);
	}
	if (error_number == 5 || error_number == 6
	    || error_number == 13 || error_number == 15) {
		result->route = YT_MAIN_ERROR_GAMEPLAY;
		return true;
	}
	result->route = YT_MAIN_ERROR_FATAL;
	return compose_fatal(error_number, source_line, date_text, date_length,
	    time_text, time_length, result);
}

static bool
compose_shared_debug(int16_t error_number, int32_t source_line,
    struct yt_shared_error_result *result)
{
	struct text_builder builder = {
		result->debug, sizeof(result->debug), 0U
	};

	if (!append_literal(&builder,
	    "YT-SUB DEBUG Error Trap Entry ERL= ")
	    || !append_str_single(&builder, (float)source_line, false)
	    || !append_literal(&builder, " ERR=")
	    || !append_print_integer(&builder, error_number))
		return false;
	result->debug_length = builder.length;
	return true;
}

static bool
add_shared_literal(struct yt_shared_error_result *result,
    enum yt_shared_error_destination destination, const char *literal)
{
	struct yt_shared_error_event *event;
	size_t length = strlen(literal);

	if (result->event_count >= YT_SHARED_ERROR_EVENTS
	    || length > YT_MAIN_ERROR_TEXT)
		return false;
	event = &result->events[result->event_count++];
	event->destination = destination;
	memcpy(event->data, literal, length);
	event->length = length;
	return true;
}

static bool
add_shared_generic(int16_t error_number, int32_t source_line,
    struct yt_shared_error_result *result)
{
	struct yt_shared_error_event *event;
	struct text_builder builder;

	if (result->event_count >= YT_SHARED_ERROR_EVENTS)
		return false;
	event = &result->events[result->event_count++];
	event->destination = YT_SHARED_ERROR_NEWS;
	builder = (struct text_builder){
		event->data, sizeof(event->data), 0U
	};
	if (!append_literal(&builder, "Untrapped YT-SUB Error> ")
	    || !append_str_integer(&builder, error_number)
	    || !append_literal(&builder, " Line> ")
	    || !append_str_single(&builder, (float)source_line, false))
		return false;
	event->length = builder.length;
	return true;
}

bool
yt_shared_error_compose(int16_t error_number, int32_t source_line,
    struct yt_shared_error_result *result)
{
	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	if (error_number == 24) {
		result->route = YT_SHARED_ERROR_RETRY_CURRENT;
		return true;
	}
	if (!compose_shared_debug(error_number, source_line, result))
		return false;
	result->ends = true;
	if (source_line == 38100) {
		result->route = YT_SHARED_ERROR_AUTOPILOT_MEMORY;
		return add_shared_literal(result,
		    YT_SHARED_ERROR_SESSION_AND_NEWS,
		    " *** Not Enough System Memory for Autopilot Function! ***");
	}
	if (source_line == 630) {
		result->route = YT_SHARED_ERROR_DATA_OPEN;
		return add_shared_literal(result,
		    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
		    "Error opening ytDATA.DAT");
	}
	if (source_line == 2710) {
		result->route = YT_SHARED_ERROR_ANSI_OPEN;
		return add_shared_literal(result,
		    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
		    "Please Create YTOPEN.ANS for ANSI graphics users!");
	}
	if (source_line == 64001) {
		result->route = YT_SHARED_ERROR_RANKINGS_FILESPEC;
		return add_shared_literal(result,
		    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
		    "Error with Player Rankings filespec");
	}
	if (source_line == 64006) {
		result->route = YT_SHARED_ERROR_DORINFO_COM;
		if (!add_shared_literal(result,
		    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
		    "Error with DORINFO file!"))
			return false;
		if (error_number == 53)
			return add_shared_literal(result,
			    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
			    "DORINFO not found!");
		if (error_number == 64)
			return add_shared_literal(result,
			    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
			    "Error opening the COM Port!");
		return true;
	}
	if (source_line > 64003 && source_line < 64006) {
		result->route = YT_SHARED_ERROR_ALIAS_FILE;
		return add_shared_literal(result,
		    YT_SHARED_ERROR_LOCAL_DIAGNOSTIC,
		    "Error with YTNAME.DAT file!");
	}
	result->route = YT_SHARED_ERROR_GENERIC;
	return add_shared_generic(error_number, source_line, result)
	    && add_shared_literal(result, YT_SHARED_ERROR_NEWS,
	    "Please record error and circumstances. Also, if the error")
	    && add_shared_literal(result, YT_SHARED_ERROR_NEWS,
	    "is Severe, Please inform Alan Davenport!");
}
