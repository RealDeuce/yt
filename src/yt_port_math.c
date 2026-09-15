#include "yt_port_math.h"

#include <stdbool.h>
#include <string.h>

void
yt_port_mbf64_promote_single(const uint8_t single[4], uint8_t raw[8])
{
	memset(raw, 0, 8U);
	if (single[3] != 0U)
		memcpy(raw + 4U, single, 4U);
}

static int
compare_magnitude(const uint8_t left[8], const uint8_t right[8])
{
	int index;

	if (left[7] != right[7])
		return left[7] < right[7] ? -1 : 1;
	for (index = 6; index >= 0; --index) {
		uint8_t left_byte = left[index];
		uint8_t right_byte = right[index];

		if (index == 6) {
			left_byte &= 0x7fU;
			right_byte &= 0x7fU;
		}
		if (left_byte != right_byte)
			return left_byte < right_byte ? -1 : 1;
	}
	return 0;
}

int
yt_port_mbf64_compare(const uint8_t left[8], const uint8_t right[8])
{
	bool left_negative;
	bool right_negative;
	int magnitude;

	if (left[7] == 0U)
		return right[7] == 0U ? 0
		    : ((right[6] & 0x80U) != 0U ? 1 : -1);
	if (right[7] == 0U)
		return (left[6] & 0x80U) != 0U ? -1 : 1;
	left_negative = (left[6] & 0x80U) != 0U;
	right_negative = (right[6] & 0x80U) != 0U;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	magnitude = compare_magnitude(left, right);
	return left_negative ? -magnitude : magnitude;
}

void
yt_port_mbf64_negate(uint8_t raw[8])
{
	if (raw[7] != 0U)
		raw[6] ^= 0x80U;
}
