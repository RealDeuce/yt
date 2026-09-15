#include "yt_session_internal.h"

#include "qb.h"
#include "yt_output.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
static bool
lottery_settle(struct yt_session *session,
    const struct yt_port *cached_earth, float cost,
    struct yt_error *error)
{
	if (!session_wait(session, 3.0, "lottery caller wait", error)
	    || !session_write_player(session, error))
		return false;
	return session_earth_receipt(session, cached_earth, cost, error);
}

bool
session_earth_lottery(struct yt_session *session, const struct yt_port *cached_earth,
    struct yt_error *error)
{
	char ticket[80];
	char cached_name[sizeof(session->player.name)];
	int winning[6];
	bool matched_winning[6] = {0};
	int matches = 0;
	int index;
	float award = 0.0f;

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (!session_reload_player(session, error))
		return false;
	if (session->player.credits < 5.0f) {
		if (!session_earth_credit_error(session,
		    "You can't afford a lottery ticket!", error))
			return false;
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery limiter leading blank", error))
		return false;
	if (session->door->game.config.lottery_plays == 0.0f) {
		if (!session_present_text(session,
		    (const uint8_t *)
		    "Sorry, The supreme ruler has banned all gambling!",
		    strlen("Sorry, The supreme ruler has banned all gambling!"),
		    SESSION_PRESENT_BOLD_LINE, "lottery disabled row", error))
			return false;
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	{
		char limit[64];
		char row[128];

		if (qb_str_single(limit, sizeof(limit), session->door->game.config.lottery_plays) < 0
		    || snprintf(row, sizeof(row), "You may play%s times daily.",
		    limit) < 0
		    || !session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE, "lottery daily limit row",
		    error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	session->player.lottery_plays =
	    qb_single_add(session->player.lottery_plays, 1.0f);
	if (session->player.lottery_plays > session->door->game.config.lottery_plays) {
		if (!session_present_text(session,
		    (const uint8_t *)
		    "You will be allowed to play again tomorrow.",
		    strlen("You will be allowed to play again tomorrow."),
		    SESSION_PRESENT_BOLD_LINE, "lottery daily reached row", error))
			return false;
		session->player.lottery_plays = qb_single_subtract(
		    session->player.lottery_plays, 1.0f);
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	if (!session_write_player(session, error))
		return false;
	{
		char plays[64];
		char row[128];

		if (qb_str_single(plays, sizeof(plays),
		    session->player.lottery_plays) < 0
		    || snprintf(row, sizeof(row),
		    "You've played%s times already.", plays) < 0
		    || !session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE,
		    "lottery already-played row", error))
			return false;
	}
	if (!yt_session_clearance(session, true, error))
		return false;
	session_set_color(session, 1);
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_present_paged_row(session,
	    (const uint8_t *)"Welcome to the Intergalactic Pick-6 Lottery!",
	    strlen("Welcome to the Intergalactic Pick-6 Lottery!")))
		return false;
	for (;;) {
		bool valid = true;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "lottery ticket leading blank", error))
			return false;
		session_set_color(session, 2);
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)
		    "Enter a 6 digit number for the lottery computer -+>",
		    strlen("Enter a 6 digit number for the lottery computer -+>"),
		    "lottery ticket prompt", error)
		    || !session_read_command(session, ticket, sizeof(ticket)))
			return false;
		if (strlen(ticket) != 6U) {
			if (!session_present_text(session,
			    (const uint8_t *)"That's not 6 digits!",
			    strlen("That's not 6 digits!"), SESSION_PRESENT_LINE,
			    "lottery ticket length row", error))
				return false;
			continue;
		}
		for (index = 0; index < 6; ++index) {
			if (ticket[index] < '0' || ticket[index] > '9')
				valid = false;
		}
		if (!valid) {
			if (!session_present_text(session,
			    (const uint8_t *)"Please enter NUMBERS only!",
			    strlen("Please enter NUMBERS only!"),
			    SESSION_PRESENT_LINE, "lottery ticket digit row", error))
				return false;
			continue;
		}
		break;
	}
	for (index = 0; index < 6; ++index) {
		float draw;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		winning[index] = (int)floorf(qb_single_multiply(draw, 10.0f));
	}
	matches = yt_lottery_match_count(winning, ticket, matched_winning);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery winning display blank", error)
	    || !session_present_text(session,
	    (const uint8_t *)"The Galactic Lottery Computer picked: ",
	    strlen("The Galactic Lottery Computer picked: "),
	    SESSION_PRESENT_RAW, "lottery winning prefix", error))
		return false;
	{
		int saved_foreground = (int)session->foreground;

	for (index = 0; index < 6; ++index) {
		int row;
		int column;
		int dummy;
		uint8_t digit;

		if (!session_wait(session, 0.4000000059604645,
		    "lottery digit pre-roll wait", error))
			return false;
		yt_out_cursor_position(&row, &column);
		session_set_color(session, 6);
		for (dummy = 0; dummy < 18; ++dummy) {
			float draw;
			struct yt_present_result rewind;
			enum yt_present_status status;

			if (!yt_random_next(&session->door->game.random, &draw, error))
				return false;
			digit = (uint8_t)('0' + (int)floorf(
			    qb_single_multiply(draw, 10.0f)));
			if (!session_present_text(session, &digit, 1U,
			    SESSION_PRESENT_RAW, "lottery dummy digit", error))
				return false;
			if (!session_wait(session, 0.004999999888241291,
			    "lottery animation wait", error))
				return false;
			status = yt_present_lottery_rewind(row, column,
			    &session->presentation, &rewind);
			if (status != YT_PRESENT_OK)
				return session_range_error(error,
				    "lottery digit rewind");
			yt_out_present_result(&rewind);
		}
		session_set_color(session, matched_winning[index] ? 3 : 7);
		digit = (uint8_t)('0' + winning[index]);
		if (!session_present_text(session, &digit, 1U,
		    SESSION_PRESENT_RAW, "lottery actual digit", error))
			return false;
	}
	if (!session_wait(session, 0.4000000059604645,
	    "lottery post-digits wait", error))
		return false;
	session_set_color(session, saved_foreground);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery post-digits first blank", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery post-digits second blank", error))
		return false;
	if (matches == 0) {
		if (!session_present_text(session,
		    (const uint8_t *)"Sorry, you didn't win this time.",
		    strlen("Sorry, you didn't win this time."),
		    SESSION_PRESENT_LINE, "lottery loss row", error))
			return false;
		return lottery_settle(session, cached_earth, 5.0f, error);
	}
	{
		char match_text[64];
		char award_text[80];
		int saved_foreground = (int)session->foreground;

		award = yt_lottery_award(matches);
		if (qb_str_single(match_text, sizeof(match_text),
		    (float)matches) < 0
		    || qb_str_double(award_text, sizeof(award_text),
		    (double)award) < 0
		    || !session_present_text(session,
		    (const uint8_t *)"You matched", strlen("You matched"),
		    SESSION_PRESENT_RAW, "lottery award prefix", error))
			return false;
		session_set_color(session, 3);
		if (!session_present_text(session, (const uint8_t *)match_text,
		    strlen(match_text), SESSION_PRESENT_BOLD_RAW,
		    "lottery award matches", error))
			return false;
		session_set_color(session, saved_foreground);
		if (!session_present_text(session,
		    (const uint8_t *)" digits and won", strlen(" digits and won"),
		    SESSION_PRESENT_RAW, "lottery award middle", error))
			return false;
		session_set_color(session, 3);
		if (!session_present_text(session, (const uint8_t *)award_text,
		    strlen(award_text), SESSION_PRESENT_BOLD_RAW,
		    "lottery award value", error))
			return false;
		session_set_color(session, saved_foreground);
		if (!session_present_text(session,
		    (const uint8_t *)" credits!", strlen(" credits!"),
		    SESSION_PRESENT_LINE, "lottery award suffix", error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	for (index = 0; index < matches; ++index) {
		if (!session_sound(session, 1.0f,
		    "lottery award sound", error))
			return false;
	}
	if (matches > 3) {
		char news[300];
		char amount[80];

		if (qb_str_double(amount, sizeof(amount), (double)award) < 0
		    || snprintf(news, sizeof(news),
		    "%s won%s credits in the lottery!", cached_name, amount) < 0)
			return session_range_error(error, "lottery news row");
		if (!yt_news_append(news, error))
			return false;
	}
	if (!session_mutate_player_credits(session, award, NULL, error)
	    || !session_wait(session, 3.0, "lottery award wait", error))
		return false;
	return lottery_settle(session, cached_earth, 5.0f, error);
}


