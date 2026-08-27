#ifndef YT_PLATFORM_H
#define YT_PLATFORM_H

#include "yt_common.h"

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

typedef bool (*yt_clock_provider)(void *context, struct yt_clock_value *value,
    struct yt_error *error);

enum yt_spawn_mode {
	YT_SPAWN_WAIT,
	YT_SPAWN_REPLACE
};

bool yt_platform_entropy(void *buffer, size_t length, struct yt_error *error);
void yt_platform_set_clock_provider(yt_clock_provider provider, void *context);
bool yt_platform_clock(struct yt_clock_value *value, struct yt_error *error);
double yt_platform_timer(void);
bool yt_platform_executable_path(char *dest, size_t size, const char *argv0,
    struct yt_error *error);
bool yt_platform_sibling_program(char *dest, size_t size,
    const char *executable_path, const char *program, struct yt_error *error);
bool yt_platform_spawn(const char *program, char *const argv[],
    enum yt_spawn_mode mode, int *exit_code, struct yt_error *error);
void yt_platform_delay(unsigned milliseconds);

#endif
