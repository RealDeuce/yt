#include "yt_session_internal.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_output.h"

#include <stdio.h>
#include <string.h>
static bool
computer_route_cint(struct yt_session *session, float value, int *converted,
    enum yt_basic_fault_site site, const char *operation,
    struct yt_error *error)
{
	bool overflow;
	int32_t result = qb_cint_mode((double)value,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow) {
		session_computer_error(error, YT_RANGE, operation);
		(void)yt_error_attach_basic_fault_number(error, site, 6U);
		return false;
	}
	*converted = (int)result;
	return true;
}

bool
yt_session_computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error)
{
	static const uint8_t start_prompt[] = "Enter start for path search? ";
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t same[] = "Hey, look out the window dummy!";
	static const uint8_t route_failure[] =
	    "*** You can't get there without going someplace you dont want to!";
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
	float maximum;
	float start_value;
	float destination_value;
	float hop_count;
	bool stale_marker = autopilot && session->navigation.route_marker == 9999.0f;
	int start;
	int destination;
	int count = session_sector_count(session);
	bool conversion_overflow;
	bool found;
	int cursor;
	enum yt_route_outcome route_outcome;

	if (!autopilot) {
		session->navigation.route_marker = 9999.0f;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_present_timed_paged_row(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response, &start_value, error))
			return false;
		session->navigation.route_start_sector = start_value;
	}
	else if (!stale_marker)
		session->navigation.route_start_sector = session->player.sector;
	start_value = session->navigation.route_start_sector;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response, &destination_value, error))
		return false;
	if (!yt_computer_path_maximum(session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	if (destination_value < 1.0f || destination_value > maximum
	    || start_value < 1.0f || start_value > maximum) {
		char number[64];
		char notice[128];

		if (qb_str_single(number, sizeof(number), maximum) < 0
		    || snprintf(notice, sizeof(notice),
		    "Valid sector numbers are from 1 to%s!", number) < 0)
			return false;
		return session_present_alert(session, (const uint8_t *)notice,
		    strlen(notice), "path invalid endpoint", error);
	}
	if (start_value == destination_value)
		return session_present_alert(session, same, sizeof(same) - 1U,
		    "path equal endpoint", error);
	start = (int)qb_cint_mode((double)start_value,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow)
		return false;
	destination = (int)qb_cint_mode((double)destination_value,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow)
		return false;
	if (start < 0 || start > count || destination < 0
	    || destination > count)
		return true;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path working blank", error)
	    || !session_present_timed_paged_row(session, working,
	    sizeof(working) - 1U, "path working prompt", error))
		return false;
	session->shared_status = 1.0f;
	if (!yt_session_build_route(session, start_value, destination_value,
	    &route, true, &found, &route_outcome, &session->shared_status,
	    error))
		return false;
	if (!found) {
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path failure first blank", error)
		    && session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path failure second blank", error)
		    && session_present_text(session, route_failure,
		    sizeof(route_failure) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "path route failure", error);
	}
	{
		char start_text[64];
		char destination_text[64];
		char heading[192];

		if (qb_str_single(start_text, sizeof(start_text), start_value) < 0
		    || qb_str_single(destination_text, sizeof(destination_text),
		    destination_value) < 0
		    || snprintf(heading, sizeof(heading),
		    "The shortest path from sector%s to sector%s is:",
		    start_text, destination_text) < 0
		    || !session_present_paged_fragment(session,
		    (const uint8_t *)heading, strlen(heading))
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route blank", error))
			return false;
	}
	cursor = start;
	programmed_moves[0] = '1';
	programmed_moves[1] = '\0';
	programmed_moves_length = 1U;
	hop_count = 0.0f;
	{
		char number[64];

		if (qb_str_single(number, sizeof(number), (float)start) < 0
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)number, strlen(number),
		    "path start token", error))
			return false;
	}
	for (;;) {
		char number[64];
		char token[80];
		int column;
		int display_index;
		int ignored_row;
		int program_vertex;
		int16_t next;

		if (!computer_route_cint(session, (float)cursor, &display_index,
		    YT_BASIC_FAULT_ROUTE_DISPLAY_VERTEX_CINT,
		    "route display vertex CINT", error))
			return false;
		next = route.next_hop[display_index];
		if (next == 0)
			break;
		cursor = next;
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0
		    || snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)token, strlen(token),
		    "path route token", error))
			return false;
		if (!computer_route_cint(session, (float)cursor, &program_vertex,
		    YT_BASIC_FAULT_ROUTE_PROGRAM_VERTEX_CINT,
		    "course-program vertex CINT", error))
			return false;
		if (!yt_computer_path_append_hop(programmed_moves,
		    sizeof(programmed_moves), &programmed_moves_length,
		    (float)program_vertex, &hop_count, error))
			return false;
		yt_out_cursor_position(&ignored_row, &column);
		if (yt_computer_path_wrap_required(column)
		    && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route wrap", error))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path token terminator", error))
		return false;
	{
		char hop_text[64];
		char course[128];

		if (qb_str_single(hop_text, sizeof(hop_text), hop_count) < 0
		    || snprintf(course, sizeof(course),
		    "Course will take%s turns.", hop_text) < 0
		    || !session_present_paged_line(session,
		    (const uint8_t *)course, strlen(course),
		    "path course row", error))
			return false;
	}
	session->navigation.route_marker = 0.0f;
	if (!autopilot || stale_marker)
		return true;
	if (!session_reload_player(session, error))
		return false;
	if (hop_count > session->player.turns) {
		if (!session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "autopilot insufficient turns", error))
			return false;
	}
	else {
		char turns[64];
		char row[128];

		if (qb_str_single(turns, sizeof(turns), session->player.turns) < 0
		    || snprintf(row, sizeof(row), "You have%s turns left.",
		    turns) < 0
		    || !session_present_paged_fragment(session,
		    (const uint8_t *)row, strlen(row))
		    || !session_confirm(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer == YT_YES_NO_YES) {
			if (!session_present_paged_line(session, engaged,
			    sizeof(engaged) - 1U, "autopilot engaged row", error)
			    || !session_present_paged_line(session, stop_notice,
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
		    (int)session->player.sector, &current_sector,
		    YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET, error))
			return false;
		for (index = 0; index < 6U; ++index)
			session->navigation.current_warps[index] =
			    current_sector.warps[index];
	}
	return true;
}
