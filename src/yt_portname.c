#include "yt_portname.h"

#include "qb.h"
#include "yt_init.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const struct yt_portname_runtime_site portname_runtime_sites[] = {
	{0x003AU, 0x003FU},
	{0x003FU, 0x0044U},
	{0x0047U, 0x004CU},
	{0x0058U, 0x005DU},
	{0x0060U, 0x0065U},
	{0x0065U, 0x006AU},
	{0x006DU, 0x0072U},
	{0x0072U, 0x0077U},
	{0x0077U, 0x007CU},
	{0x0081U, 0x0086U},
	{0x0086U, 0x008BU},
	{0x008BU, 0x0090U},
	{0x0093U, 0x0098U},
	{0x0098U, 0x009DU},
	{0x009DU, 0x00A2U},
	{0x00A4U, 0x00A9U},
	{0x00A9U, 0x00AEU},
	{0x00AEU, 0x00B3U},
	{0x00B3U, 0x00B8U},
	{0x00B8U, 0x00BDU},
	{0x00BDU, 0x00C2U},
	{0x00C5U, 0x00CAU},
	{0x00CAU, 0x00CFU},
	{0x00CFU, 0x00D4U},
	{0x00D7U, 0x00DCU},
	{0x00DCU, 0x00E1U},
	{0x00E3U, 0x00E9U},
	{0x00E9U, 0x00F0U},
	{0x00F3U, 0x00F8U},
	{0x00FDU, 0x0102U},
	{0x0107U, 0x010CU},
	{0x0114U, 0x0119U},
	{0x011BU, 0x0120U},
	{0x0122U, 0x0127U},
	{0x012BU, 0x0130U},
	{0x0130U, 0x0135U},
	{0x013AU, 0x013FU},
	{0x0144U, 0x0149U},
	{0x014CU, 0x0151U},
	{0x0151U, 0x0156U},
	{0x0156U, 0x015BU},
	{0x015EU, 0x0163U},
	{0x0163U, 0x0168U},
	{0x0168U, 0x016DU},
	{0x016DU, 0x0172U},
	{0x0175U, 0x017AU},
	{0x017DU, 0x0182U},
	{0x0185U, 0x018AU},
	{0x018AU, 0x018FU},
	{0x018FU, 0x0194U},
	{0x0194U, 0x0199U},
	{0x0199U, 0x019EU},
	{0x019EU, 0x01A3U},
	{0x01A6U, 0x01ABU},
	{0x01ABU, 0x01B0U},
	{0x01B6U, 0x01BBU},
	{0x01BEU, 0x01C3U},
	{0x01CCU, 0x01D1U},
	{0x01DCU, 0x01E1U},
	{0x01E7U, 0x01ECU},
	{0x01EFU, 0x01F4U},
	{0x01F9U, 0x01FEU},
	{0x01FEU, 0x0203U},
	{0x0208U, 0x020DU},
	{0x0212U, 0x0217U},
	{0x021CU, 0x0222U},
	{0x0222U, 0x0227U},
	{0x022EU, 0x0233U},
	{0x0233U, 0x0238U},
	{0x0238U, 0x023DU},
	{0x0240U, 0x0245U},
	{0x0248U, 0x024DU},
	{0x0250U, 0x0255U},
	{0x0259U, 0x025EU},
	{0x0263U, 0x0268U},
	{0x026BU, 0x0270U},
	{0x0275U, 0x027AU},
	{0x027FU, 0x0284U},
	{0x0287U, 0x028CU},
	{0x028CU, 0x0291U},
	{0x0291U, 0x0296U},
	{0x0299U, 0x029EU},
	{0x029EU, 0x02A3U},
	{0x02A3U, 0x02A8U},
	{0x02ABU, 0x02B0U},
	{0x02B0U, 0x02B5U},
	{0x02B8U, 0x02BDU},
	{0x02C2U, 0x02C7U},
	{0x02CFU, 0x02D4U},
	{0x02D4U, 0x02D9U},
	{0x02DFU, 0x02E4U},
	{0x02EAU, 0x02EFU},
	{0x02F2U, 0x02F7U},
	{0x02FAU, 0x02FFU},
	{0x0302U, 0x0307U},
	{0x030AU, 0x030FU},
	{0x0312U, 0x0317U},
	{0x031AU, 0x031FU},
	{0x0322U, 0x0327U},
	{0x032AU, 0x032FU},
	{0x0332U, 0x0337U},
	{0x033AU, 0x033FU},
	{0x0342U, 0x0347U},
	{0x034AU, 0x034FU},
	{0x0352U, 0x0357U},
	{0x035AU, 0x035FU},
	{0x0362U, 0x0367U},
	{0x036AU, 0x036FU},
	{0x0372U, 0x0377U},
	{0x037AU, 0x037FU},
	{0x0382U, 0x0387U},
	{0x038AU, 0x038FU},
	{0x0392U, 0x0397U},
	{0x039AU, 0x039FU},
	{0x03A3U, 0x03A8U},
	{0x03A8U, 0x03ADU},
	{0x03B2U, 0x03B7U},
	{0x03BAU, 0x03BFU},
	{0x03BFU, 0x03C4U},
	{0x03C4U, 0x03C9U},
	{0x03C9U, 0x03CEU},
	{0x03CEU, 0x03D3U},
	{0x03D3U, 0x03D8U},
	{0x03DDU, 0x03E2U},
	{0x03E7U, 0x03ECU},
	{0x03EEU, 0x03F3U},
	{0x03F5U, 0x03FAU},
	{0x03FAU, 0x03FFU},
	{0x03FFU, 0x0404U},
	{0x0404U, 0x0409U},
	{0x040CU, 0x0411U},
	{0x0411U, 0x0416U},
	{0x041CU, 0x0421U},
	{0x0424U, 0x0429U},
	{0x042CU, 0x0431U},
	{0x0434U, 0x0439U},
	{0x043CU, 0x0441U},
	{0x0445U, 0x044AU},
	{0x0452U, 0x0457U},
	{0x045DU, 0x0462U},
	{0x0467U, 0x046CU},
	{0x0477U, 0x047CU},
	{0x047FU, 0x0484U},
	{0x0489U, 0x048EU},
	{0x0496U, 0x049BU},
	{0x049DU, 0x04A2U},
	{0x04A2U, 0x04A7U},
	{0x04AFU, 0x04B4U},
	{0x04B6U, 0x04BBU},
	{0x04C2U, 0x04C8U},
	{0x04CEU, 0x04D3U},
	{0x04D6U, 0x04DBU},
	{0x04E1U, 0x04E6U},
	{0x04E8U, 0x04EDU},
	{0x04FCU, 0x0501U},
	{0x0501U, 0x0506U},
	{0x050AU, 0x050FU},
	{0x0513U, 0x0518U},
	{0x051DU, 0x0522U},
	{0x0522U, 0x0527U},
	{0x052EU, 0x0533U},
	{0x0542U, 0x0547U},
	{0x054CU, 0x0551U},
	{0x0554U, 0x0559U},
	{0x055EU, 0x0563U},
	{0x056EU, 0x0573U},
	{0x0573U, 0x0578U},
	{0x0578U, 0x057EU},
	{0x057EU, 0x0583U},
	{0x0583U, 0x0589U},
	{0x058CU, 0x0591U},
	{0x0594U, 0x0599U},
	{0x059CU, 0x05A1U},
	{0x05A4U, 0x05A9U},
	{0x05ACU, 0x05B1U},
	{0x05B4U, 0x05B9U},
	{0x05BCU, 0x05C1U},
	{0x05C4U, 0x05C9U},
	{0x05CCU, 0x05D1U},
	{0x05D1U, 0x05D6U},
	{0x05E1U, 0x05E6U},
	{0x05E8U, 0x05EDU},
	{0x05F0U, 0x05F5U},
	{0x05F8U, 0x05FDU},
	{0x0602U, 0x0607U},
	{0x0611U, 0x0616U},
	{0x061BU, 0x0620U},
	{0x0627U, 0x062CU},
	{0x0633U, 0x0638U},
	{0x063AU, 0x063FU},
	{0x0647U, 0x064CU},
	{0x064CU, 0x0651U},
	{0x0655U, 0x065AU},
	{0x065EU, 0x0663U},
};

static const uint8_t portname_intro[] =
    "\r"
    "          Yankee Trader Remote Port Rename Program\r"
    "                     By Alan Davenport\r"
    "\r"
    "\r"
    "This program will apply new, random port names to an existing game without\r"
    "effecting any other setting. Do you wish to continue? [y/N] -=> ";
static const uint8_t portname_missing[] =
    "\r\r\aERROR! DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\r";
static const uint8_t portname_abort[] = "\rAborted!\r";
static const uint8_t portname_renaming[] = "\r\rRenaming ports...\r";
static const uint8_t portname_complete[] =
    "\rNew, random names applied to all ports!\r";

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static float
single_add(float left, float right)
{
	volatile float value = left + right;

	return value;
}

static float
single_sub(float left, float right)
{
	volatile float value = left - right;

	return value;
}

size_t
yt_portname_runtime_site_count(void)
{
	return YT_ARRAY_LEN(portname_runtime_sites);
}

bool
yt_portname_runtime_site(size_t index, struct yt_portname_runtime_site *site)
{
	if (site == NULL || index >= YT_ARRAY_LEN(portname_runtime_sites))
		return false;
	*site = portname_runtime_sites[index];
	return true;
}

bool
yt_portname_runtime_fatal_run(uint16_t site_address, uint8_t error_number,
    uint16_t module_segment, bool redirected_stdin, bool function_bar,
    bool cursor_shape_known, uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_runtime_fatal_state *state)
{
	static const char module[8] =
	    {'P', 'O', 'R', 'T', 'N', 'A', 'M', 'E'};
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(portname_runtime_sites); ++index) {
		if (portname_runtime_sites[index].address == site_address)
			return yt_brun_runtime_error_fatal_run(error_number, module,
			    false, 0, module_segment,
			    portname_runtime_sites[index].saved_ip, redirected_stdin,
			    function_bar, cursor_shape_known,
			    process_entry_cursor_shape, ops, context, state);
	}
	return false;
}

enum yt_portname_confirmation
yt_portname_confirm(const uint8_t *response, size_t length)
{
	if (response == NULL || length == 0U)
		return YT_PORTNAME_CONFIRM_BLANK;
	return (response[0] & 0xdfU) == 'Y'
	    ? YT_PORTNAME_CONFIRM_ACCEPT : YT_PORTNAME_CONFIRM_REJECT;
}

enum yt_portname_parse_result
yt_portname_parse_confirmation(const uint8_t *input, size_t input_length,
    uint8_t *value, size_t value_capacity, size_t *value_length)
{
	size_t start = 0U;
	size_t end;
	size_t length;

	if ((input == NULL && input_length != 0U) || value == NULL
	    || value_length == NULL)
		return YT_PORTNAME_PARSE_RANGE;
	while (start < input_length && (input[start] == ' '
	    || input[start] == '\t' || input[start] == '\n'))
		++start;
	if (start < input_length && input[start] == '"') {
		++start;
		end = start;
		while (end < input_length && input[end] != '"'
		    && input[end] != 0U)
			++end;
		length = end - start;
		if (end < input_length && input[end] == '"')
			++end;
		while (end < input_length && (input[end] == ' '
		    || input[end] == '\t' || input[end] == '\n'))
			++end;
		if (end < input_length && input[end] != 0U)
			return YT_PORTNAME_PARSE_REDO;
	}
	else {
		end = start;
		while (end < input_length && input[end] != ','
		    && input[end] != 0U)
			++end;
		if (end < input_length && input[end] == ',')
			return YT_PORTNAME_PARSE_REDO;
		while (end > start && input[end - 1U] == ' ')
			--end;
		length = end - start;
	}
	if (length > value_capacity)
		return YT_PORTNAME_PARSE_RANGE;
	if (length != 0U)
		memcpy(value, input + start, length);
	*value_length = length;
	return YT_PORTNAME_PARSE_VALID;
}

static bool
copy_output(struct yt_portname_output *output, const uint8_t *data,
    size_t length)
{
	if (length > sizeof(output->bytes))
		return false;
	if (length != 0U)
		memcpy(output->bytes, data, length);
	output->length = length;
	return true;
}

bool
yt_portname_compose_output(enum yt_portname_output_kind kind,
    float logical_port, const uint8_t *name, size_t name_length,
    struct yt_portname_output *output)
{
	const uint8_t *fixed = NULL;
	size_t fixed_length = 0U;

	if (output == NULL || (name == NULL && name_length != 0U))
		return false;
	memset(output, 0, sizeof(*output));
	switch (kind) {
	case YT_PORTNAME_OUTPUT_INTRO:
		fixed = portname_intro;
		fixed_length = sizeof(portname_intro) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_MISSING_DATA:
		fixed = portname_missing;
		fixed_length = sizeof(portname_missing) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_ABORT:
		fixed = portname_abort;
		fixed_length = sizeof(portname_abort) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_RENAMING:
		fixed = portname_renaming;
		fixed_length = sizeof(portname_renaming) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_COMPLETE:
		fixed = portname_complete;
		fixed_length = sizeof(portname_complete) - 1U;
		break;
	case YT_PORTNAME_OUTPUT_PROGRESS: {
		char number[64];
		int number_length = qb_print_single(number, sizeof(number),
		    logical_port);

		if (number_length < 0 || name_length + (size_t)number_length + 1U
		    > sizeof(output->bytes))
			return false;
		memcpy(output->bytes, number, (size_t)number_length);
		memcpy(output->bytes + number_length, name, name_length);
		output->bytes[(size_t)number_length + name_length] = '\r';
		output->length = (size_t)number_length + name_length + 1U;
		return true;
	}
	default:
		return false;
	}
	return copy_output(output, fixed, fixed_length);
}

uint32_t
yt_portname_record_number(float port_offset, float logical_port)
{
	float expression = single_add(port_offset, logical_port);

	return qb_brun_random_record_number(expression);
}

bool
yt_portname_overlay_record(struct yt_record *record, const uint8_t *name,
    size_t name_length, struct yt_error *error)
{
	if (record == NULL || (name == NULL && name_length != 0U)
	    || name_length > YT_TEXT_FIELD_SIZE) {
		set_error(error, YT_INVALID, "PORTNAME record overlay", "YTDATA.DAT");
		return false;
	}
	yt_record_set_text(record, name, name_length);
	if (!yt_record_set_number(record, YT_F85, (float)name_length)) {
		set_error(error, YT_RANGE, "encode PORTNAME name length",
		    "YTDATA.DAT");
		return false;
	}
	return true;
}

static bool
emit(enum yt_portname_output_kind kind, float logical_port,
    const uint8_t *name, size_t name_length, yt_portname_output_fn output,
    void *context, struct yt_error *error)
{
	struct yt_portname_output composed;

	return yt_portname_compose_output(kind, logical_port, name, name_length,
	    &composed)
	    && output(context, composed.bytes, composed.length, error);
}

bool
yt_portname_rename(struct yt_database *database, float port_offset,
    float planet_offset, struct yt_random *random,
    yt_portname_output_fn output, void *output_context,
    struct yt_portname_result *result, struct yt_error *error)
{
	struct yt_portname_result local = {0};
	float logical = 1.0f;
	uint64_t starting_draws;

	if (database == NULL || database->file == NULL || random == NULL
	    || output == NULL || result == NULL) {
		set_error(error, YT_INVALID, "PORTNAME rename", "YTDATA.DAT");
		return false;
	}
	starting_draws = random->draws;
	local.loop_bound = single_sub(planet_offset, port_offset);
	if (!emit(YT_PORTNAME_OUTPUT_RENAMING, 0.0f, NULL, 0U, output,
	    output_context, error))
		return false;
	while (logical <= local.loop_bound) {
		struct yt_record record;
		char generated[42];
		const uint8_t *name;
		size_t name_length;
		uint32_t physical;
		float next;

		if (logical == 1.0f) {
			name = (const uint8_t *)"Earth";
			name_length = 5U;
		}
		else {
			if (!yt_generate_port_name(random, generated, error))
				return false;
			name = (const uint8_t *)generated;
			name_length = strlen(generated);
		}
		if (!emit(YT_PORTNAME_OUTPUT_PROGRESS, logical, name, name_length,
		    output, output_context, error))
			return false;
		physical = yt_portname_record_number(port_offset, logical);
		if (!yt_database_read(database, (size_t)physical, &record, error)
		    || !yt_portname_overlay_record(&record, name, name_length, error)
		    || !yt_database_write(database, (size_t)physical, &record, error))
			return false;
		++local.iterations;
		next = single_add(logical, 1.0f);
		if (next == logical) {
			set_error(error, YT_RANGE, "PORTNAME FOR variable stalled", "");
			return false;
		}
		logical = next;
	}
	if (!emit(YT_PORTNAME_OUTPUT_COMPLETE, 0.0f, NULL, 0U, output,
	    output_context, error))
		return false;
	if (!yt_database_random_close(database, error))
		return false;
	/* Authorized departure: retain PLAY ordering without host audio. */
	local.play_event = true;
	local.final_logical_port = logical;
	local.draws_consumed = random->draws - starting_draws;
	*result = local;
	return true;
}
