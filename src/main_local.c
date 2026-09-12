#include "qb.h"
#include "yt_cli.h"
#include "yt_file.h"
#include "yt_names.h"
#include "yt_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
join_command(int argc, char **argv, char *command, size_t size)
{
	size_t used = 0;
	int index;

	command[0] = '\0';
	for (index = 1; index < argc; ++index) {
		int count = snprintf(command + used, size - used, "%s%s",
		    used == 0 ? "" : " ", argv[index]);

		if (count < 0 || (size_t)count >= size - used)
			return false;
		used += (size_t)count;
	}
	return true;
}

static bool
load_choices(struct yt_name_file *names, struct yt_error *error)
{
	FILE *created;
	size_t size;

	if (!yt_file_size("YTNAME.DAT", &size, error)) {
		if (error->status != YT_NOT_FOUND)
			return false;
		created = fopen("YTNAME.DAT", "ab");
		if (created == NULL)
			return false;
		fclose(created);
		size = 0;
	}
	if (size == 0) {
		(void)yt_file_kill("YTNAME.DAT", error);
		return false;
	}
	return yt_names_load("YTNAME.DAT", names, error);
}

static void
list_choices(const struct yt_name_file *names)
{
	size_t index;

	for (index = 1; index <= names->count; ++index) {
		const char *first = "";
		const char *last = "";

		if (index < names->count) {
			first = names->rows[index].real_first;
			last = names->rows[index].real_last;
		}
		if (first[0] != '\0' || last[0] != '\0')
			printf(" %zu]%s %s\n", index, first, last);
		if (index % 15U == 0) {
			fputs("[Pause]", stdout);
			(void)yt_cli_key();
			fputs("\033[2J\033[H", stdout);
		}
	}
}

static bool
select_name(const struct yt_name_file *names, char *command, size_t size)
{
	for (;;) {
		struct qb_val_result parsed;

		if (command[0] == '\0')
			list_choices(names);
		fputs(names->count > 1
		    ? "Enter user number, LIST, or Your REAL name. -=> "
		    : "Enter Your REAL name. -=> ", stdout);
		if (!yt_cli_line(command, size))
			return false;
		if (command[0] == '\0') {
			puts("Aborted!");
			return false;
		}
		if (strcmp(command, "list") == 0 || strcmp(command, "LIST") == 0) {
			command[0] = '\0';
			continue;
		}
		parsed = qb_val(command);
		if (parsed.valid && parsed.value != 0.0) {
			bool overflow;
			int selected;

			if (parsed.value <= 0.0
			    || parsed.value > (double)names->count) {
				command[0] = '\0';
				continue;
			}
			selected = (int)qb_cint((double)(float)parsed.value,
			    &overflow);
			if (overflow || selected < 0
			    || (size_t)selected >= names->count) {
				command[0] = '\0';
				continue;
			}
			snprintf(command, size, "%s %s",
			    names->rows[selected].real_first,
			    names->rows[selected].real_last);
		}
		return true;
	}
}

int
main(int argc, char **argv)
{
	struct yt_name_file names;
	struct yt_error error;
	char command[512];
	char first[256];
	char last[256];
	char executable[1024];
	char child[1024];
	char *child_argv[3];
	int child_status;

	yt_error_clear(&error);
	if (!join_command(argc, argv, command, sizeof(command))) {
		fprintf(stderr, "LOCAL: command line too long\n");
		return EXIT_FAILURE;
	}
	if (!load_choices(&names, &error)) {
		yt_cli_error("LOCAL", &error);
		return EXIT_FAILURE;
	}
	if (command[0] == '\0'
	    && !select_name(&names, command, sizeof(command))) {
		yt_names_free(&names);
		return EXIT_SUCCESS;
	}
	yt_names_free(&names);
	qb_trim(command);
	qb_collapse_spaces(command);
	qb_ascii_upper(command);
	yt_names_split(command, first, sizeof(first), last, sizeof(last));
	puts("Loading...");
	if (!yt_cli_write_dorinfo(first, last, &error)
	    || !yt_platform_executable_path(executable, sizeof(executable),
	    argv[0], &error)
	    || !yt_platform_sibling_program(child, sizeof(child), executable,
	    "yt", &error)) {
		yt_cli_error("LOCAL", &error);
		return EXIT_FAILURE;
	}
	child_argv[0] = child;
	child_argv[1] = "DORINFO1.DEF";
	child_argv[2] = NULL;
	if (!yt_platform_spawn(child, child_argv, YT_SPAWN_WAIT,
	    &child_status, &error)) {
		yt_cli_error("LOCAL", &error);
		return EXIT_FAILURE;
	}
	(void)child_status;
	fputs("\033[25;1H", stdout);
	return EXIT_SUCCESS;
}
