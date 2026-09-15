#include "yt_session_internal.h"

#include "qb.h"
#include "yt_main_error.h"
#include "yt_output.h"
#include "yt_port_math.h"
#include "yt_score.h"

#include <stdio.h>
#include <string.h>

static bool
computer_error(struct yt_error *error, enum yt_status status,
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

static bool
computer_route_cint(struct yt_session *session, float value, int *converted,
    enum yt_basic_fault_site site, const char *operation,
    struct yt_error *error)
{
	bool overflow;
	int32_t result = qb_cint_mode((double)value,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow) {
		computer_error(error, YT_RANGE, operation);
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
	float maximum;
	float start_value;
	float destination_value;
	float hop_count;
	uint8_t parsed_raw[4];
	uint8_t hop_count_raw[4];
	bool stale_marker = autopilot && session->path_marker == 9999.0f;
	int start;
	int destination;
	int count = session_sector_count(session);
	bool conversion_overflow;
	bool found;
	int cursor;
	enum yt_route_outcome route_outcome;

	if (!autopilot) {
		session->path_marker = 9999.0f;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_present_timed_paged_row(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response, &start_value, parsed_raw,
		    error))
			return false;
		session->route_start = start_value;
	}
	else if (!stale_marker)
		session->route_start = qb_mbf32_decode(
		    session->player.record.bytes + YT_F57);
	start_value = session->route_start;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response, &destination_value, parsed_raw,
	    error))
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
	    NULL, true, &found, &route_outcome, &session->shared_status,
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
	session->computer_route_scratch[0] = '1';
	session->computer_route_scratch[1] = '\0';
	session->computer_route_scratch_length = 1U;
	hop_count = 0.0f;
	(void)qb_mbf32_encode(hop_count, hop_count_raw);
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
		next = session->route_second[display_index];
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
		if (!yt_computer_path_append_hop(
		    session->computer_route_scratch,
		    sizeof(session->computer_route_scratch),
		    &session->computer_route_scratch_length,
		    (float)program_vertex, &hop_count, hop_count_raw, error))
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
	session->path_marker = 0.0f;
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
			if (!yt_input_queue_prepend_program(session->queue,
			    sizeof(session->queue), &session->queue_position,
			    &session->queue_length,
			    session->computer_route_scratch,
			    session->computer_route_scratch_length))
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
			session->current_warps[index] = qb_mbf32_decode(
			    current_sector.record.bytes + YT_F41 + index * 4U);
	}
	return true;
}

static bool
yt_session_computer_owner_is_friendly(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;

	if (friendly == NULL)
		return false;
	*friendly = false;
	session->shared_status = 0.0f;
	if (owner < 2.0f
	    || owner > session_sector_offset(session)
	    || (float)session_record(session) < 2.0f
	    || (float)session_record(session)
	    > session_sector_offset(session))
		return true;
	if (owner == (float)session_record(session)) {
		*friendly = true;
		session->shared_status = -1.0f;
		return true;
	}
	if (!session_read_player_at_fault(session, session_record(session),
	    &current, YT_BASIC_FAULT_PORT_FRIENDSHIP_CURRENT_GET, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!session_read_player_at_fault(session, (int)owner, &other,
	    YT_BASIC_FAULT_PORT_FRIENDSHIP_CANDIDATE_GET, error))
		return false;
	*friendly = other.team == current.team;
	if (*friendly)
		session->shared_status = -1.0f;
	return true;
}

static bool
yt_session_computer_check_port_visibility(struct yt_session *session,
    const struct yt_sector *sector, float cached_team, bool *unavailable,
    struct yt_error *error)
{
	bool friendly;

	if (session == NULL || sector == NULL || unavailable == NULL)
		return false;
	session->path_marker = 0.0f;
	if (!yt_session_computer_owner_is_friendly(session,
	    sector->fighter_owner,
	    &friendly, error))
		return false;
	session->planet_record_expression = yt_port_single_add(
	    session_planet_offset(session), session->inherited_loop_index);
	*unavailable = (sector->port == 0.0f)
	    | (sector->fighters > 0.0f && cached_team > 0.0f && !friendly)
	    | (sector->fighters > 0.0f && cached_team == 0.0f
	    && sector->fighter_owner != (float)session_record(session));
	return true;
}

bool
yt_session_computer_port_report(struct yt_session *session,
    bool *enter_sector, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum;
	float cached_team = session->player.team;
	char response[80];
	float selected;
	int sector_number;
	struct yt_sector sector;
	bool denied;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!yt_computer_port_maximum(session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	for (;;) {
		enum yt_computer_port_selection_route route;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "computer port sector blank", error)
		    || !session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "computer port sector prompt", error)
		    || !session_read_command(session, response, sizeof(response)))
			return false;
		if (!yt_computer_port_select(response, maximum, &selected,
		    &route, error))
			return false;
		if (route == YT_COMPUTER_PORT_SELECTION_EMPTY)
			return true;
		if (route == YT_COMPUTER_PORT_SELECTION_ACCEPTED)
			break;
		{
			char number[64];
			char notice[128];

			if (qb_str_single(number, sizeof(number), maximum) < 0
			    || snprintf(notice, sizeof(notice),
			    "Invalid sector number! Range is 1 -%s", number) < 0
			    || !session_present_alert(session,
			    (const uint8_t *)notice, strlen(notice),
			    "computer port invalid sector", error))
				return false;
		}
	}
	sector_number = (int)selected;
	if (!session_read_sector_at_fault(session, sector_number, &sector,
	    YT_BASIC_FAULT_PORT_SELECTED_SECTOR_GET, error))
		return false;
	if (!yt_session_computer_check_port_visibility(session, &sector,
	    cached_team, &denied, error))
		return false;
	if (denied)
		return session_present_paged_line(session, unavailable,
		    sizeof(unavailable) - 1U,
		    "computer port unavailable", error);
	if (sector.port == 1.0f) {
		struct yt_port earth;
		float price[4];

		if (!session_earth_report(session, &earth, price, error))
			return false;
		session->earth_report_seen = false;
		return true;
	}
	{
		float sector_record_expression = yt_port_selected_expression(
		    session_sector_offset(session), (float)sector_number);
		struct yt_port_market_state market;

		return yt_session_update_port(session, sector_number,
		    &sector_record_expression, NULL, &market, error)
		    && yt_session_port_report(session, (int)market.logical_port,
		    &market, NULL, error);
	}
}

static bool
computer_planet_relation_cint(struct yt_session *session, float relationship,
    int *converted, const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t value = qb_cint_mode((double)relationship,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow)
		return computer_error(error, YT_RANGE, operation);
	*converted = (int)value;
	return true;
}

static bool
computer_field_length(struct yt_session *session, float raw, size_t maximum,
    size_t *length, const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)raw,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow || converted < 0)
		return computer_error(error, YT_RANGE, operation);
	*length = (size_t)converted;
	if (*length > maximum)
		*length = maximum;
	return true;
}

bool
yt_session_computer_planet_report(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "What sector number is the planet in? ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum = yt_port_single_sub(session_port_offset(session),
	    session_sector_offset(session));

	for (;;) {
		struct qb_val_result parsed;
		struct yt_sector sector;
		struct yt_planet planet;
		char response[160];
		double sector_fighters;
		float fighter_owner;
		float last_relationship;
		float link;
		float scratch;
		float selected;
		int relation_cint;
		bool denied;
		bool fighter_friendly;
		bool last_friendly;
		bool valid_link;

		if (!yt_session_fresh_no_turn_gate(session, &denied, error))
			return false;
		if (denied)
			return true;
		if (!session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "computer planet sector prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
		parsed = qb_val(response);
		if (parsed.overflow)
			return computer_error(error, YT_RANGE,
			    "computer planet sector VAL");
		selected = parsed.valid ? (float)qb_int(parsed.value) : 0.0f;
		if (selected < 1.0f)
			return true;
		if (selected > maximum) {
			char number[64];
			char notice[128];

			if (qb_str_single(number, sizeof(number), maximum) < 0
			    || snprintf(notice, sizeof(notice),
			    "Valid sector numbers are from 1 to%s.", number) < 0
			    || !session_present_alert(session,
			    (const uint8_t *)notice, strlen(notice),
			    "computer planet invalid sector", error))
				return false;
			continue;
		}
		if (!session_read_sector(session, (int)selected, &sector, error))
			return false;
		link = qb_mbf32_decode(sector.record.bytes + YT_F93);
		{
			float maximum_planet = yt_port_single_sub(
			    session->door->game.config.total_records,
			    session_planet_offset(session));

			valid_link = link > 0.0f && link <= maximum_planet;
		}
		if (valid_link) {
			bool limited_candidate;
			bool owner_differs;
			bool owner_nonzero;
			bool ground_nonzero;
			bool fighters_zero;
			bool fighters_positive;
			size_t name_length;

			session->hostile_deployed_fighters =
			    (double)sector.fighters;
			session->shared_target_record = qb_mbf32_decode(
			    sector.record.bytes + YT_F85);
			fighter_owner = session->shared_target_record;
			if (!yt_session_computer_owner_is_friendly(session,
			    fighter_owner, &fighter_friendly, error))
				return false;
			sector_fighters = session->hostile_deployed_fighters;
			last_relationship = session->shared_status;
			scratch = yt_port_single_add(
			    session_planet_offset(session), link);
			session->planet_record_expression = scratch;
			if (!session_read_planet(session, (int)link, &planet, error)
			    || !computer_field_length(session, planet.name_length,
			    YT_TEXT_FIELD_SIZE, &name_length,
			    "computer planet name length", error))
				return false;
			if (!computer_planet_relation_cint(session,
			    last_relationship, &relation_cint,
			    "computer planet fighter relationship CINT", error))
				return false;
			owner_differs = (float)session_record(session) != planet.owner;
			owner_nonzero = planet.owner != 0.0f;
			ground_nonzero = planet.ground_forces != 0.0f;
			fighters_zero = sector_fighters == 0.0;
			fighters_positive = sector_fighters > 0.0;
			limited_candidate = owner_differs && owner_nonzero
			    && ground_nonzero && (fighters_zero
			    || (fighters_positive && relation_cint != 0));
			if (limited_candidate) {
				char forces[64];
				uint8_t row[192];
				size_t length = 0;

				if (!yt_session_computer_owner_is_friendly(session,
				    planet.owner, &last_friendly, error))
					return false;
				last_relationship = session->shared_status;
				if (!computer_planet_relation_cint(session,
				    last_relationship, &relation_cint,
				    "computer planet owner relationship CINT", error))
					return false;
				if (~relation_cint != 0) {
					static const uint8_t prefix[] = "Planet: ";
					static const uint8_t infix[] =
					    " -*- Ground Forces:";

					if (qb_str_single(forces, sizeof(forces),
					    planet.ground_forces) < 0)
						return computer_error(error, YT_RANGE,
						    "computer planet forces format");
					memcpy(row + length, prefix,
					    sizeof(prefix) - 1U);
					length += sizeof(prefix) - 1U;
					memcpy(row + length, planet.record.bytes,
					    name_length);
					length += name_length;
					memcpy(row + length, infix,
					    sizeof(infix) - 1U);
					length += sizeof(infix) - 1U;
					memcpy(row + length, forces,
					    strlen(forces));
					length += strlen(forces);
					return session_present_paged_line(session,
					    row, length,
					    "computer planet limited row", error);
				}
			}
		}
		else {
			sector_fighters = session->hostile_deployed_fighters;
			fighter_owner = session->shared_target_record;
			last_relationship = session->shared_status;
			scratch = link;
		}
		if (!computer_planet_relation_cint(session, last_relationship,
		    &relation_cint,
		    "computer planet unavailable relationship CINT", error))
			return false;
		{
			bool scratch_zero = scratch == 0.0f;
			bool fighters_positive = sector_fighters > 0.0;
			bool team_positive = session->player.team > 0.0f;
			bool team_zero = session->player.team == 0.0f;
			bool relation_not = ~relation_cint != 0;
			bool fighter_owner_differs =
			    (float)session_record(session) != fighter_owner;
			bool no_information = scratch_zero
			    || (fighters_positive && team_positive && relation_not)
			    || (fighters_positive && team_zero
			    && fighter_owner_differs);

			if (no_information) {
				if (!yt_session_finalize_action(session, error))
					return false;
				return session_present_paged_line(session, unavailable,
				    sizeof(unavailable) - 1U,
				    "computer planet unavailable", error);
			}
		}
		if (!valid_link && session->planet_record_expression < 1.0f)
			return computer_error(error, YT_RANGE,
			    "computer planet stale current-planet record");
		return yt_session_planet_inventory(session, (int)(valid_link
		    ? link : yt_port_single_sub(
		    session->planet_record_expression,
		    session_planet_offset(session))), error);
	}
}

static bool
computer_avoid_cell(char *cell, size_t capacity, int slot, float value)
{
	char slot_text[64];
	char value_text[64];

	return qb_str_single(slot_text, sizeof(slot_text), (float)slot) >= 0
	    && qb_str_single(value_text, sizeof(value_text), value) >= 0
	    && snprintf(cell, capacity, "%s%s ]  -=> %s",
	    slot < 10 ? "[ " : "[", slot_text, value_text) >= 0;
}

bool
yt_session_computer_avoid(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading_one[] =
	    "You may set the autopilot to avoid up to 30 sectors";
	static const uint8_t heading_two[] = "Current sectors to avoid are:";
	static const uint8_t slot_prompt[] =
	    "Enter the number of the slot to change [1 - 30]: ";
	char response[80];
	float slot_value;
	float maximum;
	float new_value;
	float old_value;
	bool available;
	bool locked;
	enum yt_computer_avoid_selection_route route;
	int slot;
	int row;

	if (!session_present_paged_line(session, heading_one,
	    sizeof(heading_one) - 1U, "avoid first heading", error)
	    || !session_present_paged_line(session, heading_two,
	    sizeof(heading_two) - 1U, "avoid second heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid heading blank", error))
		return false;
	for (row = 0; row < 10; ++row) {
		char first[96];
		char middle[96];
		char last[96];

		if (!computer_avoid_cell(first, sizeof(first), row + 1,
		    session->route_avoid[row])
		    || !computer_avoid_cell(last, sizeof(last), row + 21,
		    session->route_avoid[row + 20])
		    || !session_fixed_width_bytes(session,
		    (const uint8_t *)first, strlen(first), 20.0f,
		    "avoid first cell", error)
		    || !computer_avoid_cell(middle, sizeof(middle), row + 11,
		    session->route_avoid[row + 10])
		    || !session_fixed_width_bytes(session,
		    (const uint8_t *)middle, strlen(middle), 20.0f,
		    "avoid middle cell", error)
		    || !session_present_paged_fragment(session,
		    (const uint8_t *)last, strlen(last)))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid slot-prompt blank", error)
	    || !session_present_timed_paged_row(session, slot_prompt,
	    sizeof(slot_prompt) - 1U, "avoid slot prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (!yt_computer_avoid_select_slot(response,
	    session->presentation.sound.conversion_mode, &slot_value, &slot,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	if (!yt_computer_avoid_maximum(session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	{
		char maximum_text[64];
		char prompt[160];

		if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "Enter the sector you wish to avoid [1 -%s] (0 to clear): ",
		    maximum_text) < 0
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "avoid sector-prompt blank", error)
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)prompt, strlen(prompt),
		    "avoid sector prompt", error)
		    || !session_read_number_command(session, response,
		    sizeof(response)))
			return false;
	}
	if (!yt_computer_avoid_select_sector(response, maximum, &new_value,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	old_value = session->route_avoid[slot - 1];
	session->route_avoid[slot - 1] = new_value;
	yt_computer_avoid_transition(old_value, new_value, &locked, &available);
	session_set_foreground(session, 2.0f);
	if (locked) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), new_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now locked out.", number) < 0
		    || !session_present_paged_line(session,
		    (const uint8_t *)status, strlen(status),
		    "avoid locked status", error))
			return false;
	}
	if (available) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), old_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now available.", number) < 0
		    || !session_present_paged_line(session,
		    (const uint8_t *)status, strlen(status),
		    "avoid available status", error))
			return false;
	}
	session_set_foreground(session, 1.0f);
	return true;
}

bool
yt_session_computer_owned_fighters(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t searching[] = "Searching;";
	static const uint8_t sector_heading[] = " Sector";
	static const uint8_t amount_heading[] = "Amount";
	static const uint8_t rule[] = "--------*--------";
	static const uint8_t none[] = " NONE found!";
	int maximum_sector = session_sector_count(session);
	float current_player = (float)session_record(session);
	bool found = false;
	int sector_number;

	if (maximum_sector < 0)
		return computer_error(error, YT_RANGE, "owned-fighter state");
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "owned-fighter opening blank", error)
	    || !session_present_timed_paged_row(session, searching,
	    sizeof(searching) - 1U, "owned-fighter searching row", error))
		return false;
	session->shared_status = 1.0f;

	for (sector_number = 1; sector_number <= maximum_sector;
	    ++sector_number) {
		struct yt_sector sector;
		char number[64];
		int number_length;

		if (!session_read_sector(session, sector_number, &sector, error))
			return false;
		if (sector.fighters <= 0.0f
		    || sector.fighter_owner != current_player)
			continue;

		if (!found) {
			found = true;
			if (!session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE,
			    "owned-fighter searching ending", error)
			    || !session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE,
			    "owned-fighter heading blank", error)
			    || !session_fixed_width_bytes(session, sector_heading,
			    sizeof(sector_heading) - 1U, 10.0f,
			    "owned-fighter heading sector", error)
			    || !session_present_paged_fragment(session, amount_heading,
			    sizeof(amount_heading) - 1U)
			    || !session_present_paged_fragment(session, rule,
			    sizeof(rule) - 1U))
				return false;
			session->shared_status = 0.0f;
		}

		number_length = qb_str_single(number, sizeof(number),
		    (float)sector_number);
		if (number_length < 0)
			return computer_error(error, YT_RANGE,
			    "owned-fighter sector format");
		if (!session_fixed_width_bytes(session,
		    (const uint8_t *)number, (size_t)number_length, 9.0f,
		    "owned-fighter sector field", error))
			return false;
		number_length = qb_str_single(number, sizeof(number),
		    sector.fighters);
		if (number_length < 0)
			return computer_error(error, YT_RANGE,
			    "owned-fighter amount format");
		if (!session_present_paged_fragment(session,
		    (const uint8_t *)number, (size_t)number_length))
			return false;
		if (strcmp(session->pager.key, "Q") == 0)
			break;
	}

	if (!found)
		return session_present_text(session, none, sizeof(none) - 1U,
		    SESSION_PRESENT_LINE, "owned-fighter none row", error);
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "owned-fighter trailing blank", error);
}

bool
yt_session_computer_owned_planets(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t scanning[] = "Scanning...";
	static const uint8_t none[] = "None found!";
	static const uint8_t prefix[] = "Planet: ";
	static const uint8_t infix[] = " Sector:";
	int maximum_sector = session_sector_count(session);
	float planet_record_base = session_planet_offset(session);
	float current_player = (float)session_record(session);
	bool found = false;
	int sector_number;

	if (maximum_sector < 0)
		return computer_error(error, YT_INVALID,
		    "owned-planet state");
	session_set_color(session, 2);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "owned-planet opening blank", error)
	    || !session_present_text(session, scanning, sizeof(scanning) - 1U,
	    SESSION_PRESENT_LINE, "owned-planet scanning row", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "owned-planet scanning blank", error))
		return false;
	session_set_color(session, 3);

	for (sector_number = 1; sector_number <= maximum_sector;
	    ++sector_number) {
		struct yt_sector sector;
		struct yt_record record;
		struct yt_planet planet;
		volatile float record_expression;
		uint32_t physical_record;

		if (!session_read_sector(session, sector_number, &sector, error))
			return false;
		if (sector.planet == 0.0f)
			continue;
		record_expression = planet_record_base + sector.planet;
		physical_record = qb_brun_random_record_number(record_expression);
		if (physical_record == 0U)
			return computer_error(error, YT_RANGE,
			    "owned-planet record number");
		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical_record, &record, error))
			return false;
		yt_planet_decode(&planet, &record);
		if (planet.owner == current_player) {
			uint8_t row[128];
			char number[64];
			size_t length = 0U;
			int number_length = qb_str_single(number, sizeof(number),
			    (float)sector_number);

			if (number_length < 0)
				return computer_error(error, YT_RANGE,
				    "owned-planet sector format");
			memcpy(row + length, prefix, sizeof(prefix) - 1U);
			length += sizeof(prefix) - 1U;
			memcpy(row + length, planet.record.bytes,
			    YT_TEXT_FIELD_SIZE);
			length += YT_TEXT_FIELD_SIZE;
			memcpy(row + length, infix, sizeof(infix) - 1U);
			length += sizeof(infix) - 1U;
			memcpy(row + length, number, (size_t)number_length);
			length += (size_t)number_length;
			if (!session_present_text(session, row, length,
			    SESSION_PRESENT_BOLD_LINE, "owned-planet match row",
			    error))
				return false;
			found = true;
		}
	}

	if (!found) {
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, none, sizeof(none) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "owned-planet none row", error);
	}
	return true;
}

static bool
scoreboard_progress(void *context, unsigned phase, struct yt_error *error)
{
	static const uint8_t dot[] = ".";
	struct yt_session *session = context;

	(void)phase;
	return session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error);
}

bool
yt_session_generate_scoreboard(struct yt_session *session,
    struct yt_error *error)
{
	return yt_score_generate_progress_with_layout(
	    &session->door->game, session_sector_offset(session),
	    session_port_offset(session), scoreboard_progress, session, NULL,
	    error);
}

bool
yt_session_computer_scoreboard(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x04, 0x00};
	char response[80];
	size_t length;

	session->pager.key[0] = '\0';
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "scoreboard selector leading blank", error)
	    || !session_present_timed_paged_row(session, prompt,
	    sizeof(prompt) - 1U, "scoreboard selector prompt", error)
	    || !session_read_command(session, response, sizeof(response)))
		return false;
	length = strlen(response);
	session_compat_upper_n(session,
	    (uint8_t *)session->output_source, length);
	session_compat_upper_n(session, (uint8_t *)response, length);
	session_set_pager_line_count_raw(session, dirty_zero);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "scoreboard selector trailing blank", error))
		return false;
	if (!(length == 1U && response[0] == 'O')) {
		if (!session_present_timed_paged_row(session, heading,
		    sizeof(heading) - 1U, "scoreboard update heading", error)
		    || !yt_session_generate_scoreboard(session, error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "scoreboard post-generator blank",
		    error))
			return false;
	}
	return session_display_game_file(session,
	    session->door->game.config.scoreboard, error);
}

bool
yt_session_computer_newspaper(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	char response[80];
	enum yt_computer_newspaper_choice choice;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "newspaper selector leading blank", error))
		return false;
	do {
		if (!session_present_timed_paged_row(session, prompt,
		    sizeof(prompt) - 1U, "newspaper selector prompt", error)
		    || !session_read_command(session, response, sizeof(response)))
			return false;
		session_compat_upper_n(session,
		    (uint8_t *)session->output_source, strlen(response));
		session_compat_upper_n(session, (uint8_t *)response,
		    strlen(response));
		choice = yt_computer_newspaper_select(response);
	} while (choice == YT_COMPUTER_NEWSPAPER_NONE);
	return session_display_game_file(session,
	    choice == YT_COMPUTER_NEWSPAPER_TODAY
	    ? "YTNEWS.DAT" : "YTYNEWS.DAT", error);
}
