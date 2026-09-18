#include "yt_session_internal.h"

#include "yt_file.h"

#include <stdio.h>
#include <string.h>

int
session_radio_body_key(struct yt_session *session)
{
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!yt_input_wait(&session->io.input, &selected))
			return EOF;
		if (selected.length == 1)
			return selected.bytes[0];
	}
}


static bool
radio_append_raw_bytes(const uint8_t *text, size_t length,
    int8_t sender, int8_t recipient, struct yt_error *error)
{
	static const uint8_t personal_counter[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t broadcast_counter[4] = {0x00, 0x00, 0x70, 0x85};
	struct yt_radio_file file;
	struct yt_radio_record record;
	struct yt_error close_error;
	uint32_t basic_record;
	const uint8_t *counter_raw;

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error))
		goto failed;
	if (!yt_radio_file_next_record(&file, &basic_record, error))
		goto failed;
	if (!yt_radio_file_get(&file, basic_record, &record, NULL, error))
		goto failed;
	memset(&record, 0, sizeof(record));
	counter_raw = recipient == -2
	    ? broadcast_counter : personal_counter;
	if (text == NULL && length != 0U)
		goto invalid_record;
	if (!yt_radio_set_raw_number(&record, 0U, counter_raw))
		goto invalid_record;
	if (!yt_radio_set_number(&record, 4U, (float)recipient))
		goto invalid_record;
	if (!yt_radio_set_number(&record, 8U, (float)sender))
		goto invalid_record;
	yt_radio_set_text(&record, text, length, 74U);
	if (!yt_radio_file_put(&file, basic_record, &record, error))
		return false;
	if (!yt_radio_file_close(&file, error))
		return false;
	return true;

invalid_record:
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation),
		    "construct radio record");
	}
failed:
	yt_error_clear(&close_error);
	(void)yt_radio_file_close(&file, &close_error);
	return false;
}

bool
session_append_radio_bytes(const uint8_t *text, size_t length, int8_t sender,
    int8_t recipient, struct yt_error *error)
{
	return radio_append_raw_bytes(text, length, sender, recipient, error);
}
