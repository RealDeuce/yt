#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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
	    &planet, error))
		return false;
	name_length = planet.name_length;
	if (name_length > YT_TEXT_FIELD_SIZE)
		name_length = YT_TEXT_FIELD_SIZE;
	snprintf(session->planet.name, sizeof(session->planet.name), "%s",
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
				return session_range_error(error,
				    "planet inventory numeric format");
		}
		else if (index == 6) {
			produced = floor(qb_double_multiply(economy.quantity[7],
			    0x1.47ae14p-7));
			available = floor(economy.quantity[7]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_double(in_holds, sizeof(in_holds), held[index]) < 0)
				return session_range_error(error,
				    "planet inventory credit format");
		}
		else if (index == 7) {
			produced = floor(qb_double_add(qb_double_multiply(economy.quantity[8],
			    0x1.47ae14p-7), (double)economy.contribution[8]));
			available = floor(economy.quantity[8]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_single(in_holds, sizeof(in_holds),
			    (float)held[index]) < 0)
				return session_range_error(error,
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
				return session_range_error(error,
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
		return session_range_error(error, "planet Take One item");
	if (!session_present_paged_line(session, (const uint8_t *)title,
	    strlen(title), "planet Take One title", error))
		return false;
	free_holds = (float)qb_double_subtract(qb_double_subtract(qb_double_subtract(
	    (double)session->player.holds, (double)session->player.ore),
	    (double)session->player.organics),
	    (double)session->player.equipment);
	available = (float)floor(session->planet.economy.quantity[item]);
	maximum = item <= 3 && free_holds < available
	    ? free_holds : available;
	if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0
	    || snprintf(prompt, sizeof(prompt), "How much [%s ]? ",
	    maximum_text) < 0)
		return session_range_error(error, "planet Take One prompt format");
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
	if ((double)quantity > floor(session->planet.economy.quantity[item])
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
	    session->planet.economy.quantity[item], quantity);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	session->planet.economy.quantity[item] = qb_double_subtract(
	    session->planet.economy.quantity[item], (double)quantity);
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
	    session->planet.economy.quantity, amount);
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
			return session_range_error(error,
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
	    session->planet.economy.quantity, amount);
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
		    session->planet.economy.quantity[index]);
		if (!session_write_player(session, error))
			return false;
		if (!session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_take_all_commodity_planet_overlay(&planet, index,
		    session->planet.economy.quantity[index], commodity_amount);
		if (!session_write_planet(session,
		    logical_planet, &planet, error))
			return false;
		if (qb_str_single(number, sizeof(number), commodity_amount) < 0
		    || snprintf(row, sizeof(row), "%s%s",
		    commodity_labels[index - 1], number) < 0)
			return session_range_error(error,
			    "planet take-all commodity format");
		if (!session_present_paged_fragment(session, (const uint8_t *)row, strlen(row)))
			return false;
	}
	return true;
}
