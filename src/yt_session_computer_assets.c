#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>
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
		return session_computer_error(error, YT_RANGE, "owned-fighter state");
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
			return session_computer_error(error, YT_RANGE,
			    "owned-fighter sector format");
		if (!session_fixed_width_bytes(session,
		    (const uint8_t *)number, (size_t)number_length, 9.0f,
		    "owned-fighter sector field", error))
			return false;
		number_length = qb_str_single(number, sizeof(number),
		    sector.fighters);
		if (number_length < 0)
			return session_computer_error(error, YT_RANGE,
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
		return session_computer_error(error, YT_INVALID,
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
			return session_computer_error(error, YT_RANGE,
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
				return session_computer_error(error, YT_RANGE,
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


