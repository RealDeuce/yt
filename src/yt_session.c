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

bool
session_wait(struct yt_session *session, double seconds,
    const char *operation, struct yt_error *error)
{
	if (yt_input_pause(&session->input, seconds))
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

void
session_close_game(struct yt_session *session)
{
	if (session->door->game_open) {
		session->door->game_open = false;
		yt_game_close(&session->door->game);
	}
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

static bool
read_keyboard_line(struct yt_session *session, char *dest, size_t size)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};

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
			uint8_t prefix[YT_INPUT_PENDING];
			size_t prefix_length =
			    strlen(session->command_accumulator);
			size_t saved_length = strlen(session->saved_command);

			if (prefix_length != 0U)
				memcpy(prefix, session->command_accumulator,
				    prefix_length);
			memcpy(session->paged_text,
			    session->command_accumulator, prefix_length + 1U);
			session->pager.newline_flag = 1.0f;
			if (!session_present_paged_row(session, prefix,
			    prefix_length))
				return false;
			memmove(session->command_accumulator,
			    session->saved_command, saved_length + 1U);
			key = '\r';
		}
		if (yt_input_submit_requested(key)) {
			struct yt_present_result presentation;
			enum yt_present_status status;

			session->pager.newline_flag = 0.0f;
			status = yt_present_line(NULL, 0,
			    &session->presentation, &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			snprintf(dest, size, "%s", session->command_accumulator);
			return true;
		}
		if (key == '\b' && session->command_accumulator[0] != '\0') {
			struct yt_present_result presentation;
			enum yt_present_status status;
			size_t length = strlen(session->command_accumulator);

			session->command_accumulator[length - 1U] = '\0';
			status = yt_present_editor_echo(local_erase,
			    sizeof(local_erase), remote_erase,
			    sizeof(remote_erase), &session->presentation,
			    &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			continue;
		}
		{
			struct yt_present_result presentation;
			enum yt_present_status status;
			size_t length = strlen(session->command_accumulator);

			if (key < 0x20U || key > 0x7fU
			    || length + 1U >= sizeof(session->command_accumulator)
			    || length + 1U >= size)
				continue;
			status = yt_present_editor_echo(&key, 1U, &key, 1U,
			    &session->presentation, &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			session->command_accumulator[length] = (char)key;
			session->command_accumulator[length + 1U] = '\0';
			session->paged_text[0] = (char)key;
			session->paged_text[1] = '\0';
			session->pager.newline_flag = 1.0f;
			od_kernel();
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
	yt_input_compat_upper_n((uint8_t *)text, strlen(text));
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

static bool
session_run_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length)
{
	static const uint8_t prompt[] =
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ";
	static const uint8_t notice[] = "Ctrl-X to Stop";
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
	struct yt_present_result presentation;
	struct yt_input_value sampled;
	char response[80];
	enum yt_present_status status;
	int saved_foreground;
	bool emit_notice;

	od_kernel();
	if (!yt_input_poll(&session->input, &sampled)
	    || !yt_pager_apply_key(&sampled, &key_state))
		return false;
	status = yt_present_paged_text(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	od_kernel();
	status = yt_present_paged_finish(session->pager.newline_flag != 0.0f,
	    &session->presentation, &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	if (yt_pager_advance(&session->pager, &session->presentation,
	    &saved_foreground)) {
		if (!session_run_paged_row(session, prompt,
		    sizeof(prompt) - 1U)
		    || !read_keyboard_line(session, response, sizeof(response)))
			return false;
		emit_notice = yt_pager_accept_response(&session->pager, response,
		    sizeof(response));
		if (emit_notice && !session_run_paged_row(session, notice,
		    sizeof(notice) - 1U))
			return false;
		yt_pager_complete(&session->pager, &session->presentation,
		    saved_foreground);
	}
	session->pager.newline_flag = 0.0f;
	return true;
}

bool
session_present_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length)
{
	return session_store_output_source(session, text, length)
	    && session_run_paged_row(session, text, length);
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
	if (!yt_input_pause(&session->input, 33.0)) {
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

bool
session_display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Cntl-X to Stop";
	struct yt_text_input input;
	struct yt_error local_error;
	struct yt_error *active_error = error == NULL ? &local_error : error;
	float saved_foreground = session_foreground(session);
	int saved_pager_foreground = session_pager_foreground(session);
	bool ok = false;

	if (error == NULL)
		yt_error_clear(&local_error);
	if (path == NULL) {
		active_error->status = YT_INVALID;
		active_error->system_error = 0;
		snprintf(active_error->operation,
		    sizeof(active_error->operation), "file viewer");
		active_error->path[0] = '\0';
		return false;
	}
	session->pager.key[0] = '\0';
	if (!session_present_paged_line(session, notice, sizeof(notice) - 1U,
	    "file viewer notice", active_error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "file viewer pre-open blank", active_error))
		return false;
	yt_text_input_init(&input);
	if (!yt_text_input_close(&input, active_error))
		goto done;
	session->pager.line_count = 0.0f;
	if (!yt_text_input_open(&input, path, active_error))
		goto done;
	for (;;) {
		const uint8_t *line;
		size_t length;
		bool available;
		bool eof;
		int foreground;

		if (!yt_text_input_eof(&input, &eof, active_error))
			goto done;
		if (eof || strcmp(session->pager.key, "Q") == 0)
			break;
		if (!yt_text_input_read_line(&input, &line, &length, &available,
		    active_error))
			goto done;
		if (!available) {
			active_error->status = YT_EOF;
			active_error->system_error = 0;
			snprintf(active_error->operation,
			    sizeof(active_error->operation),
			    "LINE INPUT after EOF check");
			snprintf(active_error->path, sizeof(active_error->path),
			    "%s", path);
			goto done;
		}
		foreground = yt_file_viewer_line_foreground(line, length);
		session->presentation.foreground = (float)foreground;
		session->pager.foreground = foreground;
		if (foreground != 2)
			session->presentation.bold = 1.0f;
		session_set_foreground(session,
		    session->presentation.foreground);
		if (!session_present_paged_row(session, line, length))
			goto done;
	}
	if (!yt_text_input_close(&input, active_error))
		goto done;
	session->pager.line_count = 0.0f;
	session->presentation.foreground = saved_foreground;
	session->pager.foreground = saved_pager_foreground;
	session_set_foreground(session, session->presentation.foreground);
	ok = session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "file viewer final blank", active_error);

done:
	yt_text_input_destroy(&input);
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
		    && yt_news_append_bytes(row, row_length,
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
