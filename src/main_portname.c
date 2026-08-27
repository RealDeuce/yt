#include "yt_cli.h"
#include "yt_game.h"
#include "yt_init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
	struct yt_error error;
	struct yt_game game;
	struct yt_random random;
	char answer[80];
	size_t file_size;
	int count;
	int logical;

	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "ytdata.dat", YT_OPEN_UPDATE,
	    &error)) {
		if (error.status != YT_NOT_FOUND) {
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		yt_error_clear(&error);
		if (!yt_database_open(&game.database, "ytdata.dat",
		    YT_OPEN_CREATE, &error)) {
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
	}
	if (!yt_file_size(game.database.path, &file_size, &error)) {
		yt_database_close(&game.database);
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (file_size == 0) {
		yt_database_close(&game.database);
		fputs("\aERROR! DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\n",
		    stdout);
		if (!yt_file_delete("YTDATA.DAT", true, &error)) {
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
	if (!yt_config_load(&game.database, &game.config, &error)) {
		yt_database_close(&game.database);
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	count = (int)(game.config.planet_offset - game.config.port_offset);
	puts("          Yankee Trader Remote Port Rename Program");
	puts("                     By Alan Davenport");
	puts("");
	puts("This program will apply new, random port names to an existing game without");
	fputs("effecting any other setting. Do you wish to continue? [y/N] -=> ",
	    stdout);
	if (!yt_cli_line(answer, sizeof(answer))) {
		yt_database_close(&game.database);
		return EXIT_SUCCESS;
	}
	if (answer[0] == '\0') {
		yt_database_close(&game.database);
		return EXIT_SUCCESS;
	}
	if ((answer[0] & 0xdf) != 'Y') {
		puts("Aborted!");
		yt_database_close(&game.database);
		return EXIT_SUCCESS;
	}
	yt_database_close(&game.database);
	if (!yt_database_open(&game.database, "ytdata.dat", YT_OPEN_UPDATE,
	    &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	puts("Renaming ports...");
	yt_random_init(&random);
	for (logical = 1; logical <= count; ++logical) {
		struct yt_port port;
		char name[42];

		if (!yt_game_read_port(&game, logical, &port, &error))
			goto failure;
		if (logical == 1)
			strcpy(name, "Earth");
		else if (!yt_generate_port_name(&random, name, &error))
			goto failure;
		snprintf(port.name, sizeof(port.name), "%s", name);
		port.name_length = (float)strlen(name);
		if (!yt_game_write_port(&game, logical, &port, &error))
			goto failure;
		printf(" %d %s\n", logical, name);
	}
	yt_database_close(&game.database);
	puts("New, random names applied to all ports!");
	return EXIT_SUCCESS;

failure:
	yt_database_close(&game.database);
	yt_cli_error("PORTNAME", &error);
	return EXIT_FAILURE;
}
