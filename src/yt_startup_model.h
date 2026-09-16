#ifndef YT_STARTUP_MODEL_H
#define YT_STARTUP_MODEL_H

#include "yt_common.h"

struct yt_startup_serial_layout {
	int port;
	int brun_device;
	uint16_t uart_base;
	uint16_t modem_status_port;
	uint16_t bios_address;
	uint16_t bios_value;
};

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

int yt_startup_parse_port(const uint8_t *identifier, size_t length);
bool yt_startup_serial_layout(int port,
    struct yt_startup_serial_layout *layout);
bool yt_startup_detect_baud(uint8_t dll, uint8_t dlm, float *baud);
bool yt_startup_divisor_from_observed_baud(uint32_t baud, uint8_t *dll,
    uint8_t *dlm);
bool yt_startup_framing_compose(const uint8_t *description,
    size_t description_length, struct yt_startup_framing *framing);
bool yt_startup_open_spec(int port, const uint8_t *description,
    size_t description_length, uint8_t *spec, size_t capacity,
    size_t *spec_length);
bool yt_startup_restored_divisor(float baud, uint8_t *dll, uint8_t *dlm);
bool yt_startup_canonical_name(const uint8_t *first, size_t first_length,
    const uint8_t *last, size_t last_length, uint8_t *name,
    size_t capacity, size_t *name_length);
#endif
