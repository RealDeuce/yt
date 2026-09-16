#include "yt_game.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
#define yt_chdir _chdir
#define yt_getcwd _getcwd
#define yt_mkdir(path) _mkdir(path)
#define yt_rmdir _rmdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define yt_chdir chdir
#define yt_getcwd getcwd
#define yt_rmdir rmdir
#endif

#ifndef YT_INIT_EXE
#error "YT_INIT_EXE must name the yt-init executable under test"
#endif

#define PACKAGE_FILLER_COUNT 256U

static int
fail(const char *message)
{
	fprintf(stderr, "test_clean_install: %s\n", message);
	return EXIT_FAILURE;
}

static bool
file_size_is(const char *path, long expected)
{
	FILE *file = fopen(path, "rb");
	long length;

	if (file == NULL)
		return false;
	if (fseek(file, 0, SEEK_END) != 0
	    || (length = ftell(file)) < 0
	    || fclose(file) != 0)
		return false;
	return length == expected;
}

static bool
text_has_dos_eof(const char *path, size_t minimum_length)
{
	struct yt_text_file text;
	struct yt_error error;
	bool result;

	yt_error_clear(&error);
	if (!yt_text_read(path, &text, &error))
		return false;
	result = text.length >= minimum_length
	    && text.data[text.length - 1U] == 0x1a;
	yt_text_free(&text);
	return result;
}

static bool
output_find(const struct yt_text_file *text, const char *needle,
    size_t *position)
{
	size_t needle_length = strlen(needle);
	size_t index;

	if (needle_length > text->length)
		return false;
	for (index = 0U; index <= text->length - needle_length; ++index) {
		if (memcmp(text->data + index, needle, needle_length) == 0) {
			if (position != NULL)
				*position = index;
			return true;
		}
	}
	return false;
}

static bool
output_contains(const char *needle)
{
	struct yt_text_file text;
	struct yt_error error;
	bool result;

	yt_error_clear(&error);
	if (!yt_text_read("output.txt", &text, &error))
		return false;
	result = output_find(&text, needle, NULL);
	yt_text_free(&text);
	return result;
}

static bool
output_occurs_before(const char *first, const char *second)
{
	struct yt_text_file text;
	struct yt_error error;
	size_t first_at;
	size_t second_at;
	bool result;

	yt_error_clear(&error);
	if (!yt_text_read("output.txt", &text, &error))
		return false;
	result = output_find(&text, first, &first_at)
	    && output_find(&text, second, &second_at) && first_at < second_at;
	yt_text_free(&text);
	return result;
}

static bool
populate_package_directory(void)
{
	static const char *const names[] = {
		"local", "local.freebsd.amd64.opt",
		"portname", "portname.freebsd.amd64.opt",
		"rmt-init", "rmt-init.freebsd.amd64.opt",
		"yt", "yt.freebsd.amd64.opt",
		"yt-init", "yt-init.freebsd.amd64.opt",
		"ytconfig", "ytconfig.freebsd.amd64.opt",
		"ytmaint", "ytmaint.freebsd.amd64.opt",
		"LOCKOUT.DAT", "XANNORHQ.TXT", "YT.REG", "YTECHO.TXT",
		"YTINSTR.DOC", "YTOPEN.ANS", "YTOPEN.ASC", "YTSYSOP.DOC"
	};
	size_t index;
	unsigned filler;

	for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
		FILE *file = fopen(names[index], "wb");

		if (file == NULL || fclose(file) != 0)
			return false;
	}
	for (filler = 0; filler < PACKAGE_FILLER_COUNT; ++filler) {
		char name[40];
		FILE *file;

		if (snprintf(name, sizeof(name), "package-filler-%03u",
		    filler) < 0)
			return false;
		file = fopen(name, "wb");
		if (file == NULL || fclose(file) != 0)
			return false;
	}
	return true;
}

static void
cleanup_files(void)
{
	static const char *const paths[] = {
		"YTDATA.DAT", "YTNAME.DAT", "YTNEWS.DAT", "YTSCORE.ASC",
		"YTYNEWS.DAT", "YTRMSG.DAT",
		"YTTEMP",
		"TEMP", "TEMPWORK",
		"input.txt", "output.txt",
		"local", "local.freebsd.amd64.opt",
		"portname", "portname.freebsd.amd64.opt",
		"rmt-init", "rmt-init.freebsd.amd64.opt",
		"yt", "yt.freebsd.amd64.opt",
		"yt-init", "yt-init.freebsd.amd64.opt",
		"ytconfig", "ytconfig.freebsd.amd64.opt",
		"ytmaint", "ytmaint.freebsd.amd64.opt",
		"LOCKOUT.DAT", "XANNORHQ.TXT", "YT.REG", "YTECHO.TXT",
		"YTINSTR.DOC", "YTOPEN.ANS", "YTOPEN.ASC", "YTSYSOP.DOC"
	};
	size_t index;
	unsigned filler;

	for (index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index)
		(void)remove(paths[index]);
	for (filler = 0; filler < PACKAGE_FILLER_COUNT; ++filler) {
		char name[40];

		if (snprintf(name, sizeof(name), "package-filler-%03u",
		    filler) >= 0)
			(void)remove(name);
	}
}

int
main(void)
{
	char original[1024];
	char directory[1024];
	char command[4096];
	FILE *input;
	struct yt_game game;
	struct yt_error error;
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_planet planet;
	int status;
	int basic;
	int result = EXIT_FAILURE;
	const char *failure = "unspecified failure";

	if (yt_getcwd(original, sizeof(original)) == NULL)
		return fail("cannot determine original directory");
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "%s\\yt-clean-install-%lu",
	    original, (unsigned long)_getpid());
	if (yt_mkdir(directory) != 0)
		return fail("cannot create temporary directory");
#else
	snprintf(directory, sizeof(directory),
	    "/tmp/yt-clean-install.XXXXXX");
	if (mkdtemp(directory) == NULL)
		return fail("cannot create temporary directory");
#endif
	if (yt_chdir(directory) != 0)
		return fail("cannot enter temporary directory");
	if (!populate_package_directory()) {
		failure = "cannot populate package-like install directory";
		goto done;
	}
	if (snprintf(command, sizeof(command),
	    "\"%s\" < input.txt > output.txt 2>&1", YT_INIT_EXE) < 0) {
		failure = "cannot format yt-init command";
		goto done;
	}
	input = fopen("input.txt", "wb");
	if (input == NULL || fwrite("N\n", 1, 2, input) != 2
	    || fclose(input) != 0) {
		failure = "cannot prepare declined initializer input";
		goto done;
	}
	status = system(command);
	if (status != 0 || file_size_is("YTDATA.DAT", 0L)) {
		failure = "declined initialization mutated YTDATA.DAT";
		goto done;
	}
	input = fopen("input.txt", "wb");
	if (input == NULL) {
		failure = "cannot create redirected input";
		goto done;
	}
	if (fwrite("Y\n\n", 1, 3, input) != 3) {
		failure = "cannot write redirected input";
		(void)fclose(input);
		goto done;
	}
	if (fclose(input) != 0) {
		failure = "cannot close redirected input";
		goto done;
	}
	status = system(command);
	if (status != 0) {
		failure = "yt-init or its ytmaint replacement returned failure";
		goto done;
	}

	if (!file_size_is("YTDATA.DAT", 432235L)) {
		failure = "YTDATA.DAT is absent or not 432235 bytes";
		goto done;
	}
	if (!file_size_is("YTNAME.DAT", 26L)) {
		failure = "YTNAME.DAT is absent or not 26 bytes";
		goto done;
	}
	if (!file_size_is("YTSCORE.ASC", 603L)) {
		failure = "YTSCORE.ASC is absent or not 603 bytes";
		goto done;
	}
	if (!file_size_is("YTYNEWS.DAT", 309L)) {
		failure = "YTYNEWS.DAT is absent or not 309 bytes";
		goto done;
	}
	if (!file_size_is("YTRMSG.DAT", 0L)) {
		failure = "YTRMSG.DAT is absent or not empty";
		goto done;
	}
	if (!text_has_dos_eof("YTNAME.DAT", 26U)) {
		failure = "YTNAME.DAT lacks its DOS EOF marker";
		goto done;
	}
	if (!text_has_dos_eof("YTNEWS.DAT", 2U)) {
		failure = "YTNEWS.DAT lacks its DOS EOF marker";
		goto done;
	}
	if (!text_has_dos_eof("YTSCORE.ASC", 603U)) {
		failure = "YTSCORE.ASC lacks its DOS EOF marker";
		goto done;
	}
	if (!text_has_dos_eof("YTYNEWS.DAT", 309U)) {
		failure = "YTYNEWS.DAT lacks its DOS EOF marker";
		goto done;
	}
	if (!output_contains(
	    "            Yankee Trader Initialization Program")) {
		failure = "redirected output lacks the YT-INIT banner";
		goto done;
	}
	if (!output_contains("Initialization completed sucessfully!")) {
		failure = "redirected output lost the YT-INIT completion message";
		goto done;
	}
	if (!output_contains("<YT-INIT Normal Termination>")) {
		failure = "redirected output lacks the YT-INIT termination message";
		goto done;
	}
	if (!output_contains("Running initial maintenance...")) {
		failure = "redirected output lacks the maintenance handoff message";
		goto done;
	}
	if (!output_contains(
	    "\nYankee Trader Maintenance program\n"
	    "        by Alan Davenport\n\n"
	    "       (Revision 03/14/94)\n\n"
	    "This should be run once per day.\n\n"
	    "Compressing Message Base's\n\n"
	    "Loading players, deleting inactive players and subtracting cloak "
	    "charge.\n\n\nRunning port maintenance...\n")) {
		failure = "redirected output differs from YTMAINT entry prefix";
		goto done;
	}
	if (!output_contains("Daily Maintenance Completed OK")) {
		failure = "redirected output lacks YTMAINT completion";
		goto done;
	}
	if (!output_occurs_before("# of turns per day:",
	    "# of times per day a user may play the lottery:")
	    || !output_occurs_before(
	    "# of times per day a user may play the lottery:",
	    "Xannor Headquarters placed in sector:")
	    || !output_occurs_before(
	    "Xannor Headquarters placed in sector:",
	    "Please input filename for the Scoreboard Bulletin.")) {
		failure = "redirected output reordered YT-INIT live defaults";
		goto done;
	}
	if (!output_occurs_before("Initialization completed sucessfully!",
	    "<YT-INIT Normal Termination>")
	    || !output_occurs_before("<YT-INIT Normal Termination>",
	    "Running initial maintenance...")
	    || !output_occurs_before("Running initial maintenance...",
	    "Daily Maintenance Completed OK")) {
		failure = "redirected output reordered initialization handoff";
		goto done;
	}

	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    &error)
	    || !yt_config_load(&game.database, &game.config, &error)) {
		failure = "cannot reopen initialized YTDATA.DAT";
		goto close;
	}
	if (strcmp(game.config.scoreboard, "YTSCORE.ASC") != 0
	    || game.config.turns_per_day != 500.0f
	    || game.config.sector_offset != 51.0f
	    || game.config.port_offset != 2055.0f
	    || game.config.planet_offset != 3055.0f
	    || game.config.total_records != 3155.0f
	    || game.config.initial_fighters != 25.0f
	    || game.config.initial_credits != 1005.0f
	    || game.config.initial_holds != 10.0f
	    || game.config.retention_days != 14.0f
	    || game.config.local_screen != -1.0f
	    || game.config.lottery_plays != 5.0f
	    || game.config.genesis_ports != 300.0f
	    || game.config.maximum_holds != 1000.0f
	    || game.config.marker != 6324.0f) {
		failure = "initialized configuration defaults differ";
		goto close;
	}
	for (basic = 2; basic <= 51; ++basic) {
		if (!yt_game_read_player(&game, basic, &player, &error)
		    || player.name_length != 0U || player.score != -1.0f) {
			failure = "an initialized player slot has wrong defaults";
			goto close;
		}
	}
	if (!yt_game_read_sector(&game, 1, &sector, &error)
	    || sector.warps[0] != 1.0f || sector.warps[1] != 2.0f
	    || sector.warps[2] != 3.0f || sector.warps[3] != 4.0f
	    || sector.warps[4] != 5.0f || sector.warps[5] != 6.0f) {
		failure = "sector 1 does not have the documented fixed warps";
		goto close;
	}
	for (basic = 1; basic <= 4; ++basic) {
		static const float fixed_sectors[4] = {1, 3, 5, 7};

		if (!yt_game_read_port(&game, basic, &port, &error)
		    || port.sector != fixed_sectors[basic - 1]) {
			failure = "a fixed port is absent or in the wrong sector";
			goto close;
		}
	}
	if (!yt_game_read_planet(&game, 1, &planet, &error)
	    || strcmp(planet.name, "The Wanderer") != 0
	    || !yt_game_read_planet(&game, 99, &planet, &error)
	    || strcmp(planet.name, "Mercenary Base") != 0
	    || !yt_game_read_planet(&game, 100, &planet, &error)
	    || strcmp(planet.name, "Xannoron") != 0) {
		failure = "one or more fixed planets have wrong names";
		goto close;
	}
	result = EXIT_SUCCESS;

close:
	yt_game_close(&game);
	if (result == EXIT_SUCCESS) {
		static const char old_database[] = "old database contents";

		input = fopen("YTDATA.DAT", "wb");
		if (input == NULL
		    || fwrite(old_database, 1, sizeof(old_database) - 1U,
		    input) != sizeof(old_database) - 1U
		    || fclose(input) != 0) {
			failure = "cannot prepare initializer truncation test";
			result = EXIT_FAILURE;
			goto done;
		}
		input = fopen("input.txt", "wb");
		if (input == NULL || fwrite("Y\n", 1, 2, input) != 2
		    || fclose(input) != 0) {
			failure = "cannot prepare early-EOF initializer input";
			result = EXIT_FAILURE;
			goto done;
		}
		status = system(command);
		if (status != 0 || !file_size_is("YTDATA.DAT", 0L)) {
			failure = "accepted initialization did not truncate before the scoreboard prompt";
			result = EXIT_FAILURE;
		}
	}
done:
	cleanup_files();
	if (yt_chdir(original) == 0)
		(void)yt_rmdir(directory);
	if (result == EXIT_SUCCESS)
		puts("test_clean_install: ok");
	else
		fprintf(stderr, "test_clean_install: %s\n", failure);
	return result;
}
