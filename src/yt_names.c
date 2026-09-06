#include "yt_names.h"
#include "qb.h"
#include "yt_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
input_token(const uint8_t *data, size_t length, size_t *cursor, char **dest,
    struct yt_error *error)
{
	uint8_t *buffer;
	char *value;
	size_t buffered = 0;
	size_t position = *cursor;
	size_t start;
	size_t output;
	uint8_t byte;
	bool provider_quote;

	while (position < length && data[position] == ' ')
		++position;
	if (position >= length) {
		if (error != NULL) {
			error->status = YT_EOF;
			snprintf(error->operation, sizeof(error->operation),
			    "YTNAME INPUT past end");
		}
		return false;
	}
	if (length - position > SIZE_MAX - 2U) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	buffer = malloc(length - position + 2U);
	if (buffer == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	byte = data[position++];
	provider_quote = byte == '"';
	if (provider_quote) {
		buffer[buffered++] = byte;
		while (position < length) {
			byte = data[position++];
			if (byte == 0)
				continue;
			if (byte != '"') {
				buffer[buffered++] = byte;
				continue;
			}
			while (position < length && data[position] == ' ')
				++position;
			if (position < length && data[position] == ',')
				++position;
			else if (position < length && data[position] == '\r') {
				++position;
				if (position < length && data[position] == '\n')
					++position;
			}
			break;
		}
	}
	else {
		for (;;) {
			if (byte == '\r') {
				if (position < length && data[position] == '\n')
					++position;
				break;
			}
			if (byte == '\n') {
				while (position < length && data[position] == '\n')
					++position;
				if (position >= length) {
					buffer[buffered++] = 0x1a;
					break;
				}
				byte = data[position++];
				if (byte == '\r') {
					if (position >= length)
						break;
					byte = data[position++];
				}
				continue;
			}
			if (byte == ',')
				break;
			if (byte != 0)
				buffer[buffered++] = byte;
			if (position >= length)
				break;
			byte = data[position++];
		}
	}
	start = 0;
	while (start < buffered
	    && (buffer[start] == ' ' || buffer[start] == '\t'
	    || buffer[start] == '\n'))
		++start;
	if (start < buffered && buffer[start] == '"') {
		++start;
		output = start;
		while (output < buffered && buffer[output] != '"')
			++output;
	}
	else {
		output = buffered;
		while (output > start && buffer[output - 1U] == ' ')
			--output;
	}
	value = malloc(output - start + 1U);
	if (value == NULL) {
		free(buffer);
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	memcpy(value, buffer + start, output - start);
	value[output - start] = '\0';
	free(buffer);
	*dest = value;
	*cursor = position;
	return true;
}

static void
free_row(struct yt_name_row *row)
{
	free(row->real_first);
	free(row->real_last);
	free(row->alias_first);
	free(row->alias_last);
	memset(row, 0, sizeof(*row));
}

static char *
duplicate_string(const char *source)
{
	size_t length = strlen(source);
	char *copy;

	if (length == SIZE_MAX)
		return NULL;
	copy = malloc(length + 1U);
	if (copy != NULL)
		memcpy(copy, source, length + 1U);
	return copy;
}

bool
yt_names_parse_input_groups(const uint8_t *data, size_t length,
    struct yt_name_file *names, struct yt_name_input_observation *observation,
    struct yt_error *error)
{
	size_t position = 0;

	memset(names, 0, sizeof(*names));
	if (observation != NULL)
		memset(observation, 0, sizeof(*observation));
	if (data == NULL && length != 0U) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	while (position < length) {
		struct yt_name_row staged = {0};
		struct yt_name_row *grown;
		char **fields[4] = {&staged.real_first, &staged.real_last,
		    &staged.alias_first, &staged.alias_last};
		size_t field;

		if (data[position] == 0x1a)
			break;
		for (field = 0; field < 4; ++field) {
			size_t logical_length = position;

			while (logical_length < length
			    && data[logical_length] != 0x1a)
				++logical_length;
			if (!input_token(data, logical_length, &position,
			    fields[field], error)) {
				if (observation != NULL) {
					observation->staged = staged;
					observation->staged_count = field;
					observation->cursor = position;
				}
				else
					free_row(&staged);
				return false;
			}
		}
		if (names->count == SIZE_MAX / sizeof(*names->rows)) {
			if (observation != NULL) {
				observation->staged = staged;
				observation->staged_count = 4U;
				observation->cursor = position;
			}
			else
				free_row(&staged);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		grown = realloc(names->rows,
		    (names->count + 1U) * sizeof(*names->rows));
		if (grown == NULL) {
			if (observation != NULL) {
				observation->staged = staged;
				observation->staged_count = 4U;
				observation->cursor = position;
			}
			else
				free_row(&staged);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		names->rows = grown;
		names->rows[names->count] = staged;
		++names->count;
	}
	if (observation != NULL)
		observation->cursor = position;
	return true;
}

void
yt_names_input_observation_free(
    struct yt_name_input_observation *observation)
{
	if (observation == NULL)
		return;
	free_row(&observation->staged);
	observation->staged_count = 0U;
	observation->cursor = 0U;
}

bool
yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error)
{
	struct yt_text_file text;
	struct yt_name_input_observation observation;
	bool result;

	memset(names, 0, sizeof(*names));
	if (!yt_text_read(path, &text, error))
		return false;
	result = yt_names_parse_input_groups(text.data, text.length, names,
	    &observation, error);
	yt_text_free(&text);
	yt_names_input_observation_free(&observation);
	if (!result)
		yt_names_free(names);
	return result;
}

void
yt_names_free(struct yt_name_file *names)
{
	size_t index;

	for (index = 0; index < names->count; ++index)
		free_row(&names->rows[index]);
	free(names->rows);
	names->rows = NULL;
	names->count = 0;
}

bool
yt_names_write(const char *path, const struct yt_name_file *names,
    struct yt_error *error)
{
	uint8_t *data = NULL;
	size_t length = 0;
	size_t row;
	bool result;

	for (row = 0; row < names->count; ++row) {
		const struct yt_name_row *item = &names->rows[row];
		const char *fields[4];
		size_t field_lengths[4];
		size_t row_length = 5U;
		uint8_t *grown;
		size_t field;
		size_t offset;

		if (item->real_first == NULL || item->real_last == NULL
		    || item->alias_first == NULL || item->alias_last == NULL) {
			free(data);
			if (error != NULL)
				error->status = YT_INVALID;
			return false;
		}
		fields[0] = item->real_first;
		fields[1] = item->real_last;
		fields[2] = item->alias_first;
		fields[3] = item->alias_last;
		for (field = 0; field < 4U; ++field) {
			field_lengths[field] = strlen(fields[field]);
			if (field_lengths[field] > SIZE_MAX - row_length) {
				free(data);
				if (error != NULL)
					error->status = YT_NO_MEMORY;
				return false;
			}
			row_length += field_lengths[field];
		}
		if (row_length > SIZE_MAX - length) {
			free(data);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		grown = realloc(data, length + row_length);

		if (grown == NULL) {
			free(data);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		data = grown;
		offset = length;
		for (field = 0U; field < 4U; ++field) {
			memcpy(data + offset, fields[field], field_lengths[field]);
			offset += field_lengths[field];
			if (field != 3U)
				data[offset++] = ',';
		}
		data[offset++] = '\r';
		data[offset++] = '\n';
		length += row_length;
	}
	result = yt_text_write(path, data, length, true, error);
	free(data);
	return result;
}

bool
yt_names_append(const char *path, const struct yt_name_row *row,
    struct yt_error *error)
{
	const char *fields[4];
	size_t lengths[4];
	size_t length = 3U;
	size_t field;
	size_t offset = 0U;
	uint8_t *line;
	bool result;

	if (row == NULL || row->real_first == NULL || row->real_last == NULL
	    || row->alias_first == NULL || row->alias_last == NULL) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	fields[0] = row->real_first;
	fields[1] = row->real_last;
	fields[2] = row->alias_first;
	fields[3] = row->alias_last;
	for (field = 0U; field < 4U; ++field) {
		lengths[field] = strlen(fields[field]);
		if (lengths[field] > SIZE_MAX - length) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		length += lengths[field];
	}
	line = malloc(length == 0U ? 1U : length);
	if (line == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	for (field = 0U; field < 4U; ++field) {
		memcpy(line + offset, fields[field], lengths[field]);
		offset += lengths[field];
		if (field != 3U)
			line[offset++] = ',';
	}
	result = yt_text_append_line(path, line, length, error);
	free(line);
	return result;
}

bool
yt_names_set_alias(struct yt_name_file *names, size_t index,
    const char *first, const char *last, struct yt_error *error)
{
	char *new_first;
	char *new_last;

	if (names == NULL || index >= names->count || first == NULL
	    || last == NULL) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	new_first = duplicate_string(first);
	if (new_first == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	new_last = duplicate_string(last);
	if (new_last == NULL) {
		free(new_first);
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	free(names->rows[index].alias_first);
	free(names->rows[index].alias_last);
	names->rows[index].alias_first = new_first;
	names->rows[index].alias_last = new_last;
	return true;
}

static bool
joined_name_equal(const char *left_first, const char *left_last,
    const char *right_first, const char *right_last)
{
	size_t left_first_length = strlen(left_first);
	size_t left_last_length = strlen(left_last);
	size_t right_first_length = strlen(right_first);
	size_t right_last_length = strlen(right_last);
	size_t length = left_first_length + 1U + left_last_length;
	size_t index;

	if (length != right_first_length + 1U + right_last_length)
		return false;
	for (index = 0; index < length; ++index) {
		char left = index < left_first_length ? left_first[index]
		    : index == left_first_length ? ' '
		    : left_last[index - left_first_length - 1U];
		char right = index < right_first_length ? right_first[index]
		    : index == right_first_length ? ' '
		    : right_last[index - right_first_length - 1U];

		if (left != right)
			return false;
	}
	return true;
}

const struct yt_name_row *
yt_names_find_real_last(const struct yt_name_file *names, const char *first,
    const char *last)
{
	const struct yt_name_row *match = NULL;
	size_t index;

	for (index = 0; index < names->count; ++index) {
		if (joined_name_equal(names->rows[index].real_first,
		    names->rows[index].real_last, first, last))
			match = &names->rows[index];
	}
	return match;
}

bool
yt_names_alias_exists(const struct yt_name_file *names, const char *first,
    const char *last)
{
	size_t index;

	for (index = 0; index < names->count; ++index) {
		if (joined_name_equal(names->rows[index].alias_first,
		    names->rows[index].alias_last, first, last))
			return true;
	}
	return false;
}

void
yt_names_split(const char *name, char *first, size_t first_size, char *last,
    size_t last_size)
{
	const char *space = strchr(name, ' ');
	size_t length = space == NULL ? strlen(name) : (size_t)(space - name);

	if (length >= first_size)
		length = first_size - 1U;
	memcpy(first, name, length);
	first[length] = '\0';
	snprintf(last, last_size, "%s", space == NULL ? "" : space + 1);
}

enum yt_alias_key_status
yt_names_prepare_alias(char *alias, size_t alias_size,
    const char *real_first, const char *real_last, char *alias_first,
    size_t first_size, char *alias_last, size_t last_size, char *display,
    size_t display_size)
{
	size_t index;
	size_t length;
	int written;

	if (alias_size < 42U || first_size == 0 || last_size == 0
	    || display_size < 41U)
		return YT_ALIAS_KEY_RANGE;
	if (alias[0] == '\0') {
		written = snprintf(alias, alias_size, "%s %s", real_first,
		    real_last);
		if (written < 0 || (size_t)written >= alias_size)
			return YT_ALIAS_KEY_RANGE;
	}
	for (index = 0; alias[index] != '\0'; ++index) {
		if (alias[index] == ',' || alias[index] == '"')
			alias[index] = ' ';
	}
	qb_title_case(alias);
	if (alias[0] == '\0')
		return YT_ALIAS_KEY_EMPTY;
	if (strstr(alias, "Sysop") != NULL
	    || strstr(alias, "Xannor") != NULL
	    || strstr(alias, "Mercenaries") != NULL)
		return YT_ALIAS_KEY_RESERVED;
	length = strlen(alias);
	if (length > 40U)
		length = 40U;
	alias[length++] = ' ';
	alias[length] = '\0';
	yt_names_split(alias, alias_first, first_size, alias_last, last_size);
	qb_title_case(alias_last);
	written = snprintf(display, display_size, "%s %s", alias_first,
	    alias_last);
	if (written < 0 || (size_t)written >= display_size)
		return YT_ALIAS_KEY_RANGE;
	if ((size_t)written > 40U)
		display[40] = '\0';
	return YT_ALIAS_KEY_READY;
}

bool
yt_names_create_default(struct yt_error *error)
{
	static const uint8_t sentinel[] = "Dummy,Dummy,Dummy,Dummy\r\n";

	return yt_text_write("YTNAME.DAT", sentinel, sizeof(sentinel) - 1U,
	    true, error);
}
