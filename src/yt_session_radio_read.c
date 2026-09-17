#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_main_error.h"

#include <stdio.h>
#include <string.h>

static bool
radio_name_bytes(struct yt_session *session, float record, uint8_t *dest,
    size_t capacity, size_t *length, bool sender, struct yt_error *error)
{
	const uint8_t *literal;
	size_t literal_length;

	if (length == NULL)
		return false;
	*length = 0;
	if (record > 0.0f) {
		struct yt_player player;
		uint8_t stored[YT_TEXT_FIELD_SIZE];
		size_t stored_length;

		if (!session_read_player_expression(session, record, &player,
		    error))
			return false;
		stored_length = yt_player_stored_name(&player, stored);
		if (stored_length > capacity)
			goto capacity_error;
		if (stored_length != 0)
			memcpy(dest, stored, stored_length);
		*length = stored_length;
		return true;
	}
	if (!sender) {
		literal = (const uint8_t *)"All";
		literal_length = strlen("All");
	}
	else if (record == -1.0f) {
		literal = (const uint8_t *)"The Xannor";
		literal_length = strlen("The Xannor");
	}
	else {
		literal = (const uint8_t *)"The Mercenaries";
		literal_length = strlen("The Mercenaries");
	}
	if (literal_length > capacity)
		goto capacity_error;
	memcpy(dest, literal, literal_length);
	*length = literal_length;
	return true;

capacity_error:
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio display name capacity");
	}
	return false;
}

static bool
radio_read_attach_fault(struct yt_error *error,
    enum yt_basic_fault_site site, uint16_t basic_error)
{
	if (error == NULL)
		return false;
	if (basic_error == 0U && error->basic_error_valid)
		basic_error = error->basic_error;
	if (basic_error == 0U
	    || !yt_error_attach_basic_fault_number(error, site, basic_error))
		(void)yt_error_attach_basic_fault(error, site);
	return false;
}

bool
yt_session_radio_read(struct yt_session *session, bool log_mode,
    struct yt_error *error)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t log_heading[] =
	    "Log of messages sent/recieved.";
	static const uint8_t pause[] = "[Pause]";
	static const uint8_t none[] = "None Found.";
	struct yt_radio_file file;
	struct yt_radio_pager_state pager;
	uint64_t byte_length;
	uint64_t probe_count;
	float previous_recipient = 0.0f;
	float previous_sender = 0.0f;
	float current_player = (float)session_record(session);
	bool visible = false;
	uint32_t record_number;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "radio opening blank", error)) {
		return radio_read_attach_fault(error,
		    YT_BASIC_FAULT_RADIO_OPENING_OUTPUT, 0U);
	}
	if (!session_present_text(session,
	    log_mode ? log_heading : automatic_heading,
	    log_mode ? sizeof(log_heading) - 1U
	    : sizeof(automatic_heading) - 1U, SESSION_PRESENT_LINE,
	    "radio heading", error)) {
		return radio_read_attach_fault(error,
		    log_mode ? YT_BASIC_FAULT_RADIO_LOG_HEADING_OUTPUT
		    : YT_BASIC_FAULT_RADIO_AUTO_HEADING_OUTPUT, 0U);
	}

	yt_radio_pager_begin(&pager);
	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)) {
		uint16_t basic_error = file.random.last_open.basic_error != 0U
		    ? file.random.last_open.basic_error
		    : file.random.last_close.basic_error;

		return radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_OPEN,
		    basic_error);
	}
	if (!yt_radio_file_size(&file, &byte_length, error)) {
		(void)radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_LOF,
		    file.random.last_lof.basic_error);
		goto abort;
	}
	probe_count = byte_length / YT_RADIO_RECORD_SIZE + 1U;
	if (probe_count > 0xFFFFFFU) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "radio scan bound");
			(void)snprintf(error->path, sizeof(error->path), "%s",
			    file.random.path);
		}
		goto abort;
	}

	for (record_number = 1U; record_number <= probe_count;
	    ++record_number) {
		struct yt_radio_record record;
		struct yt_radio_reader_decision decision;
		float counter;
		float recipient;
		float sender;

		if (!yt_radio_file_get(&file, record_number, &record, NULL,
		    error)) {
			(void)radio_read_attach_fault(error,
			    YT_BASIC_FAULT_RADIO_RECORD_GET,
			    file.random.last_get_basic_error);
			goto abort;
		}
		counter = yt_radio_get_number(&record, 0U);
		recipient = yt_radio_get_number(&record, 4U);
		sender = yt_radio_get_number(&record, 8U);
		if (!yt_radio_reader_decide(counter, recipient, sender,
		    current_player, log_mode ? 1.0f : 0.0f, &decision, error))
			goto abort;
		if (!decision.visible)
			continue;

		{
			uint8_t from[YT_TEXT_FIELD_SIZE];
			uint8_t to[YT_TEXT_FIELD_SIZE];
			uint8_t header[2U * YT_TEXT_FIELD_SIZE + 32U];
			size_t from_length;
			size_t to_length;
			size_t header_length;

			visible = true;
			if (!radio_name_bytes(session, recipient, to, sizeof(to),
			    &to_length, false, error)) {
				if (recipient > 0.0f)
					(void)radio_read_attach_fault(error,
					    YT_BASIC_FAULT_RADIO_RECIPIENT_GET,
					    session->door->game.database.last_get_basic_error);
				goto abort;
			}
			if (!radio_name_bytes(session, sender, from, sizeof(from),
			    &from_length, true, error)) {
				if (sender > 0.0f)
					(void)radio_read_attach_fault(error,
					    YT_BASIC_FAULT_RADIO_SENDER_GET,
					    session->door->game.database.last_get_basic_error);
				goto abort;
			}
			if (sender != previous_sender
			    || recipient != previous_recipient) {
				if (!yt_radio_reader_header(to, to_length, from,
				    from_length, header, sizeof(header),
				    &header_length)
				    || !session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "radio pair blank", error)
				    || !session_present_text(session, header,
				    header_length, SESSION_PRESENT_LINE,
				    "radio pair header", error))
					goto abort;
				yt_radio_pager_add_pair(&pager);
			}
		}

		if (!session_present_text(session, record.bytes + 12U, 74U,
		    SESSION_PRESENT_LINE, "radio body", error))
			goto abort;
		previous_sender = sender;
		previous_recipient = recipient;
		if (yt_radio_pager_add_body(&pager)) {
			if (!session_present_text(session, pause,
			    sizeof(pause) - 1U, SESSION_PRESENT_RAW,
			    "radio pause", error)) {
				(void)radio_read_attach_fault(error,
				    YT_BASIC_FAULT_RADIO_PAUSE_OUTPUT, 0U);
				goto abort;
			}
			if (!session_wait(session, 99.0,
			    "radio private-pager wait", error)) {
				(void)radio_read_attach_fault(error,
				    YT_BASIC_FAULT_RADIO_PRIVATE_WAIT, 0U);
				goto abort;
			}
			if (!session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE, "radio pause blank", error))
				goto abort;
		}
		if (decision.automatic_write
		    && (!yt_radio_reader_mutate(&record, counter)
		    || !yt_radio_file_put(&file, record_number, &record,
		    error)))
			goto abort;
	}

	if (!visible && !session_present_text(session, none,
	    sizeof(none) - 1U, SESSION_PRESENT_LINE, "radio none found",
	    error))
		goto abort;
	if (!yt_radio_file_close(&file, error)) {
		return radio_read_attach_fault(error,
		    YT_BASIC_FAULT_RADIO_FINAL_CLOSE,
		    file.random.last_close.basic_error);
	}
	return true;

abort:
	(void)yt_radio_file_close(&file, NULL);
	return false;
}
