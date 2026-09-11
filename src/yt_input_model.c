#include "yt_input_model.h"

#include "qb.h"

#include <math.h>
#include <string.h>

/* Generated from the documented upstream fault inventories at d16edb83. */
static const struct yt_input_fault_site b05d_fault_sites[] = {
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D45U, 0x1D48U, 0U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D8AU, 0x1D8DU, 0U, 610, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB069U, 0xB06CU, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB091U, 0xB094U, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB091U, 0xB094U, 0U, 36010, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB09DU, 0xB0A0U, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB0CBU, 0xB0CEU, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB0FFU, 0xB102U, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB13BU, 0xB13EU, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x43D0U, 0x43D3U, 0U, 64005, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x44CBU, 0x44CEU, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x44D3U, 0x44D6U, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x44E8U, 0x44EBU, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x44EEU, 0x44F1U, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x44F6U, 0x44F9U, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x4504U, 0x4507U, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x453DU, 0x4540U, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x454BU, 0x454EU, 0U, 64005, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x4576U, 0x4579U, 0U, 64005, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x45C1U, 0x45C4U, 0U, 64005, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB18BU, 0xB18EU, 0U, 36010, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D45U, 0x1D48U, 0U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D8AU, 0x1D8DU, 0U, 610, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB1BBU, 0xB1BEU, 0U, 36010, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB1E2U, 0xB1E5U, 0U, 36010, 7U, true},
};

static const struct yt_input_fault_site b1f3_fault_sites[] = {
	{YT_INPUT_FAULT_MODULE_YT, 0xB261U, 0xB264U, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB26DU, 0xB270U, 0U, 36010, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB272U, 0xB275U, 0U, 36010, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D99U, 0x1D9CU, 0U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DC2U, 0x1DC5U, 0U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DE0U, 0x1DE3U, 0U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DEAU, 0x1DEDU, 0U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB28CU, 0xB28FU, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB2ACU, 0xB2AFU, 0U, 36010, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB2AFU, 0xB2B2U, 0U, 36010, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB2CAU, 0xB2CDU, 0U, 36010, 14U, true},
};

static const struct yt_input_fault_site ab36_fault_sites[] = {
	{YT_INPUT_FAULT_MODULE_YT, 0xAB9BU, 0xAB9EU, 0xAB98U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABA1U, 0xABA4U, 0xAB98U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABAAU, 0xABADU, 0xABAAU, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABAFU, 0xABB2U, 0xABAFU, 36000, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABE1U, 0xABE4U, 0xABD9U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABE9U, 0xABECU, 0xABD9U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABEEU, 0xABF1U, 0xABD9U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABF3U, 0xABF6U, 0xABD9U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xABFCU, 0xABFFU, 0xABFCU, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC01U, 0xAC04U, 0xAC01U, 36000, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC1BU, 0xAC1EU, 0xAC15U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC31U, 0xAC34U, 0xAC24U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC3CU, 0xAC3FU, 0xAC3CU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC67U, 0xAC6AU, 0xAC61U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC67U, 0xAC6AU, 0xAC61U, 36000, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC73U, 0xAC76U, 0xAC70U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC89U, 0xAC8CU, 0xAC83U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAC95U, 0xAC98U, 0xAC95U, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xACA9U, 0xACACU, 0xACA3U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xACB2U, 0xACB5U, 0xACACU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xACCCU, 0xACCFU, 0xACCCU, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xACD7U, 0xACDAU, 0xACD4U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAD0CU, 0xAD0FU, 0xACFFU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAD44U, 0xAD47U, 0xAD38U, 36000, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAD4DU, 0xAD50U, 0xAD4AU, 36000, 14U, false},
	{YT_INPUT_FAULT_MODULE_YT, 0xAD63U, 0xAD66U, 0xAD4AU, 36000, 14U, false},
	{YT_INPUT_FAULT_MODULE_YT, 0xADAFU, 0xADB2U, 0xADA3U, 36000, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xADB8U, 0xADBBU, 0xADB2U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xADC6U, 0xADC9U, 0xADC0U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xADE7U, 0xADEAU, 0xADE1U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE04U, 0xAE07U, 0xADF7U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE1BU, 0xAE1EU, 0xAE15U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE24U, 0xAE27U, 0xAE1EU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE27U, 0xAE2AU, 0xAE27U, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE4AU, 0xAE4DU, 0xAE44U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE8CU, 0xAE8FU, 0xAE79U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAE94U, 0xAE97U, 0xAE79U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAEBCU, 0xAEBFU, 0xAE9CU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAEBFU, 0xAEC2U, 0xAE9CU, 36000, 6U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAEC8U, 0xAECBU, 0xAE9CU, 36000, 6U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF2BU, 0xAF2EU, 0xAF23U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF56U, 0xAF59U, 0xAF4BU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF6EU, 0xAF71U, 0xAF68U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF7DU, 0xAF80U, 0xAF7AU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF83U, 0xAF86U, 0xAF7AU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF8BU, 0xAF8EU, 0xAF7AU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAF94U, 0xAF97U, 0xAF94U, 36000, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAFE5U, 0xAFE8U, 0xAFD1U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xAFEBU, 0xAFEEU, 0xAFD1U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB006U, 0xB009U, 0xAFF3U, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB021U, 0xB024U, 0xB01EU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB041U, 0xB044U, 0xB03EU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0xB047U, 0xB04AU, 0xB03EU, 36000, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D45U, 0x1D48U, 0x1D42U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D8AU, 0x1D8DU, 0x1D8AU, 610, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1EC0U, 0x1EC3U, 0x1EBDU, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1F8EU, 0x1F91U, 0x1F85U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1F96U, 0x1F99U, 0x1F85U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1FB9U, 0x1FBCU, 0x1F9FU, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1FCFU, 0x1FD2U, 0x1FC2U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1FEDU, 0x1FF0U, 0x1FE5U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1FF9U, 0x1FFCU, 0x1FF3U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x2002U, 0x2005U, 0x1FF3U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1D99U, 0x1D9CU, 0x1D96U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DC2U, 0x1DC5U, 0x1DB4U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DE0U, 0x1DE3U, 0x1DD2U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1DEAU, 0x1DEDU, 0x1DD2U, 610, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x94FDU, 0x9500U, 0x94FAU, 64006, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x951AU, 0x951DU, 0x951AU, 64006, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x954CU, 0x954FU, 0x9546U, 64006, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x954CU, 0x954FU, 0x9546U, 64006, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0x0318U, 0x031BU, 0x0317U, 85, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0x0303U, 0x0306U, 0x02FDU, 80, 14U, true},
	{YT_INPUT_FAULT_MODULE_YT, 0x030FU, 0x0312U, 0x030FU, 80, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1B9CU, 0x1B9FU, 0x1B99U, 610, 7U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1BEAU, 0x1BEDU, 0x1BDEU, 610, 0U, true},
	{YT_INPUT_FAULT_MODULE_YT_SUB, 0x1BF9U, 0x1BFCU, 0x1BEDU, 610, 0U, true},
};

size_t
yt_input_fault_site_count(enum yt_input_fault_family family)
{
	switch (family) {
	case YT_INPUT_FAULT_B05D:
		return YT_ARRAY_LEN(b05d_fault_sites);
	case YT_INPUT_FAULT_B1F3:
		return YT_ARRAY_LEN(b1f3_fault_sites);
	case YT_INPUT_FAULT_AB36:
		return YT_ARRAY_LEN(ab36_fault_sites);
	default:
		return 0U;
	}
}

bool
yt_input_fault_site(enum yt_input_fault_family family, size_t index,
    struct yt_input_fault_site *site)
{
	const struct yt_input_fault_site *sites;
	size_t count;

	if (site == NULL)
		return false;
	switch (family) {
	case YT_INPUT_FAULT_B05D:
		sites = b05d_fault_sites;
		count = YT_ARRAY_LEN(b05d_fault_sites);
		break;
	case YT_INPUT_FAULT_B1F3:
		sites = b1f3_fault_sites;
		count = YT_ARRAY_LEN(b1f3_fault_sites);
		break;
	case YT_INPUT_FAULT_AB36:
		sites = ab36_fault_sites;
		count = YT_ARRAY_LEN(ab36_fault_sites);
		break;
	default:
		return false;
	}
	if (index >= count)
		return false;
	*site = sites[index];
	return true;
}

static bool
bounded_string_length(const char *text, size_t capacity, size_t *length)
{
	size_t index;

	if (text == NULL || capacity == 0U || length == NULL)
		return false;
	for (index = 0U; index < capacity; ++index) {
		if (text[index] == '\0') {
			*length = index;
			return true;
		}
	}
	return false;
}

static float
chat_single(float value)
{
	volatile float result = value;

	return result;
}

static float
chat_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

static float
chat_single_subtract(float left, float right)
{
	volatile float result = left - right;

	return result;
}

static bool
chat_append(struct yt_sysop_chat_output *output,
    enum yt_sysop_chat_destination destination, bool line,
    const uint8_t *data, size_t length)
{
	struct yt_sysop_chat_event *event;

	if (output->count >= YT_SYSOP_CHAT_EVENTS
	    || length > sizeof(output->events[0].data))
		return false;
	event = &output->events[output->count++];
	memset(event, 0, sizeof(*event));
	event->destination = destination;
	event->line = line;
	if (length != 0)
		memcpy(event->data, data, length);
	event->length = length;
	return true;
}

static bool
chat_key_accepted(const uint8_t *key, size_t length)
{
	if (length == 0)
		return false;
	if (key[0] < 0x7fU)
		return true;
	return key[0] == 0x7fU && length == 1U;
}

enum yt_radio_body_key_action
yt_input_radio_body_key(uint8_t key, size_t current_length)
{
	if (key == '\r')
		return YT_RADIO_BODY_KEY_COMMIT;
	if (key == '\b' || key == 0x7fU)
		return current_length != 0U
		    ? YT_RADIO_BODY_KEY_BACKSPACE : YT_RADIO_BODY_KEY_IGNORE;
	if (key >= 0x20U && key < 0x7fU)
		return YT_RADIO_BODY_KEY_PRINTABLE;
	return YT_RADIO_BODY_KEY_IGNORE;
}

bool
yt_input_ab36_remote_replace(float mode, const struct yt_input_value *remote,
    struct yt_input_value *selected)
{
	if (remote == NULL || selected == NULL || remote->length > 2U)
		return false;
	if (mode != 1.0f && remote->length != 0)
		*selected = *remote;
	return true;
}

bool
yt_input_ab36_queue_pop(char *queue, size_t capacity, size_t *position,
    size_t *length, struct yt_input_value *selected)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || selected == NULL || *position > *length
	    || *length >= capacity)
		return false;
	memset(selected, 0, sizeof(*selected));
	if (*position == *length)
		return true;
	selected->bytes[0] = (uint8_t)queue[(*position)++];
	selected->length = 1U;
	if (*position == *length) {
		*position = 0U;
		*length = 0U;
		queue[0] = '\0';
	}
	return true;
}

bool
yt_input_queue_clear(char *queue, size_t capacity, size_t *position,
    size_t *length)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || *position > *length || *length >= capacity)
		return false;
	queue[0] = '\0';
	*position = 0U;
	*length = 0U;
	return true;
}

bool
yt_input_queue_prepend_program(char *queue, size_t capacity,
    size_t *position, size_t *length, const char *program,
    size_t program_length)
{
	size_t pending;

	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || (program == NULL && program_length != 0U)
	    || *position > *length || *length >= capacity)
		return false;
	pending = *length - *position;
	if (program_length >= capacity
	    || program_length + 1U >= capacity - pending)
		return false;
	memmove(queue + program_length + 1U, queue + *position, pending);
	if (program_length != 0U)
		memcpy(queue, program, program_length);
	queue[program_length] = '\r';
	queue[program_length + 1U + pending] = '\0';
	*position = 0U;
	*length = program_length + 1U + pending;
	return true;
}

bool
yt_input_ab36_repeat_requested(bool queued,
    const struct yt_input_value *selected)
{
	return !queued && selected != NULL && selected->length == 1U
	    && selected->bytes[0] == 0x12U;
}

bool
yt_input_ab36_repeat_run(char *accumulator, size_t accumulator_capacity,
    const char *saved_command, size_t saved_capacity, char *paged_text,
    size_t paged_text_capacity, float *newline_flag, uint8_t *selected_key,
    yt_ab36_repeat_emit_fn emit, void *context)
{
	uint8_t prefix[YT_INPUT_PENDING];
	size_t prefix_length;
	size_t saved_length;

	if (newline_flag == NULL || selected_key == NULL || emit == NULL
	    || !bounded_string_length(accumulator, accumulator_capacity,
	    &prefix_length)
	    || !bounded_string_length(saved_command, saved_capacity,
	    &saved_length)
	    || prefix_length > sizeof(prefix)
	    || paged_text == NULL || prefix_length >= paged_text_capacity
	    || saved_length >= accumulator_capacity)
		return false;
	if (prefix_length != 0U)
		memcpy(prefix, accumulator, prefix_length);
	memcpy(paged_text, accumulator, prefix_length + 1U);
	*newline_flag = 1.0f;
	if (!emit(context, prefix, prefix_length))
		return false;
	memmove(accumulator, saved_command, saved_length + 1U);
	*selected_key = '\r';
	return true;
}

bool
yt_input_ab36_submit_requested(uint8_t selected_key)
{
	return selected_key == '\r';
}

bool
yt_input_ab36_submit_run(float *newline_flag, yt_ab36_submit_line_fn line,
    void *context)
{
	if (newline_flag == NULL || line == NULL)
		return false;
	*newline_flag = 0.0f;
	return line(context);
}

bool
yt_input_ab36_backspace_run(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, bool *handled,
    yt_ab36_echo_fn echo, void *context)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	size_t length;

	if (handled == NULL || echo == NULL
	    || !bounded_string_length(accumulator, accumulator_capacity,
	    &length))
		return false;
	*handled = false;
	if (selected_key != '\b' || length == 0U)
		return true;
	accumulator[length - 1U] = '\0';
	*handled = true;
	return echo(context, local_erase, sizeof(local_erase), remote_erase,
	    sizeof(remote_erase));
}

bool
yt_input_ab36_printable_run(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, size_t response_capacity,
    char *paged_text, size_t paged_text_capacity, float *newline_flag,
    bool *handled, yt_ab36_echo_fn echo, yt_ab36_carrier_fn carrier,
    void *context)
{
	size_t length;

	if (paged_text == NULL || paged_text_capacity < 2U
	    || newline_flag == NULL || handled == NULL || echo == NULL
	    || carrier == NULL
	    || !bounded_string_length(accumulator, accumulator_capacity,
	    &length))
		return false;
	*handled = false;
	if (selected_key < 0x20U || selected_key > 0x7fU
	    || length + 1U >= accumulator_capacity
	    || length + 1U >= response_capacity)
		return true;
	*handled = true;
	if (!echo(context, &selected_key, 1U, &selected_key, 1U))
		return false;
	accumulator[length] = (char)selected_key;
	accumulator[length + 1U] = '\0';
	paged_text[0] = (char)selected_key;
	paged_text[1] = '\0';
	*newline_flag = 1.0f;
	return carrier(context);
}

bool
yt_input_command_save_requested(const char *text, size_t capacity,
    bool *requested)
{
	size_t length;

	if (requested == NULL
	    || !bounded_string_length(text, capacity, &length))
		return false;
	*requested = length != 0U && text[length - 1U] == '/';
	return true;
}

static bool
command_save_fault_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK;
}

static bool
command_save_fail(struct yt_command_save_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_command_save_staged(char *text, size_t text_capacity,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity,
    enum yt_basic_fault_site target,
    struct yt_command_save_transform *result)
{
	static const char notice[] =
	    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	size_t length;
	size_t stripped_length;

	if (text == NULL || text_capacity == 0U || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL || *queue_position > *queue_length
	    || *queue_length >= queue_capacity || saved_command == NULL
	    || saved_capacity == 0U || output_source == NULL
	    || output_capacity == 0U || result == NULL
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !command_save_fault_target(target))
	    || !bounded_string_length(text, text_capacity, &length))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target != YT_BASIC_FAULT_SITE_COUNT) {
		if (length == 0U
		    || (target != YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE
		    && text[length - 1U] != '/'))
			return false;
		stripped_length = length - 1U;
		if ((target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE
		    || target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE)
		    && stripped_length == 0U)
			return false;
	}
	if (target == YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE)
		return command_save_fail(result, target);
	if (length == 0U || text[length - 1U] != '/')
		return target == YT_BASIC_FAULT_SITE_COUNT;
	result->save_requested = true;
	stripped_length = length - 1U;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE)
		return command_save_fail(result, target);
	text[stripped_length] = '\0';
	queue[0] = '\0';
	*queue_position = 0U;
	*queue_length = 0U;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE)
		return command_save_fail(result, target);
	if (stripped_length >= saved_capacity)
		return false;
	memcpy(saved_command, text, stripped_length + 1U);
	if (target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE)
		return command_save_fail(result, target);
	if (sizeof(notice) > output_capacity)
		return false;
	memcpy(output_source, notice, sizeof(notice));
	result->notice_ready = true;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK)
		return command_save_fail(result, target);
	return true;
}

bool
yt_input_ab36_inactivity_begin_process(float timer, uint8_t deadline[4])
{
	uint8_t raw[4];
	float rounded_timer;
	float value;

	if (deadline == NULL)
		return false;
	rounded_timer = chat_single(timer);
	value = chat_single_add(floorf(rounded_timer), 180.0f);
	if (qb_mbf32_encode(value, raw) == QB_MBF_OVERFLOW)
		return false;
	memcpy(deadline, raw, sizeof(raw));
	return true;
}

bool
yt_input_ab36_inactivity_expired(float timer, float deadline, float mode)
{
	return timer > deadline && mode != 1.0f;
}

bool
yt_input_ab36_session_expired(float timer, float deadline)
{
	return timer > deadline;
}

bool
yt_input_carrier_returns(float mode, bool carrier_detected)
{
	return mode != 0.0f || carrier_detected;
}

enum yt_opening_row_route
yt_input_opening_row_route(bool local_key, bool remote_pending)
{
	if (local_key)
		return YT_OPENING_ROW_STOP_LOCAL;
	if (remote_pending)
		return YT_OPENING_ROW_STOP_REMOTE;
	return YT_OPENING_ROW_CONTINUE;
}

bool
yt_input_ab36_terminal_run(enum yt_ab36_terminal_kind kind,
    bool *running, bool *terminated, yt_ab36_terminal_notice_fn notice,
    yt_ab36_terminal_close_fn close_all, void *context)
{
	static const uint8_t inactivity[] = "\aUSER FELL ASLEEP!";
	static const uint8_t session_limit[] =
	    "\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	const uint8_t *text;
	size_t length;

	if (running == NULL || terminated == NULL || notice == NULL
	    || close_all == NULL)
		return false;
	switch (kind) {
	case YT_AB36_TERMINAL_INACTIVITY:
		text = inactivity;
		length = sizeof(inactivity) - 1U;
		break;
	case YT_AB36_TERMINAL_SESSION_LIMIT:
		text = session_limit;
		length = sizeof(session_limit) - 1U;
		break;
	default:
		return false;
	}
	if (!notice(context, text, length) || !close_all(context))
		return false;
	*running = false;
	*terminated = true;
	return true;
}

bool
yt_b05d_process_key(const struct yt_input_value *value,
    struct yt_b05d_key_state *state)
{
	size_t position = *state->queue_position;
	size_t length = *state->queue_length;
	size_t queued = length - position;
	uint8_t key;

	if (value->length != 1)
		return true;
	key = value->bytes[0];
	if (key == 0x18) {
		state->accumulator[0] = '\0';
		state->queue[0] = '\0';
		*state->queue_position = 0;
		*state->queue_length = 0;
		if (state->pager_key_capacity < 2U)
			return false;
		state->pager_key[0] = 'Q';
		state->pager_key[1] = '\0';
		return true;
	}
	if (key == 0x12 && queued != 0)
		return true;
	if (key >= 0x7f || (key < 0x20 && key != '\r'))
		return true;
	if (queued + 1U >= state->queue_capacity)
		return false;
	if (queued != 0 && position != 0)
		memmove(state->queue, state->queue + position, queued);
	state->queue[queued] = (char)key;
	state->queue[queued + 1U] = '\0';
	*state->queue_position = 0;
	*state->queue_length = queued + 1U;
	return true;
}

bool
yt_input_expand_repeat(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    struct yt_repeat_transform *result)
{
	char output_source[128];

	output_source[0] = '\0';
	return yt_input_expand_repeat_observed(text, text_capacity,
	    saved_command, saved_capacity, output_source, sizeof(output_source),
	    result, NULL, NULL);
}

static bool
input_process_store_single(yt_input_process_store_fn store, void *context,
    uint16_t address, float value)
{
	uint8_t raw[4];

	if (store == NULL)
		return true;
	if (qb_mbf32_encode(value, raw) != QB_MBF_OK)
		return false;
	store(context, address, raw);
	return true;
}

static bool
input_process_store_double(yt_input_process_store_fn store, void *context,
    uint16_t address, double value)
{
	uint8_t raw[8];

	if (qb_mbf64_encode(value, raw) != QB_MBF_OK)
		return false;
	if (store != NULL) {
		store(context, address, raw);
		store(context, (uint16_t)(address + 4U), raw + 4U);
	}
	return true;
}

static bool
upper_fault_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_UPPER_FRAME_STACK
	    || target == YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE
	    || target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
	    || target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE;
}

static bool
upper_fail(struct yt_upper_transform *result,
    enum yt_basic_fault_site site, size_t length, size_t index,
    bool initialized, bool extracted, uint8_t byte, bool mapped,
    uint8_t code)
{
	result->length = length;
	result->index = index;
	result->extracted = byte;
	result->mapped = code;
	result->fault_site = site;
	result->scratch_initialized = initialized;
	result->extracted_valid = extracted;
	result->mapped_valid = mapped;
	result->fault_valid = true;
	return false;
}

bool
yt_input_compat_upper_n_staged(uint8_t *text, size_t length,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_upper_transform *result, yt_input_process_store_fn store,
    void *context)
{
	size_t index;

	if (text == NULL || result == NULL || occurrence == 0U
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !upper_fault_target(target)))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target == YT_BASIC_FAULT_UPPER_FRAME_STACK) {
		if (occurrence != 1U)
			return false;
		return upper_fail(result, target, 0U, 0U, false, false, 0U,
		    false, 0U);
	}
	if (target != YT_BASIC_FAULT_SITE_COUNT) {
		if (occurrence > length)
			return false;
		if ((target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
		    || target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE)
		    && text[occurrence - 1U] <= (uint8_t)'@')
			return false;
	}
	result->length = length;
	result->index = 1U;
	result->scratch_initialized = true;
	if (!input_process_store_single(store, context, 0x001AU,
	    (float)length)
	    || !input_process_store_single(store, context, 0x536AU,
	    (float)length)
	    || !input_process_store_single(store, context, 0x536EU, 1.0f))
		return false;
	for (index = 0U; index < length; ++index) {
		uint8_t current;
		uint8_t mapped;

		result->index = index + 1U;
		if (target == YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE
		    && occurrence == index + 1U)
			return upper_fail(result, target, length, index + 1U, true,
			    false, 0U, false, 0U);
		current = text[index];
		if (current > (uint8_t)'@') {
			if (target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
			    && occurrence == index + 1U)
				return upper_fail(result, target, length, index + 1U,
				    true, false, 0U, false, 0U);
			mapped = current & 0xdfU;
			if (target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE
			    && occurrence == index + 1U)
				return upper_fail(result, target, length, index + 1U,
				    true, true, current, true, mapped);
			text[index] = mapped;
		}
		result->index = index + 2U;
		if (!input_process_store_single(store, context, 0x001AU,
		    (float)(index + 2U))
		    || !input_process_store_single(store, context, 0x536EU,
		    (float)(index + 2U)))
			return false;
	}
	return true;
}

void
yt_input_compat_upper_n_observed(uint8_t *text, size_t length,
    yt_input_process_store_fn store, void *context)
{
	struct yt_upper_transform result;

	(void)yt_input_compat_upper_n_staged(text, length,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result, store, context);
}

bool
yt_input_repeat_prefix_staged(const uint8_t *text, size_t length,
    uint8_t *upper_scratch, size_t scratch_capacity,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_repeat_prefix_transform *result,
    yt_input_process_store_fn store, void *context)
{
	struct yt_upper_transform upper;
	size_t index;
	size_t repeat_position = 0U;

	if (text == NULL || upper_scratch == NULL || scratch_capacity == 0U
	    || result == NULL || occurrence == 0U || length >= scratch_capacity
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE
	    && !upper_fault_target(target)))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	result->upper.fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target == YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE) {
		if (occurrence != 1U || length == 0U)
			return false;
		result->fault_site = target;
		result->fault_valid = true;
		return false;
	}
	memcpy(upper_scratch, text, length);
	upper_scratch[length] = '\0';
	if (!yt_input_compat_upper_n_staged(upper_scratch, length,
	    upper_fault_target(target) ? target : YT_BASIC_FAULT_SITE_COUNT,
	    occurrence, &upper, store, context)) {
		result->upper = upper;
		if (upper.fault_valid) {
			result->fault_site = upper.fault_site;
			result->fault_valid = true;
		}
		return false;
	}
	result->upper = upper;
	for (index = 0U; index + 1U < length; ++index) {
		if (upper_scratch[index] == (uint8_t)'/'
		    && upper_scratch[index + 1U] == (uint8_t)'R') {
			repeat_position = index + 1U;
			break;
		}
	}
	if (!input_process_store_single(store, context, 0x51C4U,
	    (float)repeat_position)
	    || qb_mbf32_encode((float)repeat_position, result->work_raw)
	    != QB_MBF_OK)
		return false;
	result->repeat_position = repeat_position;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static bool
repeat_parse_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE
	    || target == YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW
	    || target == YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW;
}

static bool
repeat_parse_fault(struct yt_repeat_parse_transform *result,
    enum yt_basic_fault_site site, enum yt_repeat_failure failure)
{
	result->fault_site = site;
	result->failure = failure;
	result->fault_valid = true;
	return false;
}

bool
yt_input_repeat_parse_staged(char *text, size_t text_capacity,
    uint8_t *upper_scratch, size_t scratch_capacity,
    size_t repeat_position, enum yt_basic_fault_site target,
    struct yt_repeat_parse_transform *result,
    yt_input_process_store_fn store, void *context)
{
	struct qb_val_result parsed;
	double integer;
	size_t text_length;
	size_t upper_length;
	size_t prefix_length;
	size_t suffix_offset;
	size_t suffix_length;
	float count;

	if (text == NULL || text_capacity == 0U || upper_scratch == NULL
	    || scratch_capacity == 0U || result == NULL
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !repeat_parse_target(target))
	    || !bounded_string_length(text, text_capacity, &text_length)
	    || !bounded_string_length((const char *)upper_scratch,
	    scratch_capacity, &upper_length))
		return false;
	memset(result, 0, sizeof(*result));
	result->failure = YT_REPEAT_FAILURE_NONE;
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (repeat_position == 0U)
		return target == YT_BASIC_FAULT_SITE_COUNT;
	if (repeat_position >= upper_length
	    || upper_scratch[repeat_position - 1U] != (uint8_t)'/'
	    || upper_scratch[repeat_position] != (uint8_t)'R')
		return false;
	result->repeat_reached = true;
	prefix_length = repeat_position - 1U;
	suffix_offset = repeat_position + 1U;
	suffix_length = upper_length - suffix_offset;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE) {
		if (prefix_length == 0U)
			return false;
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	}
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE
	    && suffix_length == 0U)
		return false;
	if (prefix_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, text, prefix_length);
	result->pending_string[prefix_length] = '\0';
	result->pending_length = prefix_length;
	result->pending_role = YT_REPEAT_PENDING_PREFIX;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE)
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	if (prefix_length + 2U > text_capacity)
		return false;
	memcpy(text, result->pending_string, prefix_length);
	text[prefix_length] = ';';
	text[prefix_length + 1U] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE) {
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	}
	if (suffix_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, upper_scratch + suffix_offset,
	    suffix_length);
	result->pending_string[suffix_length] = '\0';
	result->pending_length = suffix_length;
	result->pending_role = YT_REPEAT_PENDING_SUFFIX;
	parsed = qb_val(result->pending_string);
	if (parsed.overflow) {
		if (target != YT_BASIC_FAULT_SITE_COUNT
		    && target != YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW)
			return false;
		return repeat_parse_fault(result,
		    YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW,
		    YT_REPEAT_FAILURE_VAL_OVERFLOW);
	}
	integer = qb_int(parsed.valid ? parsed.value : 0.0);
	if (!input_process_store_double(store, context, 0x0016U, integer)
	    || qb_mbf64_encode(integer, result->pending_double_raw) != QB_MBF_OK)
		return false;
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_INTEGER;
	result->pending_double_valid = true;
	count = (float)integer;
	if (qb_mbf32_encode(count, result->count_raw) == QB_MBF_OVERFLOW) {
		if (target != YT_BASIC_FAULT_SITE_COUNT
		    && target != YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW)
			return false;
		return repeat_parse_fault(result,
		    YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW,
		    YT_REPEAT_FAILURE_SINGLE_OVERFLOW);
	}
	if (!input_process_store_single(store, context, 0x001AU, count)
	    || !input_process_store_single(store, context, 0x51C4U, count))
		return false;
	upper_scratch[0] = '\0';
	result->pending_role = YT_REPEAT_PENDING_NONE;
	result->pending_double_valid = false;
	if (count > 10.0f) {
		count = 20.0f;
		if (!input_process_store_single(store, context, 0x51C4U, count)
		    || qb_mbf32_encode(count, result->count_raw) != QB_MBF_OK)
			return false;
	}
	result->count = count;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static bool
repeat_build_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK;
}

static bool
repeat_build_fault(struct yt_repeat_build_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_repeat_build_staged(char *text, size_t text_capacity,
    uint8_t *build_scratch, size_t scratch_capacity,
    char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity, float count,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_repeat_build_transform *result,
    yt_input_process_store_fn store, void *context)
{
	static const char notice_prefix[] = "Command Repeated";
	static const char notice_suffix[] =
	    " times -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	char count_text[64];
	size_t text_length;
	size_t scratch_length;
	size_t used = 0U;
	size_t expanded_length;
	size_t count_length;
	size_t prefix_length;
	size_t notice_length;
	int copies;
	int index;

	if (text == NULL || text_capacity == 0U || build_scratch == NULL
	    || scratch_capacity == 0U || saved_command == NULL
	    || saved_capacity == 0U || output_source == NULL
	    || output_capacity == 0U || result == NULL || occurrence == 0U
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !repeat_build_target(target))
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
	    && occurrence != 1U)
	    || !bounded_string_length(text, text_capacity, &text_length)
	    || !bounded_string_length((const char *)build_scratch,
	    scratch_capacity, &scratch_length)
	    || scratch_length != 0U || text_length == 0U
	    || text[text_length - 1U] != ';' || !isfinite(count)
	    || count <= 0.0f || floorf(count) != count
	    || (count > 10.0f && count != 20.0f))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	copies = (int)count;
	if (!input_process_store_single(store, context, 0x51C8U, count)
	    || !input_process_store_single(store, context, 0x4F76U, 1.0f))
		return false;
	for (index = 0; index < copies; ++index) {
		if (used > 500U)
			break;
		if (target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
		    && occurrence == (size_t)index + 1U)
			return repeat_build_fault(result, target);
		if (used + text_length >= scratch_capacity)
			return false;
		memcpy(build_scratch + used, text, text_length);
		used += text_length;
		build_scratch[used] = '\0';
		result->completed_iterations = (size_t)index + 1U;
		if (!input_process_store_single(store, context, 0x4F76U,
		    (float)(index + 2)))
			return false;
	}
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE)
		return false;
	if (used == 0U)
		return false;
	expanded_length = used - 1U;
	if (expanded_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, build_scratch, expanded_length);
	result->pending_string[expanded_length] = '\0';
	result->pending_length = expanded_length;
	result->pending_role = YT_REPEAT_PENDING_EXPANDED;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE) {
		if (expanded_length == 0U)
			return false;
		return repeat_build_fault(result, target);
	}
	if (expanded_length >= text_capacity)
		return false;
	memcpy(text, result->pending_string, expanded_length + 1U);
	build_scratch[0] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE) {
		if (expanded_length == 0U)
			return false;
		return repeat_build_fault(result, target);
	}
	if (expanded_length >= saved_capacity)
		return false;
	memcpy(saved_command, text, expanded_length + 1U);
	result->bold_committed = true;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE)
		return repeat_build_fault(result, target);
	if (qb_str_double(count_text, sizeof(count_text), (double)count) < 0)
		return false;
	count_length = strlen(count_text);
	if (count_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, count_text, count_length + 1U);
	result->pending_length = count_length;
	result->pending_role = YT_REPEAT_PENDING_COUNT_TEXT;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE)
		return repeat_build_fault(result, target);
	prefix_length = sizeof(notice_prefix) - 1U + count_length;
	if (prefix_length >= sizeof(result->pending_string))
		return false;
	memmove(result->pending_string + sizeof(notice_prefix) - 1U,
	    result->pending_string, count_length + 1U);
	memcpy(result->pending_string, notice_prefix,
	    sizeof(notice_prefix) - 1U);
	result->pending_length = prefix_length;
	result->pending_role = YT_REPEAT_PENDING_NOTICE_PREFIX;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE)
		return repeat_build_fault(result, target);
	notice_length = prefix_length + sizeof(notice_suffix) - 1U;
	if (notice_length >= output_capacity)
		return false;
	memcpy(output_source, result->pending_string, prefix_length);
	memcpy(output_source + prefix_length, notice_suffix,
	    sizeof(notice_suffix));
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	result->notice_ready = true;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK)
		return repeat_build_fault(result, target);
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

bool
yt_input_expand_repeat_observed(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity,
    struct yt_repeat_transform *result, yt_input_process_store_fn store,
    void *context)
{
	uint8_t upper[YT_INPUT_PENDING];
	struct yt_repeat_prefix_transform repeat_prefix;
	struct yt_repeat_parse_transform repeat_parse;
	struct yt_repeat_build_transform repeat_build;
	size_t text_length;
	float count;

	if (text == NULL || saved_command == NULL || output_source == NULL
	    || result == NULL || text_capacity == 0 || saved_capacity == 0
	    || output_capacity == 0)
		return false;
	result->emit_notice = false;
	result->bold_committed = false;
	result->count = 0.0f;
	result->failure = YT_REPEAT_FAILURE_NONE;
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	result->fault_valid = false;
	text_length = strlen(text);
	if (text_length >= sizeof(upper))
		return false;
	if (!yt_input_repeat_prefix_staged((const uint8_t *)text, text_length,
	    upper, sizeof(upper), YT_BASIC_FAULT_SITE_COUNT, 1U,
	    &repeat_prefix, store, context))
		return false;
	if (repeat_prefix.repeat_position == 0U)
		return true;
	if (!yt_input_repeat_parse_staged(text, text_capacity, upper,
	    sizeof(upper), repeat_prefix.repeat_position,
	    YT_BASIC_FAULT_SITE_COUNT, &repeat_parse, store, context)) {
		result->failure = repeat_parse.failure;
		result->fault_site = repeat_parse.fault_site;
		result->fault_valid = repeat_parse.fault_valid;
		return false;
	}
	count = repeat_parse.count;
	result->count = count;
	if (count <= 0.0f)
		return true;
	if (!yt_input_repeat_build_staged(text, text_capacity, upper,
	    sizeof(upper), saved_command, saved_capacity, output_source,
	    output_capacity, count, YT_BASIC_FAULT_SITE_COUNT, 1U,
	    &repeat_build, store, context)) {
		result->fault_site = repeat_build.fault_site;
		result->fault_valid = repeat_build.fault_valid;
		return false;
	}
	result->bold_committed = repeat_build.bold_committed;
	result->emit_notice = repeat_build.notice_ready;
	return true;
}

bool
yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length)
{
	return yt_input_split_semicolon_observed(text, queue, queue_capacity,
	    queue_position, queue_length, NULL, NULL);
}

bool
yt_input_split_semicolon_observed(char *text, char *queue,
    size_t queue_capacity, size_t *queue_position, size_t *queue_length,
    yt_input_process_store_fn store, void *context)
{
	struct yt_semicolon_transform result;
	size_t text_length;

	if (text == NULL)
		return false;
	text_length = strlen(text);
	return yt_input_split_semicolon_staged(text, text_length + 1U,
	    queue, queue_capacity, queue_position, queue_length,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result, store, context);
}

static bool
semicolon_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE;
}

static bool
semicolon_fault(struct yt_semicolon_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_split_semicolon_staged(char *text, size_t text_capacity,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, enum yt_basic_fault_site target,
    size_t occurrence, struct yt_semicolon_transform *result,
    yt_input_process_store_fn store, void *context)
{
	char old_queue[YT_INPUT_PENDING];
	char *semicolon;
	char *replacement;
	size_t text_length;
	size_t tail_length;
	size_t old_length;
	size_t combined_length;
	size_t prefix_length;

	if (text == NULL || text_capacity == 0U || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL || result == NULL || occurrence == 0U
	    || *queue_length < *queue_position
	    || *queue_length >= queue_capacity
	    || (target != YT_BASIC_FAULT_SITE_COUNT && !semicolon_target(target))
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
	    && occurrence != 1U)
	    || !bounded_string_length(text, text_capacity, &text_length))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	semicolon = memchr(text, ';', text_length);
	result->semicolon_position = semicolon == NULL ? 0U
	    : (size_t)(semicolon - text) + 1U;
	if (!input_process_store_single(store, context, 0x51C4U,
	    (float)result->semicolon_position))
		return false;
	if (semicolon == NULL)
		return target == YT_BASIC_FAULT_SITE_COUNT;
	tail_length = text_length - result->semicolon_position;
	old_length = *queue_length - *queue_position;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE) {
		if (tail_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	if (tail_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, semicolon + 1U, tail_length);
	result->pending_string[tail_length] = '\0';
	result->pending_length = tail_length;
	result->pending_role = YT_SEMICOLON_PENDING_TAIL;
	combined_length = tail_length + old_length;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE) {
		if (combined_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	if (old_length > sizeof(old_queue) || combined_length >= queue_capacity)
		return false;
	if (old_length != 0U)
		memcpy(old_queue, queue + *queue_position, old_length);
	if (tail_length != 0U)
		memcpy(queue, result->pending_string, tail_length);
	if (old_length != 0U)
		memcpy(queue + tail_length, old_queue, old_length);
	queue[combined_length] = '\0';
	*queue_position = 0U;
	*queue_length = combined_length;
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_SEMICOLON_PENDING_NONE;
	prefix_length = result->semicolon_position - 1U;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE) {
		if (prefix_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	text[prefix_length] = '\0';
	for (;;) {
		replacement = memchr(queue, ';', *queue_length);
		if (replacement == NULL)
			break;
		if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
		    && occurrence == result->replacements + 1U)
			return semicolon_fault(result, target);
		*replacement = '\r';
		++result->replacements;
	}
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE)
		return false;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE)
		return semicolon_fault(result, target);
	result->pending_string[0] = '\r';
	result->pending_string[1] = '\0';
	result->pending_length = 1U;
	result->pending_role = YT_SEMICOLON_PENDING_FINAL_CR;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE)
		return semicolon_fault(result, target);
	if (*queue_length + 1U >= queue_capacity)
		return false;
	queue[*queue_length] = '\r';
	++*queue_length;
	queue[*queue_length] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_SEMICOLON_PENDING_NONE;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static const struct yt_a8d2_fault_identity a8d2_faults[] = {
	{"none", 0U, 0U, 0U, 0, 0U, {0U}, 0U},
	{"LEFT$ first byte", 0xA8EFU, 0xA8F2U, 0xA8E7U, 40001,
	    0x4C9AU, {14U, 16U, 0x0AC9U}, 3U},
	{"first-byte COPY", 0xA8F4U, 0xA8F7U, 0xA8E7U, 40001,
	    0x4C9AU, {0x0ACCU}, 1U},
	{"invalid queue clear", 0xA938U, 0xA93BU, 0xA932U, 40001,
	    0x4BE0U, {0x0ACCU}, 1U},
	{"prompt clear", 0xA943U, 0xA946U, 0xA93DU, 40001,
	    0x4D3AU, {0x0ACCU}, 1U},
};

_Static_assert(YT_ARRAY_LEN(a8d2_faults) == YT_A8D2_FAULT_SITE_COUNT,
    "A8D2 fault identity table is incomplete");

const struct yt_a8d2_fault_identity *
yt_input_a8d2_fault_identity(enum yt_a8d2_fault_site site)
{
	if (site <= YT_A8D2_FAULT_NONE
	    || (unsigned)site >= YT_ARRAY_LEN(a8d2_faults))
		return NULL;
	return &a8d2_faults[(size_t)site];
}

static bool
a8d2_fault_admits(enum yt_a8d2_fault_site site, uint16_t error_number)
{
	const struct yt_a8d2_fault_identity *identity =
	    yt_input_a8d2_fault_identity(site);
	size_t index;

	if (identity == NULL)
		return false;
	for (index = 0U; index < identity->error_count; ++index)
		if (identity->errors[index] == error_number)
			return true;
	return false;
}

static bool
a8d2_fail(struct yt_a8d2_transform *result,
	    enum yt_a8d2_fault_site site, uint16_t error_number)
{
	result->fault_site = site;
	result->error_number = error_number;
	result->outcome = error_number == 14U || error_number == 16U
	    ? YT_A8D2_BASIC_ERROR : YT_A8D2_INTERNAL_FATAL;
	return true;
}

bool
yt_input_a8d2_staged(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    uint8_t *prompt, size_t prompt_capacity, size_t *prompt_length,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, float *bold,
    enum yt_a8d2_fault_site target, uint16_t error_number,
    struct yt_a8d2_transform *result)
{
	size_t length;
	char first;

	if (command_accumulator == NULL || output_source == NULL
	    || output_source_capacity < 2U || prompt == NULL
	    || prompt_capacity == 0U || prompt_length == NULL
	    || *prompt_length > prompt_capacity
	    || queue == NULL || queue_capacity == 0U
	    || queue_position == NULL || queue_length == NULL || bold == NULL
	    || result == NULL || *queue_position > *queue_length
	    || *queue_length >= queue_capacity
	    || (target != YT_A8D2_FAULT_NONE
	    && !a8d2_fault_admits(target, error_number))
	    || (target == YT_A8D2_FAULT_NONE && error_number != 0U))
		return false;
	length = strlen(command_accumulator);
	if (length >= output_source_capacity)
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_A8D2_FAULT_NONE;
	memcpy(output_source, command_accumulator, length + 1U);
	qb_compat_upper(output_source);
	result->uppercase_complete = true;
	if (target == YT_A8D2_FAULT_LEFT_ONE) {
		if (length == 0U)
			return false;
		return a8d2_fail(result, target, error_number);
	}
	first = output_source[0];
	result->left_complete = true;
	if (target == YT_A8D2_FAULT_FIRST_COPY)
		return a8d2_fail(result, target, error_number);
	if (first != '\0')
		output_source[1] = '\0';
	result->first_copy_complete = true;
	if (first == '\0')
		result->answer = YT_YES_NO_EMPTY;
	else if (first == 'Y')
		result->answer = YT_YES_NO_YES;
	else if (first == 'N')
		result->answer = YT_YES_NO_NO;
	else
		result->answer = YT_YES_NO_INVALID;
	result->answer_valid = true;
	if (result->answer == YT_YES_NO_INVALID) {
		*bold = 1.0f;
		result->bold_committed = true;
		if (target == YT_A8D2_FAULT_INVALID_QUEUE_CLEAR)
			return a8d2_fail(result, target, error_number);
		if (target == YT_A8D2_FAULT_PROMPT_CLEAR)
			return false;
		if (!yt_input_queue_clear(queue, queue_capacity,
		    queue_position, queue_length))
			return false;
		result->queue_cleared = true;
		result->outcome = YT_A8D2_RETRY;
		return true;
	}
	if (target == YT_A8D2_FAULT_INVALID_QUEUE_CLEAR)
		return false;
	if (target == YT_A8D2_FAULT_PROMPT_CLEAR)
		return a8d2_fail(result, target, error_number);
	*prompt_length = 0U;
	prompt[0] = 0U;
	result->prompt_cleared = true;
	result->outcome = YT_A8D2_RETURNED;
	return true;
}

bool
yt_input_a8d2_internal_fatal_run(
    const struct yt_a8d2_transform *transform, uint16_t module_segment,
    bool redirected_stdin, bool function_bar, bool cursor_shape_known,
    uint16_t process_entry_cursor_shape,
    const struct yt_brun_internal_fatal_ops *ops, void *context,
    struct yt_brun_internal_fatal_state *state)
{
	const struct yt_a8d2_fault_identity *identity;
	enum yt_brun_internal_fatal_entry entry;

	if (transform == NULL
	    || transform->outcome != YT_A8D2_INTERNAL_FATAL)
		return false;
	identity = yt_input_a8d2_fault_identity(transform->fault_site);
	if (identity == NULL
	    || (transform->error_number != YT_BRUN_INTERNAL_FATAL_GC
	    && transform->error_number != YT_BRUN_INTERNAL_FATAL_OWNER))
		return false;
	entry = (enum yt_brun_internal_fatal_entry)transform->error_number;
	return yt_brun_internal_fatal_run(entry, "YT      ", true,
	    identity->source_line, module_segment, identity->saved_ip,
	    redirected_stdin, function_bar, cursor_shape_known,
	    process_entry_cursor_shape, ops, context, state);
}

bool
yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer)
{
	struct yt_a8d2_transform result;
	uint8_t prompt[1] = {0U};
	size_t prompt_length = 0U;
	char queue[1] = "";
	size_t queue_position = 0U;
	size_t queue_length = 0U;
	float bold = 0.0f;

	if (answer == NULL || !yt_input_a8d2_staged(command_accumulator,
	    output_source, output_source_capacity, prompt, sizeof(prompt),
	    &prompt_length, queue, sizeof(queue), &queue_position, &queue_length,
	    &bold,
	    YT_A8D2_FAULT_NONE, 0U, &result) || !result.answer_valid)
		return false;
	*answer = result.answer;
	return true;
}

void
yt_input_numeric_response(char *text)
{
	qb_compat_upper(text);
	if (strchr(text, 'E') != NULL)
		text[0] = '\0';
}

static float
wait_single(float value)
{
	volatile float result = value;

	return result;
}

static float
wait_single_add(float left, float right)
{
	volatile float result = left + right;

	return result;
}

bool
yt_timed_wait_begin(struct yt_timed_wait_state *state, float duration,
    float initial_timer)
{
	float rounded_duration;
	float rounded_timer;

	if (state == NULL
	    || state->serial_scratch_length > sizeof(state->serial_scratch))
		return false;
	rounded_duration = wait_single(duration);
	rounded_timer = wait_single(initial_timer);
	state->duration_cell = wait_single_add(rounded_timer,
	    rounded_duration);
	state->timer_reads = 1;
	state->local_reads = 0;
	state->loc_reads = 0;
	state->serial_reads = 0;
	return true;
}

enum yt_timed_wait_reason
yt_timed_wait_timer(struct yt_timed_wait_state *state, float current_timer)
{
	float current;

	if (state == NULL)
		return YT_TIMED_WAIT_ERROR;
	current = wait_single(current_timer);
	++state->timer_reads;
	return current >= state->duration_cell
	    ? YT_TIMED_WAIT_TIMER : YT_TIMED_WAIT_CONTINUE;
}

enum yt_timed_wait_reason
yt_timed_wait_input(struct yt_timed_wait_state *state, float mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U)
		return YT_TIMED_WAIT_ERROR;
	++state->local_reads;
	if (!selected->remote && selected->length != 0)
		return YT_TIMED_WAIT_LOCAL;
	if (mode != 0.0f)
		return YT_TIMED_WAIT_CONTINUE;
	++state->loc_reads;
	if (!selected->remote)
		return selected->length == 0
		    ? YT_TIMED_WAIT_CONTINUE : YT_TIMED_WAIT_ERROR;
	if (selected->length != 1U)
		return YT_TIMED_WAIT_ERROR;
	state->serial_scratch[0] = selected->bytes[0];
	state->serial_scratch_length = 1;
	++state->serial_reads;
	return YT_TIMED_WAIT_SERIAL;
}

bool
yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue)
{
	if (state == NULL || initial_residue == NULL
	    || initial_residue->length > sizeof(state->residue))
		return false;
	memset(state, 0, sizeof(*state));
	if (initial_residue->length != 0)
		memcpy(state->residue, initial_residue->bytes,
		    initial_residue->length);
	state->residue_length = initial_residue->length;
	return true;
}

enum yt_input_drain_reason
yt_input_drain_local(struct yt_input_drain_state *state,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || (selected->remote && selected->length != 0))
		return YT_INPUT_DRAIN_ERROR;
	++state->local_reads;
	if (state->expect_paired_local) {
		if (selected->length != 0)
			memcpy(state->residue, selected->bytes,
			    selected->length);
		state->residue_length = selected->length;
		state->expect_paired_local = false;
		return YT_INPUT_DRAIN_CONTINUE;
	}
	if (selected->length == 0)
		return YT_INPUT_DRAIN_LOCAL_COMPLETE;
	state->expect_paired_local = true;
	return YT_INPUT_DRAIN_CONTINUE;
}

enum yt_input_drain_reason
yt_input_drain_serial(struct yt_input_drain_state *state, float mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || state->expect_paired_local)
		return YT_INPUT_DRAIN_ERROR;
	if (mode != 0.0f)
		return YT_INPUT_DRAIN_COMPLETE;
	++state->loc_reads;
	if (selected->length == 0)
		return YT_INPUT_DRAIN_COMPLETE;
	if (!selected->remote || selected->length != 1U)
		return YT_INPUT_DRAIN_ERROR;
	state->residue[0] = selected->bytes[0];
	state->residue_length = 1;
	++state->serial_reads;
	return YT_INPUT_DRAIN_CONTINUE;
}

bool
yt_sysop_chat_begin(struct yt_sysop_chat_state *state, float mode,
    float snoop, float deadline, float entry_timer,
    float inactivity_deadline, const uint8_t *command_accumulator,
    size_t command_accumulator_length, const uint8_t *queue,
    size_t queue_length)
{
	float rounded_deadline;
	float rounded_timer;

	if (state == NULL || command_accumulator_length > YT_INPUT_PENDING
	    || queue_length > YT_INPUT_PENDING
	    || (command_accumulator == NULL
	    && command_accumulator_length != 0U)
	    || (queue == NULL && queue_length != 0U))
		return false;
	memset(state, 0, sizeof(*state));
	state->mode = mode;
	state->snoop = snoop;
	state->foreground = 2.0f;
	state->bold = 1.0f;
	rounded_deadline = chat_single(deadline);
	rounded_timer = chat_single(entry_timer);
	state->deadline = rounded_deadline;
	state->inactivity_deadline = chat_single(inactivity_deadline);
	state->saved_remaining = chat_single_subtract(rounded_deadline,
	    chat_single(floorf(rounded_timer)));
	state->newline_flag = 0.0f;
	if (command_accumulator_length != 0U)
		memcpy(state->command_accumulator, command_accumulator,
		    command_accumulator_length);
	state->command_accumulator_length = command_accumulator_length;
	if (queue_length != 0U)
		memcpy(state->queue, queue, queue_length);
	state->queue_length = queue_length;
	state->timer_reads = 1U;
	return true;
}

static bool
chat_process_bound(const struct yt_sysop_chat_state *state)
{
	return state->process.mode != NULL && state->process.snoop != NULL
	    && state->process.foreground != NULL
	    && state->process.deadline != NULL
	    && state->process.inactivity_deadline != NULL
	    && state->process.saved_remaining != NULL
	    && state->process.newline_flag != NULL;
}

static bool
chat_process_store(float *value, uint8_t cell[4], float next)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(next, raw) != QB_MBF_OK)
		return false;
	memcpy(cell, raw, sizeof(raw));
	*value = qb_mbf32_decode(raw);
	return true;
}

void
yt_sysop_chat_sync_process(struct yt_sysop_chat_state *state)
{
	if (state == NULL || !chat_process_bound(state))
		return;
	state->mode = qb_mbf32_decode(state->process.mode);
	state->snoop = qb_mbf32_decode(state->process.snoop);
	state->foreground = qb_mbf32_decode(state->process.foreground);
	state->deadline = qb_mbf32_decode(state->process.deadline);
	state->inactivity_deadline = qb_mbf32_decode(
	    state->process.inactivity_deadline);
	state->saved_remaining = qb_mbf32_decode(
	    state->process.saved_remaining);
	state->newline_flag = qb_mbf32_decode(state->process.newline_flag);
}

bool
yt_sysop_chat_begin_process(struct yt_sysop_chat_state *state,
    const struct yt_sysop_chat_process_cells *process, float entry_timer,
    const uint8_t *command_accumulator, size_t command_accumulator_length,
    const uint8_t *queue, size_t queue_length)
{
	static const uint8_t zero[4] = {0U, 0U, 0U, 0U};
	struct yt_sysop_chat_process_cells bound;

	if (state == NULL || process == NULL || process->mode == NULL
	    || process->snoop == NULL || process->foreground == NULL
	    || process->deadline == NULL
	    || process->inactivity_deadline == NULL
	    || process->saved_remaining == NULL
	    || process->newline_flag == NULL)
		return false;
	bound = *process;
	if (!yt_sysop_chat_begin(state, qb_mbf32_decode(bound.mode),
	    qb_mbf32_decode(bound.snoop), qb_mbf32_decode(bound.deadline),
	    entry_timer, qb_mbf32_decode(bound.inactivity_deadline),
	    command_accumulator, command_accumulator_length, queue,
	    queue_length))
		return false;
	state->process = bound;
	if (!chat_process_store(&state->saved_remaining,
	    state->process.saved_remaining, state->saved_remaining))
		return false;
	memcpy(state->process.newline_flag, zero, sizeof(zero));
	state->newline_flag = 0.0f;
	return chat_process_store(&state->foreground,
	    state->process.foreground, 2.0f);
}

enum yt_sysop_chat_step_result
yt_sysop_chat_step(struct yt_sysop_chat_state *state,
    const struct yt_sysop_chat_poll *poll,
    struct yt_sysop_chat_output *output)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	const uint8_t *key;
	size_t key_length;

	if (output != NULL)
		memset(output, 0, sizeof(*output));
	if (state == NULL || poll == NULL || output == NULL
	    || state->key_length > sizeof(state->key)
	    || state->command_accumulator_length > YT_INPUT_PENDING
	    || state->queue_length > YT_INPUT_PENDING
	    || poll->local.length > sizeof(poll->local.bytes)
	    || poll->remote.length > 1U
	    || poll->position_after_output < 0)
		return YT_SYSOP_CHAT_INVALID;
	yt_sysop_chat_sync_process(state);
	if (state->terminated)
		return YT_SYSOP_CHAT_CARRIER_END;
	if (state->exited)
		return YT_SYSOP_CHAT_EXIT;
	if (state->key_length == 1U && state->key[0] == 0x1bU) {
		state->exited = true;
		return YT_SYSOP_CHAT_EXIT;
	}

	state->key_length = poll->local.length;
	if (state->key_length != 0U) {
		memcpy(state->key, poll->local.bytes, state->key_length);
		if (chat_process_bound(state))
			(void)chat_process_store(&state->foreground,
			    state->process.foreground, 6.0f);
		else
			state->foreground = 6.0f;
	}
	if (state->mode == 0.0f && poll->remote.length != 0U) {
		state->key[0] = poll->remote.bytes[0];
		state->key_length = 1U;
		if (chat_process_bound(state))
			(void)chat_process_store(&state->foreground,
			    state->process.foreground, 3.0f);
		else
			state->foreground = 3.0f;
	}
	++state->polls;
	++state->carrier_checks;
	if (state->mode == 0.0f && poll->modem_status < 0x80U) {
		state->terminated = true;
		return YT_SYSOP_CHAT_CARRIER_END;
	}

	key = state->key;
	key_length = state->key_length;
	if (key_length == 1U && key[0] == '\r') {
		state->key_length = 0U;
		key_length = 0U;
		if (state->snoop != 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode != 1.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
	}

	if (!chat_key_accepted(key, key_length))
		return YT_SYSOP_CHAT_CONTINUE;
	if (key[0] == '\b' && poll->position_after_output > 0) {
		if (!chat_append(output, YT_SYSOP_CHAT_LOCAL_RAW, false,
		    local_erase, sizeof(local_erase)))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode == 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL, false,
		    remote_erase, sizeof(remote_erase)))
			return YT_SYSOP_CHAT_INVALID;
		return YT_SYSOP_CHAT_CONTINUE;
	}

	state->bold = 1.0f;
	if (state->snoop != 0.0f
	    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED, false,
	    key, key_length))
		return YT_SYSOP_CHAT_INVALID;
	if (state->mode != 1.0f
	    && !chat_append(output, YT_SYSOP_CHAT_SERIAL, false,
	    key, key_length))
		return YT_SYSOP_CHAT_INVALID;
	if ((poll->position_after_output > 70
	    && key_length == 1U && key[0] == ' ')
	    || poll->position_after_output > 79) {
		if (state->snoop != 0.0f
		    && !chat_append(output, YT_SYSOP_CHAT_LOCAL_GATED,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
		if (state->mode != 1.0f
		    && !chat_append(output, YT_SYSOP_CHAT_SERIAL,
		    true, NULL, 0U))
			return YT_SYSOP_CHAT_INVALID;
	}
	return YT_SYSOP_CHAT_CONTINUE;
}

bool
yt_sysop_chat_finish(struct yt_sysop_chat_state *state,
    float deadline_timer, float inactivity_timer)
{
	float deadline_sample;
	float inactivity_sample;

	if (state == NULL || state->command_accumulator_length > YT_INPUT_PENDING
	    || state->queue_length > YT_INPUT_PENDING)
		return false;
	yt_sysop_chat_sync_process(state);
	if (!state->exited || state->terminated)
		return true;
	deadline_sample = chat_single(floorf(chat_single(deadline_timer)));
	inactivity_sample = chat_single(floorf(chat_single(inactivity_timer)));
	if (chat_process_bound(state)) {
		if (!chat_process_store(&state->deadline, state->process.deadline,
		    chat_single_add(deadline_sample,
		    chat_single(state->saved_remaining))))
			return false;
		if (!chat_process_store(&state->inactivity_deadline,
		    state->process.inactivity_deadline,
		    chat_single_add(inactivity_sample, 240.0f)))
			return false;
	}
	else {
		state->deadline = chat_single_add(deadline_sample,
		    chat_single(state->saved_remaining));
		state->inactivity_deadline = chat_single_add(inactivity_sample,
		    240.0f);
	}
	memset(state->command_accumulator, 0,
	    sizeof(state->command_accumulator));
	state->command_accumulator_length = 0U;
	memset(state->queue, 0, sizeof(state->queue));
	state->queue[0] = '\r';
	state->queue_length = 1U;
	state->timer_reads += 2U;
	return true;
}

bool
yt_sysop_f5_compose(bool same_f5_make, struct yt_sysop_f5_result *result)
{
	static const struct yt_sysop_f5_event events[YT_SYSOP_F5_PHASES] = {
		{YT_SYSOP_F5_CHECKPOINT, 0xBA00U},
		{YT_SYSOP_F5_BASIC_END, 0xBA01U},
		{YT_SYSOP_F5_REGISTERED_CLEANUP, 0x3B2FU},
		{YT_SYSOP_F5_CALLBACK_CLEANUP, 0x3B29U},
		{YT_SYSOP_F5_RUNTIME_CLEANUP, 0x1810U},
		{YT_SYSOP_F5_COMMON_CLEANUP, 0x1C21U},
		{YT_SYSOP_F5_DOS_EXIT, 0x064EU},
	};

	if (result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	memcpy(result->events, events, sizeof(events));
	result->event_count = YT_ARRAY_LEN(events);
	result->same_f5_pending = same_f5_make;
	result->local_end_cleanup = true;
	result->terminated = true;
	return true;
}

static const enum yt_sysop_key sysop_keys[YT_SYSOP_KEY_COUNT] = {
	YT_SYSOP_KEY_F4, YT_SYSOP_KEY_F5, YT_SYSOP_KEY_F8,
	YT_SYSOP_KEY_F9, YT_SYSOP_KEY_F10,
};

static const uint16_t sysop_key_addresses[YT_SYSOP_KEY_COUNT] = {
	0x11AAU, 0x11AFU, 0x11BEU, 0x11C3U, 0x11C8U,
};

static const uint16_t sysop_key_targets[YT_SYSOP_KEY_COUNT] = {
	0xA9E1U, 0xBA00U, 0xB6D7U, 0xB66AU, 0xB3FBU,
};

static size_t
sysop_key_index(enum yt_sysop_key key)
{
	size_t index;

	for (index = 0U; index < YT_ARRAY_LEN(sysop_keys); ++index)
		if (sysop_keys[index] == key)
			return index;
	return YT_SYSOP_KEY_COUNT;
}

#define SYSOP_KEYBOARD_TABLE 0x01C6U
#define SYSOP_KEYBOARD_INSTALLED 0x0E31U
#define SYSOP_KEY_PENDING_COUNT 0x118AU
#define SYSOP_KEY_FIFO_CONTROL 0x1254U
#define SYSOP_KEY_FIFO_BASE 0x1260U
#define SYSOP_KEY_FIFO_END 0x12B0U
#define SYSOP_KEY_FIFO_CAPACITY 0x0050U
#define SYSOP_EVENT_STACK_FLOOR 0x0A02U
#define SYSOP_EVENT_HANDLER_TOP 0x0A04U
#define SYSOP_EVENT_SAVED_BP 0x0A06U
#define SYSOP_EVENT_WAKE_FLAG 0x0A6AU
#define SYSOP_EVENT_ERROR_7 0x0A1EU
#define SYSOP_EVENT_RETURN_HANDLER 0x94D9U
#define SYSOP_EVENT_RETURN_AFTER_CALL 0xF875U
#define SYSOP_EVENT_YT_FRAME_SIZE 0x0008U

static uint16_t
sysop_key_process_word(const uint8_t *process, uint16_t address)
{
	return (uint16_t)(process[address]
	    | (uint16_t)process[(uint16_t)(address + 1U)] << 8);
}

static void
sysop_key_process_set_word(uint8_t *process, uint16_t address,
    uint16_t value)
{
	process[address] = (uint8_t)value;
	process[(uint16_t)(address + 1U)] = (uint8_t)(value >> 8);
}

static uint16_t
sysop_key_ring_next(uint16_t cursor)
{
	return cursor + 1U == SYSOP_KEY_FIFO_END
	    ? SYSOP_KEY_FIFO_BASE : (uint16_t)(cursor + 1U);
}

static bool
sysop_key_raw_matches(const struct yt_sysop_key_scheduler *scheduler)
{
	const uint8_t *process;
	uint16_t cursor;
	size_t count;
	size_t index;

	if (scheduler->fifo_position > scheduler->fifo_length
	    || scheduler->fifo_length > YT_SYSOP_KEY_COUNT
	    || scheduler->frame_depth > YT_SYSOP_KEY_COUNT
	    || scheduler->abandoned_depth > scheduler->frame_depth)
		return false;
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index)
		if (scheduler->records[index].key != sysop_keys[index]
		    || scheduler->records[index].address
		    != sysop_key_addresses[index]
		    || scheduler->records[index].target
		    != sysop_key_targets[index])
			return false;
	if (scheduler->process == NULL)
		return true;
	process = scheduler->process;
	if (process[SYSOP_KEYBOARD_INSTALLED] != 1U
	    || process[SYSOP_KEY_PENDING_COUNT]
	    != scheduler->fifo_length - scheduler->fifo_position
	    || sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 4U)
	    != SYSOP_KEY_FIFO_BASE
	    || sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 6U)
	    != SYSOP_KEY_FIFO_END
	    || sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 8U)
	    != SYSOP_KEY_FIFO_CAPACITY
	    || sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 10U)
	    != 2U * (scheduler->fifo_length - scheduler->fifo_position))
		return false;
	cursor = sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 2U);
	if (cursor < SYSOP_KEY_FIFO_BASE || cursor >= SYSOP_KEY_FIFO_END)
		return false;
	count = scheduler->fifo_length - scheduler->fifo_position;
	for (index = 0U; index < count; ++index) {
		const struct yt_sysop_key_record *record;
		size_t record_index = scheduler->fifo[
		    scheduler->fifo_position + index];

		if (record_index >= YT_SYSOP_KEY_COUNT)
			return false;
		record = &scheduler->records[record_index];
		if (process[cursor] != (uint8_t)(record->address >> 8))
			return false;
		cursor = sysop_key_ring_next(cursor);
		if (process[cursor] != (uint8_t)record->address)
			return false;
		cursor = sysop_key_ring_next(cursor);
	}
	if (cursor != sysop_key_process_word(process,
	    SYSOP_KEY_FIFO_CONTROL))
		return false;
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index) {
		const struct yt_sysop_key_record *record =
		    &scheduler->records[index];
		uint16_t keyboard = (uint16_t)(SYSOP_KEYBOARD_TABLE
		    + (uint16_t)record->key);

		if (process[keyboard] != record->keyboard_state
		    || process[record->address] != record->event_state
		    || sysop_key_process_word(process, record->address + 1U)
		    != record->target
		    || sysop_key_process_word(process, record->address + 3U)
		    != record->target_segment)
			return false;
	}
	return true;
}

static bool
sysop_key_raw_append(struct yt_sysop_key_scheduler *scheduler, uint8_t value)
{
	uint8_t *process = scheduler->process;
	uint16_t count;
	uint16_t cursor;
	uint16_t next;

	if (process == NULL)
		return true;
	cursor = sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL);
	if (cursor < SYSOP_KEY_FIFO_BASE || cursor >= SYSOP_KEY_FIFO_END)
		return false;
	process[cursor] = value;
	next = sysop_key_ring_next(cursor);
	if (next == sysop_key_process_word(process,
	    SYSOP_KEY_FIFO_CONTROL + 2U))
		return false;
	count = sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 10U);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL, next);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 10U,
	    (uint16_t)(count + 1U));
	return true;
}

static bool
sysop_key_raw_enqueue(struct yt_sysop_key_scheduler *scheduler, size_t index)
{
	struct yt_sysop_key_record *record = &scheduler->records[index];

	if (scheduler->process == NULL)
		return true;
	if (!sysop_key_raw_append(scheduler, (uint8_t)(record->address >> 8)))
		return false;
	if (!sysop_key_raw_append(scheduler, (uint8_t)record->address))
		return false;
	++scheduler->process[SYSOP_KEY_PENDING_COUNT];
	return true;
}

static bool
sysop_key_raw_dequeue(struct yt_sysop_key_scheduler *scheduler, size_t index)
{
	struct yt_sysop_key_record *record = &scheduler->records[index];
	uint8_t *process = scheduler->process;
	uint16_t count;
	uint16_t cursor;

	if (process == NULL)
		return true;
	count = sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 10U);
	cursor = sysop_key_process_word(process, SYSOP_KEY_FIFO_CONTROL + 2U);
	if (count < 2U || process[cursor] != (uint8_t)(record->address >> 8))
		return false;
	cursor = sysop_key_ring_next(cursor);
	if (process[cursor] != (uint8_t)record->address)
		return false;
	cursor = sysop_key_ring_next(cursor);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 2U,
	    cursor);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 10U,
	    (uint16_t)(count - 2U));
	--process[SYSOP_KEY_PENDING_COUNT];
	return true;
}

void
yt_sysop_key_scheduler_init(struct yt_sysop_key_scheduler *scheduler)
{
	size_t index;

	if (scheduler == NULL)
		return;
	memset(scheduler, 0, sizeof(*scheduler));
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index) {
		scheduler->records[index].key = sysop_keys[index];
		scheduler->records[index].address = sysop_key_addresses[index];
		scheduler->records[index].target = sysop_key_targets[index];
		scheduler->records[index].keyboard_state = 0x06U;
		scheduler->records[index].event_state = 0x01U;
	}
}

bool
yt_sysop_key_scheduler_bind_process(struct yt_sysop_key_scheduler *scheduler,
    uint8_t *process, size_t process_size, uint16_t target_segment)
{
	size_t index;

	if (scheduler == NULL || process == NULL
	    || process_size != YT_SYSOP_KEY_PROCESS_SIZE
	    || target_segment == 0U
	    || scheduler->process != NULL || scheduler->fifo_position != 0U
	    || scheduler->fifo_length != 0U || scheduler->frame_depth != 0U
	    || scheduler->abandoned_depth != 0U)
		return false;
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index) {
		struct yt_sysop_key_record *record = &scheduler->records[index];

		if (record->key != sysop_keys[index]
		    || record->address != sysop_key_addresses[index]
		    || record->target != sysop_key_targets[index]
		    || record->target_segment != 0U
		    || record->keyboard_state != 0x06U
		    || record->event_state != 0x01U)
			return false;
	}
	process[SYSOP_KEYBOARD_INSTALLED] = 1U;
	process[SYSOP_KEY_PENDING_COUNT] = 0U;
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL,
	    SYSOP_KEY_FIFO_BASE);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 2U,
	    SYSOP_KEY_FIFO_BASE);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 4U,
	    SYSOP_KEY_FIFO_BASE);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 6U,
	    SYSOP_KEY_FIFO_END);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 8U,
	    SYSOP_KEY_FIFO_CAPACITY);
	sysop_key_process_set_word(process, SYSOP_KEY_FIFO_CONTROL + 10U, 0U);
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index) {
		struct yt_sysop_key_record *record = &scheduler->records[index];
		uint16_t keyboard = (uint16_t)(SYSOP_KEYBOARD_TABLE
		    + (uint16_t)record->key);

		record->target_segment = target_segment;
		process[keyboard] = record->keyboard_state;
		process[record->address] = record->event_state;
		sysop_key_process_set_word(process, record->address + 1U,
		    record->target);
		sysop_key_process_set_word(process, record->address + 3U,
		    target_segment);
	}
	scheduler->process = process;
	return true;
}

const struct yt_sysop_key_record *
yt_sysop_key_record(const struct yt_sysop_key_scheduler *scheduler,
    enum yt_sysop_key key)
{
	size_t index = sysop_key_index(key);

	return scheduler == NULL || index == YT_SYSOP_KEY_COUNT
	    ? NULL : &scheduler->records[index];
}

bool
yt_sysop_key_latch(struct yt_sysop_key_scheduler *scheduler,
    enum yt_sysop_key key)
{
	size_t index = sysop_key_index(key);

	if (scheduler == NULL || index == YT_SYSOP_KEY_COUNT
	    || !sysop_key_raw_matches(scheduler))
		return false;
	/* The rooted YT records remain KEY ON, including while STOPped. */
	if ((scheduler->records[index].event_state & 0x01U) == 0U)
		return false;
	scheduler->records[index].keyboard_state |= 0x01U;
	if (scheduler->process != NULL)
		scheduler->process[SYSOP_KEYBOARD_TABLE + (uint16_t)key]
		    = scheduler->records[index].keyboard_state;
	return true;
}

static void
sysop_key_compact_fifo(struct yt_sysop_key_scheduler *scheduler)
{
	if (scheduler->fifo_position == 0U)
		return;
	if (scheduler->fifo_position < scheduler->fifo_length)
		memmove(scheduler->fifo,
		    scheduler->fifo + scheduler->fifo_position,
		    (scheduler->fifo_length - scheduler->fifo_position)
		    * sizeof(scheduler->fifo[0]));
	scheduler->fifo_length -= scheduler->fifo_position;
	scheduler->fifo_position = 0U;
}

static bool
sysop_key_enqueue(struct yt_sysop_key_scheduler *scheduler, size_t index)
{
	if (scheduler->fifo_length == YT_SYSOP_KEY_COUNT)
		sysop_key_compact_fifo(scheduler);
	if (scheduler->fifo_length == YT_SYSOP_KEY_COUNT)
		return false;
	if (!sysop_key_raw_enqueue(scheduler, index))
		return false;
	scheduler->fifo[scheduler->fifo_length++] = index;
	return true;
}

bool
yt_sysop_key_checkpoint(struct yt_sysop_key_scheduler *scheduler,
    bool error_active, struct yt_sysop_key_delivery *delivery)
{
	size_t index;

	if (scheduler == NULL || delivery == NULL
	    || scheduler->fifo_position > scheduler->fifo_length
	    || scheduler->fifo_length > YT_SYSOP_KEY_COUNT
	    || scheduler->frame_depth > YT_SYSOP_KEY_COUNT
	    || scheduler->abandoned_depth > scheduler->frame_depth
	    || !sysop_key_raw_matches(scheduler))
		return false;
	memset(delivery, 0, sizeof(*delivery));
	/* BRUN drains the low keyboard table in ascending address order. */
	for (index = 0U; index < YT_SYSOP_KEY_COUNT; ++index) {
		struct yt_sysop_key_record *record = &scheduler->records[index];
		uint8_t old_state;

		if (record->keyboard_state != 0x07U)
			continue;
		record->keyboard_state = 0x06U;
		if (scheduler->process != NULL)
			scheduler->process[SYSOP_KEYBOARD_TABLE
			    + (uint16_t)record->key] = 0x06U;
		if ((record->event_state & 0x01U) == 0U)
			continue;
		old_state = record->event_state;
		record->event_state |= 0x04U;
		if (scheduler->process != NULL)
			scheduler->process[record->address] = record->event_state;
		if ((old_state & (0x02U | 0x04U)) == 0U
		    && !sysop_key_enqueue(scheduler, index))
			return false;
	}
	if (error_active)
		return true;
	while (scheduler->fifo_position < scheduler->fifo_length) {
		struct yt_sysop_key_record *record;

		index = scheduler->fifo[scheduler->fifo_position++];
		if (index >= YT_SYSOP_KEY_COUNT)
			return false;
		record = &scheduler->records[index];
		if (!sysop_key_raw_dequeue(scheduler, index))
			return false;
		record->event_state &= (uint8_t)~0x04U;
		if (scheduler->process != NULL)
			scheduler->process[record->address] = record->event_state;
		if ((record->event_state & 0x01U) == 0U)
			continue;
		if (scheduler->frame_depth == YT_SYSOP_KEY_COUNT)
			return false;
		record->event_state |= 0x02U;
		if (scheduler->process != NULL)
			scheduler->process[record->address] = record->event_state;
		scheduler->frames[scheduler->frame_depth++] = index;
		delivery->delivered = true;
		delivery->key = record->key;
		delivery->record_address = record->address;
		delivery->target = record->target;
		if (scheduler->fifo_position == scheduler->fifo_length) {
			scheduler->fifo_position = 0U;
			scheduler->fifo_length = 0U;
		}
		return true;
	}
	scheduler->fifo_position = 0U;
	scheduler->fifo_length = 0U;
	return true;
}

bool
yt_sysop_key_return(struct yt_sysop_key_scheduler *scheduler,
    enum yt_sysop_key *returned)
{
	struct yt_sysop_key_record *record;
	size_t index;

	if (scheduler == NULL
	    || scheduler->frame_depth <= scheduler->abandoned_depth
	    || scheduler->frame_depth > YT_SYSOP_KEY_COUNT)
		return false;
	if (!sysop_key_raw_matches(scheduler))
		return false;
	index = scheduler->frames[scheduler->frame_depth - 1U];
	if (index >= YT_SYSOP_KEY_COUNT)
		return false;
	record = &scheduler->records[index];
	if ((record->event_state & 0x02U) == 0U)
		return false;
	--scheduler->frame_depth;
	record->event_state &= (uint8_t)~0x02U;
	if (scheduler->process != NULL)
		scheduler->process[record->address] = record->event_state;
	if ((record->event_state & 0x04U) != 0U
	    && !sysop_key_enqueue(scheduler, index))
		return false;
	if (returned != NULL)
		*returned = record->key;
	return true;
}

bool
yt_sysop_key_resume_abandon(struct yt_sysop_key_scheduler *scheduler)
{
	if (scheduler == NULL
	    || scheduler->frame_depth <= scheduler->abandoned_depth
	    || !sysop_key_raw_matches(scheduler))
		return false;
	/* RESUME 081F does not run any event RETURN trampoline. */
	scheduler->abandoned_depth = scheduler->frame_depth;
	return true;
}

static bool
sysop_event_stack_valid(const struct yt_sysop_event_stack *stack)
{
	return stack != NULL && stack->bytes != NULL
	    && stack->size == YT_SYSOP_EVENT_STACK_SIZE;
}

static uint16_t
sysop_event_stack_word(const struct yt_sysop_event_stack *stack,
    uint16_t address)
{
	return (uint16_t)(stack->bytes[address]
	    | (uint16_t)stack->bytes[(uint16_t)(address + 1U)] << 8);
}

static void
sysop_event_stack_set_word(struct yt_sysop_event_stack *stack,
    uint16_t address, uint16_t value)
{
	stack->bytes[address] = (uint8_t)value;
	stack->bytes[(uint16_t)(address + 1U)] = (uint8_t)(value >> 8);
}

bool
yt_sysop_event_stack_init(struct yt_sysop_key_scheduler *scheduler,
    struct yt_sysop_event_stack *stack, uint16_t floor)
{
	uint16_t target_segment;
	size_t index;

	if (scheduler == NULL || scheduler->process == NULL
	    || !sysop_event_stack_valid(stack)
	    || !sysop_key_raw_matches(scheduler))
		return false;
	target_segment = scheduler->records[0].target_segment;
	if (target_segment == 0U || stack->cs != target_segment
	    || stack->ip != 0x0120U
	    || sysop_key_process_word(scheduler->process,
	    SYSOP_EVENT_STACK_FLOOR) != 0U
	    || sysop_key_process_word(scheduler->process,
	    SYSOP_EVENT_HANDLER_TOP) != 0U
	    || sysop_key_process_word(scheduler->process,
	    SYSOP_EVENT_SAVED_BP) != 0U)
		return false;
	for (index = 1U; index < YT_SYSOP_KEY_COUNT; ++index)
		if (scheduler->records[index].target_segment != target_segment)
			return false;
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_STACK_FLOOR, floor);
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_HANDLER_TOP, stack->sp);
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_SAVED_BP, stack->bp);
	return true;
}

enum yt_sysop_event_stack_outcome
yt_sysop_event_deliver_raw(struct yt_sysop_key_scheduler *scheduler,
    const struct yt_sysop_key_delivery *delivery,
    struct yt_sysop_event_stack *stack, uint16_t brun_segment,
    struct yt_sysop_event_registers *registers)
{
	struct yt_sysop_key_record *record;
	size_t index;
	uint16_t interrupted_cs;
	uint16_t resume_ip;
	uint16_t program_sp;
	uint16_t frame_base;
	uint16_t frame_top;
	uint16_t post_push_sp;

	if (scheduler == NULL || scheduler->process == NULL || delivery == NULL
	    || registers == NULL
	    || !delivery->delivered || !sysop_event_stack_valid(stack)
	    || !sysop_key_raw_matches(scheduler)
	    || scheduler->frame_depth == 0U)
		return YT_SYSOP_EVENT_STACK_INVALID;
	index = sysop_key_index(delivery->key);
	if (index == YT_SYSOP_KEY_COUNT
	    || scheduler->frames[scheduler->frame_depth - 1U] != index)
		return YT_SYSOP_EVENT_STACK_INVALID;
	record = &scheduler->records[index];
	if (record->event_state != 0x03U
	    || delivery->record_address != record->address
	    || delivery->target != record->target
	    || record->target == 0U || record->target_segment == 0U)
		return YT_SYSOP_EVENT_STACK_INVALID;
	program_sp = stack->sp;
	interrupted_cs = stack->cs;
	resume_ip = (uint16_t)(stack->ip + 1U);
	sysop_event_stack_set_word(stack, (uint16_t)(program_sp - 2U),
	    registers->flags);
	sysop_event_stack_set_word(stack, (uint16_t)(program_sp - 4U),
	    interrupted_cs);
	sysop_event_stack_set_word(stack, (uint16_t)(program_sp - 6U),
	    resume_ip);
	frame_base = (uint16_t)(program_sp - 2U);
	frame_top = (uint16_t)(frame_base - SYSOP_EVENT_YT_FRAME_SIZE);
	sysop_event_stack_set_word(stack, frame_base, stack->bp);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 2U),
	    interrupted_cs);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 4U),
	    resume_ip);
	post_push_sp = (uint16_t)(frame_top - 4U);
	registers->bx = record->address;
	registers->cx = interrupted_cs;
	registers->dx = resume_ip;
	registers->si = SYSOP_KEY_FIFO_CONTROL;
	if (post_push_sp < sysop_key_process_word(scheduler->process,
	    SYSOP_EVENT_STACK_FLOOR)) {
		stack->sp = post_push_sp;
		stack->bp = frame_base;
		stack->cs = brun_segment;
		stack->ip = SYSOP_EVENT_ERROR_7;
		return YT_SYSOP_EVENT_STACK_ERROR_7;
	}
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_SAVED_BP, frame_base);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_base - 2U),
	    record->target_segment);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_base - 4U), 1U);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 6U),
	    record->address);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 8U), 0U);
	stack->sp = (uint16_t)(frame_top - 8U);
	stack->bp = frame_base;
	stack->cs = record->target_segment;
	stack->ip = record->target;
	registers->ax = record->target_segment;
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_HANDLER_TOP, stack->sp);
	return YT_SYSOP_EVENT_STACK_OK;
}

bool
yt_sysop_event_checkpoint_quiet_raw(
    struct yt_sysop_key_scheduler *scheduler,
    struct yt_sysop_event_stack *stack,
    struct yt_sysop_event_registers *registers)
{
	uint16_t sp;

	if (scheduler == NULL || scheduler->process == NULL || registers == NULL
	    || !sysop_event_stack_valid(stack)
	    || !sysop_key_raw_matches(scheduler))
		return false;
	if (scheduler->process[SYSOP_EVENT_WAKE_FLAG] != 0U)
		scheduler->process[SYSOP_EVENT_WAKE_FLAG] = 0U;
	sp = stack->sp;
	sysop_event_stack_set_word(stack, (uint16_t)(sp - 2U),
	    registers->flags);
	sysop_event_stack_set_word(stack, (uint16_t)(sp - 4U), stack->cs);
	sysop_event_stack_set_word(stack, (uint16_t)(sp - 6U),
	    (uint16_t)(stack->ip + 1U));
	stack->ip = (uint16_t)(stack->ip + 1U);
	return true;
}

bool
yt_sysop_event_return_raw(struct yt_sysop_key_scheduler *scheduler,
    struct yt_sysop_event_stack *stack, uint16_t opcode_flags,
    enum yt_sysop_key *returned)
{
	struct yt_sysop_key_record *record;
	size_t index;
	uint16_t body_sp;
	uint16_t outer_ip;
	uint16_t outer_cs;
	uint16_t frame_top;
	uint16_t old_bp;
	uint16_t resumed_sp;

	if (scheduler == NULL || scheduler->process == NULL
	    || !sysop_event_stack_valid(stack)
	    || !sysop_key_raw_matches(scheduler)
	    || scheduler->frame_depth <= scheduler->abandoned_depth)
		return false;
	index = scheduler->frames[scheduler->frame_depth - 1U];
	if (index >= YT_SYSOP_KEY_COUNT)
		return false;
	record = &scheduler->records[index];
	body_sp = stack->sp;
	if ((record->event_state != 0x03U && record->event_state != 0x07U)
	    || sysop_event_stack_word(stack, body_sp) != 0U
	    || sysop_event_stack_word(stack, (uint16_t)(body_sp + 2U))
	    != record->address
	    || sysop_event_stack_word(stack, (uint16_t)(stack->bp - 4U))
	    != 1U)
		return false;
	outer_ip = sysop_event_stack_word(stack, (uint16_t)(body_sp + 4U));
	outer_cs = sysop_event_stack_word(stack, (uint16_t)(body_sp + 6U));
	old_bp = sysop_event_stack_word(stack, stack->bp);
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 2U),
	    opcode_flags);
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 4U), stack->cs);
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 6U),
	    (uint16_t)(stack->ip + 2U));
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 2U), stack->cs);
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 4U),
	    (uint16_t)(stack->ip + 3U));
	sysop_event_stack_set_word(stack, (uint16_t)(body_sp - 6U),
	    SYSOP_EVENT_RETURN_HANDLER);
	sysop_event_stack_set_word(stack, (uint16_t)(stack->bp - 4U), 0U);
	frame_top = (uint16_t)(body_sp + 8U);
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_HANDLER_TOP, frame_top);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 2U), outer_cs);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 4U), outer_ip);
	sysop_event_stack_set_word(stack, (uint16_t)(frame_top - 6U),
	    SYSOP_EVENT_RETURN_AFTER_CALL);
	if (!yt_sysop_key_return(scheduler, returned))
		return false;
	resumed_sp = (uint16_t)(stack->bp + 2U);
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_HANDLER_TOP, resumed_sp);
	sysop_key_process_set_word(scheduler->process,
	    SYSOP_EVENT_SAVED_BP, old_bp);
	sysop_event_stack_set_word(stack, (uint16_t)(resumed_sp - 2U), outer_cs);
	sysop_event_stack_set_word(stack, (uint16_t)(resumed_sp - 4U), outer_ip);
	stack->sp = resumed_sp;
	stack->bp = old_bp;
	stack->cs = outer_cs;
	stack->ip = outer_ip;
	return true;
}

size_t
yt_sysop_key_fifo_bytes(const struct yt_sysop_key_scheduler *scheduler,
    uint8_t *bytes, size_t capacity)
{
	size_t count;
	size_t index;

	if (scheduler == NULL || scheduler->fifo_position > scheduler->fifo_length
	    || scheduler->fifo_length > YT_SYSOP_KEY_COUNT)
		return 0U;
	count = scheduler->fifo_length - scheduler->fifo_position;
	if (count > SIZE_MAX / 2U || count * 2U > capacity
	    || (bytes == NULL && count != 0U))
		return 0U;
	for (index = 0U; index < count; ++index) {
		const struct yt_sysop_key_record *record;
		size_t record_index = scheduler->fifo[
		    scheduler->fifo_position + index];

		if (record_index >= YT_SYSOP_KEY_COUNT)
			return 0U;
		record = &scheduler->records[record_index];
		bytes[index * 2U] = (uint8_t)(record->address >> 8);
		bytes[index * 2U + 1U] = (uint8_t)record->address;
	}
	return count * 2U;
}
