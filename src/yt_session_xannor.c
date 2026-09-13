#include "yt_session_internal.h"

#include "yt_text.h"

#include <stdio.h>
#include <string.h>

static bool
xannor_victory_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_INVALID;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
xannor_victory_file_present(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	return session_present_text(context, line, length,
	    SESSION_PRESENT_LINE, "Xannor victory file row", error);
}

static bool
xannor_victory_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	return yt_text_sequential_play(path, xannor_victory_file_present,
	    session, error);
}

bool
yt_session_xannor_victory(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t pause[] = "[PAUSE]";
	static const uint8_t bonus[] =
	    "Collect 16,000,000 credit bonus!";
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t winner[128];
	uint8_t banner[79];
	struct yt_sector sector;
	size_t player_name_length;
	size_t winner_length;
	bool credit_hydrated = false;
	unsigned ordinal;

	session_set_foreground(session, 7.0f);
	if (!xannor_victory_file(session, "XANNORHQ.TXT", error)
	    || !session_present_text(session, pause, sizeof(pause) - 1U,
	    SESSION_PRESENT_RAW, "Xannor victory pause", error)
	    || !session_wait(session, 99.0, "Xannor victory wait", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "Xannor victory post-wait blank", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, bonus, sizeof(bonus) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "Xannor victory bonus", error))
		return false;
	session_clear_queue(session);

	if (!session_mutate_player_credits(session, 16000000.0f,
	    &credit_hydrated, error))
		return false;
	if (!credit_hydrated)
		return xannor_victory_failure(error,
		    "Xannor victory credit hydrate");
	for (ordinal = 0U; ordinal < 3U; ++ordinal) {
		if (!session_sound(session, 2.0f, "Xannor victory sound", error))
			return false;
	}

	memset(banner, '*', sizeof(banner));
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error)
	    || !yt_xannor_victory_winner(player_name, player_name_length,
	    winner, sizeof(winner), &winner_length)
	    || !session_append_news_bytes(session, banner, sizeof(banner), error)
	    || !session_append_news_bytes(session, winner, winner_length, error)
	    || !session_append_news_bytes(session, banner, sizeof(banner), error)
	    || !session_append_radio_bytes(banner, sizeof(banner), -2.0f,
	    -2.0f, error)
	    || !session_append_radio_bytes(winner, winner_length, -2.0f,
	    -2.0f, error)
	    || !session_append_radio_bytes(banner, sizeof(banner), -2.0f,
	    -2.0f, error)
	    || !session_read_sector(session, 21, &sector, error))
		return false;
	sector.metadata = (float)session_record(session);
	return session_write_sector(session, 21, &sector, error);
}
