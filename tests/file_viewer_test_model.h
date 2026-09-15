#ifndef FILE_VIEWER_TEST_MODEL_H
#define FILE_VIEWER_TEST_MODEL_H

#include "yt_text.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct yt_file_viewer_state {
	float *foreground;
	int *pager_foreground;
	float *bold;
	float *line_count;
	char *pager_key;
	float saved_foreground;
	int saved_pager_foreground;
};

typedef bool (*yt_file_viewer_present_fn)(void *context,
    const uint8_t *text, size_t length, bool paged,
    struct yt_error *error);

static void
test_file_viewer_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static bool
yt_file_viewer_display(const char *path, struct yt_file_viewer_state *state,
    yt_file_viewer_present_fn present, void *context,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Cntl-X to Stop";
	struct yt_text_input input;
	bool result = false;

	if (path == NULL || state == NULL || present == NULL
	    || state->foreground == NULL
	    || state->pager_foreground == NULL || state->bold == NULL
	    || state->line_count == NULL || state->pager_key == NULL) {
		errno = 0;
		test_file_viewer_error(error, YT_INVALID, "file viewer", path);
		return false;
	}
	state->pager_key[0] = '\0';
	if (!present(context, notice, sizeof(notice) - 1U, true, error)
	    || !present(context, NULL, 0U, false, error))
		return false;
	yt_text_input_init(&input);
	if (!yt_text_input_close(&input, error))
		goto done;
	*state->line_count = 0.0f;
	if (!yt_text_input_open(&input, path, error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		int foreground;

		if (!yt_text_input_eof(&input, &eof, error))
			goto done;
		if (eof || strcmp(state->pager_key, "Q") == 0)
			break;
		if (!yt_text_input_read_line(&input, &line, &length, &available,
		    error))
			goto done;
		if (!available) {
			errno = 0;
			test_file_viewer_error(error, YT_EOF,
			    "LINE INPUT after EOF check", path);
			goto done;
		}
		foreground = yt_file_viewer_line_foreground(line, length);
		*state->foreground = (float)foreground;
		*state->pager_foreground = foreground;
		if (foreground != 2)
			*state->bold = 1.0f;
		if (!present(context, line, length, true, error))
			goto done;
	}
	if (!yt_text_input_close(&input, error))
		goto done;
	*state->line_count = 0.0f;
	*state->foreground = state->saved_foreground;
	*state->pager_foreground = state->saved_pager_foreground;
	result = present(context, NULL, 0U, false, error);

done:
	yt_text_input_destroy(&input);
	return result;
}

#endif
