#include "yt_cli.h"
#include "yt_init.h"
#include "yt_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct console_output {
	FILE *stream;
	size_t column;
};

static bool
console_present(void *context, enum yt_init_output_entry entry,
    const uint8_t *payload, size_t payload_length, struct yt_error *error)
{
	struct console_output *output = context;
	FILE *stream;
	size_t spaces;

	if (output == NULL)
		return false;
	stream = output->stream;
	if (entry == YT_INIT_OUTPUT_PLAY)
		return true;
	if (entry == YT_INIT_OUTPUT_LOCATE_COLUMN_ONE) {
		if (fputc('\r', stream) == EOF)
			goto failure;
		output->column = 0U;
		return true;
	}
	if (entry == YT_INIT_OUTPUT_LOCATE_ROW_25) {
		output->column = 0U;
		return true;
	}
	if (output->column != 0U && payload_length + 1U > 80U - output->column) {
		if (fputc('\n', stream) == EOF)
			goto failure;
		output->column = 0U;
	}
	if (payload_length != 0U) {
		if (fwrite(payload, 1, payload_length, stream) != payload_length)
			goto failure;
	}
	output->column = (output->column + payload_length) % 80U;
	if (entry == YT_INIT_OUTPUT_LINE) {
		if (fputc('\n', stream) == EOF)
			goto failure;
		output->column = 0U;
	}
	else if (entry == YT_INIT_OUTPUT_COMMA) {
		spaces = 14U - output->column % 14U;
		if (output->column != 0U && spaces + 14U > 80U - output->column) {
			if (fputc('\n', stream) == EOF)
				goto failure;
			output->column = 0U;
		}
		else {
			while (spaces-- != 0U) {
				if (fputc(' ', stream) == EOF)
					goto failure;
				++output->column;
			}
		}
	}
	return true;

failure:
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 0;
		snprintf(error->operation, sizeof(error->operation),
		    "write YT-INIT console");
		error->path[0] = '\0';
	}
	return false;
}

int
main(int argc, char **argv)
{
	struct yt_error error;
	struct yt_random random;
	struct yt_initializer_preparation preparation;
	struct yt_database database = {0};
	struct console_output console = {stdout, 0U};
	const struct yt_init_presenter presenter = {
	    .context = &console,
	    .write = console_present
	};
	char answer[80];
	char scoreboard[80];
	char executable[1024];
	char maintenance[1024];
	char *maintenance_argv[2];

	(void)argc;
	yt_error_clear(&error);
	yt_random_init(&random);
	yt_initializer_layout_yt(&preparation);
	if (!yt_init_present_confirmation_prefix(&presenter, &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_cli_line(answer, sizeof(answer)))
		return EXIT_SUCCESS;
	if (!yt_initializer_confirm_response(answer))
		return EXIT_SUCCESS;
	if (!yt_init_present_opening(&presenter, &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_initialize_begin_yt(&error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_initialize_bind_yt(&database, &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_initializer_prepare_yt(NULL, &random, &preparation, &error)) {
		yt_database_close(&database);
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_init_present_prepared_configuration(&preparation,
	    &presenter, &error)) {
		yt_database_close(&database);
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_cli_line(scoreboard, sizeof(scoreboard))) {
		yt_database_close(&database);
		return EXIT_SUCCESS;
	}
	if (scoreboard[0] == '\0')
		strcpy(scoreboard, "YTSCORE.ASC");
	if (!yt_initialize_yt_prepared_bound(&database, &preparation, scoreboard,
	    NULL, &random,
	    &presenter, &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_platform_executable_path(executable, sizeof(executable),
	    argv[0], &error)) {
		yt_cli_error("YT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_platform_sibling_program(maintenance, sizeof(maintenance),
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
