#include "yt_startup.h"

#include "qb.h"

#include <string.h>


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
