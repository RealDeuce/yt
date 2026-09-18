#include "yt_patch.h"

#include "qb.h"

static const uint8_t negative_xannor_divisor[8] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff
};

static const uint8_t ground_forces[] = "[7] Ground Forces";
static const uint8_t malformed_ground_forces[] = "[7] Ground Forces\xd4";
static const uint8_t anti_cloak_activation[] =
    "Anti-Cloaking device activated!";
static const uint8_t malformed_anti_cloak_activation[] =
    "ti-Cloaking device activated!\xd4" "D";
static const uint8_t anti_cloak_waves[] =
    "Waves of electromagnetic disruption flood the galaxy...";
static const uint8_t malformed_anti_cloak_waves[] =
    "Waves of electromagnetic disruption flood the galaxy..."
    "\xd4\x0e\x00\x86\xc1" " is uncl";

#define YT_PATCH_TEXT(field, value) \
	.field = value, .field ## _length = sizeof(value) - 1U

static const struct yt_patch_profile profiles[YT_PATCH_LEVEL_COUNT] = {
	[YT_PATCH_36] = {
		.level = YT_PATCH_36,
		.name = "3.6",
		.open_doors_version = "3.6",
		.registration_author = "By Alan Davenport",
		.registration_description =
		    "Copyright (c) 1989,1990,1991,1992,1993,1994 Alan Davenport",
		.registration_contact =
		    "BBS Number 1-717-686-3037 -=- Fidonet 1:13/75",
		.registration_banner = "Version 3.6 * YT * Compiled 03/16/94",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation, anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, anti_cloak_waves),
		.xannor_turn_divisor = 200.0,
		.earth_purchase_price = 1000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 250000U,
		.cargo_hold_coefficient = 500.0f,
		.fighter_coefficient = 100.0f,
		.anti_cloak_cost = 500000.0f,
		.ground_force_coefficient = 750.0f,
		.spy_cost = 333000U,
		.plasma_score_weight = 1000000.0f,
		.xannor_headquarters_award = 1000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = '!',
		.computer_credit_key = '$',
	},
	[YT_PATCH_36A] = {
		.level = YT_PATCH_36A,
		.name = "3.6A",
		.open_doors_version = "3.6A",
		.registration_author = "By Alan Davenport",
		.registration_description =
		    "Copyright (c) 1989,1990,1991,1992,1993,1994 Alan Davenport",
		.registration_contact =
		    "BBS Number 1-717-686-3037 -=- Fidonet 1:13/75",
		.registration_banner = "Version 3.6 * YT * Compiled 03/16/94",
		YT_PATCH_TEXT(ground_forces_label, malformed_ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = 200.0,
		.earth_purchase_price = 4096000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 250000U,
		.cargo_hold_coefficient = 500.0f,
		.fighter_coefficient = 100.0f,
		.anti_cloak_cost = 80000000.0f,
		.ground_force_coefficient = 750.0f,
		.spy_cost = 333000U,
		.plasma_score_weight = 1000000.0f,
		.xannor_headquarters_award = 1000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = '!',
		.computer_credit_key = '$',
	},
	[YT_PATCH_36C] = {
		.level = YT_PATCH_36C,
		.name = "3.6C",
		.open_doors_version = "3.6C",
		.registration_author = "(c)Alan Davenport",
		.registration_description =
		    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		.registration_contact =
		    "Strategy Guide: www.starflt.com/yt.html      ",
		.registration_banner = "Version 3.6c * YT * Modded 3/28/2021",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = 8000.0,
		.earth_purchase_price = 4096000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 250000U,
		.cargo_hold_coefficient = 500.0f,
		.fighter_coefficient = 100.0f,
		.anti_cloak_cost = 1000000000.0f,
		.ground_force_coefficient = 750.0f,
		.spy_cost = 333000U,
		.plasma_score_weight = 16000000.0f,
		.xannor_headquarters_award = 16000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = '!',
		.computer_credit_key = '$',
	},
	[YT_PATCH_36D] = {
		.level = YT_PATCH_36D,
		.name = "3.6D",
		.open_doors_version = "3.6D",
		.registration_author = "(c)Alan Davenport",
		.registration_description =
		    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		.registration_contact =
		    "Strategy Guide: www.starflt.com/yt.html      ",
		.registration_banner = "Version 3.6d * YT * Modded 3/28/2021",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = 8000.0,
		.earth_purchase_price = 4096000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 250000U,
		.cargo_hold_coefficient = 500.0f,
		.fighter_coefficient = 100.0f,
		.anti_cloak_cost = 1000000000.0f,
		.ground_force_coefficient = 750.0f,
		.spy_cost = 333000U,
		.plasma_score_weight = 1000000.0f,
		.xannor_headquarters_award = 1000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = ')',
		.computer_credit_key = '$',
	},
	[YT_PATCH_36E] = {
		.level = YT_PATCH_36E,
		.name = "3.6E",
		.open_doors_version = "3.6E",
		.registration_author = "(c)Alan Davenport",
		.registration_description =
		    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		.registration_contact =
		    "Strategy Guide: www.starflt.com/yt.html      ",
		.registration_banner = "Version 3.6e * YT * Mod 07/22/2022",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = -1.7013858731203996e38,
		.xannor_turn_divisor_raw = negative_xannor_divisor,
		.earth_purchase_price = 1000000000.0,
		.cloak_energy_cost = 8000.0f,
		.danger_scanner_cost = 500000U,
		.cargo_hold_coefficient = 250.0f,
		.fighter_coefficient = 50.0f,
		.anti_cloak_cost = 1000000000.0f,
		.ground_force_coefficient = 200.5f,
		.spy_cost = 1000000000U,
		.plasma_score_weight = 1000000.0f,
		.xannor_headquarters_award = 1000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = ')',
		.computer_credit_key = '!',
	},
	[YT_PATCH_36F] = {
		.level = YT_PATCH_36F,
		.name = "3.6F",
		.open_doors_version = "3.6F",
		.registration_author = "(c)Alan Davenport",
		.registration_description =
		    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		.registration_contact =
		    "Strategy Guide: www.starflt.com/yt.html      ",
		.registration_banner = "Version 3.6f * YT * Mod 10/03/2022",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = -1.7013858731203996e38,
		.xannor_turn_divisor_raw = negative_xannor_divisor,
		.earth_purchase_price = 1000000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 500000U,
		.cargo_hold_coefficient = 250.0f,
		.fighter_coefficient = 50.0f,
		.anti_cloak_cost = 1000000000.0f,
		.ground_force_coefficient = 200.5f,
		.spy_cost = 1000000000U,
		.plasma_score_weight = 16000000.0f,
		.xannor_headquarters_award = 16000000.0f,
		.initializer_sector_count = 3004U,
		.missile_key = ')',
		.computer_credit_key = '!',
	},
	[YT_PATCH_36G] = {
		.level = YT_PATCH_36G,
		.name = "3.6G",
		.open_doors_version = "3.6G",
		.registration_author = "(c)Alan Davenport",
		.registration_description =
		    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		.registration_contact =
		    "Strategy Guide: www.starflt.com/yt.html      ",
		.registration_banner = "Version 3.6g * YT * Mod 02/09/2024  ",
		YT_PATCH_TEXT(ground_forces_label, ground_forces),
		YT_PATCH_TEXT(anti_cloak_activation,
		    malformed_anti_cloak_activation),
		YT_PATCH_TEXT(anti_cloak_waves, malformed_anti_cloak_waves),
		.xannor_turn_divisor = 256000.0,
		.earth_purchase_price = 1000000000.0,
		.cloak_energy_cost = 1000.0f,
		.danger_scanner_cost = 500000U,
		.cargo_hold_coefficient = 250.0f,
		.fighter_coefficient = 50.0f,
		.anti_cloak_cost = 1000000000.0f,
		.ground_force_coefficient = 200.5f,
		.spy_cost = 1000000000U,
		.plasma_score_weight = 16000000.0f,
		.xannor_headquarters_award = 16000000.0f,
		.initializer_sector_count = 2004U,
		.missile_key = ')',
		.computer_credit_key = '!',
	},
};

#undef YT_PATCH_TEXT

const struct yt_patch_profile *
yt_patch_default(void)
{
	return &profiles[YT_PATCH_36];
}

const struct yt_patch_profile *
yt_patch_get(enum yt_patch_level level)
{
	if (level < 0 || level >= YT_PATCH_LEVEL_COUNT)
		return NULL;
	return &profiles[level];
}

const struct yt_patch_profile *
yt_patch_find(const char *name)
{
	size_t index;

	if (name == NULL)
		return NULL;
	for (index = 0U; index < YT_ARRAY_LEN(profiles); ++index) {
		if (qb_ascii_casecmp(name, profiles[index].name) == 0)
			return &profiles[index];
	}
	return NULL;
}

double
yt_patch_xannor_turn_divisor(const struct yt_patch_profile *patch)
{
	if (patch == NULL)
		patch = yt_patch_default();
	if (patch->xannor_turn_divisor_raw != NULL)
		return qb_mbf64_decode(patch->xannor_turn_divisor_raw);
	return patch->xannor_turn_divisor;
}

bool
yt_patch_sector_count_matches(const struct yt_patch_profile *patch,
    uint16_t sector_count)
{
	if (patch == NULL)
		patch = yt_patch_default();
	return sector_count == patch->initializer_sector_count;
}
