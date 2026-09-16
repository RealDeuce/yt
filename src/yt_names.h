#ifndef YT_NAMES_H
#define YT_NAMES_H

#include "yt_common.h"
#include "yt_data.h"

struct yt_name_row {
	char *real_first;
	char *real_last;
	char *alias_first;
	char *alias_last;
};

struct yt_name_file {
	struct yt_name_row *rows;
	size_t count;
};

struct yt_name_input_observation {
	struct yt_name_row staged;
	size_t staged_count;
	size_t cursor;
};

struct yt_text_input;
struct yt_database;
struct yt_text_output;

enum yt_names_sequential_operation {
	YT_NAMES_SEQUENTIAL_NONE,
	YT_NAMES_SEQUENTIAL_OPEN,
	YT_NAMES_SEQUENTIAL_EOF,
	YT_NAMES_SEQUENTIAL_TOKEN,
	YT_NAMES_SEQUENTIAL_STORE_TOKEN,
	YT_NAMES_SEQUENTIAL_STORE_ROW,
	YT_NAMES_SEQUENTIAL_CLOSE,
};

struct yt_names_sequential_state {
	enum yt_names_sequential_operation failed_operation;
	size_t eof_checks;
	size_t token_reads;
	size_t rows_committed;
	bool file_opened;
	bool close_attempted;
	bool file_closed;
	bool complete;
};

enum yt_names_output_operation {
	YT_NAMES_OUTPUT_NONE,
	YT_NAMES_OUTPUT_OPEN,
	YT_NAMES_OUTPUT_SELECT,
	YT_NAMES_OUTPUT_REAL_FIRST,
	YT_NAMES_OUTPUT_COMMA_1,
	YT_NAMES_OUTPUT_REAL_LAST,
	YT_NAMES_OUTPUT_COMMA_2,
	YT_NAMES_OUTPUT_ALIAS_FIRST,
	YT_NAMES_OUTPUT_COMMA_3,
	YT_NAMES_OUTPUT_ALIAS_LAST_LINE,
	YT_NAMES_OUTPUT_CLOSE,
};

struct yt_names_output_state {
	enum yt_names_output_operation attempted;
	size_t row_index;
	size_t rows_completed;
	size_t values_completed;
	bool file_opened;
	bool close_attempted;
	bool file_closed;
	bool complete;
};

enum yt_alias_key_status {
	YT_ALIAS_KEY_READY,
	YT_ALIAS_KEY_EMPTY,
	YT_ALIAS_KEY_RESERVED,
	YT_ALIAS_KEY_RANGE
};

bool yt_names_load(const char *path, struct yt_name_file *names,
    struct yt_error *error);
bool yt_names_read_sequential_group(struct yt_text_input *input,
	struct yt_name_row *row, size_t *staged_count,
	struct yt_error *error);
void yt_name_row_free(struct yt_name_row *row);
/*
 * Executes the sequential OPEN/EOF/four-token/CLOSE transaction.  On a
 * failure, names retains completed rows and observation retains the staged
 * row prefix; the input object retains the physical carrier state.
 */
bool yt_names_load_sequential(struct yt_text_input *input, const char *path,
	struct yt_name_file *names,
	struct yt_name_input_observation *observation,
	struct yt_names_sequential_state *state, struct yt_error *error);
/*
 * On incomplete input, names retains every completed group and observation
 * owns the successfully staged fields from the interrupted group.  Release
 * both objects even when this function returns false.
 */
bool yt_names_parse_input_groups(const uint8_t *data, size_t length,
    struct yt_name_file *names, struct yt_name_input_observation *observation,
    struct yt_error *error);
void yt_names_input_observation_free(
    struct yt_name_input_observation *observation);
void yt_names_free(struct yt_name_file *names);
bool yt_names_write(const char *path, const struct yt_name_file *names,
    struct yt_error *error);
bool yt_names_write_sequential(struct yt_text_output *output,
	const char *path, const struct yt_name_file *names,
	struct yt_names_output_state *state, struct yt_error *error);
bool yt_names_propagate_alias(struct yt_database *database,
	const uint8_t *old_alias, size_t old_alias_length,
	const uint8_t *new_alias, size_t new_alias_length,
	struct yt_error *error);
bool yt_names_append(const char *path, const struct yt_name_row *row,
    struct yt_error *error);
bool yt_names_set_alias(struct yt_name_file *names, size_t index,
    const char *first, const char *last, struct yt_error *error);
const struct yt_name_row *yt_names_find_real_last(
    const struct yt_name_file *names, const char *first, const char *last);
bool yt_names_alias_exists(const struct yt_name_file *names,
    const char *first, const char *last);
void yt_names_split(const char *name, char *first, size_t first_size,
    char *last, size_t last_size);
enum yt_alias_key_status yt_names_prepare_alias(char *alias,
    size_t alias_size, const char *real_first, const char *real_last,
    char *alias_first, size_t first_size, char *alias_last,
    size_t last_size, char *display, size_t display_size);
bool yt_names_create_default(struct yt_error *error);

#endif
