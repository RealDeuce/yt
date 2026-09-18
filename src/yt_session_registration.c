#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_output.h"
#include "yt_text.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGISTRATION_LINE_COUNT 3U
#define REGISTRATION_DISPLAY_COUNT 2U
#define REGISTRATION_STRING_CAPACITY 32767U

struct registration_text {
	uint8_t *data;
	size_t length;
};

struct registration_files {
	struct yt_database random;
	struct yt_text_input sequential;
	char path[512];
	bool path_resolved;
};

static bool
registration_error(struct yt_error *error, enum yt_status status,
    const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
registration_io_error(struct registration_files *files,
    struct yt_error *error, enum yt_status status, const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = status == YT_IO_ERROR ? errno : 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		(void)snprintf(error->path, sizeof(error->path), "%s",
		    files->path_resolved ? files->path : "YT.REG");
	}
	return false;
}

static bool
registration_resolve_path(struct registration_files *files,
    struct yt_error *error)
{
	if (files->path_resolved)
		return true;
	if (!yt_resolve_case_path("YT.REG", true, files->path,
	    sizeof(files->path), error))
		return false;
	files->path_resolved = true;
	return true;
}

static bool
registration_close_file4(struct registration_files *files,
    struct yt_error *error)
{
	if (files->sequential.file != NULL)
		return yt_text_input_close(&files->sequential, error);
	return yt_database_random_close(&files->random, error);
}

static bool
registration_read_line(struct registration_files *files,
    struct registration_text *line, struct yt_error *error)
{
	const uint8_t *text;
	size_t length;
	bool available;

	if (files->sequential.file == NULL)
		return registration_io_error(files, error, YT_INVALID,
		    "registration LINE INPUT without file");
	if (!yt_text_input_read_line(&files->sequential, &text, &length,
	    &available, error))
		return false;
	if (!available)
		return registration_io_error(files, error, YT_EOF,
		    "registration LINE INPUT past end");
	if (length > REGISTRATION_STRING_CAPACITY)
		return registration_io_error(files, error, YT_NO_MEMORY,
		    "registration LINE INPUT string space");
	if (length != 0U)
		memcpy(line->data, text, length);
	line->length = length;
	return true;
}

static bool
registration_copy(struct registration_text *destination,
    const uint8_t *prefix, size_t prefix_length, const uint8_t *value,
    size_t value_length, struct yt_error *error)
{
	if (prefix_length > REGISTRATION_STRING_CAPACITY
	    || value_length > REGISTRATION_STRING_CAPACITY - prefix_length)
		return registration_error(error, YT_NO_MEMORY,
		    "registration string space");
	if (prefix_length != 0U)
		memcpy(destination->data, prefix, prefix_length);
	if (value_length != 0U)
		memcpy(destination->data + prefix_length, value, value_length);
	destination->length = prefix_length + value_length;
	return true;
}

static bool
registration_prepend(struct registration_text *destination,
    const uint8_t *prefix, size_t prefix_length, struct yt_error *error)
{
	if (prefix_length > REGISTRATION_STRING_CAPACITY
	    || destination->length > REGISTRATION_STRING_CAPACITY
	    - prefix_length)
		return registration_error(error, YT_NO_MEMORY,
		    "registration string space");
	if (destination->length != 0U)
		memmove(destination->data + prefix_length, destination->data,
		    destination->length);
	if (prefix_length != 0U)
		memcpy(destination->data, prefix, prefix_length);
	destination->length += prefix_length;
	return true;
}

static bool
registration_arithmetic(const struct registration_text line[2],
    uint8_t calculated_key[8], struct yt_error *error)
{
	static const uint8_t signature[8] =
	    {0x00, 0x00, 0xa8, 0x43, 0x3b, 0x6a, 0x4f, 0xa5};
	uint8_t accumulator[8];
	uint8_t contribution[8];
	uint8_t seven[8];
	uint8_t product[8];
	uint8_t final_product[8];
	uint8_t root[8];
	size_t index;
	enum qb_mbf_status status;

	status = qb_mbf64_from_u64(21U, accumulator);
	for (index = 0U; status == QB_MBF_OK
	    && index < line[0].length; ++index) {
		status = qb_mbf64_from_u64(3U * line[0].data[index],
		    contribution);
		if (status == QB_MBF_OK)
			status = qb_mbf64_add_raw(accumulator, contribution,
			    accumulator);
	}
	if (status != QB_MBF_OK)
		return registration_error(error, YT_RANGE,
		    "registration first-name arithmetic");
	status = qb_mbf64_mul_raw(signature, accumulator, product);
	if (status == QB_MBF_OK)
		status = qb_mbf64_sqrt_raw(product, root);
	if (status == QB_MBF_OK)
		status = qb_mbf64_floor_positive_raw(root, accumulator);
	if (status != QB_MBF_OK)
		return registration_error(error, YT_RANGE,
		    "registration first square root");
	for (index = 0U; status == QB_MBF_OK
	    && index < line[1].length; ++index) {
		status = qb_mbf64_from_u64(5U * line[1].data[index],
		    contribution);
		if (status == QB_MBF_OK)
			status = qb_mbf64_add_raw(accumulator, contribution,
			    accumulator);
	}
	if (status != QB_MBF_OK)
		return registration_error(error, YT_RANGE,
		    "registration second-name arithmetic");
	status = qb_mbf64_mul_raw(signature, accumulator, product);
	if (status == QB_MBF_OK)
		status = qb_mbf64_from_u64(7U, seven);
	if (status == QB_MBF_OK)
		status = qb_mbf64_mul_raw(product, seven, final_product);
	if (status == QB_MBF_OK)
		status = qb_mbf64_sqrt_raw(final_product, root);
	if (status == QB_MBF_OK)
		status = qb_mbf64_floor_positive_raw(root, calculated_key);
	if (status != QB_MBF_OK)
		return registration_error(error, YT_RANGE,
		    "registration final square root");
	return true;
}

static bool
registration_centered(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_centered_line(text, length, &session->presentation,
	    &presentation);
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
registration_beep(struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_local_beep(&presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "registration local BEEP");
	}
	return false;
}

static bool
registration_title(struct yt_session *session, struct yt_error *error)
{
	const struct yt_patch_profile *patch = session_patch(session);
	const char *const centered[] = {
		"Yankee Trader", patch->registration_author,
		patch->registration_description, patch->registration_contact,
		patch->registration_banner,
	};
	size_t index;

	for (index = 0U; index < 3U; ++index) {
		if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
		    "registration title blank", error))
			return false;
	}
	if (!registration_centered(session, (const uint8_t *)centered[0],
	    strlen(centered[0]), "registration title", error))
		return false;
	if (!registration_centered(session, (const uint8_t *)centered[1],
	    strlen(centered[1]), "registration copyright", error))
		return false;
	if (!registration_centered(session, (const uint8_t *)centered[2],
	    strlen(centered[2]), "registration features", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error))
		return false;
	if (!registration_centered(session, (const uint8_t *)centered[3],
	    strlen(centered[3]), "registration strategy", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error))
		return false;
	if (!registration_centered(session, (const uint8_t *)centered[4],
	    strlen(centered[4]), "registration version", error))
		return false;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error))
		return false;
	return true;
}

static void
registration_close_all(struct yt_session *session,
    struct registration_files *files)
{
	(void)yt_text_input_close(&files->sequential, NULL);
	yt_database_close(&files->random);
	session_close_game(session);
}

bool
yt_session_registration(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t evaluation_first[] =
	    "UNREGISTERED EVALUATION COPY!";
	static const uint8_t evaluation_second[] =
	    "PLEASE ENCOURAGE YOUR SYSOP TO REGISTER THIS GAME.";
	static const uint8_t invalid_notice[] =
	    " * INVALID REGISTRATION KEY! *";
	struct registration_files files = {0};
	struct registration_text line[REGISTRATION_LINE_COUNT] = {0};
	struct registration_text display[REGISTRATION_DISPLAY_COUNT] = {0};
	uint8_t parsed_key[8];
	uint8_t calculated_key[8];
	uint8_t *storage = NULL;
	uint32_t size;
	struct qb_val_result parsed;
	size_t index;
	bool result = false;

	if (!registration_title(session, error))
		return false;
	storage = malloc((REGISTRATION_LINE_COUNT + REGISTRATION_DISPLAY_COUNT)
	    * REGISTRATION_STRING_CAPACITY);
	if (storage == NULL)
		return registration_error(error, YT_NO_MEMORY,
		    "registration string storage");
	for (index = 0U; index < REGISTRATION_LINE_COUNT; ++index)
		line[index].data = storage + index * REGISTRATION_STRING_CAPACITY;
	for (index = 0U; index < REGISTRATION_DISPLAY_COUNT; ++index)
		display[index].data = storage
		    + (index + REGISTRATION_LINE_COUNT)
		    * REGISTRATION_STRING_CAPACITY;

	session->registered = false;
	if (!registration_close_file4(&files, error))
		goto done;
	if (!registration_resolve_path(&files, error))
		goto done;
	if (!yt_database_open(&files.random, files.path,
	    YT_OPEN_UPDATE_CREATE, error))
		goto done;
	if (!yt_database_random_lof(&files.random, &size, error))
		goto done;
	if (!registration_close_file4(&files, error))
		goto done;
	if (size == 0U) {
		if (!yt_file_kill(files.path, error))
			goto done;
		if (!registration_copy(&display[0], NULL, 0U,
		    evaluation_first, sizeof(evaluation_first) - 1U, error))
			goto done;
		if (!registration_copy(&display[1], NULL, 0U,
		    evaluation_second, sizeof(evaluation_second) - 1U, error))
			goto done;
	}
	else {
		if (!yt_text_input_open(&files.sequential, files.path, error))
			goto done;
		for (index = 0U; index < REGISTRATION_LINE_COUNT; ++index) {
			if (!registration_read_line(&files, &line[index], error))
				goto done;
		}
		if (!registration_close_file4(&files, error))
			goto done;
		line[0].length = qb_title_case_n(line[0].data, line[0].length);
		line[1].length = qb_title_case_n(line[1].data, line[1].length);
		parsed = qb_val_n(line[2].data, line[2].length);
		if (parsed.overflow) {
			(void)registration_error(error, YT_RANGE,
			    "registration key VAL");
			goto done;
		}
		memcpy(parsed_key, parsed.mbf, sizeof(parsed_key));
		if (!registration_arithmetic(line, calculated_key, error))
			goto done;
		if (memcmp(parsed_key, calculated_key, sizeof(parsed_key)) != 0) {
			if (!registration_beep(error))
				goto done;
			if (!registration_beep(error))
				goto done;
			if (!session_present_forced_local_line(invalid_notice,
			    sizeof(invalid_notice) - 1U,
			    "registration forced local row", error))
				goto done;
			if (!registration_beep(error))
				goto done;
			if (!registration_beep(error))
				goto done;
			registration_close_all(session, &files);
			session->running = false;
			session->terminated = true;
			result = true;
			goto done;
		}
		if (!registration_copy(&display[0], NULL, 0U, line[0].data,
		    line[0].length, error))
			goto done;
		if (!registration_copy(&display[1], NULL, 0U, line[1].data,
		    line[1].length, error))
			goto done;
		session->registered = true;
		if (!registration_prepend(&display[0],
		    (const uint8_t *)"Registered to ", 14U, error))
			goto done;
		if (!registration_prepend(&display[1],
		    (const uint8_t *)"Registered by ", 14U, error))
			goto done;
	}
	for (index = 0U; index < REGISTRATION_DISPLAY_COUNT; ++index) {
		if (!registration_centered(session, display[index].data,
		    display[index].length, "registration result row", error))
			goto done;
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "registration result blank", error))
			goto done;
	}
	result = session_wait(session, session->registered ? 2.0 : 10.0,
	    session->registered ? "registration registered wait"
	    : "registration evaluation wait", error);

done:
	if (files.sequential.file != NULL || files.random.file != NULL)
		(void)registration_close_file4(&files, NULL);
	yt_text_input_destroy(&files.sequential);
	free(storage);
	return result;
}
