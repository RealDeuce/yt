#include "yt_game.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_clearance_candidate_needed(size_t item, float trigger_draw,
    float discount, bool create)
{
	static const float trigger[4] = {
		0.7900000214576721f, 0.7900000214576721f,
		0.8399999737739563f, 0.8899999856948853f
	};

	return item < 4U && trigger_draw > trigger[item]
	    && discount == 0.0f && create;
}

bool
yt_clearance_normalize(size_t item, float *discount)
{
	static const float maximum[4] = {
		0.9509999752044678f, 0.9800000190734863f,
		0.800000011920929f, 0.8999999761581421f
	};

	if (item >= 4U || discount == NULL)
		return false;
	if (*discount < 0.10000000149011612f
	    || *discount > maximum[item]) {
		*discount = 0.0f;
		return false;
	}
	return true;
}

uint8_t
yt_clearance_percentage(float discount)
{
	return (uint8_t)floorf((100.0f * discount));
}

void
yt_earth_prices(const float discount[4], uint8_t price[4])
{
	if (discount == NULL || price == NULL)
		return;
	price[0] = (uint8_t)floorf((250.0f -
	    (250.0f * discount[0])));
	price[1] = (uint8_t)floorf((50.0f -
	    (50.0f * discount[1])));
	price[2] = (uint8_t)floorf((50.0f *
	    (1.0f - discount[2])));
	price[3] = (uint8_t)floorf((200.5f *
	    (1.0f - discount[3])));
}

double
yt_earth_affordable(float credits, uint32_t price)
{
	return floor(((double)credits / (double)price));
}

int
yt_earth_selector_position(const char *command)
{
	const char *position;

	if (command == NULL)
		return 0;
	position = strstr("LM0C", command);
	return position == NULL ? 0 : (int)(position - "LM0C") + 1;
}

float
yt_earth_purchase_quantity(double value)
{
	return (float)floor(value);
}

float
yt_earth_receipt_amount(int owner, int buyer_record, float cost)
{
	if (owner == 0)
		return 0.0f;
	if (owner == buyer_record)
		return floorf((0.009999999776482582f * cost));
	return cost;
}

uint8_t
yt_earth_cloak_points(float cloak)
{
	return (uint8_t)floorf((50.0f * cloak));
}

uint8_t
yt_earth_cloak_default(uint8_t deficit, float credits)
{
	if (((float)deficit * 1000.0f) > credits)
		return (uint8_t)yt_earth_affordable(credits, 1000U);
	return deficit;
}

float
yt_earth_cloak_overlay(uint8_t points, uint8_t quantity)
{
	return ((float)(points + quantity) / 50.0f);
}

void
yt_earth_supply_overlay(struct yt_player *player, int choice, float quantity)
{
	if (player == NULL)
		return;
	if (choice == 3)
		player->fighters = (player->fighters + quantity);
	else if (choice == 7)
		player->ground_forces = floorf((
		    player->ground_forces + quantity));
	else if (choice == 8)
		player->shields = floorf((
		    player->shields + quantity));
}

int
yt_lottery_match_count(const int winning[6], const char ticket[6],
    bool matched_winning[6])
{
	bool used_winning[6] = {0};
	bool used_ticket[6] = {0};
	int matches = 0;
	int index;

	if (winning == NULL || ticket == NULL || matched_winning == NULL)
		return 0;
	memset(matched_winning, 0, 6U * sizeof(*matched_winning));
	for (index = 0; index < 6; ++index) {
		int candidate;

		for (candidate = 0; candidate < 6; ++candidate) {
			if (!used_winning[index] && !used_ticket[candidate]
			    && winning[index] == ticket[candidate] - '0') {
				used_winning[index] = true;
				used_ticket[candidate] = true;
				matched_winning[index] = true;
				++matches;
				break;
			}
		}
	}
	return matches;
}

uint32_t
yt_lottery_award(int matches)
{
	static const uint32_t awards[6] = {
		100U, 1000U, 10000U, 100000U,
		1000000U, 100000000U
	};

	return matches < 1 || matches > 6 ? 0U : awards[matches - 1];
}
