#include "qb.h"
#include "yt_cli.h"
#include "yt_config.h"
#include "yt_file.h"
#include "yt_init.h"
#include "yt_names.h"
#include "yt_rmt_door.h"
#include "yt_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rmt_remote_info {
	uint8_t identifier[2048];
	size_t identifier_length;
	uint8_t description[2048];
	size_t description_length;
	char first[128];
	char last[128];
	float com_port;
	bool local_mode;
};

struct rmt_output_context {
	bool local_mode;
	struct yt_rmt_door *door;
	struct yt_rmt_output_state state;
};

struct rmt_handoff_file {
	struct yt_database random;
	struct yt_text_input sequential;
};

static void
rmt_handoff_init(struct rmt_handoff_file *handoff)
{
	memset(handoff, 0, sizeof(*handoff));
	yt_text_input_init(&handoff->sequential);
}

static void
rmt_handoff_destroy(struct rmt_handoff_file *handoff)
{
	if (handoff == NULL)
		return;
	yt_database_close(&handoff->random);
	yt_text_input_destroy(&handoff->sequential);
}

static bool
rmt_handoff_close(struct rmt_handoff_file *handoff, struct yt_error *error)
{
	if (handoff->random.file != NULL
	    && !yt_database_random_close(&handoff->random, error))
		return false;
	if (handoff->sequential.file != NULL
	    && !yt_text_input_close(&handoff->sequential, error))
		return false;
	return true;
}

static bool
read_handoff(struct rmt_handoff_file *handoff, char path[512],
    bool *standalone, struct yt_error *error)
{
	struct yt_rmt_handoff_result parsed;
	const uint8_t *line;
	uint32_t size;
	size_t length;
	bool available;

	path[0] = '\0';
	if (!yt_database_open(&handoff->random, "rmtinit.tmp",
	    YT_OPEN_UPDATE_CREATE, error)
	    || !yt_database_random_lof(&handoff->random, &size, error))
		return false;
	if (size == 0U) {
		if (!yt_rmt_handoff_parse(NULL, 0U, &parsed))
			return false;
		*standalone = parsed.standalone;
		return true;
	}
	if (!yt_database_random_close(&handoff->random, error)
	    || !yt_text_input_open(&handoff->sequential, "rmtinit.tmp", error)
	    || !yt_text_input_read_line(&handoff->sequential, &line, &length,
	    &available, error))
		return false;
	/* A physical nonempty file remains the remote branch for an empty line. */
	if (length == 0U) {
		static const uint8_t empty_line[] = "\r";

		line = empty_line;
		length = 1U;
	}
	if (!yt_rmt_handoff_parse(line, length, &parsed)) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	memcpy(path, parsed.path, parsed.path_length + 1U);
	*standalone = parsed.standalone;
	(void)available;
	return yt_text_input_close(&handoff->sequential, error);
}

static bool
read_dorinfo_name(const char *path, struct rmt_remote_info *info,
    struct yt_error *error)
{
	struct yt_rmt_dorinfo_result parsed;
	struct yt_text_file text;
	struct qb_val_result port_value;
	uint8_t storage[2048];
	const uint8_t *identifier;
	const uint8_t *description;
	const uint8_t *first_value;
	const uint8_t *last_value;
	size_t identifier_length;
	size_t description_length;
	size_t first_length;
	size_t last_length;
	bool valid = false;

	if (info == NULL)
		return false;
	memset(info, 0, sizeof(*info));
	if (!yt_text_read(path, &text, error))
		return false;
	if (!yt_rmt_dorinfo_parse(text.data, text.length, storage,
	    sizeof(storage), &parsed)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "parse RMT DORINFO");
		}
		goto done;
	}
	if (parsed.outcome != YT_RMT_DORINFO_SUCCESS) {
		if (error != NULL) {
			error->status = YT_EOF;
			snprintf(error->operation, sizeof(error->operation),
			    "read RMT DORINFO field %zu", parsed.failed_field);
		}
		goto done;
	}
	identifier = yt_rmt_dorinfo_field(&parsed, storage, 3U,
	    &identifier_length);
	description = yt_rmt_dorinfo_field(&parsed, storage, 4U,
	    &description_length);
	first_value = yt_rmt_dorinfo_field(&parsed, storage, 6U,
	    &first_length);
	last_value = yt_rmt_dorinfo_field(&parsed, storage, 7U, &last_length);
	if (identifier == NULL || description == NULL || first_value == NULL
	    || last_value == NULL || first_length >= 128U
	    || last_length >= 128U
	    || identifier_length > sizeof(info->identifier)
	    || description_length > sizeof(info->description)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "copy RMT DORINFO fields");
		}
		goto done;
	}
	if (identifier_length != 0U
	    && identifier[identifier_length - 1U] == ':')
		--identifier_length;
	port_value = qb_val_n(identifier_length != 0U
	    ? identifier + identifier_length - 1U : NULL,
	    identifier_length != 0U ? 1U : 0U);
	if (identifier_length != 0U)
		memcpy(info->identifier, identifier, identifier_length);
	info->identifier_length = identifier_length;
	if (description_length != 0U)
		memcpy(info->description, description, description_length);
	info->description_length = description_length;
	info->com_port = (float)port_value.value;
	if (first_length != 0U)
		memcpy(info->first, first_value, first_length);
	info->first[first_length] = '\0';
	if (last_length != 0U)
		memcpy(info->last, last_value, last_length);
	info->last[last_length] = '\0';
	info->local_mode = info->com_port < 1.0f || info->com_port > 4.0f;
	valid = true;

done:
	yt_text_free(&text);
	return valid;
}

static bool
credited_remote_name(const char *first, const char *last, char credited[90],
    struct yt_error *error)
{
	struct yt_database alias_random = {0};
	struct yt_name_file names;
	bool result;

	if (!yt_database_random_close(&alias_random, error)
	    || !yt_names_load("YTNAME.DAT", &names, error))
		return false;
	result = yt_rmt_credited_name(first, last, &names, credited, 90U);
	yt_names_free(&names);
	return result;
}

static bool
write_local_bytes(struct yt_rmt_door *door, const uint8_t *bytes,
    size_t length, struct yt_error *error)
{
	if (door != NULL && door->initialized) {
		if (yt_rmt_door_local_write(door, bytes, length))
			return true;
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "write RMT-INIT local screen");
		}
		return false;
	}
	if (length != 0U && fwrite(bytes, 1U, length, stdout) != length) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "write RMT-INIT console");
		}
		return false;
	}
	return true;
}

static bool
write_rmt_output(struct rmt_output_context *context,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	struct yt_rmt_output_apply_result applied;
	struct yt_rmt_output_result output;
	struct yt_rmt_output_state final_state;
	struct yt_rmt_output_sink sink;
	uint8_t local[256];
	uint8_t serial[256];

	if (context == NULL
	    || !yt_rmt_output_compose_state(entry, payload, payload_length,
	    context->local_mode, &context->state, local, sizeof(local), serial,
	    sizeof(serial), &output, &final_state)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "compose RMT-INIT output row");
		}
		return false;
	}
	if (context->local_mode) {
		if (!write_local_bytes(context->door, local, output.local_length,
		    error))
			return false;
		context->state = final_state;
		return true;
	}
	yt_rmt_door_sink(context->door, &sink);
	if (!yt_rmt_output_apply(local, serial, &output, &sink, &applied)) {
		if (error != NULL) {
			error->status = YT_INVALID;
			snprintf(error->operation, sizeof(error->operation),
			    "apply RMT-INIT output row");
		}
		return false;
	}
	if (applied.outcome == YT_RMT_OUTPUT_APPLY_SUCCESS) {
		context->state = final_state;
		return true;
	}
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		error->system_error = 0;
		snprintf(error->operation, sizeof(error->operation),
		    "write RMT-INIT %s endpoint",
		    applied.outcome == YT_RMT_OUTPUT_APPLY_SERIAL_FAILURE
		    ? "serial" : "local");
	}
	return false;
}

static bool
write_rmt_line(struct rmt_output_context *context,
    const uint8_t *payload, size_t payload_length, struct yt_error *error)
{
	return write_rmt_output(context, YT_RMT_OUTPUT_LINE, payload,
	    payload_length, error);
}

static bool
write_rmt_blank(struct rmt_output_context *context, struct yt_error *error)
{
	return write_rmt_output(context, YT_RMT_OUTPUT_BLANK, NULL, 0U, error);
}

static bool
write_missing_old_data(struct rmt_output_context *context,
    struct yt_error *error)
{
	static const uint8_t payload[] =
	    "\aERROR! OLD DATA FILES NOT FOUND!!!!!!!!!!!!!!!!!!!!!!!!\a";

	/* The two preceding PRINTs are local-only blank rows. */
	if (!write_local_bytes(context->door, (const uint8_t *)"\r\r", 2U,
	    error)
	    || !write_rmt_line(context, payload, sizeof(payload) - 1U,
	    error))
		return false;
	return true;
}

static bool
write_rmt_completion(struct rmt_output_context *context,
    const char *credited, struct yt_error *error)
{
	struct yt_rmt_completion_result completion;
	struct yt_rmt_delay_result delay;

	if (!yt_rmt_completion_compose(context->local_mode, credited,
	    &completion)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "compose RMT-INIT completion");
		}
		return false;
	}
	for (size_t line = 0U; line < completion.line_count; ++line) {
		bool written;

		if (completion.returns_to_bbs
		    && line + 1U == completion.line_count
		    && !write_rmt_blank(context, error))
			return false;
		written = completion.lines[line].length == 0U
		    ? write_rmt_blank(context, error)
		    : write_rmt_line(context, completion.lines[line].bytes,
		    completion.lines[line].length, error);

		if (!written)
			return false;
	}
	if (completion.returns_to_bbs && !yt_rmt_completion_delay(&delay)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "execute RMT-INIT completion delay");
		}
		return false;
	}
	return true;
}

static bool
write_rmt_presentation(void *opaque, uint16_t site,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error)
{
	struct rmt_output_context *context = opaque;

	if (!write_rmt_output(context, entry, payload, payload_length, error))
		return false;
	if (site == 0x10f1U && !context->local_mode
	    && context->state.serial_column > 50U)
		return write_rmt_output(context, YT_RMT_OUTPUT_SERIAL_LINE, NULL,
		    0U, error);
	return true;
}

static int
finish_rmt(struct rmt_handoff_file *handoff, struct yt_rmt_door *door,
    int status)
{
	rmt_handoff_destroy(handoff);
	yt_rmt_door_finish(door, status);
	return status;
}

static bool
rmt_close_all_com(void *context, int8_t file_class, struct yt_error *error)
{
	if (file_class != -4 && file_class != -5) {
		if (error != NULL) {
			error->status = YT_INVALID;
			error->system_error = 0;
			snprintf(error->operation, sizeof(error->operation),
			    "RMT CLOSE-all COM class");
			error->path[0] = '\0';
		}
		return false;
	}
	yt_rmt_door_finish(context, EXIT_SUCCESS);
	return true;
}

static bool
rmt_close_all(struct yt_database *database, struct yt_rmt_door *door,
    float com_port, struct yt_error *error)
{
	struct yt_close_all_control controls[2];
	size_t control_count = 0U;

	/* The COM alias is allocated before the later YTDATA random block. */
	if (door != NULL && (door->initialized || door->serial.prepared)) {
		controls[control_count++] = (struct yt_close_all_control){
			.heap_type = YT_CLOSE_ALL_HEAP_FILE,
			.file_class = com_port == 1.0f || com_port == 3.0f
			    ? -4 : -5,
			.method = rmt_close_all_com,
			.context = door,
		};
	}
	if (database != NULL && database->file != NULL) {
		controls[control_count++] = (struct yt_close_all_control){
			.heap_type = YT_CLOSE_ALL_HEAP_FILE,
			.file_class = 0,
			.method = yt_database_close_all_method,
			.context = database,
		};
	}
	return yt_close_all_run(control_count != 0U ? controls : NULL,
	    control_count, NULL, NULL, error);
}

int
main(void)
{
	struct yt_error error;
	struct rmt_handoff_file handoff_file;
	struct yt_database old;
	struct yt_config config;
	struct yt_random random;
	struct yt_rmt_door door;
	struct rmt_output_context output_context = {0};
	struct yt_rmt_presenter presenter = {
		.context = &output_context,
		.write = write_rmt_presentation,
	};
	struct rmt_remote_info remote;
	struct yt_rmt_serial_state serial_state;
	struct yt_startup_framing framing;
	char handoff[512];
	char answer[80];
	char credited[90];
	bool standalone;
	bool local_mode;
	float com_port = 0.0f;
	uint32_t old_size;
	uint8_t dll;
	uint8_t dlm;

	yt_error_clear(&error);
	rmt_handoff_init(&handoff_file);
	memset(&old, 0, sizeof(old));
	memset(&door, 0, sizeof(door));
	memset(&remote, 0, sizeof(remote));
	if (!read_handoff(&handoff_file, handoff, &standalone, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (standalone) {
		struct yt_rmt_standalone_output output;

		if (!yt_rmt_standalone_prompt_compose(&output)
		    || !write_local_bytes(&door, output.bytes, output.length, &error)) {
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
		if (!yt_cli_line(answer, sizeof(answer)))
			return finish_rmt(&handoff_file, &door, EXIT_SUCCESS);
		if (!yt_rmt_standalone_response_compose(
		    (const uint8_t *)answer, strlen(answer), &output)
		    || !write_local_bytes(&door, output.bytes, output.length, &error)) {
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
		if (!output.proceed)
			return finish_rmt(&handoff_file, &door, EXIT_SUCCESS);
		local_mode = true;
		strcpy(credited, "The Sysop");
	}
	else {
		local_mode = false;
		credited[0] = '\0';
	}
	if (!rmt_handoff_close(&handoff_file, &error)
	    || !yt_file_kill("rmtinit.tmp", NULL, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (!standalone) {
		struct yt_rmt_standalone_output output;

		if (!read_dorinfo_name(handoff, &remote, &error)) {
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
		local_mode = remote.local_mode;
		com_port = remote.com_port;
		if (!local_mode) {
			if (!yt_startup_framing_compose(remote.description,
			    remote.description_length, &framing)
			    || !yt_rmt_door_prepare(&door, (int)com_port, &framing,
			    &error)
			    || !yt_startup_divisor_from_observed_baud(
			    door.serial.observed_baud, &dll, &dlm)
			    || !yt_rmt_serial_state_compose(remote.identifier,
			    remote.identifier_length, remote.description,
			    remote.description_length, dll, dlm, &serial_state)
			    || !yt_rmt_door_start(&door, &serial_state, &error)) {
				if (error.status == YT_OK) {
					error.status = YT_RANGE;
					snprintf(error.operation, sizeof(error.operation),
					    "adapt observed RMT serial speed");
				}
				yt_cli_error("RMT-INIT", &error);
				return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
			}
		}
		if (!yt_rmt_remote_status_compose(!local_mode, com_port,
		    local_mode ? 0.0f : serial_state.detected_baud, &output)
		    || !write_local_bytes(&door, output.bytes, output.length, &error)
		    || !credited_remote_name(remote.first, remote.last, credited,
		    &error)) {
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
	}
	output_context.local_mode = local_mode;
	output_context.door = &door;
	if (!yt_database_open(&old, "YTDATA.DAT", YT_OPEN_UPDATE_CREATE,
	    &error)
	    || !yt_database_random_lof(&old, &old_size, &error)) {
		yt_database_close(&old);
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (old_size == 0U) {
		if (!write_missing_old_data(&output_context, &error)
		    || !rmt_close_all(&old, &door, com_port, &error)) {
			yt_database_close(&old);
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
		if (!yt_file_kill("YTDATA.DAT", NULL, &error)) {
			yt_cli_error("RMT-INIT", &error);
			return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
		}
		return finish_rmt(&handoff_file, &door, EXIT_SUCCESS);
	}
	if (!yt_config_load(&old, &config, &error)
	    || !yt_rmt_preprocess_old_database(&old, &config, &error)) {
		yt_database_close(&old);
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (!yt_database_random_close(&old, &error)) {
		yt_database_close(&old);
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	yt_rmt_normalize_config(&config, local_mode || standalone);
	yt_random_init(&random);
	if (!yt_initialize_rmt_presented(&config, credited, &random, &presenter,
	    &error)) {
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (!write_rmt_completion(&output_context, credited, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	if (!rmt_close_all(NULL, &door, com_port, &error)) {
		yt_cli_error("RMT-INIT", &error);
		return finish_rmt(&handoff_file, &door, EXIT_FAILURE);
	}
	return finish_rmt(&handoff_file, &door, EXIT_SUCCESS);
}
