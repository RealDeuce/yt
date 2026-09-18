#include "yt_session_internal.h"

#include "qb.h"
#include "yt_input.h"
#include "yt_main_error.h"
#include "yt_output.h"
#include "yt_pager.h"
#include "yt_platform.h"
#include "yt_sound.h"
#include "yt_text.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length);

static bool
read_keyboard_line(struct yt_session *session, char *dest, size_t size)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};

	if (size == 0)
		return false;
	yt_pager_editor_enter(&session->pager, session->io.editor_buffer,
	    sizeof(session->io.editor_buffer));
	dest[0] = '\0';
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};
		bool queued = session->io.typeahead_position < session->io.typeahead_length;
		uint8_t key;

		if (queued) {
			if (!yt_input_queue_pop(session->io.typeahead,
			    sizeof(session->io.typeahead), &session->io.typeahead_position,
			    &session->io.typeahead_length, &selected))
				return false;
		}
		else {
			if (!yt_input_wait(&session->io.input, &selected))
				return false;
		}
		if (selected.length != 1)
			continue;
		key = selected.bytes[0];
		if (yt_input_repeat_requested(queued, &selected)) {
			uint8_t prefix[YT_INPUT_PENDING];
			size_t prefix_length =
			    strlen(session->io.editor_buffer);
			size_t saved_length = strlen(session->io.saved_command);

			if (prefix_length != 0U)
				memcpy(prefix, session->io.editor_buffer,
				    prefix_length);
			memcpy(session->io.pending_echo,
			    session->io.editor_buffer, prefix_length + 1U);
			session->pager.newline_flag = true;
			if (!session_present_paged_row(session, prefix,
			    prefix_length))
				return false;
			memmove(session->io.editor_buffer,
			    session->io.saved_command, saved_length + 1U);
			key = '\r';
		}
		if (yt_input_submit_requested(key)) {
			struct yt_present_result presentation;
			enum yt_present_status status;

			session->pager.newline_flag = false;
			status = yt_present_line(NULL, 0,
			    &session->presentation, &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			snprintf(dest, size, "%s", session->io.editor_buffer);
			return true;
		}
		if (key == '\b' && session->io.editor_buffer[0] != '\0') {
			struct yt_present_result presentation;
			enum yt_present_status status;
			size_t length = strlen(session->io.editor_buffer);

			session->io.editor_buffer[length - 1U] = '\0';
			status = yt_present_editor_echo(local_erase,
			    sizeof(local_erase), remote_erase,
			    sizeof(remote_erase), &session->presentation,
			    &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			continue;
		}
		{
			struct yt_present_result presentation;
			enum yt_present_status status;
			size_t length = strlen(session->io.editor_buffer);

			if (key < 0x20U || key > 0x7fU
			    || length + 1U >= sizeof(session->io.editor_buffer)
			    || length + 1U >= size)
				continue;
			status = yt_present_editor_echo(&key, 1U, &key, 1U,
			    &session->presentation, &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			session->io.editor_buffer[length] = (char)key;
			session->io.editor_buffer[length + 1U] = '\0';
			session->io.pending_echo[0] = (char)key;
			session->io.pending_echo[1] = '\0';
			session->pager.newline_flag = true;
			od_kernel();
			continue;
		}
	}
}

void
session_clear_queue(struct yt_session *session)
{
	(void)yt_input_queue_clear(session->io.typeahead, sizeof(session->io.typeahead),
	    &session->io.typeahead_position, &session->io.typeahead_length);
}

static bool
session_command_notice(struct yt_session *session, const char *text)
{
	if (!session_present_paged_line(session, (const uint8_t *)text, strlen(text),
	    "command notice", NULL))
		return false;
	return session_wait(session, 1.0, "command notice wait", NULL);
}

static bool
expand_repeat(struct yt_session *session, char *text, size_t size)
{
	struct yt_repeat_transform result;

	if (!yt_input_expand_repeat_with_notice(text, size,
	    session->io.saved_command, sizeof(session->io.saved_command),
	    session->io.text_workspace, sizeof(session->io.text_workspace), &result)) {
		if (result.fault_valid && session->error != NULL) {
			yt_error_clear(session->error);
			session->error->status = YT_RANGE;
			(void)snprintf(session->error->operation,
			    sizeof(session->error->operation), "%s",
			    result.failure == YT_REPEAT_FAILURE_VAL_OVERFLOW
			    ? "ADE0 repeat VAL overflow"
			    : "ADE0 repeat SINGLE overflow");
			(void)yt_error_attach_basic_fault_number(session->error,
			    result.fault_site, 6U);
		}
		return false;
	}
	if (!result.emit_notice)
		return true;
	if (result.bold_committed)
		session->presentation.bold = true;
	return session_command_notice(session, session->io.text_workspace);
}

static bool
session_line(struct yt_session *session, char *text, size_t size)
{
	bool notice_ready;

	if (!read_keyboard_line(session, text, size))
		return false;
	if (!yt_input_save_command(text, size, session->io.typeahead,
	    sizeof(session->io.typeahead), &session->io.typeahead_position,
	    &session->io.typeahead_length, session->io.saved_command,
	    sizeof(session->io.saved_command), session->io.text_workspace,
	    sizeof(session->io.text_workspace), &notice_ready))
		return false;
	if (notice_ready) {
		if (!session_command_notice(session, session->io.text_workspace))
			return false;
	}
	if (!expand_repeat(session, text, size))
		return false;
	return yt_input_split_semicolon(text, session->io.typeahead,
	    sizeof(session->io.typeahead), &session->io.typeahead_position,
	    &session->io.typeahead_length);
}

bool
session_read_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_line(session, text, size))
		return false;
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

bool
session_read_upper_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_read_command(session, text, size))
		return false;
	yt_input_compat_upper_n((uint8_t *)text, strlen(text));
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

bool
session_read_number_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_read_upper_command(session, text, size))
		return false;
	if (strchr(text, 'E') != NULL)
		text[0] = '\0';
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length)
{
	if ((text == NULL && length != 0U)
	    || length >= sizeof(session->io.text_workspace))
		return false;
	if (length != 0U)
		memmove(session->io.text_workspace, text, length);
	session->io.text_workspace[length] = '\0';
	return true;
}

static bool
session_run_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t notice[] = "Ctrl-X to Stop";
	struct yt_pager_key_state key_state = {
		.accumulator = session->io.editor_buffer,
		.accumulator_capacity = sizeof(session->io.editor_buffer),
		.queue = session->io.typeahead,
		.queue_capacity = sizeof(session->io.typeahead),
		.queue_position = &session->io.typeahead_position,
		.queue_length = &session->io.typeahead_length,
		.pager_key = session->pager.key,
		.pager_key_capacity = sizeof(session->pager.key),
	};
	struct yt_present_result presentation;
	struct yt_input_value sampled;
	char response[80];
	enum yt_present_status status;
	int saved_foreground;
	bool emit_notice;

	od_kernel();
	if (!yt_input_poll(&session->io.input, &sampled))
		return false;
	if (!yt_pager_apply_key(&sampled, &key_state))
		return false;
	status = yt_present_paged_text(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	od_kernel();
	status = yt_present_paged_finish(session->pager.newline_flag,
	    &session->presentation, &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	if (yt_pager_advance(&session->pager, &session->presentation,
	    &saved_foreground)) {
		if (!session_run_paged_row(session, prompt,
		    sizeof(prompt) - 1U))
			return false;
		if (!read_keyboard_line(session, response, sizeof(response)))
			return false;
		emit_notice = yt_pager_accept_response(&session->pager, response,
		    sizeof(response));
		if (emit_notice) {
			if (!session_run_paged_row(session, notice,
			    sizeof(notice) - 1U))
				return false;
		}
		yt_pager_complete(&session->pager, &session->presentation,
		    saved_foreground);
	}
	session->pager.newline_flag = false;
	return true;
}

bool
session_present_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length)
{
	if (!session_store_output_source(session, text, length))
		return false;
	return session_run_paged_row(session, text, length);
}

void
session_set_color(struct yt_session *session, int logical)
{
	static const int pc_color[8] = {0, 4, 2, 6, 1, 5, 3, 7};

	session_set_foreground(session, logical);
	session->presentation.background = 0;
	if (logical >= 0 && logical < 8)
		od_set_color(pc_color[logical], 0);
}

bool
session_sound(struct yt_session *session, enum yt_sound_cue cue,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_sound(cue, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_attention_bytes(struct yt_session *session, const uint8_t *text,
    size_t length,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_attention(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_present_text(struct yt_session *session, const uint8_t *text,
    size_t length, enum session_present_text_kind kind,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	switch (kind) {
	case SESSION_PRESENT_LINE:
		status = yt_present_line(text, length, &session->presentation,
		    &presentation);
		break;
	case SESSION_PRESENT_RAW:
		status = yt_present_character(text, length,
		    &session->presentation, &presentation);
		break;
	case SESSION_PRESENT_BOLD_LINE:
		status = yt_present_bold_line(text, length,
		    &session->presentation, &presentation);
		break;
	case SESSION_PRESENT_BOLD_RAW:
		status = yt_present_bold_character(text, length,
		    &session->presentation, &presentation);
		break;
	default:
		status = YT_PRESENT_RANGE;
		break;
	}
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}


bool
session_present_paged_fragment(struct yt_session *session,
    const uint8_t *text, size_t length)
{
	session->pager.newline_flag = false;
	return session_present_paged_row(session, text, length);
}

bool
session_present_paged_line(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	return session_present_paged_fragment(session, text, length);
}

bool
session_present_alert(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	session->presentation.bold = true;
	session->presentation.blink = true;
	session_clear_queue(session);
	return session_present_paged_fragment(session, text, length);
}

static bool
session_low_time(struct yt_session *session, const char *operation,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	bool warned;

	status = yt_present_low_time(session->time.text,
	    session->time.text_length, &session->low_time_remembered,
	    &session->presentation, &presentation, &warned);
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_present_timed_paged_row(struct yt_session *session,
    const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_low_time(session, operation, error))
		return false;
	session->pager.newline_flag = true;
	return session_present_paged_row(session, text, length);
}

static bool
session_drain_pending_input(struct yt_session *session)
{
	struct yt_input_drain_state drain;
	enum yt_input_drain_reason reason;

	if (!yt_input_drain_begin(&drain, &session->io.drain_residue))
		return false;
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!yt_input_poll_source(&session->io.input, false, &selected))
			return false;
		reason = yt_input_drain_local(&drain, &selected);
		if (reason == YT_INPUT_DRAIN_ERROR)
			return false;
		if (reason == YT_INPUT_DRAIN_LOCAL_COMPLETE)
			break;
	}
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!session->presentation.sound.local_mode) {
			if (!yt_input_poll_source(&session->io.input, true,
			    &selected))
				return false;
		}
		reason = yt_input_drain_serial(&drain,
		    session->presentation.sound.local_mode, &selected);
		if (reason == YT_INPUT_DRAIN_ERROR)
			return false;
		if (reason == YT_INPUT_DRAIN_COMPLETE)
			break;
	}
	memset(&session->io.drain_residue, 0, sizeof(session->io.drain_residue));
	if (drain.residue_length != 0)
		memcpy(session->io.drain_residue.bytes, drain.residue,
		    drain.residue_length);
	session->io.drain_residue.length = drain.residue_length;
	return true;
}

bool
session_press_any_key(struct yt_session *session, bool drain,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	enum yt_status failure_status = YT_RANGE;
	const char *failure_operation = "press any key presentation";
	int saved_foreground;

	if (drain) {
		if (!session_drain_pending_input(session)) {
			failure_status = YT_IO_ERROR;
			failure_operation = "press any key input drain";
			goto failed;
		}
	}
	status = yt_present_press_prompt(&session->presentation,
	    &presentation, &saved_foreground);
	if (status != YT_PRESENT_OK)
		goto failed;
	session_set_foreground(session, 3);
	yt_out_present_result(&presentation);
	if (!yt_input_pause(&session->io.input, 33.0)) {
		failure_status = YT_IO_ERROR;
		failure_operation = "press any key wait";
		goto failed;
	}
	status = yt_present_press_cleanup(saved_foreground,
	    &session->presentation, &presentation);
	if (status != YT_PRESENT_OK)
		goto failed;
	yt_out_present_result(&presentation);
	session_set_foreground(session, saved_foreground);
	return true;

failed:
	if (error != NULL) {
		error->status = failure_status;
		snprintf(error->operation, sizeof(error->operation),
		    "%s", failure_operation);
	}
	return false;
}

bool
session_fixed_width_bytes(struct yt_session *session, const uint8_t *text,
    size_t text_length, float width, const char *operation,
    struct yt_error *error)
{
	uint8_t mutable[256];
	size_t length = text_length;
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (length > sizeof(mutable)
	    || (length != 0 && text == NULL)) {
		status = YT_PRESENT_CAPACITY;
	}
	else {
		if (length != 0)
			memcpy(mutable, text, length);
		status = yt_present_fixed_width(mutable, &length,
		    sizeof(mutable), width, &session->presentation,
		    &presentation);
		if (status == YT_PRESENT_OK)
			yt_out_present_result(&presentation);
	}
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_right_aligned(struct yt_session *session, const char *text,
    float width, const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_right_aligned((const uint8_t *)text, strlen(text),
	    width, &session->presentation, &presentation);
	if (status == YT_PRESENT_OK)
		yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Cntl-X to Stop";
	struct yt_text_input input;
	struct yt_error local_error;
	struct yt_error *active_error = error == NULL ? &local_error : error;
	int saved_foreground = session->presentation.foreground;
	bool ok = false;

	if (error == NULL)
		yt_error_clear(&local_error);
	if (path == NULL) {
		active_error->status = YT_INVALID;
		active_error->system_error = 0;
		snprintf(active_error->operation,
		    sizeof(active_error->operation), "file viewer");
		active_error->path[0] = '\0';
		return false;
	}
	session->pager.key[0] = '\0';
	if (!session_present_paged_line(session, notice, sizeof(notice) - 1U,
	    "file viewer notice", active_error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "file viewer pre-open blank", active_error))
		return false;
	yt_text_input_init(&input);
	if (!yt_text_input_close(&input, active_error))
		goto done;
	session->pager.line_count = 0;
	if (!yt_text_input_open(&input, path, active_error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		int foreground;

		if (!yt_text_input_eof(&input, &eof, active_error))
			goto done;
		if (eof || strcmp(session->pager.key, "Q") == 0)
			break;
		if (!yt_text_input_read_line(&input, &line, &length, &available,
		    active_error))
			goto done;
		if (!available) {
			active_error->status = YT_EOF;
			active_error->system_error = 0;
			snprintf(active_error->operation,
			    sizeof(active_error->operation),
			    "LINE INPUT after EOF check");
			snprintf(active_error->path, sizeof(active_error->path),
			    "%s", path);
			goto done;
		}
		foreground = yt_file_viewer_line_foreground(line, length);
		session_set_foreground(session, foreground);
		if (foreground != 2)
			session->presentation.bold = true;
		if (!session_present_paged_row(session, line, length))
			goto done;
	}
	if (!yt_text_input_close(&input, active_error))
		goto done;
	session->pager.line_count = 0;
	session_set_foreground(session, saved_foreground);
	ok = session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "file viewer final blank", active_error);

done:
	yt_text_input_destroy(&input);
	if (!ok && active_error->status == YT_NOT_FOUND) {
		struct yt_main_error_result handler;
		uint8_t row[sizeof(active_error->path) + 32U];
		size_t row_length;

		if (!yt_main_error_compose(53, 40000,
		    (const uint8_t *)path, strlen(path), NULL, 0U, NULL, 0U,
		    &handler))
			return false;
		if (handler.route != YT_MAIN_ERROR_MISSING_FILE)
			return false;
		if (!session_present_forced_local_line(handler.debug,
		    handler.debug_length, "file viewer missing debug row",
		    active_error))
			return false;
		yt_error_clear(active_error);
		if (!yt_file_viewer_missing_row(path, row, sizeof(row),
		    &row_length))
			return false;
		if (!session_present_paged_line(session, row, row_length,
		    "file viewer missing row", active_error))
			return false;
		return yt_news_append_bytes(row, row_length, active_error);
	}
	return ok;
}

bool
session_confirm(struct yt_session *session, const uint8_t *prompt,
    size_t prompt_length, enum yt_yes_no_answer *answer,
    struct yt_error *error)
{
	uint8_t prompt_scratch[YT_COMMAND_SIZE];
	size_t prompt_scratch_length = prompt_length;

	if (answer == NULL || (prompt == NULL && prompt_length != 0U)
	    || prompt_length > sizeof(prompt_scratch))
		return false;
	if (prompt_length != 0U)
		memcpy(prompt_scratch, prompt, prompt_length);
	for (;;) {
		char response[YT_COMMAND_SIZE];
		enum yt_confirmation_outcome outcome;

		if (!session_present_text(session, prompt_scratch,
		    prompt_scratch_length,
		    SESSION_PRESENT_RAW, "yes/no prompt", error))
			return false;
		if (!session_read_upper_command(session, response,
		    sizeof(response)))
			return false;
		if (!yt_input_confirmation(response, session->io.text_workspace,
		    sizeof(session->io.text_workspace), prompt_scratch,
		    sizeof(prompt_scratch), &prompt_scratch_length, session->io.typeahead,
		    sizeof(session->io.typeahead), &session->io.typeahead_position,
		    &session->io.typeahead_length, &session->presentation.bold,
		    answer, &outcome))
			return false;
		if (outcome == YT_CONFIRMATION_RETURNED)
			return true;
		if (outcome != YT_CONFIRMATION_RETRY)
			return false;
	}
}
