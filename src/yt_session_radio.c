#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_main_error.h"
#include "yt_output.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int
session_radio_body_key(struct yt_session *session)
{
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!yt_input_wait(&session->input, &selected))
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


static bool
session_radio_backspace(struct yt_session *session, int line_number,
    size_t shortened_length, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_radio_backspace(line_number,
	    shortened_length, &session->presentation, &presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio body backspace");
	}
	return false;
}

static bool
session_radio_wrap_cleanup(struct yt_session *session, int line_number,
    size_t wrap_marker, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_radio_wrap_cleanup(line_number,
	    wrap_marker, &session->presentation, &presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio body wrap cleanup");
	}
	return false;
}


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
		if (!yt_player_stored_name(&player, stored, &stored_length, error))
			return false;
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
			    file.random.last_get.basic_error);
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
					    session->door->game.database.last_get.basic_error);
				goto abort;
			}
			if (!radio_name_bytes(session, sender, from, sizeof(from),
			    &from_length, true, error)) {
				if (sender > 0.0f)
					(void)radio_read_attach_fault(error,
					    YT_BASIC_FAULT_RADIO_SENDER_GET,
					    session->door->game.database.last_get.basic_error);
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


static bool
radio_player_search(struct yt_session *session, const char *query,
    int *selected, struct yt_error *error)
{
	int basic;

	*selected = 0;
	if (query[0] == '\0')
		return true;
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= (int)session_sector_offset(session); ++basic) {
		struct yt_player player;
		enum yt_yes_no_answer answer;
		uint8_t prompt[YT_TEXT_FIELD_SIZE + sizeof(" [Y]? ") - 1U];
		size_t prompt_length;

		if (!yt_game_read_player(&session->door->game, basic, &player,
		    error))
			return false;
		if (player.record.bytes[YT_F85 + 3U] == 0
		    || !yt_fixed_text_contains(player.record.bytes,
		    (const uint8_t *)query, strlen(query)))
			continue;
		if (!yt_radio_player_prompt(&player, prompt, sizeof(prompt),
		    &prompt_length, error)
		    || !session_confirm(session, prompt, prompt_length, &answer,
		    error))
			return false;
		if (answer != YT_YES_NO_NO) {
			*selected = basic;
			return true;
		}
	}
	return session_present_paged_fragment(session, (const uint8_t *)"Not found.",
	    strlen("Not found."));
}

static bool
radio_line_prompt(struct yt_session *session, int line_number,
    const char *text, struct yt_error *error)
{
	char prompt[96];

	if (snprintf(prompt, sizeof(prompt), " %d:%s", line_number, text) < 0)
		return false;
	return session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "radio body line prompt", error);
}

static bool
radio_edit_draft(struct yt_session *session, char lines[21][76],
    int completed, struct yt_error *error)
{
	char response[80];
	struct qb_val_result parsed;
	char count_text[64];
	char prompt[128];
	int selected;

	if (qb_str_single(count_text, sizeof(count_text), (float)completed) < 0
	    || snprintf(prompt, sizeof(prompt),
	    "Edit Which line? (1 -%s) -=> ", count_text) < 0
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "radio edit line prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "radio edit VAL");
		}
		return false;
	}
	if (!parsed.valid || !isfinite(parsed.value)
	    || floor(parsed.value) < (double)INT_MIN
	    || floor(parsed.value) > (double)INT_MAX)
		selected = 0;
	else
		selected = (int)floor(parsed.value);
	if (selected < 1 || selected > completed) {
		return session_present_alert(session,
		    (const uint8_t *)"INVALID LINE NUMBER!",
		    strlen("INVALID LINE NUMBER!"),
		    "radio edit invalid line", error);
	}

	for (;;) {
		char search[76];
		char replacement[76];
		char changed[152];
		char selected_text[64];
		char heading[128];
		char quoted[160];
		char *match;
		size_t prefix;

		if (qb_str_single(selected_text, sizeof(selected_text),
		    (float)selected) < 0
		    || snprintf(heading, sizeof(heading), "Line%s reads:",
		    selected_text) < 0
		    || snprintf(quoted, sizeof(quoted), "\"%s\"",
		    lines[selected - 1]) < 0
		    || !session_present_paged_line(session, (const uint8_t *)heading,
		    strlen(heading), "radio edit old heading", error)
		    || !session_present_paged_line(session, (const uint8_t *)quoted,
		    strlen(quoted), "radio edit old row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit search blank", error)
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)"Replace what section? -=> ",
		    strlen("Replace what section? -=> "),
		    "radio edit search prompt", error)
		    || !session_read_command(session, search, sizeof(search)))
			return false;
		if (search[0] == '\0')
			return true;
		match = strstr(lines[selected - 1], search);
		if (match == NULL) {
			char missing[256];

			if (snprintf(missing, sizeof(missing),
			    "\"%s\" NOT FOUND in line%s!", search,
			    selected_text) < 0
			    || !session_present_alert(session, (const uint8_t *)missing,
			    strlen(missing), "radio edit search miss", error))
				return false;
			continue;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit replacement blank", error)
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)"Replace it with what? -=> ",
		    strlen("Replace it with what? -=> "),
		    "radio edit replacement prompt", error)
		    || !session_read_command(session, replacement, sizeof(replacement)))
			return false;
		prefix = (size_t)(match - lines[selected - 1]);
		snprintf(changed, sizeof(changed), "%.*s%s%s", (int)prefix,
		    lines[selected - 1], replacement, match + strlen(search));
		changed[74] = '\0';

		for (;;) {
			enum yt_yes_no_answer answer;
			static const uint8_t confirmation[] =
			    "Is this OK? [Y/N]? -=> ";

			if (snprintf(heading, sizeof(heading), "Line%s now reads:",
			    selected_text) < 0
			    || snprintf(quoted, sizeof(quoted), "\"%s\"", changed) < 0
			    || !session_present_paged_line(session, (const uint8_t *)heading,
			    strlen(heading), "radio edit preview heading", error)
			    || !session_present_paged_line(session, (const uint8_t *)quoted,
			    strlen(quoted), "radio edit preview row", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio edit confirm blank", error)
			    || !session_confirm(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer == YT_YES_NO_YES) {
				snprintf(lines[selected - 1], 76, "%s", changed);
				return session_present_paged_line(session,
				    (const uint8_t *)"Change Saved!",
				    strlen("Change Saved!"),
				    "radio edit saved row", error);
			}
			if (answer == YT_YES_NO_NO)
				return session_present_alert(session,
				    (const uint8_t *)"CANCELED!",
				    strlen("CANCELED!"),
				    "radio edit canceled row", error);
		}
	}
}

bool
yt_session_radio_compose(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t warming[] = "Warming up sub-space radio.";
	static const uint8_t target_prompt[] =
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? ";
	static const uint8_t broadcast[] =
	    "This will be a broadcast message to ALL players";
	static const uint8_t limit[] =
	    "   Due to the distances involved, messages are limited to 20 lines.";
	char target[160];
	float recipients[4] = {0};
	int recipient_count = 0;
	char lines[21][76] = {{0}};
	int line_count = 0;
	size_t wrap_marker = 0;
	bool all = false;
	bool send = false;
	int index;

	if (!session_present_paged_line(session, warming, sizeof(warming) - 1U,
	    "radio warmup row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio target blank", error)
	    || !session_present_timed_paged_row(session, target_prompt,
	    sizeof(target_prompt) - 1U, "radio target prompt", error)
	    || !session_read_command(session, target, sizeof(target)))
		return false;
	if (target[0] == '\0')
		return true;
	qb_title_case(target);
	if (strcmp(target, "All") == 0) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio broadcast blank", error)
		    || !session_present_paged_fragment(session, broadcast,
		    sizeof(broadcast) - 1U))
			return false;
		recipients[0] = -2.0f;
		recipient_count = 1;
		all = true;
	}
	else if (strcmp(target, "Team") == 0) {
		static const uint8_t teamless[] =
		    "You Don't belong to a team!";

		if (!session_reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
			return session_present_alert(session, teamless,
			    sizeof(teamless) - 1U, "radio teamless row", error);
		}
		if (!yt_session_load_team_cache(session, (int)session->player.team,
		    session_record(session), NULL, NULL, NULL, error))
			return false;
		for (index = 0; index < 4; ++index)
			recipients[index] =
			    (float)session->team_cache.roster[index];
		recipient_count = 4;
	}
	else {
		int selected;

		if (!radio_player_search(session, target, &selected, error))
			return false;
		if (selected == 0)
			return true;
		recipients[0] = (float)selected;
		recipient_count = 1;
	}
	if (!all && !session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "radio tuning blank", error))
		return false;
	for (index = 0; index < recipient_count; ++index) {
		struct yt_player target_player;
		uint8_t row[sizeof("Tuning in to ") - 1U + YT_TEXT_FIELD_SIZE
		    + sizeof("'s frequency.") - 1U];
		size_t row_length;

		if (all || recipients[index] == 0.0f)
			continue;
		if (!session_read_player_expression(session, recipients[index],
		    &target_player, error)
		    || !yt_radio_tuning_row(&target_player, row, sizeof(row),
		    &row_length, error))
			return false;
		if (!session_present_paged_fragment(session, row, row_length))
			return false;
	}
	if (!session_present_paged_line(session, limit, sizeof(limit) - 1U,
	    "radio line-limit row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio body handoff blank", error))
		return false;

	while (!send) {
		bool menu = false;

		if (line_count >= 20) {
			if (!session_present_alert(session,
			    (const uint8_t *)"Message full!",
			    strlen("Message full!"), "radio message full", error))
				return false;
			menu = true;
		}
		else {
			session_set_pager_line_count(session, 0.0f);
			if (!radio_line_prompt(session, line_count + 1,
			    lines[line_count], error))
				return false;
			while (!menu) {
				int key = session_radio_body_key(session);
				enum yt_radio_body_key_action action;
				size_t length;

				if (key == EOF)
					return false;
				length = strlen(lines[line_count]);
				action = yt_input_radio_body_key((uint8_t)key, length);
				if (action == YT_RADIO_BODY_KEY_COMMIT) {
					if (!session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE, "radio body enter", error))
						return false;
					if (length == 0) {
						if (line_count == 0) {
							if (!session_present_text(session, NULL, 0,
							    SESSION_PRESENT_LINE,
							    "radio first-empty menu blank", error))
								return false;
							return true;
						}
						menu = true;
					}
					else {
						++line_count;
						wrap_marker = 0;
						if (line_count >= 20) {
							if (!session_present_alert(session,
							    (const uint8_t *)"Message full!",
							    strlen("Message full!"),
							    "radio entered message full", error))
								return false;
							menu = true;
						}
						else {
							session_set_pager_line_count(session, 0.0f);
							if (!radio_line_prompt(session,
							    line_count + 1, lines[line_count],
							    error))
								return false;
						}
					}
					continue;
				}
				if (action == YT_RADIO_BODY_KEY_BACKSPACE) {
					lines[line_count][length - 1U] = '\0';
					if (!session_radio_backspace(session,
					    line_count + 1, length - 1U, error))
						return false;
					continue;
				}
				if (action != YT_RADIO_BODY_KEY_PRINTABLE
				    || length >= 75U)
					continue;
				if (key == ' ')
					wrap_marker = length + 1U;
				lines[line_count][length] = (char)key;
				lines[line_count][length + 1U] = '\0';
				if (length + 1U <= 74U) {
					uint8_t byte = (uint8_t)key;

					if (!session_present_text(session, &byte, 1U,
					    SESSION_PRESENT_RAW, "radio body character",
					    error))
						return false;
				}
				if (length + 1U > 74U) {
					size_t split = wrap_marker != 0
					    ? wrap_marker : 74U;
					size_t carry = length + 1U - split;

					if (!session_radio_wrap_cleanup(session,
					    line_count + 1, split, error))
						return false;
					memcpy(lines[line_count + 1],
					    lines[line_count] + split, carry);
					lines[line_count + 1][carry] = '\0';
					lines[line_count][split] = '\0';
					++line_count;
					wrap_marker = 0;
					if (!session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE, "radio body wrap blank",
					    error))
						return false;
					if (line_count >= 20) {
						if (!session_present_alert(session,
						    (const uint8_t *)"Message full!",
						    strlen("Message full!"),
						    "radio wrap message full", error))
							return false;
						menu = true;
					}
					else {
						session_set_pager_line_count(session, 0.0f);
						if (!radio_line_prompt(session,
						    line_count + 1, lines[line_count],
						    error))
							return false;
					}
				}
			}
		}

		while (menu && !send) {
			char choice[80];
			bool abort;
			static const uint8_t menu_prompt[] =
			    "[L] List [S] Send [A] Abort [C] Continue [E] Edit -=> ";

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio menu blank", error)
			    || !session_present_timed_paged_row(session, menu_prompt,
			    sizeof(menu_prompt) - 1U, "radio menu prompt", error)
			    || !session_read_upper_command(session, choice, sizeof(choice)))
				return false;
			if (choice[0] != '\0'
			    && !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio menu dispatch blank", error))
				return false;
			if (strcmp(choice, "L") == 0) {
				static const uint8_t list_dirty_zero[4] = {
					0x00U, 0x00U, 0x80U, 0x00U,
				};

				session_set_pager_line_count_raw(session,
				    list_dirty_zero);
				for (index = 0; index < line_count; ++index) {
					char row[96];

					if (snprintf(row, sizeof(row), " %d:%s", index + 1,
					    lines[index]) < 0
					    || !session_present_paged_fragment(session,
					    (const uint8_t *)row, strlen(row)))
						return false;
				}
			}
			else if (strcmp(choice, "A") == 0) {
				enum yt_yes_no_answer answer;
				static const uint8_t abort_prompt[] =
				    "Are you sure? [y/N]";

				if (!session_confirm(session, abort_prompt,
				    sizeof(abort_prompt) - 1U, &answer, error))
					return false;
				abort = answer == YT_YES_NO_YES;
				if (abort)
					return true;
			}
			else if (strcmp(choice, "C") == 0) {
				if (line_count > 0
				    && lines[line_count - 1][0] == '\0') {
					--line_count;
					if (line_count > 0)
						wrap_marker =
						    strlen(lines[line_count - 1]);
				}
				menu = false;
			}
			else if (strcmp(choice, "E") == 0) {
				if (!radio_edit_draft(session, lines, line_count, error))
					return false;
			}
			else if (strcmp(choice, "S") == 0)
				send = true;
		}
	}
	if (all) {
		static const uint8_t prefix[] = "  -  Message from: ";
		uint8_t news[sizeof(prefix) - 1U + YT_TEXT_FIELD_SIZE];
		size_t length = sizeof(prefix) - 1U
		    + session->cached_player_name_length;

		memcpy(news, prefix, sizeof(prefix) - 1U);
		memcpy(news + sizeof(prefix) - 1U,
		    session->cached_player_name,
		    session->cached_player_name_length);
		if (!session_append_news_bytes(session, news, length, error))
			return false;
	}
	for (index = 0; index < recipient_count; ++index) {
		int body;

		if (recipients[index] == 0.0f)
			continue;
		for (body = 0; body < line_count; ++body) {
			size_t length = strlen(lines[body]);

			if (all) {
				static const uint8_t prefix[] = "  -  ";
				uint8_t news[sizeof(prefix) - 1U + 75U];

				memcpy(news, prefix, sizeof(prefix) - 1U);
				memcpy(news + sizeof(prefix) - 1U, lines[body],
				    length);
				if (!session_append_news_bytes(session, news,
				    sizeof(prefix) - 1U + length, error))
					return false;
			}
			if (!session_append_radio_bytes(
			    (const uint8_t *)lines[body], length,
			    (float)session_record(session), recipients[index], error))
				return false;
		}
	}
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session,
	    (const uint8_t *)"Transmission successful!",
	    strlen("Transmission successful!"));
}

