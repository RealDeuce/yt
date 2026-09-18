#include "yt_game.h"

#include "qb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool
yt_projectile_target_prompt(bool plasma, float displayed, float maximum,
    uint8_t *prompt, size_t capacity, size_t *length)
{
	const char *label = plasma ? " plasma bolt " : " cruise missile ";
	char displayed_text[64];
	char maximum_text[64];
	int written;

	if (prompt == NULL || length == NULL || capacity == 0U)
		return false;
	if (qb_str_single(displayed_text, sizeof(displayed_text), displayed)
	    < 0)
		return false;
	if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0)
		return false;
	written = snprintf((char *)prompt, capacity,
	    "You have%s. Send your%sto what sector? [ 1 to%s ] ?",
	    displayed_text, label, maximum_text);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

void
yt_projectile_plasma_opening_values(float bolts, double *energy,
    float *hop_loss)
{
	volatile double quotient;
	volatile float rounded;

	*energy = (double)qb_single_multiply(2500000.0f, bolts);
	quotient = *energy / 50.0;
	rounded = (float)quotient;
	*hop_loss = rounded;
}

bool
yt_projectile_plasma_energy_row(double energy, uint8_t *row,
    size_t capacity, size_t *length)
{
	char number[64];
	int written;

	if (row == NULL || length == NULL)
		return false;
	if (qb_str_double(number, sizeof(number), energy) < 0)
		return false;
	written = snprintf((char *)row, capacity,
	    "Plasma bolts targeted... firing%s megawatts!", number);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

bool
yt_projectile_plasma_firing_row(float counter, uint8_t *row,
    size_t capacity, size_t *length)
{
	char number[64];
	int written;

	if (row == NULL || length == NULL)
		return false;
	if (qb_str_single(number, sizeof(number), counter) < 0)
		return false;
	written = snprintf((char *)row, capacity, "Firing%s!", number);
	if (written < 0 || (size_t)written >= capacity)
		return false;
	*length = (size_t)written;
	return true;
}

float
yt_projectile_plasma_next_firing(float counter)
{
	return qb_single_add(counter, 1.0f);
}

void
yt_projectile_debit_overlay(struct yt_player *player, bool plasma,
    float amount)
{
	volatile float remaining;
	size_t offset;

	if (player == NULL)
		return;
	if (plasma) {
		remaining = player->plasma - amount;
		player->plasma = remaining;
		offset = YT_F113;
	}
	else {
		remaining = player->missiles - amount;
		player->missiles = remaining;
		offset = YT_F97;
	}
	(void)yt_record_set_number(&player->record, offset, remaining);
}

float
yt_counterlaunch_score_count(double cached_score, float retained)
{
	static const uint8_t score_factor_raw[8] = {
		0x84, 0x47, 0x1b, 0x47, 0xac, 0xc5, 0x27, 0x70
	};
	volatile double product;
	volatile double integral;
	volatile double result;

	if (cached_score <= 0.0)
		return retained;
	product = cached_score * qb_mbf64_decode(score_factor_raw);
	integral = floor(product);
	result = integral + 1.0;
	return (float)result;
}

void
yt_counterlaunch_debit_overlay(struct yt_player *fresh_target,
    float first_available, float selected_count)
{
	volatile float remaining;

	if (fresh_target == NULL)
		return;
	remaining = first_available - selected_count;
	fresh_target->missiles = remaining;
	(void)yt_record_set_number(&fresh_target->record, YT_F97, remaining);
}

bool
yt_counterlaunch_rows(const uint8_t *target_name,
    size_t target_name_length, float selected_count,
    const uint8_t *saved_name, size_t saved_name_length,
    uint8_t *terminal, size_t terminal_capacity, size_t *terminal_length,
    uint8_t *news, size_t news_capacity, size_t *news_length)
{
	static const uint8_t middle[] = " shot back with";
	static const uint8_t terminal_suffix[] = " missiles at you!";
	static const uint8_t news_middle[] = " missiles at ";
	char number[64];
	int number_length;
	size_t terminal_needed;
	size_t news_needed;
	size_t position;

	if (terminal_length == NULL || news_length == NULL
	    || (target_name == NULL && target_name_length != 0U)
	    || (saved_name == NULL && saved_name_length != 0U))
		return false;
	*terminal_length = 0U;
	*news_length = 0U;
	number_length = qb_str_single(number, sizeof(number), selected_count);
	if (number_length < 0)
		return false;
	terminal_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(terminal_suffix) - 1U;
	news_needed = target_name_length + sizeof(middle) - 1U
	    + (size_t)number_length + sizeof(news_middle) - 1U
	    + saved_name_length + 1U;
	if (terminal_needed > terminal_capacity || news_needed > news_capacity
	    || (terminal_needed != 0U && terminal == NULL)
	    || (news_needed != 0U && news == NULL))
		return false;
	position = 0U;
	if (target_name_length != 0U)
		memcpy(terminal + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(terminal + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(terminal + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(terminal + position, terminal_suffix,
	    sizeof(terminal_suffix) - 1U);
	*terminal_length = terminal_needed;

	position = 0U;
	if (target_name_length != 0U)
		memcpy(news + position, target_name, target_name_length);
	position += target_name_length;
	memcpy(news + position, middle, sizeof(middle) - 1U);
	position += sizeof(middle) - 1U;
	memcpy(news + position, number, (size_t)number_length);
	position += (size_t)number_length;
	memcpy(news + position, news_middle, sizeof(news_middle) - 1U);
	position += sizeof(news_middle) - 1U;
	if (saved_name_length != 0U)
		memcpy(news + position, saved_name, saved_name_length);
	position += saved_name_length;
	news[position] = '!';
	*news_length = news_needed;
	return true;
}
