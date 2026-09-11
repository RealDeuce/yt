#include "yt_cli.h"
#include "yt_door.h"
#include "yt_platform.h"
#include "yt_session.h"

#include <stdlib.h>

#ifdef ODPLAT_WIN32
#include <windows.h>

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
    int show_command)
{
	struct yt_door door;
	struct yt_error error;
	char executable[1024];
	bool ok;

	(void)instance;
	(void)previous;
	(void)show_command;
	yt_error_clear(&error);
	if (!yt_platform_executable_path(executable, sizeof(executable), NULL,
	    &error))
		return EXIT_FAILURE;
	if (!yt_door_start(&door, command_line, &error))
		return EXIT_FAILURE;
	ok = yt_session_run(&door, executable, &error);
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
	char executable[1024];
	bool ok;

	yt_error_clear(&error);
	if (!yt_platform_executable_path(executable, sizeof(executable),
	    argc > 0 ? argv[0] : NULL, &error))
		return EXIT_FAILURE;
	if (!yt_door_start(&door, argc, argv, &error))
		return EXIT_FAILURE;
	ok = yt_session_run(&door, executable, &error);
	if (!ok)
		yt_cli_error("YT", &error);
	yt_door_finish(ok ? EXIT_SUCCESS : EXIT_FAILURE);
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
