#include "yt_patch_cli.h"

#include "OpenDoor.h"
#include "qb.h"

#include <stdio.h>
#include <string.h>

static struct yt_patch_selection *active_selection;

static bool
patch_keyword(const char *keyword)
{
	if (keyword == NULL)
		return false;
	while (*keyword == '-' || *keyword == '/')
		++keyword;
	return qb_ascii_casecmp(keyword, "PATCH") == 0;
}

static void ODCALL
patch_option(char *keyword, char *options)
{
	const struct yt_patch_profile *profile;

	if (active_selection == NULL || !patch_keyword(keyword))
		return;
	active_selection->seen = true;
	profile = yt_patch_find(options);
	if (profile == NULL) {
		active_selection->valid = false;
		(void)snprintf(active_selection->rejected,
		    sizeof(active_selection->rejected), "%s",
		    options != NULL ? options : "");
		return;
	}
	active_selection->profile = profile;
	active_selection->valid = true;
	active_selection->rejected[0] = '\0';
}

static bool
finish_parse(struct yt_patch_selection *selection, struct yt_error *error)
{
	active_selection = NULL;
	od_control.od_cmd_line_handler = NULL;
	if (selection->valid)
		return true;
	if (error != NULL) {
		error->status = YT_INVALID;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation),
		    "select patch level (3.6, A, C, D, E, F, G)");
		(void)snprintf(error->path, sizeof(error->path), "%s",
		    selection->rejected);
	}
	return false;
}

#ifdef ODPLAT_WIN32
bool
yt_patch_parse_command_line(char *command_line,
    struct yt_patch_selection *selection, struct yt_error *error)
#else
bool
yt_patch_parse_command_line(int argc, char **argv,
    struct yt_patch_selection *selection, struct yt_error *error)
#endif
{
	if (selection == NULL || active_selection != NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "parse patch level");
			error->path[0] = '\0';
		}
		return false;
	}
	memset(selection, 0, sizeof(*selection));
	selection->profile = yt_patch_default();
	selection->valid = true;
	active_selection = selection;
	od_control.od_cmd_line_handler = patch_option;
#ifdef ODPLAT_WIN32
	od_parse_cmd_line(command_line);
#else
	od_parse_cmd_line(argc, argv);
#endif
	return finish_parse(selection, error);
}
