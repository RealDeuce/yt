#include "yt_session_internal.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_input.h"
#include "yt_main_error.h"
#include "yt_maint.h"
#include "yt_names.h"
#include "yt_output.h"
#include "yt_pager.h"
#include "yt_platform.h"
#include "yt_route.h"
#include "yt_sound.h"
#include "yt_startup_model.h"
#include "yt_text.h"
#include "yt_team.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define YT_PLAYER_FIRST YT_PLAYER_FIRST_RECORD
#define YT_PLAYER_LAST YT_PLAYER_LAST_RECORD

static bool
session_is_destroyed(const struct yt_session *session)
{
	return session->destroyed;
}

float
session_sector_offset(const struct yt_session *session)
{
	return session->door->game.config.sector_offset;
}

float
session_port_offset(const struct yt_session *session)
{
	return session->door->game.config.port_offset;
}

float
session_planet_offset(const struct yt_session *session)
{
	return session->door->game.config.planet_offset;
}

static float
session_foreground(const struct yt_session *session)
{
	return session->foreground;
}

static int
session_pager_foreground(const struct yt_session *session)
{
	return (int)session_foreground(session);
}

void
session_set_pager_line_count_raw(struct yt_session *session,
    const uint8_t raw[4])
{
	session->pager.line_count = qb_mbf32_decode(raw);
}

void
session_set_pager_line_count(struct yt_session *session, float value)
{
	session->pager.line_count = value;
}

static void
session_set_pager_nonstop(struct yt_session *session, float value)
{
	session->pager.nonstop = value;
}

static void
session_set_pager_newline(struct yt_session *session, float value)
{
	session->pager.newline_flag = value;
}

void
session_set_foreground(struct yt_session *session, float value)
{
	session->foreground = value;
	session->presentation.foreground = value;
	session->pager.foreground = (int)value;
}

bool
session_current_date_serial(struct yt_session *session, int *serial,
    int *adjusted_year, struct yt_error *error)
{
	return yt_current_date_serial(session->door->game.config.epoch_year,
	    serial, adjusted_year, error);
}

int
session_record(const struct yt_session *session)
{
	return session->player_record_carrier;
}

static void
session_set_current_player_record(struct yt_session *session, int record)
{
	session->player_record_carrier = record;
}

bool yt_session_build_route(struct yt_session *session, float start,
    float destination, int16_t *next_hop, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error);
bool session_present_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length);
static bool session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length);
bool session_present_paged_line(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error);
bool
session_is_disruption_sector(const struct yt_session *session, float sector)
{
	return sector == session->disruption_sectors[0]
	    || sector == session->disruption_sectors[1];
}

static bool
session_timed_wait(struct yt_session *session, double seconds)
{
	struct yt_input_value selected = {{0, 0}, 0, false};
	uint64_t deadline_milliseconds;
	uint64_t duration_milliseconds;
	DWORD current_seconds;
	WORD current_milliseconds;
	bool timed_out;

	if (!isfinite(seconds))
		return false;
	if (seconds <= 0.0)
		return true;
	if (seconds > (double)UINT32_MAX)
		return false;
	od_get_time(&current_seconds, &current_milliseconds);
	duration_milliseconds = (uint64_t)llround(seconds * 1000.0);
	if (duration_milliseconds == 0U)
		duration_milliseconds = 1U;
	deadline_milliseconds = (uint64_t)current_seconds * 1000U
	    + current_milliseconds + duration_milliseconds;
	if (deadline_milliseconds > (uint64_t)UINT32_MAX * 1000U + 999U)
		return false;
	return yt_input_wait_until(&session->input,
	    (uint32_t)(deadline_milliseconds / 1000U),
	    (uint16_t)(deadline_milliseconds % 1000U), &selected, &timed_out);
}

bool
session_wait(struct yt_session *session, double seconds,
    const char *operation, struct yt_error *error)
{
	if (session_timed_wait(session, seconds))
		return true;
	if (error != NULL) {
		error->status = YT_IO_ERROR;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_range_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
session_returning_rebuild_wait(struct yt_session *session,
    struct yt_error *error)
{
	return session_wait(session, 5.0, "returning-player rebuild wait",
	    error);
}

void
session_close_game(struct yt_session *session)
{
	if (session->door->game_open) {
		session->door->game_open = false;
		yt_game_close(&session->door->game);
	}
}

static bool
session_editor_repeat_emit(void *context, const uint8_t *prefix, size_t length)
{
	struct yt_session *session = context;

	session_set_pager_newline(session, 1.0f);
	return session_present_paged_row(session, prefix, length);
}

static bool
session_editor_submit_line(void *context)
{
	struct yt_session *session = context;
	struct yt_present_result presentation;
	enum yt_present_status status;

	session_set_pager_newline(session, 0.0f);
	status = yt_present_line(NULL, 0, &session->presentation,
	    &presentation);
	if (status != YT_PRESENT_OK)
		return false;
	yt_out_present_result(&presentation);
	return true;
}

static bool
session_editor_echo(void *context, const uint8_t *local,
    size_t local_length, const uint8_t *remote, size_t remote_length)
{
	struct yt_session *session = context;
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_editor_echo(local, local_length, remote,
	    remote_length, &session->presentation, &presentation);
	if (status != YT_PRESENT_OK)
		return false;
	yt_out_present_result(&presentation);
	return true;
}

static bool
session_editor_continue(void *context)
{
	struct yt_session *session = context;

	session_set_pager_newline(session, 1.0f);
	od_kernel();
	return true;
}

static float
single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

uint32_t
session_sector_basic_record(const struct yt_session *session,
    float logical_sector)
{
	return (uint32_t)yt_sector_basic_record(&session->door->game.config,
	    (int)logical_sector);
}

uint32_t
session_port_basic_record(const struct yt_session *session, float logical_port)
{
	return (uint32_t)yt_port_basic_record(&session->door->game.config,
	    (int)logical_port);
}

uint32_t
session_planet_basic_record(const struct yt_session *session,
    float logical_planet)
{
	return (uint32_t)yt_planet_basic_record(&session->door->game.config,
	    (int)logical_planet);
}

bool
session_read_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

bool
session_write_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &sector->record, error);
}

bool
session_read_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

bool
session_write_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &port->record, error);
}

bool
session_read_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
session_write_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	yt_planet_encode(planet);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &planet->record, error);
}

int
session_sector_count(const struct yt_session *session)
{
	return (int)(session_port_offset(session)
	    - session_sector_offset(session));
}

bool
session_write_player(struct yt_session *session, struct yt_error *error)
{
	return yt_game_write_player(&session->door->game,
	    session_record(session), &session->player, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static void
attach_database_get_fault(struct yt_session *session, struct yt_error *error,
    enum yt_basic_fault_site site)
{
	const struct yt_database_get_result *result =
	    &session->door->game.database.last_get;
	bool raised = result->outcome == YT_DATABASE_GET_RECORD_ERROR
	    || result->outcome == YT_DATABASE_GET_SEEK_ERROR
	    || result->outcome == YT_DATABASE_GET_READ_ERROR;

	if (!raised || !yt_error_attach_basic_fault_number(error, site,
	    result->basic_error))
		(void)yt_error_attach_basic_fault(error, site);
}

static void
attach_database_put_fault(struct yt_session *session, struct yt_error *error,
    enum yt_basic_fault_site site)
{
	const struct yt_database_put_result *result =
	    &session->door->game.database.last_put;
	bool raised = result->outcome == YT_DATABASE_PUT_RECORD_ERROR
	    || result->outcome == YT_DATABASE_PUT_SEEK_ERROR
	    || result->outcome == YT_DATABASE_PUT_WRITE_ERROR
	    || result->outcome == YT_DATABASE_PUT_REJECTED_SHORT;

	if (raised && !yt_error_attach_basic_fault_number(error, site,
	    result->basic_error))
		(void)yt_error_attach_basic_fault(error, site);
}

static bool
basic_fault_retries(const struct yt_error *error)
{
	struct yt_basic_fault_projection projection;

	return yt_basic_fault_project(error, NULL, 0U, NULL, 0U, NULL, 0U,
	    &projection)
	    && projection.disposition == YT_BASIC_FAULT_RETRY_STATEMENT;
}

bool
read_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error)
{
	for (;;) {
		if (yt_database_read(&session->door->game.database,
		    (size_t)physical_record, record, error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_player_at_fault(struct yt_session *session, int player_record,
    struct yt_player *player, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (yt_game_read_player(&session->door->game, player_record, player,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_sector_at_fault(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (session_read_sector(session, logical_sector, sector,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
session_read_port_at_fault(struct yt_session *session, int logical_port,
    struct yt_port *port, enum yt_basic_fault_site site,
    struct yt_error *error)
{
	for (;;) {
		if (session_read_port(session, logical_port, port,
		    error))
			return true;
		attach_database_get_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
}

bool
write_database_record_at_fault(struct yt_session *session,
    uint32_t physical_record, const struct yt_record *record,
    enum yt_basic_fault_site site, struct yt_error *error)
{
	for (;;) {
		if (yt_database_write(&session->door->game.database,
		    (size_t)physical_record, record, error))
			break;
		attach_database_put_fault(session, error, site);
		if (!basic_fault_retries(error))
			return false;
		yt_error_clear(error);
	}
	return yt_database_flush(&session->door->game.database, error);
}

bool
session_reload_player(struct yt_session *session, struct yt_error *error)
{
	struct yt_player fresh;

	if (!session_read_player_at_fault(session, session_record(session), &fresh,
	    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_GET, error)
	    || !yt_current_player_hydrate(&session->player, &fresh,
	    session_record(session), session_sector_offset(session),
	    session->anti_cloak_enabled, &session->current_sector_record,
	    &session->player_cache, error))
		return false;
	session->combat_ship_fighters = session->player.fighters;
	session->combat_ship_shields = session->player.shields;
	return true;
}

bool
session_mutate_player_credits(struct yt_session *session, float argument,
    bool *hydrated, struct yt_error *error)
{
	uint8_t raw[4];
	float sum;
	float result;

	if (hydrated != NULL)
		*hydrated = false;
	if (qb_mbf32_encode(argument, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation argument MBF32");
		}
		return false;
	}
	if (!session_reload_player(session, error))
		return false;
	if (hydrated != NULL)
		*hydrated = true;
	sum = single_add(session->player.credits, argument);
	result = floorf(sum);
	if (qb_mbf32_encode(sum, raw) == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(result, raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation result MBF32");
		}
		return false;
	}
	session->player.credits = qb_mbf32_decode(raw);
	if (!yt_record_set_raw_number(&session->player.record, YT_F81, raw)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation overlay");
		}
		return false;
	}
	return yt_database_write_durable(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}

bool
session_append_news(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	(void)session;
	return yt_news_append(text, error);
}

bool
session_append_news_bytes(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	(void)context;
	return yt_news_append_bytes(text, length, error);
}

static bool
read_keyboard_line(struct yt_session *session, char *dest, size_t size)
{

	if (size == 0)
		return false;
	yt_pager_editor_enter(&session->pager, session->command_accumulator,
	    sizeof(session->command_accumulator));
	dest[0] = '\0';
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};
		bool queued = session->queue_position < session->queue_length;
		uint8_t key;

		if (queued) {
			if (!yt_input_queue_pop(session->queue,
			    sizeof(session->queue), &session->queue_position,
			    &session->queue_length, &selected))
				return false;
		}
		else {
			if (!yt_input_wait(&session->input, &selected))
				return false;
		}
		if (selected.length != 1)
			continue;
		key = selected.bytes[0];
		if (yt_input_repeat_requested(queued, &selected)) {
			if (!yt_input_repeat_current_command(
			    session->command_accumulator,
			    sizeof(session->command_accumulator),
			    session->saved_command,
			    sizeof(session->saved_command),
			    session->paged_text,
			    sizeof(session->paged_text),
			    &session->pager.newline_flag, &key,
			    session_editor_repeat_emit, session))
				return false;
		}
		if (yt_input_submit_requested(key)) {
			if (!yt_input_submit(
			    &session->pager.newline_flag,
			    session_editor_submit_line, session))
				return false;
			snprintf(dest, size, "%s", session->command_accumulator);
			return true;
		}
		{
			bool handled;

			if (!yt_input_apply_backspace(key,
			    session->command_accumulator,
			    sizeof(session->command_accumulator), &handled,
			    session_editor_echo, session))
				return false;
			if (handled)
				continue;
		}
		{
			bool handled;

			if (!yt_input_append_printable(key,
			    session->command_accumulator,
			    sizeof(session->command_accumulator), size,
			    session->paged_text, sizeof(session->paged_text),
			    &session->pager.newline_flag, &handled,
			    session_editor_echo,
			    session_editor_continue, session))
				return false;
			if (handled)
				continue;
		}
	}
}

void
session_clear_queue(struct yt_session *session)
{
	(void)yt_input_queue_clear(session->queue, sizeof(session->queue),
	    &session->queue_position, &session->queue_length);
}

static bool
session_command_notice(struct yt_session *session, const char *text)
{
	if (!session_present_paged_line(session, (const uint8_t *)text, strlen(text),
	    "command notice", NULL))
		return false;
	return session_wait(session, 1.0, "command notice wait", NULL);
}

void
session_compat_upper_n(struct yt_session *session, uint8_t *text,
    size_t length)
{
	(void)session;
	yt_input_compat_upper_n(text, length);
}

static bool
expand_repeat(struct yt_session *session, char *text, size_t size)
{
	struct yt_repeat_transform result;

	if (!yt_input_expand_repeat_with_notice(text, size,
	    session->saved_command, sizeof(session->saved_command),
	    session->output_source, sizeof(session->output_source), &result)) {
		if (result.fault_valid && session->error != NULL) {
			yt_error_clear(session->error);
			session->error->status = YT_RANGE;
			(void)snprintf(session->error->operation,
			    sizeof(session->error->operation), "%s",
			    result.failure == YT_REPEAT_FAILURE_VAL_OVERFLOW
			    ? "ADE0 repeat VAL overflow"
			    : "ADE0 repeat SINGLE overflow");
			(void)yt_error_attach_basic_fault_number(session->error,
			    result.fault_site, 6U);
		}
		return false;
	}
	if (!result.emit_notice)
		return true;
	if (result.bold_committed)
		yt_present_set_bold(&session->presentation, 1.0f);
	return session_command_notice(session, session->output_source);
}

static bool
session_line(struct yt_session *session, char *text, size_t size)
{
	struct yt_command_save_transform save;

	if (!read_keyboard_line(session, text, size))
		return false;
	if (!yt_input_command_save_staged(text, size, session->queue,
	    sizeof(session->queue), &session->queue_position,
	    &session->queue_length, session->saved_command,
	    sizeof(session->saved_command), session->output_source,
	    sizeof(session->output_source), YT_BASIC_FAULT_SITE_COUNT, &save))
		return false;
	if (save.notice_ready) {
		if (!session_command_notice(session, session->output_source))
			return false;
	}
	if (!expand_repeat(session, text, size))
		return false;
	return yt_input_split_semicolon(text, session->queue,
	    sizeof(session->queue), &session->queue_position,
	    &session->queue_length);
}

bool
session_read_command(struct yt_session *session, char *text, size_t size)
{
	return session_line(session, text, size)
	    && session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

bool
session_read_upper_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_read_command(session, text, size))
		return false;
	session_compat_upper_n(session, (uint8_t *)text, strlen(text));
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

bool
session_read_number_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_read_upper_command(session, text, size))
		return false;
	if (strchr(text, 'E') != NULL)
		text[0] = '\0';
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_paged_kernel(void *context)
{
	(void)context;
	od_kernel();
	return true;
}

static bool
session_paged_sample(void *context, struct yt_input_value *sampled)
{
	struct yt_session *session = context;

	return yt_input_poll(&session->input, sampled);
}

static bool
session_paged_present(void *context, const uint8_t *text, size_t length)
{
	struct yt_session *session = context;
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_paged_text(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	return status == YT_PRESENT_OK;
}

static bool
session_paged_finish(void *context, bool newline_flag)
{
	struct yt_session *session = context;
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_paged_finish(newline_flag,
	    &session->presentation, &presentation);
	yt_out_present_result(&presentation);
	return status == YT_PRESENT_OK;
}

static bool
session_paged_response(void *context, char *response, size_t capacity)
{
	return read_keyboard_line(context, response, capacity);
}

static bool
session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length)
{
	if ((text == NULL && length != 0U)
	    || length >= sizeof(session->output_source))
		return false;
	if (length != 0U)
		memmove(session->output_source, text, length);
	session->output_source[length] = '\0';
	return true;
}

bool
session_present_paged_row(struct yt_session *session, const uint8_t *text, size_t length)
{
	static const struct yt_paged_row_ops ops = {
		session_paged_kernel,
		session_paged_sample,
		session_paged_present,
		session_paged_finish,
		session_paged_response,
	};
	struct yt_pager_key_state key_state = {
		.accumulator = session->command_accumulator,
		.accumulator_capacity = sizeof(session->command_accumulator),
		.queue = session->queue,
		.queue_capacity = sizeof(session->queue),
		.queue_position = &session->queue_position,
		.queue_length = &session->queue_length,
		.pager_key = session->pager.key,
		.pager_key_capacity = sizeof(session->pager.key),
	};
	bool result;

	if (!session_store_output_source(session, text, length))
		return false;
	result = yt_paged_row_run(&session->pager, &session->presentation,
	    &key_state, text, length, &ops, session);
	return result;
}

void
session_set_color(struct yt_session *session, int logical)
{
	static const int pc_color[8] = {0, 4, 2, 6, 1, 5, 3, 7};

	session_set_foreground(session, (float)logical);
	yt_present_set_background(&session->presentation, 0.0f);
	if (logical >= 0 && logical < 8)
		od_set_color(pc_color[logical], 0);
}

bool
session_sound(struct yt_session *session, float selector,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_sound(selector, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_attention_bytes(struct yt_session *session, const uint8_t *text,
    size_t length,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_attention(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_present_text(struct yt_session *session, const uint8_t *text,
    size_t length, enum session_present_text_kind kind,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	switch (kind) {
	case SESSION_PRESENT_LINE:
		status = yt_present_line(text, length, &session->presentation,
		    &presentation);
		break;
	case SESSION_PRESENT_RAW:
		status = yt_present_character(text, length,
		    &session->presentation, &presentation);
		break;
	case SESSION_PRESENT_BOLD_LINE:
		status = yt_present_bold_line(text, length,
		    &session->presentation, &presentation);
		break;
	case SESSION_PRESENT_BOLD_RAW:
		status = yt_present_bold_character(text, length,
		    &session->presentation, &presentation);
		break;
	default:
		status = YT_PRESENT_RANGE;
		break;
	}
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_present_forced_local_line(const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_forced_local_line(text, length, &presentation);
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
session_local_line(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_local_line(text, length,
	    &session->presentation, &presentation);

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
session_main_error_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "main error fatal row", error);
}

enum session_fault_disposition {
	SESSION_FAULT_UNHANDLED,
	SESSION_FAULT_RESUME_GAMEPLAY,
	SESSION_FAULT_ENDED,
	SESSION_FAULT_HANDLER_FAILED,
};

static bool
session_commit_shared_terminal(struct yt_session *session,
    const struct yt_shared_error_result *result, struct yt_error *error)
{
	size_t index;

	if (!session_local_line(session, result->debug, result->debug_length,
	    "shared error debug row", error))
		return false;
	for (index = 0U; index < result->event_count; ++index) {
		const struct yt_shared_error_event *event = &result->events[index];

		switch (event->destination) {
		case YT_SHARED_ERROR_LOCAL_DIAGNOSTIC:
			if (!session_local_line(session, event->data,
			    event->length, "shared error local row", error))
				return false;
			break;
		case YT_SHARED_ERROR_SESSION_AND_NEWS:
			if (!session_present_text(session, event->data,
			    event->length, SESSION_PRESENT_LINE,
			    "shared error session row", error)
			    || !session_append_news_bytes(session, event->data,
			    event->length, error))
				return false;
			break;
		case YT_SHARED_ERROR_NEWS:
			if (!session_append_news_bytes(session, event->data,
			    event->length, error))
				return false;
			break;
		}
	}
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	yt_error_clear(error);
	return true;
}

static enum session_fault_disposition
session_route_basic_fault(struct yt_session *session, struct yt_error *error)
{
	struct yt_basic_fault_projection projection;
	const struct yt_basic_fault_identity *identity;
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];

	if (error == NULL || !error->basic_fault_valid
	    || !error->basic_error_valid)
		return SESSION_FAULT_UNHANDLED;
	identity = yt_basic_fault_identity(
	    (enum yt_basic_fault_site)error->basic_fault_site);
	if (identity == NULL)
		return SESSION_FAULT_UNHANDLED;
	if (!yt_basic_fault_project(error, (const uint8_t *)error->path,
	    strlen(error->path), NULL, 0U, NULL, 0U, &projection))
		return SESSION_FAULT_UNHANDLED;
	if (projection.disposition == YT_BASIC_FAULT_RETRY_STATEMENT
	    || projection.disposition == YT_BASIC_FAULT_RESUME_MISSING_FILE)
		return SESSION_FAULT_UNHANDLED;
	if (identity->module == YT_BASIC_FAULT_MAIN) {
		if (!session_present_forced_local_line(projection.main.debug,
		    projection.main.debug_length, "main error debug row", error))
			return SESSION_FAULT_HANDLER_FAILED;
		if (projection.disposition == YT_BASIC_FAULT_RESUME_GAMEPLAY)
			return SESSION_FAULT_RESUME_GAMEPLAY;
		if (!yt_platform_clock(&date_now, error)
		    || !yt_platform_clock(&time_now, error))
			return SESSION_FAULT_HANDLER_FAILED;
		yt_format_date(&date_now, date);
		yt_format_time(&time_now, time_text);
		if (!yt_main_error_compose((int16_t)projection.error_number,
		    identity->source_line, (const uint8_t *)error->path,
		    strlen(error->path), (const uint8_t *)date, strlen(date),
		    (const uint8_t *)time_text, strlen(time_text),
		    &projection.main))
			return SESSION_FAULT_HANDLER_FAILED;
		if (!yt_main_error_commit_fatal(&projection.main,
		    session_main_error_present, session, error))
			return SESSION_FAULT_HANDLER_FAILED;
	}
	else {
		if (!session_commit_shared_terminal(session, &projection.shared,
		    error))
			return SESSION_FAULT_HANDLER_FAILED;
		return SESSION_FAULT_ENDED;
	}
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	yt_error_clear(error);
	return SESSION_FAULT_ENDED;
}

static bool
session_handle_gameplay_fault(struct yt_session *session,
    struct yt_error *error, bool *resume_gameplay)
{
	enum session_fault_disposition disposition;

	if (resume_gameplay == NULL)
		return false;
	*resume_gameplay = false;
	if (session->terminated)
		return true;
	disposition = session_route_basic_fault(session, error);
	if (disposition == SESSION_FAULT_ENDED)
		return true;
	if (disposition != SESSION_FAULT_RESUME_GAMEPLAY)
		return false;
	yt_error_clear(error);
	*resume_gameplay = true;
	return true;
}

bool
session_present_paged_fragment(struct yt_session *session,
    const uint8_t *text, size_t length)
{
	session_set_pager_newline(session, 0.0f);
	return session_present_paged_row(session, text, length);
}

bool
session_present_paged_line(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	return session_present_paged_fragment(session, text, length);
}

bool
session_present_alert(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	session_clear_queue(session);
	return session_present_paged_fragment(session, text, length);
}

static bool
session_low_time(struct yt_session *session, const char *operation,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	bool warned;

	status = yt_present_low_time(session->time.text,
	    session->time.text_length, &session->low_time_remembered,
	    &session->presentation, &presentation, &warned);
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_present_timed_paged_row(struct yt_session *session,
    const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_low_time(session, operation, error))
		return false;
	session_set_pager_newline(session, 1.0f);
	return session_present_paged_row(session, text, length);
}

static bool
session_drain_pending_input(struct yt_session *session)
{
	struct yt_input_drain_state drain;
	enum yt_input_drain_reason reason;

	if (!yt_input_drain_begin(&drain, &session->input_residue))
		return false;
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!yt_input_poll_source(&session->input, false, &selected))
			return false;
		reason = yt_input_drain_local(&drain, &selected);
		if (reason == YT_INPUT_DRAIN_ERROR)
			return false;
		if (reason == YT_INPUT_DRAIN_LOCAL_COMPLETE)
			break;
	}
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (session->presentation.sound.mode == 0.0f
		    && !yt_input_poll_source(&session->input, true, &selected))
			return false;
		reason = yt_input_drain_serial(&drain,
		    session->presentation.sound.mode, &selected);
		if (reason == YT_INPUT_DRAIN_ERROR)
			return false;
		if (reason == YT_INPUT_DRAIN_COMPLETE)
			break;
	}
	memset(&session->input_residue, 0, sizeof(session->input_residue));
	if (drain.residue_length != 0)
		memcpy(session->input_residue.bytes, drain.residue,
		    drain.residue_length);
	session->input_residue.length = drain.residue_length;
	return true;
}

bool
session_press_any_key(struct yt_session *session, bool drain,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	enum yt_status failure_status = YT_RANGE;
	const char *failure_operation = "press any key presentation";
	float saved_foreground;

	if (drain && !session_drain_pending_input(session)) {
		failure_status = YT_IO_ERROR;
		failure_operation = "press any key input drain";
		goto failed;
	}
	status = yt_present_press_prompt(&session->presentation,
	    &presentation, &saved_foreground);
	if (status != YT_PRESENT_OK)
		goto failed;
	session_set_foreground(session, (float)(3));
	yt_out_present_result(&presentation);
	if (!session_timed_wait(session, 33.0)) {
		failure_status = YT_IO_ERROR;
		failure_operation = "press any key wait";
		goto failed;
	}
	status = yt_present_press_cleanup(saved_foreground,
	    &session->presentation, &presentation);
	if (status != YT_PRESENT_OK)
		goto failed;
	yt_out_present_result(&presentation);
	session_set_foreground(session, saved_foreground);
	return true;

failed:
	if (error != NULL) {
		error->status = failure_status;
		snprintf(error->operation, sizeof(error->operation),
		    "%s", failure_operation);
	}
	return false;
}

bool
session_fixed_width_bytes(struct yt_session *session, const uint8_t *text,
    size_t text_length, float width, const char *operation,
    struct yt_error *error)
{
	uint8_t mutable[256];
	size_t length = text_length;
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (length > sizeof(mutable)
	    || (length != 0 && text == NULL)) {
		status = YT_PRESENT_CAPACITY;
	}
	else {
		if (length != 0)
			memcpy(mutable, text, length);
		status = yt_present_fixed_width(mutable, &length,
		    sizeof(mutable), width, &session->presentation,
		    &presentation);
		if (status == YT_PRESENT_OK)
			yt_out_present_result(&presentation);
	}
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

bool
session_right_aligned(struct yt_session *session, const char *text,
    float width, const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_right_aligned((const uint8_t *)text, strlen(text),
	    width, &session->presentation, &presentation);
	if (status == YT_PRESENT_OK)
		yt_out_present_result(&presentation);
	if (status == YT_PRESENT_OK)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

struct session_file_viewer_context {
	struct yt_session *session;
	bool notice_presented;
	bool preopen_blank_presented;
};

static bool
session_file_viewer_output(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	if (!viewer->notice_presented) {
		viewer->notice_presented = true;
		return paged && session_present_paged_line(viewer->session,
		    text, length, "file viewer notice", error);
	}
	if (!viewer->preopen_blank_presented) {
		viewer->preopen_blank_presented = true;
		return !paged && session_present_text(viewer->session, text,
		    length, SESSION_PRESENT_LINE,
		    "file viewer pre-open blank", error);
	}
	session_set_foreground(viewer->session,
	    viewer->session->presentation.foreground);
	if (paged)
		return session_present_paged_row(viewer->session, text, length);
	return session_present_text(viewer->session, text, length,
	    SESSION_PRESENT_LINE, "file viewer final blank", error);
}

bool
session_display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	struct session_file_viewer_context context;
	struct yt_error local_error;
	struct yt_error *active_error = error == NULL ? &local_error : error;
	float saved_foreground = session_foreground(session);
	int saved_pager_foreground = session_pager_foreground(session);
	struct yt_file_viewer_state state = {
		.foreground = &session->presentation.foreground,
		.pager_foreground = &session->pager.foreground,
		.bold = &session->presentation.bold,
		.line_count = &session->pager.line_count,
		.pager_key = session->pager.key,
		.saved_foreground = saved_foreground,
		.saved_pager_foreground = saved_pager_foreground,
	};
	bool ok;

	memset(&context, 0, sizeof(context));
	context.session = session;
	if (error == NULL)
		yt_error_clear(&local_error);
	ok = yt_file_viewer_display(path, &state,
	    session_file_viewer_output, &context, active_error);
	if (ok)
		session_set_pager_line_count(session, session->pager.line_count);
	if (!ok && active_error->status == YT_NOT_FOUND) {
		struct yt_main_error_result handler;
		uint8_t row[sizeof(active_error->path) + 32U];
		size_t row_length;

		if (!yt_main_error_compose(53, 40000,
		    (const uint8_t *)path, strlen(path), NULL, 0U, NULL, 0U,
		    &handler)
		    || handler.route != YT_MAIN_ERROR_MISSING_FILE
		    || !session_present_forced_local_line(handler.debug,
		    handler.debug_length, "file viewer missing debug row",
		    active_error))
			return false;
		yt_error_clear(active_error);
		return yt_file_viewer_missing_row(path, row, sizeof(row),
		    &row_length)
		    && session_present_paged_line(session, row, row_length,
		    "file viewer missing row", active_error)
		    && session_append_news_bytes(session, row, row_length,
		    active_error);
	}
	return ok;
}

bool
session_confirm(struct yt_session *session, const uint8_t *prompt,
    size_t prompt_length, enum yt_yes_no_answer *answer,
    struct yt_error *error)
{
	uint8_t prompt_scratch[YT_COMMAND_SIZE];
	size_t prompt_scratch_length = prompt_length;

	if (answer == NULL || (prompt == NULL && prompt_length != 0U)
	    || prompt_length > sizeof(prompt_scratch))
		return false;
	if (prompt_length != 0U)
		memcpy(prompt_scratch, prompt, prompt_length);
	for (;;) {
		char response[YT_COMMAND_SIZE];
		struct yt_confirmation_transform transform;

		if (!session_present_text(session, prompt_scratch,
		    prompt_scratch_length,
		    SESSION_PRESENT_RAW, "yes/no prompt", error)
		    || !session_read_upper_command(session, response, sizeof(response))
		    || !yt_input_confirmation_staged(response, session->output_source,
		    sizeof(session->output_source), prompt_scratch,
		    sizeof(prompt_scratch), &prompt_scratch_length, session->queue,
		    sizeof(session->queue), &session->queue_position,
		    &session->queue_length, &session->presentation.bold,
		    YT_CONFIRMATION_FAULT_NONE, 0U, &transform)
		    || !transform.answer_valid)
			return false;
		*answer = transform.answer;
		if (transform.outcome == YT_CONFIRMATION_RETURNED)
			return true;
		if (transform.outcome != YT_CONFIRMATION_RETRY)
			return false;
	}
}

static bool
load_configuration(struct yt_session *session, struct yt_error *error)
{
	struct yt_game *game = &session->door->game;
	bool ok;

	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	ok = yt_game_load_startup_configuration(game, "YTDATA.DAT",
	    session->door->identity.local, &session->player_cache,
	    session->disruption_sectors, &session->presentation.sound.snoop,
	    error);
	session->door->game_open = game->database.file != NULL;
	return ok;
}

static bool
opening_poll_local(void *context, bool *ready, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_input_value local = {{0, 0}, 0, false};

	if (!yt_input_poll_source(&session->input, false, &local)) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "ANSI opening local input poll");
		}
		return false;
	}
	*ready = local.length != 0U;
	return true;
}

static bool
opening_poll_remote(void *context, bool *ready, struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	return yt_input_source_ready(&session->input, true, ready);
}

static bool
opening_wait(void *context, float seconds, struct yt_error *error)
{
	return seconds == 3.0f
	    && session_wait(context, 3.0, "ANSI opening EOF wait", error);
}

static bool
opening_and_date(struct yt_session *session, struct yt_error *error)
{
	struct yt_shared_error_result shared_error;
	uint16_t opening_basic_error;
	bool found;

	if (!yt_session_build_route(session, 1, 2, NULL, false, &found, NULL, NULL,
	    error))
		return false;
	if (!found) {
		static const uint8_t diagnostic[] =
		    "*** You can't get there without going someplace you dont want to!";

		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "startup route failure blank", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "startup route failure blank", error))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_text(session, diagnostic,
		    sizeof(diagnostic) - 1U, SESSION_PRESENT_LINE,
		    "startup route failure diagnostic", error))
			return false;
	}
	if (session->presentation.sound.ansi != 0.0f) {
		if (!yt_out_opening_file("YTOPEN.ANS",
		    session->presentation.sound.mode,
		    session->presentation.sound.snoop,
		    opening_poll_local,
		    opening_poll_remote, opening_wait, session,
		    &opening_basic_error, error)) {
			if (opening_basic_error != 0U) {
				if (!yt_shared_error_compose(
				    (int16_t)opening_basic_error, 2710,
				    &shared_error)
				    || !session_commit_shared_terminal(session,
				    &shared_error, error))
					return false;
			}
			return false;
		}
	}
	/* Row 25 belongs to the deferred OpenDoors local personality. */
	session_set_pager_nonstop(session, 1.0f);
	return session_display_game_file(session, "YTOPEN.ASC", error);
}

static bool
startup_pre_admission(struct yt_session *session, struct yt_error *error)
{
	char welcome[320];
	int adjusted_year;
	int today;

	session_set_foreground(session, 5.0f);
	if (!session_present_paged_line(session, (const uint8_t *)"Initializing...",
	    strlen("Initializing..."), "startup initializing row", error))
		return false;
	if (!session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!yt_session_check_lockout(session, error))
		return false;
	snprintf(welcome, sizeof(welcome), "Welcome %s!",
	    session->door->identity.real_first);
	if (!session_present_paged_line(session, (const uint8_t *)welcome, strlen(welcome),
	    "startup welcome row", error)
	    || !session_present_paged_fragment(session,
	    (const uint8_t *)"Searching my records for your name.",
	    strlen("Searching my records for your name.")))
		return false;
	return true;
}

static bool
resolve_alias(struct yt_session *session, char first[128], char last[128],
    struct yt_error *error)
{
	struct yt_name_file names;
	const struct yt_name_row *match;

	snprintf(first, 128, "%s", session->door->identity.real_first);
	snprintf(last, 128, "%s", session->door->identity.real_last);
	qb_title_case(first);
	qb_title_case(last);
	if (!yt_names_load("YTNAME.DAT", &names, error))
		return false;
	match = yt_names_find_real_last(&names, first, last);
	if (match != NULL) {
		snprintf(first, 128, "%s", match->alias_first);
		snprintf(last, 128, "%s", match->alias_last);
		yt_names_free(&names);
		return true;
	}
	for (;;) {
		char alias[256];
		char alias_first[128];
		char alias_last[128];
		char display[258];
		char confirmation[80];
		enum yt_alias_key_status alias_status;
		struct yt_name_row row;

		session_set_foreground(session, 2.0f);
		if (!session_present_paged_line(session,
		    (const uint8_t *)"You are a new player.",
		    strlen("You are a new player."), "new alias notice", error)
		    || !session_present_paged_line(session,
		    (const uint8_t *)
		    "Enter the FULL alias you wish to use in the game.",
		    strlen("Enter the FULL alias you wish to use in the game."),
		    "new alias instruction", error)
		    || !session_present_paged_line(session,
		    (const uint8_t *)"Press [ENTER] to use your real name.",
		    strlen("Press [ENTER] to use your real name."),
		    "new alias real-name instruction", error)
		    || !session_present_timed_paged_row(session, (const uint8_t *)"-+> ", 4,
		    "new alias prompt", error)
		    || !session_read_command(session, alias, sizeof(alias))) {
			yt_names_free(&names);
			return false;
		}
		alias_status = yt_names_prepare_alias(alias, sizeof(alias), first,
		    last, alias_first, sizeof(alias_first), alias_last,
		    sizeof(alias_last), display, sizeof(display));
		if (alias_status == YT_ALIAS_KEY_EMPTY)
			continue;
		if (alias_status == YT_ALIAS_KEY_RANGE) {
			yt_names_free(&names);
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "new alias key preparation");
			}
			return false;
		}
		if (alias_status == YT_ALIAS_KEY_RESERVED) {
			if (!session_present_paged_fragment(session,
			    (const uint8_t *)
			    "That ALIAS is NOT allowed. Please choose another.",
			    strlen("That ALIAS is NOT allowed. Please choose another."))) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		if (yt_names_alias_exists(&names, alias_first, alias_last)) {
			char collision[320];

			snprintf(collision, sizeof(collision),
			    "I'm sorry %s, but that Alias is already in use.", first);
			if (!session_present_paged_fragment(session, (const uint8_t *)collision,
			    strlen(collision))) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		session_set_foreground(session, 3.0f);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "new alias identity blank", error)) {
			yt_names_free(&names);
			return false;
		}
		yt_present_set_bold(&session->presentation, 1.0f);
		{
			char identity[560];

			snprintf(identity, sizeof(identity), "%s %s a.k.a. %s",
			    first, last, display);
			if (!session_present_paged_fragment(session, (const uint8_t *)identity,
			    strlen(identity))
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "new alias confirmation blank", error)) {
				yt_names_free(&names);
				return false;
			}
		}
		session_set_foreground(session, 6.0f);
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)"Is this OK (Y/[N])? ",
		    strlen("Is this OK (Y/[N])? "),
		    "new alias confirmation prompt", error)
		    || !session_read_upper_command(session, confirmation, sizeof(confirmation))) {
			yt_names_free(&names);
			return false;
		}
		if (strcmp(confirmation, "Y") != 0)
			continue;
		row.real_first = first;
		row.real_last = last;
		row.alias_first = alias_first;
		row.alias_last = alias_last;
		if (!yt_names_append("YTNAME.DAT", &row, error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_present_alert(session,
		    (const uint8_t *)"Your Alias has been recorded. Have fun!",
		    strlen("Your Alias has been recorded. Have fun!"),
		    "new alias accepted row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "new alias final blank", error)) {
			yt_names_free(&names);
			return false;
		}
		snprintf(first, 128, "%s", alias_first);
		snprintf(last, 128, "%s", alias_last);
		yt_names_free(&names);
		return true;
	}
}

static bool
construct_player_visible(struct yt_session *session, struct yt_error *error)
{
	struct yt_player_constructor_state state;
	uint8_t date_raw[4];
	uint8_t turns_raw[4];

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "player constructor blank", error)
	    || !session_present_text(session,
	    (const uint8_t *)"Your ship has been built.",
	    strlen("Your ship has been built."), SESSION_PRESENT_LINE,
	    "player constructor row", error))
		return false;
	if (qb_mbf32_encode((float)session->door->game.today, date_raw)
	    != QB_MBF_OK)
		return false;
	memcpy(turns_raw, session->door->game.config.record.bytes + YT_F49,
	    sizeof(turns_raw));
	if (yt_game_construct_player(&session->door->game,
	    session_record(session), date_raw, turns_raw, &session->player,
	    &state, error))
		return true;
	if (!state.config_hydrated)
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_CONFIG_GET);
	else if (!state.player_hydrated)
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_GET);
	else if (state.put_attempted)
		attach_database_put_fault(session, error,
		    YT_BASIC_FAULT_CONSTRUCTOR_PLAYER_PUT);
	if (error != NULL && error->basic_fault_valid)
		(void)session_route_basic_fault(session, error);
	return false;
}

static bool
set_new_player_identity(struct yt_session *session, int player_record,
    const uint8_t *name, size_t length, struct yt_error *error)
{
	struct yt_player player;
	struct yt_record identity;
	uint8_t length_raw[4];

	if (name == NULL && length != 0U)
		return false;
	if (!session_read_player_at_fault(session, player_record, &player,
	    YT_BASIC_FAULT_IDENTITY_PLAYER_GET, error)) {
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	identity = player.record;
	yt_record_set_text(&identity, name, length);
	if (qb_mbf32_encode((float)length, length_raw) != QB_MBF_OK)
		return false;
	(void)yt_record_set_raw_number(&identity, YT_F85, length_raw);
	(void)yt_record_set_number(&identity, YT_F89, 0.0f);
	yt_player_decode(&session->player, &identity);
	if (!write_database_record_at_fault(session, (uint32_t)player_record,
	    &identity, YT_BASIC_FAULT_IDENTITY_PLAYER_PUT, error)) {
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	return true;
}

bool
yt_session_instruction_offer(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want instructions (Y/N) [N]? ";
	char response[80];

	for (;;) {
		enum yt_yes_no_answer answer;

		memcpy(session->output_source, prompt, sizeof(prompt));
		if (!session_present_text(session,
		    (const uint8_t *)session->output_source,
		    sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "instruction question", error)
		    || !session_read_command(session, response, sizeof(response))
		    || !yt_input_yes_no_candidate(session->command_accumulator,
		    session->output_source, sizeof(session->output_source),
		    &answer))
			return false;
		if (answer == YT_YES_NO_EMPTY || answer == YT_YES_NO_NO)
			return true;
		if (answer == YT_YES_NO_YES)
			return session_display_game_file(session, "YTINSTR.DOC", error);
		yt_present_set_bold(&session->presentation, 1.0f);
		session_clear_queue(session);
	}
}

static bool
startup_retention(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prefix[] =
	    "Notice: If your ship is dead and you have not played for";
	static const uint8_t second[] =
	    "days, it will be deleted to make room for someone else.";
	struct yt_record record;
	uint8_t first[128];
	char number[64];
	int number_length;
	size_t first_length;

	if (!yt_database_read(&session->door->game.database, 1U, &record,
	    error))
		return false;
	number_length = qb_str_single(number, sizeof(number),
	    qb_mbf32_decode(record.bytes + YT_F77));
	if (number_length < 0
	    || sizeof(prefix) - 1U + (size_t)number_length > sizeof(first))
		return false;
	memcpy(first, prefix, sizeof(prefix) - 1U);
	memcpy(first + sizeof(prefix) - 1U, number, (size_t)number_length);
	first_length = sizeof(prefix) - 1U + (size_t)number_length;
	return session_present_paged_line(session, first, first_length,
	    "new player retention first row", error)
	    && session_present_paged_fragment(session, second, sizeof(second) - 1U)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "new player retention final blank", error);
}

static bool
returning_daily_update(struct yt_session *session,
    const uint8_t today_raw[4], const uint8_t turns_per_day_raw[4],
    float *previous_day, float *killer, struct yt_error *error)
{
	static const uint8_t zero[4] = {0x00U, 0x00U, 0x00U, 0x00U};
	static const uint8_t row[] = "You have been on today.";
	struct yt_player player;
	struct yt_record daily;
	uint8_t turns_scratch[4];
	bool same_day;

	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &player, error)) {
		attach_database_get_fault(session, error,
		    YT_BASIC_FAULT_RETURNING_DAILY_GET);
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	*previous_day = player.last_active;
	same_day = *previous_day == qb_mbf32_decode(today_raw);
	if (same_day && !session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE, "returning same-day row", error)) {
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	*killer = player.killed_by;
	memcpy(turns_scratch, player.record.bytes + YT_F49,
	    sizeof(turns_scratch));

	daily = player.record;
	(void)yt_record_set_raw_number(&daily, YT_F41, today_raw);
	if (!same_day) {
		if (qb_mbf32_decode(turns_scratch)
		    < qb_mbf32_decode(turns_per_day_raw))
			memcpy(turns_scratch, turns_per_day_raw,
			    sizeof(turns_scratch));
		if (memcmp(turns_scratch, daily.bytes + YT_F49,
		    sizeof(turns_scratch)) != 0)
			(void)yt_record_set_raw_number(&daily, YT_F49,
			    turns_scratch);
		(void)yt_record_set_raw_number(&daily, YT_F105, zero);
	}
	yt_player_decode(&player, &daily);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &daily, error)) {
		attach_database_put_fault(session, error,
		    YT_BASIC_FAULT_RETURNING_DAILY_PUT);
		if (error != NULL && error->basic_fault_valid)
			(void)session_route_basic_fault(session, error);
		return false;
	}
	session->player = player;
	return true;
}

static bool
returning_self_denial(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] =
	    "You will be allowed to play again tomorrow!";

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "returning self-denial blank", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	session_set_foreground(session, 7.0f);
	if (!session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "returning self-denial row", error))
		return false;
	session_close_game(session);
	session->running = false;
	session->terminated = true;
	return true;
}

static bool
admit_player(struct yt_session *session, const char *first, const char *last,
    struct yt_error *error)
{
	char full[256];
	struct yt_clock_value now;
	float returning_bound;
	int basic;
	bool returning = false;

	snprintf(full, sizeof(full), "%s %s", first, last);
	returning_bound = session->door->game.config.sector_offset;
	for (basic = YT_PLAYER_FIRST;
	    (float)basic <= returning_bound; ++basic) {
		struct yt_player candidate;
		bool matches;

		session->inherited_loop_index = (float)basic;
		if (!yt_game_read_player(&session->door->game, basic, &candidate,
		    error)
		    || !yt_player_name_matches(&candidate, (const uint8_t *)full,
		    strlen(full), &matches, error))
			return false;
		if (matches) {
			session->player_record_carrier = basic;
			session->player = candidate;
			if (!yt_player_stored_name(&candidate,
			    session->cached_player_name,
			    &session->cached_player_name_length, error))
				return false;
			returning = true;
			break;
		}
		session->inherited_loop_index = (float)(basic + 1);
	}
	if (!returning) {
		int vacant = 0;
		float vacancy_bound;

		session_set_foreground(session, 5.0f);
		if (!session_present_paged_line(session,
		    (const uint8_t *)"Entering a new player...",
		    strlen("Entering a new player..."),
		    "new player entering row", error))
			return false;
		vacancy_bound = session->door->game.config.sector_offset;
		session_set_current_player_record(session, YT_PLAYER_FIRST);
		for (basic = YT_PLAYER_FIRST;
		    (float)basic <= vacancy_bound;
		    ++basic) {
			struct yt_player candidate;

			if (!yt_game_read_player(&session->door->game, basic,
			    &candidate, error))
				return false;
			if (candidate.name_length < 1.0f) {
				vacant = basic;
				break;
			}
			session_set_current_player_record(session, basic + 1);
		}

		if (vacant == 0) {
			char date[11];

			if (!session_present_alert(session,
			    (const uint8_t *)
			    "I'm sorry but the game is full. Try again tomorrow.",
			    strlen("I'm sorry but the game is full. Try again tomorrow."),
			    "new player full row", error))
				return false;
			if (!yt_platform_clock(&now, error))
				return false;
			yt_format_date(&now, date);
			if (!yt_news_append_game_full(date, full, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		if (!startup_retention(session, error))
			return false;
		if (!construct_player_visible(session, error)
		    || !set_new_player_identity(session, vacant,
		    (const uint8_t *)full, strlen(full), error)
		    || !yt_player_stored_name(&session->player,
		    session->cached_player_name,
		    &session->cached_player_name_length, error))
			return false;
		if (!yt_platform_clock(&now, error))
			return false;
		{
			char date[11];

			yt_format_date(&now, date);
			if (!yt_news_append_new_player(date, full, error))
				return false;
		}
		return yt_session_instruction_offer(session, error);
	}
	session_set_foreground(session, 2.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "returning player blank", error))
		return false;
	{
		uint8_t today_raw[4];
		uint8_t turns_raw[4];
		float previous_day;
		float killer;
		float startup_day;
		bool self_kill;

		if (qb_mbf32_encode((float)session->door->game.today, today_raw)
		    != QB_MBF_OK)
			return false;
		memcpy(turns_raw, session->door->game.config.record.bytes + YT_F49,
		    sizeof(turns_raw));
		if (!returning_daily_update(session, today_raw, turns_raw,
		    &previous_day, &killer, error))
			return false;
		if (!yt_database_flush(&session->door->game.database, error))
			return false;
		startup_day = qb_mbf32_decode(today_raw);
		self_kill = killer == (float)session_record(session);
		if (!yt_platform_clock(&now, error))
			return false;
		{
			char time_text[9];

			yt_format_time(&now, time_text);
			if (!yt_news_append_login_bytes(
			    (const uint8_t *)time_text, strlen(time_text),
			    session->cached_player_name,
			    session->cached_player_name_length, error))
				return false;
		}
		if (killer != 0.0f) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "returning death blank", error))
				return false;
			if (!self_kill)
				yt_present_set_blink(&session->presentation, 1.0f);
			if (killer == -1.0f) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by The Xannor!",
				    strlen("You have been killed by The Xannor!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning Xannor death row", error))
					return false;
			}
			else if (killer == -2.0f) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by mercenaries!",
				    strlen("You have been killed by mercenaries!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning mercenary death row", error))
					return false;
			}
			else if (killer == -98.0f) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You have been killed by a deleted player.",
				    strlen("You have been killed by a deleted player."),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning deleted-player death row", error))
					return false;
			}
			else if (self_kill) {
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You managed to kill yourself on your last time on.",
				    strlen("You managed to kill yourself on your last time on."),
				    SESSION_PRESENT_LINE,
				    "returning self-death row", error))
					return false;
			}
			else if (killer > 1.0f
			    && killer <= session_sector_offset(session)) {
				struct yt_player attacker;
				uint8_t attacker_row[YT_TEXT_FIELD_SIZE
				    + sizeof(" destroyed your ship!") - 1U];
				size_t attacker_length;
				bool emit;

				if (!yt_game_read_player(&session->door->game,
				    (int)killer, &attacker, error)) {
					attach_database_get_fault(session, error,
					    YT_BASIC_FAULT_RETURNING_KILLER_GET);
					(void)session_route_basic_fault(session, error);
					return false;
				}
				if (!yt_player_killer_row(&attacker, attacker_row,
				    sizeof(attacker_row), &attacker_length, &emit, error)) {
					if (error != NULL && strcmp(error->operation,
					    "player name CINT") == 0)
						(void)yt_error_attach_basic_fault_number(error,
						    YT_BASIC_FAULT_RETURNING_KILLER_CINT,
						    6U);
					else if (error != NULL && strcmp(error->operation,
					    "player name LEFT$ length") == 0)
						(void)yt_error_attach_basic_fault_number(error,
						    YT_BASIC_FAULT_RETURNING_KILLER_LEFT,
						    5U);
					(void)session_route_basic_fault(session, error);
					return false;
				}
				if (emit && !session_present_text(session, attacker_row,
				    attacker_length, SESSION_PRESENT_BOLD_LINE,
				    "returning player death row", error))
					return false;
			}
			if (self_kill
			    && previous_day == startup_day) {
				(void)returning_self_denial(session, error);
				return false;
			}
			if (!construct_player_visible(session, error))
				return false;
			if (!session_returning_rebuild_wait(session, error))
				return false;
		}
	}
	return true;
}

static bool
post_login(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] = "[ Press any Key ]";

	{
		struct yt_record repaired;
		uint8_t maximum_raw[4];

		if (!session_reload_player(session, error))
			return false;
		if (session->player.sector < 1.0f) {
			repaired = session->player.record;
			(void)yt_record_set_number(&repaired, YT_F57, 1.0f);
			yt_player_decode(&session->player, &repaired);
			if (!write_database_record_at_fault(session,
			    (uint32_t)session_record(session), &repaired,
			    YT_BASIC_FAULT_POST_LOGIN_SECTOR_PUT, error))
				return false;
		}
		if (!session_reload_player(session, error))
			return false;
		memcpy(maximum_raw, session->door->game.config.record.bytes + YT_F121,
	    sizeof(maximum_raw));
		if ((double)session->player.holds
		    > (double)qb_mbf32_decode(maximum_raw)) {
			repaired = session->player.record;
			(void)yt_record_set_number(&repaired, YT_F69, 0.0f);
			(void)yt_record_set_number(&repaired, YT_F73, 0.0f);
			(void)yt_record_set_raw_number(&repaired, YT_F77,
			    maximum_raw);
			(void)yt_record_set_raw_number(&repaired, YT_F65,
			    maximum_raw);
			yt_player_decode(&session->player, &repaired);
			if (!write_database_record_at_fault(session,
			    (uint32_t)session_record(session), &repaired,
			    YT_BASIC_FAULT_POST_LOGIN_CARGO_PUT, error))
				return false;
		}
	}
	/* Do not emit the deferred local-personality status row here. */
	if (!yt_session_show_ship(session, error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login Info trailing blank", error))
		return false;
	if (!session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "post-login low-time warning", error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "post-login press prompt");
		}
		return false;
	}
	if (!session_wait(session, 99.0, "post-login press wait", error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login press trailing blank", error))
		return false;
	if (!yt_session_radio_read(session, false, error))
		return false;
	return true;
}

bool
read_planet_physical(struct yt_session *session, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

bool
session_write_planet_physical(struct yt_session *session,
    uint32_t physical_record,
    struct yt_planet *planet, bool encode, struct yt_error *error)
{
	if (encode) {
		uint8_t stored_name[YT_TEXT_FIELD_SIZE];

		memcpy(stored_name, planet->record.bytes, sizeof(stored_name));
		yt_planet_encode(planet);
		memcpy(planet->record.bytes, stored_name, sizeof(stored_name));
	}
	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &planet->record, error);
}
bool
yt_session_load_team_cache(struct yt_session *session, int team_id,
    int current_player_record, struct yt_record *overlay,
    bool *overlay_loaded, bool *live,
    struct yt_error *error)
{
	struct yt_record loaded;
	float expression;
	uint32_t physical_record;

	if (overlay_loaded != NULL)
		*overlay_loaded = false;
	if (live != NULL)
		*live = false;
	memset(&session->team_cache, 0, sizeof(session->team_cache));
	if (team_id < 1 || team_id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	expression = single_add(session_sector_offset(session), (float)team_id);
	physical_record = qb_brun_random_record_number(expression);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &loaded, error))
		return false;
	if (overlay != NULL)
		*overlay = loaded;
	if (overlay_loaded != NULL)
		*overlay_loaded = true;
	yt_team_cache_load(&session->team_cache, &loaded,
	    current_player_record, live);
	return true;
}

bool
session_read_combat_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (player_record != session_record(session))
		return yt_game_read_player(&session->door->game, player_record,
		    player, error);
	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

bool
session_write_combat_player(struct yt_session *session, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	if (player_record == session_record(session))
		session->player = *player;
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error);
}

bool
yt_session_fighter_shield_spill(struct yt_session *session, double *fighters,
    float *shields, bool bind_hostile_cells, struct yt_error *error)
{
	uint8_t fighter_row[128];
	uint8_t shield_row[128];
	size_t fighter_length;
	size_t shield_length;

	while (*fighters > 0.0 && *shields > 0.0f) {
		float draw;

		if (!yt_random_next(&session->door->game.random, &draw, error)
		    || !yt_fighter_shield_spill_step(fighters, shields, draw))
			return false;
		if (bind_hostile_cells) {
			if (draw >= 0.5f)
				session->hostile_deployed_fighters = *fighters;
			else
				session->combat_ship_shields = *shields;
		}
	}
	return yt_fighter_shield_spill_rows(*fighters, *shields,
	    fighter_row, sizeof(fighter_row), &fighter_length,
	    shield_row, sizeof(shield_row), &shield_length)
	    && session_present_text(session, fighter_row, fighter_length,
	    SESSION_PRESENT_LINE, "fighter spill result", error)
	    && session_present_text(session, shield_row, shield_length,
	    SESSION_PRESENT_LINE, "shield spill result", error);
}


bool
yt_session_run(struct yt_door *door, const char *executable_path,
    struct yt_error *error)
{
	struct yt_session session;
	struct yt_random launch_random;
	bool resume_gameplay = false;
	char first[128];
	char last[128];

	if (door == NULL) {
		if (error != NULL) {
			error->status = YT_INVALID;
			snprintf(error->operation, sizeof(error->operation),
			    "player session");
		}
		return false;
	}
	memset(&session, 0, sizeof(session));
	yt_input_init(&session.input);
	session.error = error;
	session.door = door;
	session.executable_path = executable_path;
	session.running = true;
	/* YT:040A is the ordinary instruction after the handed-off checkpoint. */
	session_set_pager_nonstop(&session, 1.0f);
	if (door->identity.ansi
	    && !qb_mbf32_truth(door->identity.ansi_raw)) {
		session.presentation.sound.ansi = 1.0f;
	}
	else {
		session.presentation.sound.ansi =
		    qb_mbf32_decode(door->identity.ansi_raw);
	}
	session.presentation.sound.mode = door->identity.local ? 1.0f : 0.0f;
	session.presentation.sound.user_sound = -1.0f;
	session.presentation.sound.local_sound =
	    door->identity.local ? -1.0f : 0.0f;
	session_set_foreground(&session, 7.0f);
	yt_random_init(&launch_random);
	if (!yt_random_market_bases(&launch_random, session.market_bases, error))
		return false;
	if (!load_configuration(&session, error))
		return session.terminated;
	session_set_foreground(&session, 6.0f);
	if (!session_present_text(&session, NULL, 0, SESSION_PRESENT_LINE,
	    "startup pre-title blank", error)
	    || !yt_session_registration(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!opening_and_date(&session, error)
	    || !startup_pre_admission(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!resolve_alias(&session, first, last, error)
	    || !admit_player(&session, first, last, error))
		return session.terminated;
	if (!session.running)
		return true;
	if (!post_login(&session, error)) {
		if (!session_handle_gameplay_fault(&session, error,
		    &resume_gameplay))
			return false;
	}
	else if (!yt_session_sector_entry(&session, error)) {
		if (!session_handle_gameplay_fault(&session, error,
		    &resume_gameplay))
			return false;
	}
	if (session.terminated)
		return true;
	if (!session_is_destroyed(&session) && session.running) {
		for (;;) {
			bool completed = resume_gameplay
			    ? yt_session_sector_entry(&session, error)
			    : yt_session_command_shell(&session, error);
			if (completed) {
				if (!resume_gameplay)
					break;
				resume_gameplay = false;
				if (!session.running || session_is_destroyed(&session))
					break;
				continue;
			}
			if (!session_handle_gameplay_fault(&session, error,
			    &resume_gameplay))
				return false;
			if (session.terminated)
				return true;
		}
	}
	if (session_is_destroyed(&session) && !session.fatal_wait_complete) {
		if (!session_wait(&session, 5.0, "common fatal wait", error))
			return false;
		session.fatal_wait_complete = true;
	}
	return yt_session_quit(&session, error);
}
