#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_main_error.h"
#include "yt_maint.h"
#include "yt_names.h"
#include "yt_output.h"
#include "yt_platform.h"
#include "yt_startup.h"
#include "yt_text.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define YT_PLAYER_FIRST YT_PLAYER_FIRST_RECORD
#define YT_PLAYER_LAST YT_PLAYER_LAST_RECORD

static bool
load_configuration(struct yt_session *session, struct yt_error *error)
{
	struct yt_game *game = &session->door->game;
	bool ok;

	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	ok = yt_game_load_startup_configuration(game, "YTDATA.DAT",
	    session->door->identity.local, &session->player_cache,
	    session->disruption_sectors,
	    &session->presentation.sound.local_output,
	    error);
	session->door->game_open = game->database.file != NULL;
	if (ok) {
		uint16_t sector_count = game->config.port_offset
		    - game->config.sector_offset;
		const struct yt_patch_profile *patch = session->door->patch;

		if (patch == NULL)
			patch = yt_patch_default();
		if (!yt_patch_sector_count_matches(patch, sector_count)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				error->system_error = 0;
				(void)snprintf(error->operation,
				    sizeof(error->operation),
				    "validate %s universe; expected %u sector records",
				    patch->name,
				    (unsigned)patch->initializer_sector_count);
				(void)snprintf(error->path, sizeof(error->path),
				    "YTDATA.DAT has %u", (unsigned)sector_count);
			}
			ok = false;
		}
	}
	return ok;
}

static bool
opening_and_date(struct yt_session *session, struct yt_error *error)
{
	struct yt_shared_error_result shared_error;
	struct session_route_plan route;
	uint16_t opening_basic_error;

	if (!yt_session_build_route(session, 1.0f, 2.0f, false, &route, error))
		return false;
	if (session->presentation.sound.ansi) {
		if (!yt_out_opening_file("YTOPEN.ANS",
		    session->presentation.sound.local_mode,
		    session->presentation.sound.local_output,
		    &session->io.input,
		    &opening_basic_error, error)) {
			if (opening_basic_error != 0U) {
				if (!yt_shared_error_compose(
				    (int16_t)opening_basic_error, 2710,
				    &shared_error))
					return false;
				if (!session_commit_shared_terminal(session,
				    &shared_error, error))
					return false;
			}
			return false;
		}
	}
	/* Row 25 belongs to the deferred OpenDoors local personality. */
	session->pager.nonstop = true;
	return session_display_game_file(session, "YTOPEN.ASC", error);
}

static bool
startup_pre_admission(struct yt_session *session, struct yt_error *error)
{
	char welcome[320];
	int adjusted_year;
	int today;

	session_set_foreground(session, 5);
	if (!session_present_paged_line(session, (const uint8_t *)"Initializing...",
	    strlen("Initializing..."), "startup initializing row", error))
		return false;
	if (!session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!yt_session_check_lockout(session, error))
		return false;
	snprintf(welcome, sizeof(welcome), "Welcome %s!",
	    session->door->identity.real_first);
	if (!session_present_paged_line(session, (const uint8_t *)welcome,
	    strlen(welcome), "startup welcome row", error))
		return false;
	if (!session_present_paged_fragment(session,
	    (const uint8_t *)"Searching my records for your name.",
	    strlen("Searching my records for your name.")))
		return false;
	return true;
}

static bool
resolve_alias(struct yt_session *session, char first[128], char last[128],
    struct yt_error *error)
{
	struct yt_name_file names;
	const struct yt_name_row *match;

	snprintf(first, 128, "%s", session->door->identity.real_first);
	snprintf(last, 128, "%s", session->door->identity.real_last);
	qb_title_case(first);
	qb_title_case(last);
	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	match = yt_names_find_real_last(&names, first, last);
	if (match != NULL) {
		snprintf(first, 128, "%s", match->alias_first);
		snprintf(last, 128, "%s", match->alias_last);
		yt_names_free(&names);
		return true;
	}
	for (;;) {
		char alias[256];
		char alias_first[128];
		char alias_last[128];
		char display[258];
		char confirmation[80];
		enum yt_alias_key_status alias_status;
		struct yt_name_row row;

		session_set_foreground(session, 2);
		if (!session_present_paged_line(session,
		    (const uint8_t *)"You are a new player.",
		    strlen("You are a new player."), "new alias notice", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_paged_line(session,
		    (const uint8_t *)
		    "Enter the FULL alias you wish to use in the game.",
		    strlen("Enter the FULL alias you wish to use in the game."),
		    "new alias instruction", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_paged_line(session,
		    (const uint8_t *)"Press [ENTER] to use your real name.",
		    strlen("Press [ENTER] to use your real name."),
		    "new alias real-name instruction", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)"-+> ", 4, "new alias prompt", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_read_command(session, alias, sizeof(alias))) {
			yt_names_free(&names);
			return false;
		}
		alias_status = yt_names_prepare_alias(alias, sizeof(alias), first,
		    last, alias_first, sizeof(alias_first), alias_last,
		    sizeof(alias_last), display, sizeof(display));
		if (alias_status == YT_ALIAS_KEY_EMPTY)
			continue;
		if (alias_status == YT_ALIAS_KEY_RANGE) {
			yt_names_free(&names);
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "new alias key preparation");
			}
			return false;
		}
		if (alias_status == YT_ALIAS_KEY_RESERVED) {
			if (!session_present_paged_fragment(session,
			    (const uint8_t *)
			    "That ALIAS is NOT allowed. Please choose another.",
			    strlen("That ALIAS is NOT allowed. Please choose another."))) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		if (yt_names_alias_exists(&names, alias_first, alias_last)) {
			char collision[320];

			snprintf(collision, sizeof(collision),
			    "I'm sorry %s, but that Alias is already in use.", first);
			if (!session_present_paged_fragment(session, (const uint8_t *)collision,
			    strlen(collision))) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		session_set_foreground(session, 3);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "new alias identity blank", error)) {
			yt_names_free(&names);
			return false;
		}
		session->presentation.bold = true;
		{
			char identity[560];

			snprintf(identity, sizeof(identity), "%s %s a.k.a. %s",
			    first, last, display);
			if (!session_present_paged_fragment(session,
			    (const uint8_t *)identity, strlen(identity))) {
				yt_names_free(&names);
				return false;
			}
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "new alias confirmation blank", error)) {
				yt_names_free(&names);
				return false;
			}
		}
		session_set_foreground(session, 6);
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)"Is this OK (Y/[N])? ",
		    strlen("Is this OK (Y/[N])? "),
		    "new alias confirmation prompt", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_read_upper_command(session, confirmation,
		    sizeof(confirmation))) {
			yt_names_free(&names);
			return false;
		}
		if (strcmp(confirmation, "Y") != 0)
			continue;
		row.real_first = first;
		row.real_last = last;
		row.alias_first = alias_first;
		row.alias_last = alias_last;
		if (!yt_names_append("YTNAME.DAT", &row, error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_alert(session,
		    (const uint8_t *)"Your Alias has been recorded. Have fun!",
		    strlen("Your Alias has been recorded. Have fun!"),
		    "new alias accepted row", error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "new alias final blank", error)) {
			yt_names_free(&names);
			return false;
		}
		snprintf(first, 128, "%s", alias_first);
		snprintf(last, 128, "%s", alias_last);
		yt_names_free(&names);
		return true;
	}
}

static bool
construct_player_visible(struct yt_session *session, struct yt_error *error)
{
	enum yt_player_constructor_failure failure;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "player constructor blank", error))
		return false;
	if (!session_present_text(session,
	    (const uint8_t *)"Your ship has been built.",
	    strlen("Your ship has been built."), SESSION_PRESENT_LINE,
	    "player constructor row", error))
		return false;
	if (yt_game_construct_player(&session->door->game,
	    session_record(session), (uint16_t)session->door->game.today,
	    session->door->game.config.turns_per_day, &session->player,
	    &failure, error))
		return true;
	if (failure == YT_PLAYER_CONSTRUCTOR_CONFIG_GET)
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_CONFIG_GET);
	else if (failure == YT_PLAYER_CONSTRUCTOR_PLAYER_GET)
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_GET);
	else if (failure == YT_PLAYER_CONSTRUCTOR_PLAYER_PUT)
		attach_database_put_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_PUT);
	if (error != NULL && error->basic_fault_valid)
		(void)session_route_basic_fault(session, error);
	return false;
}

static bool
set_new_player_identity(struct yt_session *session, int player_record,
    const uint8_t *name, size_t length, struct yt_error *error)
{
	struct yt_player player;
	struct yt_record identity;
	uint8_t length_raw[4];

	if (name == NULL && length != 0U)
		return false;
	if (!session_read_player_at_fault(session, player_record, &player,
	    YT_BASIC_FAULT_IDENTITY_PLAYER_GET, error)) {
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	identity = player.record;
	yt_record_set_text(&identity, name, length);
	if (qb_mbf32_encode((float)length, length_raw) != QB_MBF_OK)
		return false;
	(void)yt_record_set_raw_number(&identity, YT_F85, length_raw);
	(void)yt_record_set_number(&identity, YT_F89, 0.0f);
	yt_player_decode(&session->player, &identity);
	if (!write_database_record_at_fault(session, (uint32_t)player_record,
	    &identity, YT_BASIC_FAULT_IDENTITY_PLAYER_PUT, error)) {
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	return true;
}

bool
yt_session_instruction_offer(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want instructions (Y/N) [N]? ";
	char response[80];

	for (;;) {
		enum yt_yes_no_answer answer;

		memcpy(session->io.text_workspace, prompt, sizeof(prompt));
		if (!session_present_text(session,
		    (const uint8_t *)session->io.text_workspace,
		    sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "instruction question", error))
			return false;
		if (!session_read_command(session, response, sizeof(response)))
			return false;
		if (!yt_input_yes_no_candidate(session->io.editor_buffer,
		    session->io.text_workspace, sizeof(session->io.text_workspace),
		    &answer))
			return false;
		if (answer == YT_YES_NO_EMPTY || answer == YT_YES_NO_NO)
			return true;
		if (answer == YT_YES_NO_YES)
			return session_display_game_file(session, "YTINSTR.DOC", error);
		session->presentation.bold = true;
		session_clear_queue(session);
	}
}

static bool
startup_retention(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prefix[] =
	    "Notice: If your ship is dead and you have not played for";
	static const uint8_t second[] =
	    "days, it will be deleted to make room for someone else.";
	struct yt_record record;
	struct yt_config config;
	uint8_t first[128];
	char number[64];
	int number_length;
	size_t first_length;

	if (!yt_database_read(&session->door->game.database, 1U, &record,
	    error))
		return false;
	if (!yt_config_decode(&config, &record, error))
		return false;
	number_length = qb_str_single(number, sizeof(number),
	    config.retention_days);
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length > sizeof(first))
		return false;
	memcpy(first, prefix, sizeof(prefix) - 1U);
	memcpy(first + sizeof(prefix) - 1U, number, (size_t)number_length);
	first_length = sizeof(prefix) - 1U + (size_t)number_length;
	if (!session_present_paged_line(session, first, first_length,
	    "new player retention first row", error))
		return false;
	if (!session_present_paged_fragment(session, second,
	    sizeof(second) - 1U))
		return false;
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "new player retention final blank", error);
}

static bool
returning_daily_update(struct yt_session *session,
    uint16_t today, float turns_per_day,
    uint16_t *previous_day, int *killer, struct yt_error *error)
{
	static const uint8_t row[] = "You have been on today.";
	struct yt_player player;
	struct yt_record daily;
	bool same_day;

	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &player, error)) {
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_RETURNING_DAILY_GET);
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	*previous_day = player.last_active;
	same_day = *previous_day == today;
	if (same_day) {
		if (!session_present_text(session, row, sizeof(row) - 1U,
		    SESSION_PRESENT_LINE, "returning same-day row", error)) {
			if (error != NULL && error->basic_fault_valid)
				(void)session_route_basic_fault(session, error);
			return false;
		}
	}
	*killer = player.killed_by;

	daily = player.record;
	if (!yt_record_set_number(&daily, YT_F41, (float)today))
		return false;
	if (!same_day) {
		if (player.turns < turns_per_day)
			player.turns = turns_per_day;
		if (!yt_record_set_number_if_changed(&daily, YT_F49,
		    player.turns))
			return false;
		if (!yt_record_set_number(&daily, YT_F105, 0.0f))
			return false;
	}
	yt_player_decode(&player, &daily);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &daily, error)) {
		attach_database_put_fault(session, error,
		    YT_BASIC_FAULT_RETURNING_DAILY_PUT);
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	session->player = player;
	return true;
}

static bool
returning_self_denial(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] =
	    "You will be allowed to play again tomorrow!";

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "returning self-denial blank", error))
		return false;
	session->presentation.blink = true;
	session_set_foreground(session, 7);
	if (!session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "returning self-denial row", error))
		return false;
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	return true;
}

static bool
admit_player(struct yt_session *session, const char *first, const char *last,
    struct yt_error *error)
{
	char full[256];
	struct yt_clock_value now;
	int returning_bound;
	int basic;
	bool returning = false;

	snprintf(full, sizeof(full), "%s %s", first, last);
	returning_bound = session->door->game.config.sector_offset;
	for (basic = YT_PLAYER_FIRST; basic <= returning_bound; ++basic) {
		struct yt_player candidate;
		bool matches;

		session->planet.fallback_index = basic;
		if (!yt_game_read_player(&session->door->game, basic, &candidate,
		    error))
			return false;
		matches = yt_player_name_matches(&candidate,
		    (const uint8_t *)full, strlen(full));
		if (matches) {
			session->active_player_record = basic;
			session->player = candidate;
			session->cached_player_name_length =
			    yt_player_stored_name(&candidate,
			    session->cached_player_name);
			returning = true;
			break;
		}
		session->planet.fallback_index = basic + 1;
	}
	if (!returning) {
		int vacant = 0;
		int vacancy_bound;

		session_set_foreground(session, 5);
		if (!session_present_paged_line(session,
		    (const uint8_t *)"Entering a new player...",
		    strlen("Entering a new player..."),
		    "new player entering row", error))
			return false;
		vacancy_bound = session->door->game.config.sector_offset;
		session->active_player_record = YT_PLAYER_FIRST;
		for (basic = YT_PLAYER_FIRST;
		    basic <= vacancy_bound;
		    ++basic) {
			struct yt_player candidate;

			if (!yt_game_read_player(&session->door->game, basic,
			    &candidate, error))
				return false;
			if (candidate.name_length < 1U) {
				vacant = basic;
				break;
			}
			session->active_player_record = basic + 1;
		}

		if (vacant == 0) {
			char date[11];

			if (!session_present_alert(session,
			    (const uint8_t *)
			    "I'm sorry but the game is full. Try again tomorrow.",
			    strlen("I'm sorry but the game is full. Try again tomorrow."),
			    "new player full row", error))
				return false;
			if (!yt_clock_read(&session->door->game.clock, &now, error))
				return false;
			yt_format_date(&now, date);
			if (!yt_news_append_game_full(date, full, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		if (!startup_retention(session, error))
			return false;
		if (!construct_player_visible(session, error))
			return false;
		if (!set_new_player_identity(session, vacant,
		    (const uint8_t *)full, strlen(full), error))
			return false;
		session->cached_player_name_length =
		    yt_player_stored_name(&session->player,
		    session->cached_player_name);
		if (!yt_clock_read(&session->door->game.clock, &now, error))
			return false;
		{
			char date[11];

			yt_format_date(&now, date);
			if (!yt_news_append_new_player(date, full, error))
				return false;
		}
		return yt_session_instruction_offer(session, error);
	}
	session_set_foreground(session, 2);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "returning player blank", error))
		return false;
	{
		uint16_t previous_day;
		int killer;
		uint16_t startup_day;
		bool self_kill;

		if (!returning_daily_update(session,
		    (uint16_t)session->door->game.today,
		    session->door->game.config.turns_per_day,
		    &previous_day, &killer, error))
			return false;
		if (!yt_database_flush(&session->door->game.database, error))
			return false;
		startup_day = (uint16_t)session->door->game.today;
		self_kill = killer == session_record(session);
		if (!yt_clock_read(&session->door->game.clock, &now, error))
			return false;
		{
			char time_text[9];

			yt_format_time(&now, time_text);
			if (!yt_news_append_login_bytes(
			    (const uint8_t *)time_text, strlen(time_text),
			    session->cached_player_name,
			    session->cached_player_name_length, error))
				return false;
		}
		if (killer != 0) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "returning death blank", error))
				return false;
			if (!self_kill)
				session->presentation.blink = true;
			if (killer == -1) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by The Xannor!",
				    strlen("You have been killed by The Xannor!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning Xannor death row", error))
					return false;
			}
			else if (killer == -2) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by mercenaries!",
				    strlen("You have been killed by mercenaries!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning mercenary death row", error))
					return false;
			}
			else if (killer == -98) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by a deleted player.",
				    strlen("You have been killed by a deleted player."),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning deleted-player death row", error))
					return false;
			}
			else if (self_kill) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You managed to kill yourself on your last time on.",
				    strlen("You managed to kill yourself on your last time on."),
				    SESSION_PRESENT_LINE,
				    "returning self-death row", error))
					return false;
			}
			else if (killer > 1
			    && killer <= session_sector_offset(session)) {
				struct yt_player attacker;
				uint8_t attacker_row[YT_TEXT_FIELD_SIZE
				    + sizeof(" destroyed your ship!") - 1U];
				size_t attacker_length;
				bool emit;

				if (!yt_game_read_player(&session->door->game,
				    killer, &attacker, error)) {
					attach_database_get_fault(session, error,
					    YT_BASIC_FAULT_RETURNING_KILLER_GET);
					(void)session_route_basic_fault(session, error);
					return false;
				}
				if (!yt_player_killer_row(&attacker, attacker_row,
				    sizeof(attacker_row), &attacker_length, &emit, error))
					return false;
				if (emit) {
					if (!session_present_text(session, attacker_row,
					    attacker_length, SESSION_PRESENT_BOLD_LINE,
					    "returning player death row", error))
						return false;
				}
			}
			if (self_kill
			    && previous_day == startup_day) {
				(void)returning_self_denial(session, error);
				return false;
			}
			if (!construct_player_visible(session, error))
				return false;
			if (!session_wait(session, 5.0,
			    "returning-player rebuild wait", error))
				return false;
		}
	}
	return true;
}

static bool
post_login(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] = "[ Press any Key ]";

	{
		struct yt_record repaired;
		float maximum_holds;

		if (!session_reload_player(session, error))
			return false;
		if (session->player.sector < 1) {
			repaired = session->player.record;
			(void)yt_record_set_number(&repaired, YT_F57, 1.0f);
			yt_player_decode(&session->player, &repaired);
			if (!write_database_record_at_fault(session,
			    (uint32_t)session_record(session), &repaired,
			    YT_BASIC_FAULT_POST_LOGIN_SECTOR_PUT, error))
				return false;
		}
		if (!session_reload_player(session, error))
			return false;
		maximum_holds = session->door->game.config.maximum_holds;
		if ((double)session->player.holds
		    > (double)maximum_holds) {
			repaired = session->player.record;
			(void)yt_record_set_number(&repaired, YT_F69, 0.0f);
			(void)yt_record_set_number(&repaired, YT_F73, 0.0f);
			(void)yt_record_set_number(&repaired, YT_F77,
			    maximum_holds);
			(void)yt_record_set_number(&repaired, YT_F65,
			    maximum_holds);
			yt_player_decode(&session->player, &repaired);
			if (!write_database_record_at_fault(session,
			    (uint32_t)session_record(session), &repaired,
			    YT_BASIC_FAULT_POST_LOGIN_CARGO_PUT, error))
				return false;
		}
	}
	/* Do not emit the deferred local-personality status row here. */
	if (!yt_session_show_ship(session, error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login Info trailing blank", error))
		return false;
	if (!session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "post-login low-time warning", error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "post-login press prompt");
		}
		return false;
	}
	if (!session_wait(session, 99.0, "post-login press wait", error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login press trailing blank", error))
		return false;
	if (!yt_session_radio_read(session, false, error))
		return false;
	return true;
}

bool
yt_session_run(struct yt_door *door, const char *executable_path,
    struct yt_error *error)
{
	struct yt_session session;
	struct yt_random launch_random;
	bool resume_gameplay = false;
	char first[128];
	char last[128];

	if (door == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			snprintf(error->operation, sizeof(error->operation),
			    "player session");
		}
		return false;
	}
	memset(&session, 0, sizeof(session));
	yt_input_init(&session.io.input);
	session.error = error;
	session.door = door;
	session.executable_path = executable_path;
	session.running = true;
	/* YT:040A is the ordinary instruction after the handed-off checkpoint. */
	session.pager.nonstop = true;
	session.presentation.sound.ansi = door->identity.ansi;
	session.presentation.sound.local_mode = door->identity.local;
	session.presentation.sound.user_sound = true;
	session.presentation.sound.local_sound = door->identity.local;
	session_set_foreground(&session, 7);
	yt_random_init(&launch_random);
	if (!yt_random_market_bases(&launch_random, session.market_bases, error))
		return false;
	if (!load_configuration(&session, error))
		return session.terminated;
	session_set_foreground(&session, 6);
	if (!session_present_text(&session, NULL, 0, SESSION_PRESENT_LINE,
	    "startup pre-title blank", error))
		return session.terminated;
	if (!yt_session_registration(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!opening_and_date(&session, error))
		return session.terminated;
	if (!startup_pre_admission(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!resolve_alias(&session, first, last, error))
		return session.terminated;
	if (!admit_player(&session, first, last, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!post_login(&session, error)) {
		if (!session_handle_gameplay_fault(&session, error,
		    &resume_gameplay))
			return false;
	}
	else if (!yt_session_sector_entry(&session, error)) {
		if (!session_handle_gameplay_fault(&session, error,
		    &resume_gameplay))
			return false;
	}
	if (session.terminated)
		return true;
	if (!session.destroyed && session.running) {
		for (;;) {
			bool completed = resume_gameplay
			    ? yt_session_sector_entry(&session, error)
			    : yt_session_command_shell(&session, error);
			if (completed) {
				if (!resume_gameplay)
					break;
				resume_gameplay = false;
				if (!session.running || session.destroyed)
					break;
				continue;
			}
			if (!session_handle_gameplay_fault(&session, error,
			    &resume_gameplay))
				return false;
			if (session.terminated)
				return true;
		}
	}
	if (session.destroyed && !session.fatal_wait_complete) {
		if (!session_wait(&session, 5.0, "common fatal wait", error))
			return false;
		session.fatal_wait_complete = true;
	}
	return yt_session_quit(&session, error);
}
