#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <math.h>
#include <string.h>

bool
yt_maintenance_default_headquarters(float *headquarters)
{
	if (headquarters == NULL || *headquarters != 0.0f)
		return false;
	*headquarters = 85.0f;
	return true;
}

static bool
maintenance_output_row(struct yt_maintenance_output_result *result,
    uint16_t address, const uint8_t *data, size_t length, bool newline)
{
	struct yt_maintenance_output_row *row;

	if ((data == NULL && length != 0U)
	    || result->row_count >= YT_MAINTENANCE_OUTPUT_ROWS
	    || length > YT_MAINTENANCE_OUTPUT_ROW_SIZE
	    || length > YT_MAINTENANCE_OUTPUT_SIZE - result->output_length
	    || (newline
	    && result->output_length + length >= YT_MAINTENANCE_OUTPUT_SIZE))
		return false;
	row = &result->rows[result->row_count++];
	row->address = address;
	row->newline = newline;
	row->length = length;
	if (length != 0U) {
		memcpy(row->data, data, length);
		memcpy(result->output + result->output_length, data, length);
		result->output_length += length;
	}
	if (newline) {
		result->output[result->output_length++] = '\r';
		result->final_column = 0U;
	}
	else
		result->final_column += length;
	return true;
}

static bool
maintenance_text_valid(const struct yt_maintenance_text *text)
{
	return text != NULL && (text->data != NULL || text->length == 0U);
}

bool
yt_maintenance_same_day(float stored_marker, float computed_serial)
{
	return stored_marker == computed_serial;
}

bool
yt_maintenance_compose_entry(bool same_day,
    struct yt_maintenance_output_result *result)
{
	static const struct yt_maintenance_text empty = {NULL, 0U};
	static const uint8_t same_day_text[] = "Maintenance not needed!";
	static const uint8_t title[] = "Yankee Trader Maintenance program";
	static const uint8_t byline[] = "        by Alan Davenport";
	static const uint8_t spacer[] = "       ";
	static const uint8_t revision[] = "(Revision 03/14/94)";
	static const uint8_t warning[] = "This should be run once per day.";
	static const uint8_t player_phase[] =
	    "Loading players, deleting inactive players and subtracting cloak charge.";

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
#define ROW(address, value, line) \
	maintenance_output_row(result, address, (value).data, (value).length, line)
#define LITERAL(address, value, line) \
	maintenance_output_row(result, address, value, sizeof(value) - 1U, line)
	if (same_day
	    && (!ROW(0x036DU, empty, true)
	    || !LITERAL(0x037FU, same_day_text, true)))
		return false;
	if (!ROW(0x0399U, empty, true)
	    || !LITERAL(0x03ADU, title, true)
	    || !LITERAL(0x03BFU, byline, true)
	    || !ROW(0x03D3U, empty, true)
	    || !LITERAL(0x03E5U, spacer, false)
	    || !LITERAL(0x03ECU, revision, true)
	    || !ROW(0x03FDU, empty, true)
	    || !LITERAL(0x040CU, warning, true)
	    || !ROW(0x04E8U, empty, true)
	    || !LITERAL(0x04FCU, player_phase, true)
	    || !ROW(0x050DU, empty, true))
		return false;
#undef LITERAL
#undef ROW
	return true;
}

static bool
maintenance_two_rows(uint16_t first_address, uint16_t second_address,
    const uint8_t *second, size_t second_length,
    struct yt_maintenance_output_result *result)
{
	if (result == NULL || (second == NULL && second_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, first_address, NULL, 0U, true)
	    && maintenance_output_row(result, second_address, second,
	    second_length, true);
}

bool
yt_maintenance_compose_wrapper(struct yt_maintenance_output_result *result)
{
	static const uint8_t completed[] = "Daily Maintenance Completed OK";

	return maintenance_two_rows(0x004FU,
	    0x0061U, completed, sizeof(completed) - 1U, result);
}

bool
yt_maintenance_compose_message_compaction(
    struct yt_maintenance_output_result *result)
{
	static const uint8_t compact[] = "Compressing Message Base's";

	return maintenance_two_rows(0x673AU,
	    0x674CU, compact, sizeof(compact) - 1U, result);
}

bool
yt_maintenance_compose_port_phase(const uint8_t *blank,
    size_t blank_length, int plagued_count,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t phase[] = "Running port maintenance...";
	static const uint8_t count_suffix[] =
	    " *** ports contracted the plague and lost productivity! ***";
	char number[48];
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t length = 0U;
	int number_length;

	if (result == NULL || (blank == NULL
	    && blank_length != 0U) || plagued_count < 0
	    || plagued_count > 1000)
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x07A3U, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x07B5U, phase,
	    sizeof(phase) - 1U, true))
		return false;
	if (plagued_count == 0)
		return true;
	number_length = qb_str_single(number, sizeof(number),
	    (float)plagued_count);
	if (number_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)number, (size_t)number_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    count_suffix, sizeof(count_suffix) - 1U)
	    || !maintenance_output_row(result, 0x0F62U, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x0F94U, line, length, true))
		return false;
	return true;
}

bool
yt_maintenance_compose_mercenary_phase(const uint8_t *blank,
    size_t blank_length, float tax_pool, bool rebuilt_base,
    float hired_fighters, struct yt_maintenance_output_result *result)
{
	static const uint8_t tax_prefix[] =
	    "  -  The goverment has collected";
	static const uint8_t tax_suffix[] = " credits tax from the ports.";
	static const uint8_t phase[] = "Mercenary Maintenance...";
	static const uint8_t checking[] =
	    "Checking for Mercenary Planet.. Rebuild if missing";
	static const uint8_t rebuilt[] =
	    "  -  The Mercenaries have built a home base using a captured "
	    "Genesis Device!";
	static const uint8_t hired_prefix[] = "  -  The government has hired";
	static const uint8_t hired_suffix[] =
	    " mercenaries to help Fight the Xannor!";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char number[64];
	size_t length;
	int number_length;

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x3F86U, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x3F95U, NULL, 0U, true))
		return false;
	if (tax_pool != 0.0f) {
		length = 0U;
		number_length = qb_str_single(number, sizeof(number), tax_pool);
		if (number_length < 0
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    tax_prefix, sizeof(tax_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)number, (size_t)number_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    tax_suffix, sizeof(tax_suffix) - 1U)
		    || !maintenance_output_row(result, 0x40BCU, line, length,
		    true))
			return false;
	}
	if (!maintenance_output_row(result, 0x40DEU, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x40F2U, phase,
	    sizeof(phase) - 1U, true)
	    || !maintenance_output_row(result, 0x4103U, NULL, 0U, true)
	    || !maintenance_output_row(result, 0x4130U, checking,
	    sizeof(checking) - 1U, true))
		return false;
	if (rebuilt_base
	    && (!maintenance_output_row(result, 0x41DEU, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x41FAU, rebuilt,
	    sizeof(rebuilt) - 1U, true)))
		return false;
	if (hired_fighters != 0.0f) {
		length = 0U;
		number_length = qb_str_single(number, sizeof(number),
		    hired_fighters);
		if (number_length < 0
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    hired_prefix, sizeof(hired_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)number, (size_t)number_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    hired_suffix, sizeof(hired_suffix) - 1U)
		    || !maintenance_output_row(result, 0x4631U, line, length,
		    true))
			return false;
	}
	return true;
}

bool
yt_maintenance_compose_mercenary_movement(double moving_fighters,
    float origin_sector, struct yt_maintenance_output_result *result)
{
	static const uint8_t prefix[] = "  - ";
	static const uint8_t label[] = "Mercenaries moving from sector";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char moving[64];
	char origin[64];
	size_t length = 0U;
	int moving_length;
	int origin_length;

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	moving_length = qb_print_double(moving, sizeof(moving),
	    moving_fighters);
	origin_length = qb_print_single(origin, sizeof(origin), origin_sector);
	if (moving_length < 0 || origin_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    prefix, sizeof(prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)moving, (size_t)moving_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    label, sizeof(label) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)origin, (size_t)origin_length))
		return false;
	return maintenance_output_row(result, 0x4911U, line, length, true);
}

bool
yt_maintenance_compose_planet_phase(const uint8_t *blank,
    size_t blank_length, const struct yt_maintenance_text *planet_name,
    const struct yt_maintenance_planet_result *mutation,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t phase[] = "Running planet maintenance...";
	static const uint8_t event_prefix[] = "  -  ";
	static const uint8_t plague[] = "A PLAGUE";
	static const uint8_t civil_war[] = "CIVIL WAR";
	static const uint8_t event_middle[] = " has struck planet ";
	static const uint8_t event_suffix[] =
	    " as a result of overcrowding!";
	static const uint8_t production_prefix[] =
	    "  -  Productivity reduced from";
	static const uint8_t production_middle[] = " units to";
	static const uint8_t units_suffix[] = " units!";
	static const uint8_t ground_prefix[] =
	    "  -  Ground forces reduced from";
	static const uint8_t ground_middle[] = " to";
	static const uint8_t expense_prefix[] = "  - ";
	static const uint8_t expense_suffix[] =
	    " credits were spent putting down the insurrection!";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char first[64];
	char second[64];
	const uint8_t *event_name;
	size_t event_name_length;
	size_t length;
	int first_length;
	int second_length;

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	if (mutation != NULL
	    && (mutation->event < YT_MAINTENANCE_PLANET_NO_EVENT
	    || mutation->event > YT_MAINTENANCE_PLANET_CIVIL_WAR
	    || (mutation->event == YT_MAINTENANCE_PLANET_NO_EVENT
	    && (mutation->emit_ground_line
	    || mutation->civil_war_expense != 0.0f))
	    || (mutation->event == YT_MAINTENANCE_PLANET_PLAGUE
	    && mutation->civil_war_expense != 0.0f)))
		return false;
	if (mutation != NULL
	    && mutation->event != YT_MAINTENANCE_PLANET_NO_EVENT
	    && !maintenance_text_valid(planet_name))
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x0FB6U, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x0FC8U, phase,
	    sizeof(phase) - 1U, true))
		return false;
	if (mutation == NULL
	    || mutation->event == YT_MAINTENANCE_PLANET_NO_EVENT)
		return true;
	if (mutation->event == YT_MAINTENANCE_PLANET_PLAGUE) {
		event_name = plague;
		event_name_length = sizeof(plague) - 1U;
	}
	else if (mutation->event == YT_MAINTENANCE_PLANET_CIVIL_WAR) {
		event_name = civil_war;
		event_name_length = sizeof(civil_war) - 1U;
	}
	else
		return false;
	if (!maintenance_output_row(result, 0x1899U, blank,
	    blank_length, true))
		return false;
	length = 0U;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    event_prefix, sizeof(event_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    event_name, event_name_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    event_middle, sizeof(event_middle) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    planet_name->data, planet_name->length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    event_suffix, sizeof(event_suffix) - 1U)
	    || !maintenance_output_row(result, 0x196AU, line, length, true))
		return false;
	first_length = qb_str_single(first, sizeof(first),
	    mutation->old_event_total);
	second_length = qb_str_single(second, sizeof(second),
	    mutation->new_event_total);
	length = 0U;
	if (first_length < 0 || second_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    production_prefix, sizeof(production_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)first, (size_t)first_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    production_middle, sizeof(production_middle) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)second, (size_t)second_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    units_suffix, sizeof(units_suffix) - 1U)
	    || !maintenance_output_row(result, 0x19FBU, line, length, true))
		return false;
	if (mutation->emit_ground_line) {
		first_length = qb_str_single(first, sizeof(first),
		    floorf(mutation->old_event_ground));
		second_length = qb_str_single(second, sizeof(second),
		    floorf(mutation->new_event_ground));
		length = 0U;
		if (first_length < 0 || second_length < 0
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    ground_prefix, sizeof(ground_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)first, (size_t)first_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    ground_middle, sizeof(ground_middle) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)second, (size_t)second_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    units_suffix, sizeof(units_suffix) - 1U)
		    || !maintenance_output_row(result, 0x1AB6U, line, length,
		    true))
			return false;
	}
	if (mutation->event == YT_MAINTENANCE_PLANET_CIVIL_WAR
	    && mutation->civil_war_expense != 0.0f) {
		first_length = qb_str_double(first, sizeof(first),
		    (double)mutation->civil_war_expense);
		length = 0U;
		if (first_length < 0
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    expense_prefix, sizeof(expense_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    (const uint8_t *)first, (size_t)first_length)
		    || !maintenance_copy_part(line, sizeof(line), &length,
		    expense_suffix, sizeof(expense_suffix) - 1U)
		    || !maintenance_output_row(result, 0x1B0BU, line, length,
		    true))
			return false;
	}
	return true;
}

bool
yt_maintenance_compose_wanderer_phase(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t phase[] = "Moving The Wanderer (Planet #1)";
	static const uint8_t missing[] =
	    "  -  The Wanderer is missing or has been destroyed!";
	static const uint8_t regenerated[] =
	    "  -  The Wanderer regenerated with P.H.O.E.N.I.X. device!";
	static const uint8_t warped[] = "Wanderer has successfully warped!";

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x1CEBU, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x1CFDU, phase,
	    sizeof(phase) - 1U, true))
		return false;
	if (rebuilt
	    && (!maintenance_output_row(result, 0x1DC4U, missing,
	    sizeof(missing) - 1U, true)
	    || !maintenance_output_row(result, 0x1F37U, regenerated,
	    sizeof(regenerated) - 1U, true)))
		return false;
	return maintenance_output_row(result, 0x1F6CU, blank,
	    blank_length, true)
	    && maintenance_output_row(result, 0x1F93U, warped,
	    sizeof(warped) - 1U, true);
}

bool
yt_maintenance_compose_xannor_home(const uint8_t *blank,
    size_t blank_length, bool rebuilt,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t phase[] =
	    "Checking for Planet Xannor, create it if missing.";
	static const uint8_t created[] =
	    "  -  The Xannor have made a Planet!";
	static const uint8_t linked[] =
	    "The Xannor home base now has a planet!";

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x2073U, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x2085U, phase,
	    sizeof(phase) - 1U, true))
		return false;
	if (!rebuilt)
		return true;
	return maintenance_output_row(result, 0x2107U, blank,
	    blank_length, true)
	    && maintenance_output_row(result, 0x2123U, created,
	    sizeof(created) - 1U, true)
	    && maintenance_output_row(result, 0x22C4U, linked,
	    sizeof(linked) - 1U, true);
}

bool
yt_maintenance_compose_xannor_hunt(const uint8_t *blank,
    size_t blank_length, const struct yt_maintenance_text *hunt_name,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t processing[] = "Processing the Xannor.....";
	static const uint8_t locating[] =
	    "Locating Top Player... (For Groups 16 - 20 to Pick on!)";
	static const uint8_t hunt_prefix[] = "Group 20 will hunt for ";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t length = 0U;

	if (result == NULL || (blank == NULL
	    && blank_length != 0U)
	    || (hunt_name != NULL && !maintenance_text_valid(hunt_name)))
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_output_row(result, 0x23EAU, blank,
	    blank_length, true)
	    || !maintenance_output_row(result, 0x23FEU, processing,
	    sizeof(processing) - 1U, true)
	    || !maintenance_output_row(result, 0x240FU, NULL, 0U, true)
	    || !maintenance_output_row(result, 0x2421U, locating,
	    sizeof(locating) - 1U, true))
		return false;
	if (hunt_name == NULL)
		return true;
	if (!maintenance_copy_part(line, sizeof(line), &length,
	    hunt_prefix, sizeof(hunt_prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    hunt_name->data, hunt_name->length))
		return false;
	return maintenance_output_row(result, 0x25D6U, blank,
	    blank_length, true)
	    && maintenance_output_row(result, 0x2604U, line, length, true);
}

bool
yt_maintenance_compose_xannor_regeneration(const uint8_t *blank,
    size_t blank_length, double regeneration,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t prefix[] =
	    "Calculated Dynamic Xannor Regeneration is";
	static const uint8_t suffix[] = " fighters.";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char number[64];
	size_t length = 0U;
	int number_length;

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	number_length = qb_str_double(number, sizeof(number), regeneration);
	if (number_length < 0
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    prefix, sizeof(prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    (const uint8_t *)number, (size_t)number_length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    suffix, sizeof(suffix) - 1U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x2A47U, blank,
	    blank_length, true)
	    && maintenance_output_row(result, 0x2A7AU, line, length, true)
	    && maintenance_output_row(result, 0x2A9CU, blank,
	    blank_length, true);
}

bool
yt_maintenance_compose_xannor_reclaim_attempt(
    const struct yt_maintenance_text *opponent,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t prefix[] =
	    " *** The Xannor are attempting to reclaim their base from ";
	static const uint8_t suffix[] = "!";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t length = 0U;

	if (result == NULL || !maintenance_text_valid(opponent)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    prefix, sizeof(prefix) - 1U)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    opponent->data, opponent->length)
	    || !maintenance_copy_part(line, sizeof(line), &length,
	    suffix, sizeof(suffix) - 1U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x2BE3U, line, length, true);
}

bool
yt_maintenance_compose_xannor_reclaim_result(bool successful,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t success[] = " *** Successful!";
	static const uint8_t failure[] = " *** Failed!";
	const uint8_t *line = successful ? success : failure;
	size_t length = successful ? sizeof(success) - 1U : sizeof(failure) - 1U;

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x2D3BU, line, length, true);
}

bool
yt_maintenance_compose_xannor_relocation(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result)
{
	static const uint8_t line[] =
	    " *** The Xannor have MOVED their Headquarters! ***\a";

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x2F8FU, line,
	    sizeof(line) - 1U, true)
	    && maintenance_output_row(result, 0x2FA1U, blank,
	    blank_length, true);
}

bool
yt_maintenance_compose_xannor_revenge(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result)
{
	static const uint8_t line[] = " *** Xannor REVENGE! ***\a";

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x3051U, blank,
	    blank_length, true)
	    && maintenance_output_row(result, 0x308BU, line,
	    sizeof(line) - 1U, true)
	    && maintenance_output_row(result, 0x309DU, blank,
	    blank_length, true);
}

bool
yt_maintenance_compose_xannor_roaming(const uint8_t *blank,
    size_t blank_length, struct yt_maintenance_output_result *result)
{
	static const uint8_t line[] = "The Xannor are on the prowl...";

	if (result == NULL || (blank == NULL
	    && blank_length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	return maintenance_output_row(result, 0x30F7U, line,
	    sizeof(line) - 1U, true)
	    && maintenance_output_row(result, 0x3109U, blank,
	    blank_length, true);
}

bool
yt_maintenance_compose_xannor_group(int group_number, float group_size,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t prefix[] = "  -  Group:";
	static const uint8_t gap[] = "  ";
	static const uint8_t size_prefix[] = "Size:";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char number[64];
	size_t length = 0U;
	size_t column;
	size_t padding;
	int number_length;

	if (result == NULL || group_number < 2 || group_number > 20)
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_copy_part(line, sizeof(line), &length, prefix,
	    sizeof(prefix) - 1U))
		return false;
	number_length = qb_print_single(number, sizeof(number),
	    (float)group_number);
	if (number_length < 0 || !maintenance_copy_part(line, sizeof(line),
	    &length, (const uint8_t *)number, (size_t)number_length)
	    || !maintenance_copy_part(line, sizeof(line), &length, gap,
	    sizeof(gap) - 1U))
		return false;
	column = length % 80U;
	padding = 14U - column % 14U;
	if (padding > sizeof(line) - length)
		return false;
	memset(line + length, ' ', padding);
	length += padding;
	if (!maintenance_copy_part(line, sizeof(line), &length, size_prefix,
	    sizeof(size_prefix) - 1U))
		return false;
	number_length = qb_print_single(number, sizeof(number), group_size);
	if (number_length < 0 || !maintenance_copy_part(line, sizeof(line),
	    &length, (const uint8_t *)number, (size_t)number_length))
		return false;
	return maintenance_output_row(result, 0x34B2U, line, length, true);
}

bool
yt_maintenance_compose_xannor_path_error(float source, float target,
    struct yt_maintenance_output_result *result)
{
	static const uint8_t prefix[] =
	    "*** Error - Sector path not found - from sector";
	static const uint8_t infix[] = " to sector ";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	char number[64];
	size_t length = 0U;
	int number_length;

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	if (!maintenance_copy_part(line, sizeof(line), &length, prefix,
	    sizeof(prefix) - 1U))
		return false;
	number_length = qb_str_single(number, sizeof(number), source);
	if (number_length < 0 || !maintenance_copy_part(line, sizeof(line),
	    &length, (const uint8_t *)number, (size_t)number_length)
	    || !maintenance_copy_part(line, sizeof(line), &length, infix,
	    sizeof(infix) - 1U))
		return false;
	number_length = qb_str_single(number, sizeof(number), target);
	if (number_length < 0 || !maintenance_copy_part(line, sizeof(line),
	    &length, (const uint8_t *)number, (size_t)number_length))
		return false;
	return maintenance_output_row(result, 0x38A8U, line, length, true);
}

bool
maintenance_copy_part(uint8_t *dest, size_t capacity, size_t *length,
    const uint8_t *data, size_t data_length)
{
	if (*length > capacity || (data == NULL && data_length != 0U)
	    || data_length > capacity - *length)
		return false;
	if (data_length != 0U)
		memcpy(dest + *length, data, data_length);
	*length += data_length;
	return true;
}

bool
yt_maintenance_compose_player_aging(
    const struct yt_maintenance_text *name,
    const struct yt_maintenance_text *time_text,
    const struct yt_maintenance_text *date_text, bool cloak_expired,
    bool delete_player, struct yt_maintenance_player_output_result *result)
{
	static const uint8_t expiry_prefix[] =
	    " *** Cloaking Device Energy Expired for ";
	static const uint8_t deletion_prefix[] = " *** ";
	static const uint8_t deletion_suffix[] = " deleted from game";
	static const uint8_t radio_prefix[] =
	    "Your cloaking energy ran out at ";
	static const uint8_t radio_middle[] = " on ";
	static const uint8_t suffix[] = "!";
	uint8_t line[YT_MAINTENANCE_OUTPUT_ROW_SIZE];
	size_t line_length = 0U;

	if (result == NULL || !maintenance_text_valid(name)
	    || !maintenance_text_valid(time_text)
	    || !maintenance_text_valid(date_text))
		return false;
	memset(result, 0, sizeof(*result));
	if (cloak_expired) {
		if (!maintenance_copy_part(line, sizeof(line), &line_length,
		    expiry_prefix, sizeof(expiry_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    name->data, name->length)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    suffix, sizeof(suffix) - 1U)
		    || !maintenance_output_row(&result->screen, 0x067AU, line,
		    line_length, true)
		    || !maintenance_copy_part(result->radio_message,
		    sizeof(result->radio_message), &result->radio_length,
		    radio_prefix, sizeof(radio_prefix) - 1U)
		    || !maintenance_copy_part(result->radio_message,
		    sizeof(result->radio_message), &result->radio_length,
		    time_text->data, time_text->length)
		    || !maintenance_copy_part(result->radio_message,
		    sizeof(result->radio_message), &result->radio_length,
		    radio_middle, sizeof(radio_middle) - 1U)
		    || !maintenance_copy_part(result->radio_message,
		    sizeof(result->radio_message), &result->radio_length,
		    date_text->data, date_text->length)
		    || !maintenance_copy_part(result->radio_message,
		    sizeof(result->radio_message), &result->radio_length,
		    suffix, sizeof(suffix) - 1U))
			return false;
	}
	else if (delete_player) {
		if (!maintenance_copy_part(line, sizeof(line), &line_length,
		    deletion_prefix, sizeof(deletion_prefix) - 1U)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    name->data, name->length)
		    || !maintenance_copy_part(line, sizeof(line), &line_length,
		    deletion_suffix, sizeof(deletion_suffix) - 1U)
		    || !maintenance_output_row(&result->screen, 0x0749U, line,
		    line_length, true))
			return false;
		result->deletion_reached = true;
	}
	return true;
}
