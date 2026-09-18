#include "yt_game.h"
#include "yt_game_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

float
yt_planet_move_destination(const struct qb_val_result *parsed)
{
	return (float)qb_val_int_or_zero(parsed);
}

uint16_t
yt_planet_move_add_cost(uint16_t cost)
{
	return (uint16_t)(cost + 10U);
}

void
yt_planet_move_sector_overlay(struct yt_sector *sector, int planet_link)
{
	if (sector == NULL)
		return;
	sector->planet = planet_link;
	(void)yt_record_set_number(&sector->record, YT_F93,
	    (float)planet_link);
}

void
yt_planet_move_explosion_overlay(struct yt_planet *planet)
{
	static const uint8_t zero_raw[4] = {0, 0, 0, 0};

	if (planet == NULL)
		return;
	planet->name[0] = '\0';
	planet->name_length = 0U;
	memcpy(planet->record.bytes, zero_raw, sizeof(zero_raw));
	memset(planet->record.bytes + sizeof(zero_raw), ' ',
	    YT_TEXT_FIELD_SIZE - sizeof(zero_raw));
	(void)yt_record_set_raw_number(&planet->record, YT_F85, zero_raw);
}

void
yt_planet_move_fighter_overlay(struct yt_player *player, float loss)
{
	if (player == NULL)
		return;
	player->fighters -= loss;
	(void)yt_record_set_number(&player->record, YT_F61,
	    player->fighters);
}

void
yt_planet_move_success_overlay(struct yt_player *player,
    int destination)
{
	if (player == NULL)
		return;
	player->turns -= 10.0f;
	player->sector = destination;
	(void)yt_record_set_number(&player->record, YT_F49, player->turns);
	(void)yt_record_set_number(&player->record, YT_F57,
	    (float)destination);
}

bool
yt_planet_move_path_heading(uint16_t start, uint16_t destination,
    uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t first[] = "The shortest path from sector";
	static const uint8_t middle[] = " to sector";
	static const uint8_t suffix[] = " is:";
	char start_text[64];
	char destination_text[64];
	int start_length = qb_str_single(start_text, sizeof(start_text),
	    (float)start);
	int destination_length = qb_str_single(destination_text,
	    sizeof(destination_text), (float)destination);

	if (start_length < 0 || destination_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)start_text, (size_t)start_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)destination_text, (size_t)destination_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_summary(uint16_t cost, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "Distance is";
	static const uint8_t middle[] = " and will take";
	static const uint8_t suffix[] = " turns.";
	uint16_t distance = cost / 10U;
	char distance_text[64];
	char cost_text[64];
	int distance_length = qb_str_single(distance_text,
	    sizeof(distance_text), (float)distance);
	int cost_length = qb_str_single(cost_text, sizeof(cost_text), (float)cost);

	if (distance_length < 0 || cost_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)distance_text, (size_t)distance_length,
	    middle, sizeof(middle) - 1U,
	    (const uint8_t *)cost_text, (size_t)cost_length,
	    suffix, sizeof(suffix) - 1U, row, capacity, length);
}

bool
yt_planet_move_turns_row(float turns, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "You have";
	static const uint8_t suffix[] = " turns left.";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), turns);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(first, sizeof(first) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_explosion_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = "The stress was too much! PLANET ";
	static const uint8_t suffix[] = " EXPLODED!";

	return yt_game_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, suffix, sizeof(suffix) - 1U,
	    NULL, 0U, NULL, 0U, row, capacity, length);
}

bool
yt_planet_move_explosion_news(const uint8_t *planet_name,
    size_t planet_name_length, const uint8_t *player_name,
    size_t player_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t first[] = " *** Planet ";
	static const uint8_t middle[] = " EXPLODED while being moved by ";
	static const uint8_t suffix[] = "!!!";

	return yt_game_join_parts(first, sizeof(first) - 1U,
	    planet_name, planet_name_length, middle, sizeof(middle) - 1U,
	    player_name, player_name_length, suffix, sizeof(suffix) - 1U,
	    row, capacity, length);
}

bool
yt_planet_move_loss_row(const uint8_t *actor, size_t actor_length,
    float loss, uint8_t *row, size_t capacity, size_t *length)
{
	static const uint8_t middle[] = " lost";
	static const uint8_t suffix[] = " fighters in the explosion!";
	char number[64];
	int number_length = qb_str_single(number, sizeof(number), loss);

	if (number_length < 0)
		return false;
	return yt_game_join_parts(actor, actor_length, middle, sizeof(middle) - 1U,
	    (const uint8_t *)number, (size_t)number_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U,
	    row, capacity, length);
}

bool
yt_planet_move_success_row(const uint8_t *planet_name,
    size_t planet_name_length, uint8_t *row, size_t capacity,
    size_t *length)
{
	static const uint8_t suffix[] =
	    " moved! (Xannoron Movers, we move anyTHING, anyWHERE!)";

	return yt_game_join_parts(planet_name, planet_name_length,
	    suffix, sizeof(suffix) - 1U, NULL, 0U, NULL, 0U, NULL, 0U,
	    row, capacity, length);
}
