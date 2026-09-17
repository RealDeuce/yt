#include "yt_main_error.h"

#include "qb.h"
#include "yt_text.h"

#include <string.h>

#define GET_ERRORS {5U, 52U, 57U, 63U, 70U, 75U}
#define PUT_ERRORS {5U, 52U, 57U, 61U, 63U, 70U, 75U}
#define RETURNING_GET_ERRORS {52U, 57U, 70U, 75U}
#define RETURNING_PUT_ERRORS {52U, 57U, 61U, 70U, 75U}
#define LEFT_ERRORS {5U, 14U, 16U}
#define CINT_ERRORS {6U}
#define SPACE_ERRORS {14U}
#define STACK_ERRORS {7U}
#define GENESIS_OPEN_ERRORS {14U, 53U, 57U, 67U, 70U, 75U, 76U}
#define GENESIS_COMMAND_ERRORS {14U, 16U}
#define A8D2_LEFT_ERRORS {14U, 16U}
#define GENESIS_PRINT_ERRORS {52U, 57U, 61U, 70U, 71U}
#define GENESIS_COMPLETION_ERRORS {57U}
#define GENESIS_CLOSE_ERRORS {57U, 61U, 70U}
#define RADIO_OPEN_ERRORS {5U, 52U, 53U, 55U, 57U, 67U, 68U, 70U, 71U, 72U, 75U}
#define RADIO_LOF_ERRORS {52U, 57U}
#define RADIO_CLOSE_ERRORS {52U, 57U, 70U}
#define DELEGATED_ERRORS {0U}
#define MAIN_FAULT(label, op, saved, statement, erl, domain, count) \
	{label, YT_BASIC_FAULT_MAIN, op, saved, statement, erl, 0xB2DAU, \
	    domain, count}
#define SHARED_FAULT(label, op, saved, statement, erl, domain, count) \
	{label, YT_BASIC_FAULT_SHARED, op, saved, statement, erl, 0x45F7U, \
	    domain, count}

static const struct yt_basic_fault_identity basic_faults[] = {
	MAIN_FAULT("selected-sector GET", 0x8E10U, 0x8E13U, 0x8DFCU,
	    33780, GET_ERRORS, 6U),
	SHARED_FAULT("friendship current-player GET", 0x97C6U, 0x97C9U,
	    0x97BBU, 64006, GET_ERRORS, 6U),
	SHARED_FAULT("friendship candidate-player GET", 0x97EEU, 0x97F1U,
	    0x97E3U, 64006, GET_ERRORS, 6U),
	MAIN_FAULT("ordinary updater sector GET", 0x67D8U, 0x67DBU,
	    0x67CDU, 33000, GET_ERRORS, 6U),
	MAIN_FAULT("ordinary updater port GET", 0x680BU, 0x680EU,
	    0x6800U, 33000, GET_ERRORS, 6U),
	MAIN_FAULT("ordinary updater port PUT", 0x6AE6U, 0x6AE9U,
	    0x6ADBU, 33000, PUT_ERRORS, 7U),
	MAIN_FAULT("owner-player GET", 0xA9A2U, 0xA9A5U, 0xA997U,
	    40001, GET_ERRORS, 6U),
	MAIN_FAULT("current-player A41C GET", 0xA428U, 0xA42BU, 0xA41DU,
	    33990, GET_ERRORS, 6U),
	MAIN_FAULT("current-player A41C player-index CINT", 0xA557U,
	    0xA55AU, 0xA554U, 33990, CINT_ERRORS, 1U),
	MAIN_FAULT("hostile Attack opening sector GET", 0x0BC0U, 0x0BC3U,
	    0x0BB5U, 20430, RETURNING_GET_ERRORS, 4U),
	MAIN_FAULT("ordinary report port GET", 0x6B0BU, 0x6B0EU,
	    0x6B00U, 33100, GET_ERRORS, 6U),
	MAIN_FAULT("Earth port GET", 0x6CDBU, 0x6CDEU, 0x6CC7U,
	    33150, GET_ERRORS, 6U),
	SHARED_FAULT("route start FIFO index CINT", 0x103FU, 0x1042U,
	    0x103CU, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route start predecessor index CINT", 0x1050U,
	    0x1053U, 0x104DU, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("avoid predecessor index CINT", 0x1067U, 0x106AU,
	    0x1064U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("avoid endpoint index CINT", 0x1072U, 0x1075U,
	    0x106FU, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route FIFO head index CINT", 0x1099U, 0x109CU,
	    0x1096U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route FIFO node CINT", 0x10A6U, 0x10A9U,
	    0x1096U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route destination predecessor CINT", 0x10B4U,
	    0x10B7U, 0x10B1U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route expanded-node CINT", 0x1105U, 0x1108U,
	    0x1102U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route sector GET", 0x1156U, 0x1159U, 0x113EU,
	    0, GET_ERRORS, 6U),
	SHARED_FAULT("route reconstruction child CINT", 0x134CU,
	    0x134FU, 0x1349U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route reconstruction parent CINT", 0x136CU,
	    0x136FU, 0x1369U, 0, CINT_ERRORS, 1U),
	SHARED_FAULT("route next-hop index CINT", 0x13ADU, 0x13B0U,
	    0x13AAU, 0, CINT_ERRORS, 1U),
	MAIN_FAULT("route display vertex CINT", 0x90E9U, 0x90ECU,
	    0x90E1U, 33880, CINT_ERRORS, 1U),
	MAIN_FAULT("course-program vertex CINT", 0x9123U, 0x9126U,
	    0x9120U, 33880, CINT_ERRORS, 1U),
	MAIN_FAULT("autopilot final-sector GET", 0x92E1U, 0x92E4U,
	    0x92CCU, 33890, GET_ERRORS, 6U),
	SHARED_FAULT("returning daily player GET", 0x1584U, 0x1587U,
	    0x1579U, 0, RETURNING_GET_ERRORS, 4U),
	SHARED_FAULT("returning daily player PUT", 0x163CU, 0x163FU,
	    0x1631U, 0, RETURNING_PUT_ERRORS, 5U),
	SHARED_FAULT("returning killer player GET", 0x17E7U, 0x17EAU,
	    0x17DFU, 0, RETURNING_GET_ERRORS, 4U),
	SHARED_FAULT("constructor configuration GET", 0x35A2U, 0x35A5U,
	    0x359CU, 35450, RETURNING_GET_ERRORS, 4U),
	SHARED_FAULT("constructor player GET", 0x35D4U, 0x35D7U,
	    0x35C9U, 35450, RETURNING_GET_ERRORS, 4U),
	SHARED_FAULT("constructor player PUT", 0x36DEU, 0x36E1U,
	    0x36D3U, 35450, RETURNING_PUT_ERRORS, 5U),
	MAIN_FAULT("new-player identity GET", 0x064BU, 0x064EU, 0x0640U,
	    11120, RETURNING_GET_ERRORS, 4U),
	MAIN_FAULT("new-player identity PUT", 0x0683U, 0x0686U, 0x0678U,
	    11120, RETURNING_PUT_ERRORS, 5U),
	MAIN_FAULT("post-login sector repair PUT", 0x0764U, 0x0767U,
	    0x0759U, 19000, RETURNING_PUT_ERRORS, 5U),
	MAIN_FAULT("post-login cargo repair PUT", 0x07B9U, 0x07BCU,
	    0x07AEU, 19100, RETURNING_PUT_ERRORS, 5U),
	MAIN_FAULT("Genesis OUTPUT open", 0x283DU, 0x2840U, 0x282FU,
	    24950, GENESIS_OPEN_ERRORS, 7U),
	MAIN_FAULT("Genesis COMMAND$ materialization", 0x2846U, 0x2849U,
	    0x2840U, 24950, GENESIS_COMMAND_ERRORS, 2U),
	MAIN_FAULT("Genesis PRINT value", 0x2849U, 0x284CU, 0x2840U,
	    24950, GENESIS_PRINT_ERRORS, 5U),
	MAIN_FAULT("Genesis PRINT completion", 0x284CU, 0x284FU, 0x2840U,
	    24950, GENESIS_COMPLETION_ERRORS, 1U),
	MAIN_FAULT("Genesis CLOSE all", 0x284FU, 0x2852U, 0x284FU,
	    24950, GENESIS_CLOSE_ERRORS, 3U),
	MAIN_FAULT("ADE0 repeat VAL overflow", 0xAEBFU, 0xAEC2U,
	    0xAE9CU, 36000, CINT_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat SINGLE overflow", 0xAEC8U, 0xAECBU,
	    0xAE9CU, 36000, CINT_ERRORS, 1U),
	MAIN_FAULT("ADE0 slash-test RIGHT$ space", 0xADE7U, 0xADEAU,
	    0xADE1U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 save-strip LEFT$ space", 0xAE04U, 0xAE07U,
	    0xADF7U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 save-command clone space", 0xAE1BU, 0xAE1EU,
	    0xAE15U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 save-notice clone space", 0xAE24U, 0xAE27U,
	    0xAE1EU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 save-notice GOSUB stack", 0xAE27U, 0xAE2AU,
	    0xAE27U, 36000, STACK_ERRORS, 1U),
	MAIN_FAULT("ADE0 uppercase-scratch clone space", 0xAE4AU, 0xAE4DU,
	    0xAE44U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-prefix LEFT$ space", 0xAE8CU, 0xAE8FU,
	    0xAE79U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-semicolon CONCAT space", 0xAE94U, 0xAE97U,
	    0xAE79U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-suffix RIGHT$ space", 0xAEBCU, 0xAEBFU,
	    0xAE9CU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-build CONCAT space", 0xAF2BU, 0xAF2EU,
	    0xAF23U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-final LEFT$ space", 0xAF56U, 0xAF59U,
	    0xAF4BU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-save clone space", 0xAF6EU, 0xAF71U,
	    0xAF68U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-count STR$ space", 0xAF7DU, 0xAF80U,
	    0xAF7AU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-prefix CONCAT space", 0xAF83U, 0xAF86U,
	    0xAF7AU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-notice CONCAT space", 0xAF8BU, 0xAF8EU,
	    0xAF7AU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 repeat-notice GOSUB stack", 0xAF94U, 0xAF97U,
	    0xAF94U, 36000, STACK_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-tail MID$ space", 0xAFE5U, 0xAFE8U,
	    0xAFD1U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-queue CONCAT space", 0xAFEBU, 0xAFEEU,
	    0xAFD1U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-prefix LEFT$ space", 0xB006U, 0xB009U,
	    0xAFF3U, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-replacement CHR$ space", 0xB021U,
	    0xB024U, 0xB01EU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-final CR CHR$ space", 0xB041U,
	    0xB044U, 0xB03EU, 36000, SPACE_ERRORS, 1U),
	MAIN_FAULT("ADE0 semicolon-final CONCAT space", 0xB047U, 0xB04AU,
	    0xB03EU, 36000, SPACE_ERRORS, 1U),
	SHARED_FAULT("uppercase helper frame stack", 0x1D99U, 0x1D9CU,
	    0x1D96U, 610, STACK_ERRORS, 1U),
	SHARED_FAULT("uppercase helper compare MID$ space", 0x1DC2U,
	    0x1DC5U, 0x1DB4U, 610, SPACE_ERRORS, 1U),
	SHARED_FAULT("uppercase helper value MID$ space", 0x1DE0U,
	    0x1DE3U, 0x1DD2U, 610, SPACE_ERRORS, 1U),
	SHARED_FAULT("uppercase helper CHR$ space", 0x1DEAU, 0x1DEDU,
	    0x1DD2U, 610, SPACE_ERRORS, 1U),
	MAIN_FAULT("A8D2 LEFT$ first byte", 0xA8EFU, 0xA8F2U,
	    0xA8E7U, 40001, A8D2_LEFT_ERRORS, 2U),
	MAIN_FAULT("action-finalizer player-index CINT", 0xA773U,
	    0xA776U, 0xA770U, 40001, CINT_ERRORS, 1U),
	SHARED_FAULT("radio OPEN", 0x2071U, 0x2074U, 0x2062U, 633,
	    RADIO_OPEN_ERRORS, 11U),
	SHARED_FAULT("radio LOF", 0x268FU, 0x2692U, 0x268CU, 6200,
	    RADIO_LOF_ERRORS, 2U),
	SHARED_FAULT("radio record GET", 0x26BBU, 0x26BEU, 0x26B0U, 6200,
	    GET_ERRORS, 6U),
	SHARED_FAULT("radio recipient player GET", 0x274DU, 0x2750U,
	    0x273CU, 6200, GET_ERRORS, 6U),
	SHARED_FAULT("radio sender player GET", 0x27C1U, 0x27C4U,
	    0x27B0U, 6200, GET_ERRORS, 6U),
	SHARED_FAULT("radio final CLOSE", 0x2987U, 0x298AU, 0x2984U, 6200,
	    RADIO_CLOSE_ERRORS, 3U),
	SHARED_FAULT("radio opening direct output", 0x2645U, 0x264AU,
	    0x2638U, 6200, DELEGATED_ERRORS, 0U),
	SHARED_FAULT("radio log-heading direct output", 0x2664U, 0x2669U,
	    0x2657U, 6200, DELEGATED_ERRORS, 0U),
	SHARED_FAULT("radio automatic-heading direct output", 0x2679U,
	    0x267EU, 0x266CU, 6200, DELEGATED_ERRORS, 0U),
	SHARED_FAULT("radio pause direct output", 0x28BEU, 0x28C3U,
	    0x28B1U, 6200, DELEGATED_ERRORS, 0U),
	SHARED_FAULT("radio private wait", 0x28D3U, 0x28D8U, 0x28C3U,
	    6200, DELEGATED_ERRORS, 0U),
};

_Static_assert(YT_ARRAY_LEN(basic_faults) == YT_BASIC_FAULT_SITE_COUNT,
    "basic fault identity table is incomplete");

#undef SHARED_FAULT
#undef MAIN_FAULT
#undef LEFT_ERRORS
#undef CINT_ERRORS
#undef SPACE_ERRORS
#undef STACK_ERRORS
#undef DELEGATED_ERRORS
#undef RADIO_CLOSE_ERRORS
#undef RADIO_LOF_ERRORS
#undef RADIO_OPEN_ERRORS
#undef GENESIS_CLOSE_ERRORS
#undef GENESIS_COMPLETION_ERRORS
#undef GENESIS_PRINT_ERRORS
#undef GENESIS_COMMAND_ERRORS
#undef A8D2_LEFT_ERRORS
#undef GENESIS_OPEN_ERRORS
#undef RETURNING_PUT_ERRORS
#undef RETURNING_GET_ERRORS
#undef PUT_ERRORS
#undef GET_ERRORS

const struct yt_basic_fault_identity *
yt_basic_fault_identity(enum yt_basic_fault_site site)
{
	if ((unsigned)site >= YT_ARRAY_LEN(basic_faults))
		return NULL;
	return &basic_faults[(size_t)site];
}

bool
yt_basic_fault_admits(enum yt_basic_fault_site site, uint8_t error_number)
{
	const struct yt_basic_fault_identity *identity =
	    yt_basic_fault_identity(site);
	size_t index;

	if (identity == NULL)
		return false;
	if (identity->error_count == 0U)
		return true;
	for (index = 0U; index < identity->error_count; ++index)
		if (identity->errors[index] == error_number)
			return true;
	return false;
}

bool
yt_error_attach_basic_fault(struct yt_error *error,
    enum yt_basic_fault_site site)
{
	if (error == NULL || yt_basic_fault_identity(site) == NULL)
		return false;
	error->basic_fault_site = (uint16_t)site;
	error->basic_error = 0U;
	error->basic_fault_valid = true;
	error->basic_error_valid = false;
	return true;
}

bool
yt_error_attach_basic_fault_number(struct yt_error *error,
    enum yt_basic_fault_site site, uint16_t error_number)
{
	if (error_number > UINT8_MAX
	    || !yt_basic_fault_admits(site, (uint8_t)error_number)
	    || !yt_error_attach_basic_fault(error, site))
		return false;
	error->basic_error = error_number;
	error->basic_error_valid = true;
	return true;
}

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

static void
set_transaction_error(struct yt_error *error, const char *operation,
    const char *path)
{
	if (error == NULL)
		return;
	error->status = YT_INVALID;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s", path != NULL ? path : "");
}

bool
yt_main_error_append_fatal_to(const char *path,
    const struct yt_main_error_result *result, struct yt_error *error)
{
	if (path == NULL || result == NULL
	    || result->route != YT_MAIN_ERROR_FATAL
	    || result->action_length > sizeof(result->action)) {
		set_transaction_error(error, "commit fatal main error", path);
		return false;
	}
	return yt_text_append_line(path, result->action, result->action_length,
	    error);
}

bool
yt_main_error_append_fatal(const struct yt_main_error_result *result,
    struct yt_error *error)
{
	return yt_main_error_append_fatal_to("ERRORS.DOR", result, error);
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
		    "Error opening YTDATA.DAT");
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

bool
yt_basic_fault_project(const struct yt_error *error,
    const uint8_t *pathname, size_t pathname_length,
    const uint8_t *date_text, size_t date_length,
    const uint8_t *time_text, size_t time_length,
    struct yt_basic_fault_projection *projection)
{
	const struct yt_basic_fault_identity *identity;
	enum yt_basic_fault_site site;

	if (error == NULL || projection == NULL || !error->basic_fault_valid
	    || !error->basic_error_valid
	    || error->basic_fault_site >= YT_BASIC_FAULT_SITE_COUNT
	    || error->basic_error > UINT8_MAX)
		return false;
	site = (enum yt_basic_fault_site)error->basic_fault_site;
	identity = yt_basic_fault_identity(site);
	if (identity == NULL
	    || !yt_basic_fault_admits(site, (uint8_t)error->basic_error))
		return false;
	memset(projection, 0, sizeof(*projection));
	projection->site = site;
	projection->error_number = error->basic_error;
	projection->identity = identity;
	if (identity->module == YT_BASIC_FAULT_MAIN) {
		if (!yt_main_error_compose((int16_t)error->basic_error,
		    identity->source_line, pathname, pathname_length,
		    date_text, date_length, time_text, time_length,
		    &projection->main))
			return false;
		switch (projection->main.route) {
		case YT_MAIN_ERROR_RETRY_CURRENT:
			projection->disposition = YT_BASIC_FAULT_RETRY_STATEMENT;
			break;
		case YT_MAIN_ERROR_MISSING_FILE:
			projection->disposition = YT_BASIC_FAULT_RESUME_MISSING_FILE;
			break;
		case YT_MAIN_ERROR_GAMEPLAY:
			projection->disposition = YT_BASIC_FAULT_RESUME_GAMEPLAY;
			break;
		case YT_MAIN_ERROR_FATAL:
			projection->disposition = YT_BASIC_FAULT_END;
			break;
		}
		return true;
	}
	if (!yt_shared_error_compose((int16_t)error->basic_error,
	    identity->source_line, &projection->shared))
		return false;
	projection->disposition = projection->shared.route
	    == YT_SHARED_ERROR_RETRY_CURRENT
	    ? YT_BASIC_FAULT_RETRY_STATEMENT : YT_BASIC_FAULT_END;
	return true;
}
