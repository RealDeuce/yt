#include "yt_config.h"
#include "yt_file.h"
#include "yt_game.h"
#include "yt_init.h"
#include "yt_names.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
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

#ifndef YT_CONFIG_EXE
#error "YT_CONFIG_EXE must name the ytconfig executable under test"
#endif
#ifndef YT_PORTNAME_EXE
#error "YT_PORTNAME_EXE must name the portname executable under test"
#endif
#ifndef YT_RMT_INIT_EXE
#error "YT_RMT_INIT_EXE must name the rmt-init executable under test"
#endif
#ifndef YT_LOCAL_EXE
#error "YT_LOCAL_EXE must name the local executable under test"
#endif

static int
fail(const char *message)
{
	fprintf(stderr, "test_utilities: %s\n", message);
	return EXIT_FAILURE;
}

static bool
write_file(const char *path, const void *data, size_t length)
{
	FILE *file = fopen(path, "wb");

	if (file == NULL)
		return false;
	if (length > 0 && fwrite(data, 1, length, file) != length) {
		(void)fclose(file);
		return false;
	}
	return fclose(file) == 0;
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
test_name_input_grammar(struct yt_error *error)
{
	static const uint8_t stream[] =
	    "A,B,C\r\nD,E,F,G,H\r\n\x1a";
	static const uint8_t quoted[] =
	    "\"Real, First\",Last,Alias,Name\r\n\x1a";
	static const uint8_t multiline[] =
	    "\"A\r\nB\",C,D,E\r\n\x1a";
	static const uint8_t double_quote[] =
	    "\"A\"\"B\",C,D\r\n\x1a";
	static const uint8_t nul_quote[] =
	    {0, '"', 'A', ',', 'B', '"', ',', 'C', ',', 'D', '\r', '\n', 0x1a};
	static const uint8_t lf_cr[] =
	    "A\n\rB,C,D,E\r\n\x1a";
	static const uint8_t closed_eof[] = "A,B,C,\"D\"\x1a";
	static const uint8_t lf_eof[] = "A,B,C,\n\x1a";
	static const uint8_t incomplete[] = "A,B,C\x1a";
	struct yt_name_file names;
	bool ok = true;

#define LOAD_NAMES(bytes) (write_file("names.in", (bytes), sizeof(bytes) - 1U) \
	&& yt_names_load("names.in", &names, error))
#define REQUIRE_NAMES(expr, stage) do { \
	if (!(expr)) { \
		fprintf(stderr, "YTNAME grammar stage %s failed: status=%d op=%s\n", \
		    (stage), (int)error->status, error->operation); \
		return false; \
	} \
} while (0)
	REQUIRE_NAMES(LOAD_NAMES(stream), "cross-row load");
	ok = names.count == 2
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B") == 0
	    && strcmp(names.rows[0].alias_first, "C") == 0
	    && strcmp(names.rows[0].alias_last, "D") == 0
	    && strcmp(names.rows[1].real_first, "E") == 0
	    && strcmp(names.rows[1].real_last, "F") == 0
	    && strcmp(names.rows[1].alias_first, "G") == 0
	    && strcmp(names.rows[1].alias_last, "H") == 0
	    && yt_names_find_real_last(&names, "a", "B") == NULL
	    && !yt_names_alias_exists(&names, "c", "D");
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "cross-row values");
	REQUIRE_NAMES(LOAD_NAMES(quoted), "quoted load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "Real, First") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "quoted values");
	REQUIRE_NAMES(LOAD_NAMES(multiline), "multiline load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A\r\nB") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "multiline values");
	REQUIRE_NAMES(LOAD_NAMES(double_quote), "double quote load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "double quote values");
	REQUIRE_NAMES(write_file("names.in", nul_quote, sizeof(nul_quote))
	    && yt_names_load("names.in", &names, error), "NUL quote load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "A") == 0
	    && strcmp(names.rows[0].real_last, "B\"") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "NUL quote values");
	REQUIRE_NAMES(LOAD_NAMES(lf_cr), "LF-CR load");
	ok = names.count == 1
	    && strcmp(names.rows[0].real_first, "AB") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "LF-CR values");
	REQUIRE_NAMES(LOAD_NAMES(closed_eof), "closed EOF load");
	ok = names.count == 1 && strcmp(names.rows[0].alias_last, "D") == 0;
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "closed EOF values");
	REQUIRE_NAMES(LOAD_NAMES(lf_eof), "LF EOF load");
	ok = names.count == 1
	    && (unsigned char)names.rows[0].alias_last[0] == 0x1a
	    && names.rows[0].alias_last[1] == '\0';
	yt_names_free(&names);
	REQUIRE_NAMES(ok, "LF EOF values");
	REQUIRE_NAMES(write_file("names.in", incomplete,
	    sizeof(incomplete) - 1U), "incomplete write");
	yt_error_clear(error);
	ok = !yt_names_load("names.in", &names, error)
	    && error->status == YT_EOF && names.rows == NULL && names.count == 0;
#undef LOAD_NAMES
#undef REQUIRE_NAMES
	return ok;
}

static bool
read_file(const char *path, uint8_t **data, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;

	*data = NULL;
	*length = 0;
	if (file == NULL || fseek(file, 0, SEEK_END) != 0
	    || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
		if (file != NULL)
			(void)fclose(file);
		return false;
	}
	*data = malloc((size_t)size + 1U);
	if (*data == NULL) {
		(void)fclose(file);
		return false;
	}
	if (size > 0 && fread(*data, 1, (size_t)size, file) != (size_t)size) {
		free(*data);
		*data = NULL;
		(void)fclose(file);
		return false;
	}
	if (fclose(file) != 0) {
		free(*data);
		*data = NULL;
		return false;
	}
	(*data)[size] = 0;
	*length = (size_t)size;
	return true;
}

static bool
run_redirected(const char *program, const char *input, const char *output)
{
	char command[4096];
	int written = snprintf(command, sizeof(command),
	    "\"%s\" < \"%s\" > \"%s\" 2>&1", program, input, output);

	return written >= 0 && (size_t)written < sizeof(command)
	    && system(command) == 0;
}

static bool
initialize_direct(struct yt_error *error)
{
	struct yt_random random;

	yt_random_init(&random);
	return yt_initialize_yt("YTSCORE.ASC", &random, error);
}

static bool
test_ytconfig(struct yt_error *error)
{
	struct yt_game game;
	static const char input[] = "JX";

	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    error) || !yt_config_load(&game.database, &game.config, error))
		return false;
	game.config.local_screen = -2.0f;
	if (!yt_config_store(&game.database, &game.config, error)) {
		yt_game_close(&game);
		return false;
	}
	yt_game_close(&game);
	if (!write_file("config.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_CONFIG_EXE, "config.in", "config.out"))
		return false;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error))
		return false;
	if (game.config.local_screen != 0.0f) {
		yt_game_close(&game);
		return false;
	}
	yt_game_close(&game);
	return true;
}

static bool
test_portname(struct yt_error *error)
{
	uint8_t *before = NULL;
	uint8_t *after = NULL;
	size_t before_length = 0;
	size_t after_length = 0;
	struct yt_game game;
	int logical;
	static const char input[] = "yes\n";

	if (!read_file("YTDATA.DAT", &before, &before_length)
	    || !write_file("portname.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_PORTNAME_EXE, "portname.in",
	    "portname.out")
	    || !read_file("YTDATA.DAT", &after, &after_length)) {
		free(before);
		return false;
	}
	if (after_length != before_length) {
		free(before);
		free(after);
		return false;
	}
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error)) {
		free(before);
		free(after);
		return false;
	}
	for (logical = 1; logical <= 1000; ++logical) {
		struct yt_port port;
		size_t record = (size_t)yt_port_basic_record(&game.config,
		    logical);
		size_t base = (record - 1U) * YT_RECORD_SIZE;
		size_t byte;

		if (!yt_game_read_port(&game, logical, &port, error)
		    || port.name_length < 1.0f || port.name_length > 41.0f
		    || (logical == 1 && strcmp(port.name, "Earth") != 0)) {
			yt_game_close(&game);
			free(before);
			free(after);
			return false;
		}
		for (byte = 0; byte < YT_RECORD_SIZE; ++byte) {
			bool mutable = byte < 41U
			    || (byte >= YT_F85 && byte < YT_F85 + 4U);

			if (!mutable && before[base + byte] != after[base + byte]) {
				yt_game_close(&game);
				free(before);
				free(after);
				return false;
			}
		}
	}
	yt_game_close(&game);
	free(before);
	free(after);
	return true;
}

static bool
test_rmt_init(struct yt_error *error)
{
	struct yt_text_file output;
	struct yt_game game;
	static const char input[] = "Y\n";
	static const char scoreboard[] = "stale scoreboard survives";
	bool result;

	(void)remove("RMTINIT.TMP");
	(void)remove("rmtinit.tmp");
	if (!write_file("YTSCORE.ASC", scoreboard, sizeof(scoreboard) - 1U)
	    || !write_file("rmt.in", input, sizeof(input) - 1U)
	    || !run_redirected(YT_RMT_INIT_EXE, "rmt.in", "rmt.out")
	    || !file_size_is("YTDATA.DAT", 432235L)
	    || !file_size_is("YTNAME.DAT", 26L)
	    || !file_size_is("YTYNEWS.DAT", 151L)
	    || !file_size_is("YTRMSG.DAT", 430L)
	    || !file_size_is("YTSCORE.ASC",
	    (long)(sizeof(scoreboard) - 1U))
	    || !yt_text_read("rmt.out", &output, error))
		return false;
	result = strstr((const char *)output.data,
	    "Initialization completed sucessfully!") != NULL
	    && strstr((const char *)output.data, "Congratulations") == NULL
	    && strstr((const char *)output.data,
	    "Running stand alone... re-initializing using old sysop defined defaults.")
	    != NULL;
	yt_text_free(&output);
	if (!result)
		return false;
	memset(&game, 0, sizeof(game));
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_READ,
	    error) || !yt_config_load(&game.database, &game.config, error))
		return false;
	result = game.config.local_screen == -1.0f
	    && game.config.maximum_holds == 1000.0f
	    && game.config.genesis_ports == 300.0f;
	yt_game_close(&game);
	return result;
}

#ifndef _WIN32
static bool
copy_executable(const char *source, const char *dest)
{
	uint8_t *data;
	size_t length;
	bool result;

	if (!read_file(source, &data, &length))
		return false;
	result = write_file(dest, data, length) && chmod(dest, 0700) == 0;
	free(data);
	return result;
}

static bool
test_local(struct yt_error *error)
{
	static const char child[] =
	    "#!/bin/sh\n"
	    "printf '%s\\n' \"$1\" > child.out\n"
	    "exit 7\n";
	static const char command[] =
	    "./local '  jane   doe  ' > local.out 2>&1";
	static const char expected[] =
	    "Yankee Trader Local Logon Program\r\n"
	    "Alan\r\n"
	    "Davenport\r\n"
	    "COM0\r\n"
	    "0 BAUD,N,8,1\r\n"
	    " 0 \r\n"
	    "JANE\r\n"
	    "DOE\r\n"
	    "Anytown, USA\r\n"
	    " 1 \r\n"
	    " 100 \r\n"
	    " 180 \r\n"
	    " 0 \r\n"
	    "\x1a";
	struct yt_text_file dorinfo;
	struct yt_text_file child_output;
	bool result;

	if (!copy_executable(YT_LOCAL_EXE, "local")
	    || !write_file("yt", child, sizeof(child) - 1U)
	    || chmod("yt", 0700) != 0
	    || system(command) != 0
	    || !yt_text_read("DORINFO1.DEF", &dorinfo, error)
	    || !yt_text_read("child.out", &child_output, error))
		return false;
	result = dorinfo.length == sizeof(expected) - 1U
	    && memcmp(dorinfo.data, expected, sizeof(expected) - 1U) == 0
	    && strcmp((const char *)child_output.data, "DORINFO1.DEF\n") == 0;
	yt_text_free(&dorinfo);
	yt_text_free(&child_output);
	return result;
}
#endif

static void
cleanup_files(void)
{
	static const char *const paths[] = {
		"YTDATA.DAT", "YTNAME.DAT", "YTNEWS.DAT", "YTYNEWS.DAT",
		"YTRMSG.DAT", "YTSCORE.ASC", "RMTINIT.TMP", "rmtinit.tmp",
		"config.in", "config.out", "portname.in", "portname.out",
		"rmt.in", "rmt.out", "local", "local.out", "yt", "child.out",
		"DORINFO1.DEF", "names.in"
	};
	size_t index;

	for (index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index)
		(void)remove(paths[index]);
}

int
main(void)
{
	char original[1024];
	char directory[1024];
	struct yt_error error;
	const char *failure = NULL;

	if (yt_getcwd(original, sizeof(original)) == NULL)
		return fail("cannot determine original directory");
#ifdef _WIN32
	snprintf(directory, sizeof(directory), "%s\\yt-utilities-%lu",
	    original, (unsigned long)_getpid());
	if (yt_mkdir(directory) != 0)
		return fail("cannot create temporary directory");
#else
	snprintf(directory, sizeof(directory), "/tmp/yt-utilities.XXXXXX");
	if (mkdtemp(directory) == NULL)
		return fail("cannot create temporary directory");
#endif
	if (yt_chdir(directory) != 0)
		return fail("cannot enter temporary directory");
	yt_error_clear(&error);
	if (!initialize_direct(&error))
		failure = "cannot create utility test database";
	else if (!test_ytconfig(&error))
		failure = "YTCONFIG executable behavior differs";
	else if (!test_portname(&error))
		failure = "PORTNAME changed data outside its two owned fields";
	else if (!test_rmt_init(&error))
		failure = "RMT-INIT standalone behavior differs";
	else if (!test_name_input_grammar(&error))
		failure = "YTNAME INPUT# grammar differs";
#ifndef _WIN32
	else if (!test_local(&error))
		failure = "LOCAL child handoff or DORINFO bytes differ";
#endif
	cleanup_files();
	if (yt_chdir(original) != 0)
		return fail("cannot restore original directory");
	(void)yt_rmdir(directory);
	if (failure != NULL)
		return fail(failure);
	puts("test_utilities: ok");
	return EXIT_SUCCESS;
}
