#include "yt_team.h"

#include "qb.h"

#include <string.h>

static bool
append_bytes(uint8_t *destination, size_t capacity, size_t *length,
    const void *source, size_t source_length)
{
	if (destination == NULL || length == NULL || *length > capacity
	    || source_length > capacity - *length
	    || (source == NULL && source_length != 0U))
		return false;
	if (source_length != 0U)
		memcpy(destination + *length, source, source_length);
	*length += source_length;
	return true;
}

static bool
append_text(uint8_t *destination, size_t capacity, size_t *length,
    const char *text)
{
	return text != NULL && append_bytes(destination, capacity, length,
	    text, strlen(text));
}

bool
yt_team_audit_message(enum yt_team_audit_event event,
    const char *player_name, const char *attempt, const char *date,
    const char *time_text, uint8_t *message, size_t capacity,
    size_t *length)
{
	const char *event_text;
	size_t used = 0U;

	if (player_name == NULL || attempt == NULL || message == NULL
	    || length == NULL)
		return false;
	switch (event) {
	case YT_TEAM_AUDIT_INVALID_PASSWORD:
		event_text = " entered invalid password for your team: ";
		break;
	case YT_TEAM_AUDIT_JOIN:
		event_text = " joined your team on ";
		break;
	case YT_TEAM_AUDIT_QUIT:
		event_text = " QUIT your team on ";
		break;
	default:
		return false;
	}
	if (!append_text(message, capacity, &used, player_name)
	    || !append_text(message, capacity, &used, " *** ")
	    || !append_text(message, capacity, &used, event_text))
		return false;
	if (event == YT_TEAM_AUDIT_INVALID_PASSWORD) {
		if (!append_text(message, capacity, &used, attempt))
			return false;
	} else if (date == NULL || time_text == NULL
	    || !append_text(message, capacity, &used, date)
	    || !append_text(message, capacity, &used, " at ")
	    || !append_text(message, capacity, &used, time_text)) {
		return false;
	}
	if (!append_text(message, capacity, &used, "!"))
		return false;
	*length = used;
	return true;
}

bool
yt_info_team_row(enum yt_info_team_row_kind kind, int team_id,
    const uint8_t *name, size_t name_length, uint8_t *row,
    size_t capacity, size_t *length)
{
	char number[64];
	int number_length;
	size_t used = 0U;

	if (row == NULL || length == NULL
	    || (name == NULL && name_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), (float)team_id);
	if (number_length < 0)
		return false;
	switch (kind) {
	case YT_INFO_TEAM_SUMMARY:
		if (!append_text(row, capacity, &used, "Team  :")
		    || !append_bytes(row, capacity, &used, number,
		    (size_t)number_length)
		    || !append_text(row, capacity, &used, ", ")
		    || !append_bytes(row, capacity, &used, name, name_length))
			return false;
		break;
	case YT_INFO_TEAM_SELF_CAPTAIN:
		if (!append_text(row, capacity, &used,
		    "You are the Captain of team")
		    || !append_bytes(row, capacity, &used, number,
		    (size_t)number_length)
		    || !append_text(row, capacity, &used, "!"))
			return false;
		break;
	case YT_INFO_TEAM_OTHER_CAPTAIN:
		if (!append_text(row, capacity, &used,
		    "Your Team Captain is: ")
		    || !append_bytes(row, capacity, &used, name, name_length)
		    || !append_text(row, capacity, &used, "!"))
			return false;
		break;
	default:
		return false;
	}
	*length = used;
	return true;
}

void
yt_team_cache_load(struct yt_team_cache *cache,
    const struct yt_record *record, int current_player_record, bool *live)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125,
	};
	float roster[4];
	size_t name_length;
	size_t index;
	bool any_member = false;

	if (live != NULL)
		*live = false;
	if (cache == NULL || record == NULL)
		return;
	memset(cache, 0, sizeof(*cache));
	for (index = 0U; index < YT_ARRAY_LEN(roster); ++index) {
		roster[index] = yt_record_get_number(record,
		    roster_offsets[index]);
		if (roster[index] != 0.0f)
			any_member = true;
	}
	if (!any_member)
		return;
	name_length = (size_t)yt_record_get_number(record, YT_F73);
	if (name_length > YT_TEXT_FIELD_SIZE)
		name_length = YT_TEXT_FIELD_SIZE;
	memcpy(cache->name, record->bytes, name_length);
	cache->name[name_length] = '\0';
	cache->name_length = name_length;
	memcpy(cache->password, record->bytes + YT_F113, 4U);
	cache->password[4] = '\0';
	cache->captain = (int)yt_record_get_number(record, YT_F77);
	cache->current_player_is_captain =
	    cache->captain == current_player_record;
	for (index = 0U; index < YT_ARRAY_LEN(roster); ++index)
		cache->roster[index] = (int)roster[index];
	if (live != NULL)
		*live = true;
}

bool
yt_team_choice_rejected(float choice, float raw_team,
    int32_t captain_cint, int32_t team_cint)
{
	return (choice > 3.0f && raw_team == 0.0f)
	    || (choice > 6.0f && captain_cint != -1)
	    || (choice > 1.0f && choice < 4.0f && team_cint != 0)
	    || choice < 1.0f || choice > 10.0f;
}

void
yt_team_transfer_apply_sector(struct yt_sector *sector,
    double initial_fighters, float amount)
{
	volatile float updated;

	if (sector == NULL)
		return;
	updated = (float)(initial_fighters + (double)amount);
	sector->fighters = updated;
	(void)yt_record_set_number(&sector->record, YT_F81, updated);
}

void
yt_team_transfer_apply_player(struct yt_player *player, float amount)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = (float)((double)player->fighters - (double)amount);
	player->fighters = updated;
	(void)yt_record_set_number(&player->record, YT_F61, updated);
}

void
yt_team_membership_apply_player(struct yt_player *player, int team)
{
	if (player == NULL)
		return;
	player->team = (float)team;
	(void)yt_record_set_number(&player->record, YT_F89, (float)team);
}

void
yt_team_banish_apply_player(struct yt_player *player)
{
	yt_team_membership_apply_player(player, 0);
}

void
yt_team_roster_overlay(struct yt_record *record, const int roster[4])
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125,
	};
	size_t index;

	if (record == NULL || roster == NULL)
		return;
	for (index = 0; index < YT_ARRAY_LEN(offsets); ++index)
		(void)yt_record_set_number(record, offsets[index],
		    (float)roster[index]);
}

void
yt_team_name_overlay(struct yt_record *record, const uint8_t *name,
    size_t length)
{
	if (record == NULL || (name == NULL && length != 0U))
		return;
	yt_record_set_text(record, name, length);
	(void)yt_record_set_number(record, YT_F73, (float)length);
}

bool
yt_team_prepare_name(char *name, size_t *length)
{
	size_t normalized;

	if (name == NULL || length == NULL || strlen(name) < 3U)
		return false;
	normalized = qb_title_case_n((uint8_t *)name, strlen(name));
	if (normalized > YT_TEXT_FIELD_SIZE)
		normalized = YT_TEXT_FIELD_SIZE;
	name[normalized] = '\0';
	*length = normalized;
	return true;
}

void
yt_team_password_overlay(struct yt_record *record, const uint8_t password[4])
{
	if (record == NULL || password == NULL)
		return;
	memcpy(record->bytes + YT_F113, password, 4U);
}

void
yt_team_inactive_overlay(struct yt_record *record)
{
	static const size_t offsets[5] = {
		YT_F77, YT_F109, YT_F117, YT_F121, YT_F125,
	};
	static const uint8_t zero[4] = {0};
	size_t index;

	if (record == NULL)
		return;
	for (index = 0; index < YT_ARRAY_LEN(offsets); ++index)
		(void)yt_record_set_raw_number(record, offsets[index], zero);
	memset(record->bytes + YT_F113, ' ', 4U);
}
