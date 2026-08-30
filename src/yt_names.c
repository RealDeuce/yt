#include "yt_names.h"
#include "qb.h"
#include "yt_text.h"

#include <stdlib.h>
#include <string.h>

static bool
input_token(const uint8_t *data, size_t length, size_t *cursor, char *dest,
    size_t size, struct yt_error *error)
{
	uint8_t *buffer;
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
	if (output - start >= size) {
		free(buffer);
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "YTNAME string length");
		}
		return false;
	}
	memcpy(dest, buffer + start, output - start);
	dest[output - start] = '\0';
	free(buffer);
	*cursor = position;
	return true;
}

bool
yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error)
{
	struct yt_text_file text;
	size_t position = 0;

	memset(names, 0, sizeof(*names));
	if (!yt_text_read(path, &text, error))
		return false;
	while (position < text.length) {
		struct yt_name_row staged;
		struct yt_name_row *grown;
		char *fields[4] = {staged.real_first, staged.real_last,
		    staged.alias_first, staged.alias_last};
		size_t sizes[4] = {sizeof(staged.real_first),
		    sizeof(staged.real_last), sizeof(staged.alias_first),
		    sizeof(staged.alias_last)};
		size_t field;

		if (text.data[position] == 0x1a)
			break;
		for (field = 0; field < 4; ++field) {
			size_t logical_length = position;

			while (logical_length < text.length
			    && text.data[logical_length] != 0x1a)
				++logical_length;
			if (!input_token(text.data, logical_length, &position,
			    fields[field], sizes[field], error)) {
				yt_names_free(names);
				yt_text_free(&text);
				return false;
			}
		}
		grown = realloc(names->rows,
		    (names->count + 1U) * sizeof(*names->rows));
		if (grown == NULL) {
			yt_names_free(names);
			yt_text_free(&text);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		names->rows = grown;
		names->rows[names->count] = staged;
		++names->count;
	}
	yt_text_free(&text);
	return true;
}

void
yt_names_free(struct yt_name_file *names)
{
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
		size_t row_length = strlen(item->real_first) + strlen(item->real_last)
		    + strlen(item->alias_first) + strlen(item->alias_last) + 5U;
		uint8_t *grown = realloc(data, length + row_length + 1U);
		int written;

		if (grown == NULL) {
			free(data);
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		data = grown;
		written = snprintf((char *)data + length, row_length + 1U,
		    "%s,%s,%s,%s\r\n", item->real_first, item->real_last,
		    item->alias_first, item->alias_last);
		if (written < 0 || (size_t)written != row_length) {
			free(data);
			if (error != NULL)
				error->status = YT_INVALID;
			return false;
		}
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
	char line[180];
	int length = snprintf(line, sizeof(line), "%s,%s,%s,%s",
	    row->real_first, row->real_last, row->alias_first, row->alias_last);

	if (length < 0 || (size_t)length >= sizeof(line)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	return yt_text_append_line(path, (const uint8_t *)line, (size_t)length,
	    error);
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
