#include "yt_cli.h"
#include "yt_maint.h"

#include <stdlib.h>

int
main(void)
{
	struct yt_error error;

	yt_error_clear(&error);
	if (!yt_maintenance_run(&error)) {
		yt_cli_error("YTMAINT", &error);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
