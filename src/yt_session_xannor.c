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
xannor_victory_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	struct yt_text_input input;
	bool result = false;

	yt_text_input_init(&input);
	if (!yt_text_input_open(&input, path, error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;

		if (!yt_text_input_read_line(&input, &line, &length, &available,
		    error))
			goto done;
		if (!available)
			break;
		if (!session_present_text(session, line, length,
		    SESSION_PRESENT_LINE, "Xannor victory file row", error))
			goto done;
	}
	result = yt_text_input_close(&input, error);

done:
	yt_text_input_destroy(&input);
	return result;
}

bool
yt_session_launch_xannor_retaliation(struct yt_session *session,
    int *provoking_player, struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_sector headquarters;
	float saved_cloak;
	int saved_record;
	int target_candidate;
	float target;
	float projectile_amount;
	int amount;
	int ignored_counterattack = 0;
	char amount_text[64];
	char target_text[64];
	char row[192];
	int sector_count = session_sector_count(session);
	bool valid_cache;
	bool cache_cleared = false;
	bool result = false;

	if (provoking_player == NULL)
		return false;
	*provoking_player = session->projectile.pending_xannor_provoker;
	if (session->projectile.pending_xannor_provoker == 0
	    && session->player.score < 25000000.0f) {
		result = true;
		goto done;
	}
	if (!session_read_sector(session,
	    (int)session->door->game.config.headquarters, &headquarters, error))
		goto done;
	if (headquarters.fighters == 0.0f
	    || headquarters.fighter_owner != -1) {
		result = true;
		goto done;
	}
	if (!yt_random_nested_integer(&session->door->game.random, 3, 100,
	    &amount, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "Xannor retaliation blank", error))
		goto done;

	saved_player = session->player;
	saved_record = session_record(session);
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		saved_cloak = yt_player_cache_cloak(&session->player_cache,
		    saved_record);
		if (session->projectile.pending_xannor_provoker != 0) {
			(void)yt_player_cache_set_cloak(&session->player_cache,
			    saved_record, 0.0f);
			cache_cleared = true;
		}
	}
	(void)snprintf(session->player.name, sizeof(session->player.name), "%s",
	    "The Xannor");
	session->active_player_record = -1;

	if (!yt_random_integer(&session->door->game.random, sector_count,
	    &target_candidate, error))
		goto done;
	target = (float)target_candidate;
	if (session->projectile.pending_xannor_provoker != 0)
		target = (float)saved_player.sector;
	if (qb_str_single(amount_text, sizeof(amount_text), (float)amount) < 0
	    || qb_str_single(target_text, sizeof(target_text), target) < 0
	    || snprintf(row, sizeof(row),
	    "The Xannor have launched%s missiles at sector%s!",
	    amount_text, target_text) < 0)
		goto done;
	projectile_amount = (float)amount;
	if (!session_present_text(session, (const uint8_t *)row, strlen(row),
	    SESSION_PRESENT_BOLD_LINE, "Xannor retaliation row", error)
	    || !session_launch_projectile(session,
	    &session->door->game.config.headquarters, &target,
	    &projectile_amount, false, &ignored_counterattack,
	    &session->projectile.pending_xannor_provoker, error))
		goto done;

	session->active_player_record = saved_record;
	session->player = saved_player;
	if (valid_cache && cache_cleared)
		(void)yt_player_cache_set_cloak(&session->player_cache,
		    saved_record, saved_cloak);
	if (!yt_game_read_player(&session->door->game, saved_record,
	    &session->player, error))
		goto done;
	if (session->player.killed_by != 0)
		session->destroyed = true;
	if (!session_wait(session, 4.0, "Xannor retaliation wait", error))
		goto done;
	session->projectile.pending_xannor_provoker = 0;
	result = true;

done:
	*provoking_player = session->projectile.pending_xannor_provoker;
	return result;
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
	session->presentation.blink = true;
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
		if (!session_sound(session, YT_SOUND_CUE_ATTACK, "Xannor victory sound", error))
			return false;
	}

	memset(banner, '*', sizeof(banner));
	player_name_length = yt_player_stored_name(&session->player,
	    player_name);
	if (!yt_xannor_victory_winner(player_name, player_name_length,
	    winner, sizeof(winner), &winner_length)
	    || !yt_news_append_bytes(banner, sizeof(banner), error)
	    || !yt_news_append_bytes(winner, winner_length, error)
	    || !yt_news_append_bytes(banner, sizeof(banner), error)
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
