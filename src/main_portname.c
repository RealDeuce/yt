#include "qb.h"
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
write_composed_values(enum yt_portname_output_kind kind, float logical_port,
    const uint8_t *name, size_t name_length, struct yt_error *error)
{
	struct yt_portname_output output;

	return yt_portname_compose_output(kind, logical_port, name, name_length,
	    &output)
	    && write_output(stdout, output.bytes, output.length, error);
}

static bool
write_composed(enum yt_portname_output_kind kind, struct yt_error *error)
{
	return write_composed_values(kind, 0.0f, NULL, 0U, error);
}

static bool
rename_ports(struct yt_game *game, struct yt_random *random,
    struct yt_error *error)
{
	float logical = 1.0f;
	float loop_bound = qb_single_subtract(game->config.planet_offset,
	    game->config.port_offset);

	if (!write_composed(YT_PORTNAME_OUTPUT_RENAMING, error))
		return false;
	while (logical <= loop_bound) {
		struct yt_record record;
		char generated[42];
		const uint8_t *name = (const uint8_t *)"Earth";
		size_t name_length = 5U;
		uint32_t physical;
		float next;

		if (logical != 1.0f) {
			if (!yt_generate_port_name(random, generated, error))
				return false;
			name = (const uint8_t *)generated;
			name_length = strlen(generated);
		}
		if (!write_composed_values(YT_PORTNAME_OUTPUT_PROGRESS, logical,
		    name, name_length, error))
			return false;
		physical = yt_portname_record_number(game->config.port_offset,
		    logical);
		if (!yt_database_read(&game->database, (size_t)physical, &record,
		    error)
		    || !yt_portname_overlay_record(&record, name, name_length, error)
		    || !yt_database_write(&game->database, (size_t)physical,
		    &record, error))
			return false;
		next = qb_single_add(logical, 1.0f);
		if (next == logical) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation),
				    "PORTNAME FOR variable stalled");
				error->path[0] = '\0';
			}
			return false;
		}
		logical = next;
	}
	return write_composed(YT_PORTNAME_OUTPUT_COMPLETE, error)
	    && yt_database_random_close(&game->database, error);
}

static bool
portname_close_all(struct yt_game *game, struct yt_error *error)
{
	return game->database.file == NULL
	    || yt_database_close_all_single(&game->database, error);
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
		    || !yt_file_kill("YTDATA.DAT", &error)) {
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
	if (!rename_ports(&game, &random, &error))
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
