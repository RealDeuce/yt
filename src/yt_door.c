#include "yt_door.h"

#include "OpenDoor.h"
#include "qb.h"
#include "yt_file.h"
#include "yt_startup_model.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static struct yt_door *current_door;
static bool cleanup_registered;

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = 0;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static void
copy_text(char *dest, size_t size, const char *source)
{
	if (size > 0)
		snprintf(dest, size, "%s", source != NULL ? source : "");
}

static bool
read_line(FILE *file, char *dest, size_t size)
{
	size_t length;

	if (fgets(dest, (int)size, file) == NULL)
		return false;
	length = strlen(dest);
	while (length > 0 && (dest[length - 1] == '\r'
	    || dest[length - 1] == '\n' || dest[length - 1] == 0x1a))
		dest[--length] = '\0';
	return true;
}

static bool
read_dorinfo(const char *path, struct yt_identity *identity)
{
	char resolved[512];
	struct yt_error ignored;
	FILE *file;
	char lines[12][256];
	int index;

	yt_error_clear(&ignored);
	if (path == NULL || path[0] == '\0'
	    || !yt_resolve_case_path(path, false, resolved, sizeof(resolved),
	    &ignored))
		return false;
	file = fopen(resolved, "rb");
	if (file == NULL)
		return false;
	for (index = 0; index < 12; ++index) {
		if (!read_line(file, lines[index], sizeof(lines[index]))) {
			fclose(file);
			return false;
		}
	}
	fclose(file);
	copy_text(identity->system, sizeof(identity->system), lines[0]);
	copy_text(identity->sysop_first, sizeof(identity->sysop_first), lines[1]);
	copy_text(identity->sysop_last, sizeof(identity->sysop_last), lines[2]);
	copy_text(identity->real_first, sizeof(identity->real_first), lines[6]);
	copy_text(identity->real_last, sizeof(identity->real_last), lines[7]);
	copy_text(identity->location, sizeof(identity->location), lines[8]);
	qb_title_case(identity->sysop_first);
	qb_title_case(identity->sysop_last);
	qb_title_case(identity->real_first);
	qb_title_case(identity->real_last);
	if (!yt_startup_ansi_raw((const uint8_t *)lines[9],
	    strlen(lines[9]), identity->ansi_raw))
		return false;
	identity->ansi = qb_mbf32_truth(identity->ansi_raw);
	identity->minutes = (int)qb_val(lines[11]).value;
	{
		size_t length = strlen(lines[3]);
		int port = length > 0 ? lines[3][length - 1] - '0' : 0;
		identity->local = port < 1 || port > 4;
	}
	return true;
}

static void
identity_from_open_doors(struct yt_identity *identity)
{
	char full[sizeof(od_control.user_name)];
	char sysop[sizeof(od_control.sysop_name)];
	char *space;

	copy_text(identity->system, sizeof(identity->system),
	    od_control.system_name);
	copy_text(sysop, sizeof(sysop), od_control.sysop_name);
	space = strchr(sysop, ' ');
	if (space == NULL) {
		copy_text(identity->sysop_first, sizeof(identity->sysop_first),
		    sysop);
		identity->sysop_last[0] = '\0';
	}
	else {
		*space++ = '\0';
		copy_text(identity->sysop_first, sizeof(identity->sysop_first),
		    sysop);
		copy_text(identity->sysop_last, sizeof(identity->sysop_last),
		    space);
	}
	copy_text(full, sizeof(full), od_control.user_name);
	space = strchr(full, ' ');
	if (space == NULL) {
		copy_text(identity->real_first, sizeof(identity->real_first), full);
		identity->real_last[0] = '\0';
	}
	else {
		*space++ = '\0';
		copy_text(identity->real_first, sizeof(identity->real_first), full);
		copy_text(identity->real_last, sizeof(identity->real_last), space);
	}
	copy_text(identity->location, sizeof(identity->location),
	    od_control.user_location);
	identity->ansi = od_control.user_ansi != 0;
	(void)qb_mbf32_encode(identity->ansi ? 1.0f : 0.0f,
	    identity->ansi_raw);
	identity->minutes = od_control.user_timelimit;
	identity->local = od_control.od_force_local || !od_carrier();
	qb_title_case(identity->sysop_first);
	qb_title_case(identity->sysop_last);
	qb_title_case(identity->real_first);
	qb_title_case(identity->real_last);
}

static bool
regular_file(const char *path)
{
	struct stat info;

	return path != NULL && path[0] != '\0' && stat(path, &info) == 0
	    && S_ISREG(info.st_mode);
}

static bool
directory_path(const char *path)
{
	struct stat info;

	return path != NULL && path[0] != '\0' && stat(path, &info) == 0
	    && S_ISDIR(info.st_mode);
}

static bool
read_effective_dorinfo(const char *requested_path,
    struct yt_identity *identity)
{
	char candidate[768];
	char filename[32];
	const char *directory = requested_path;
	int node = od_control.od_node;
	int written;

	if (regular_file(requested_path))
		return read_dorinfo(requested_path, identity);
	if (directory == NULL || directory[0] == '\0')
		directory = ".";
	if (!directory_path(directory))
		return false;
	if (node > 35)
		strcpy(filename, "dorinfo1.def");
	else if (node > 9)
		snprintf(filename, sizeof(filename), "dorinfo%c.def", node + 55);
	else
		snprintf(filename, sizeof(filename), "dorinfo%d.def", node);
	written = snprintf(candidate, sizeof(candidate), "%s%s%s", directory,
	    directory[strlen(directory) - 1U] == '/'
#ifdef _WIN32
	    || directory[strlen(directory) - 1U] == '\\'
#endif
	    ? "" : "/", filename);
	if (written >= 0 && (size_t)written < sizeof(candidate)
	    && read_dorinfo(candidate, identity))
		return true;
	written = snprintf(candidate, sizeof(candidate), "%s%sdorinfo1.def",
	    directory, directory[strlen(directory) - 1U] == '/'
#ifdef _WIN32
	    || directory[strlen(directory) - 1U] == '\\'
#endif
	    ? "" : "/");
	return written >= 0 && (size_t)written < sizeof(candidate)
	    && read_dorinfo(candidate, identity);
}

static bool
is_drop_flag(const char *argument)
{
	if (argument == NULL)
		return false;
	while (*argument == '-' || *argument == '/')
		++argument;
	return qb_ascii_casecmp(argument, "D") == 0
	    || qb_ascii_casecmp(argument, "DROPFILE") == 0;
}

#ifdef ODPLAT_WIN32
static const char *
next_windows_token(const char *cursor, char *token, size_t size)
{
	bool quoted = false;
	size_t used = 0;

	while (*cursor == ' ' || *cursor == '\t')
		++cursor;
	if (*cursor == '\0') {
		token[0] = '\0';
		return cursor;
	}
	while (*cursor != '\0') {
		char character = *cursor++;

		if (character == '"') {
			quoted = !quoted;
			continue;
		}
		if (!quoted && (character == ' ' || character == '\t'))
			break;
		if (used + 1U < size)
			token[used++] = character;
	}
	token[used] = '\0';
	return cursor;
}

static void
prefill_legacy_path(const char *command_line)
{
	const char *cursor = command_line;
	char token[512];
	char first[512] = "";
	bool explicit_path = false;
	bool first_seen = false;

	for (;;) {
		cursor = next_windows_token(cursor, token, sizeof(token));
		if (token[0] == '\0')
			break;
		if (!first_seen) {
			copy_text(first, sizeof(first), token);
			first_seen = true;
		}
		if (is_drop_flag(token))
			explicit_path = true;
	}
	if (!explicit_path && first_seen && first[0] != '-'
	    && !(first[0] == '/' && first[1] != '\0'))
		copy_text(od_control.info_path, sizeof(od_control.info_path), first);
}
#else
static void
prefill_legacy_path(int argc, char **argv)
{
	bool explicit_path = false;
	int index;

	for (index = 1; index < argc; ++index) {
		if (is_drop_flag(argv[index])) {
			explicit_path = true;
			break;
		}
	}
	if (!explicit_path && argc > 1 && argv[1][0] != '-')
		copy_text(od_control.info_path, sizeof(od_control.info_path),
		    argv[1]);
}
#endif

void
yt_door_cleanup(void)
{
	struct yt_door *door = current_door;

	if (door == NULL)
		return;
	if (door->game_open) {
		door->game_open = false;
		yt_game_close(&door->game);
	}
}

static bool
finish_start(struct yt_door *door, const char *requested_path,
    struct yt_error *error)
{
	struct yt_identity supplement = {0};

	(void)error;
	door->open_doors_initialized = true;
	identity_from_open_doors(&door->identity);
	if (read_effective_dorinfo(requested_path, &supplement))
		door->identity = supplement;
	return true;
}

#ifdef ODPLAT_WIN32
bool
yt_door_start(struct yt_door *door, char *command_line,
    struct yt_error *error)
#else
bool
yt_door_start(struct yt_door *door, int argc, char **argv,
    struct yt_error *error)
#endif
{
	char requested_path[sizeof(od_control.info_path)];

	if (door == NULL) {
		set_error(error, YT_INVALID, "door startup", "");
		return false;
	}
	memset(door, 0, sizeof(*door));
#ifdef ODPLAT_WIN32
	copy_text(door->command_line, sizeof(door->command_line),
	    command_line);
#else
	{
		size_t used = 0;
		int index;

		for (index = 1; index < argc; ++index) {
			int written = snprintf(door->command_line + used,
			    sizeof(door->command_line) - used, "%s%s",
			    index == 1 ? "" : " ", argv[index]);

			if (written < 0
			    || (size_t)written
			    >= sizeof(door->command_line) - used)
				break;
			used += (size_t)written;
		}
	}
#endif
	current_door = door;
	if (!cleanup_registered) {
		if (atexit(yt_door_cleanup) != 0) {
			set_error(error, YT_INVALID, "register cleanup", "");
			return false;
		}
		cleanup_registered = true;
	}
	/* OpenDoors can call exit() below; install both cleanup hooks first. */
	od_control.od_before_exit = yt_door_cleanup;
	od_control.od_nocopyright = TRUE;
	od_control.od_always_clear = FALSE;
	copy_text(od_control.od_prog_name, sizeof(od_control.od_prog_name),
	    "Yankee Trader");
	copy_text(od_control.od_prog_version, sizeof(od_control.od_prog_version),
	    "3.6g");
#ifdef ODPLAT_WIN32
	prefill_legacy_path(command_line);
#else
	prefill_legacy_path(argc, argv);
#endif
	copy_text(requested_path, sizeof(requested_path), od_control.info_path);
	/* Must be called exactly once, and before od_init(). */
#ifdef ODPLAT_WIN32
	od_parse_cmd_line(command_line);
#else
	od_parse_cmd_line(argc, argv);
#endif
	/* Exact mode keeps CP437 bytes and disables RA/QBBS substitutions. */
	od_control.od_cp437_to_utf8_out = FALSE;
	od_control.od_no_ra_codes = TRUE;
	copy_text(requested_path, sizeof(requested_path), od_control.info_path);
	od_init();
	return finish_start(door, requested_path, error);
}

void
yt_door_finish(int errorlevel)
{
	struct yt_door *door = current_door;

	yt_door_cleanup();
	if (door != NULL && door->open_doors_initialized)
		od_exit(errorlevel, FALSE);
	exit(errorlevel);
}

struct yt_door *
yt_door_current(void)
{
	return current_door;
}
