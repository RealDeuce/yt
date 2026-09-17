#include "yt_session_internal.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool
earth_quantity_input(struct yt_session *session, const char *prompt,
    double *value, bool *blank, struct yt_error *error)
{
	char line[160];
	struct qb_val_result parsed;

	if (!session_present_timed_paged_row(session,
	    (const uint8_t *)prompt, strlen(prompt),
	    "Earth purchase quantity prompt", error))
		return false;
	if (!session_read_number_command(session, line, sizeof(line)))
		return false;
	*blank = line[0] == '\0';
	parsed = qb_val(line);
	if (parsed.overflow)
		return session_range_error(error, "Earth purchase quantity VAL");
	*value = parsed.valid ? parsed.value : 0.0;
	return true;
}

bool
session_earth_credit_error(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	return session_present_alert(session, (const uint8_t *)text, strlen(text),
	    "Earth purchase attention", error);
}

static bool
earth_purchase_holds(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	static const char prompt[] = "Buy how many holds? [0]? ";
	char amount[64];
	char row[128];
	double requested;
	double affordable;
	float quantity;
	float cost;
	bool blank;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth Holds leading blank", error))
		return false;
	if (session->player.holds >= session->door->game.config.maximum_holds)
		return session_earth_credit_error(session, "You dont need any holds.", error);
	if (qb_str_single(amount, sizeof(amount), qb_single_subtract(
	    session->door->game.config.maximum_holds,
	    session->player.holds)) < 0)
		return session_range_error(error, "Earth Holds needed row");
	if (snprintf(row, sizeof(row), "You need%s holds.", amount) < 0)
		return session_range_error(error, "Earth Holds needed row");
	if (!session_present_paged_line(session, (const uint8_t *)row, strlen(row),
	    "Earth Holds needed row", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (!earth_quantity_input(session, prompt, &requested, &blank, error))
		return false;
	quantity = yt_earth_purchase_quantity(requested);
	if (quantity < 1.0f)
		return true;
	if ((double)quantity > affordable)
		return session_earth_credit_error(session,
		    "You do not have enough credits!", error);
	if (qb_single_add(session->player.holds, quantity)
	    > session->door->game.config.maximum_holds)
		return session_earth_credit_error(session,
		    "You don't need that many!", error);
	session->player.holds = qb_single_add(session->player.holds, quantity);
	cost = qb_single_multiply(quantity, price);
	if (!session_write_player(session, error))
		return false;
	return session_earth_receipt(session, cached_earth, cost, error);
}

static bool
earth_purchase_supply(struct yt_session *session,
    const struct yt_port *cached_earth, int choice, float price,
    struct yt_error *error)
{
	const char *prompt;
	double requested;
	double affordable;
	float quantity;
	float cost;
	bool blank;

	if (choice == 3)
		prompt = "Buy how many fighters? [0]? ";
	else if (choice == 7)
		prompt = "Buy how many ground force units? [0]? ";
	else
		prompt = "Buy how much shield power? [0]? ";
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth supply leading blank", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (!earth_quantity_input(session, prompt, &requested, &blank, error))
		return false;
	quantity = yt_earth_purchase_quantity(requested);
	if (quantity < 1.0f)
		return true;
	if ((double)quantity > affordable)
		return session_earth_credit_error(session,
		    "You do not have enough credits!", error);
	cost = qb_single_multiply(quantity, price);
	yt_earth_supply_overlay(&session->player, choice, quantity);
	if (!session_write_player(session, error))
		return false;
	return session_earth_receipt(session, cached_earth, cost, error);
}

static bool
earth_purchase_cloak(struct yt_session *session,
    const struct yt_port *cached_earth, struct yt_error *error)
{
	for (;;) {
		char deficit_text[64];
		char default_text[64];
		char row[128];
		char prompt[192];
		double requested;
		float points;
		float deficit;
		float default_quantity;
		float quantity;
		float cost;
		bool blank;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth Cloak leading blank", error))
			return false;
		points = yt_earth_cloak_points(session->player.cloak);
		deficit = qb_single_subtract(50.0f, points);
		default_quantity = yt_earth_cloak_default(deficit,
		    session->player.credits);
		if (qb_str_single(deficit_text, sizeof(deficit_text), deficit) < 0)
			return session_range_error(error,
			    "Earth Cloak row formatting");
		if (qb_str_single(default_text, sizeof(default_text),
		    default_quantity) < 0)
			return session_range_error(error,
			    "Earth Cloak row formatting");
		if (snprintf(row, sizeof(row),
		    "Cloak energy is down by%s%%.", deficit_text) < 0)
			return session_range_error(error,
			    "Earth Cloak row formatting");
		if (snprintf(prompt, sizeof(prompt),
		    "Buy how many points of Cloak Energy? (0 -%s) [%s ] ?",
		    deficit_text, default_text) < 0)
			return session_range_error(error,
			    "Earth Cloak row formatting");
		if (!session_present_paged_row(session,
		    (const uint8_t *)row, strlen(row)))
			return false;
		if (!earth_quantity_input(session, prompt, &requested, &blank,
		    error))
			return false;
		quantity = blank ? default_quantity
		    : yt_earth_purchase_quantity(requested);
		if (quantity < 1.0f)
			return true;
		if (qb_single_add(points, quantity) > 50.0f) {
			if (!session_earth_credit_error(session,
			    "You can't have over 100% cloak!", error))
				return false;
			continue;
		}
		cost = qb_single_multiply(quantity, 1000.0f);
		if (cost > session->player.credits)
			return session_earth_credit_error(session,
			    "You do not have enough credits!", error);
		session->player.cloak = yt_earth_cloak_overlay(points, quantity);
		if (!session_write_player(session, error))
			return false;
		return session_earth_receipt(session, cached_earth, cost, error);
	}
}

static bool
earth_purchase_scanner(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	double affordable = yt_earth_affordable(session->player.credits, price);

	if (session->player.danger_scanner != 0)
		return session_earth_credit_error(session,
		    "You already HAVE a Danger Scanner!", error);
	if (floor(affordable) < 1.0)
		return session_earth_credit_error(session,
		    "You cannot afford a Danger Scanner!", error);
	session->player.danger_scanner = -1;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth Scanner leading blank", error))
		return false;
	session->presentation.bold = true;
	if (!session_present_paged_row(session,
	    (const uint8_t *)"Danger Scanner installed in your ship!",
	    strlen("Danger Scanner installed in your ship!")))
		return false;
	if (!session_write_player(session, error))
		return false;
	return session_earth_receipt(session, cached_earth, price, error);
}

static bool
earth_purchase_spies(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	for (;;) {
		char active_text[64];
		char active_row[128];
		char quantity_prompt[96];
		double requested;
		double affordable;
		float quantity_value;
		float cost;
		int quantity;
		int spy_index;
		int active_count = session->spies.count;
		bool blank;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth Spies leading blank", error))
			return false;
		affordable = yt_earth_affordable(session->player.credits, price);
		if (active_count != 0) {
			if (qb_str_single(active_text, sizeof(active_text),
			    (float)active_count) < 0)
				return session_range_error(error,
				    "Earth Spies active row");
			if (snprintf(active_row, sizeof(active_row),
			    "You have%s spies active already.", active_text) < 0)
				return session_range_error(error,
				    "Earth Spies active row");
			session->presentation.bold = true;
			if (!session_present_paged_fragment(session, (const uint8_t *)active_row,
			    strlen(active_row)))
				return false;
		}
		if (snprintf(quantity_prompt, sizeof(quantity_prompt),
		    "Hire how many%s spies? [0]? ",
		    active_count != 0 ? " more" : "") < 0)
			return session_range_error(error,
			    "Earth Spies quantity prompt");
		if (!earth_quantity_input(session, quantity_prompt, &requested,
		    &blank, error))
			return false;
		quantity_value = yt_earth_purchase_quantity(requested);
		if (quantity_value < 1.0f)
			return true;
		if ((double)quantity_value > affordable)
			return session_earth_credit_error(session,
			    "You do not have enough credits!", error);
		if ((float)active_count + quantity_value > 3.0f) {
			if (!session_earth_credit_error(session,
			    "Max spies allowed is 3!", error))
				return false;
			continue;
		}
		quantity = (int)quantity_value;
		cost = qb_single_multiply(quantity_value, price);
		for (spy_index = 0; spy_index < quantity; ++spy_index) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "Earth Spy assignment blank", error))
				return false;
			for (;;) {
				char number[64];
				char prompt[128];
				double sector_value;
				float sector;
				bool overflow;
				int32_t selected;

				if (qb_str_single(number, sizeof(number),
				    (float)(active_count + spy_index + 1)) < 0)
					return false;
				if (snprintf(prompt, sizeof(prompt),
				    "Start spy #%s in what sector?", number) < 0)
					return false;
				if (!earth_quantity_input(session, prompt,
				    &sector_value, &blank, error))
					return false;
				sector = yt_earth_purchase_quantity(sector_value);
				if (sector == 0.0f
				    || sector > (float)session_sector_count(session))
					continue;
				selected = qb_cint(sector, &overflow);
				if (overflow)
					return session_range_error(error,
					    "Earth Spy sector CINT");
				session->spies.sectors[active_count + spy_index] =
				    (int)selected;
				break;
			}
		}
		session->spies.count = active_count + quantity;
		if (!yt_session_list_spies(session, error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "spy purchase pause blank", error))
			return false;
		if (!session_press_any_key(session, false, error))
			return false;
		if (!session_write_player(session, error))
			return false;
		return session_earth_receipt(session, cached_earth, cost, error);
	}
}

static bool
earth_anti_cloak(struct yt_session *session, float price,
    struct yt_error *error)
{
	static const uint8_t activation[] =
	    "ti-Cloaking device activated!\xd4" "D";
	static const uint8_t waves[] =
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl";
	static const uint8_t uncloaked[] = " is uncloaked!";
	static const uint8_t none[] = "Too bad noone was cloaked anyhow!";
	static const uint8_t fade[] = "...the effect fades.";
	struct yt_player field_player;
	uint8_t row[YT_TEXT_FIELD_SIZE + sizeof(uncloaked) - 1U];
	int player_terminal = session_sector_offset(session);
	int player_record;
	bool field_loaded = false;
	bool reported = false;

	if (!session_present_text(session, activation,
	    sizeof(activation) - 1U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error))
		return false;
	if (session->presentation.foreground != 2)
		session_set_color(session, 2);
	if (!session_present_text(session, waves, sizeof(waves) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error))
		return false;
	if (!session_sound(session, YT_SOUND_CUE_ATTACK,
	    "anti-cloak transaction sound", error))
		return false;
	for (player_record = YT_PLAYER_FIRST_RECORD;
	    player_record <= player_terminal; ++player_record) {
		size_t name_length;

		if (yt_player_cache_cloak(&session->player_cache,
		    player_record) <= 0.0f)
			continue;
		(void)yt_player_cache_set_cloak(&session->player_cache,
		    player_record, 0.0f);
		if (!yt_game_read_player(&session->door->game, player_record,
		    &field_player, error)) {
			if (session->presentation.foreground != 6)
				session_set_color(session, 6);
			if (field_loaded)
				session->player.record = field_player.record;
			return false;
		}
		field_loaded = true;
		if (field_player.killed_by != 0)
			continue;
		name_length = yt_player_stored_name(&field_player, row);
		memcpy(row + name_length, uncloaked, sizeof(uncloaked) - 1U);
		if (session->presentation.foreground != 6)
			session_set_color(session, 6);
		if (!session_present_text(session, row,
		    name_length + sizeof(uncloaked) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error)) {
			session->player.record = field_player.record;
			return false;
		}
		if (!session_sound(session, YT_SOUND_CUE_REWARD,
		    "anti-cloak transaction sound", error)) {
			session->player.record = field_player.record;
			return false;
		}
		reported = true;
	}
	if (session->presentation.foreground != 2)
		session_set_color(session, 2);
	if (!reported) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "anti-cloak transaction row", error)) {
			if (field_loaded)
				session->player.record = field_player.record;
			return false;
		}
		if (!session_present_text(session, none, sizeof(none) - 1U,
		    SESSION_PRESENT_LINE, "anti-cloak transaction row", error)) {
			if (field_loaded)
				session->player.record = field_player.record;
			return false;
		}
	}
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error)) {
		if (field_loaded)
			session->player.record = field_player.record;
		return false;
	}
	if (!session_present_text(session, fade, sizeof(fade) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error)) {
		if (field_loaded)
			session->player.record = field_player.record;
		return false;
	}
	if (!session_sound(session, YT_SOUND_CUE_DAMAGE,
	    "anti-cloak transaction sound", error)) {
		if (field_loaded)
			session->player.record = field_player.record;
		return false;
	}
	{
		bool credit_loaded = false;

		if (!session_mutate_player_credits(session, -price,
		    &credit_loaded, error)) {
			if (!credit_loaded && field_loaded)
				session->player.record = field_player.record;
			return false;
		}
	}
	session_set_color(session, 3);
	return true;
}

bool
yt_session_earth_store(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t menu[] =
	    "[I] Ship Info -=*=- [0] Leave Port";
	static const uint8_t prompt_suffix[] =
	    " -=*=- Buy Which Item? -=>";
	static const uint8_t invalid[] = "INAVLID CHOICE!";

	if (enter_sector != NULL)
		*enter_sector = false;
	for (;;) {
		struct yt_port earth;
		struct qb_val_result parsed;
		char line[80];
		char credits_text[64];
		char prompt[160];
		float price[4];
		int choice;
		float holds_price;
		float fighters_price;
		float shields_price;
		float ground_price;

		if (!session_earth_report(session, &earth, price, error))
			return false;
		holds_price = price[0];
		fighters_price = price[1];
		shields_price = price[2];
		ground_price = price[3];
		if (!session_present_paged_line(session, menu, sizeof(menu) - 1U,
		    "Earth report menu", error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth prompt blank", error))
			return false;
		if (qb_str_double(credits_text, sizeof(credits_text),
		    (double)session->player.credits) < 0)
			return false;
		if (snprintf(prompt, sizeof(prompt), "Credits:%s%s",
		    credits_text, prompt_suffix) < 0)
			return false;
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)prompt, strlen(prompt),
		    "Earth item prompt", error))
			return false;
		if (!session_read_upper_command(session, line, sizeof(line)))
			return false;
		if (line[0] == '\0')
			continue;
		parsed = qb_val(line);
		if (parsed.overflow)
			return session_range_error(error, "Earth menu VAL");
		{
			bool overflow;
			float selected = (float)(parsed.valid ? parsed.value : 0.0);

			choice = (int)qb_cint(selected, &overflow);
			if (overflow)
				return session_range_error(error,
				    "Earth menu CINT");
		}
		if (strcmp(line, "S") == 0) {
			if (!yt_session_display_sector(session, true, error))
				return false;
			if (!session_wait(session, 9.0,
			    "Earth Sensors wait", error))
				return false;
			continue;
		}
		if (strcmp(line, "I") == 0) {
			if (!yt_session_show_ship(session, error))
				return false;
			if (!session_wait(session, 9.0,
			    "Earth Info wait", error))
				return false;
			continue;
		}
		if (choice < 1 || choice > 9) {
			int position;

			session->earth.report_seen = false;
			position = yt_earth_selector_position(line);
			if (position == 0) {
				if (!session_present_alert(session, invalid,
				    sizeof(invalid) - 1U,
				    "Earth invalid choice", error))
					return false;
				continue;
			}
			switch (position) {
			case 1: {
				bool selected = false;

				if (!yt_session_command_land(session, &selected, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = selected;
				return true;
			}
			case 2: {
				bool moved;

				if (!yt_session_command_move(session, &moved, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = moved;
				return true;
			}
			case 3:
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 4: {
				bool selected = false;

				if (!yt_session_computer_menu(session, &selected, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = selected;
				return true;
			}
			default:
				return false;
			}
		}
		if (choice == 4) {
			if (!session_earth_lottery(session, &earth, error))
				return false;
			continue;
		}
		if (choice == 5) {
			if (!earth_purchase_scanner(session, &earth, 500000.0f,
			    error))
				return false;
			continue;
		}
		if (choice == 6) {
			static const uint8_t confirmation[] =
			    "Anti-Cloaking Device works for this logon only. "
			    "Buy one? [y/N]";
			static const uint8_t pause[] = "Hit [Enter]";
			enum yt_yes_no_answer answer;
			char response[80];

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak leading blank",
			    error))
				return false;
			if (session->player.credits < 1000000000.0f) {
				if (!session_earth_credit_error(session,
				    "You do not have enough credits!", error))
					return false;
				continue;
			}
			if (!session_confirm(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer != YT_YES_NO_YES)
				continue;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak accepted blank",
			    error))
				return false;
			if (!earth_anti_cloak(session, 1000000000.0f, error))
				return false;
			session->earth.anti_cloak_enabled = true;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak pause blank", error))
				return false;
			if (!session_present_timed_paged_row(session, pause,
			    sizeof(pause) - 1U, "Earth Anti-Cloak pause prompt", error))
				return false;
			if (!session_read_command(session, response, sizeof(response)))
				return false;
			continue;
		}
		if (choice == 9) {
			if (!earth_purchase_spies(session, &earth, 1000000000.0f,
			    error))
				return false;
			continue;
		}
		if (choice == 1) {
			if (!earth_purchase_cloak(session, &earth, error))
				return false;
		}
		else if (choice == 2) {
			if (!earth_purchase_holds(session, &earth, holds_price, error))
				return false;
		}
		else if (choice == 3 || choice == 7 || choice == 8) {
			float selected_price = choice == 3 ? fighters_price
			    : choice == 7 ? ground_price : shields_price;

			if (!earth_purchase_supply(session, &earth, choice,
			    selected_price, error))
				return false;
		}
	}
}
