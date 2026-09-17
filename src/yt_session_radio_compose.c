#include "yt_session_internal.h"

#include "qb.h"
#include "yt_output.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
radio_player_search(struct yt_session *session, const char *query,
    int *selected, struct yt_error *error)
{
	int basic;

	*selected = 0;
	if (query[0] == '\0')
		return true;
	for (basic = YT_PLAYER_FIRST_RECORD;
	    basic <= session_sector_offset(session); ++basic) {
		struct yt_player player;
		enum yt_yes_no_answer answer;
		uint8_t prompt[YT_TEXT_FIELD_SIZE + sizeof(" [Y]? ") - 1U];
		size_t prompt_length;

		if (!yt_game_read_player(&session->door->game, basic, &player,
		    error))
			return false;
		if (player.name_length == 0U
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
	int recipients[4] = {0};
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
		recipients[0] = -2;
		recipient_count = 1;
		all = true;
	}
	else if (strcmp(target, "Team") == 0) {
		static const uint8_t teamless[] =
		    "You Don't belong to a team!";

		if (!session_reload_player(session, error))
			return false;
		if (session->player.team == 0) {
			return session_present_alert(session, teamless,
			    sizeof(teamless) - 1U, "radio teamless row", error);
		}
		if (!yt_session_load_team_cache(session, session->player.team,
		    session_record(session), NULL, NULL, NULL, error))
			return false;
		for (index = 0; index < 4; ++index)
			recipients[index] = session->team_cache.roster[index];
		recipient_count = 4;
	}
	else {
		int selected;

		if (!radio_player_search(session, target, &selected, error))
			return false;
		if (selected == 0)
			return true;
		recipients[0] = selected;
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

		if (all || recipients[index] == 0)
			continue;
		if (!yt_game_read_player(&session->door->game, recipients[index],
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
				session_set_pager_line_count(session, 0.0f);
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
		if (!yt_news_append_bytes(news, length, error))
			return false;
	}
	for (index = 0; index < recipient_count; ++index) {
		int body;

		if (recipients[index] == 0)
			continue;
		for (body = 0; body < line_count; ++body) {
			size_t length = strlen(lines[body]);

			if (all) {
				static const uint8_t prefix[] = "  -  ";
				uint8_t news[sizeof(prefix) - 1U + 75U];

				memcpy(news, prefix, sizeof(prefix) - 1U);
				memcpy(news + sizeof(prefix) - 1U, lines[body],
				    length);
				if (!yt_news_append_bytes(news,
				    sizeof(prefix) - 1U + length, error))
					return false;
			}
			if (!session_append_radio_bytes(
			    (const uint8_t *)lines[body], length,
			    (float)session_record(session),
			    (float)recipients[index], error))
				return false;
		}
	}
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session,
	    (const uint8_t *)"Transmission successful!",
	    strlen("Transmission successful!"));
}
