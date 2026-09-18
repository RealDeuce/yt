#include "yt_door.h"

#include "OpenDoor.h"
#include "qb.h"
#include "yt_patch_cli.h"

#include <stdlib.h>
#include <string.h>

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
	identity->local = od_control.od_force_local || od_control.baud == 0U;
	qb_title_case(identity->sysop_first);
	qb_title_case(identity->sysop_last);
	qb_title_case(identity->real_first);
	qb_title_case(identity->real_last);
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
finish_start(struct yt_door *door, struct yt_error *error)
{
	(void)error;
	door->open_doors_initialized = true;
	identity_from_open_doors(&door->identity);
	/* OpenDoors owns these policies; local sessions retain the original
	 * absence of an inactivity disconnect. */
	od_control.od_disable &= (WORD)~(DIS_CARRIERDETECT | DIS_TIMEOUT);
	od_control.od_disable_inactivity = FALSE;
	od_control.od_inactivity = door->identity.local ? 0 : 240;
	od_control.od_inactive_warning = 0;
	od_control.od_inactivity_warning = "";
	od_control.od_time_warning = "";
	od_control.od_inactivity_timeout =
	    "\r\n\aUSER FELL ASLEEP!\n\r";
	od_control.od_no_time =
	    "\r\n\a\a\aTIME LIMIT EXCEEDED!\a\a\a\n\r";
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
	struct yt_patch_selection patch_selection;

	if (door == NULL) {
		set_error(error, YT_INVALID, "door startup", "");
		return false;
	}
	memset(door, 0, sizeof(*door));
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
	copy_text(od_control.od_prog_version,
	    sizeof(od_control.od_prog_version),
	    yt_patch_default()->open_doors_version);
#ifdef ODPLAT_WIN32
	prefill_legacy_path(command_line);
#else
	prefill_legacy_path(argc, argv);
#endif
	/* Must be called exactly once, and before od_init(). */
#ifdef ODPLAT_WIN32
	if (!yt_patch_parse_command_line(command_line, &patch_selection, error))
		return false;
#else
	if (!yt_patch_parse_command_line(argc, argv, &patch_selection, error))
		return false;
#endif
	door->patch = patch_selection.profile;
	copy_text(od_control.od_prog_version, sizeof(od_control.od_prog_version),
	    door->patch->open_doors_version);
	od_control.od_maxtime = 180;
	/* Exact mode keeps CP437 bytes and disables RA/QBBS substitutions. */
	od_control.od_cp437_to_utf8_out = FALSE;
	od_control.od_no_ra_codes = TRUE;
	od_init();
	copy_text(door->rmt_handoff_path, sizeof(door->rmt_handoff_path),
	    od_control.info_path);
	/* Initialization installs OpenDoors defaults after command parsing. */
	od_control.od_always_clear = FALSE;
	od_control.od_status_on = FALSE;
	return finish_start(door, error);
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

void
yt_door_shutdown_for_replace(void)
{
	struct yt_door *door = current_door;

	if (door == NULL || !door->open_doors_initialized)
		return;
	/* RUN replaces this process.  Let OpenDoors finish its session and
	 * restore the host state, but return long enough for exec(). */
	od_control.od_noexit = TRUE;
	od_exit(EXIT_SUCCESS, FALSE);
	door->open_doors_initialized = false;
}

struct yt_door *
yt_door_current(void)
{
	return current_door;
}
