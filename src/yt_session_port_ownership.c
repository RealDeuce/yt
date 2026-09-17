#include "yt_session_internal.h"

#include "qb.h"
#include "yt_port_math.h"

#include <stdio.h>
#include <string.h>

static bool
treasury_error(struct yt_error *error, const char *operation)
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

static bool
treasury_format_single(const char *prefix, float value, char *text,
    size_t capacity, struct yt_error *error, const char *operation)
{
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), value);
	int result;

	if (number_length < 0)
		return treasury_error(error, operation);
	result = snprintf(text, capacity, "%s%s", prefix, number);
	if (result < 0 || (size_t)result >= capacity)
		return treasury_error(error, operation);
	return true;
}

static bool
treasury_format_double(const char *prefix, const uint8_t raw[8],
    const char *suffix, char *text, size_t capacity,
    struct yt_error *error, const char *operation)
{
	char number[96];
	int number_length = qb_str_mbf64(number, sizeof(number), raw);
	int result;

	if (number_length < 0)
		return treasury_error(error, operation);
	result = snprintf(text, capacity, "%s%s%s", prefix, number,
	    suffix == NULL ? "" : suffix);
	if (result < 0 || (size_t)result >= capacity)
		return treasury_error(error, operation);
	return true;
}

static bool
treasury_add(uint8_t total[8], const uint8_t value[4],
    struct yt_error *error)
{
	uint8_t promoted[8];
	uint8_t sum[8];

	yt_port_mbf64_promote_single(value, promoted);
	if (qb_mbf64_add_raw(total, promoted, sum) != QB_MBF_OK)
		return treasury_error(error, "treasury MBF56 accumulation");
	memcpy(total, sum, sizeof(sum));
	return true;
}

static bool
treasury_update_player(struct yt_player *player, float owned,
    const uint8_t total[8], struct yt_error *error)
{
	uint8_t fresh_credits[8];
	uint8_t summed_credits[8];
	uint8_t stored_credits[4];
	uint8_t stored_owned[4];

	yt_port_mbf64_promote_single(player->record.bytes + YT_F81,
	    fresh_credits);
	if (qb_mbf64_add_raw(fresh_credits, total, summed_credits) != QB_MBF_OK
	    || qb_mbf32_from_mbf64_raw(summed_credits, stored_credits)
	    == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(owned, stored_owned) == QB_MBF_OVERFLOW
	    || !yt_record_set_raw_number(&player->record, YT_F117,
	    stored_owned)
	    || !yt_record_set_raw_number(&player->record, YT_F81,
	    stored_credits))
		return treasury_error(error, "treasury player overlay");
	player->ports_owned = qb_mbf32_decode(stored_owned);
	player->credits = qb_mbf32_decode(stored_credits);
	return true;
}

bool
yt_session_treasury(struct yt_session *session, bool collecting,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	static const uint8_t no_ports[] = "You don't OWN any ports!!!";
	static const uint8_t collect_prefix[] =
	    "Sending out armored cargo ships to";
	static const uint8_t report_prefix[] =
	    "Checking galactic bank statement for";
	static const uint8_t heading_suffix[] = " ports with credits...";
	struct yt_player player;
	struct yt_port port;
	struct yt_record record;
	uint8_t total[8] = {0};
	char text[192];
	float loop_bound;
	float counter;
	float owned = 0.0f;
	float credited = 0.0f;
	float barren;
	uint32_t player_record;

	if (session == NULL)
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury opening blank", error))
		return false;
	player_record = (uint32_t)session_record(session);
	if (player_record == 0U)
		return treasury_error(error, "treasury player record conversion");
	if (!yt_database_read(&session->door->game.database,
	    (size_t)player_record, &record, error))
		return false;
	yt_player_decode(&player, &record);
	if (player.ports_owned < 1.0f) {
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, no_ports,
		    sizeof(no_ports) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "treasury no-owned notice", error);
	}
	if (!session_present_text(session,
	    collecting ? collect_prefix : report_prefix,
	    collecting ? sizeof(collect_prefix) - 1U
	    : sizeof(report_prefix) - 1U,
	    SESSION_PRESENT_RAW, "treasury heading prefix", error)
	    || !session_present_text(session, heading_suffix,
	    sizeof(heading_suffix) - 1U, SESSION_PRESENT_LINE,
	    "treasury heading suffix", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury scan blank", error))
		return false;
	loop_bound = qb_single_subtract(session_planet_offset(session),
	    session_port_offset(session));
	for (counter = 1.0f; counter <= loop_bound;
	    counter = qb_single_add(counter, 1.0f)) {
		float expression = qb_single_add(session_port_offset(session),
		    counter);
		uint32_t physical_record = qb_brun_random_record_number(expression);

		if (physical_record == 0U)
			return treasury_error(error,
			    "treasury port record conversion");
		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical_record, &record, error))
			return false;
		yt_port_decode(&port, &record);
		if (port.owner != (float)session_record(session))
			continue;
		owned = qb_single_add(owned, 1.0f);
		if (port.owner == 0.0f)
			continue;
		credited = qb_single_add(credited, 1.0f);
		if (!treasury_add(total, port.record.bytes + YT_F89, error)
		    || !treasury_format_single("Sector:", port.sector, text,
		    sizeof(text), error, "treasury sector field")
		    || !session_fixed_width_bytes(session, (const uint8_t *)text,
		    strlen(text), 14.0f, "treasury sector field", error))
			return false;
		{
			size_t name_length = port.name_length;

			if (name_length > YT_TEXT_FIELD_SIZE)
				name_length = YT_TEXT_FIELD_SIZE;
			if (!session_fixed_width_bytes(session, port.record.bytes,
			    name_length, 25.0f, "treasury port-name field", error))
				return false;
		}
		if (!treasury_format_single(" Credits:", port.treasury, text,
		    sizeof(text), error, "treasury credit field")
		    || !session_fixed_width_bytes(session, (const uint8_t *)text,
		    strlen(text), 20.0f, "treasury credit field", error)
		    || !treasury_format_double(" Total:", total, NULL, text,
		    sizeof(text), error, "treasury row total")
		    || !session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE, "treasury row total",
		    error))
			return false;
		if (collecting) {
			port.treasury = 0.0f;
			if (!yt_record_set_raw_number(&port.record, YT_F89,
			    dirty_zero)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)physical_record, &port.record, error))
				return false;
		}
	}
	if (total[7] != 0U
	    && !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury nonzero-total blank", error))
		return false;
	if (!treasury_format_single("Total ports...:", owned, text,
	    sizeof(text), error, "treasury total ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury total ports", error)
	    || !treasury_format_single("With credits..:", credited, text,
	    sizeof(text), error, "treasury credited ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury credited ports",
	    error))
		return false;
	barren = qb_single_subtract(owned, credited);
	if (!treasury_format_single("Barren ports..:", barren, text,
	    sizeof(text), error, "treasury barren ports")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury barren ports", error)
	    || !treasury_format_double("Total credits.:", total, NULL, text,
	    sizeof(text), error, "treasury total credits")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE, "treasury total credits", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "treasury summary blank", error))
		return false;
	if (!collecting) {
		if (!treasury_format_double("You have", total,
		    " credits in your port accounts.", text, sizeof(text), error,
		    "treasury report result"))
			return false;
		return session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE,
		    "treasury report result", error);
	}
	if (!treasury_format_double("You collected a total of", total,
	    " credits.", text, sizeof(text), error,
	    "treasury collection result")
	    || !session_present_text(session, (const uint8_t *)text,
	    strlen(text), SESSION_PRESENT_LINE,
	    "treasury collection result", error)
	    || !yt_database_read(&session->door->game.database,
	    (size_t)player_record, &record, error))
		return false;
	yt_player_decode(&player, &record);
	if (!treasury_update_player(&player, owned, total, error)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->player = player;
	return true;
}

bool
yt_session_edit_port_name(struct yt_session *session, int logical_port,
    const uint8_t *cached, size_t cached_length, struct yt_port *port,
    struct yt_error *error)
{
	static const uint8_t keep[] = "Press [ENTER] to keep same name.";
	static const uint8_t instruction[] =
	    "Please enter a NAME for your port.";
	static const uint8_t name_prompt[] = "-=> ";
	uint8_t entered[YT_COMMAND_SIZE];
	uint8_t candidate[YT_COMMAND_SIZE];
	uint8_t row[YT_COMMAND_SIZE];
	size_t entered_length;
	size_t candidate_length;
	size_t row_length;

	if (session == NULL || port == NULL
	    || (cached == NULL && cached_length != 0U)
	    || cached_length > sizeof(candidate))
		return false;
	for (;;) {
		enum yt_yes_no_answer answer;

		if (!yt_port_name_display_row(cached, cached_length, row,
		    sizeof(row), &row_length)
		    || !session_present_paged_line(session, row, row_length,
		    "port name current row", error)
		    || !session_present_paged_line(session, keep,
		    sizeof(keep) - 1U, "port name keep row", error)
		    || !session_present_paged_line(session, instruction,
		    sizeof(instruction) - 1U,
		    "port name instruction row", error)
		    || !session_present_timed_paged_row(session, name_prompt,
		    sizeof(name_prompt) - 1U, "port name prompt", error)
		    || !session_read_command(session, (char *)entered,
		    sizeof(entered)))
			return false;
		entered_length = strlen((const char *)entered);
		if (!yt_port_name_prepare_candidate(entered, entered_length,
		    cached, cached_length, candidate, sizeof(candidate),
		    &candidate_length))
			return false;
		if (candidate_length == 0U)
			continue;
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE,
		    "port name confirmation leading blank", error)
		    || !yt_port_name_confirmation_prompt(candidate,
		    candidate_length, row, sizeof(row), &row_length)
		    || !session_confirm(session, row, row_length, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES)
			continue;
		if (!yt_port_name_overlay(port, candidate, candidate_length))
			return false;
		return yt_database_write(&session->door->game.database,
		    (size_t)session_port_basic_record(session,
		    (float)logical_port), &port->record, error);
	}
}

bool
yt_session_command_rename_port(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t not_owner[] = "This isn't your port!";
	static const uint8_t earth[] = "Can't rename Earth!";
	struct yt_sector sector;
	struct yt_port port;
	uint8_t cached_name[YT_TEXT_FIELD_SIZE];
	size_t cached_name_length;
	int logical_port;
	float relative_port;

	if (session == NULL || !session_reload_player(session, error)
	    || !session_read_sector(session, (int)session->player.sector,
	    &sector, error))
		return false;
	if (sector.port == 0.0f)
		return session_present_alert(session, no_port,
		    sizeof(no_port) - 1U, "rename no-port row", error);
	logical_port = (int)sector.port;
	relative_port = sector.port;
	if (!session_read_port_physical(session,
	    session_port_basic_record(session, (float)logical_port),
	    &port, error))
		return false;
	if (port.owner != (float)session_record(session))
		return session_present_alert(session, not_owner,
		    sizeof(not_owner) - 1U, "rename ownership row", error);
	if (relative_port == 1.0f)
		return session_present_alert(session, earth, sizeof(earth) - 1U,
		    "rename Earth row", error);
	cached_name_length = port.name_length;
	if (cached_name_length > sizeof(cached_name))
		cached_name_length = sizeof(cached_name);
	memcpy(cached_name, port.record.bytes, cached_name_length);
	return yt_session_edit_port_name(session, logical_port, cached_name,
	    cached_name_length, &port, error);
}

static bool
purchase_report(struct yt_session *session, int logical_port, bool earth,
    struct yt_port *early_port, struct yt_port *terminal_port,
    float production[3], struct yt_error *error)
{
	if (earth) {
		float earth_prices[4];

		if (!session_earth_report(session, early_port, earth_prices, error))
			return false;
		*terminal_port = *early_port;
		memset(production, 0, 3U * sizeof(production[0]));
		return true;
	}
	{
		struct yt_port_market_state market;
		struct yt_sector updater_sector = {0};

		updater_sector.port = (float)logical_port;
		if (!yt_session_update_port(session, 0, NULL, &updater_sector,
		    &market, error))
			return false;
		*early_port = market.port;
		memcpy(production, market.port.production,
		    3U * sizeof(production[0]));
		return yt_session_port_report(session, logical_port, &market,
		    terminal_port, error);
	}
}

static bool
purchase_present_sold(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t sold[] = "Sold!";

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "buy sold leading blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session, sold, sizeof(sold) - 1U);
}

static bool
purchase_accept(struct yt_session *session, int logical_port,
    float relative_port, float old_owner, double price,
    float cached_buyer_sector, const uint8_t *cached_trader,
    size_t cached_trader_length, const uint8_t *old_name,
    size_t old_name_length, const uint8_t *owner_name,
    size_t owner_name_length, struct yt_error *error)
{
	static const uint8_t transfer_prefix[] = "Credits transferred to ";
	static const uint8_t transfer_suffix[] = "'s account!";
	static const uint8_t radio_one[] = " bought your port \"";
	static const uint8_t radio_two[] = "\" in";
	static const uint8_t radio_three[] = " for";
	static const uint8_t radio_four[] = " credits";
	static const uint8_t success_prefix[] = "Congratulations ";
	static const uint8_t success_suffix[] =
	    "! When others trade at your port their CREDITS";
	static const uint8_t success_tail[] =
	    "will go into the port treasury for you to take out later!";
	const uint8_t *first_name =
	    (const uint8_t *)session->door->identity.real_first;
	size_t first_name_length = strlen((const char *)first_name);
	struct yt_port port;
	struct yt_player player;
	uint32_t physical_port = session_port_basic_record(session,
	    (float)logical_port);
	uint8_t row[512];
	uint8_t message[512];
	char sector_text[64];
	char price_text[64];
	size_t length;

	if (!purchase_present_sold(session, error)
	    || !session_read_port_physical(session, physical_port, &port, error))
		return false;
	if (old_owner != 0.0f) {
		int seller_record;

		length = 0U;
		if (!session_buffer_append(row, sizeof(row), &length, transfer_prefix,
		    sizeof(transfer_prefix) - 1U)
		    || !session_buffer_append(row, sizeof(row), &length, owner_name,
		    owner_name_length)
		    || !session_buffer_append(row, sizeof(row), &length, transfer_suffix,
		    sizeof(transfer_suffix) - 1U)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE,
		    "buy seller transfer leading blank", error)
		    || !session_present_paged_fragment(session, row, length))
			return false;
		seller_record = (int)old_owner;
		if (!yt_game_read_player(&session->door->game, seller_record,
		    &player, error)
		    || !yt_port_purchase_seller_overlay(&player, port.treasury,
		    price)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)seller_record, &player.record, error))
			return false;
		if (qb_str_single(sector_text, sizeof(sector_text),
		    cached_buyer_sector) < 0
		    || qb_str_double(price_text, sizeof(price_text), price) < 0)
			return treasury_error(error, "buy seller radio formatting");
		length = 0U;
		if (!session_buffer_append(message, sizeof(message), &length,
		    cached_trader, cached_trader_length)
		    || !session_buffer_append(message, sizeof(message), &length, radio_one,
		    sizeof(radio_one) - 1U)
		    || !session_buffer_append(message, sizeof(message), &length, old_name,
		    old_name_length)
		    || !session_buffer_append(message, sizeof(message), &length, radio_two,
		    sizeof(radio_two) - 1U)
		    || !session_buffer_append(message, sizeof(message), &length,
		    sector_text, strlen(sector_text))
		    || !session_buffer_append(message, sizeof(message), &length, radio_three,
		    sizeof(radio_three) - 1U)
		    || !session_buffer_append(message, sizeof(message), &length,
		    price_text, strlen(price_text))
		    || !session_buffer_append(message, sizeof(message), &length, radio_four,
		    sizeof(radio_four) - 1U)
		    || !session_append_radio_bytes(message, length, -2.0f,
		    old_owner, error)
		    || !session_read_port_physical(session, physical_port, &port, error))
			return false;
	}
	if (relative_port > 1.0f
	    && !yt_session_edit_port_name(session, logical_port, old_name,
	    old_name_length, &port, error))
		return false;
	if (!session_read_port_physical(session, physical_port, &port, error)
	    || !yt_port_purchase_title_overlay(&port, session_record(session))
	    || !yt_database_write(&session->door->game.database,
	    (size_t)physical_port, &port.record, error)
	    || !session_reload_player(session, error))
		return false;
	player = session->player;
	if (!yt_port_purchase_buyer_overlay(&player, price))
		return false;
	session->player = player;
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &player.record, error))
		return false;
	length = 0U;
	if (!session_buffer_append(row, sizeof(row), &length, success_prefix,
	    sizeof(success_prefix) - 1U)
	    || !session_buffer_append(row, sizeof(row), &length, first_name,
	    first_name_length)
	    || !session_buffer_append(row, sizeof(row), &length, success_suffix,
	    sizeof(success_suffix) - 1U)
	    || !session_present_paged_line(session, row, length,
	    "buy congratulations row", error))
		return false;
	return session_present_paged_fragment(session, success_tail,
	    sizeof(success_tail) - 1U);
}

bool
yt_session_command_buy_port(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t already_prefix[] = "You already OWN this port ";
	static const uint8_t unaffordable[] =
	    "Come back when you can afford it!";
	static const uint8_t offer_prefix[] = "You may buy it from ";
	static const uint8_t offer_suffix[] = " if you wish.";
	static const uint8_t prompt[] = "Do you wish to buy it? [y/N]";
	static const uint8_t declined[] = "What a shame.. it's a nice port!";
	static const uint8_t earth_name[] = "Earth";
	const uint8_t *first_name;
	size_t first_name_length;
	struct yt_player buyer;
	struct yt_sector sector;
	struct yt_port early_port;
	struct yt_port terminal_port;
	struct yt_port display_port;
	uint8_t cached_trader[YT_TEXT_FIELD_SIZE];
	size_t cached_trader_length;
	uint8_t old_name[YT_TEXT_FIELD_SIZE];
	size_t old_name_length;
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t owner_name_length = 0U;
	float production[3];
	float cached_buyer_credits;
	float cached_buyer_sector;
	float old_owner;
	float relative_port;
	double price;
	uint8_t row[512];
	char price_text[64];
	char credits_text[64];
	size_t length;
	int logical_port;
	bool earth;
	enum yt_yes_no_answer answer;

	if (session == NULL || !session_reload_player(session, error))
		return false;
	first_name = (const uint8_t *)session->door->identity.real_first;
	first_name_length = strlen((const char *)first_name);
	buyer = session->player;
	cached_buyer_credits = buyer.credits;
	cached_buyer_sector = buyer.sector;
	cached_trader_length = yt_player_stored_name(&buyer, cached_trader);
	if (!session_read_sector(session, (int)cached_buyer_sector,
	    &sector, error))
		return false;
	if (sector.port == 0.0f)
		return session_present_alert(session, no_port,
		    sizeof(no_port) - 1U, "buy no-port row", error);
	logical_port = (int)sector.port;
	relative_port = sector.port;
	earth = sector.port == 1.0f;
	if (!purchase_report(session, logical_port, earth, &early_port,
	    &terminal_port, production, error))
		return false;
	old_owner = early_port.owner;
	if (earth) {
		price = 1000000000.0;
		memcpy(old_name, earth_name, sizeof(earth_name) - 1U);
		old_name_length = sizeof(earth_name) - 1U;
	}
	else {
		old_name_length = terminal_port.name_length;
		if (old_name_length > sizeof(old_name))
			old_name_length = sizeof(old_name);
		memcpy(old_name, terminal_port.record.bytes, old_name_length);
		price = yt_port_purchase_price(production);
	}
	if (old_owner == (float)session_record(session)) {
		length = 0U;
		if (!session_buffer_append(row, sizeof(row), &length, already_prefix,
		    sizeof(already_prefix) - 1U)
		    || !session_buffer_append(row, sizeof(row), &length, first_name,
		    first_name_length)
		    || !session_buffer_append(row, sizeof(row), &length, "!", 1U))
			return treasury_error(error,
			    "buy already-owner row composition");
		return session_present_alert(session, row, length,
		    "buy already-owner row", error);
	}
	if (qb_str_double(price_text, sizeof(price_text), price) < 0
	    || qb_str_double(credits_text, sizeof(credits_text),
	    (double)cached_buyer_credits) < 0)
		return treasury_error(error, "buy price formatting");
	{
		int result = snprintf((char *)row, sizeof(row),
		    "This port is for sale for%s credits. You have%s credits.",
		    price_text, credits_text);

		if (result < 0 || (size_t)result >= sizeof(row))
			return treasury_error(error, "buy price composition");
		length = (size_t)result;
	}
	if (!session_present_paged_line(session, row, length,
	    "buy price row", error))
		return false;
	if ((double)cached_buyer_credits < price)
		return session_present_alert(session, unaffordable,
		    sizeof(unaffordable) - 1U, "buy unaffordable row", error);
	if (old_owner != 0.0f) {
		display_port = terminal_port;
		display_port.owner = old_owner;
		if (!session_port_owner_row_capture(session, &display_port,
		    owner_name, sizeof(owner_name), &owner_name_length, error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "buy owner offer leading blank", error))
			return false;
		length = 0U;
		if (!session_buffer_append(row, sizeof(row), &length, offer_prefix,
		    sizeof(offer_prefix) - 1U)
		    || !session_buffer_append(row, sizeof(row), &length, owner_name,
		    owner_name_length)
		    || !session_buffer_append(row, sizeof(row), &length, offer_suffix,
		    sizeof(offer_suffix) - 1U)
		    || !session_present_paged_fragment(session, row, length)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "buy owner offer trailing blank", error))
			return false;
	}
	if (!session_confirm(session, prompt, sizeof(prompt) - 1U, &answer,
	    error))
		return false;
	if (answer != YT_YES_NO_YES)
		return session_present_alert(session, declined,
		    sizeof(declined) - 1U, "buy declined row", error);
	return purchase_accept(session, logical_port, relative_port, old_owner,
	    price, cached_buyer_sector, cached_trader, cached_trader_length,
	    old_name, old_name_length, owner_name, owner_name_length, error);
}
