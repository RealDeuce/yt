#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static bool
owned_fighters_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
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
		return owned_fighters_error(error, "owned-fighter state");
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
			return owned_fighters_error(error,
			    "owned-fighter sector format");
		if (!session_fixed_width_bytes(session,
		    (const uint8_t *)number, (size_t)number_length, 9.0f,
		    "owned-fighter sector field", error))
			return false;
		number_length = qb_str_single(number, sizeof(number),
		    sector.fighters);
		if (number_length < 0)
			return owned_fighters_error(error,
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

static bool
owned_planets_error(struct yt_error *error, enum yt_status status,
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
		return owned_planets_error(error, YT_INVALID,
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
			return owned_planets_error(error, YT_RANGE,
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
				return owned_planets_error(error, YT_RANGE,
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
