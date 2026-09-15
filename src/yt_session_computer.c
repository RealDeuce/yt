#include "yt_session_internal.h"

#include "qb.h"
#include "yt_main_error.h"

#include <stdio.h>
#include <string.h>

bool
session_computer_error(struct yt_error *error, enum yt_status status,
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
	session->navigation.route_marker = 0.0f;
	if (!yt_session_computer_owner_is_friendly(session,
	    sector->fighter_owner,
	    &friendly, error))
		return false;
	session->planet_record_expression = qb_single_add(
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
		session->earth.report_seen = false;
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
		return session_computer_error(error, YT_RANGE, operation);
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
		return session_computer_error(error, YT_RANGE, operation);
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
	float maximum = qb_single_subtract(session_port_offset(session),
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
			return session_computer_error(error, YT_RANGE,
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
			float maximum_planet = qb_single_subtract(
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

			session->combat.deployed_fighters =
			    (double)sector.fighters;
			session->shared_target_record = qb_mbf32_decode(
			    sector.record.bytes + YT_F85);
			fighter_owner = session->shared_target_record;
			if (!yt_session_computer_owner_is_friendly(session,
			    fighter_owner, &fighter_friendly, error))
				return false;
			sector_fighters = session->combat.deployed_fighters;
			last_relationship = session->shared_status;
			scratch = qb_single_add(
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
						return session_computer_error(error, YT_RANGE,
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
			sector_fighters = session->combat.deployed_fighters;
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
			return session_computer_error(error, YT_RANGE,
			    "computer planet stale current-planet record");
		return yt_session_planet_inventory(session, (int)(valid_link
		    ? link : qb_single_subtract(
		    session->planet_record_expression,
		    session_planet_offset(session))), error);
	}
}
