#include "yt_session_internal.h"

#include "qb.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lockout_files {
	struct yt_database random;
	struct yt_text_input input;
};

static bool
lockout_line_matches(const uint8_t *line, size_t length,
    const uint8_t *identity, size_t identity_length, bool *matched,
    struct yt_error *error)
{
	uint8_t *canonical;
	size_t canonical_length;

	*matched = false;
	canonical = malloc(length == 0U ? 1U : length);
	if (canonical == NULL) {
		if (error != NULL) {
			error->status = YT_NO_MEMORY;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "canonicalize lockout row");
		}
		return false;
	}
	if (length != 0U)
		memcpy(canonical, line, length);
	canonical_length = qb_title_case_n(canonical, length);
	*matched = canonical_length == identity_length
	    && (canonical_length == 0U
	    || memcmp(canonical, identity, canonical_length) == 0);
	free(canonical);
	return true;
}

static bool
lockout_close_current(struct lockout_files *files, struct yt_error *error)
{
	if (files->random.file != NULL)
		return yt_database_random_close(&files->random, error);
	return yt_text_input_close(&files->input, error);
}

bool
yt_session_check_lockout(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t revoked[] =
	    "\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a";
	struct lockout_files files = {0};
	uint8_t identity[300];
	size_t identity_length;
	char contact[320];
	uint32_t size;
	bool result = false;

	if (!yt_startup_canonical_name(
	    (const uint8_t *)session->door->identity.real_first,
	    strlen(session->door->identity.real_first),
	    (const uint8_t *)session->door->identity.real_last,
	    strlen(session->door->identity.real_last), identity,
	    sizeof(identity), &identity_length))
		return false;
	(void)snprintf(contact, sizeof(contact),
	    "Please contact your sysop %s %s.",
	    session->door->identity.sysop_first,
	    session->door->identity.sysop_last);
	yt_text_input_init(&files.input);
	if (!yt_database_open(&files.random, "LOCKOUT.DAT",
	    YT_OPEN_UPDATE_CREATE, error))
		goto done;
	if (!yt_database_random_lof(&files.random, &size, error))
		goto done;
	if (!lockout_close_current(&files, error))
		goto done;
	if (size == 0U) {
		result = true;
		goto done;
	}
	if (!yt_text_input_open(&files.input, "LOCKOUT.DAT", error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool matched;

		if (!yt_text_input_read_line(&files.input, &line, &length,
		    &available, error))
			goto done;
		if (!available)
			break;
		if (!lockout_line_matches(line, length, identity,
		    identity_length, &matched, error))
			goto done;
		if (!matched)
			continue;
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "lockout blank", error))
			goto done;
		if (!session_present_text(session, revoked,
		    sizeof(revoked) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "lockout revoked row", error))
			goto done;
		if (!session_present_text(session, (const uint8_t *)contact,
		    strlen(contact), SESSION_PRESENT_BOLD_LINE,
		    "lockout contact row", error))
			goto done;
		if (!session_wait(session, 10.0, "lockout denial wait", error))
			goto done;
		result = lockout_close_current(&files, error);
		session_close_game(session);
		if (!result)
			goto done;
		session->running = false;
		session->terminated = true;
		result = false;
		goto done;
	}
	result = lockout_close_current(&files, error);

done:
	yt_database_close(&files.random);
	yt_text_input_destroy(&files.input);
	return result;
}
