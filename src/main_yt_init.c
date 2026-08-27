#include "yt_cli.h"
#include "yt_init.h"
#include "yt_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
	struct yt_error error;
	struct yt_random random;
	char answer[80];
	char scoreboard[80];
	char executable[1024];
	char maintenance[1024];
	char *maintenance_argv[2];

	(void)argc;
	puts("            Yankee Trader Initialization Program");
	puts("                     By Alan Davenport");
	puts("This program will initialize Yankee Trader. You must run this program at");
	puts("least once when you start up the game. If this program is run on an");
	puts("existing game, the old game will be wiped out and be replaced by a new one.");
	fputs("Continue (Y/N)? ", stdout);
	if (!yt_cli_line(answer, sizeof(answer))
	    || !((answer[0] == 'Y' || answer[0] == 'y')
	    && answer[1] == '\0'))
		return EXIT_SUCCESS;
	puts("Creating main data file: YTDATA.DAT");
	yt_error_clear(&error);
	if (!yt_initialize_begin_yt(&error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	puts("Please input filename for the Scoreboard Bulletin.");
	puts("Include FULL PATH and NAME of file! ([ENTER] for YTSCORE.ASC) : ");
	fputs("-=> ", stdout);
	if (!yt_cli_line(scoreboard, sizeof(scoreboard)))
		return EXIT_SUCCESS;
	if (scoreboard[0] == '\0')
		strcpy(scoreboard, "YTSCORE.ASC");
	yt_random_init(&random);
	{
		struct yt_initializer_options options = {
		    .family = YT_INITIALIZER_YT,
		    .scoreboard = scoreboard,
		    .database_already_truncated = true
		};

		if (!yt_initialize_world(&options, &random, &error)) {
			yt_cli_error("YT-INIT", &error);
			return EXIT_FAILURE;
		}
	}
	puts("Initialization completed sucessfully!");
	puts("<YT-INIT Normal Termination>");
	puts("Be SURE to run YTMAINT.EXE at LEAST ONCE per day EVERY DAY!");
	puts("Run YTCONFIG and change the default OPTIONS if you wish!");
	puts("Running initial maintenance...");
	if (!yt_platform_executable_path(executable, sizeof(executable),
	    argv[0], &error)
	    || !yt_platform_sibling_program(maintenance, sizeof(maintenance),
	    executable, "ytmaint", &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	maintenance_argv[0] = maintenance;
	maintenance_argv[1] = NULL;
	if (fflush(NULL) != 0) {
		fputs("YT-INIT: unable to flush output before maintenance.\n",
		    stderr);
		return EXIT_FAILURE;
	}
	if (!yt_platform_spawn(maintenance, maintenance_argv,
	    YT_SPAWN_REPLACE, NULL, &error)) {
		yt_cli_error("YT-INIT/YTMAINT", &error);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS; /* Unreachable after a successful RUN replacement. */
}
