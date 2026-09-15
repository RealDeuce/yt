#include "yt_session_internal.h"

#include "qb.h"
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
    const uint8_t sender_raw[4], const uint8_t recipient_raw[4],
    struct yt_error *error)
{
	static const uint8_t personal_counter[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t broadcast_counter[4] = {0x00, 0x00, 0x70, 0x85};
	struct yt_radio_file file;
	struct yt_radio_record record;
	struct yt_error close_error;
	uint32_t basic_record;
	const uint8_t *counter_raw;

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)
	    || !yt_radio_file_next_record(&file, &basic_record, error)
	    || !yt_radio_file_get(&file, basic_record, &record, NULL,
	    error))
		goto failed;
	memset(&record, 0, sizeof(record));
	counter_raw = qb_mbf32_decode(recipient_raw) == -2.0f
	    ? broadcast_counter : personal_counter;
	if ((text == NULL && length != 0U)
	    || !yt_radio_set_raw_number(&record, 0U, counter_raw)
	    || !yt_radio_set_raw_number(&record, 4U, recipient_raw)
	    || !yt_radio_set_raw_number(&record, 8U, sender_raw)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "construct radio record");
		}
		goto failed;
	}
	yt_radio_set_text(&record, text, length, 74U);
	if (!yt_radio_file_put(&file, basic_record, &record, error)
	    || !yt_radio_file_close(&file, error))
		return false;
	return true;

failed:
	yt_error_clear(&close_error);
	(void)yt_radio_file_close(&file, &close_error);
	return false;
}

bool
session_append_radio_bytes(const uint8_t *text, size_t length, float sender,
    float recipient, struct yt_error *error)
{
	uint8_t sender_raw[4];
	uint8_t recipient_raw[4];

	if (qb_mbf32_encode(sender, sender_raw) != QB_MBF_OK
	    || qb_mbf32_encode(recipient, recipient_raw) != QB_MBF_OK) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "construct radio record");
		}
		return false;
	}
	return radio_append_raw_bytes(text, length, sender_raw, recipient_raw,
	    error);
}
