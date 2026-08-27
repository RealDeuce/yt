#include "qb.h"
#include "yt_cli.h"
#include "yt_config.h"
#include "yt_file.h"
#include "yt_init.h"
#include "yt_names.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
read_handoff(char path[512], bool *standalone, struct yt_error *error)
{
	struct yt_text_file text;
	FILE *file;
	char resolved[512];
	size_t size;
	size_t length;

	path[0] = '\0';
	if (!yt_resolve_case_path("rmtinit.tmp", true, resolved,
	    sizeof(resolved), error))
		return false;
	file = fopen(resolved, "ab");
	if (file == NULL)
		return false;
	fclose(file);
	if (!yt_file_size("rmtinit.tmp", &size, error))
		return false;
	*standalone = size == 0;
	if (*standalone)
		return true;
	if (!yt_text_read("rmtinit.tmp", &text, error))
		return false;
	length = 0;
	while (length < text.length && text.data[length] != '\r'
	    && text.data[length] != '\n' && text.data[length] != 0x1a)
		++length;
	if (length >= 512U) {
		yt_text_free(&text);
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	memcpy(path, text.data, length);
	path[length] = '\0';
	yt_text_free(&text);
	return true;
}

static bool
read_dorinfo_name(const char *path, char first[128], char last[128],
    bool *local_mode, struct yt_error *error)
{
	char resolved[512];
	FILE *file;
	char lines[8][256];
	int index;

	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "rb");
	if (file == NULL)
		return false;
	for (index = 0; index < 8; ++index) {
		size_t length;

		if (fgets(lines[index], sizeof(lines[index]), file) == NULL) {
			fclose(file);
			if (error != NULL)
				error->status = YT_EOF;
			return false;
		}
		length = strlen(lines[index]);
		while (length > 0 && (lines[index][length - 1] == '\r'
		    || lines[index][length - 1] == '\n'
		    || lines[index][length - 1] == 0x1a))
			lines[index][--length] = '\0';
	}
	fclose(file);
	{
		char full[256];
		size_t length = strlen(lines[3]);
		int port = length > 0 ? lines[3][length - 1] - '0' : 0;

		if (length > 0 && lines[3][length - 1U] == ':') {
			lines[3][--length] = '\0';
			port = length > 0 ? lines[3][length - 1U] - '0' : 0;
		}
		snprintf(full, sizeof(full), "%s %s", lines[6], lines[7]);
		qb_title_case(full);
		yt_names_split(full, first, 128, last, 128);
		*local_mode = port < 1 || port > 4;
	}
	return true;
}

static bool
preprocess_old_database(struct yt_database *database,
    struct yt_config *config, struct yt_error *error)
{
	int basic;

	if (config->headquarters == 0.0f) {
		config->headquarters = 85.0f;
		yt_record_set_number(&config->record, YT_F117, 85.0f);
		if (!yt_database_write(database, 1, &config->record, error))
			return false;
	}
	for (basic = 2; basic <= (int)config->sector_offset; ++basic) {
		struct yt_record record;
		float cloak;

		if (!yt_database_read(database, (size_t)basic, &record, error))
			return false;
		cloak = yt_record_get_number(&record, YT_F125);
		if (cloak > 0.0f) {
			record.bytes[YT_F125 + 2U] ^= 0x80U;
			if (!yt_database_write(database, (size_t)basic, &record,
			    error))
				return false;
		}
	}
	return yt_database_flush(database, error);
}

static void
transform_config(struct yt_config *config, bool local_mode)
{
	if (config->scoreboard[0] == '\0')
		strcpy(config->scoreboard, "NUL");
	if (config->local_screen < -1.0f || config->local_screen > 0.0f
	    || local_mode)
		config->local_screen = -1.0f;
	if (config->lottery_plays < 1.0f)
		config->lottery_plays = 1.0f;
	if (config->genesis_ports < 20.0f || config->genesis_ports > 300.0f)
		config->genesis_ports = 200.0f;
	if (config->maximum_holds < 5.0f
	    || config->maximum_holds > 1000.0f)
		config->maximum_holds = 50.0f;
	config->marker = 6324.0f;
	config->maximum_planets = 0.0f;
}

static bool
credited_remote_name(const char *first, const char *last, char credited[90],
    struct yt_error *error)
{
	struct yt_name_file names;
	const struct yt_name_row *match;

	credited[0] = '\0';
	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	match = NULL;
	for (size_t index = 0; index < names.count; ++index) {
		if (strcmp(names.rows[index].real_first, first) == 0
		    && strcmp(names.rows[index].real_last, last) == 0)
			match = &names.rows[index];
	}
	if (match != NULL)
		snprintf(credited, 90, "%s %s", match->alias_first,
		    match->alias_last);
	yt_names_free(&names);
	return true;
}

int
main(void)
{
	struct yt_error error;
	struct yt_database old;
	struct yt_config config;
	struct yt_random random;
	char handoff[512];
	char answer[80];
	char first[128] = "";
	char last[128] = "";
	char credited[90];
	bool standalone;
	bool local_mode;
	size_t old_size;

	yt_error_clear(&error);
	memset(&old, 0, sizeof(old));
	if (!read_handoff(handoff, &standalone, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (standalone) {
		puts("");
		puts("");
		puts("Running stand alone... re-initializing using old sysop defined defaults.");
		puts("");
		fputs("Do you wish to re-init Y.T. using your old default values?",
		    stdout);
		if (!yt_cli_line(answer, sizeof(answer))
		    || !((answer[0] == 'Y' || answer[0] == 'y')
		    && answer[1] == '\0'))
			return EXIT_SUCCESS;
		local_mode = true;
		strcpy(credited, "The Sysop");
	}
	else {
		local_mode = false;
		credited[0] = '\0';
	}
	if (!yt_file_delete("rmtinit.tmp", false, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!standalone
	    && (!read_dorinfo_name(handoff, first, last, &local_mode, &error)
	    || !credited_remote_name(first, last, credited, &error))) {
		yt_cli_error("RMT-INIT", &error);
		return EXIT_FAILURE;
	}
	if (!yt_file_size("YTDATA.DAT", &old_size, &error) || old_size == 0) {
		if (error.status == YT_NOT_FOUND) {
			FILE *empty = fopen("YTDATA.DAT", "wb");

			if (empty != NULL)
				fclose(empty);
		}
		(void)yt_file_delete("YTDATA.DAT", true, NULL);
		fputs("\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a\n",
		    stdout);
		return EXIT_FAILURE;
	}
	if (!yt_database_open(&old, "YTDATA.DAT", YT_OPEN_UPDATE, &error)
	    || !yt_config_load(&old, &config, &error)
	    || !preprocess_old_database(&old, &config, &error)) {
		yt_database_close(&old);
		yt_cli_error("RMT-INIT", &error);
		return EXIT_FAILURE;
	}
	yt_database_close(&old);
	transform_config(&config, local_mode || standalone);
	yt_random_init(&random);
	if (!yt_initialize_rmt(&config, credited, &random, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return EXIT_FAILURE;
	}
	fputs("\aInitialization completed sucessfully!\a\n", stdout);
	if (strcmp(credited, "The Sysop") != 0) {
		for (int repeat = 0; repeat < 3; ++repeat)
			printf("Congratulations %s! You have fulfilled the prophesy!!\n",
			    credited);
	}
	if (!local_mode)
		puts("Returning you to the BBS...");
	return EXIT_SUCCESS;
}
