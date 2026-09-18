#ifndef YT_PATCH_H
#define YT_PATCH_H

#include "yt_common.h"

enum yt_patch_level {
	YT_PATCH_36,
	YT_PATCH_36A,
	YT_PATCH_36C,
	YT_PATCH_36D,
	YT_PATCH_36E,
	YT_PATCH_36F,
	YT_PATCH_36G,
	YT_PATCH_LEVEL_COUNT
};

struct yt_patch_profile {
	enum yt_patch_level level;
	const char *name;
	const char *open_doors_version;
	const char *registration_author;
	const char *registration_description;
	const char *registration_contact;
	const char *registration_banner;
	const uint8_t *ground_forces_label;
	size_t ground_forces_label_length;
	const uint8_t *anti_cloak_activation;
	size_t anti_cloak_activation_length;
	const uint8_t *anti_cloak_waves;
	size_t anti_cloak_waves_length;
	double xannor_turn_divisor;
	const uint8_t *xannor_turn_divisor_raw;
	double earth_purchase_price;
	float cloak_energy_cost;
	uint32_t danger_scanner_cost;
	float cargo_hold_coefficient;
	float fighter_coefficient;
	float anti_cloak_cost;
	float ground_force_coefficient;
	uint32_t spy_cost;
	float plasma_score_weight;
	float xannor_headquarters_award;
	uint16_t initializer_sector_count;
	char missile_key;
	char computer_credit_key;
};

const struct yt_patch_profile *yt_patch_default(void);
const struct yt_patch_profile *yt_patch_get(enum yt_patch_level level);
const struct yt_patch_profile *yt_patch_find(const char *name);
double yt_patch_xannor_turn_divisor(const struct yt_patch_profile *patch);
bool yt_patch_sector_count_matches(const struct yt_patch_profile *patch,
    uint16_t sector_count);

#endif
