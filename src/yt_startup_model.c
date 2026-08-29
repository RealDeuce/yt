#include "yt_startup_model.h"

#include "qb.h"

#include <math.h>
#include <string.h>

static float
single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

static float
single_subtract(float left, float right)
{
	volatile float result = left - right;
	return result;
}

static float
single_multiply(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
single_divide(float left, float right)
{
	volatile float result = left / right;
	return result;
}

bool
yt_startup_split_command(const uint8_t *command, size_t length,
    struct yt_startup_command_split *result)
{
	size_t split;

	if (result == NULL || (command == NULL && length != 0U)
	    || length > YT_STARTUP_COMMAND_SIZE)
		return false;
	memset(result, 0, sizeof(*result));
	for (split = 0U; split < length && command[split] != ' '; ++split)
		;
	if (split < length)
		++split;
	result->path_length = split;
	result->remainder_length = length - split;
	if (split != 0U)
		memcpy(result->path, command, split);
	if (result->remainder_length != 0U)
		memcpy(result->remainder, command + split,
		    result->remainder_length);
	return true;
}

bool
yt_startup_compose_entry(const uint8_t *command, size_t length,
    struct yt_startup_entry_result *result)
{
	if (result == NULL || (command == NULL && length != 0U))
		return false;
	memset(result, 0, sizeof(*result));
	result->installed_handler = 0x45F7U;
	result->handler_installed = true;
	if (!yt_startup_split_command(command, length, &result->command))
		return false;
	if (length == 0U) {
		result->outcome = YT_STARTUP_ENTRY_MISSING_COMMAND_END;
		result->process_end = true;
	}
	else
		result->outcome = YT_STARTUP_ENTRY_CONTINUE;
	return true;
}

int
yt_startup_parse_port(const uint8_t *identifier, size_t length)
{
	uint8_t final;

	if (identifier == NULL || length == 0U)
		return 0;
	if (identifier[length - 1U] == ':')
		--length;
	if (length == 0U)
		return 0;
	final = identifier[length - 1U];
	return final >= '0' && final <= '9' ? final - '0' : 0;
}

bool
yt_startup_serial_layout(int port, struct yt_startup_serial_layout *layout)
{
	uint16_t offset;

	if (layout == NULL || port < 1 || port > 4)
		return false;
	memset(layout, 0, sizeof(*layout));
	offset = (port == 2 || port == 4) ? 0x100U : 0U;
	if (port == 3 || port == 4)
		offset = (uint16_t)(offset + 0x10U);
	layout->port = port;
	layout->brun_device = (port == 1 || port == 3) ? 1 : 2;
	layout->uart_base = (uint16_t)(0x3F8U - offset);
	layout->modem_status_port = (uint16_t)(layout->uart_base + 6U);
	if (port == 3) {
		layout->bios_address = 0x400U;
		layout->bios_value = 0x03E8U;
	}
	else if (port == 4) {
		layout->bios_address = 0x402U;
		layout->bios_value = 0x02E8U;
	}
	return true;
}

bool
yt_startup_detect_baud(uint8_t dll, uint8_t dlm, float *baud)
{
	uint16_t divisor = (uint16_t)(((uint16_t)dlm << 8) | dll);

	if (baud == NULL || divisor == 0U)
		return false;
	*baud = single_divide(115200.0f, (float)divisor);
	return true;
}

bool
yt_startup_divisor_from_observed_baud(uint32_t baud, uint8_t *dll,
    uint8_t *dlm)
{
	uint32_t divisor;

	if (dll == NULL || dlm == NULL || baud == 0U || baud > 115200U
	    || 115200U % baud != 0U)
		return false;
	divisor = 115200U / baud;
	if (divisor == 0U || divisor > UINT16_MAX)
		return false;
	*dll = (uint8_t)(divisor & 0xffU);
	*dlm = (uint8_t)(divisor >> 8);
	return true;
}

bool
yt_startup_framing_compose(const uint8_t *description,
    size_t description_length, struct yt_startup_framing *framing)
{
	size_t index;

	if ((description == NULL && description_length != 0U)
	    || framing == NULL)
		return false;
	framing->opening_baud = 1200U;
	framing->parity = YT_STARTUP_PARITY_NONE;
	framing->data_bits = 8U;
	framing->stop_bits = 1U;
	for (index = 0U; index < description_length; ++index) {
		if (description[index] == '7') {
			framing->parity = YT_STARTUP_PARITY_EVEN;
			framing->data_bits = 7U;
			break;
		}
	}
	return true;
}

bool
yt_startup_open_spec(int port, const uint8_t *description,
    size_t description_length, uint8_t *spec, size_t capacity,
    size_t *spec_length)
{
	struct yt_startup_framing open_framing;
	struct yt_startup_serial_layout layout;
	const char *framing;
	const char *device;
	const char *suffix = ",CS65535,DS,CD";
	size_t device_length;
	size_t framing_length;
	size_t suffix_length = strlen(suffix);
	size_t length;

	if (!yt_startup_serial_layout(port, &layout)
	    || (description == NULL && description_length != 0U)
	    || spec == NULL || spec_length == NULL
	    || !yt_startup_framing_compose(description, description_length,
	    &open_framing))
		return false;
	framing = open_framing.parity == YT_STARTUP_PARITY_EVEN
	    ? ",E,7,1" : ",N,8,1";
	framing_length = strlen(framing);
	device = layout.brun_device == 1 ? "COM1:1200" : "COM2:1200";
	device_length = strlen(device);
	length = device_length + framing_length + suffix_length;
	if (length > capacity)
		return false;
	memcpy(spec, device, device_length);
	memcpy(spec + device_length, framing, framing_length);
	memcpy(spec + device_length + framing_length, suffix, suffix_length);
	*spec_length = length;
	return true;
}

bool
yt_startup_restored_divisor(float baud, uint8_t *dll, uint8_t *dlm)
{
	float divisor;
	float high_product;
	float high_fixed;
	float low;
	int32_t low_integer;
	int32_t high_integer;
	bool overflow;

	if (dll == NULL || dlm == NULL || !isfinite(baud) || baud <= 0.0f)
		return false;
	divisor = single_divide(115200.0f, baud);
	high_product = single_multiply(divisor, 1.0f / 256.0f);
	high_fixed = truncf(high_product);
	low = single_subtract(divisor,
	    single_multiply(high_fixed, 256.0f));
	low_integer = qb_cint_mode((double)low, 0U, &overflow);
	if (overflow)
		return false;
	high_integer = qb_cint_mode((double)high_fixed, 0U, &overflow);
	if (overflow || low_integer < 0 || low_integer > 256
	    || high_integer < 0 || high_integer > 255)
		return false;
	*dll = (uint8_t)low_integer;
	*dlm = (uint8_t)high_integer;
	return true;
}

float
yt_startup_session_deadline(float timer, double minutes, float cap_timer)
{
	float requested;
	float maximum;
	float minute_single = (float)minutes;

	requested = single_subtract(single_add(floorf(timer),
	    single_multiply(60.0f, minute_single)), 3.0f);
	maximum = single_add(floorf(cap_timer), 10800.0f);
	return requested < maximum ? requested : maximum;
}

bool
yt_startup_canonical_name(const uint8_t *first, size_t first_length,
    const uint8_t *last, size_t last_length, uint8_t *name,
    size_t capacity, size_t *name_length)
{
	size_t length;

	if ((first == NULL && first_length != 0U)
	    || (last == NULL && last_length != 0U)
	    || name == NULL || name_length == NULL
	    || first_length > capacity || last_length > capacity - first_length
	    || first_length + last_length == SIZE_MAX
	    || first_length + last_length + 1U > capacity)
		return false;
	if (first_length != 0U)
		memcpy(name, first, first_length);
	name[first_length] = ' ';
	if (last_length != 0U)
		memcpy(name + first_length + 1U, last, last_length);
	length = first_length + last_length + 1U;
	length = qb_trim_n(name, length);
	length = qb_collapse_spaces_n(name, length);
	length = qb_title_case_n(name, length);
	*name_length = length;
	return true;
}

bool
yt_startup_parse_dorinfo(const uint8_t *raw, size_t raw_length,
    uint8_t *storage, size_t storage_capacity,
    struct yt_startup_dorinfo_result *result)
{
	size_t position = 0U;
	size_t used = 0U;
	size_t field;

	if ((raw == NULL && raw_length != 0U)
	    || (storage == NULL && storage_capacity != 0U) || result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	result->outcome = YT_STARTUP_DORINFO_SUCCESS;
	for (field = 0U; field < YT_STARTUP_DORINFO_FIELDS; ++field) {
		struct yt_startup_dorinfo_field *destination =
		    &result->fields[field];

		destination->offset = used;
		if (position >= raw_length || raw[position] == 0x1aU) {
			result->outcome = YT_STARTUP_DORINFO_INPUT_PAST_END;
			result->failed_field = field + 1U;
			result->cursor = position;
			result->error_number = 62;
			return true;
		}
		while (position < raw_length && raw[position] != 0x1aU) {
			uint8_t value = raw[position++];

			if (value == 0U)
				continue;
			if (value == '\r') {
				if (position < raw_length && raw[position] == '\n')
					++position;
				break;
			}
			if (used >= storage_capacity)
				return false;
			storage[used++] = value;
		}
		destination->length = used - destination->offset;
		++result->fields_assigned;
	}
	result->cursor = position;
	return true;
}

const uint8_t *
yt_startup_dorinfo_field(const struct yt_startup_dorinfo_result *result,
    const uint8_t *storage, size_t field, size_t *length)
{
	if (result == NULL || storage == NULL || length == NULL
	    || field >= result->fields_assigned)
		return NULL;
	*length = result->fields[field].length;
	return storage + result->fields[field].offset;
}

bool
yt_startup_compose_state(const struct yt_startup_dorinfo_result *dorinfo,
    uint8_t *storage, uint8_t dll, uint8_t dlm, float timer,
    float cap_timer, uint8_t *canonical_name, size_t canonical_capacity,
    struct yt_startup_state_result *result)
{
	uint8_t *field[YT_STARTUP_DORINFO_FIELDS];
	size_t length[YT_STARTUP_DORINFO_FIELDS];
	struct qb_val_result numeric;
	size_t index;

	if (dorinfo == NULL || storage == NULL || canonical_name == NULL
	    || result == NULL || dorinfo->outcome != YT_STARTUP_DORINFO_SUCCESS
	    || dorinfo->fields_assigned != YT_STARTUP_DORINFO_FIELDS)
		return false;
	memset(result, 0, sizeof(*result));
	for (index = 0U; index < YT_STARTUP_DORINFO_FIELDS; ++index) {
		field[index] = (uint8_t *)yt_startup_dorinfo_field(dorinfo,
		    storage, index, &length[index]);
		if (field[index] == NULL)
			return false;
	}
	result->requested_port = yt_startup_parse_port(field[3], length[3]);
	result->sampled_dll = dll;
	result->sampled_dlm = dlm;
	if (!yt_startup_canonical_name(field[6], length[6], field[7],
	    length[7], canonical_name, canonical_capacity,
	    &result->canonical_name_length))
		return false;
	if (result->requested_port >= 1 && result->requested_port <= 4) {
		if (!yt_startup_detect_baud(dll, dlm, &result->detected_baud)) {
			result->outcome = YT_STARTUP_STATE_ZERO_DIVISOR;
			return true;
		}
		qb_compat_upper_n(field[4], length[4]);
		if (!yt_startup_open_spec(result->requested_port, field[4],
		    length[4], result->open_spec, sizeof(result->open_spec),
		    &result->open_spec_length)
		    || !yt_startup_restored_divisor(result->detected_baud,
		    &result->restored_dll, &result->restored_dlm))
			return false;
		result->outcome = YT_STARTUP_STATE_REMOTE_READY;
		result->carrier_local_screen = -1.0f;
	}
	else {
		result->outcome = YT_STARTUP_STATE_LOCAL_READY;
		result->local_mode = 1.0f;
		result->carrier_local_screen = -1.0f;
		result->local_sound = -1.0f;
	}
	numeric = qb_val_n(field[9], length[9]);
	if (numeric.overflow)
		return false;
	result->ansi_flag = (float)numeric.value;
	numeric = qb_val_n(field[11], length[11]);
	if (numeric.overflow)
		return false;
	result->deadline = yt_startup_session_deadline(timer, numeric.value,
	    cap_timer);
	result->deadline_set = true;
	result->game_sound = -1.0f;
	for (index = 0U; index < YT_STARTUP_DORINFO_FIELDS; ++index)
		result->cleared_fields[index] = index == 3U || index == 4U
		    || index == 8U || index == 9U || index == 10U;
	return true;
}

static bool
add_event(struct yt_startup_event_result *result,
    enum yt_startup_event_operation operation, uint16_t address,
    uint16_t value, int error_number, bool complete)
{
	struct yt_startup_event *event;

	if (result->event_count >= YT_STARTUP_EVENTS)
		return false;
	event = &result->events[result->event_count++];
	event->operation = operation;
	event->address = address;
	event->value = value;
	event->error_number = error_number;
	event->complete = complete;
	return true;
}

bool
yt_startup_compose_events(const struct yt_startup_state_result *state,
    uint8_t pre_open_lcr, uint8_t pre_open_ier, int serial_open_error,
    enum yt_startup_wait_exit wait_exit, uint8_t modem_status,
    uint8_t post_open_lcr, uint8_t post_open_ier,
    struct yt_startup_event_result *result)
{
	struct yt_startup_serial_layout layout;
	uint16_t base;

	if (state == NULL || result == NULL || serial_open_error < 0
	    || serial_open_error > 255
	    || wait_exit < YT_STARTUP_WAIT_TIMER
	    || wait_exit > YT_STARTUP_WAIT_SERIAL_KEY)
		return false;
	memset(result, 0, sizeof(*result));
	result->wait_exit = wait_exit;
	if (state->outcome == YT_STARTUP_STATE_LOCAL_READY) {
		result->outcome = YT_STARTUP_EVENTS_LOCAL_READY;
		return true;
	}
	if ((state->outcome != YT_STARTUP_STATE_REMOTE_READY
	    && state->outcome != YT_STARTUP_STATE_ZERO_DIVISOR)
	    || !yt_startup_serial_layout(state->requested_port, &layout))
		return false;
	base = layout.uart_base;
	if (!add_event(result, YT_STARTUP_EVENT_DEF_SEG, 0U, 0U, 0, true))
		return false;
	if (layout.bios_address != 0U
	    && (!add_event(result, YT_STARTUP_EVENT_POKE,
	    layout.bios_address, (uint16_t)(layout.bios_value & 0xffU), 0,
	    true)
	    || !add_event(result, YT_STARTUP_EVENT_POKE,
	    (uint16_t)(layout.bios_address + 1U),
	    (uint16_t)(layout.bios_value >> 8), 0, true)))
		return false;
	if (!add_event(result, YT_STARTUP_EVENT_IN, (uint16_t)(base + 3U),
	    pre_open_lcr, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr & 0x7fU), 0,
	    true)
	    || !add_event(result, YT_STARTUP_EVENT_IN,
	    (uint16_t)(base + 1U), pre_open_ier, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 1U), 0U, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr | 0x80U), 0,
	    true)
	    || !add_event(result, YT_STARTUP_EVENT_IN,
	    (uint16_t)(base + 1U), state->sampled_dlm, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_IN, base,
	    state->sampled_dll, 0, true))
		return false;
	if (state->outcome == YT_STARTUP_STATE_ZERO_DIVISOR) {
		if (!add_event(result, YT_STARTUP_EVENT_RUNTIME_ERROR, 0U, 0U,
		    11, false))
			return false;
		result->outcome = YT_STARTUP_EVENTS_ZERO_DIVISOR;
		return true;
	}
	if (!add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(pre_open_lcr & 0x7fU), 0,
	    true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 1U), pre_open_ier, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), pre_open_lcr, 0, true))
		return false;
	if (!add_event(result, YT_STARTUP_EVENT_UPPERCASE, 0x5690U, 0U, 0,
	    true))
		return false;
	if (!add_event(result, YT_STARTUP_EVENT_OPEN, 3U, 0U,
	    serial_open_error, serial_open_error == 0))
		return false;
	if (serial_open_error != 0) {
		result->outcome = YT_STARTUP_EVENTS_OPEN_ERROR;
		return true;
	}
	if (!add_event(result, YT_STARTUP_EVENT_SET_LOCAL_SOUND, 0x4B70U,
	    0U, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_WAIT, 0U,
	    (uint16_t)wait_exit, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_CARRIER_CHECK,
	    layout.modem_status_port, modem_status, 0, true))
		return false;
	result->carrier_checked = true;
	result->carrier_detected = (modem_status & 0x80U) != 0U;
	if (!result->carrier_detected) {
		if (state->carrier_local_screen != 0.0f
		    && !add_event(result, YT_STARTUP_EVENT_CARRIER_NOTICE, 0U,
		    0U, 0, true))
			return false;
		if (!add_event(result, YT_STARTUP_EVENT_CLOSE, 0U, 0U, 0, true)
		    || !add_event(result, YT_STARTUP_EVENT_PROCESS_END, 0U, 0U,
		    0, true))
			return false;
		result->outcome = YT_STARTUP_EVENTS_CARRIER_DROP;
		return true;
	}
	if (!add_event(result, YT_STARTUP_EVENT_IN, (uint16_t)(base + 3U),
	    post_open_lcr, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_IN,
	    (uint16_t)(base + 1U), post_open_ier, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 1U), 0U, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), (uint16_t)(post_open_lcr | 0x80U), 0,
	    true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT, base,
	    state->restored_dll, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 1U), state->restored_dlm, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 3U), post_open_lcr, 0, true)
	    || !add_event(result, YT_STARTUP_EVENT_OUT,
	    (uint16_t)(base + 1U), post_open_ier, 0, true))
		return false;
	result->outcome = YT_STARTUP_EVENTS_REMOTE_READY;
	return true;
}
