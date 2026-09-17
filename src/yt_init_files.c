#include "yt_init_internal.h"

#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
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

bool yt_init_write_sequential_file(const char *path, const uint8_t *data,
    size_t length, struct yt_error *error);

static bool
write_banner(const char *credited_name, bool rmt,
    const struct yt_clock *clock, struct yt_error *error)
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

		if (!yt_clock_read(clock, &time_now, error))
			goto failure;
		if (!yt_clock_read(clock, &date_now, error))
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
	if (!yt_init_write_sequential_file("YTNEWS.DAT", data, length, error))
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
clear_yt_radio_messages(struct yt_error *error)
{
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, "YTRMSG.DAT", error))
		goto done;
	if (!yt_text_output_close_all(&output, error))
		goto done;
	if (!yt_file_kill("YTRMSG.DAT", error))
		goto done;
	result = true;

done:
	yt_text_output_destroy(&output);
	return result;
}

bool
yt_init_write_sequential_file(const char *path, const uint8_t *data,
    size_t length, struct yt_error *error)
{
	struct yt_text_output output;
	bool result = false;

	yt_text_output_init(&output);
	if (!yt_text_output_open(&output, path, error))
		goto done;
	if (!yt_text_output_write(&output, data, length, error))
		goto done;
	if (!yt_text_output_close(&output, error))
		goto done;
	result = true;

done:
	yt_text_output_destroy(&output);
	return result;
}

bool
yt_init_write_yt_auxiliary(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_error *error)
{
	static const uint8_t dummy[] = "Dummy,Dummy,Dummy,Dummy\r\n";
	static const uint8_t play[] = "L64cgaL1p1p1p1";

	if (!write_banner(NULL, false, options->clock, error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error))
		return false;
	if (!yt_database_random_close(database, error))
		return false;
	if (!yt_init_write_sequential_file("YTNAME.DAT", dummy,
	    sizeof(dummy) - 1U, error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Clearing YTRMSG.DAT  (Radio message file)", error))
		return false;
	if (!clear_yt_radio_messages(error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Initialization completed sucessfully!", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "<YT-INIT Normal Termination>", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Be SURE to run YTMAINT.EXE at LEAST ONCE per day EVERY DAY!", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Run YTCONFIG and change the default OPTIONS if you wish!", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE, "", error))
		return false;
	if (!yt_present_text(options, YT_INIT_OUTPUT_LINE,
	    "Running initial maintenance...", error))
		return false;
	if (!yt_present(options, YT_INIT_OUTPUT_PLAY, play,
	    sizeof(play) - 1U, error))
		return false;
	return true;
}

bool
yt_init_write_rmt_auxiliary(struct yt_database *database,
    const char *credited_name, const struct yt_initializer_options *options,
    struct yt_error *error)
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

	if (!write_banner(credited_name, true, options->clock, error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Setting up yesterday's newspaper file.", error))
		return false;
	if (!yt_init_write_sequential_file("YTYNEWS.DAT", yesterday,
	    sizeof(yesterday) - 1U, error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
	    "Initializing the alias file (Matches real name to alias.)", error))
		return false;
	if (!yt_database_random_close(database, error))
		return false;
	if (!yt_init_write_sequential_file("YTNAME.DAT", dummy,
	    sizeof(dummy) - 1U, error))
		return false;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		return false;
	if (!rmt_present_text(options, YT_RMT_OUTPUT_LINE,
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
	if (!yt_init_write_sequential_file("YTRMSG.DAT", NULL, 0U, error))
		goto done;
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error))
		goto done;
	for (index = 0; index < 5; ++index) {
		uint32_t record;
		uint64_t size;

		/*
		 * The empty OUTPUT close at 2113 leaves one DOS EOF byte.  The
		 * first LOF/86+1 expression converts that fractional value to
		 * record 1 and overwrites it; the following four LOFs are aligned.
		 */
		if (index == 0) {
			if (!yt_radio_file_size(&file, &size, error)) {
				if (error != NULL && error->status == YT_OK)
					set_error(error, YT_RANGE,
					    "RMT initial radio LOF", "YTRMSG.DAT");
				goto done;
			}
			if (size != 1U) {
				if (error != NULL && error->status == YT_OK)
					set_error(error, YT_RANGE,
					    "RMT initial radio LOF", "YTRMSG.DAT");
				goto done;
			}
			record = 1U;
		}
		else if (!yt_radio_file_next_record(&file, &record, error))
			goto done;
		if (!yt_radio_file_put(&file, record, &radio, error))
			goto done;
	}
	if (!yt_radio_file_close(&file, error))
		goto done;
	if (!rmt_present(options, YT_RMT_OUTPUT_BLANK, NULL, 0U, error))
		goto done;
	valid = true;

done:
	if (file.random.file != NULL)
		(void)yt_radio_file_close(&file, NULL);
	return valid;
}
