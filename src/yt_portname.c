#include "yt_portname.h"

#include "qb.h"
#include "yt_init.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const uint8_t portname_intro[] =
    "\r"
    "          Yankee Trader Remote Port Rename Program\r"
    "                     By Alan Davenport\r"
    "\r"
    "\r"
    "This program will apply new, random port names to an existing game without\r"
    "effecting any other setting. Do you wish to continue? [y/N] -=> ";
static const uint8_t portname_missing[] =
    "\r\r\aERROR! DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
static const uint8_t portname_abort[] = "\rAborted!\r";
static const uint8_t portname_renaming[] = "\r\rRenaming ports...\r";
static const uint8_t portname_complete[] =
    "\rNew, random names applied to all ports!\r";

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

static float
single_add(float left, float right)
{
	volatile float value = left + right;

	return value;
}

static float
single_sub(float left, float right)
{
	volatile float value = left - right;

	return value;
}

enum yt_portname_confirmation
yt_portname_confirm(const uint8_t *response, size_t length)
{
	if (response == NULL || length == 0U)
		return YT_PORTNAME_CONFIRM_BLANK;
	return (response[0] & 0xdfU) == 'Y'
	    ? YT_PORTNAME_CONFIRM_ACCEPT : YT_PORTNAME_CONFIRM_REJECT;
}

enum yt_portname_parse_result
yt_portname_parse_confirmation(const uint8_t *input, size_t input_length,
    uint8_t *value, size_t value_capacity, size_t *value_length)
{
	size_t start = 0U;
	size_t end;
	size_t length;

	if ((input == NULL && input_length != 0U) || value == NULL
	    || value_length == NULL)
		return YT_PORTNAME_PARSE_RANGE;
	while (start < input_length && (input[start] == ' '
	    || input[start] == '\t' || input[start] == '\n'))
		++start;
	if (start < input_length && input[start] == '"') {
		++start;
		end = start;
		while (end < input_length && input[end] != '"'
		    && input[end] != 0U)
			++end;
		length = end - start;
		if (end < input_length && input[end] == '"')
			++end;
		while (end < input_length && (input[end] == ' '
		    || input[end] == '\t' || input[end] == '\n'))
			++end;
		if (end < input_length && input[end] != 0U)
			return YT_PORTNAME_PARSE_REDO;
	}
	else {
		end = start;
		while (end < input_length && input[end] != ','
		    && input[end] != 0U)
			++end;
		if (end < input_length && input[end] == ',')
			return YT_PORTNAME_PARSE_REDO;
		while (end > start && input[end - 1U] == ' ')
			--end;
		length = end - start;
	}
	if (length > value_capacity)
		return YT_PORTNAME_PARSE_RANGE;
	if (length != 0U)
		memcpy(value, input + start, length);
	*value_length = length;
	return YT_PORTNAME_PARSE_VALID;
}

static bool
copy_output(struct yt_portname_output *output, const uint8_t *data,
    size_t length)
{
	if (length > sizeof(output->bytes))
		return false;
	if (length != 0U)
		memcpy(output->bytes, data, length);
	output->length = length;
	return true;
}

bool
yt_portname_compose_output(enum yt_portname_output_kind kind,
    float logical_port, const uint8_t *name, size_t name_length,
    struct yt_portname_output *output)
{
	const uint8_t *fixed = NULL;
	size_t fixed_length = 0U;

	if (output == NULL || (name == NULL && name_length != 0U))
		return false;
	memset(output, 0, sizeof(*output));
	switch (kind) {
	case YT_PORTNAME_OUTPUT_INTRO:
		fixed = portname_intro;
		fixed_length = sizeof(portname_intro) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_MISSING_DATA:
		fixed = portname_missing;
		fixed_length = sizeof(portname_missing) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_ABORT:
		fixed = portname_abort;
		fixed_length = sizeof(portname_abort) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_RENAMING:
		fixed = portname_renaming;
		fixed_length = sizeof(portname_renaming) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_COMPLETE:
		fixed = portname_complete;
		fixed_length = sizeof(portname_complete) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_PROGRESS: {
		char number[64];
		int number_length = qb_print_single(number, sizeof(number),
		    logical_port);

		if (number_length < 0 || name_length + (size_t)number_length + 1U
		    > sizeof(output->bytes))
			return false;
		memcpy(output->bytes, number, (size_t)number_length);
		memcpy(output->bytes + number_length, name, name_length);
		output->bytes[(size_t)number_length + name_length] = '\r';
		output->length = (size_t)number_length + name_length + 1U;
		return true;
	}
	default:
		return false;
	}
	return copy_output(output, fixed, fixed_length);
}

uint32_t
yt_portname_record_number(float port_offset, float logical_port)
{
	float expression = single_add(port_offset, logical_port);

	return qb_brun_random_record_number(expression);
}

bool
yt_portname_overlay_record(struct yt_record *record, const uint8_t *name,
    size_t name_length, struct yt_error *error)
{
	if (record == NULL || (name == NULL && name_length != 0U)
	    || name_length > YT_TEXT_FIELD_SIZE) {
		set_error(error, YT_INVALID, "PORTNAME record overlay", "YTDATA.DAT");
		return false;
	}
	yt_record_set_text(record, name, name_length);
	if (!yt_record_set_number(record, YT_F85, (float)name_length)) {
		set_error(error, YT_RANGE, "encode PORTNAME name length",
		    "YTDATA.DAT");
		return false;
	}
	return true;
}

static bool
emit(enum yt_portname_output_kind kind, float logical_port,
    const uint8_t *name, size_t name_length, yt_portname_output_fn output,
    void *context, struct yt_error *error)
{
	struct yt_portname_output composed;

	return yt_portname_compose_output(kind, logical_port, name, name_length,
	    &composed)
	    && output(context, composed.bytes, composed.length, error);
}

bool
yt_portname_rename(struct yt_database *database, float port_offset,
    float planet_offset, struct yt_random *random,
    yt_portname_output_fn output, void *output_context,
    struct yt_portname_result *result, struct yt_error *error)
{
	struct yt_portname_result local = {0};
	float logical = 1.0f;
	uint64_t starting_draws;

	if (database == NULL || database->file == NULL || random == NULL
	    || output == NULL || result == NULL) {
		set_error(error, YT_INVALID, "PORTNAME rename", "YTDATA.DAT");
		return false;
	}
	starting_draws = random->draws;
	local.loop_bound = single_sub(planet_offset, port_offset);
	if (!emit(YT_PORTNAME_OUTPUT_RENAMING, 0.0f, NULL, 0U, output,
	    output_context, error))
		return false;
	while (logical <= local.loop_bound) {
		struct yt_record record;
		char generated[42];
		const uint8_t *name;
		size_t name_length;
		uint32_t physical;
		float next;

		if (logical == 1.0f) {
			name = (const uint8_t *)"Earth";
			name_length = 5U;
		}
		else {
			if (!yt_generate_port_name(random, generated, error))
				return false;
			name = (const uint8_t *)generated;
			name_length = strlen(generated);
		}
		if (!emit(YT_PORTNAME_OUTPUT_PROGRESS, logical, name, name_length,
		    output, output_context, error))
			return false;
		physical = yt_portname_record_number(port_offset, logical);
		if (!yt_database_read(database, (size_t)physical, &record, error)
		    || !yt_portname_overlay_record(&record, name, name_length, error)
		    || !yt_database_write(database, (size_t)physical, &record, error))
			return false;
		++local.iterations;
		next = single_add(logical, 1.0f);
		if (next == logical) {
			set_error(error, YT_RANGE, "PORTNAME FOR variable stalled", "");
			return false;
		}
		logical = next;
	}
	if (!emit(YT_PORTNAME_OUTPUT_COMPLETE, 0.0f, NULL, 0U, output,
	    output_context, error))
		return false;
	yt_database_close(database);
	/* Authorized departure: retain PLAY ordering without host audio. */
	local.play_event = true;
	local.final_logical_port = logical;
	local.draws_consumed = random->draws - starting_draws;
	*result = local;
	return true;
}
