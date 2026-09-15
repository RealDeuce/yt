#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float *
player_item(struct yt_player *player, int item)
{
	switch (item) {
	case 1: return &player->ore;
	case 2: return &player->organics;
	case 3: return &player->equipment;
	case 4: return &player->fighters;
	case 5: return &player->missiles;
	case 6: return &player->mines;
	case 9: return &player->plasma;
	default: return NULL;
	}
}
bool
yt_session_planet_garrison(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t insufficient[] = "Insuficient forces!";
	struct yt_planet planet;
	struct qb_val_result parsed;
	char response[160];
	uint8_t prompt[160];
	uint8_t success[128];
	size_t prompt_length;
	size_t success_length;
	float old_garrison;
	float desired;
	float after;

	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	old_garrison = planet.ground_forces;
	if (!session_reload_player(session, error))
		return false;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet garrison opening blank", error)
	    || !yt_planet_garrison_prompt(session->player.ground_forces,
	    old_garrison, prompt, sizeof(prompt), &prompt_length)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "planet garrison prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	desired = (float)floor(parsed.valid ? parsed.value : 0.0);
	after = yt_planet_garrison_after(session->player.ground_forces,
	    desired, old_garrison);
	if (desired < 0.0f || after < 0.0f)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U, "planet garrison refusal", error);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_garrison_overlay(&planet, desired, 0);
	if (desired >= 1.0f) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet garrison success blank", error)
		    || !yt_planet_garrison_success_row(desired, success,
		    sizeof(success), &success_length))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_paged_fragment(session, success, success_length))
			return false;
		planet.owner = (float)session_record(session);
		if (!yt_record_set_number(&planet.record, YT_F73, planet.owner)
		    || !session_sound(session, 4.0f,
		    "planet garrison sound", error))
			return false;
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &planet.record, error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_garrison_player_overlay(&session->player, after);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}

bool
yt_session_planet_bank(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t savings[] =
	    "We are a SAVINGS not a LOAN institution!";
	static const uint8_t insufficient[] =
	    "You don't have that many Credits!";
	static const uint8_t farewell[] = "Have a nice day!";
	struct yt_planet planet;
	struct qb_val_result parsed;
	char title[160];
	char available_text[64];
	char prompt[192];
	char response[160];
	char amount_text[64];
	char success[192];
	double target;
	double available;
	double remaining;
	float old_bank;
	float credit_argument;

	session_set_foreground(session, 6.0f);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	old_bank = planet.bank;
	available = yt_planet_bank_available(session->player.credits, old_bank);
	if (snprintf(title, sizeof(title),
	    "Welcome to the intergalactic bank of %s!",
	    session->planet_name) < 0
	    || qb_str_double(available_text, sizeof(available_text), available) < 0
	    || snprintf(prompt, sizeof(prompt),
	    "How many credits do you want in the account?%s Available ->",
	    available_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)title, strlen(title),
	    "planet Bank title", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Bank pre-prompt blank", error)
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Bank amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow)
		return session_range_error(error, "planet Bank amount VAL");
	target = qb_int(parsed.valid ? parsed.value : 0.0);
	if (target < 0.0)
		return session_present_alert(session, savings, sizeof(savings) - 1U,
		    "planet Bank savings error", error);
	remaining = yt_planet_bank_remaining(session->player.credits, old_bank,
	    target);
	if (remaining < 0.0)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Bank credit error", error);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_bank_planet_overlay(&planet, target);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	session->player.credits = (float)remaining;
	if (target != 0.0) {
		if (qb_str_double(amount_text, sizeof(amount_text), target) < 0
		    || snprintf(success, sizeof(success),
		    "You have%s credits on deposit at 1%% interest. %s",
		    amount_text, farewell) < 0)
			return session_range_error(error,
			    "planet Bank success format");
	}
	else {
		memcpy(success, farewell, sizeof(farewell));
	}
	if (!session_present_paged_line(session, (const uint8_t *)success, strlen(success),
	    "planet Bank accepted", error)
	    || !session_sound(session, 4.0f, "planet bank sound", error))
		return false;
	credit_argument = yt_planet_bank_credit_argument(old_bank, target);
	return session_mutate_player_credits(session, credit_argument, NULL,
	    error);
}

bool
yt_session_planet_rename(struct yt_session *session, int logical_planet,
    bool *renamed,
    struct yt_error *error)
{
	static const uint8_t protected[] =
	    "You can't re-name this planet!";
	static const uint8_t prompt[] =
	    "What do you want to name this planet? -=>";
	static const uint8_t reserved[] = "I Don't think so!";
	static const uint8_t confirmation_suffix[] =
	    " Is this OK? (Y/n) [Y] ?";
	struct yt_planet planet;
	char name[YT_COMMAND_SIZE];
	uint8_t confirmation[2U + 41U + sizeof(confirmation_suffix) - 1U];
	float current_record;
	bool written;

	if (renamed != NULL)
		*renamed = false;

	for (;;) {
		enum yt_yes_no_answer answer;
		enum yt_planet_rename_name_result name_result;
		size_t name_length;
		size_t confirmation_length = 0;

		current_record = qb_single_add(
		    session_planet_offset(session),
		    (float)logical_planet);
		if (yt_planet_rename_protected(current_record,
		    session_planet_offset(session),
		    session->door->game.config.total_records))
			return session_present_alert(session, protected,
			    sizeof(protected) - 1U,
			    "planet Rename protected", error);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Rename leading blank", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "planet Rename name prompt", error)
		    || !session_read_command(session, name, sizeof(name)))
			return false;
		name_result = yt_planet_rename_prepare_name(name, &name_length);
		if (name_result == YT_PLANET_RENAME_EMPTY)
			return true;
		if (name_result == YT_PLANET_RENAME_RESERVED)
			return session_present_alert(session, reserved,
			    sizeof(reserved) - 1U, "planet Rename reserved", error);
		confirmation[confirmation_length++] = '"';
		memcpy(confirmation + confirmation_length, name, name_length);
		confirmation_length += name_length;
		confirmation[confirmation_length++] = '"';
		memcpy(confirmation + confirmation_length, confirmation_suffix,
		    sizeof(confirmation_suffix) - 1U);
		confirmation_length += sizeof(confirmation_suffix) - 1U;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Rename confirmation blank", error)
		    || !session_confirm(session, confirmation, confirmation_length,
		    &answer, error))
			return false;
		if (answer != YT_YES_NO_NO)
			break;
	}
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_rename_overlay(&planet, name, strlen(name));
	snprintf(session->planet_name, sizeof(session->planet_name), "%s", name);
	written = session_write_planet(session, logical_planet,
	    &planet, error);
	if (written && renamed != NULL)
		*renamed = true;
	return written;
}

bool
yt_session_planet_transfer(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Transfer items to planet>";
	static const uint8_t question[] = "Transfer which item?";
	static const uint8_t plasma_row[] = "[B] Plasma Bolts";
	static const uint8_t cargo_row[] = "[C] Cargo";
	static const uint8_t fighter_row[] = "[F] Fighters";
	static const uint8_t missile_row[] = "[S] Missiles";
	static const uint8_t mine_row[] = "[M] Mines";
	static const uint8_t selector_prompt[] = "-=>";
	struct yt_planet planet;
	char command[80];

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet Transfer title", error)
	    || !session_present_paged_line(session, question, sizeof(question) - 1U,
	    "planet Transfer question", error)
	    || !session_present_paged_line(session, plasma_row, sizeof(plasma_row) - 1U,
	    "planet Transfer plasma row", error)
	    || !session_present_paged_fragment(session, cargo_row, sizeof(cargo_row) - 1U)
	    || !session_present_paged_fragment(session, fighter_row, sizeof(fighter_row) - 1U)
	    || !session_present_paged_fragment(session, missile_row, sizeof(missile_row) - 1U)
	    || !session_present_paged_fragment(session, mine_row, sizeof(mine_row) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Transfer selector blank", error)
	    || !session_present_timed_paged_row(session, selector_prompt,
	    sizeof(selector_prompt) - 1U, "planet Transfer selector", error)
	    || !session_read_upper_command(session, command, sizeof(command)))
		return false;
	if (command[0] == '\0')
		return true;
	if (yt_planet_transfer_selector_position(command) == 0)
		return true;
	if (strcmp(command, "C") == 0) {
		struct yt_planet_economy economy;
		double held[3];
		size_t index;

		if (!session_reload_player(session, error))
			return false;
		held[0] = (double)session->player.ore;
		held[1] = (double)session->player.organics;
		held[2] = (double)session->player.equipment;
		if (!yt_session_update_planet(session, logical_planet, &planet,
		    &economy, error))
			return false;
		if (yt_planet_transfer_cargo_empty(held)) {
			static const uint8_t empty[] =
			    "You don't have any cargo!";

			return session_present_alert(session, empty, sizeof(empty) - 1U,
			    "planet Transfer no cargo", error);
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer cargo blank", error))
			return false;
		yt_planet_transfer_cargo_cache(economy.production,
		    economy.quantity, held);
		for (index = 0; index < 3; ++index) {
			int item = (int)index + 1;

			session->planet_economy.quantity[item] =
			    economy.quantity[item];
		}
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_cargo_player_overlay(&session->player);
		if (!session_write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_cargo_planet_overlay(&planet,
		    economy.production, economy.quantity, economy.contribution);
		if (!session_write_planet(session,
		    logical_planet, &planet, error))
			return false;
		{
			static const uint8_t success[] = "Cargo transferred!!";

			if (!session_present_paged_fragment(session, success, sizeof(success) - 1U))
				return false;
		}
	}
	else if (strcmp(command, "F") == 0) {
		char number[64];
		char prompt[160];
		char response[160];
		float cached_fighters = session->player.fighters;
		float amount;

		if (qb_str_double(number, sizeof(number),
		    (double)cached_fighters) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "You have%s fighters. Transfer how many -=>", number) < 0)
			return session_range_error(error,
			    "planet Transfer fighter prompt format");
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer fighter blank", error)
		    || !session_present_timed_paged_row(session, (const uint8_t *)prompt,
		    strlen(prompt), "planet Transfer fighter prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_planet_transfer_fighter_amount(response, &amount, error))
			return false;
		if (yt_planet_transfer_fighter_rejected(amount, cached_fighters))
			return true;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_fighter_player_overlay(&session->player,
		    cached_fighters, amount);
		if (!session_write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_fighter_planet_overlay(&planet,
		    session->planet_economy.quantity[4], amount);
		if (!session_write_planet(session, logical_planet,
		    &planet, error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer fighter success blank",
		    error))
			return false;
		yt_present_set_blink(&session->presentation, 1.0f);
		{
			static const uint8_t success[] = "Fighters Transferred!";

			if (!session_present_paged_fragment(session, success, sizeof(success) - 1U))
				return false;
		}
	}
	else if (strcmp(command, "B") == 0
	    || strcmp(command, "S") == 0
	    || strcmp(command, "M") == 0) {
		float *held;
		float amount;
		int item = command[0] == 'B' ? 9 : command[0] == 'S' ? 5 : 6;
		const uint8_t *success;
		size_t success_length;

		held = player_item(&session->player, item);
		amount = *held;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_direct_player_overlay(&session->player, item);
		if (!session_write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_direct_planet_overlay(&planet, item,
		    session->planet_economy.quantity[item], amount);
		if (!session_write_planet(session, logical_planet,
		    &planet, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer weapon success blank",
		    error))
			return false;
		yt_present_set_blink(&session->presentation, 1.0f);
		if (item == 9) {
			static const uint8_t text[] = "Plasma Bolts Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		else if (item == 5) {
			static const uint8_t text[] = "Missiles Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		else {
			static const uint8_t text[] = "Mines Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		if (!session_present_paged_fragment(session, success, success_length))
			return false;
	}
	if (!session_reload_player(session, error)
	    || !yt_session_update_planet(session, logical_planet, &planet,
	    NULL, error))
		return false;
	return session_sound(session, 4.0f,
	    "planet transfer sound", error);
}

bool
yt_session_planet_productivity(struct yt_session *session,
    int logical_planet,
    struct yt_error *error)
{
	static const uint8_t explanation[] =
	    "Productivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.";
	static const uint8_t prompt[] =
	    "Spend how much to raise productivity? -+> ";
	static const uint8_t insufficient[] =
	    "You dont have that many credits!";
	static const char *const fragments[4] = {
		"Also increased: Fighters:", ", Missiles:",
		", Mines:", ", Plasma Bolts:"
	};
	struct yt_planet_economy economy;
	struct yt_planet planet;
	struct qb_val_result parsed;
	char response[160];
	char credits_text[64];
	char credits_row[128];
	char units_text[64];
	char success[160];
	double spend;
	double units;
	float delta[4];
	float credit_argument;
	size_t index;

	if (!session_reload_player(session, error)
	    || !yt_session_update_planet(session, logical_planet, &planet,
	    &economy, error))
		return false;
	if (qb_str_double(credits_text, sizeof(credits_text),
	    (double)session->player.credits) < 0
	    || snprintf(credits_row, sizeof(credits_row),
	    "You have%s Credits.", credits_text) < 0
	    || !session_present_paged_line(session, explanation, sizeof(explanation) - 1U,
	    "planet Productivity explanation", error)
	    || !session_present_paged_line(session, (const uint8_t *)credits_row,
	    strlen(credits_row), "planet Productivity credits", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity pre-prompt blank", error)
	    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "planet Productivity spend prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return session_range_error(error,
		    "planet Productivity amount VAL");
	spend = qb_int(parsed.valid ? parsed.value : 0.0);
	if (spend < 1.0)
		return true;
	if (spend > (double)session->player.credits)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Productivity credit error", error);
	units = yt_planet_productivity_units(spend);
	if (qb_str_double(units_text, sizeof(units_text), units) < 0
	    || snprintf(success, sizeof(success),
	    "Productivity increased by%s units of ORE, ORG & EQU!",
	    units_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)success,
	    strlen(success), "planet Productivity accepted", error))
		return false;
	yt_planet_productivity_cache(economy.production, units, delta);
	for (index = 0; index < 4U; ++index) {
		char delta_text[64];
		char fragment[128];

		if (delta[index] == 0.0f)
			continue;
		if (qb_str_single(delta_text, sizeof(delta_text), delta[index]) < 0
		    || snprintf(fragment, sizeof(fragment), "%s%s",
		    fragments[index], delta_text) < 0
		    || !session_present_timed_paged_row(session, (const uint8_t *)fragment,
		    strlen(fragment), "planet Productivity derived fragment",
		    error))
			return false;
	}
	if (delta[0] != 0.0f
	    && !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity derived ending", error))
		return false;
	credit_argument = yt_planet_productivity_credit_argument(units);
	if (!session_mutate_player_credits(session, credit_argument, NULL, error)
	    || !session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_productivity_planet_overlay(&planet, economy.production,
	    economy.quantity, economy.contribution);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	return session_reload_player(session, error);
}

