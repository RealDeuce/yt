#include "yt_init_internal.h"

#include "qb.h"
#include "yt_names.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
yt_init_present_one(const struct yt_init_presenter *presenter,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (presenter == NULL || presenter->write == NULL) {
		set_error(error, YT_INVALID, "YT-INIT output", "");
		return false;
	}
	if (!presenter->write(presenter->context, entry, payload,
	    payload_length, error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "present YT-INIT output");
			error->path[0] = '\0';
		}
		return false;
	}
	return true;
}

static bool
yt_init_present_text(const struct yt_init_presenter *presenter,
    enum yt_init_output_entry entry, const char *text, struct yt_error *error)
{
	return yt_init_present_one(presenter, entry,
	    (const uint8_t *)text, strlen(text), error);
}

bool
yt_present(const struct yt_initializer_options *options,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (options->family != YT_INITIALIZER_YT)
		return true;
	return yt_init_present_one(options->yt_presenter, entry, payload,
	    payload_length, error);
}

bool
yt_present_text(const struct yt_initializer_options *options,
    enum yt_init_output_entry entry, const char *text, struct yt_error *error)
{
	return yt_present(options, entry, (const uint8_t *)text,
	    strlen(text), error);
}

bool
yt_present_number(const struct yt_initializer_options *options,
    float value, enum yt_init_output_entry entry,
    struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0 || (size_t)length >= sizeof(text)) {
		set_error(error, YT_RANGE, "format YT-INIT number", "");
		return false;
	}
	text[length++] = ' ';
	return yt_present(options, entry, (const uint8_t *)text,
	    (size_t)length, error);
}

bool
yt_present_str_number_line(const struct yt_initializer_options *options,
    const char *label, float value, struct yt_error *error)
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
	return yt_present(options, YT_INIT_OUTPUT_LINE, payload,
	    label_length + (size_t)number_length, error);
}

bool
yt_init_present_confirmation_prefix(const struct yt_init_presenter *presenter,
    struct yt_error *error)
{
	return yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "            Yankee Trader Initialization Program", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "                     By Alan Davenport", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "This program will initialize Yankee Trader. You must run this program at",
	    error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "least once when you start up the game. If this program is run on an",
	    error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "existing game, the old game will be wiped out and be replaced by a new one.",
	    error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "Continue (Y/N)? ", error);
}

bool
yt_init_present_opening(const struct yt_init_presenter *presenter,
    struct yt_error *error)
{
	return yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "Creating main data file: YTDATA.DAT", error);
}

bool
rmt_present(const struct yt_initializer_options *options,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	if (options->family != YT_INITIALIZER_RMT)
		return true;
	if (options->rmt_presenter == NULL
	    || options->rmt_presenter->write == NULL) {
		set_error(error, YT_INVALID, "RMT-INIT output", "");
		return false;
	}
	if (!options->rmt_presenter->write(options->rmt_presenter->context,
	    entry, payload, payload_length, error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "present RMT-INIT output");
			error->path[0] = '\0';
		}
		return false;
	}
	return true;
}

bool
rmt_present_text(const struct yt_initializer_options *options,
    enum yt_rmt_output_entry entry, const char *text, struct yt_error *error)
{
	return rmt_present(options, entry, (const uint8_t *)text,
	    strlen(text), error);
}

bool
rmt_present_number_line(const struct yt_initializer_options *options,
    const char *label, float value, struct yt_error *error)
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
	return rmt_present(options, YT_RMT_OUTPUT_LINE, payload,
	    label_length + (size_t)number_length, error);
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
    const struct yt_rmt_output_state *state, uint8_t *dest,
    size_t capacity,
    struct yt_rmt_output_result *result,
    struct yt_rmt_output_state *final_state)
{
	static const uint8_t lf[] = {'\n'};
	enum rmt_punctuation punctuation;
	bool serial_only;
	bool wormhole;
	size_t length = 0U;

	if (state == NULL || result == NULL || final_state == NULL
	    || (payload == NULL && payload_length != 0U)
	    || entry < YT_RMT_OUTPUT_LINE
	    || entry > YT_RMT_OUTPUT_SERIAL_LINE
	    || state->column >= 80U
	    || (entry == YT_RMT_OUTPUT_BLANK && payload_length != 0U))
		return false;
	*final_state = *state;
	serial_only = entry == YT_RMT_OUTPUT_SERIAL_LINE;
	wormhole = entry == YT_RMT_OUTPUT_WORMHOLE;
	if (local_mode && serial_only) {
		result->length = 0U;
		return true;
	}
	punctuation = entry == YT_RMT_OUTPUT_LINE
	    || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE
	    ? RMT_PUNCTUATION_NEWLINE
	    : wormhole
	    ? RMT_PUNCTUATION_COMMA : RMT_PUNCTUATION_SEMICOLON;
	if (!rmt_render_value(dest, capacity, &length,
	    &final_state->column,
	    !local_mode && (entry == YT_RMT_OUTPUT_LINE
	    || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE) ? lf : NULL,
	    !local_mode && (entry == YT_RMT_OUTPUT_LINE
	    || entry == YT_RMT_OUTPUT_BLANK
	    || entry == YT_RMT_OUTPUT_SERIAL_LINE) ? 1U : 0U,
	    payload, payload_length, punctuation))
		return false;
	result->length = length;
	return true;
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

void
yt_rmt_completion_delay(void)
{
	/* RMT-INIT:2305..232D: FOR scratch = 1 TO 2222, with no body. */
	volatile float scratch = 1.0f;

	while (scratch <= 2222.0f)
		scratch += 1.0f;
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
	size_t normalized_length;
	int written;

	if (first == NULL || last == NULL || names == NULL || credited == NULL
	    || credited_size == 0U
	    || (names->rows == NULL && names->count != 0U))
		return false;
	written = snprintf(normalized, sizeof(normalized), "%s %s", first, last);
	if (written < 0 || (size_t)written >= sizeof(normalized))
		return false;
	qb_title_case(normalized);
	normalized_length = strlen(normalized);
	credited[0] = '\0';
	for (index = 0U; index < names->count; ++index) {
		const struct yt_name_row *row = &names->rows[index];
		size_t first_length = strlen(row->real_first);
		size_t last_length = strlen(row->real_last);

		if (first_length >= normalized_length
		    || last_length != normalized_length - first_length - 1U
		    || memcmp(row->real_first, normalized, first_length) != 0
		    || normalized[first_length] != ' '
		    || memcmp(row->real_last, normalized + first_length + 1U,
		    last_length) != 0)
			continue;
		written = snprintf(credited, credited_size, "%s %s",
		    row->alias_first, row->alias_last);
		if (written < 0 || (size_t)written >= credited_size)
			return false;
	}
	return true;
}

static bool
yt_init_present_number(const struct yt_init_presenter *presenter,
    float value, enum yt_init_output_entry entry, struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0) {
		set_error(error, YT_RANGE, "format YT-INIT number", "");
		return false;
	}
	return yt_init_present_one(presenter, entry,
	    (const uint8_t *)text, (size_t)length, error);
}

static bool
yt_init_present_print_number(const struct yt_init_presenter *presenter,
    float value, enum yt_init_output_entry entry, struct yt_error *error)
{
	char text[32];
	int length = qb_str_single(text, sizeof(text), value);

	if (length < 0 || (size_t)length >= sizeof(text)) {
		set_error(error, YT_RANGE, "format YT-INIT PRINT number", "");
		return false;
	}
	text[length++] = ' ';
	return yt_init_present_one(presenter, entry,
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
	return yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "Starting year:", error)
	    && yt_init_present_number(presenter, config->epoch_year,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "Starting player info:", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of fighters at start:", error)
	    && yt_init_present_number(presenter, config->initial_fighters,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of credits at start:", error)
	    && yt_init_present_number(presenter, config->initial_credits,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of cargo holds at start:", error)
	    && yt_init_present_number(presenter, config->initial_holds,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of days inactivity until an dead player is deleted:", error)
	    && yt_init_present_number(presenter, config->retention_days,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "  Last day maintenance run: Yesterday", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of turns per day:", error)
	    && yt_init_present_number(presenter, config->turns_per_day,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  # of times per day a user may play the lottery:", error)
	    && yt_init_present_number(presenter, config->lottery_plays,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  Xannor Headquarters placed in sector:", error)
	    && yt_init_present_print_number(presenter, config->headquarters,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "  Ports needed to initiate Genesis: 450", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE,
	    "  Maximum Cargo holds set to:", error)
	    && yt_init_present_print_number(presenter, config->maximum_holds,
	    YT_INIT_OUTPUT_LINE, error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "  Local screen on with remote callers: On", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "If at any time you wish to change these settings, run YTCONFIG.",
	    error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "Please input filename for the Scoreboard Bulletin.", error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_LINE,
	    "Include FULL PATH and NAME of file! ([ENTER] for YTSCORE.ASC) : ",
	    error)
	    && yt_init_present_text(presenter, YT_INIT_OUTPUT_INLINE, "-=> ", error);
}
