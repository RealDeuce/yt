#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_session_fresh_no_turn_gate(struct yt_session *session, bool *denied,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Sorry but you have no turns left.";

	if (!session_reload_player(session, error))
		return false;
	*denied = yt_no_turn_gate_denied(session->player.turns);
	if (*denied) {
		return session_present_alert(session, notice, sizeof(notice) - 1U,
		    "no-turn gate notice", error);
	}
	return true;
}

bool
yt_session_finalize_action(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t cloak_expired[] = " WARNING! CLOAK EXPIRED!";
	static const float cloak_display_scale = 50.0f;
	static const float turn_divisor = 25.0f;
	static const float xannor_threshold = 0.99f;
	static const uint8_t cloak_dirty_zero[4] = {0x00, 0x00, 0xa3, 0x00};
	int xannor_provoker;
	float quotient;
	float draw;
	char number[64];
	char row[128];
	bool anti_cloak_allows;

	if (!yt_session_spy_sweep(session, error)
	    || !session_reload_player(session, error))
		return false;
	session->player.turns = qb_single_subtract(session->player.turns, 1.0f);
	if (!yt_record_set_number(&session->player.record, YT_F49,
	    session->player.turns))
		return false;
	quotient = qb_single_divide(session->player.turns, turn_divisor);
	anti_cloak_allows = !session->earth.anti_cloak_enabled;
	if (quotient == floorf(quotient) && anti_cloak_allows) {
		float display;
		int saved_foreground;
		int cache_record;

		session->player.cloak = qb_single_subtract(session->player.cloak,
		    0.009999999776482582f);
		if (session->player.cloak < 0.0f) {
			session->player.cloak = 0.0f;
			if (!yt_record_set_raw_number(&session->player.record,
			    YT_F125, cloak_dirty_zero))
				return false;
		}
		else if (!yt_record_set_number(&session->player.record, YT_F125,
		    session->player.cloak))
			return false;
		cache_record = session_record(session);
		(void)yt_player_cache_set_cloak(&session->player_cache,
		    cache_record, session->player.cloak);
		display = floorf(qb_single_multiply(session->player.cloak,
		    cloak_display_scale));
		qb_str_single(number, sizeof(number), display);
		snprintf(row, sizeof(row), "Cloak at%s%%", number);
		saved_foreground = session->presentation.foreground;
		session_set_foreground(session, 7);
		if (!session_present_timed_paged_row(session, (const uint8_t *)row, strlen(row),
		    "action-finalizer cloak row", error))
			return false;
		session_set_foreground(session, saved_foreground);
		if (session->player.cloak == 0.0f) {
			if (!session_attention_bytes(session, cloak_expired,
			    sizeof(cloak_expired) - 1U,
			    "action-finalizer cloak attention", error))
				return false;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer expired trailing blank", error))
				return false;
		}
		else {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer cloak first blank", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer cloak second blank", error))
				return false;
		}
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error))
		return false;
	qb_str_single(number, sizeof(number), session->player.turns);
	snprintf(row, sizeof(row), "One Turn Deducted,%s left.", number);
	if (session->player.turns < 51.0f) {
		session_set_foreground(session, 3);
		session->presentation.bold = true;
		session->presentation.blink = true;
	}
	if (!session_present_paged_fragment(session, (const uint8_t *)row, strlen(row)))
		return false;
	if (!yt_random_next(&session->door->game.random, &draw, error))
		return false;
	if (draw > xannor_threshold) {
		xannor_provoker = session->projectile.pending_xannor_provoker;
		if (!yt_session_launch_xannor_retaliation(session,
		    &xannor_provoker,
		    error))
			return false;
		if (session->destroyed)
			return false;
		if (!session_reload_player(session, error))
			return false;
	}
	return true;
}

bool
yt_session_emergency_warp(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t attention[] = " * EMERGENCY WARP ENGAGED! * ";
	static const uint8_t meltdown[] = "MELT DOWN!";
	static const uint8_t wormhole[] =
	    "You enter a wormhole as your engines build up to emergency power!";
	static const uint8_t temperature[] = "     * Engine Temperature *";
	static const uint8_t scale[] = "[ Normal ][ Danger ][ Overheat ]";
	static const uint8_t ruler[] = "================================";
	static const uint8_t gauge_open[] = "[";
	static const uint8_t gauge_tick[] = "*";
	static const uint8_t relief[] =
	    "You sigh in relief as you look at your scanner and find yourself in";
	static const uint8_t engines_disabled[] = "Your engines are disabled!";
	static const uint8_t repair[] =
	    "It will take a solar day to repair them.";
	float first;
	float second;
	float duration;
	float heat = 0.0f;
	int counter = 1;
	int destination;
	float override;
	float turn_draw;
	float cost;
	uint8_t row[256];
	size_t row_length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp leading blank", error)
	    || !session_attention_bytes(session, attention,
	    sizeof(attention) - 1U,
	    "emergency warp attention", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-title blank", error)
	    || !session_present_text(session, wormhole, sizeof(wormhole) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp wormhole row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp pre-temperature blank", error))
		return false;
	session_set_foreground(session, 6);
	if (!session_present_text(session, temperature,
	    sizeof(temperature) - 1U, SESSION_PRESENT_BOLD_LINE,
	    "emergency warp temperature title", error)
	    || !session_present_text(session, scale, sizeof(scale) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature scale", error))
		return false;
	session_set_foreground(session, 2);
	if (!session_present_text(session, ruler, sizeof(ruler) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature ruler", error))
		return false;
	session_set_foreground(session, 6);
	if (!session_present_text(session, gauge_open,
	    sizeof(gauge_open) - 1U, SESSION_PRESENT_BOLD_RAW,
	    "emergency warp gauge open", error)
	    || !yt_random_next(&session->door->game.random, &first, error)
	    || !yt_random_next(&session->door->game.random, &second, error))
		return false;
	duration = yt_emergency_warp_duration(first, second);
	for (;;) {
		float draw;

		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		if (draw > 0.75f)
			heat = qb_single_add(heat, 1.0f);
		if (heat < 10.0f) {
			session_set_foreground(session, 2);
		}
		else if (heat < 20.0f) {
			session_set_foreground(session, 3);
		}
		else {
			session_set_foreground(session, 1);
			session->presentation.blink = true;
		}
		if (!session_present_text(session, gauge_tick,
		    sizeof(gauge_tick) - 1U, SESSION_PRESENT_BOLD_RAW,
		    "emergency warp gauge tick", error))
			return false;
		if (!session_wait(session, 0.33000001311302185,
		    "emergency-warp heat wait", error))
			return false;
		if (heat >= 31.0f)
			break;
		++counter;
		if ((float)counter > duration)
			break;
	}
	session_set_foreground(session, 2);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank one", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank two", error)
	    || !session_reload_player(session, error))
		return false;
	if (!yt_random_next(&session->door->game.random, &first, error)
	    || !yt_random_next(&session->door->game.random, &override, error)
	    || !yt_random_next(&session->door->game.random, &turn_draw, error))
		return false;
	destination = yt_emergency_warp_destination(first,
	    session_sector_count(session));
	if (override > 0.949999988079071f)
		destination = (int)session->door->game.config.headquarters;
	cost = yt_emergency_warp_cost(heat, turn_draw, session->player.turns,
	    heat >= 31.0f);
	if (heat >= 31.0f) {
		if (!session_attention_bytes(session, meltdown,
		    sizeof(meltdown) - 1U,
		    "meltdown attention", error))
			return false;
		session_set_foreground(session, 1);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown leading blank", error)
		    || !session_present_text(session, engines_disabled,
		    sizeof(engines_disabled) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "meltdown engines-disabled row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown middle blank", error)
		    || !session_present_text(session, repair, sizeof(repair) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "meltdown repair row", error))
			return false;
		for (int ordinal = 0; ordinal < 5; ++ordinal) {
			if (!session_sound(session, YT_SOUND_CUE_DAMAGE,
			    "meltdown sound", error))
				return false;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown trailing blank", error)
		    || !yt_emergency_warp_stranded_row(destination, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "meltdown stranded row", error))
			return false;
	}
	else {
		if (!session_sound(session, YT_SOUND_CUE_REWARD,
		    "emergency warp completion sound", error))
			return false;
		if (!session_present_text(session, relief, sizeof(relief) - 1U,
		    SESSION_PRESENT_LINE, "emergency warp relief row", error)
		    || !yt_emergency_warp_result_row(destination, cost, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "emergency warp result row", error))
			return false;
	}
	yt_emergency_warp_player_overlay(&session->player, destination, cost);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	(void)yt_player_cache_set_sector(&session->player_cache,
	    session_record(session), session->player.sector);
	return true;
}

bool
yt_session_direct_emergency_warp(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t warning_one[] =
	    "This is a desperate move! Your engines will be drained and will take time";
	static const uint8_t warning_two[] =
	    "to recharge! You also risk a melt down! Are you sure you wish to do this?";
	static const uint8_t prompt[] = "[y/N] -=> ";
	enum yt_yes_no_answer answer;
	bool denied;

	if (!yt_session_fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp leading blank", error))
		return false;
	session->presentation.bold = true;
	session_set_foreground(session, 7);
	if (!session_present_paged_fragment(session, warning_one, sizeof(warning_one) - 1U))
		return false;
	session->presentation.bold = true;
	if (!session_present_paged_fragment(session, warning_two, sizeof(warning_two) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp confirmation blank", error))
		return false;
	session->presentation.bold = true;
	if (!session_confirm(session, prompt, sizeof(prompt) - 1U, &answer, error))
		return false;
	if (answer == YT_YES_NO_YES)
		return yt_session_emergency_warp(session, error);
	return true;
}
