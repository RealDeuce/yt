#include "text_test_support.h"
#include "yt_file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status, const char *operation,
    const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s", path != NULL ? path : "");
}

bool
yt_text_read(const char *path, struct yt_text_file *text,
    struct yt_error *error)
{
	char resolved[512];
	FILE *file;
	long length;

	memset(text, 0, sizeof(*text));
	if (!yt_resolve_case_path(path, false, resolved, sizeof(resolved), error))
		return false;
	file = fopen(resolved, "rb");
	if (file == NULL) {
		set_error(error, YT_IO_ERROR, "open text", resolved);
		return false;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0
	    || fseek(file, 0, SEEK_SET) != 0) {
		set_error(error, YT_IO_ERROR, "size text", resolved);
		fclose(file);
		return false;
	}
	text->data = malloc((size_t)length + 1U);
	if (text->data == NULL) {
		set_error(error, YT_NO_MEMORY, "allocate text", resolved);
		fclose(file);
		return false;
	}
	if (length > 0
	    && fread(text->data, 1, (size_t)length, file) != (size_t)length) {
		set_error(error, YT_IO_ERROR, "read text", resolved);
		yt_text_free(text);
		fclose(file);
		return false;
	}
	fclose(file);
	text->data[length] = 0;
	text->length = (size_t)length;
	return true;
}

void
yt_text_free(struct yt_text_file *text)
{
	free(text->data);
	text->data = NULL;
	text->length = 0;
}
