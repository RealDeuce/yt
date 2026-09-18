#include "yt_portname.h"

#include "qb.h"
#include "yt_random.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define YT_NAME_TOKENS 908

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

struct port_name_token {
	const char *text;
	size_t length;
	bool leading;
};

struct port_name_pool {
	bool ready;
	struct port_name_token tokens[YT_NAME_TOKENS];
};

static struct port_name_pool prepared_port_names;

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

static bool
prepare_port_name_pool(void)
{
	const char *cursor = port_name_blob;
	size_t index;

	if (prepared_port_names.ready)
		return true;
	for (index = 0; index < YT_NAME_TOKENS; ++index) {
		size_t length;

		if (*cursor == '\0')
			return false;
		length = strlen(cursor);
		prepared_port_names.tokens[index].text = cursor;
		prepared_port_names.tokens[index].length = length;
		prepared_port_names.tokens[index].leading = length > 4U;
		cursor += length + 1U;
	}
	if (*cursor != '\0')
		return false;
	prepared_port_names.ready = true;
	return true;
}

bool
yt_generate_port_name(struct yt_random *random, char name[42],
    struct yt_error *error)
{
	float first;
	float second;
	int parts;
	int part;
	size_t used = 0;

	if (!prepare_port_name_pool()) {
		set_error(error, YT_INVALID, "compiled port-name pool", "");
		return false;
	}
	if (!yt_random_next(random, &first, error))
		return false;
	if (!yt_random_next(random, &second, error))
		return false;
	parts = (int)floorf(qb_single_multiply(qb_single_multiply(first, second),
	    3.0f)) + 2;
	name[0] = '\0';
	for (part = 0; part < parts; ++part) {
		const struct port_name_token *token;
		const char *cursor;
		float sample;
		int selected;
		size_t length;

		if (!yt_random_next(random, &sample, error))
			return false;
		selected = (int)floorf(qb_single_multiply(sample,
		    (float)YT_NAME_TOKENS));
		token = &prepared_port_names.tokens[selected];
		cursor = token->text;
		length = token->length;
		if (token->leading && used < 41U)
			name[used++] = ' ';
		if (length > 0U && used < 41U) {
			name[used++] = token->leading
			    ? (char)((uint8_t)cursor[0] & 0xdfU) : cursor[0];
			++cursor;
			--length;
		}
		while (length-- > 0U && used < 41U)
			name[used++] = *cursor++;
	}
	if (used > 0U && name[0] == ' ') {
		--used;
		memmove(name, name + 1, used);
	}
	name[used] = '\0';
	if (used > 0U)
		name[0] = (char)((uint8_t)name[0] & 0xdfU);
	return true;
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
    uint16_t logical_port, const uint8_t *name, size_t name_length,
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
		    (float)logical_port);

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
