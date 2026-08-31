#include "yt_init.h"

#include "qb.h"
#include "yt_names.h"
#include "yt_startup_model.h"
#include "yt_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define YT_INIT_PLAYERS 50
#define YT_INIT_SECTORS 2004
#define YT_INIT_PORTS 1000
#define YT_INIT_PLANETS 100
#define YT_NAME_TOKENS 908

static const uint8_t raw_zero_residue[4] = {0x00, 0x00, 0xa0, 0x00};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
static const char port_name_blob[] =
#include "yt_portnames.inc"
;
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

struct world {
	int sectors;
	int ports;
	int (*warps)[6];
	int *port_sectors;
	int *sector_ports;
};

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static bool
yt_init_present_one(const struct yt_init_presenter *presenter, uint16_t site,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (presenter == NULL)
		return true;
	if (presenter->write == NULL
	    || !presenter->write(presenter->context, site, entry, payload,
	    payload_length, error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "present YT-INIT site %04X", site);
			error->path[0] = '\0';
		}
		return false;
	}
	return true;
}

static bool
yt_init_present_text(const struct yt_init_presenter *presenter, uint16_t site,
    enum yt_init_output_entry entry, const char *text, struct yt_error *error)
{
	return yt_init_present_one(presenter, site, entry,
	    (const uint8_t *)text, strlen(text), error);
}

static bool
yt_present(const struct yt_initializer_options *options, uint16_t site,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (options->family != YT_INITIALIZER_YT)
		return true;
	return yt_init_present_one(options->yt_presenter, site, entry, payload,
	    payload_length, error);
}

static bool
yt_present_text(const struct yt_initializer_options *options, uint16_t site,
    enum yt_init_output_entry entry, const char *text, struct yt_error *error)
{
	return yt_present(options, site, entry, (const uint8_t *)text,
	    strlen(text), error);
}

static bool
yt_present_number(const struct yt_initializer_options *options,
    uint16_t site, float value, enum yt_init_output_entry entry,
    struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0 || (size_t)length >= sizeof(text)) {
		set_error(error, YT_RANGE, "format YT-INIT number", "");
		return false;
	}
	text[length++] = ' ';
	return yt_present(options, site, entry, (const uint8_t *)text,
	    (size_t)length, error);
}

static bool
yt_present_str_number_line(const struct yt_initializer_options *options,
    uint16_t site, const char *label, float value, struct yt_error *error)
{
	uint8_t payload[192];
	char number[32];
	size_t label_length = strlen(label);
	int number_length = qb_str_single(number, sizeof(number), value);

	if (number_length < 0
	    || label_length + (size_t)number_length > sizeof(payload)) {
		set_error(error, YT_RANGE, "format YT-INIT status number", "");
		return false;
	}
	memcpy(payload, label, label_length);
	memcpy(payload + label_length, number, (size_t)number_length);
	return yt_present(options, site, YT_INIT_OUTPUT_LINE, payload,
	    label_length + (size_t)number_length, error);
}

bool
yt_init_present_confirmation_prefix(const struct yt_init_presenter *presenter,
    struct yt_error *error)
{
	return yt_init_present_text(presenter, 0x060aU, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x061eU, YT_INIT_OUTPUT_LINE,
	    "            Yankee Trader Initialization Program", error)
	    && yt_init_present_text(presenter, 0x0630U, YT_INIT_OUTPUT_LINE,
	    "                     By Alan Davenport", error)
	    && yt_init_present_text(presenter, 0x0641U, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x0653U, YT_INIT_OUTPUT_LINE,
	    "This program will initialize Yankee Trader. You must run this program at",
	    error)
	    && yt_init_present_text(presenter, 0x0665U, YT_INIT_OUTPUT_LINE,
	    "least once when you start up the game. If this program is run on an",
	    error)
	    && yt_init_present_text(presenter, 0x0677U, YT_INIT_OUTPUT_LINE,
	    "existing game, the old game will be wiped out and be replaced by a new one.",
	    error)
	    && yt_init_present_text(presenter, 0x0688U, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x069aU, YT_INIT_OUTPUT_INLINE,
	    "Continue (Y/N)? ", error);
}

bool
yt_init_present_opening(const struct yt_init_presenter *presenter,
    struct yt_error *error)
{
	return yt_init_present_text(presenter, 0x06dcU, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x06eeU, YT_INIT_OUTPUT_LINE,
	    "Creating main data file: YTDATA.DAT", error);
}

bool
yt_rmt_output_compose(enum yt_rmt_output_entry entry,
    const uint8_t *payload, size_t payload_length, bool local_mode,
    uint8_t *local, size_t local_capacity, uint8_t *serial,
    size_t serial_capacity, struct yt_rmt_output_result *result)
{
	const struct yt_rmt_output_state state = {0U, 0U};
	struct yt_rmt_output_state final_state;

	return yt_rmt_output_compose_state(entry, payload, payload_length,
	    local_mode, &state, local, local_capacity, serial, serial_capacity,
	    result, &final_state);
}

enum rmt_punctuation {
	RMT_PUNCTUATION_NEWLINE,
	RMT_PUNCTUATION_SEMICOLON,
	RMT_PUNCTUATION_COMMA
};

static bool
rmt_append_byte(uint8_t *dest, size_t capacity, size_t *length, uint8_t value)
{
	if (*length >= capacity || dest == NULL)
		return false;
	dest[(*length)++] = value;
	return true;
}

static bool
rmt_append_payload(uint8_t *dest, size_t capacity, size_t *length,
    size_t *column, const uint8_t *payload, size_t payload_length)
{
	if (*length > capacity || payload_length > capacity - *length
	    || (payload == NULL && payload_length != 0U)
	    || (dest == NULL && payload_length != 0U))
		return false;
	if (payload_length != 0U)
		memcpy(dest + *length, payload, payload_length);
	*length += payload_length;
	for (size_t index = 0U; index < payload_length; ++index) {
		if (payload[index] == '\r')
			*column = 0U;
		else if (payload[index] >= 0x20U)
			*column = (*column + 1U) % 80U;
	}
	return true;
}

static bool
rmt_render_value(uint8_t *dest, size_t capacity, size_t *length,
    size_t *column, const uint8_t *prefix, size_t prefix_length,
    const uint8_t *payload, size_t payload_length,
    enum rmt_punctuation punctuation)
{
	size_t probe;
	size_t spaces;

	if (prefix_length > SIZE_MAX - payload_length - 1U)
		return false;
	probe = prefix_length + payload_length + 1U;
	if (*column != 0U && (probe > 0xffU || probe > 80U - *column)) {
		if (!rmt_append_byte(dest, capacity, length, '\r'))
			return false;
		*column = 0U;
	}
	if (!rmt_append_payload(dest, capacity, length, column, prefix,
	    prefix_length)
	    || !rmt_append_payload(dest, capacity, length, column, payload,
	    payload_length))
		return false;
	if (punctuation == RMT_PUNCTUATION_NEWLINE) {
		if (!rmt_append_byte(dest, capacity, length, '\r'))
			return false;
		*column = 0U;
	}
	else if (punctuation == RMT_PUNCTUATION_COMMA) {
		spaces = 14U - (*column % 14U);
		if (*column != 0U && spaces + 14U > 80U - *column) {
			if (!rmt_append_byte(dest, capacity, length, '\r'))
				return false;
			*column = 0U;
		}
		else {
			for (size_t index = 0U; index < spaces; ++index) {
				if (!rmt_append_byte(dest, capacity, length, ' '))
					return false;
			}
			*column = (*column + spaces) % 80U;
		}
	}
	return true;
}

bool
yt_rmt_output_compose_state(enum yt_rmt_output_entry entry,
    const uint8_t *payload, size_t payload_length, bool local_mode,
    const struct yt_rmt_output_state *state, uint8_t *local,
    size_t local_capacity, uint8_t *serial, size_t serial_capacity,
    struct yt_rmt_output_result *result,
    struct yt_rmt_output_state *final_state)
{
	static const uint8_t lf[] = {'\n'};
	enum rmt_punctuation punctuation;
	bool serial_only;
	size_t local_length = 0U;
	size_t serial_length = 0U;

	if (state == NULL || result == NULL || final_state == NULL
	    || (payload == NULL && payload_length != 0U)
	    || entry < YT_RMT_OUTPUT_LINE
	    || entry > YT_RMT_OUTPUT_SERIAL_LINE
	    || state->local_column >= 80U || state->serial_column >= 80U
	    || (entry == YT_RMT_OUTPUT_BLANK && payload_length != 0U))
		return false;
	*final_state = *state;
	serial_only = entry == YT_RMT_OUTPUT_SERIAL_LINE;
	punctuation = entry == YT_RMT_OUTPUT_LINE
	    || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE
	    ? RMT_PUNCTUATION_NEWLINE
	    : entry == YT_RMT_OUTPUT_COMMA_SERIAL_FIRST
	    ? RMT_PUNCTUATION_COMMA : RMT_PUNCTUATION_SEMICOLON;
	if (!serial_only
	    && !rmt_render_value(local, local_capacity, &local_length,
	    &final_state->local_column, NULL, 0U, payload, payload_length,
	    punctuation))
		return false;
	if (!local_mode
	    && !rmt_render_value(serial, serial_capacity, &serial_length,
	    &final_state->serial_column,
	    entry == YT_RMT_OUTPUT_LINE || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE ? lf : NULL,
	    entry == YT_RMT_OUTPUT_LINE || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE ? 1U : 0U,
	    payload, payload_length, punctuation))
		return false;
	result->local_length = local_length;
	result->serial_length = serial_length;
	result->serial_first = entry != YT_RMT_OUTPUT_INLINE;
	return true;
}

static bool
rmt_output_apply_one(enum yt_rmt_output_endpoint endpoint,
    const uint8_t *bytes, size_t length, yt_rmt_output_write write,
    const struct yt_rmt_output_sink *sink,
    struct yt_rmt_output_apply_result *result)
{
	struct yt_rmt_output_attempt *attempt;
	bool complete;

	if (length == 0U)
		return true;
	attempt = &result->attempts[result->attempt_count++];
	attempt->endpoint = endpoint;
	attempt->requested = length;
	attempt->accepted = 0U;
	complete = write(sink->context, bytes, length, &attempt->accepted);
	if (attempt->accepted > length)
		return false;
	if (!complete || attempt->accepted != length) {
		result->outcome = endpoint == YT_RMT_OUTPUT_ENDPOINT_SERIAL
		    ? YT_RMT_OUTPUT_APPLY_SERIAL_FAILURE
		    : YT_RMT_OUTPUT_APPLY_LOCAL_FAILURE;
	}
	return true;
}

bool
yt_rmt_output_apply(const uint8_t *local, const uint8_t *serial,
    const struct yt_rmt_output_result *output,
    const struct yt_rmt_output_sink *sink,
    struct yt_rmt_output_apply_result *result)
{
	if (output == NULL || sink == NULL || result == NULL)
		return false;
	if ((local == NULL && output->local_length != 0U)
	    || (serial == NULL && output->serial_length != 0U)
	    || (sink->local == NULL && output->local_length != 0U)
	    || (sink->serial == NULL && output->serial_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	result->outcome = YT_RMT_OUTPUT_APPLY_SUCCESS;
	if (output->serial_first) {
		if (!rmt_output_apply_one(YT_RMT_OUTPUT_ENDPOINT_SERIAL, serial,
		    output->serial_length, sink->serial, sink, result))
			return false;
		if (result->outcome != YT_RMT_OUTPUT_APPLY_SUCCESS)
			return true;
		return rmt_output_apply_one(YT_RMT_OUTPUT_ENDPOINT_LOCAL, local,
		    output->local_length, sink->local, sink, result);
	}
	if (!rmt_output_apply_one(YT_RMT_OUTPUT_ENDPOINT_LOCAL, local,
	    output->local_length, sink->local, sink, result))
		return false;
	if (result->outcome != YT_RMT_OUTPUT_APPLY_SUCCESS)
		return true;
	return rmt_output_apply_one(YT_RMT_OUTPUT_ENDPOINT_SERIAL, serial,
	    output->serial_length, sink->serial, sink, result);
}

static bool
rmt_completion_add(struct yt_rmt_completion_result *result,
    const uint8_t *bytes, size_t length)
{
	struct yt_rmt_completion_line *line;

	if (result->line_count >= YT_RMT_COMPLETION_LINES
	    || length > YT_RMT_COMPLETION_PAYLOAD)
		return false;
	line = &result->lines[result->line_count++];
	if (length != 0U)
		memcpy(line->bytes, bytes, length);
	line->length = length;
	return true;
}

bool
yt_rmt_completion_compose(bool local_mode, const char *credited_name,
    struct yt_rmt_completion_result *result)
{
	static const uint8_t completed[] =
	    "\aInitialization completed sucessfully!\a";
	static const uint8_t returning[] = "Returning you to the BBS...";
	static const char prefix[] = "Congratulations ";
	static const char suffix[] = "! You have fulfilled the prophesy!!";
	uint8_t congratulations[YT_RMT_COMPLETION_PAYLOAD];
	size_t credited_length;
	size_t length;

	if (credited_name == NULL || result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	if (!rmt_completion_add(result, completed, sizeof(completed) - 1U))
		return false;
	if (strcmp(credited_name, "The Sysop") != 0) {
		credited_length = strlen(credited_name);
		length = sizeof(prefix) - 1U + credited_length
		    + sizeof(suffix) - 1U;
		if (length > sizeof(congratulations)
		    || !rmt_completion_add(result, NULL, 0U))
			return false;
		memcpy(congratulations, prefix, sizeof(prefix) - 1U);
		memcpy(congratulations + sizeof(prefix) - 1U, credited_name,
		    credited_length);
		memcpy(congratulations + sizeof(prefix) - 1U + credited_length,
		    suffix, sizeof(suffix) - 1U);
		for (size_t repeat = 0U; repeat < 3U; ++repeat) {
			if (!rmt_completion_add(result, congratulations, length))
				return false;
		}
	}
	result->returns_to_bbs = !local_mode;
	if (result->returns_to_bbs
	    && !rmt_completion_add(result, returning, sizeof(returning) - 1U))
		return false;
	return true;
}

bool
yt_rmt_completion_delay(struct yt_rmt_delay_result *result)
{
	/* RMT-INIT:2305..232D: FOR scratch = 1 TO 2222, with no body. */
	volatile float scratch = 1.0f;
	size_t admitted = 0U;

	if (result == NULL)
		return false;
	while (scratch <= 2222.0f) {
		++admitted;
		scratch += 1.0f;
	}
	result->admitted_values = admitted;
	result->final_value = scratch;
	return true;
}

static bool
rmt_present(const struct yt_initializer_options *options, uint16_t site,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (options->family != YT_INITIALIZER_RMT
	    || options->rmt_presenter == NULL)
		return true;
	if (options->rmt_presenter->write == NULL
	    || !options->rmt_presenter->write(options->rmt_presenter->context,
	    site, entry, payload, payload_length, error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "present RMT-INIT site %04X", site);
			error->path[0] = '\0';
		}
		return false;
	}
	return true;
}

static bool
rmt_present_text(const struct yt_initializer_options *options, uint16_t site,
    enum yt_rmt_output_entry entry, const char *text, struct yt_error *error)
{
	return rmt_present(options, site, entry, (const uint8_t *)text,
	    strlen(text), error);
}

static bool
rmt_present_number_line(const struct yt_initializer_options *options,
    uint16_t site, const char *label, float value, struct yt_error *error)
{
	uint8_t payload[192];
	char number[32];
	size_t label_length = strlen(label);
	int number_length = qb_str_single(number, sizeof(number), value);

	if (number_length < 0 || label_length + (size_t)number_length
	    > sizeof(payload)) {
		set_error(error, YT_RANGE, "compose RMT numeric row", "");
		return false;
	}
	memcpy(payload, label, label_length);
	memcpy(payload + label_length, number, (size_t)number_length);
	return rmt_present(options, site, YT_RMT_OUTPUT_LINE, payload,
	    label_length + (size_t)number_length, error);
}

bool
yt_rmt_standalone_prompt_compose(struct yt_rmt_standalone_output *output)
{
	static const uint8_t prompt[] =
	    "\r\r"
	    "Running stand alone... re-initializing using old sysop defined defaults.\r"
	    "\r"
	    "Do you wish to re-init Y.T. using your old default values?";

	if (output == NULL || sizeof(prompt) - 1U > sizeof(output->bytes))
		return false;
	memcpy(output->bytes, prompt, sizeof(prompt) - 1U);
	output->length = sizeof(prompt) - 1U;
	output->proceed = false;
	return true;
}

bool
yt_rmt_standalone_response_compose(const uint8_t *response,
    size_t response_length, struct yt_rmt_standalone_output *output)
{
	bool proceed;
	size_t length;

	if (output == NULL || (response == NULL && response_length != 0U))
		return false;
	proceed = response_length == 1U
	    && (response[0] == 'Y' || response[0] == 'y');
	/*
	 * The normal B5 editor offers its leading preparation space, the final
	 * value, two repaint spaces and accepted CR. The following 0103 blank is
	 * owned by the application and exists only for exact Y/y.
	 */
	length = 1U + response_length + 2U + 1U + (proceed ? 1U : 0U);
	if (length > sizeof(output->bytes))
		return false;
	output->bytes[0] = ' ';
	if (response_length != 0U)
		memcpy(output->bytes + 1U, response, response_length);
	output->bytes[1U + response_length] = ' ';
	output->bytes[2U + response_length] = ' ';
	output->bytes[3U + response_length] = '\r';
	if (proceed)
		output->bytes[4U + response_length] = '\r';
	output->length = length;
	output->proceed = proceed;
	return true;
}

bool
yt_rmt_handoff_parse(const uint8_t *data, size_t length,
    struct yt_rmt_handoff_result *result)
{
	size_t path_length = 0U;

	if (result == NULL || (data == NULL && length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	if (length == 0U) {
		result->standalone = true;
		return true;
	}
	while (path_length < length && data[path_length] != '\r'
	    && data[path_length] != '\n' && data[path_length] != 0x1aU)
		++path_length;
	if (path_length >= sizeof(result->path))
		return false;
	if (path_length != 0U)
		memcpy(result->path, data, path_length);
	result->path[path_length] = 0U;
	result->path_length = path_length;
	return true;
}

bool
yt_rmt_dorinfo_parse(const uint8_t *raw, size_t raw_length,
    uint8_t *storage, size_t storage_capacity,
    struct yt_rmt_dorinfo_result *result)
{
	size_t position = 0U;
	size_t used = 0U;
	size_t field;

	if ((raw == NULL && raw_length != 0U)
	    || (storage == NULL && storage_capacity != 0U) || result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	result->outcome = YT_RMT_DORINFO_SUCCESS;
	for (field = 0U; field < YT_RMT_DORINFO_FIELDS; ++field) {
		struct yt_rmt_dorinfo_field *destination =
		    &result->fields[field];

		destination->offset = used;
		if (position >= raw_length || raw[position] == 0x1aU) {
			result->outcome = YT_RMT_DORINFO_INPUT_PAST_END;
			result->failed_field = field + 1U;
			result->cursor = position;
			result->error_number = 62;
			return true;
		}
		while (position < raw_length && raw[position] != 0x1aU) {
			uint8_t value = raw[position++];

			if (value == 0U)
				continue;
			if (value == '\r') {
				if (position < raw_length && raw[position] == '\n')
					++position;
				break;
			}
			if (used >= storage_capacity)
				return false;
			storage[used++] = value;
		}
		destination->length = used - destination->offset;
		++result->fields_assigned;
	}
	result->cursor = position;
	return true;
}

const uint8_t *
yt_rmt_dorinfo_field(const struct yt_rmt_dorinfo_result *result,
    const uint8_t *storage, size_t field, size_t *length)
{
	if (result == NULL || storage == NULL || length == NULL
	    || field >= result->fields_assigned)
		return NULL;
	*length = result->fields[field].length;
	return storage + result->fields[field].offset;
}

bool
yt_rmt_serial_state_compose(const uint8_t *identifier,
    size_t identifier_length, const uint8_t *description,
    size_t description_length, uint8_t dll, uint8_t dlm,
    struct yt_rmt_serial_state *result)
{
	struct yt_startup_serial_layout layout;

	if (result == NULL || (identifier == NULL && identifier_length != 0U)
	    || (description == NULL && description_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	result->requested_port = yt_startup_parse_port(identifier,
	    identifier_length);
	result->sampled_dll = dll;
	result->sampled_dlm = dlm;
	if (result->requested_port < 1 || result->requested_port > 4) {
		result->outcome = YT_RMT_SERIAL_LOCAL;
		return true;
	}
	if (!yt_startup_serial_layout(result->requested_port, &layout))
		return false;
	result->brun_device = layout.brun_device;
	result->uart_base = layout.uart_base;
	result->modem_status_port = layout.modem_status_port;
	result->bios_address = layout.bios_address;
	result->bios_value = layout.bios_value;
	if (!yt_startup_framing_compose(description, description_length,
	    &result->opening_framing))
		return false;
	if (!yt_startup_detect_baud(dll, dlm, &result->detected_baud)) {
		result->outcome = YT_RMT_SERIAL_ZERO_DIVISOR;
		return true;
	}
	if (!yt_startup_open_spec(result->requested_port, description,
	    description_length, result->open_spec, sizeof(result->open_spec),
	    &result->open_spec_length)
	    || !yt_startup_restored_divisor(result->detected_baud,
	    &result->restored_dll, &result->restored_dlm))
		return false;
	result->outcome = YT_RMT_SERIAL_REMOTE;
	return true;
}

static bool
rmt_add_serial_event(struct yt_rmt_serial_event_result *result,
    enum yt_rmt_serial_event_operation operation, uint16_t address,
    uint16_t value, int error_number, bool complete)
{
	struct yt_rmt_serial_event *event;

	if (result->event_count >= YT_RMT_SERIAL_EVENTS)
		return false;
	event = &result->events[result->event_count++];
	event->operation = operation;
	event->address = address;
	event->value = value;
	event->error_number = error_number;
	event->complete = complete;
	return true;
}

bool
yt_rmt_serial_events_compose(const struct yt_rmt_serial_state *state,
    uint8_t pre_open_lcr, uint8_t pre_open_ier, int serial_open_error,
    uint8_t post_open_lcr, uint8_t post_open_ier,
    struct yt_rmt_serial_event_result *result)
{
	uint16_t base;

	if (state == NULL || result == NULL || serial_open_error < 0
	    || serial_open_error > 255)
		return false;
	memset(result, 0, sizeof(*result));
	if (state->outcome == YT_RMT_SERIAL_LOCAL) {
		result->outcome = YT_RMT_SERIAL_EVENTS_LOCAL;
		return true;
	}
	if ((state->outcome != YT_RMT_SERIAL_REMOTE
	    && state->outcome != YT_RMT_SERIAL_ZERO_DIVISOR)
	    || state->requested_port < 1 || state->requested_port > 4)
		return false;
	base = state->uart_base;
	if (!rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_DEF_SEG,
	    0U, 0U, 0, true))
		return false;
	if (state->bios_address != 0U
	    && (!rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_POKE,
	    state->bios_address, (uint16_t)(state->bios_value & 0xffU), 0,
	    true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_POKE,
	    (uint16_t)(state->bios_address + 1U),
	    (uint16_t)(state->bios_value >> 8), 0, true)))
		return false;
	if (!rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN,
	    (uint16_t)(base + 3U), pre_open_lcr, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr & 0x7fU), 0,
	    true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN,
	    (uint16_t)(base + 1U), pre_open_ier, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 1U), 0U, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr | 0x80U), 0,
	    true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN,
	    (uint16_t)(base + 1U), state->sampled_dlm, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN, base,
	    state->sampled_dll, 0, true))
		return false;
	if (state->outcome == YT_RMT_SERIAL_ZERO_DIVISOR) {
		if (!rmt_add_serial_event(result,
		    YT_RMT_SERIAL_EVENT_RUNTIME_ERROR, 0U, 0U, 11, false))
			return false;
		result->outcome = YT_RMT_SERIAL_EVENTS_ZERO_DIVISOR;
		return true;
	}
	if (!rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr & 0x7fU), 0,
	    true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 1U), pre_open_ier, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), pre_open_lcr, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OPEN,
	    3U, 0U, serial_open_error, serial_open_error == 0))
		return false;
	if (serial_open_error != 0) {
		result->outcome = YT_RMT_SERIAL_EVENTS_OPEN_ERROR;
		return true;
	}
	if (!rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN,
	    (uint16_t)(base + 3U), post_open_lcr, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_IN,
	    (uint16_t)(base + 1U), post_open_ier, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 1U), 0U, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(post_open_lcr | 0x80U), 0,
	    true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT, base,
	    state->restored_dll, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 1U), state->restored_dlm, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 3U), post_open_lcr, 0, true)
	    || !rmt_add_serial_event(result, YT_RMT_SERIAL_EVENT_OUT,
	    (uint16_t)(base + 1U), post_open_ier, 0, true))
		return false;
	result->outcome = YT_RMT_SERIAL_EVENTS_REMOTE;
	return true;
}

bool
yt_rmt_remote_status_compose(bool serial_open, float com_port, float baud,
    struct yt_rmt_standalone_output *output)
{
	static const uint8_t local[] = "Local Console Mode\r";
	static const uint8_t opening[] = "Opening COM port";
	char number[64];
	int number_length;
	size_t length = 0U;

	if (output == NULL)
		return false;
	memset(output, 0, sizeof(*output));
	if (!serial_open) {
		memcpy(output->bytes, local, sizeof(local) - 1U);
		output->length = sizeof(local) - 1U;
		return true;
	}
	memcpy(output->bytes, opening, sizeof(opening) - 1U);
	length = sizeof(opening) - 1U;
	number_length = qb_print_single(number, sizeof(number), com_port);
	if (number_length < 0
	    || (size_t)number_length > sizeof(output->bytes) - length)
		return false;
	memcpy(output->bytes + length, number, (size_t)number_length);
	length += (size_t)number_length;
	if (sizeof("at") - 1U > sizeof(output->bytes) - length)
		return false;
	memcpy(output->bytes + length, "at", sizeof("at") - 1U);
	length += sizeof("at") - 1U;
	number_length = qb_print_single(number, sizeof(number), baud);
	if (number_length < 0
	    || (size_t)number_length > sizeof(output->bytes) - length)
		return false;
	memcpy(output->bytes + length, number, (size_t)number_length);
	length += (size_t)number_length;
	if (sizeof("baud\r") - 1U > sizeof(output->bytes) - length)
		return false;
	memcpy(output->bytes + length, "baud\r", sizeof("baud\r") - 1U);
	output->length = length + sizeof("baud\r") - 1U;
	return true;
}

bool
yt_rmt_credited_name(const char *first, const char *last,
    const struct yt_name_file *names, char *credited, size_t credited_size)
{
	char normalized[256];
	size_t index;
	int written;

	if (first == NULL || last == NULL || names == NULL || credited == NULL
	    || credited_size == 0U
	    || (names->rows == NULL && names->count != 0U))
		return false;
	written = snprintf(normalized, sizeof(normalized), "%s %s", first, last);
	if (written < 0 || (size_t)written >= sizeof(normalized))
		return false;
	qb_title_case(normalized);
	credited[0] = '\0';
	for (index = 0U; index < names->count; ++index) {
		const struct yt_name_row *row = &names->rows[index];
		char real[257];

		written = snprintf(real, sizeof(real), "%s %s", row->real_first,
		    row->real_last);
		if (written < 0 || (size_t)written >= sizeof(real))
			return false;
		if (strcmp(real, normalized) != 0)
			continue;
		written = snprintf(credited, credited_size, "%s %s",
		    row->alias_first, row->alias_last);
		if (written < 0 || (size_t)written >= credited_size)
			return false;
	}
	return true;
}

void
yt_rmt_normalize_config(struct yt_config *config, bool local_mode)
{
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "NUL");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode)
		config->local_screen = -1.0f;
	if (config->lottery_plays < 1.0f)
		config->lottery_plays = 1.0f;
	if (config->genesis_ports < 20.0f || config->genesis_ports > 300.0f)
		config->genesis_ports = 200.0f;
	if (config->maximum_holds < 5.0f
	    || config->maximum_holds > 1000.0f)
		config->maximum_holds = 50.0f;
	config->marker = 6324.0f;
	config->maximum_planets = 0.0f;
}

bool
yt_rmt_preprocess_old_database(struct yt_database *database,
    struct yt_config *config, struct yt_error *error)
{
	int basic;

	if (config->headquarters == 0.0f) {
		config->headquarters = 85.0f;
		yt_record_set_number(&config->record, YT_F117, 85.0f);
		if (!yt_database_write(database, 1, &config->record, error))
			return false;
	}
	for (basic = 2; basic <= (int)config->sector_offset; ++basic) {
		struct yt_record record;
		float cloak;

		if (!yt_database_read(database, (size_t)basic, &record, error))
			return false;
		cloak = yt_record_get_number(&record, YT_F125);
		if (cloak > 0.0f) {
			record.bytes[YT_F125 + 2U] ^= 0x80U;
			if (!yt_database_write(database, (size_t)basic, &record,
			    error))
				return false;
		}
	}
	return yt_database_flush(database, error);
}

static float
single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

bool
yt_init_sector_prepass(struct yt_database *database, float sector_offset,
    int sector_count, float *port_offset, struct yt_error *error)
{
	struct yt_record record;
	float computed;

	if (database == NULL || port_offset == NULL || sector_count < 0) {
		set_error(error, YT_INVALID, "YT-INIT sector prepass", "");
		return false;
	}
	computed = single_add(sector_offset, (float)sector_count);
	*port_offset = computed;
	if (!yt_database_read(database, 1U, &record, error))
		return false;
	yt_record_set_number(&record, YT_F57, computed);
	return yt_database_write(database, 1U, &record, error);
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
float_bits(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

static bool
draw(struct yt_random *random, float *value, struct yt_error *error)
{
	return yt_random_next(random, value, error);
}

bool
yt_initializer_confirm_response(const char *response)
{
	return response != NULL
	    && (response[0] == 'Y' || response[0] == 'y')
	    && response[1] == '\0';
}

void
yt_initializer_layout_yt(struct yt_initializer_preparation *preparation)
{
	if (preparation == NULL)
		return;
	memset(preparation, 0, sizeof(*preparation));
	preparation->config.sector_offset = YT_INIT_PLAYERS + 1.0f;
	preparation->config.port_offset = preparation->config.sector_offset
	    + YT_INIT_SECTORS;
	preparation->config.planet_offset = preparation->config.port_offset
	    + YT_INIT_PORTS;
	preparation->config.total_records = preparation->config.planet_offset
	    + YT_INIT_PLANETS;
}

bool
yt_initializer_prepare_yt(struct yt_random *random,
    struct yt_initializer_preparation *preparation, struct yt_error *error)
{
	struct yt_clock_value epoch_date;
	struct yt_clock_value maintenance_date;
	float sample;

	if (random == NULL || preparation == NULL) {
		set_error(error, YT_INVALID, "YT-INIT preparation", "");
		return false;
	}
	yt_initializer_layout_yt(preparation);
	if (!yt_platform_clock(&epoch_date, error)
	    || !yt_platform_clock(&maintenance_date, error)
	    || !draw(random, &sample, error))
		return false;
	preparation->config.epoch_year = (float)(epoch_date.year % 100);
	preparation->config.turns_per_day = 500.0f;
	preparation->config.initial_fighters = 25.0f;
	preparation->config.initial_credits = 1005.0f;
	preparation->config.initial_holds = 10.0f;
	preparation->config.retention_days = 14.0f;
	preparation->config.local_screen = -1.0f;
	preparation->config.lottery_plays = 5.0f;
	preparation->config.genesis_ports = 300.0f;
	preparation->config.maximum_holds = 1000.0f;
	preparation->config.marker = 6324.0f;
	preparation->config.maximum_planets = 0.0f;
	preparation->today = yt_date_serial(&maintenance_date,
	    (int)preparation->config.epoch_year, NULL);
	preparation->config.last_maintenance = (float)(preparation->today - 1);
	preparation->config.headquarters = (float)((int)floorf(single_mul(sample,
	    (float)(YT_INIT_SECTORS - 7))) + 1);
	return true;
}

static bool
yt_init_present_number(const struct yt_init_presenter *presenter,
    uint16_t site, float value, enum yt_init_output_entry entry,
    struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0) {
		set_error(error, YT_RANGE, "format YT-INIT number", "");
		return false;
	}
	return yt_init_present_one(presenter, site, entry,
	    (const uint8_t *)text, (size_t)length, error);
}

static bool
yt_init_present_print_number(const struct yt_init_presenter *presenter,
    uint16_t site, float value, enum yt_init_output_entry entry,
    struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0 || (size_t)length >= sizeof(text)) {
		set_error(error, YT_RANGE, "format YT-INIT PRINT number", "");
		return false;
	}
	text[length++] = ' ';
	return yt_init_present_one(presenter, site, entry,
	    (const uint8_t *)text, (size_t)length, error);
}

bool
yt_init_present_prepared_configuration(
    const struct yt_initializer_preparation *preparation,
    const struct yt_init_presenter *presenter, struct yt_error *error)
{
	const struct yt_config *config;

	if (preparation == NULL) {
		set_error(error, YT_INVALID, "YT-INIT prepared presentation", "");
		return false;
	}
	config = &preparation->config;
	return yt_init_present_text(presenter, 0x0769U, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x077bU,
	    YT_INIT_OUTPUT_INLINE, "Starting year:", error)
	    && yt_init_present_number(presenter, 0x0787U,
	    config->epoch_year, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x0799U, YT_INIT_OUTPUT_LINE,
	    "Starting player info:", error)
	    && yt_init_present_text(presenter, 0x07b6U,
	    YT_INIT_OUTPUT_INLINE, "  # of fighters at start:", error)
	    && yt_init_present_number(presenter, 0x07c2U,
	    config->initial_fighters, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x07ebU,
	    YT_INIT_OUTPUT_INLINE, "  # of credits at start:", error)
	    && yt_init_present_number(presenter, 0x07f7U,
	    config->initial_credits, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x0820U,
	    YT_INIT_OUTPUT_INLINE, "  # of cargo holds at start:", error)
	    && yt_init_present_number(presenter, 0x082cU,
	    config->initial_holds, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x0855U,
	    YT_INIT_OUTPUT_INLINE,
	    "  # of days inactivity until an dead player is deleted:", error)
	    && yt_init_present_number(presenter, 0x0861U,
	    config->retention_days, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x089cU, YT_INIT_OUTPUT_LINE,
	    "  Last day maintenance run: Yesterday", error)
	    && yt_init_present_text(presenter, 0x08c7U,
	    YT_INIT_OUTPUT_INLINE, "  # of turns per day:", error)
	    && yt_init_present_number(presenter, 0x08d3U,
	    config->turns_per_day, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x08fcU,
	    YT_INIT_OUTPUT_INLINE,
	    "  # of times per day a user may play the lottery:", error)
	    && yt_init_present_number(presenter, 0x0908U,
	    config->lottery_plays, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x0960U,
	    YT_INIT_OUTPUT_INLINE,
	    "  Xannor Headquarters placed in sector:", error)
	    && yt_init_present_print_number(presenter, 0x0967U,
	    config->headquarters, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x0990U, YT_INIT_OUTPUT_LINE,
	    "  Ports needed to initiate Genesis: 450", error)
	    && yt_init_present_text(presenter, 0x09ccU,
	    YT_INIT_OUTPUT_INLINE, "  Maximum Cargo holds set to:", error)
	    && yt_init_present_print_number(presenter, 0x09d3U,
	    config->maximum_holds, YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, 0x09e5U, YT_INIT_OUTPUT_LINE,
	    "  Local screen on with remote callers: On", error)
	    && yt_init_present_text(presenter, 0x09f7U, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x0a18U, YT_INIT_OUTPUT_LINE,
	    "If at any time you wish to change these settings, run YTCONFIG.",
	    error)
	    && yt_init_present_text(presenter, 0x0a29U, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, 0x0a3bU, YT_INIT_OUTPUT_LINE,
	    "Please input filename for the Scoreboard Bulletin.", error)
	    && yt_init_present_text(presenter, 0x0a4dU, YT_INIT_OUTPUT_LINE,
	    "Include FULL PATH and NAME of file! ([ENTER] for YTSCORE.ASC) : ",
	    error)
	    && yt_init_present_text(presenter, 0x0a5fU,
	    YT_INIT_OUTPUT_INLINE, "-=> ", error);
}

bool
yt_initializer_bounded(struct yt_random *random, int bound, int *value,
    struct yt_error *error)
{
	float sample;

	if (bound <= 0) {
		set_error(error, YT_RANGE, "bounded random", "");
		return false;
	}
	if (!draw(random, &sample, error))
		return false;
	*value = (int)floorf(single_mul(sample, (float)bound)) + 1;
	return true;
}

static bool
pool_pointers(const char *pointers[YT_NAME_TOKENS])
{
	const char *cursor = port_name_blob;
	size_t index;

	for (index = 0; index < YT_NAME_TOKENS; ++index) {
		if (*cursor == '\0')
			return false;
		pointers[index] = cursor;
		cursor += strlen(cursor) + 1U;
	}
	return *cursor == '\0';
}

bool
yt_generate_port_name(struct yt_random *random, char name[42],
    struct yt_error *error)
{
	const char *pool[YT_NAME_TOKENS];
	float first;
	float second;
	int parts;
	int part;
	size_t used = 0;

	if (!pool_pointers(pool)) {
		set_error(error, YT_INVALID, "compiled port-name pool", "");
		return false;
	}
	if (!draw(random, &first, error) || !draw(random, &second, error))
		return false;
	parts = (int)floorf(single_mul(single_mul(first, second), 3.0f)) + 2;
	name[0] = '\0';
	for (part = 0; part < parts; ++part) {
		float sample;
		int selected;
		const char *token;
		size_t length;
		bool leading;

		if (!draw(random, &sample, error))
			return false;
		selected = (int)floorf(single_mul(sample,
		    (float)YT_NAME_TOKENS));
		token = pool[selected];
		length = strlen(token);
		leading = length > 4;
		if (leading && used > 0 && used < 41)
			name[used++] = ' ';
		if (length > 0 && used < 41) {
			name[used++] = leading
			    ? (char)((uint8_t)token[0] & 0xdfU) : token[0];
			++token;
			--length;
		}
		while (length-- > 0 && used < 41)
			name[used++] = *token++;
	}
	name[used] = '\0';
	if (used > 0)
		name[0] = (char)((uint8_t)name[0] & 0xdfU);
	return true;
}

static bool
pair_already_linked(const struct world *world, int source, int destination)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[source][slot] == destination
		    || world->warps[destination][slot] == source)
			return true;
	}
	return false;
}

static bool
sector_nonempty(const struct world *world, int sector)
{
	int slot;

	for (slot = 0; slot < 6; ++slot) {
		if (world->warps[sector][slot] != 0)
			return true;
	}
	return false;
}

static bool
randomize_sector(struct world *world, int sector, struct yt_random *random,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	const float local_threshold = float_bits(UINT32_C(0x3ecccccd));
	const float long_threshold = float_bits(UINT32_C(0x3f7c28f6));

	do {
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			float probability;

			if (!draw(random, &probability, error))
				return false;
			if (probability <= local_threshold && slot < 5) {
				int distance;
				int destination;

				if (!yt_initializer_bounded(random, 10, &distance, error))
					return false;
				if (sector > world->sectors - 10)
					distance = -distance;
				destination = sector + distance;
				if (destination < 1 || destination > world->sectors) {
					set_error(error, YT_RANGE,
					    "initializer local warp", "");
					return false;
				}
				if (world->warps[sector][slot] == 0
				    && world->warps[destination][slot] == 0
				    && !pair_already_linked(world, sector,
				    destination)) {
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
				}
			}

			if (slot == 5 && world->warps[sector][slot] == 0) {
				for (;;) {
					int destination;

					if (!draw(random, &probability, error))
						return false;
					if (probability < long_threshold)
						break;
					if (!yt_initializer_bounded(random, world->sectors,
					    &destination, error))
						return false;
					if (destination == sector)
						continue;
					if (world->warps[destination][slot] > 0)
						break;
					world->warps[sector][slot] = destination;
					world->warps[destination][slot] = sector;
					{
						uint8_t payload[48];
						char source_text[16];
						char destination_text[16];
						int source_length = qb_str_single(source_text,
						    sizeof(source_text), (float)sector);
						int destination_length = qb_str_single(
						    destination_text, sizeof(destination_text),
						    (float)destination);
						size_t length;

						if (source_length < 0 || destination_length < 0)
							return false;
						length = (size_t)source_length + 2U
						    + (size_t)destination_length;
						memcpy(payload, source_text,
						    (size_t)source_length);
						memcpy(payload + source_length, " -", 2U);
						memcpy(payload + source_length + 2U,
						    destination_text,
						    (size_t)destination_length);
						if (!yt_present_number(options, 0x10f8U,
						    (float)sector, YT_INIT_OUTPUT_INLINE,
						    error)
						    || !yt_present_text(options, 0x1102U,
						    YT_INIT_OUTPUT_INLINE, "-", error)
						    || !yt_present_number(options, 0x110aU,
						    (float)destination, YT_INIT_OUTPUT_LINE,
						    error)
						    || !rmt_present(options, 0x10f1U,
						    YT_RMT_OUTPUT_COMMA_SERIAL_FIRST,
						    payload, length, error))
							return false;
					}
					break;
				}
			}
		}
	} while (sector != 1 && !sector_nonempty(world, sector));
	return true;
}

static bool
reachable(const struct world *world, int target, bool *result,
    struct yt_error *error)
{
	uint8_t *seen;
	int *queue;
	size_t head = 0;
	size_t tail = 0;

	seen = calloc((size_t)world->sectors + 1U, 1);
	queue = malloc(((size_t)world->sectors + 1U) * sizeof(*queue));
	if (seen == NULL || queue == NULL) {
		free(seen);
		free(queue);
		set_error(error, YT_NO_MEMORY, "initializer BFS", "");
		return false;
	}
	seen[1] = 1;
	queue[tail++] = 1;
	while (head < tail) {
		int current = queue[head++];
		int slot;

		for (slot = 0; slot < 6; ++slot) {
			int neighbor = world->warps[current][slot];

			if (!seen[neighbor]) {
				seen[neighbor] = 1;
				queue[tail++] = neighbor;
			}
		}
	}
	*result = seen[target] != 0;
	free(seen);
	free(queue);
	return true;
}

static bool
build_graph(struct world *world, enum yt_initializer_family family,
    struct yt_random *random, const struct yt_initializer_options *options,
    struct yt_error *error)
{
	int sector;
	int slot;
	float position;

	for (slot = 0; slot < 6; ++slot) {
		int destination = family == YT_INITIALIZER_YT ? slot + 1 : slot + 2;

		if (destination > world->sectors) {
			set_error(error, YT_RANGE, "initializer fixed warps", "");
			return false;
		}
		world->warps[1][slot] = destination;
		world->warps[destination][slot] = 1;
	}
	for (sector = 1; sector <= world->sectors; ++sector) {
		if (!randomize_sector(world, sector, random, options, error))
			return false;
	}
	if (!yt_present_text(options, 0x1245U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1254U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1268U, YT_INIT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error)
	    || !rmt_present(options, 0x1279U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x127cU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x128aU, YT_RMT_OUTPUT_LINE,
	    "Verifying warps.. linking isolated sectors.", error)
	    || !rmt_present(options, 0x128dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error))
		return false;
	for (sector = 2; sector <= world->sectors; ++sector) {
		bool found;

		if (!yt_present(options, 0x12acU,
		    YT_INIT_OUTPUT_LOCATE_COLUMN_ONE, NULL, 0U, error)
		    || !yt_present_text(options, 0x12b9U,
		    YT_INIT_OUTPUT_INLINE, "Verifying warp to sector", error)
		    || !yt_present_number(options, 0x12c1U, (float)sector,
		    YT_INIT_OUTPUT_INLINE, error)
		    || !reachable(world, sector, &found, error))
			return false;
		if (!found) {
			int candidate;
			uint8_t payload[96];
			char target_text[16];
			char candidate_text[16];
			int target_length;
			int candidate_length;
			size_t length;

			do {
				if (!yt_initializer_bounded(random, sector - 1, &candidate,
				    error))
					return false;
			} while (world->warps[candidate][5] != 0);
			target_length = qb_str_single(target_text,
			    sizeof(target_text), (float)sector);
			candidate_length = qb_str_single(candidate_text,
			    sizeof(candidate_text), (float)candidate);
			if (target_length < 0 || candidate_length < 0)
				return false;
			length = sizeof("*** Error - No Path to sector") - 1U
			    + (size_t)target_length + 2U;
			memcpy(payload, "*** Error - No Path to sector",
			    sizeof("*** Error - No Path to sector") - 1U);
			memcpy(payload + sizeof("*** Error - No Path to sector") - 1U,
			    target_text, (size_t)target_length);
			memcpy(payload + length - 2U, "!!", 2U);
			if (!yt_present(options, 0x1501U,
			    YT_INIT_OUTPUT_LOCATE_COLUMN_ONE, NULL, 0U, error)
			    || !yt_present_text(options, 0x150eU,
			    YT_INIT_OUTPUT_INLINE,
			    "*** Error - No Path to sector", error)
			    || !yt_present(options, 0x151bU,
			    YT_INIT_OUTPUT_INLINE, (const uint8_t *)target_text,
			    (size_t)target_length, error)
			    || !yt_present_text(options, 0x1523U,
			    YT_INIT_OUTPUT_LINE, "!!", error)
			    || !rmt_present(options, 0x2f48U, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			length = sizeof("Sector") - 1U + (size_t)target_length
			    + sizeof(" has been linked to sector") - 1U
			    + (size_t)candidate_length;
			memcpy(payload, "Sector", sizeof("Sector") - 1U);
			memcpy(payload + sizeof("Sector") - 1U, target_text,
			    (size_t)target_length);
			memcpy(payload + sizeof("Sector") - 1U
			    + (size_t)target_length, " has been linked to sector",
			    sizeof(" has been linked to sector") - 1U);
			memcpy(payload + length - (size_t)candidate_length,
			    candidate_text, (size_t)candidate_length);
			if (!yt_present_text(options, 0x15c8U,
			    YT_INIT_OUTPUT_INLINE, "Sector", error)
			    || !yt_present_number(options, 0x15cfU,
			    (float)sector, YT_INIT_OUTPUT_INLINE, error)
			    || !yt_present_text(options, 0x15d7U,
			    YT_INIT_OUTPUT_INLINE, "has been linked to sector", error)
			    || !yt_present_number(options, 0x15deU,
			    (float)candidate, YT_INIT_OUTPUT_LINE, error)
			    || !rmt_present(options, 0x302cU, YT_RMT_OUTPUT_LINE,
			    payload, length, error))
				return false;
			/* The stale reciprocal at the target is deliberately kept. */
			world->warps[sector][5] = candidate;
			world->warps[candidate][5] = sector;
		}
	}
	if (!yt_present_text(options, 0x1319U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1328U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x133cU, YT_INIT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error)
	    || !yt_present_text(options, 0x134dU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x135fU, YT_INIT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error)
	    || !rmt_present(options, 0x12f9U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1307U, YT_RMT_OUTPUT_LINE,
	    " ** Warp verification complete!! **", error)
	    || !rmt_present(options, 0x130aU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1318U, YT_RMT_OUTPUT_LINE,
	    " ** Building shortcuts back to sector 1", error))
		return false;
	if (!draw(random, &position, error))
		return false;
	position = single_add(single_mul(position, 400.0f), 8.0f);
	while (position < (float)world->sectors) {
		bool overflow;
		int selected = (int)qb_cint(position, &overflow);
		float increment;

		if (overflow || selected < 1 || selected > world->sectors) {
			set_error(error, YT_RANGE, "initializer shortcut", "");
			return false;
		}
		if (world->warps[selected][5] == 0)
			world->warps[selected][5] = 1;
		if (!draw(random, &increment, error))
			return false;
		position = single_add(position, single_mul(increment, 400.0f));
	}
	return true;
}

static bool
assign_ports(struct world *world, struct yt_random *random,
    struct yt_error *error)
{
	uint8_t *occupied;
	int port;

	if (world->ports < 4 || world->sectors < 7) {
		set_error(error, YT_RANGE, "initializer port layout", "");
		return false;
	}
	occupied = calloc((size_t)world->sectors + 1U, 1);
	if (occupied == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer ports", "");
		return false;
	}
	world->port_sectors[1] = 1;
	world->port_sectors[2] = 3;
	world->port_sectors[3] = 5;
	world->port_sectors[4] = 7;
	/* Compiled bug: mark sector indices 1..4, not 1,3,5,7. */
	for (port = 1; port <= 4; ++port)
		occupied[port] = 1;
	for (port = 5; port <= world->ports; ++port) {
		int sector;

		do {
			if (!yt_initializer_bounded(random, world->sectors - 1,
			    &sector, error)) {
				free(occupied);
				return false;
			}
			++sector;
		} while (occupied[sector]);
		world->port_sectors[port] = sector;
		occupied[sector] = 1;
	}
	for (port = 1; port <= world->ports; ++port)
		world->sector_ports[world->port_sectors[port]] = port;
	free(occupied);
	return true;
}

static void
make_config_record(struct yt_config *config, float stored_scoreboard_length)
{
	size_t length = strlen(config->scoreboard);

	yt_record_clear(&config->record);
	yt_record_set_text(&config->record,
	    (const uint8_t *)config->scoreboard, length);
	yt_record_set_number(&config->record, YT_F41, stored_scoreboard_length);
	yt_record_set_number(&config->record, YT_F45, config->epoch_year);
	yt_record_set_number(&config->record, YT_F49, config->turns_per_day);
	yt_record_set_number(&config->record, YT_F53, config->sector_offset);
	yt_record_set_number(&config->record, YT_F57, config->port_offset);
	yt_record_set_number(&config->record, YT_F61, config->planet_offset);
	yt_record_set_number(&config->record, YT_F65, config->initial_fighters);
	yt_record_set_number(&config->record, YT_F69, config->initial_credits);
	yt_record_set_number(&config->record, YT_F73, config->initial_holds);
	yt_record_set_number(&config->record, YT_F77, config->retention_days);
	yt_record_set_number(&config->record, YT_F81,
	    config->last_maintenance);
	yt_record_set_number(&config->record, YT_F85, config->local_screen);
	yt_record_set_number(&config->record, YT_F93, config->total_records);
	yt_record_set_number(&config->record, YT_F101, config->lottery_plays);
	yt_record_set_number(&config->record, YT_F105, config->genesis_ports);
	yt_record_set_number(&config->record, YT_F117, config->headquarters);
	yt_record_set_number(&config->record, YT_F121, config->maximum_holds);
	yt_record_set_number(&config->record, YT_F125, config->marker);
	yt_record_set_number(&config->record, YT_F129,
	    config->maximum_planets);
	config->scoreboard_length = stored_scoreboard_length;
}

static bool
rmt_present_preopen(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return rmt_present(options, 0x0863U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0871U, YT_RMT_OUTPUT_LINE,
	    "          Yankee Trader Remote Initialization Program v2.2", error)
	    && rmt_present_text(options, 0x087fU, YT_RMT_OUTPUT_LINE,
	    "                         By Alan Davenport", error)
	    && rmt_present(options, 0x0882U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present(options, 0x0885U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x089bU, YT_RMT_OUTPUT_LINE,
	    "Re-creating main data file: YTDATA.DAT", error);
}

static bool
rmt_present_before_headquarters(const struct yt_initializer_options *options,
    struct yt_config *config, struct yt_error *error)
{
	struct yt_clock_value maintenance_date;
	int maintenance_serial;

	if (!rmt_present(options, 0x0907U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x0915U, YT_RMT_OUTPUT_LINE,
	    "Starting player info:", error)
	    || !rmt_present_number_line(options, 0x0930U,
	    "  # of fighters at start:", config->initial_fighters, error)
	    || !rmt_present_number_line(options, 0x096eU,
	    "  # of credits at start:", config->initial_credits, error)
	    || !rmt_present_number_line(options, 0x099cU,
	    "  # of cargo holds at start:", config->initial_holds, error)
	    || !rmt_present_number_line(options, 0x09caU,
	    "  # of days inactivity until an dead player is deleted:",
	    config->retention_days, error))
		return false;
	if (!yt_platform_clock(&maintenance_date, error))
		return false;
	maintenance_serial = yt_date_serial(&maintenance_date,
	    (int)config->epoch_year, NULL);
	config->last_maintenance = (float)(maintenance_serial - 1);
	return rmt_present_text(options, 0x09fdU, YT_RMT_OUTPUT_LINE,
	    "  Last day maintenance run: Yesterday", error)
	    && rmt_present_number_line(options, 0x0a2bU,
	    "  # of turns per day:", config->turns_per_day, error)
	    && rmt_present_number_line(options, 0x0a59U,
	    "  # of times per day a user may play the lottery:",
	    config->lottery_plays, error);
}

static bool
rmt_present_after_headquarters(const struct yt_initializer_options *options,
    const struct yt_config *config, struct yt_error *error)
{
	uint8_t payload[192];
	size_t prefix_length;
	size_t scoreboard_length;

	if (!rmt_present_number_line(options, 0x0ab6U,
	    "  Xannor Headquarters placed in sector:", config->headquarters,
	    error)
	    || !rmt_present_number_line(options, 0x0ad1U,
	    "  Ports needed to initiate Genesis:", config->genesis_ports, error)
	    || !rmt_present_number_line(options, 0x0b0fU,
	    "  Maximum Cargo holds set to:", config->maximum_holds, error))
		return false;
	prefix_length = sizeof("  Scoreboard bulletin name and path is: ") - 1U;
	scoreboard_length = strlen(config->scoreboard);
	if (prefix_length + scoreboard_length > sizeof(payload)) {
		set_error(error, YT_RANGE, "compose RMT scoreboard row", "");
		return false;
	}
	memcpy(payload, "  Scoreboard bulletin name and path is: ",
	    prefix_length);
	memcpy(payload + prefix_length, config->scoreboard, scoreboard_length);
	return rmt_present(options, 0x0b35U, YT_RMT_OUTPUT_LINE, payload,
	    prefix_length + scoreboard_length, error)
	    && rmt_present(options, 0x0b5dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
rmt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return rmt_present_text(options, 0x0d75U, YT_RMT_OUTPUT_LINE,
	    "Initializing sectors...", error)
	    && rmt_present(options, 0x0da0U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0daeU, YT_RMT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && rmt_present(options, 0x0e22U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    && rmt_present_text(options, 0x0e30U, YT_RMT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && rmt_present(options, 0x0e33U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
yt_present_graph_opening(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	return yt_present_text(options, 0x0defU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, 0x0e01U, YT_INIT_OUTPUT_LINE,
	    "Generating Randomized Universe... Please be patient...", error)
	    && yt_present_text(options, 0x0e79U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    && yt_present_text(options, 0x0e8dU, YT_INIT_OUTPUT_LINE,
	    "Wormholes (Long Warps)..", error)
	    && yt_present_text(options, 0x0e9eU, YT_INIT_OUTPUT_LINE, "",
	    error);
}

static bool
write_config_and_players(struct yt_database *database,
    const struct yt_config *config,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	struct yt_record record;
	uint8_t payload[96];
	char players_text[32];
	int players_length;
	size_t prefix_length;
	int logical;

	if (options->family == YT_INITIALIZER_YT) {
		players_length = qb_str_single(players_text, sizeof(players_text),
		    config->sector_offset - 1.0f);
		if (players_length < 0
		    || !yt_present_text(options, 0x0abbU, YT_INIT_OUTPUT_LINE,
		    "", error)
		    || !yt_database_write(database, 1, &config->record, error)
		    || !yt_present(options, 0x0b09U, YT_INIT_OUTPUT_LINE,
		    config->record.bytes + YT_F53, 4U, error)
		    || !yt_present_text(options, 0x0b1bU,
		    YT_INIT_OUTPUT_INLINE, "Generating Player Records for", error)
		    || !yt_present_number(options, 0x0b22U,
		    config->sector_offset - 1.0f, YT_INIT_OUTPUT_INLINE, error)
		    || !yt_present_text(options, 0x0b2aU, YT_INIT_OUTPUT_LINE,
		    "Players.", error))
			return false;
	}
	if (options->family == YT_INITIALIZER_RMT) {
		players_length = qb_str_single(players_text, sizeof(players_text),
		    config->sector_offset - 1.0f);
		prefix_length = sizeof("Generating Player Records for") - 1U;
		if (players_length < 0
		    || prefix_length + (size_t)players_length
		    + sizeof(" Players.") - 1U > sizeof(payload)) {
			set_error(error, YT_RANGE, "compose RMT player row", "");
			return false;
		}
		memcpy(payload, "Generating Player Records for", prefix_length);
		memcpy(payload + prefix_length, players_text,
		    (size_t)players_length);
		memcpy(payload + prefix_length + (size_t)players_length,
		    " Players.", sizeof(" Players.") - 1U);
		if (!yt_database_write(database, 1, &config->record, error)
		    || !rmt_present(options, 0x0b7bU, YT_RMT_OUTPUT_BLANK, NULL,
		    0U, error)
		    || !rmt_present(options, 0x0babU, YT_RMT_OUTPUT_LINE,
		    payload, prefix_length + (size_t)players_length
		    + sizeof(" Players.") - 1U, error))
			return false;
	}

	yt_record_blank(&record);
	for (logical = 1; logical <= (int)config->sector_offset - 1;
	    ++logical) {
		if (!yt_database_write(database, (size_t)logical + 1U,
		    &record, error))
			return false;
	}
	if (!yt_database_flush(database, error)
	    || !yt_present_text(options, 0x0cf5U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x0d07U, YT_INIT_OUTPUT_LINE,
	    "Initializing sectors...", error))
		return false;
	return rmt_present(options, 0x0d67U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error);
}

static bool
write_world_database(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_config *config,
    const struct world *world, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_record record;
	struct yt_clock_value port_date;
	int today;
	int logical;

	if (!yt_present_text(options, 0x163fU, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1651U, YT_INIT_OUTPUT_LINE,
	    "Random sector data generated and verified. Writing...", error)
	    || !rmt_present(options, 0x138eU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x139cU, YT_RMT_OUTPUT_INLINE,
	    "Random sector data generated and verified. Writing...", error))
		return false;
	for (logical = 1; logical <= world->sectors; ++logical) {
		int slot;

		yt_record_blank(&record);
		for (slot = 0; slot < 6; ++slot)
			yt_record_set_number(&record, YT_F41 + (size_t)slot * 4U,
			    (float)world->warps[logical][slot]);
		yt_record_set_number(&record, YT_F65,
		    (float)world->sector_ports[logical]);
		if (!yt_database_write(database,
		    (size_t)yt_sector_basic_record(config, logical),
		    &record, error))
			return false;
		if (logical % 50 == 0
		    && !rmt_present_text(options, 0x13e9U,
		    YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present_text(options, 0x18f6U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1908U, YT_INIT_OUTPUT_LINE,
	    "Initializing ports...", error)
	    || !rmt_present(options, 0x1678U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x167bU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1689U, YT_RMT_OUTPUT_LINE,
	    "Initializing ports... (Be patient)", error))
		return false;
	if (!yt_platform_clock(&port_date, error))
		return false;
	today = yt_date_serial(&port_date, (int)config->epoch_year, NULL);
	if (!yt_present_text(options, 0x1a55U, YT_INIT_OUTPUT_LINE, "", error)
	    || !yt_present_text(options, 0x1a69U, YT_INIT_OUTPUT_LINE,
	    "   They started producing 10 days ago...", error)
	    || !yt_present_text(options, 0x1a7aU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present(options, 0x1acaU, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, 0x1ad7U, YT_INIT_OUTPUT_COMMA,
	    "Port #", error)
	    || !yt_present_text(options, 0x1adfU, YT_INIT_OUTPUT_COMMA,
	    "Prod Ore", error)
	    || !yt_present_text(options, 0x1ae7U, YT_INIT_OUTPUT_COMMA,
	    "Prod Org", error)
	    || !yt_present_text(options, 0x1aefU, YT_INIT_OUTPUT_COMMA,
	    "Prod Equ", error)
	    || !yt_present_text(options, 0x1af7U, YT_INIT_OUTPUT_LINE,
	    "Port Name", error)
	    || !rmt_present(options, 0x17f4U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1802U, YT_RMT_OUTPUT_INLINE,
	    "   They started producing 10 days ago...", error))
		return false;
	for (logical = 1; logical <= world->ports; ++logical) {
		char name[42];
		float sample;
		int commodity;
		int index;

		if (!yt_present_number(options, 0x1b1eU, (float)logical,
		    YT_INIT_OUTPUT_COMMA, error))
			return false;
		yt_record_clear(&record);
		for (index = 0; index < 3; ++index) {
			if (!draw(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F61 + (size_t)index * 4U,
			    (float)((int)floorf(single_mul(sample, 31767.0f))
			    + 1000));
		}
		for (index = 0; index < 3; ++index) {
			if (!draw(random, &sample, error))
				return false;
			yt_record_set_number(&record, YT_F73 + (size_t)index * 4U,
			    (float)(-(int)floorf(single_mul(sample, 100.0f))
			    - 1));
		}
		if (logical == 1)
			strcpy(name, "Earth");
		else if (!yt_generate_port_name(random, name, error))
			return false;
		if (!yt_present_text(options, 0x1c59U, YT_INIT_OUTPUT_LINE,
		    name, error))
			return false;
		if (!draw(random, &sample, error))
			return false;
		commodity = (int)floorf(single_mul(sample, 3.0f)) + 1;
		yt_record_set_text(&record, (const uint8_t *)name, strlen(name));
		yt_record_set_number(&record, YT_F41, (float)commodity);
		yt_record_set_number(&record, YT_F45, (float)(today - 10));
		for (index = 0; index < 3; ++index)
			yt_record_set_raw_number(&record,
			    YT_F49 + (size_t)index * 4U, raw_zero_residue);
		yt_record_set_number(&record,
		    YT_F73 + (size_t)(3 - commodity) * 4U,
		    -yt_record_get_number(&record,
		    YT_F73 + (size_t)(3 - commodity) * 4U));
		yt_record_set_number(&record, YT_F85, (float)strlen(name));
		yt_record_set_raw_number(&record, YT_F89, raw_zero_residue);
		yt_record_set_number(&record, YT_F93,
		    (float)world->port_sectors[logical]);
		yt_record_set_raw_number(&record, YT_F97, raw_zero_residue);
		if (options->family == YT_INITIALIZER_RMT)
			yt_record_set_raw_number(&record, YT_F101,
			    raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F105, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F109, raw_zero_residue);
		yt_record_set_raw_number(&record, YT_F129, raw_zero_residue);
		if (options->family == YT_INITIALIZER_YT && logical == 1) {
			yt_record_set_number(&record, YT_F101,
			    config->lottery_plays);
			yt_record_set_number(&record, YT_F117,
			    config->headquarters);
			yt_record_set_number(&record, YT_F121,
			    config->maximum_holds);
			yt_record_set_number(&record, YT_F125, config->marker);
		}
		if (!yt_database_write(database,
		    (size_t)yt_port_basic_record(config, logical),
		    &record, error))
			return false;
		if (logical % 15 == 0
		    && !rmt_present_text(options, 0x1c46U,
		    YT_RMT_OUTPUT_INLINE, ".", error))
			return false;
	}

	if (!yt_present(options, 0x1ecaU, YT_INIT_OUTPUT_LOCATE_ROW_25,
	    NULL, 0U, error)
	    || !yt_present_text(options, 0x1edeU, YT_INIT_OUTPUT_LINE,
	    "                                                                               ",
	    error)
	    || !yt_present_text(options, 0x1ef0U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1f02U, YT_INIT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !yt_present_str_number_line(options, 0x1f2bU,
	    "   Maximum number of planets:",
	    config->total_records - config->planet_offset, error)
	    || !rmt_present(options, 0x1c9dU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present(options, 0x1ca0U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1caeU, YT_RMT_OUTPUT_LINE,
	    "Initializing planets...", error)
	    || !rmt_present_number_line(options, 0x1cc9U,
	    "   Maximum number of planets:",
	    config->total_records - config->planet_offset, error))
		return false;
	record = config->record;
	yt_record_set_number(&record, YT_F85, 0.0f);
	for (logical = 1;
	    logical <= (int)config->total_records - (int)config->planet_offset;
	    ++logical) {
		if (!yt_database_write(database,
		    (size_t)yt_planet_basic_record(config, logical),
		    &record, error))
			return false;
	}

	if (!yt_present_text(options, 0x1fd1U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x1fe5U, YT_INIT_OUTPUT_LINE,
	    "Initializing the Xannor...", error)
	    || !rmt_present(options, 0x1d53U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1d61U, YT_RMT_OUTPUT_LINE,
	    "Initializing the Xannor...", error))
		return false;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config,
	    (int)config->headquarters), &record, error))
		return false;
	yt_record_set_number(&record, YT_F81, 10000.0f);
	yt_record_set_number(&record, YT_F85, -1.0f);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config,
	    (int)config->headquarters), &record, error))
		return false;
	if (!yt_database_read(database,
	    (size_t)yt_sector_basic_record(config, 1), &record, error))
		return false;
	yt_record_set_number(&record, YT_F105, config->headquarters);
	if (!yt_database_write(database,
	    (size_t)yt_sector_basic_record(config, 1), &record, error)
	    || !yt_database_flush(database, error))
		return false;
	return true;
}

static bool
append_bytes(uint8_t **data, size_t *length, size_t *capacity,
    const void *addition, size_t added, struct yt_error *error)
{
	size_t required = *length + added;

	if (required > *capacity) {
		size_t grown = *capacity == 0 ? 512U : *capacity;
		uint8_t *replacement;

		while (grown < required)
			grown *= 2U;
		replacement = realloc(*data, grown);
		if (replacement == NULL) {
			set_error(error, YT_NO_MEMORY, "initializer text", "");
			return false;
		}
		*data = replacement;
		*capacity = grown;
	}
	memcpy(*data + *length, addition, added);
	*length = required;
	return true;
}

static bool
write_banner(const char *credited_name, bool rmt, struct yt_error *error)
{
	static const char *const decorations[] = {
		"**********************",
		"**********************",
		"**                  **",
		"** Game Initialized **",
		"**                  **",
		"**********************",
		"**********************"
	};
	uint8_t *data = NULL;
	size_t length = 0;
	size_t capacity = 0;
	size_t index;

	if (rmt) {
		char prophecy[180];
		int written = snprintf(prophecy, sizeof(prophecy),
		    "** The Prophesy has been fulfilled by %s!! **\r\n",
		    credited_name != NULL ? credited_name : "");

		if (written < 0 || (size_t)written >= sizeof(prophecy))
			goto range_failure;
		for (index = 0; index < 3; ++index) {
			if (!append_bytes(&data, &length, &capacity, prophecy,
			    (size_t)written, error))
				goto failure;
		}
	}
	for (index = 0; index < YT_ARRAY_LEN(decorations); ++index) {
		struct yt_clock_value time_now;
		struct yt_clock_value date_now;
		char date[11];
		char time[9];
		char line[100];
		int written;

		if (!yt_platform_clock(&time_now, error)
		    || !yt_platform_clock(&date_now, error))
			goto failure;
		yt_format_time(&time_now, time);
		yt_format_date(&date_now, date);
		written = snprintf(line, sizeof(line), "%s %s %s\r\n",
		    time, date, decorations[index]);
		if (written < 0 || (size_t)written >= sizeof(line))
			goto range_failure;
		if (!append_bytes(&data, &length, &capacity, line,
		    (size_t)written, error))
			goto failure;
	}
	if (!yt_text_write("YTNEWS.DAT", data, length, true, error))
		goto failure;
	free(data);
	return true;

range_failure:
	set_error(error, YT_RANGE, "initializer banner", "YTNEWS.DAT");
failure:
	free(data);
	return false;
}

static bool
write_yt_auxiliary(const struct yt_initializer_options *options,
    struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t play[] = "L64cgaL1p1p1p1";

	if (!yt_present_text(options, 0x208cU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x209eU, YT_INIT_OUTPUT_LINE,
	    "Setting up newspaper file.", error)
	    || !write_banner(NULL, false, error)
	    || !yt_present_text(options, 0x224bU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x225fU, YT_INIT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error)
	    || !yt_text_write("YTNAME.DAT", dummy, sizeof(dummy) - 1U,
	    true, error)
	    || !yt_present_text(options, 0x22deU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x22f0U, YT_INIT_OUTPUT_LINE,
	    "Clearing YTRMSG.DAT  (Radio message file)", error)
	    || !yt_text_write("YTRMSG.DAT", NULL, 0, false, error)
	    || !yt_file_delete("YTRMSG.DAT", false, error)
	    || !yt_present_text(options, 0x2325U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x2339U, YT_INIT_OUTPUT_LINE,
	    "Initialization completed sucessfully!", error)
	    || !yt_present_text(options, 0x234aU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x235cU, YT_INIT_OUTPUT_LINE,
	    "<YT-INIT Normal Termination>", error)
	    || !yt_present_text(options, 0x236dU, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x237fU, YT_INIT_OUTPUT_LINE,
	    "Be SURE to run YTMAINT.EXE at LEAST ONCE per day EVERY DAY!", error)
	    || !yt_present_text(options, 0x2390U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x23a2U, YT_INIT_OUTPUT_LINE,
	    "Run YTCONFIG and change the default OPTIONS if you wish!", error)
	    || !yt_present_text(options, 0x23b3U, YT_INIT_OUTPUT_LINE, "",
	    error)
	    || !yt_present_text(options, 0x23c5U, YT_INIT_OUTPUT_LINE,
	    "Running initial maintenance...", error)
	    || !yt_present(options, 0x23cfU, YT_INIT_OUTPUT_PLAY, play,
	    sizeof(play) - 1U, error))
		return false;
	return true;
}

static bool
write_rmt_auxiliary(const char *credited_name,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t yesterday[] =
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n"
	    "NO YESTERDAY'S NEWS TO READ!\r\n";
	struct yt_radio_record radio;
	struct yt_radio_file file;
	char prophecy[180];
	int written;
	int index;
	bool valid = false;

	yt_radio_file_init(&file);

	if (!rmt_present(options, 0x1dfaU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x1e08U, YT_RMT_OUTPUT_LINE,
	    "Setting up newspaper file.", error)
	    || !write_banner(credited_name, true, error)
	    || !rmt_present(options, 0x2014U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x2022U, YT_RMT_OUTPUT_LINE,
	    "Setting up yesterday's newspaper file.", error)
	    || !yt_text_write("YTYNEWS.DAT", yesterday,
	    sizeof(yesterday) - 1U, true, error)
	    || !rmt_present(options, 0x20a1U, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x20afU, YT_RMT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error)
	    || !yt_text_write("YTNAME.DAT", dummy, sizeof(dummy) - 1U,
	    true, error)
	    || !rmt_present(options, 0x20ebU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error)
	    || !rmt_present_text(options, 0x20f9U, YT_RMT_OUTPUT_LINE,
	    "Clearing YTRMSG.DAT  (Radio message file)", error))
		return false;
	written = snprintf(prophecy, sizeof(prophecy),
	    "** The Prophesy has been fulfilled by %s!! **",
	    credited_name != NULL ? credited_name : "");
	if (written < 0 || (size_t)written >= sizeof(prophecy)) {
		set_error(error, YT_RANGE, "RMT prophecy", "YTRMSG.DAT");
		return false;
	}
	memset(&radio, 0, sizeof(radio));
	yt_radio_set_number(&radio, 0, 50.0f);
	yt_radio_set_number(&radio, 4, -2.0f);
	yt_radio_set_number(&radio, 8, -2.0f);
	yt_radio_set_text(&radio, (const uint8_t *)prophecy,
	    (size_t)written, 72);
	if (!yt_text_write("YTRMSG.DAT", NULL, 0U, false, error)
	    || !yt_radio_file_open(&file, "YTRMSG.DAT", error))
		goto done;
	for (index = 0; index < 5; ++index) {
		uint32_t record;

		if (!yt_radio_file_next_record(&file, &record, error)
		    || !yt_radio_file_put(&file, record, &radio, error))
			goto done;
	}
	if (!yt_radio_file_close(&file, error)
	    || !rmt_present(options, 0x227eU, YT_RMT_OUTPUT_BLANK, NULL, 0U,
	    error))
		goto done;
	valid = true;

done:
	if (file.random.file != NULL)
		(void)yt_radio_file_close(&file, NULL);
	return valid;
}

static void
free_world(struct world *world)
{
	free(world->warps);
	free(world->port_sectors);
	free(world->sector_ports);
	memset(world, 0, sizeof(*world));
}

static bool
allocate_world(struct world *world, struct yt_error *error)
{
	world->warps = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->warps));
	world->port_sectors = calloc((size_t)world->ports + 1U,
	    sizeof(*world->port_sectors));
	world->sector_ports = calloc((size_t)world->sectors + 1U,
	    sizeof(*world->sector_ports));
	if (world->warps == NULL || world->port_sectors == NULL
	    || world->sector_ports == NULL) {
		set_error(error, YT_NO_MEMORY, "initializer world", "");
		return false;
	}
	return true;
}

bool
yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error)
{
	struct yt_clock_value current;
	struct yt_config config;
	struct yt_database owned_database;
	struct yt_database *database;
	struct world world = {0};
	float sample;
	bool explicit_close_failed = false;
	bool result = false;

	if (options == NULL || random == NULL) {
		set_error(error, YT_INVALID, "initializer arguments", "");
		return false;
	}
	memset(&owned_database, 0, sizeof(owned_database));
	database = options->bound_database != NULL
	    ? options->bound_database : &owned_database;
	if (options->family == YT_INITIALIZER_RMT) {
		if (options->bound_database != NULL) {
			set_error(error, YT_INVALID, "bound RMT initializer", "");
			goto done;
		}
		if (!options->use_existing_config) {
			set_error(error, YT_INVALID, "RMT initializer configuration", "");
			goto done;
		}
		config = options->config;
		config.scoreboard_length = (float)strlen(config.scoreboard);
		world.sectors = (int)(config.port_offset - config.sector_offset);
		world.ports = (int)(config.planet_offset - config.port_offset);
		if (world.sectors < 7 || world.ports < 4) {
			set_error(error, YT_RANGE, "initializer configuration", "");
			goto done;
		}
		if (!allocate_world(&world, error)
		    || !rmt_present_preopen(options, error)
		    /* 08A1 OUTPUT/CLOSE leaves DOS EOF before 26D7 RANDOM reopen. */
		    || !yt_text_write("YTDATA.DAT", NULL, 0U, true, error)
		    || !yt_database_random_close(database, error)
		    || !yt_database_open(database, "YTDATA.DAT",
		    YT_OPEN_UPDATE_CREATE, error)
		    || !yt_platform_clock(&current, error))
			goto done;
		config.epoch_year = (float)(current.year % 100);
		if (!rmt_present_before_headquarters(options, &config, error)
		    || !draw(random, &sample, error))
			goto done;
		config.headquarters = (float)((int)floorf(single_mul(sample,
		    single_add((float)world.sectors, -7.0f))) + 1);
		if (!rmt_present_after_headquarters(options, &config, error))
			goto done;
		make_config_record(&config, config.scoreboard_length);
		if (!write_config_and_players(database, &config, options, error)
		    || !rmt_present_graph_opening(options, error))
			goto done;
	}
	else {
		const char *scoreboard;
		size_t scoreboard_length;

		if (options->bound_database != NULL) {
			if (database->file == NULL) {
				set_error(error, YT_INVALID,
				    "bound YT initializer", "YTDATA.DAT");
				goto done;
			}
		}
		else if (!yt_database_open(database, "YTDATA.DAT",
		    options->database_already_truncated ? YT_OPEN_UPDATE
		    : YT_OPEN_CREATE, error))
			goto done;
		if (options->prepared_yt) {
			if (!options->use_existing_config) {
				set_error(error, YT_INVALID,
				    "prepared YT-INIT configuration", "");
				goto done;
			}
			config = options->config;
			config.scoreboard_length =
			    (float)strlen(config.scoreboard);
			world.sectors = (int)(config.port_offset
			    - config.sector_offset);
			world.ports = (int)(config.planet_offset
			    - config.port_offset);
		}
		else if (!yt_platform_clock(&current, error))
			goto done;
		else if (options->use_existing_config) {
			config = options->config;
			config.scoreboard_length =
			    (float)strlen(config.scoreboard);
			world.sectors = (int)(config.port_offset
			    - config.sector_offset);
			world.ports = (int)(config.planet_offset
			    - config.port_offset);
		}
		else {
			scoreboard = options->scoreboard != NULL
			    && options->scoreboard[0] != '\0'
			    ? options->scoreboard : "YTSCORE.ASC";
			scoreboard_length = strlen(scoreboard);
			memset(&config, 0, sizeof(config));
			memcpy(config.scoreboard, scoreboard,
			    scoreboard_length < sizeof(config.scoreboard) - 1U
			    ? scoreboard_length : sizeof(config.scoreboard) - 1U);
			config.scoreboard[scoreboard_length
			    < sizeof(config.scoreboard) - 1U
			    ? scoreboard_length : sizeof(config.scoreboard) - 1U] = '\0';
			config.scoreboard_length = (float)scoreboard_length;
			config.epoch_year = (float)(current.year % 100);
			config.turns_per_day = 500.0f;
			config.sector_offset = YT_INIT_PLAYERS + 1.0f;
			config.port_offset = config.sector_offset + YT_INIT_SECTORS;
			config.planet_offset = config.port_offset + YT_INIT_PORTS;
			config.initial_fighters = 25.0f;
			config.initial_credits = 1005.0f;
			config.initial_holds = 10.0f;
			config.retention_days = 14.0f;
			config.local_screen = -1.0f;
			config.total_records = config.planet_offset + YT_INIT_PLANETS;
			config.lottery_plays = 5.0f;
			config.genesis_ports = 300.0f;
			config.maximum_holds = 1000.0f;
			config.marker = 6324.0f;
			config.maximum_planets = 0.0f;
			world.sectors = YT_INIT_SECTORS;
			world.ports = YT_INIT_PORTS;
		}
		if (world.sectors < 7 || world.ports < 4) {
			set_error(error, YT_RANGE, "initializer configuration", "");
			goto done;
		}
		if (!options->prepared_yt) {
			if (!yt_platform_clock(&current, error))
				goto done;
			config.last_maintenance = (float)(yt_date_serial(&current,
			    (int)config.epoch_year, NULL) - 1);
			if (!draw(random, &sample, error))
				goto done;
			config.headquarters = (float)((int)floorf(single_mul(sample,
			    single_add((float)world.sectors, -7.0f))) + 1);
		}
		make_config_record(&config, config.scoreboard_length);
		if (!write_config_and_players(database, &config, options, error)
		    || !yt_init_sector_prepass(database, config.sector_offset,
		    world.sectors, &config.port_offset, error)
		    || !allocate_world(&world, error))
			goto done;
	}
	if (!yt_present_graph_opening(options, error)
	    || !build_graph(&world, options->family, random, options, error)
	    || !assign_ports(&world, random, error)
	    || !write_world_database(database, options, &config, &world,
	    random, error))
		goto done;
	if (!yt_database_random_close(database, error)) {
		explicit_close_failed = true;
		goto done;
	}
	if (options->family == YT_INITIALIZER_YT)
		result = write_yt_auxiliary(options, error);
	else
		result = write_rmt_auxiliary(options->credited_name, options, error);

done:
	if (!explicit_close_failed)
		yt_database_close(database);
	free_world(&world);
	return result;
}

bool
yt_initialize_begin_yt(struct yt_error *error)
{
	struct yt_database database;

	if (!yt_database_open(&database, "YTDATA.DAT", YT_OPEN_CREATE, error))
		return false;
	return yt_database_random_close(&database, error);
}

bool
yt_initialize_bind_yt(struct yt_database *database,
    struct yt_init_binding *binding, struct yt_error *error)
{
	struct yt_record first;

	if (database == NULL || binding == NULL) {
		set_error(error, YT_INVALID, "bind YT initializer", "YTDATA.DAT");
		return false;
	}
	memset(binding, 0, sizeof(*binding));
	if (!yt_database_open(database, "YTDATA.DAT", YT_OPEN_UPDATE, error)
	    || !yt_database_random_get(database, 1U, &first,
	    &binding->first_accepted, error)
	    || !yt_config_decode(&binding->loaded, &first, error)
	    || !yt_database_random_get(database, 1U, &binding->second_record,
	    &binding->second_accepted, error)) {
		yt_database_close(database);
		return false;
	}
	return true;
}

bool
yt_initialize_yt(const char *scoreboard, struct yt_random *random,
    struct yt_error *error)
{
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_YT,
	    .scoreboard = scoreboard
	};

	return yt_initialize_world(&options, random, error);
}

bool
yt_initialize_yt_prepared(
    const struct yt_initializer_preparation *preparation,
    const char *scoreboard, struct yt_random *random,
    const struct yt_init_presenter *presenter, struct yt_error *error)
{
	return yt_initialize_yt_prepared_bound(NULL, preparation, scoreboard,
	    random, presenter, error);
}

bool
yt_initialize_yt_prepared_bound(struct yt_database *database,
    const struct yt_initializer_preparation *preparation,
    const char *scoreboard, struct yt_random *random,
    const struct yt_init_presenter *presenter, struct yt_error *error)
{
	struct yt_initializer_options options;
	const char *selected;
	size_t length;
	size_t retained;

	if (preparation == NULL || random == NULL) {
		set_error(error, YT_INVALID, "prepared YT-INIT", "");
		return false;
	}
	memset(&options, 0, sizeof(options));
	options.family = YT_INITIALIZER_YT;
	options.config = preparation->config;
	selected = scoreboard != NULL && scoreboard[0] != '\0'
	    ? scoreboard : "YTSCORE.ASC";
	length = strlen(selected);
	retained = length < sizeof(options.config.scoreboard) - 1U
	    ? length : sizeof(options.config.scoreboard) - 1U;
	memcpy(options.config.scoreboard, selected, retained);
	options.config.scoreboard[retained] = '\0';
	options.config.scoreboard_length = (float)length;
	options.use_existing_config = true;
	options.database_already_truncated = true;
	options.yt_presenter = presenter;
	options.prepared_yt = true;
	options.bound_database = database;
	return yt_initialize_world(&options, random, error);
}

bool
yt_initialize_rmt(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    struct yt_error *error)
{
	return yt_initialize_rmt_presented(config, credited_name, random, NULL,
	    error);
}

bool
yt_initialize_rmt_presented(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    const struct yt_rmt_presenter *presenter, struct yt_error *error)
{
	if (config == NULL) {
		set_error(error, YT_INVALID, "RMT initializer configuration", "");
		return false;
	}
	struct yt_initializer_options options = {
	    .family = YT_INITIALIZER_RMT,
	    .config = *config,
	    .use_existing_config = true,
	    .credited_name = credited_name,
	    .rmt_presenter = presenter
	};

	return yt_initialize_world(&options, random, error);
}
