#include "yt_names.h"
#include "qb.h"
#include "yt_file.h"
#include "yt_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void
yt_name_row_free(struct yt_name_row *row)
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

static char *
duplicate_bytes(const uint8_t *source, size_t length)
{
	char *copy;

	if ((source == NULL && length != 0U) || length == SIZE_MAX)
		return NULL;
	copy = malloc(length + 1U);
	if (copy == NULL)
		return NULL;
	if (length != 0U)
		memcpy(copy, source, length);
	copy[length] = '\0';
	return copy;
}

bool
yt_names_read_row(struct yt_text_input *input,
    struct yt_name_row *row, size_t *staged_count, struct yt_error *error)
{
	char **fields[4];
	size_t field;

	if (row != NULL)
		memset(row, 0, sizeof(*row));
	if (staged_count != NULL)
		*staged_count = 0U;
	if (input == NULL || row == NULL || staged_count == NULL) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	fields[0] = &row->real_first;
	fields[1] = &row->real_last;
	fields[2] = &row->alias_first;
	fields[3] = &row->alias_last;
	for (field = 0U; field < 4U; ++field) {
		const uint8_t *value;
		size_t length;
		bool available;

		if (!yt_text_input_read_string_token(input, &value, &length,
		    &available, error))
			return false;
		if (!available) {
			if (error != NULL) {
				error->status = YT_EOF;
				snprintf(error->operation, sizeof(error->operation),
				    "YTNAME INPUT past end");
			}
			return false;
		}
		*fields[field] = duplicate_bytes(value, length);
		if (*fields[field] == NULL) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		*staged_count = field + 1U;
	}
	return true;
}

bool
yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error)
{
	struct yt_text_input input;
	struct yt_name_row staged = {0};
	struct yt_name_row *grown;
	size_t staged_count = 0U;
	bool eof;
	bool result = false;

	if (path == NULL || names == NULL) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	memset(names, 0, sizeof(*names));
	yt_text_input_init(&input);
	if (!yt_text_input_open(&input, path, error))
		goto done;
	for (;;) {
		if (!yt_text_input_eof(&input, &eof, error))
			goto done;
		if (eof)
			break;
		if (!yt_names_read_row(&input, &staged, &staged_count, error))
			goto done;
		if (names->count == SIZE_MAX / sizeof(*names->rows)) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			goto done;
		}
		grown = realloc(names->rows,
		    (names->count + 1U) * sizeof(*names->rows));
		if (grown == NULL) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			goto done;
		}
		names->rows = grown;
		names->rows[names->count++] = staged;
		memset(&staged, 0, sizeof(staged));
		staged_count = 0U;
	}
	result = yt_text_input_close(&input, error);

done:
	yt_text_input_destroy(&input);
	yt_name_row_free(&staged);
	if (!result) {
		yt_names_free(names);
	}
	return result;
}

void
yt_names_free(struct yt_name_file *names)
{
	size_t index;

	for (index = 0; index < names->count; ++index)
		yt_name_row_free(&names->rows[index]);
	free(names->rows);
	names->rows = NULL;
	names->count = 0;
}

static bool
names_output_value(struct yt_text_output *output,
    const uint8_t *data, size_t length, bool newline,
    struct yt_error *error)
{
	static const uint8_t row_end[] = {'\r', '\n'};

	if (!yt_text_output_write(output, data, length, error)
	    || (newline && !yt_text_output_write(output, row_end,
	    sizeof(row_end), error)))
		return false;
	return true;
}

bool
yt_names_write(const char *path, const struct yt_name_file *names,
    struct yt_error *error)
{
	static const uint8_t comma[] = ",";
	struct yt_text_output output;
	size_t row;
	bool result = false;

	if (path == NULL || names == NULL
	    || (names->rows == NULL && names->count != 0U)) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, path, error))
		goto done;
	for (row = 0U; row < names->count; ++row) {
		const struct yt_name_row *item = &names->rows[row];

		if (output.file == NULL || item->real_first == NULL
		    || item->real_last == NULL || item->alias_first == NULL
		    || item->alias_last == NULL) {
			if (error != NULL)
				error->status = YT_INVALID;
			goto done;
		}
		if (!names_output_value(&output,
		    (const uint8_t *)item->real_first,
		    strlen(item->real_first), false, error)
		    || !names_output_value(&output, comma, sizeof(comma) - 1U, false,
		    error)
		    || !names_output_value(&output,
		    (const uint8_t *)item->real_last,
		    strlen(item->real_last), false, error)
		    || !names_output_value(&output, comma, sizeof(comma) - 1U, false,
		    error)
		    || !names_output_value(&output,
		    (const uint8_t *)item->alias_first,
		    strlen(item->alias_first), false, error)
		    || !names_output_value(&output, comma, sizeof(comma) - 1U, false,
		    error)
		    || !names_output_value(&output,
		    (const uint8_t *)item->alias_last,
		    strlen(item->alias_last), true, error))
			goto done;
	}
	result = yt_text_output_close(&output, error);

done:
	yt_text_output_destroy(&output);
	return result;
}

static bool
fixed_field_contains(const uint8_t field[YT_TEXT_FIELD_SIZE],
    const uint8_t *needle, size_t length)
{
	size_t index;

	if (length == 0U)
		return true;
	if (length > YT_TEXT_FIELD_SIZE)
		return false;
	for (index = 0U; index + length <= YT_TEXT_FIELD_SIZE; ++index) {
		if (memcmp(field + index, needle, length) == 0)
			return true;
	}
	return false;
}

bool
yt_names_propagate_alias(struct yt_database *database,
    const uint8_t *old_alias, size_t old_alias_length,
    const uint8_t *new_alias, size_t new_alias_length,
    struct yt_error *error)
{
	int basic;

	if (database == NULL || (old_alias == NULL && old_alias_length != 0U)
	    || (new_alias == NULL && new_alias_length != 0U)
	    || new_alias_length > YT_TEXT_FIELD_SIZE) {
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	for (basic = 2; basic <= 51; ++basic) {
		struct yt_record record;

		if (!yt_database_read(database, (size_t)basic, &record, error))
			return false;
		if (!fixed_field_contains(record.bytes, old_alias,
		    old_alias_length))
			continue;
		yt_record_set_text(&record, new_alias, new_alias_length);
		if (!yt_record_set_number(&record, YT_F85,
		    (float)new_alias_length)) {
			if (error != NULL)
				error->status = YT_RANGE;
			return false;
		}
		if (!yt_database_write(database, (size_t)basic, &record, error))
			return false;
	}
	return true;
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
