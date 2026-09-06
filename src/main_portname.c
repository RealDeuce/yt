#include "yt_cli.h"
#include "yt_game.h"
#include "yt_init.h"
#include "yt_portname.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
write_output(void *context, const uint8_t *data, size_t length,
    struct yt_error *error)
{
	FILE *stream = context;

	if (length != 0U && fwrite(data, 1U, length, stream) != length) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "write PORTNAME console");
		}
		return false;
	}
	return true;
}

static bool
write_composed(enum yt_portname_output_kind kind, struct yt_error *error)
{
	struct yt_portname_output output;

	return yt_portname_compose_output(kind, 0.0f, NULL, 0U, &output)
	    && write_output(stdout, output.bytes, output.length, error);
}

static bool
portname_close_all(struct yt_game *game, struct yt_error *error)
{
	struct yt_close_all_control control = {
		.heap_type = YT_CLOSE_ALL_HEAP_FILE,
		.file_class = 0,
		.method = yt_database_close_all_method,
		.context = &game->database,
	};
	size_t control_count = game->database.file != NULL ? 1U : 0U;

	return yt_close_all_run(control_count != 0U ? &control : NULL,
	    control_count, NULL, NULL, error);
}

static bool
read_confirmation(uint8_t value[256], size_t *value_length,
    struct yt_error *error)
{
	char input[256];

	for (;;) {
		enum yt_portname_parse_result parsed;

		if (!write_output(stdout, (const uint8_t *)"? ", 2U, error))
			return false;
		if (!yt_cli_line(input, sizeof(input)))
			return false;
		if (!write_output(stdout, (const uint8_t *)"\r", 1U, error))
			return false;
		parsed = yt_portname_parse_confirmation((const uint8_t *)input,
		    strlen(input), value, 256U, value_length);
		if (parsed == YT_PORTNAME_PARSE_VALID)
			return true;
		if (parsed == YT_PORTNAME_PARSE_RANGE) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation),
				    "parse PORTNAME confirmation");
			}
			return false;
		}
		if (!write_output(stdout,
		    (const uint8_t *)"?Redo from start\r", 17U, error))
			return false;
	}
}

int
main(void)
{
	struct yt_error error;
	struct yt_game game;
	struct yt_random random;
	struct yt_portname_result rename_result;
	uint8_t answer[256];
	size_t answer_length;
	uint32_t file_size;

	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	yt_random_init(&random);
	/* The shipped random-file opener begins with CLOSE #1. */
	if (!yt_database_random_close(&game.database, &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (!yt_database_open(&game.database, "YTDATA.DAT",
	    YT_OPEN_UPDATE_CREATE, &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (!yt_database_random_lof(&game.database, &file_size, &error)) {
		yt_database_close(&game.database);
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (file_size == 0) {
		if (!write_composed(YT_PORTNAME_OUTPUT_MISSING_DATA, &error)
		    || !portname_close_all(&game, &error)
		    || !yt_file_kill("YTDATA.DAT", NULL, &error)) {
			yt_database_close(&game.database);
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
	if (!write_composed(YT_PORTNAME_OUTPUT_INTRO, &error)
	    || !read_confirmation(answer, &answer_length, &error)) {
		yt_database_close(&game.database);
		if (error.status != YT_OK) {
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS; /* physical EOF outside ordinary INPUT */
	}
	if (yt_portname_confirm(answer, answer_length)
	    == YT_PORTNAME_CONFIRM_BLANK) {
		if (!portname_close_all(&game, &error)) {
			yt_database_close(&game.database);
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
	if (yt_portname_confirm(answer, answer_length)
	    == YT_PORTNAME_CONFIRM_REJECT) {
		if (!write_composed(YT_PORTNAME_OUTPUT_ABORT, &error)) {
			yt_database_close(&game.database);
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		if (!portname_close_all(&game, &error)) {
			yt_database_close(&game.database);
			yt_cli_error("PORTNAME", &error);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
	if (!yt_database_random_close(&game.database, &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	/* The shared opener redundantly closes file 1 before reopening it. */
	if (!yt_database_random_close(&game.database, &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (!yt_database_open(&game.database, "YTDATA.DAT",
	    YT_OPEN_UPDATE_CREATE,
	    &error)) {
		yt_cli_error("PORTNAME", &error);
		return EXIT_FAILURE;
	}
	if (!yt_portname_rename(&game.database, game.config.port_offset,
	    game.config.planet_offset, &random, write_output, stdout,
	    &rename_result, &error))
		goto failure;
	/* 02A3 CLOSE-all sees the database already closed, then reaches PLAY. */
	if (!portname_close_all(&game, &error))
		goto failure;
	return EXIT_SUCCESS;

failure:
	yt_database_close(&game.database);
	yt_cli_error("PORTNAME", &error);
	return EXIT_FAILURE;
}
