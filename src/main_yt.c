#include "yt_cli.h"
#include "yt_door.h"
#include "yt_platform.h"
#include "yt_session.h"
#include "yt_startup_model.h"

#include <stdlib.h>
#include <string.h>

static bool
finish_startup_prefix(struct yt_door *door,
    struct yt_startup_main_prefix *prefix)
{
	size_t first_length = strlen(door->identity.real_first);
	size_t last_length = strlen(door->identity.real_last);

	if (!yt_startup_main_prefix_finish(
	    (uint8_t *)door->identity.real_first, &first_length,
	    (uint8_t *)door->identity.real_last, &last_length, prefix))
		return false;
	door->identity.real_first[first_length] = '\0';
	door->identity.real_last[last_length] = '\0';
	return true;
}

#ifdef ODPLAT_WIN32
#include <windows.h>

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
    int show_command)
{
	struct yt_door door;
	struct yt_error error;
	struct yt_startup_main_prefix startup_prefix;
	char executable[1024];
	bool ok;

	(void)instance;
	(void)previous;
	(void)show_command;
	yt_error_clear(&error);
	if (!yt_platform_executable_path(executable, sizeof(executable), NULL,
	    &error))
		return EXIT_FAILURE;
	if (!yt_startup_main_prefix_begin(&startup_prefix))
		return EXIT_FAILURE;
	if (!yt_door_start(&door, command_line, &error))
		return EXIT_FAILURE;
	if (!finish_startup_prefix(&door, &startup_prefix))
		return EXIT_FAILURE;
	ok = yt_session_run(&door, executable, &startup_prefix, &error);
	if (!ok)
		yt_cli_error("YT", &error);
	yt_door_finish(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
int
main(int argc, char **argv)
{
	struct yt_door door;
	struct yt_error error;
	struct yt_startup_main_prefix startup_prefix;
	char executable[1024];
	bool ok;

	yt_error_clear(&error);
	if (!yt_platform_executable_path(executable, sizeof(executable),
	    argc > 0 ? argv[0] : NULL, &error))
		return EXIT_FAILURE;
	if (!yt_startup_main_prefix_begin(&startup_prefix))
		return EXIT_FAILURE;
	if (!yt_door_start(&door, argc, argv, &error))
		return EXIT_FAILURE;
	if (!finish_startup_prefix(&door, &startup_prefix))
		return EXIT_FAILURE;
	ok = yt_session_run(&door, executable, &startup_prefix, &error);
	if (!ok)
		yt_cli_error("YT", &error);
	yt_door_finish(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
