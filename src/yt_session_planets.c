#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"
#include "yt_port_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_session_update_planet_physical(struct yt_session *session,
    uint32_t physical_record, struct yt_planet *planet,
    struct yt_planet_economy *economy, struct yt_error *error)
{
	struct yt_planet_economy updated_economy;
	struct yt_planet_update update;
	struct yt_record record;
	int today;
	int adjusted_year;
	float timer_seconds;

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	if (!yt_planet_update_prepare(&record, &update, error))
		return false;
	timer_seconds = (float)yt_platform_timer();
	if (!yt_planet_update_record(&record, &update, (float)today,
	    timer_seconds, &updated_economy, error)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	yt_planet_decode(planet, &record);
	session->planet_economy = updated_economy;
	if (economy != NULL)
		*economy = updated_economy;
	return true;
}

static bool
planet_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
planet_name_length(struct yt_session *session, float raw, size_t *length,
    struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)raw,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow || converted < 0)
		return planet_error(error, "planet inventory name length");
	*length = (size_t)converted;
	if (*length > YT_TEXT_FIELD_SIZE)
		*length = YT_TEXT_FIELD_SIZE;
	return true;
}

static double
planet_double_add(double left, double right)
{
	volatile double result = left + right;
	return result;
}

static double
planet_double_sub(double left, double right)
{
	volatile double result = left - right;
	return result;
}

static double
planet_double_mul(double left, double right)
{
	volatile double result = left * right;
	return result;
}
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
yt_session_planet_inventory(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	struct yt_planet_economy economy;
	static const char *labels[9] = {
		"Ore..........", "Organics.....", "Equipment....",
		"Fighters.....", "Missiles.....", "Mines........",
		"Credits......", "Forces.......", "Plasma bolts."
	};
	static const uint8_t header[] =
	    " Item           Production     Amount    In Holds";
	static const uint8_t rule[] =
	    "=============  ============   ========  ==========";
	double held[9];
	uint8_t title[64];
	size_t title_length = 0;
	size_t name_length;
	int index;

	if (!session_reload_player(session, error)
	    || !yt_session_update_planet(session, logical_planet, &planet,
	    &economy, error)
	    || !session_read_planet(session, logical_planet,
	    &planet, error)
	    || !planet_name_length(session, planet.name_length,
	    &name_length, error))
		return false;
	snprintf(session->planet_name, sizeof(session->planet_name), "%s",
	    planet.name);
	held[0] = (double)session->player.ore;
	held[1] = (double)session->player.organics;
	held[2] = (double)session->player.equipment;
	held[3] = (double)session->player.fighters;
	held[4] = (double)session->player.missiles;
	held[5] = (double)session->player.mines;
	held[6] = (double)session->player.credits;
	held[7] = (double)session->player.ground_forces;
	held[8] = (double)session->player.plasma;
	memcpy(title, "Planet: ", strlen("Planet: "));
	title_length = strlen("Planet: ");
	memcpy(title + title_length, planet.record.bytes, name_length);
	title_length += name_length;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet inventory title blank", error)
	    || !session_present_paged_fragment(session, title, title_length)
	    || !session_present_paged_line(session, header, sizeof(header) - 1U,
	    "planet inventory header", error)
	    || !session_present_paged_fragment(session, rule, sizeof(rule) - 1U))
		return false;
	for (index = 0; index < 9; ++index) {
		char production[64];
		char amount[64];
		char in_holds[64];
		double produced;
		double available;

		if (index < 6) {
			produced = (double)floorf(economy.production[index + 1]);
			available = floor(economy.quantity[index + 1]);
			if (qb_str_single(production, sizeof(production),
			    (float)produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_double(in_holds, sizeof(in_holds),
			    held[index]) < 0)
				return planet_error(error,
				    "planet inventory numeric format");
		}
		else if (index == 6) {
			produced = floor(planet_double_mul(economy.quantity[7],
			    0x1.47ae14p-7));
			available = floor(economy.quantity[7]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_double(in_holds, sizeof(in_holds), held[index]) < 0)
				return planet_error(error,
				    "planet inventory credit format");
		}
		else if (index == 7) {
			produced = floor(planet_double_add(planet_double_mul(economy.quantity[8],
			    0x1.47ae14p-7), (double)economy.contribution[8]));
			available = floor(economy.quantity[8]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_single(in_holds, sizeof(in_holds),
			    (float)held[index]) < 0)
				return planet_error(error,
				    "planet inventory force format");
		}
		else {
			produced = (double)floorf(economy.production[9]);
			available = floor(economy.quantity[9]);
			if (qb_str_single(production, sizeof(production),
			    (float)produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_single(in_holds, sizeof(in_holds),
			    (float)held[index]) < 0)
				return planet_error(error,
				    "planet inventory plasma format");
		}
		if (!session_present_text(session, (const uint8_t *)labels[index],
		    strlen(labels[index]), SESSION_PRESENT_RAW,
		    "planet inventory label", error)
		    || !session_right_aligned(session, production, 13.0f,
		    "planet inventory production", error)
		    || !session_right_aligned(session, amount, 11.0f,
		    "planet inventory amount", error)
		    || !session_right_aligned(session, in_holds, 12.0f,
		    "planet inventory holds", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet inventory row ending", error))
			return false;
	}
	return true;
}

bool
yt_session_planet_take_one(struct yt_session *session, int logical_planet,
    int item,
    struct yt_error *error)
{
	struct yt_planet planet;
	const char *title = yt_planet_take_one_title(item);
	float free_holds;
	float available;
	float maximum;
	float quantity;
	char maximum_text[64];
	char prompt[100];
	char response[160];
	struct qb_val_result parsed;

	if (title == NULL)
		return planet_error(error, "planet Take One item");
	if (!session_present_paged_line(session, (const uint8_t *)title,
	    strlen(title), "planet Take One title", error))
		return false;
	free_holds = (float)planet_double_sub(planet_double_sub(planet_double_sub(
	    (double)session->player.holds, (double)session->player.ore),
	    (double)session->player.organics),
	    (double)session->player.equipment);
	available = (float)floor(session->planet_economy.quantity[item]);
	maximum = item <= 3 && free_holds < available
	    ? free_holds : available;
	if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0
	    || snprintf(prompt, sizeof(prompt), "How much [%s ]? ",
	    maximum_text) < 0)
		return planet_error(error, "planet Take One prompt format");
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Take One prompt blank", error)
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Take One amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		quantity = maximum;
	else {
		parsed = qb_val(response);
		quantity = (float)floor(parsed.valid ? parsed.value : 0.0);
	}
	if ((double)quantity > floor(session->planet_economy.quantity[item])
	    || quantity < 0.0f) {
		static const uint8_t stock[] = "They don't have that many.";

		return session_present_alert(session, stock, sizeof(stock) - 1U,
		    "planet Take One stock error", error);
	}
	if (quantity > maximum) {
		static const uint8_t capacity[] = "You can't take that much!";

		return session_present_alert(session, capacity, sizeof(capacity) - 1U,
		    "planet Take One capacity error", error);
	}
	if (!session_reload_player(session, error))
		return false;
	yt_planet_take_one_player_overlay(&session->player, item, quantity);
	if (!session_write_player(session, error))
		return false;
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_one_planet_overlay(&planet, item,
	    session->planet_economy.quantity[item], quantity);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	session->planet_economy.quantity[item] = planet_double_sub(
	    session->planet_economy.quantity[item], (double)quantity);
	return session_reload_player(session, error);
}

bool
yt_session_planet_take_all(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	static const uint8_t title[] = "<Take all>";
	static const uint8_t taking[] = "Taking:";
	static const char *const weapon_labels[4] = {
		"Fighters.....", "Missiles.....", "Mines........",
		"Plasma bolts."
	};
	static const int weapon_items[4] = {4, 5, 6, 9};
	static const char *const commodity_labels[3] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	double amount[10];
	int index;

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet take-all title", error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_take_all_weapon_player_overlay(&session->player,
	    session->planet_economy.quantity, amount);
	if (!session_write_player(session, error)
	    || !session_present_paged_line(session, taking, sizeof(taking) - 1U,
	    "planet take-all taking", error))
		return false;
	for (index = 0; index < 4; ++index) {
		char number[64];
		char row[96];
		int item = weapon_items[index];

		if ((index == 0
		    ? qb_str_double(number, sizeof(number), amount[item])
		    : qb_str_single(number, sizeof(number), (float)amount[item])) < 0
		    || snprintf(row, sizeof(row), "%s%s", weapon_labels[index],
		    number) < 0)
			return planet_error(error,
			    "planet take-all weapon format");
		if (index == 0) {
			if (!session_present_paged_line(session, (const uint8_t *)row,
			    strlen(row), "planet take-all fighters", error))
				return false;
		}
		else if (!session_present_paged_fragment(session, (const uint8_t *)row,
		    strlen(row)))
			return false;
	}
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_all_weapon_planet_overlay(&planet,
	    session->planet_economy.quantity, amount);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	for (index = 3; index >= 1; --index) {
		char number[64];
		char row[96];
		float commodity_amount;

		if (!session_reload_player(session, error))
			return false;
		commodity_amount = yt_planet_take_all_commodity_player_overlay(
		    &session->player, index,
		    session->planet_economy.quantity[index]);
		if (!session_write_player(session, error))
			return false;
		if (!session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_take_all_commodity_planet_overlay(&planet, index,
		    session->planet_economy.quantity[index], commodity_amount);
		if (!session_write_planet(session,
		    logical_planet, &planet, error))
			return false;
		if (qb_str_single(number, sizeof(number), commodity_amount) < 0
		    || snprintf(row, sizeof(row), "%s%s",
		    commodity_labels[index - 1], number) < 0)
			return planet_error(error,
			    "planet take-all commodity format");
		if (!session_present_paged_fragment(session, (const uint8_t *)row, strlen(row)))
			return false;
	}
	return true;
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
		return planet_error(error, "planet Bank amount VAL");
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
			return planet_error(error,
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

		current_record = yt_port_single_add(
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
			return planet_error(error,
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
		return planet_error(error,
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

bool
yt_session_planet_assault(struct yt_session *session,
    uint32_t physical_planet,
    float commitment, bool *defeated, struct yt_error *error)
{
	static const uint8_t engaging[] = "Forces engaging!";
	static const uint8_t defenses[] = "Planetary defenses destroyed!";
	static const uint8_t defenses_news[] =
	    " +++ Planetary defenses destroyed!";
	static const uint8_t captured[] = "You've captured the planet!";
	struct yt_planet planet;
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[320];
	size_t player_name_length;
	size_t planet_name_length;
	size_t row_length;
	float attackers = commitment;
	float defenders;
	float saved_foreground;

	if (defeated == NULL)
		return false;
	*defeated = false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault entry blank", error))
		return false;
	saved_foreground = session->foreground;
	if (!yt_session_update_planet_physical(session, physical_planet, &planet,
	    NULL, error)
	    || !read_planet_physical(session, physical_planet, &planet, error)
	    || !yt_planet_stored_name(&planet, planet_name,
	    &planet_name_length, error)
	    || !session_reload_player(session, error))
		return false;
	defenders = floorf(planet.ground_forces);
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	yt_planet_assault_player_overlay(&session->player, commitment);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_planet_assault_attack_news(player_name, player_name_length,
	    planet_name, planet_name_length, commitment, row, sizeof(row),
	    &row_length)
	    || !session_append_news_bytes(session, row, row_length, error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, engaging, sizeof(engaging) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "planet assault engagement row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault engagement blank", error)
	    || !session_sound(session, 2.0f,
	    "planet assault engagement sound", error))
		return false;
	while (attackers > 0.0f && defenders > 0.0f) {
		float side;
		float amount;
		bool attacker_damage;

		if (!yt_random_next(&session->door->game.random, &side, error))
			return false;
		attacker_damage = side > 0.4000000059604645f;
		if (!yt_random_next(&session->door->game.random, &amount, error))
			return false;
		yt_planet_assault_round(attacker_damage, amount, &attackers,
		    &defenders);
		session_set_foreground(session, attacker_damage ? 3.0f : 4.0f);
		if (!yt_planet_assault_status_row(attacker_damage,
		    attacker_damage ? attackers : defenders, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet assault force-status row", error))
			return false;
		if (!attacker_damage
		    && !session_sound(session, 2.0f,
			    "planet assault defender sound", error))
			return false;
	}
	session_set_foreground(session, saved_foreground);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault terminal blank", error))
		return false;
	if (defenders <= 0.0f) {
		float owner = 0.0f;

		if (!session_present_text(session, defenses,
		    sizeof(defenses) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "planet assault defenses-destroyed row", error)
		    || !session_append_news_bytes(session, defenses_news,
		    sizeof(defenses_news) - 1U, error)
		    || !session_sound(session, 1.0f,
		    "planet defenses destroyed sound", error))
			return false;
		if (attackers > 0.0f) {
			owner = (float)session_record(session);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "planet assault capture blank", error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, captured,
			    sizeof(captured) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet assault capture row", error)
			    || !yt_planet_assault_capture_news(player_name,
			    player_name_length, planet_name, planet_name_length,
			    row, sizeof(row), &row_length)
			    || !session_append_news_bytes(session, row, row_length, error)
			    || !session_sound(session, 1.0f,
			    "planet capture sound", error))
				return false;
		}
		else
			attackers = 0.0f;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
		yt_planet_assault_victory_overlay(&planet, owner, attackers);
		return session_write_planet_physical(session, physical_planet,
		    &planet, false, error);
	}
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	yt_planet_assault_failure_overlay(&planet, defenders);
	if (!session_write_planet_physical(session, physical_planet, &planet,
	    false, error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!yt_planet_assault_failure_row(defenders, true, row, sizeof(row),
	    &row_length)
	    || !session_append_news_bytes(session, row, row_length, error)
	    || !yt_planet_assault_failure_row(defenders, false, row,
	    sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_BOLD_LINE, "planet assault failure row", error))
		return false;
	*defeated = true;
	return true;
}



bool
yt_session_update_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_planet_economy *economy,
    struct yt_error *error)
{
	return yt_session_update_planet_physical(session,
	    session_planet_basic_record(session, (float)logical_planet), planet,
	    economy, error);
}

bool
yt_session_planet_permission(struct yt_session *session,
    int logical_planet, bool *denied, struct yt_error *error)
{
	static const uint8_t governor[] =
	    "This planet has no governor! Hail to the new planetary governor!!";
	static const uint8_t unrest[] =
	    "Due to the unrest caused by the lack of planetary govornment, ";
	static const uint8_t permission[] = "Permission to land is ";
	static const uint8_t denial[] = "DENIED!";
	struct yt_planet planet;
	struct yt_planet fresh;
	struct yt_player current;
	struct yt_player owner;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
	size_t cached_name_length;
	uint8_t row[256];
	size_t row_length;
	float cached_owner;
	float cached_ground_forces;
	float first_draw;
	float second_draw;
	float reduced_ground_forces;
	uint32_t physical_planet_record;
	int owner_record;
	bool friendly = false;
	bool vacant;

	if (denied == NULL)
		return false;
	*denied = false;
	physical_planet_record = session_planet_basic_record(session,
	    (float)logical_planet);
	if (!yt_session_update_planet_physical(session, physical_planet_record,
	    &(struct yt_planet){0}, NULL, error)
	    || !read_planet_physical(session, physical_planet_record,
	    &planet, error)
	    || !yt_planet_stored_name(&planet, cached_name,
	    &cached_name_length, error))
		return false;
	cached_owner = planet.owner;
	cached_ground_forces = planet.ground_forces;
	if (floorf(cached_ground_forces) <= 0.0f
	    || cached_owner == (float)session_record(session))
		return true;
	if (cached_owner >= 2.0f
	    && cached_owner <= (float)YT_PLAYER_LAST_RECORD) {
		owner_record = (int)cached_owner;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &current, error))
			return false;
		if (current.team != 0.0f) {
			if (!yt_game_read_player(&session->door->game,
			    owner_record, &owner, error))
				return false;
			friendly = yt_sector_force_same_team(current.team,
			    owner.team);
		}
	}
	if (friendly)
		return true;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "planet permission late blank", error))
		return false;
	vacant = cached_owner == 0.0f;
	if (!vacant && cached_owner >= 2.0f
	    && cached_owner <= (float)YT_PLAYER_LAST_RECORD) {
		owner_record = (int)cached_owner;
		if (!yt_game_read_player(&session->door->game, owner_record,
		    &owner, error))
			return false;
		vacant = owner.killed_by != 0.0f;
	}
	if (vacant) {
		if (!session_present_text(session, governor,
		    sizeof(governor) - 1U, SESSION_PRESENT_LINE,
		    "vacant planet governor row", error)
		    || !session_sound(session, 1.0f, "vacant planet sound", error)
		    || !session_wait(session, 2.0,
		    "vacant-planet governor wait", error)
		    || !read_planet_physical(session, physical_planet_record,
		    &fresh, error)
		    || !yt_random_next(&session->door->game.random, &first_draw,
		    error)
		    || !yt_random_next(&session->door->game.random, &second_draw,
		    error))
			return false;
		reduced_ground_forces = yt_planet_landing_attrition(first_draw,
		    second_draw, cached_ground_forces);
		if (!session_present_text(session, unrest, sizeof(unrest) - 1U,
		    SESSION_PRESENT_LINE, "vacant planet unrest row", error)
		    || !yt_planet_landing_unrest_row(reduced_ground_forces,
		    cached_ground_forces, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "vacant planet reduction row", error))
			return false;
		yt_planet_landing_vacancy_overlay(&fresh,
		    reduced_ground_forces, session_record(session));
		if (!session_write_planet_physical(session,
		    physical_planet_record, &fresh, false, error)
		    || !session_wait(session, 5.0,
		    "vacant-planet unrest wait", error))
			return false;
		return true;
	}
	if (!yt_planet_landing_traffic_row(cached_name, cached_name_length,
	    row, sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "planet permission traffic row", error)
	    || !session_present_text(session, permission,
	    sizeof(permission) - 1U, SESSION_PRESENT_RAW,
	    "planet permission prefix", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	session_set_foreground(session, 3.0f);
	if (!session_present_text(session, denial, sizeof(denial) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "planet permission denial", error))
		return false;
	*denied = true;
	session_set_foreground(session, 6.0f);
	return true;
}
