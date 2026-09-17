#include "yt_game.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

bool
yt_direct_attack_radio_text(const uint8_t *name, size_t name_length,
    double defender_loss, uint8_t *text, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " destroyed";
	static const uint8_t suffix[] = " of your fighters!";
	uint8_t raw_double[8];
	char aliased[64];
	int aliased_length;
	size_t total;

	if (length != NULL)
		*length = 0;
	if ((name == NULL && name_length != 0U) || text == NULL
	    || length == NULL)
		return false;
	if (qb_mbf64_encode(defender_loss, raw_double) != QB_MBF_OK)
		return false;
	aliased_length = qb_str_mbf32(aliased, sizeof(aliased), raw_double);
	if (aliased_length < 0)
		return false;
	total = name_length + sizeof(middle) - 1U + (size_t)aliased_length
	    + sizeof(suffix) - 1U;
	if (total > capacity)
		return false;
	if (name_length != 0U)
		memcpy(text, name, name_length);
	memcpy(text + name_length, middle, sizeof(middle) - 1U);
	memcpy(text + name_length + sizeof(middle) - 1U, aliased,
	    (size_t)aliased_length);
	memcpy(text + name_length + sizeof(middle) - 1U
	    + (size_t)aliased_length, suffix, sizeof(suffix) - 1U);
	*length = total;
	return true;
}

static bool
direct_attack_append(uint8_t *output, size_t capacity, size_t *position,
    const void *data, size_t length)
{
	if (output == NULL || position == NULL || (data == NULL && length != 0U)
	    || *position > capacity || length > capacity - *position)
		return false;
	if (length != 0U)
		memcpy(output + *position, data, length);
	*position += length;
	return true;
}

bool
yt_direct_attack_team_row(const uint8_t *name, size_t name_length,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "NOT attacking team member ";
	static const uint8_t suffix[] = "!";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_candidate_prompt(const uint8_t *name,
    size_t name_length, uint8_t *prompt, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Attack ";
	static const uint8_t suffix[] = " (Y/N)[Y]? ";
	size_t position = 0U;

	if (length == NULL || (name == NULL && name_length != 0U))
		return false;
	*length = 0U;
	if (!direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, name,
	    name_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_commitment_prompt(double fighters, uint8_t *prompt,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You have";
	static const uint8_t suffix[] = ". Use how many fighters? [0] ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(prompt, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(prompt, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(prompt, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_too_many_row(double fighters, uint8_t *row,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "You only have";
	static const uint8_t suffix[] = "!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	number_length = qb_str_double(number, sizeof(number), fighters);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, number,
	    (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_direct_attack_result_rows(double attacker_loss,
    double cached_reserve, double defender_loss, double defenders,
    uint8_t *attacker_row, size_t attacker_capacity,
    size_t *attacker_length, uint8_t *defender_row,
    size_t defender_capacity, size_t *defender_length)
{
	static const uint8_t attacker_prefix[] = "You lost";
	static const uint8_t attacker_middle[] = " fighter(s),";
	static const uint8_t remain[] = " remain.";
	static const uint8_t defender_prefix[] = "You destroyed";
	static const uint8_t defender_middle[] = " enemy fighters,";
	char attacker_loss_text[64];
	char reserve_text[64];
	char defender_loss_text[64];
	char defenders_text[64];
	int attacker_loss_length;
	int reserve_length;
	int defender_loss_length;
	int defenders_length;
	size_t attacker_position = 0U;
	size_t defender_position = 0U;

	if (attacker_length == NULL || defender_length == NULL)
		return false;
	*attacker_length = 0U;
	*defender_length = 0U;
	attacker_loss_length = qb_str_double(attacker_loss_text,
	    sizeof(attacker_loss_text), attacker_loss);
	reserve_length = qb_str_double(reserve_text, sizeof(reserve_text),
	    cached_reserve);
	defender_loss_length = qb_str_double(defender_loss_text,
	    sizeof(defender_loss_text), defender_loss);
	defenders_length = qb_str_double(defenders_text,
	    sizeof(defenders_text), defenders);
	if (attacker_loss_length < 0 || reserve_length < 0
	    || defender_loss_length < 0 || defenders_length < 0
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_prefix, sizeof(attacker_prefix) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_loss_text,
	    (size_t)attacker_loss_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, attacker_middle, sizeof(attacker_middle) - 1U)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, reserve_text, (size_t)reserve_length)
	    || !direct_attack_append(attacker_row, attacker_capacity,
	    &attacker_position, remain, sizeof(remain) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_prefix, sizeof(defender_prefix) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_loss_text,
	    (size_t)defender_loss_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defender_middle, sizeof(defender_middle) - 1U)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, defenders_text, (size_t)defenders_length)
	    || !direct_attack_append(defender_row, defender_capacity,
	    &defender_position, remain, sizeof(remain) - 1U))
		return false;
	*attacker_length = attacker_position;
	*defender_length = defender_position;
	return true;
}

void
yt_direct_attack_fighter_overlay(struct yt_player *player, float fighters)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_direct_attack_shield_overlay(struct yt_player *player, float shields)
{
	if (player == NULL)
		return;
	player->shields = shields;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
}

void
yt_deployed_attack_player_overlay(struct yt_player *player,
    float shields, float fighters)
{
	if (player == NULL)
		return;
	player->shields = shields;
	player->fighters = fighters;
	(void)yt_record_set_number(&player->record, YT_F53, shields);
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
}

void
yt_deployed_attack_sector_overlay(struct yt_sector *sector, float fighters)
{
	if (sector == NULL)
		return;
	sector->fighters = fighters;
	(void)yt_record_set_number(&sector->record, YT_F81, fighters);
	if (fighters < 1.0f) {
		sector->fighter_owner = 0;
		(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	}
}

void
yt_death_player_overlay(struct yt_player *player, int killer)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x7a, 0x00};

	if (player == NULL)
		return;
	player->killed_by = killer;
	player->sector = 0;
	player->ports_owned = 0.0f;
	(void)yt_record_set_number(&player->record, YT_F45, (float)killer);
	(void)yt_record_set_raw_number(&player->record, YT_F57, dirty_zero);
	(void)yt_record_set_raw_number(&player->record, YT_F117, dirty_zero);
}

bool
yt_death_sector_overlay(struct yt_sector *sector, int victim)
{
	if (sector == NULL || sector->fighter_owner != victim)
		return false;
	sector->fighter_owner = -2;
	(void)yt_record_set_number(&sector->record, YT_F85, -2.0f);
	return true;
}

enum yt_death_port_route
yt_death_port_overlay(struct yt_port *port, int victim, int killer,
    int last_player)
{
	bool valid;

	if (port == NULL || port->owner != victim)
		return YT_DEATH_PORT_UNMATCHED;
	valid = (killer != victim) & (killer > 1) & (killer <= last_player);
	if (valid) {
		port->owner = killer;
		port->last_minute = (float)killer;
		(void)yt_record_set_number(&port->record, YT_F97, (float)killer);
		(void)yt_record_set_number(&port->record, YT_F101, (float)killer);
		return YT_DEATH_PORT_TRANSFERRED;
	}
	port->owner = 0;
	port->treasury = 0.0f;
	(void)yt_record_set_number(&port->record, YT_F97, 0.0f);
	(void)yt_record_set_number(&port->record, YT_F89, 0.0f);
	return YT_DEATH_PORT_CLEARED;
}

void
yt_death_killer_credit_overlay(struct yt_player *player, float ports)
{
	volatile float updated;

	if (player == NULL)
		return;
	updated = player->ports_owned + ports;
	player->ports_owned = updated;
	(void)yt_record_set_number(&player->record, YT_F117, updated);
}

bool
yt_death_title_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "The titles to";
	static const uint8_t middle[] = " ports of ";
	static const uint8_t suffix[] = "'s are now yours!";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length)
	    || !direct_attack_append(row, capacity, &position, suffix,
	    sizeof(suffix) - 1U))
		return false;
	*length = position;
	return true;
}

bool
yt_death_kill_news_row(const uint8_t *killer, size_t killer_length,
    const uint8_t *victim, size_t victim_length, bool self,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  ";
	static const uint8_t self_suffix[] = " was killed!";
	static const uint8_t other_infix[] = " killed ";
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (killer == NULL && killer_length != 0U)
	    || (victim == NULL && victim_length != 0U))
		return false;
	if (!direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position, killer,
	    killer_length)
	    || !direct_attack_append(row, capacity, &position,
	    self ? self_suffix : other_infix,
	    self ? sizeof(self_suffix) - 1U : sizeof(other_infix) - 1U)
	    || (!self && !direct_attack_append(row, capacity, &position,
	    victim, victim_length)))
		return false;
	*length = position;
	return true;
}

bool
yt_death_port_news_row(const uint8_t *victim, size_t victim_length,
    float ports, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "  -  Took";
	static const uint8_t middle[] = " ports from ";
	char number[64];
	int number_length;
	size_t position = 0U;

	if (length == NULL)
		return false;
	*length = 0U;
	if (row == NULL || (victim == NULL && victim_length != 0U))
		return false;
	number_length = qb_str_single(number, sizeof(number), ports);
	if (number_length < 0
	    || !direct_attack_append(row, capacity, &position, prefix,
	    sizeof(prefix) - 1U)
	    || !direct_attack_append(row, capacity, &position,
	    (const uint8_t *)number, (size_t)number_length)
	    || !direct_attack_append(row, capacity, &position, middle,
	    sizeof(middle) - 1U)
	    || !direct_attack_append(row, capacity, &position, victim,
	    victim_length))
		return false;
	*length = position;
	return true;
}

void
yt_bribe_sector_overlay(struct yt_sector *sector)
{
	if (sector == NULL)
		return;
	sector->fighter_owner = 0;
	(void)yt_record_set_number(&sector->record, YT_F85, 0.0f);
	sector->fighters = 0.0f;
	(void)yt_record_set_number(&sector->record, YT_F81, 0.0f);
}

void
yt_bribe_player_overlay(struct yt_player *player, float fighters,
    float credits)
{
	if (player == NULL)
		return;
	player->fighters = fighters;
	player->credits = credits;
	(void)yt_record_set_number(&player->record, YT_F61, fighters);
	(void)yt_record_set_number(&player->record, YT_F81, credits);
}

bool
yt_player_killer_row(const struct yt_player *player, uint8_t *row,
    size_t capacity, size_t *length, bool *emit, struct yt_error *error)
{
	static const uint8_t suffix[] = " destroyed your ship!";
	size_t prefix;

	if (length != NULL)
		*length = 0;
	if (emit != NULL)
		*emit = false;
	if (player == NULL || row == NULL || length == NULL || emit == NULL)
		return false;
	if (player->name_length == 0U)
		return true;
	prefix = yt_player_stored_name(player, row);
	if (prefix + sizeof(suffix) - 1U > capacity) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "killer row capacity");
		}
		return false;
	}
	memcpy(row + prefix, suffix, sizeof(suffix) - 1U);
	*length = prefix + sizeof(suffix) - 1U;
	*emit = true;
	return true;
}

bool
yt_player_name_matches(const struct yt_player *player, const uint8_t *name,
    size_t length)
{
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	size_t stored = yt_player_stored_name(player, stored_name);

	return length == stored
	    && (stored == 0U || memcmp(stored_name, name, stored) == 0);
}
