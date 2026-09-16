#include "yt_startup_model.h"

#include "qb.h"

#include <math.h>
#include <string.h>

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
	*baud = qb_single_divide(115200.0f, (float)divisor);
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
	divisor = qb_single_divide(115200.0f, baud);
	high_product = qb_single_multiply(divisor, 1.0f / 256.0f);
	high_fixed = truncf(high_product);
	low = qb_single_subtract(divisor,
	    qb_single_multiply(high_fixed, 256.0f));
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
