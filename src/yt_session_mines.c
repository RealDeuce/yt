#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static float
single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
single_sub(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static bool
drop_mines_failure(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

bool
yt_session_command_mines(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_mines[] = "You don't HAVE any!";
	static const uint8_t union_refusal[] =
	    "The Union doesnt like the home 7 sectors mined!";
	static const char prompt_suffix[] =
	    " mines. Drop how many? [0] -=>";
	static const char success_suffix[] = " is now mined!";
	struct qb_val_result parsed;
	struct yt_player player;
	struct yt_sector sector;
	enum qb_mbf_status conversion;
	char response[4096] = {0};
	char number[64];
	uint8_t amount_raw[4];
	uint8_t row[192];
	float amount;
	float carried;
	float remaining;
	int current_sector;
	int number_length;
	size_t row_length;

	if (!session_reload_player(session, error))
		return false;
	player = session->player;
	carried = player.mines;
	current_sector = (int)player.sector;
	if (carried < 0.0f) {
		player.mines = 0.0f;
		if (!yt_record_set_number(&player.record, YT_F129, 0.0f)
		    || !yt_game_write_player(&session->door->game,
		    session_record(session), &player, error)
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
	}
	if (carried < 1.0f)
		return session_present_alert(session, no_mines,
		    sizeof(no_mines) - 1U, "no sector mines", error);
	if (player.sector < 8.0f)
		return session_present_alert(session, union_refusal,
		    sizeof(union_refusal) - 1U, "Union sector mine refusal",
		    error);

	number_length = qb_str_single(number, sizeof(number), carried);
	if (number_length < 0
	    || sizeof("You have") - 1U + (size_t)number_length
	    + sizeof(prompt_suffix) - 1U > sizeof(row))
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines prompt");
	memcpy(row, "You have", sizeof("You have") - 1U);
	row_length = sizeof("You have") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, prompt_suffix, sizeof(prompt_suffix) - 1U);
	row_length += sizeof(prompt_suffix) - 1U;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine prompt blank", error)
	    || !session_present_timed_paged_row(session, row, row_length,
	    "sector mine prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		amount = 0.0f;
	else {
		parsed = qb_val(response);
		if (!parsed.valid || parsed.overflow)
			return drop_mines_failure(error, YT_RANGE,
			    "drop-mines:VAL");
		amount = (float)parsed.value;
	}
	conversion = qb_mbf32_encode(amount, amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines:amount-csng");
	amount = qb_mbf32_decode(amount_raw);
	if (yt_sector_mine_admit(carried, amount) != YT_SECTOR_MINE_ACCEPTED)
		return true;

	session->self_mine_suppressed = true;
	remaining = single_sub(carried, amount);
	player.mines = remaining;
	if (!yt_record_set_number(&player.record, YT_F129, remaining)
	    || !yt_game_write_player(&session->door->game,
	    session_record(session), &player, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !session_read_sector(session, current_sector, &sector, error))
		return false;
	sector.mines = single_add(sector.mines, amount);
	if (!yt_record_set_number(&sector.record, YT_F129, sector.mines)
	    || !session_write_sector(session, current_sector, &sector, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;

	number_length = qb_str_single(number, sizeof(number), player.sector);
	if (number_length < 0
	    || sizeof("Sector") - 1U + (size_t)number_length
	    + sizeof(success_suffix) - 1U > sizeof(row))
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines success row");
	memcpy(row, "Sector", sizeof("Sector") - 1U);
	row_length = sizeof("Sector") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, success_suffix, sizeof(success_suffix) - 1U);
	row_length += sizeof(success_suffix) - 1U;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine success blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session, row, row_length)
	    && session_sound(session, 4.0f, "sector mine sound", error);
}
