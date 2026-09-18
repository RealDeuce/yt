#include "yt_session_internal.h"

#include "yt_main_error.h"
#include "yt_output.h"
#include "yt_platform.h"

#include <stdio.h>
#include <string.h>

void
attach_database_get_fault(struct yt_session *session, struct yt_error *error,
    enum yt_basic_fault_site site)
{
	uint16_t basic_error =
	    session->door->game.database.last_get_basic_error;

	if (basic_error == 0U) {
		(void)yt_error_attach_basic_fault(error, site);
		return;
	}
	if (!yt_error_attach_basic_fault_number(error, site, basic_error))
		(void)yt_error_attach_basic_fault(error, site);
}

void
attach_database_put_fault(struct yt_session *session, struct yt_error *error,
    enum yt_basic_fault_site site)
{
	uint16_t basic_error =
	    session->door->game.database.last_put_basic_error;

	if (basic_error != 0U) {
		if (!yt_error_attach_basic_fault_number(error, site, basic_error))
			(void)yt_error_attach_basic_fault(error, site);
	}
}
bool
session_present_forced_local_line(const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_forced_local_line(text, length, &presentation);
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
session_local_line(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_local_line(text, length,
	    &session->presentation, &presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_commit_shared_terminal(struct yt_session *session,
    const struct yt_shared_error_result *result, struct yt_error *error)
{
	size_t index;

	if (!session_local_line(session, result->debug, result->debug_length,
	    "shared error debug row", error))
		return false;
	for (index = 0U; index < result->event_count; ++index) {
		const struct yt_shared_error_event *event = &result->events[index];

		switch (event->destination) {
		case YT_SHARED_ERROR_LOCAL_DIAGNOSTIC:
			if (!session_local_line(session, event->data,
			    event->length, "shared error local row", error))
				return false;
			break;
		case YT_SHARED_ERROR_SESSION_AND_NEWS:
			if (!session_present_text(session, event->data,
			    event->length, SESSION_PRESENT_LINE,
			    "shared error session row", error))
				return false;
			if (!yt_news_append_bytes(event->data,
			    event->length, error))
				return false;
			break;
		case YT_SHARED_ERROR_NEWS:
			if (!yt_news_append_bytes(event->data,
			    event->length, error))
				return false;
			break;
		}
	}
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	yt_error_clear(error);
	return true;
}

enum session_fault_disposition
session_route_basic_fault(struct yt_session *session, struct yt_error *error)
{
	struct yt_basic_fault_projection projection;
	const struct yt_basic_fault_identity *identity;
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];

	if (error == NULL || !error->basic_fault_valid
	    || !error->basic_error_valid)
		return SESSION_FAULT_UNHANDLED;
	identity = yt_basic_fault_identity(
	    (enum yt_basic_fault_site)error->basic_fault_site);
	if (identity == NULL)
		return SESSION_FAULT_UNHANDLED;
	if (!yt_basic_fault_project(error, (const uint8_t *)error->path,
	    strlen(error->path), NULL, 0U, NULL, 0U, &projection))
		return SESSION_FAULT_UNHANDLED;
	if (projection.disposition == YT_BASIC_FAULT_RETRY_STATEMENT
	    || projection.disposition == YT_BASIC_FAULT_RESUME_MISSING_FILE)
		return SESSION_FAULT_UNHANDLED;
	if (identity->module == YT_BASIC_FAULT_MAIN) {
		if (!session_present_forced_local_line(projection.main.debug,
		    projection.main.debug_length, "main error debug row", error))
			return SESSION_FAULT_HANDLER_FAILED;
		if (projection.disposition == YT_BASIC_FAULT_RESUME_GAMEPLAY)
			return SESSION_FAULT_RESUME_GAMEPLAY;
		if (!yt_clock_read(&session->door->game.clock, &date_now, error))
			return SESSION_FAULT_HANDLER_FAILED;
		if (!yt_clock_read(&session->door->game.clock, &time_now,
		    error))
			return SESSION_FAULT_HANDLER_FAILED;
		yt_format_date(&date_now, date);
		yt_format_time(&time_now, time_text);
		if (!yt_main_error_compose((int16_t)projection.error_number,
		    identity->source_line, (const uint8_t *)error->path,
		    strlen(error->path), (const uint8_t *)date, strlen(date),
		    (const uint8_t *)time_text, strlen(time_text),
		    &projection.main))
			return SESSION_FAULT_HANDLER_FAILED;
		/* The session row completes before ERRORS.DOR is opened. */
		if (!session_present_text(session, projection.main.action,
		    projection.main.action_length, SESSION_PRESENT_LINE,
		    "main error fatal row", error))
			return SESSION_FAULT_HANDLER_FAILED;
		if (!yt_main_error_append_fatal(&projection.main, error))
			return SESSION_FAULT_HANDLER_FAILED;
	}
	else {
		if (!session_commit_shared_terminal(session, &projection.shared,
		    error))
			return SESSION_FAULT_HANDLER_FAILED;
		return SESSION_FAULT_ENDED;
	}
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	yt_error_clear(error);
	return SESSION_FAULT_ENDED;
}

bool
session_handle_gameplay_fault(struct yt_session *session,
    struct yt_error *error, bool *resume_gameplay)
{
	enum session_fault_disposition disposition;

	if (resume_gameplay == NULL)
		return false;
	*resume_gameplay = false;
	if (session->terminated)
		return true;
	disposition = session_route_basic_fault(session, error);
	if (disposition == SESSION_FAULT_ENDED)
		return true;
	if (disposition != SESSION_FAULT_RESUME_GAMEPLAY)
		return false;
	yt_error_clear(error);
	*resume_gameplay = true;
	return true;
}
