#ifndef YT_COMMON_H
#define YT_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum yt_status {
	YT_OK = 0,
	YT_EOF,
	YT_NOT_FOUND,
	YT_INVALID,
	YT_RANGE,
	YT_IO_ERROR,
	YT_NO_MEMORY,
	YT_RANDOM_ERROR,
	YT_CHILD_ERROR
};

struct yt_error {
	enum yt_status status;
	int system_error;
	char operation[48];
	char path[512];
	uint16_t basic_fault_site;
	uint16_t basic_error;
	bool basic_fault_valid;
	bool basic_error_valid;
};

static inline void
yt_error_clear(struct yt_error *error)
{
	if (error != NULL) {
		error->status = YT_OK;
		error->system_error = 0;
		error->operation[0] = '\0';
		error->path[0] = '\0';
		error->basic_fault_site = 0U;
		error->basic_error = 0U;
		error->basic_fault_valid = false;
		error->basic_error_valid = false;
	}
}

#define YT_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
