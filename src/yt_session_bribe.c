#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>
static bool
bribe_name_row(const uint8_t *prefix, size_t prefix_length,
    const uint8_t *name, size_t name_length, const uint8_t *suffix,
    size_t suffix_length, uint8_t *row, size_t capacity, size_t *length)
{
	size_t used = 0U;

	if (!session_buffer_append(row, capacity, &used, prefix, prefix_length)
	    || !session_buffer_append(row, capacity, &used, name, name_length)
	    || !session_buffer_append(row, capacity, &used, suffix, suffix_length))
		return false;
	*length = used;
	return true;
}

static bool
bribe_accept(struct yt_session *session, double cached_defenders,
    float offer, struct yt_error *error)
{
	static const uint8_t deal[] = "Good Deal! We join up with you!";
	struct yt_sector sector;
	struct yt_player current;
	volatile double fighters;
	volatile double credits;
	int current_sector = (int)session->player.sector;
	int player_record = session_record(session);

	if (!session_present_alert(session, deal, sizeof(deal) - 1U,
	    "accepted Mercenary Bribe", error)
	    || !session_sound(session, YT_SOUND_CUE_REWARD, "accepted bribe sound", error)
	    || !session_read_sector(session, current_sector, &sector, error))
		return false;
	yt_bribe_sector_overlay(&sector);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    current_sector), &sector.record, error)
	    || !session_reload_player(session, error))
		return false;
	current = session->player;
	fighters = qb_double_add((double)current.fighters,
	    cached_defenders);
	credits = qb_double_subtract((double)current.credits, (double)offer);
	yt_bribe_player_overlay(&current, (float)fighters, (float)credits);
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &current.record, error);
}

static bool
bribe_force_attack(struct yt_session *session, struct yt_sector *sector,
    bool mercenary_fatal_gate, bool *direct_hostile_menu,
    bool *forced_attack, struct yt_error *error)
{
	enum qb_mbf_status conversion;
	float commitment = (float)session->combat.ship_fighters;
	uint8_t raw[4];

	*forced_attack = true;
	conversion = qb_mbf32_encode(commitment, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:commitment-csng");
		}
		return false;
	}
	commitment = qb_mbf32_decode(raw);
	switch (yt_bribe_forced_admit(session->combat.ship_fighters,
	    session->combat.ship_shields, mercenary_fatal_gate, commitment)) {
	case YT_BRIBE_FORCED_FATAL:
		return yt_session_common_fatal_self(session, error);
	case YT_BRIBE_FORCED_LESS_THAN_ONE:
		*direct_hostile_menu = true;
		return true;
	case YT_BRIBE_FORCED_ATTACK:
		return yt_session_attack_deployed(session, sector,
		    (double)commitment, true, error);
	}
	return false;
}

bool
yt_session_bribe_deployed(struct yt_session *session,
    struct yt_sector *sector, bool *direct_hostile_menu,
    bool *forced_attack, struct yt_error *error)
{
	static const uint8_t ordinary_prefix[] =
	    "We don't accept no Bribes ";
	static const uint8_t planet_prefix[] = "Scram ";
	static const uint8_t planet_suffix[] = ", This planet is OURS!";
	static const uint8_t life_prefix[] =
	    "We just want your miserable life ";
	static const uint8_t introduction_prefix[] =
	    "We MAY join up if you pay us enough ";
	static const uint8_t rejected_prefix[] = "You insult us ";
	static const uint8_t rejected_suffix[] = "! Prepare to DIE!";
	static const uint8_t bang[] = "!";
	static const uint8_t prompt_prefix[] = "You have";
	static const uint8_t prompt_suffix[] =
	    " credits. How much do you offer? -+>";
	uint8_t row[512];
	uint8_t prompt[512];
	char credits[64];
	char response[4096] = {0};
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	uint8_t raw[4];
	size_t row_length;
	size_t prompt_length = 0U;
	double cached_defenders;
	double ship_fighters;
	double available_credits;
	double threshold;
	float draw;
	float second_draw;
	float offer;
	const uint8_t *name;
	size_t name_length;
	int credits_length;
	bool force_attack;

	if (session == NULL || sector == NULL || direct_hostile_menu == NULL
	    || forced_attack == NULL)
		return false;
	*direct_hostile_menu = false;
	*forced_attack = false;
	cached_defenders = session->combat.deployed_fighters;
	ship_fighters = session->combat.ship_fighters;
	available_credits = (double)session->player.credits;
	name = (const uint8_t *)session->door->identity.real_first;
	name_length = strlen(session->door->identity.real_first);

	if (session->combat.hostile_owner != -2.0f) {
		if (!bribe_name_row(ordinary_prefix,
		    sizeof(ordinary_prefix) - 1U, name, name_length, bang,
		    sizeof(bang) - 1U, row, sizeof(row), &row_length)
		    || !session_present_alert(session, row, row_length,
		    "ordinary Bribe refusal", error)
		    || !yt_random_next(&session->door->game.random, &draw, error))
			return false;
		force_attack = yt_bribe_ordinary_forces(session->combat.hostile_owner,
		    cached_defenders, ship_fighters, draw);
		if (!force_attack)
			return true;
		return bribe_force_attack(session, sector, false,
		    direct_hostile_menu, forced_attack, error);
	}

	if (sector->planet != 0.0f) {
		return bribe_name_row(planet_prefix,
		    sizeof(planet_prefix) - 1U, name, name_length, planet_suffix,
		    sizeof(planet_suffix) - 1U, row, sizeof(row), &row_length)
		    && session_present_alert(session, row, row_length,
		    "Mercenary planet refusal", error);
	}

	if (!yt_random_next(&session->door->game.random, &draw, error)
	    || !yt_random_next(&session->door->game.random, &second_draw,
	    error))
		return false;
	force_attack = yt_bribe_mercenary_forces(cached_defenders,
	    ship_fighters, draw, second_draw, session->combat.mercenaries_hurt);
	if (force_attack) {
		if (!bribe_name_row(life_prefix, sizeof(life_prefix) - 1U,
		    name, name_length, bang, sizeof(bang) - 1U, row,
		    sizeof(row), &row_length)
		    || !session_present_alert(session, row, row_length,
		    "Mercenary life demand", error))
			return false;
		return bribe_force_attack(session, sector, true,
		    direct_hostile_menu, forced_attack, error);
	}

	if (!bribe_name_row(introduction_prefix,
	    sizeof(introduction_prefix) - 1U, name, name_length, bang,
	    sizeof(bang) - 1U, row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "Mercenary Bribe introduction", error))
		return false;
	credits_length = qb_str_double(credits, sizeof(credits),
	    available_credits);
	if (credits_length < 0
	    || !session_buffer_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_prefix, sizeof(prompt_prefix) - 1U)
	    || !session_buffer_append(prompt, sizeof(prompt), &prompt_length,
	    (const uint8_t *)credits, (size_t)credits_length)
	    || !session_buffer_append(prompt, sizeof(prompt), &prompt_length,
	    prompt_suffix, sizeof(prompt_suffix) - 1U)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "Mercenary Bribe offer prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:VAL");
		}
		return false;
	}
	offer = (float)parsed.value;
	conversion = qb_mbf32_encode(offer, raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:offer-csng");
		}
		return false;
	}
	offer = qb_mbf32_decode(raw);
	if (!yt_random_next(&session->door->game.random, &draw, error))
		return false;
	threshold = yt_bribe_offer_threshold(cached_defenders, draw);
	if ((double)offer <= available_credits
	    && yt_bribe_offer_accepted(offer, available_credits, threshold))
		return bribe_accept(session, cached_defenders, offer, error);

	if (!bribe_name_row(rejected_prefix, sizeof(rejected_prefix) - 1U,
	    name, name_length, rejected_suffix, sizeof(rejected_suffix) - 1U,
	    row, sizeof(row), &row_length)
	    || !session_present_alert(session, row, row_length,
	    "Mercenary rejected offer", error))
		return false;
	return bribe_force_attack(session, sector, true, direct_hostile_menu,
	    forced_attack, error);
}
