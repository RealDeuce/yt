#ifndef YT_PLATFORM_H
#define YT_PLATFORM_H

#include "yt_common.h"
#include "yt_startup_model.h"

#include <time.h>

struct yt_clock_value {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int hundredth;
};

typedef bool (*yt_clock_read_fn)(void *context, struct yt_clock_value *value,
    struct yt_error *error);

struct yt_clock {
	yt_clock_read_fn read;
	void *context;
};

enum yt_spawn_mode {
	YT_SPAWN_WAIT,
	YT_SPAWN_REPLACE
};

#define YT_PLATFORM_RMT_SERIAL_PRIVATE 128U

struct yt_platform_rmt_serial {
	uintptr_t native_handle;
	uint32_t observed_baud;
	bool owns_handle;
	bool prepared;
	bool restored;
	uint8_t private_state[YT_PLATFORM_RMT_SERIAL_PRIVATE];
};

bool yt_platform_entropy(void *buffer, size_t length, struct yt_error *error);
bool yt_clock_read(const struct yt_clock *clock, struct yt_clock_value *value,
    struct yt_error *error);
double yt_clock_timer(const struct yt_clock *clock);
bool yt_platform_executable_path(char *dest, size_t size, const char *argv0,
    struct yt_error *error);
bool yt_platform_sibling_program(char *dest, size_t size,
    const char *executable_path, const char *program, struct yt_error *error);
bool yt_platform_spawn(const char *program, char *const argv[],
    enum yt_spawn_mode mode, int *exit_code, struct yt_error *error);
bool yt_platform_rmt_serial_prepare(int port,
    const struct yt_startup_framing *framing,
    struct yt_platform_rmt_serial *serial, struct yt_error *error);
bool yt_platform_rmt_serial_restore(
    struct yt_platform_rmt_serial *serial, struct yt_error *error);
void yt_platform_rmt_serial_close(struct yt_platform_rmt_serial *serial);

#endif
