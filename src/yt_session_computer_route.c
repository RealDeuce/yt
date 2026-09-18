#include "yt_session_internal.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_output.h"

#include <stdio.h>
#include <string.h>

bool
yt_session_computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error)
{
	static const uint8_t start_prompt[] = "Enter start for path search? ";
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t same[] = "Hey, look out the window dummy!";
	static const uint8_t insufficient[] =
	    "Not enough turns left to autopilot this course!";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t engaged[] = "Autopilot Engaged.";
	static const uint8_t stop_notice[] = "Ctrl-X to Stop";
	enum yt_yes_no_answer answer;
	char response[160];
	char programmed_moves[YT_COMMAND_SIZE];
	size_t programmed_moves_length;
	struct session_route_plan route;
	uint16_t maximum;
	float start_value;
	float destination_value;
	uint16_t hop_count;
	bool reuse_retained_start = autopilot
	    && session->navigation.reuse_route_start;
	int start;
	int destination;
	int cursor;

	if (!autopilot) {
		session->navigation.reuse_route_start = true;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error))
			return false;
		if (!session_present_timed_paged_row(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error))
			return false;
		if (!session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response, &start_value, error))
			return false;
		session->navigation.route_start_sector = start_value;
	}
	else if (!reuse_retained_start)
		session->navigation.route_start_sector =
		    (float)session->player.sector;
	start_value = session->navigation.route_start_sector;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error))
		return false;
	if (!session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error))
		return false;
	if (!session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response, &destination_value, error))
		return false;
	maximum = (uint16_t)session_sector_count(session);
	if (destination_value < 1.0f || destination_value > (float)maximum
	    || start_value < 1.0f || start_value > (float)maximum) {
		char number[64];
		char notice[128];

		if (qb_str_single(number, sizeof(number), (float)maximum) < 0)
			return false;
		if (snprintf(notice, sizeof(notice),
		    "Valid sector numbers are from 1 to%s!", number) < 0)
			return false;
		return session_present_alert(session, (const uint8_t *)notice,
		    strlen(notice), "path invalid endpoint", error);
	}
	if (start_value == destination_value)
		return session_present_alert(session, same, sizeof(same) - 1U,
		    "path equal endpoint", error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path working blank", error))
		return false;
	if (!session_present_timed_paged_row(session, working,
	    sizeof(working) - 1U, "path working prompt", error))
		return false;
	if (!yt_session_build_route(session, start_value, destination_value,
	    true, &route, error))
		return false;
	if (route.outcome == YT_ROUTE_NOT_FOUND)
		return true;
	start = route.start;
	destination = route.destination;
	{
		char start_text[64];
		char destination_text[64];
		char heading[192];

		if (qb_str_single(start_text, sizeof(start_text), start_value) < 0)
			return false;
		if (qb_str_single(destination_text, sizeof(destination_text),
		    destination_value) < 0)
			return false;
		if (snprintf(heading, sizeof(heading),
		    "The shortest path from sector%s to sector%s is:",
		    start_text, destination_text) < 0)
			return false;
		if (!session_present_paged_fragment(session,
		    (const uint8_t *)heading, strlen(heading)))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route blank", error))
			return false;
	}
	cursor = start;
	programmed_moves[0] = '1';
	programmed_moves[1] = '\0';
	programmed_moves_length = 1U;
	hop_count = 0U;
	{
		char number[64];

		if (qb_str_single(number, sizeof(number), (float)start) < 0)
			return false;
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)number, strlen(number),
		    "path start token", error))
			return false;
	}
	for (;;) {
		char number[64];
		char token[80];
		int column;
		int ignored_row;
		int next;

		next = route.next_hop[cursor];
		if (next == 0)
			break;
		cursor = next;
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0)
			return false;
		if (snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0)
			return false;
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)token, strlen(token),
		    "path route token", error))
			return false;
		if (!yt_computer_path_append_hop(programmed_moves,
		    sizeof(programmed_moves), &programmed_moves_length,
		    (float)cursor, &hop_count, error))
			return false;
		yt_out_cursor_position(&ignored_row, &column);
		if (yt_computer_path_wrap_required(column)) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "path route wrap", error))
				return false;
		}
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path token terminator", error))
		return false;
	{
		char hop_text[64];
		char course[128];

		if (qb_str_single(hop_text, sizeof(hop_text), (float)hop_count) < 0)
			return false;
		if (snprintf(course, sizeof(course),
		    "Course will take%s turns.", hop_text) < 0)
			return false;
		if (!session_present_paged_line(session,
		    (const uint8_t *)course, strlen(course),
		    "path course row", error))
			return false;
	}
	session->navigation.reuse_route_start = false;
	if (!autopilot || reuse_retained_start)
		return true;
	if (!session_reload_player(session, error))
		return false;
	if ((float)hop_count > session->player.turns) {
		if (!session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "autopilot insufficient turns", error))
			return false;
	}
	else {
		char turns[64];
		char row[128];

		if (qb_str_single(turns, sizeof(turns), session->player.turns) < 0)
			return false;
		if (snprintf(row, sizeof(row), "You have%s turns left.",
		    turns) < 0)
			return false;
		if (!session_present_paged_fragment(session,
		    (const uint8_t *)row, strlen(row)))
			return false;
		if (!session_confirm(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer == YT_YES_NO_YES) {
			if (!session_present_paged_line(session, engaged,
			    sizeof(engaged) - 1U, "autopilot engaged row", error))
				return false;
			if (!session_present_paged_line(session, stop_notice,
			    sizeof(stop_notice) - 1U,
			    "autopilot stop row", error))
				return false;
			if (!yt_input_queue_prepend_program(session->io.typeahead,
			    sizeof(session->io.typeahead), &session->io.typeahead_position,
			    &session->io.typeahead_length, programmed_moves,
			    programmed_moves_length))
				return false;
		}
	}
	{
		struct yt_sector current_sector;
		size_t index;

		if (!session_read_sector_at_fault(session,
		    session->player.sector, &current_sector,
		    YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET, error))
			return false;
		for (index = 0; index < 6U; ++index)
			session->navigation.current_warps[index] =
			    current_sector.warps[index];
	}
	return true;
}
