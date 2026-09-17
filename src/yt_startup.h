#ifndef YT_STARTUP_H
#define YT_STARTUP_H

#include "yt_common.h"

enum yt_startup_parity {
	YT_STARTUP_PARITY_NONE,
	YT_STARTUP_PARITY_EVEN
};

struct yt_startup_framing {
	uint32_t opening_baud;
	enum yt_startup_parity parity;
	uint8_t data_bits;
	uint8_t stop_bits;
};

bool yt_startup_framing_compose(const uint8_t *description,
    size_t description_length, struct yt_startup_framing *framing);
bool yt_startup_canonical_name(const uint8_t *first, size_t first_length,
    const uint8_t *last, size_t last_length, uint8_t *name,
    size_t capacity, size_t *name_length);
#endif
