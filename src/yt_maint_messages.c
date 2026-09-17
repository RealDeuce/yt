#include "yt_maint.h"

#include "yt_text.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

bool
yt_news_append_bytes(const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return yt_text_append_line("YTNEWS.DAT", text, length, error);
}

bool
yt_news_append(const char *text, struct yt_error *error)
{
	return text != NULL && yt_news_append_bytes(
	    (const uint8_t *)text, strlen(text), error);
}

static bool
news_append_two_values(const char *format, const char *first,
    const char *second, struct yt_error *error)
{
	char line[512];
	int written;

	if (format == NULL || first == NULL || second == NULL) {
		set_error(error, YT_INVALID, "format news", "YTNEWS.DAT");
		return false;
	}
	written = snprintf(line, sizeof(line), format, first, second);
	if (written < 0 || (size_t)written >= sizeof(line)) {
		set_error(error, YT_RANGE, "format news", "YTNEWS.DAT");
		return false;
	}
	return yt_news_append(line, error);
}

bool
yt_news_append_login_bytes(const uint8_t *time_text, size_t time_length,
    const uint8_t *player_name, size_t player_length,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "-=*=- ";
	static const uint8_t separator[] = " ";
	static const uint8_t suffix[] = " Logged on -=*=-";
	uint8_t *row;
	size_t fixed;
	size_t length;
	size_t position = 0U;
	bool result;

	if ((time_text == NULL && time_length != 0U)
	    || (player_name == NULL && player_length != 0U)) {
		set_error(error, YT_INVALID, "format news", "YTNEWS.DAT");
		return false;
	}
	fixed = sizeof(prefix) - 1U + sizeof(separator) - 1U
	    + sizeof(suffix) - 1U;
	if (time_length > SIZE_MAX - fixed
	    || player_length > SIZE_MAX - fixed - time_length) {
		set_error(error, YT_RANGE, "format news", "YTNEWS.DAT");
		return false;
	}
	length = fixed + time_length + player_length;
	row = malloc(length == 0U ? 1U : length);
	if (row == NULL) {
		set_error(error, YT_NO_MEMORY, "allocate news row", "YTNEWS.DAT");
		return false;
	}
#define COPY_LOGIN_PART(data, part_length) do { \
	memcpy(row + position, (data), (part_length)); \
	position += (part_length); \
} while (0)
	COPY_LOGIN_PART(prefix, sizeof(prefix) - 1U);
	if (time_length != 0U)
		COPY_LOGIN_PART(time_text, time_length);
	COPY_LOGIN_PART(separator, sizeof(separator) - 1U);
	if (player_length != 0U)
		COPY_LOGIN_PART(player_name, player_length);
	COPY_LOGIN_PART(suffix, sizeof(suffix) - 1U);
#undef COPY_LOGIN_PART
	result = yt_news_append_bytes(row, position, error);
	free(row);
	return result;
}

bool
yt_news_append_new_player(const char *date_text, const char *player_name,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "-=*=- ";
	static const uint8_t separator[] = " ";
	static const uint8_t suffix[] = " New Player Entered -=*=-";
	uint8_t *row;
	size_t date_length;
	size_t name_length;
	size_t length;
	size_t position = 0U;
	bool result;

	if (date_text == NULL || player_name == NULL) {
		set_error(error, YT_INVALID, "format news", "YTNEWS.DAT");
		return false;
	}
	date_length = strlen(date_text);
	name_length = strlen(player_name);
	length = sizeof(prefix) - 1U + sizeof(separator) - 1U
	    + sizeof(suffix) - 1U;
	if (date_length > SIZE_MAX - length
	    || name_length > SIZE_MAX - length - date_length) {
		set_error(error, YT_RANGE, "format news", "YTNEWS.DAT");
		return false;
	}
	length += date_length + name_length;
	row = malloc(length == 0U ? 1U : length);
	if (row == NULL) {
		set_error(error, YT_NO_MEMORY, "allocate news row", "YTNEWS.DAT");
		return false;
	}
#define COPY_NEWS_PART(data, part_length) do { \
	memcpy(row + position, (data), (part_length)); \
	position += (part_length); \
} while (0)
	COPY_NEWS_PART(prefix, sizeof(prefix) - 1U);
	COPY_NEWS_PART(date_text, date_length);
	COPY_NEWS_PART(separator, sizeof(separator) - 1U);
	COPY_NEWS_PART(player_name, name_length);
	COPY_NEWS_PART(suffix, sizeof(suffix) - 1U);
#undef COPY_NEWS_PART
	result = yt_news_append_bytes(row, position, error);
	free(row);
	return result;
}

bool
yt_news_append_game_full(const char *date_text, const char *player_name,
    struct yt_error *error)
{
	return news_append_two_values(
	    "***%s %s: New player not allowed - game full.",
	    date_text, player_name, error);
}

bool
yt_radio_append_maintenance_bytes(const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error)
{
	struct yt_radio_file file;
	struct yt_radio_record record;
	uint32_t basic_record;

	if (text == NULL && length != 0U)
		return false;
	memset(&record, 0, sizeof(record));
	yt_radio_set_number(&record, 0, recipient == -2.0f ? 20.0f : 1.0f);
	yt_radio_set_number(&record, 4, recipient);
	yt_radio_set_number(&record, 8, sender);
	yt_radio_set_text(&record, text, length, 72);
	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)
	    || !yt_radio_file_next_record(&file, &basic_record, error)
	    || !yt_radio_file_put(&file, basic_record, &record, error)
	    || !yt_radio_file_close(&file, error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	return true;
}

bool
yt_radio_append_maintenance(const char *text, float sender, float recipient,
    struct yt_error *error)
{
	return text != NULL && yt_radio_append_maintenance_bytes(
	    (const uint8_t *)text, strlen(text), sender, recipient, error);
}

bool
yt_radio_compact(struct yt_error *error)
{
	struct yt_text_output temporary;
	struct yt_radio_file destination;
	struct yt_radio_file source;
	struct yt_radio_record input;
	struct yt_radio_record output;
	uint64_t source_length;
	uint64_t record_count;
	uint32_t source_record;
	uint32_t retained_record = 0U;
	bool result = false;

	yt_text_output_init(&temporary);
	yt_radio_file_init(&destination);
	yt_radio_file_init(&source);
	if (!yt_text_output_open(&temporary, "TEMP", error)
	    || !yt_text_output_close(&temporary, error)
	    || !yt_file_kill("TEMP", error)
	    || !yt_radio_file_open_text_width(&destination, "TEMP", 72U, error)
	    || !yt_radio_file_open_text_width(&source, "YTRMSG.DAT", 72U,
	    error)
	    || !yt_radio_file_size(&source, &source_length, error))
		goto done;
	record_count = source_length / YT_RADIO_RECORD_SIZE;
	if (record_count > 0xFFFFFFU) {
		set_error(error, YT_RANGE, "radio compaction bound",
		    "YTRMSG.DAT");
		goto done;
	}
	for (source_record = 1U; source_record <= record_count;
	    ++source_record) {
		size_t accepted = 0U;

		if (!yt_radio_file_get(&source, source_record, &input, &accepted,
		    error))
			goto done;
		if (accepted != sizeof(input.bytes))
			break;
		if (yt_radio_get_number(&input, 0) == 0.0f)
			continue;
		memset(&output, 0, sizeof(output));
		memcpy(output.bytes, input.bytes, 12U);
		memcpy(output.bytes + 12U, input.bytes + 12U, 72U);
		++retained_record;
		if (!yt_radio_file_put(&destination, retained_record, &output,
		    error))
			goto done;
	}
	if (!yt_radio_file_close(&source, error)
	    || !yt_radio_file_close(&destination, error)
	    || !yt_file_kill("YTRMSG.DAT", error)
	    || !yt_file_rename("TEMP", "YTRMSG.DAT", error))
		goto done;
	result = true;

done:
	(void)yt_radio_file_close(&source, NULL);
	(void)yt_radio_file_close(&destination, NULL);
	yt_text_output_destroy(&temporary);
	return result;
}

bool
yt_news_rotate(struct yt_error *error)
{
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (!yt_text_output_open_append(&output, "YTNEWS.DAT", error)
	    || !yt_text_output_close(&output, error))
		goto done;
	/* The second compiled OPEN reuses file number four after its CLOSE. */
	yt_text_output_destroy(&output);
	yt_text_output_init(&output);
	if (!yt_text_output_open_append(&output, "YTYNEWS.DAT", error)
	    || !yt_text_output_close(&output, error)
	    || !yt_file_kill("YTYNEWS.DAT", error)
	    || !yt_file_rename("YTNEWS.DAT", "YTYNEWS.DAT", error))
		goto done;
	result = true;

done:
	yt_text_output_destroy(&output);
	return result;
}

bool
yt_maintenance_write_header(const struct yt_clock *clock,
    struct yt_error *error)
{
	struct yt_clock_value time_value;
	struct yt_clock_value date_value;
	char time_text[9];
	char date_text[11];
	char line[160];

	if (!yt_clock_read(clock, &time_value, error))
		return false;
	yt_format_time(&time_value, time_text);
	if (!yt_clock_read(clock, &date_value, error))
		return false;
	yt_format_date(&date_value, date_text);
	(void)snprintf(line, sizeof(line),
	    "%s %s: Maintenance Program Ran (Revision 03/14/94)",
	    time_text, date_text);
	return yt_news_append(line, error);
}
