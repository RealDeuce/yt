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

static void
session_player_cache_raw(const struct yt_session *session, int player_record,
    enum yt_player_cache_kind kind, uint8_t raw[4])
{
	yt_player_cache_raw(&session->player_cache, player_record, kind, raw);
}

static float
session_player_cache_value(const struct yt_session *session, int player_record,
    enum yt_player_cache_kind kind)
{
	return yt_player_cache_value(&session->player_cache, player_record, kind);
}

static void
session_set_player_cache_raw(struct yt_session *session, int player_record,
    enum yt_player_cache_kind kind, const uint8_t raw[4])
{
	(void)yt_player_cache_set_raw(&session->player_cache, player_record,
	    kind, raw);
}

static float
session_team_roster_value(const struct yt_session *session, size_t index)
{
	if (index >= YT_ARRAY_LEN(session->team_cache.roster))
		return 0.0f;
	return (float)session->team_cache.roster[index];
}

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

static void
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

static void
session_set_foreground(struct yt_session *session, float value)
{
	session->foreground = value;
	session->presentation.foreground = value;
	session->pager.foreground = (int)value;
}

static bool
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
session_store_counterattack_player(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->counterattack_player = (int)qb_mbf32_decode(raw);
}

static void
session_load_counterattack_player(struct yt_session *session,
    int *counterattack)
{
	if (counterattack != NULL)
		*counterattack = session->counterattack_player;
}

static void
session_load_xannor_provoker(struct yt_session *session, int *provoker)
{
	if (provoker != NULL)
		*provoker = session->xannor_provoker;
}

static void
session_set_current_player_record(struct yt_session *session, int record)
{
	session->player_record_carrier = record;
}

static bool random_value(void *context, float *value,
    struct yt_error *error);
static bool computer_spies(struct yt_session *session,
    struct yt_error *error);
static bool mine_encounter(struct yt_session *session, bool *terminal,
    struct yt_error *error);
static bool clearance(struct yt_session *session, bool create,
    struct yt_error *error);
static bool launch_xannor_retaliation(struct yt_session *session,
    int *provoking_player, struct yt_error *error);
static void clear_queue(struct yt_session *session);
static bool show_ship(struct yt_session *session, struct yt_error *error);
static bool command_mines(struct yt_session *session,
    struct yt_error *error);
static bool command_team(struct yt_session *session,
    struct yt_error *error);
static bool earth_store(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
static bool command_move(struct yt_session *session, bool *moved,
    struct yt_error *error);
static bool command_land(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
static bool quit_session(struct yt_session *session,
    struct yt_error *error);
static bool info_refresh_time(struct yt_session *session,
    struct yt_error *error);
static bool build_route(struct yt_session *session, float start,
    float destination, int16_t *next_hop, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error);
static bool session_present_paged_row(struct yt_session *session, const uint8_t *text,
    size_t length);
static bool session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length);
static bool session_present_paged_line(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error);
static bool computer_port_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error);
static bool computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
static bool port_report_length(struct yt_session *session, float raw,
    size_t maximum, size_t *length, const char *operation,
    struct yt_error *error);
static bool fighter_shield_spill(struct yt_session *session,
    double *fighters, float *shields, bool bind_hostile_cells,
    struct yt_error *error);
static bool scanner_read_player(struct yt_session *session,
    float basic_record, struct yt_player *player, struct yt_error *error);

static void
session_current_warps(const struct yt_session *session, float warps[6])
{
	memcpy(warps, session->current_warps, sizeof(session->current_warps));
}

static bool
session_is_disruption_sector(const struct yt_session *session, float sector)
{
	return sector == session->disruption_sectors[0]
	    || sector == session->disruption_sectors[1];
}

static void
session_set_relationship(struct yt_session *session, float value)
{
	session->shared_status = value;
}

static void
session_set_self_mine_suppression(struct yt_session *session, bool enabled)
{
	session->self_mine_suppressed = enabled;
}

static void
session_set_current_sector_record(struct yt_session *session, float value)
{
	session->current_sector_record = value;
}

static bool
session_anti_cloak_enabled(const struct yt_session *session)
{
	return session->anti_cloak_enabled;
}

static void
session_enable_anti_cloak(struct yt_session *session)
{
	session->anti_cloak_enabled = true;
}

static double
session_hostile_deployed_fighters(const struct yt_session *session)
{
	return session->hostile_deployed_fighters;
}

static int
session_radio_body_key(struct yt_session *session)
{
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, false};

		if (!yt_input_wait(&session->input, &selected))
			return EOF;
		if (selected.length == 1)
			return selected.bytes[0];
	}
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

static bool
session_returning_rebuild_wait(struct yt_session *session,
    struct yt_error *error)
{
	return session_wait(session, 5.0, "returning-player rebuild wait",
	    error);
}

static bool
session_editor_close_all(void *context)
{
	struct yt_session *session = context;

	if (session->door->game_open) {
		session->door->game_open = false;
		yt_game_close(&session->door->game);
	}
	return true;
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

static float
single_sub(float left, float right)
{
	volatile float result = left - right;
	return result;
}

static uint32_t
session_sector_basic_record(const struct yt_session *session,
    float logical_sector)
{
	return (uint32_t)yt_sector_basic_record(&session->door->game.config,
	    (int)logical_sector);
}

static uint32_t
session_port_basic_record(const struct yt_session *session, float logical_port)
{
	return (uint32_t)yt_port_basic_record(&session->door->game.config,
	    (int)logical_port);
}

static uint32_t
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

static bool
session_write_sector(struct yt_session *session, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	yt_sector_encode(sector);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &sector->record, error);
}

static bool
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

static bool
session_write_port(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	yt_port_encode(port);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &port->record, error);
}

static bool
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

static bool
session_write_planet(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	yt_planet_encode(planet);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &planet->record, error);
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
single_div(float left, float right)
{
	volatile float result = left / right;
	return result;
}

static double
double_add(double left, double right)
{
	volatile double result = left + right;
	return result;
}

static double
double_sub(double left, double right)
{
	volatile double result = left - right;
	return result;
}

static double
double_mul(double left, double right)
{
	volatile double result = left * right;
	return result;
}

int
session_sector_count(const struct yt_session *session)
{
	return (int)(session_port_offset(session)
	    - session_sector_offset(session));
}

static int
port_count(const struct yt_session *session)
{
	return (int)(session_planet_offset(session)
	    - session_port_offset(session));
}

static bool
write_player(struct yt_session *session, struct yt_error *error)
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

static bool
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

static bool
read_player_at_fault(struct yt_session *session, int player_record,
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

static bool
read_sector_at_fault(struct yt_session *session, int logical_sector,
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

static bool
read_port_at_fault(struct yt_session *session, int logical_port,
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

static bool
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

	if (!read_player_at_fault(session, session_record(session), &fresh,
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
apply_player_credit_mutation(void *context, float player_record,
    float argument, struct yt_player *player, bool *hydrated,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)player_record;
	if (hydrated != NULL)
		*hydrated = false;
	if (!session_mutate_player_credits(session, argument, hydrated, error)) {
		if (player != NULL && hydrated != NULL && *hydrated)
			*player = session->player;
		return false;
	}
	if (player != NULL)
		*player = session->player;
	return true;
}

static bool
computer_prompt_hydrate(struct yt_session *session, struct yt_error *error)
{
	return session_reload_player(session, error);
}

static bool
session_close_file5(struct yt_error *error)
{
	struct yt_database file = {0};

	/* These callers have no live random file-5 owner at this boundary. */
	return yt_database_random_close(&file, error);
}

static bool
session_close_game_all(void *context, int8_t file_class,
    struct yt_error *error)
{
	struct yt_door *door = context;
	bool result = yt_database_close_all_method(&door->game.database,
	    file_class, error);

	if (door->game.database.file == NULL)
		door->game_open = false;
	return result;
}

static bool
append_news(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	(void)session;
	return session_close_file5(error) && yt_news_append(text, error);
}

bool
session_append_news_bytes(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	(void)context;
	return session_close_file5(error)
	    && yt_news_append_bytes(text, length, error);
}

static bool
radio_append_raw_bytes(const uint8_t *text, size_t length,
    const uint8_t sender_raw[4], const uint8_t recipient_raw[4],
    struct yt_error *error)
{
	static const uint8_t personal_counter[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t broadcast_counter[4] = {0x00, 0x00, 0x70, 0x85};
	struct yt_radio_file file;
	struct yt_radio_record record;
	struct yt_error close_error;
	uint32_t basic_record;
	const uint8_t *counter_raw;

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)
	    || !yt_radio_file_next_record(&file, &basic_record, error)
	    || !yt_radio_file_get(&file, basic_record, &record, NULL,
	    error))
		goto failed;
	memset(&record, 0, sizeof(record));
	counter_raw = qb_mbf32_decode(recipient_raw) == -2.0f
	    ? broadcast_counter : personal_counter;
	if ((text == NULL && length != 0U)
	    || !yt_radio_set_raw_number(&record, 0U, counter_raw)
	    || !yt_radio_set_raw_number(&record, 4U, recipient_raw)
	    || !yt_radio_set_raw_number(&record, 8U, sender_raw)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "construct radio record");
		}
		goto failed;
	}
	yt_radio_set_text(&record, text, length, 74U);
	if (!yt_radio_file_put(&file, basic_record, &record, error)
	    || !yt_radio_file_close(&file, error))
		return false;
	return true;

failed:
	yt_error_clear(&close_error);
	(void)yt_radio_file_close(&file, &close_error);
	return false;
}

static bool
radio_append_bytes(const uint8_t *text, size_t length, float sender,
    float recipient, struct yt_error *error)
{
	uint8_t sender_raw[4];
	uint8_t recipient_raw[4];

	if (qb_mbf32_encode(sender, sender_raw) != QB_MBF_OK
	    || qb_mbf32_encode(recipient, recipient_raw) != QB_MBF_OK) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "construct radio record");
		}
		return false;
	}
	return radio_append_raw_bytes(text, length, sender_raw, recipient_raw,
	    error);
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

static void
clear_queue(struct yt_session *session)
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

static bool
session_read_upper_command(struct yt_session *session, char *text, size_t size)
{
	if (!session_read_command(session, text, size))
		return false;
	session_compat_upper_n(session, (uint8_t *)text, strlen(text));
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
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

static bool
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

static bool
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

static bool
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

static bool
session_attention(struct yt_session *session, const char *text,
    const char *operation, struct yt_error *error)
{
	return session_attention_bytes(session, (const uint8_t *)text,
	    strlen(text), operation, error);
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

static bool
session_radio_backspace(struct yt_session *session, int line_number,
    size_t shortened_length, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_radio_backspace(line_number,
	    shortened_length, &session->presentation, &presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio body backspace");
	}
	return false;
}

static bool
session_radio_wrap_cleanup(struct yt_session *session, int line_number,
    size_t wrap_marker, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_radio_wrap_cleanup(line_number,
	    wrap_marker, &session->presentation, &presentation);

	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio body wrap cleanup");
	}
	return false;
}

static bool
session_forced_local_line(const uint8_t *text, size_t length,
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
	(void)session_editor_close_all(session);
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
		if (!session_forced_local_line(projection.main.debug,
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
	(void)session_editor_close_all(session);
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

static bool
session_present_paged_line(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	return session_present_paged_fragment(session, text, length);
}

static bool
session_present_alert(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	clear_queue(session);
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

static bool
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

static bool
session_fixed_width(struct yt_session *session, const char *text,
    float width, const char *operation, struct yt_error *error)
{
	return session_fixed_width_bytes(session, (const uint8_t *)text,
	    strlen(text), width, operation, error);
}

static bool
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

static bool
session_centered_line_bytes(struct yt_session *session, const uint8_t *text,
    size_t length,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_centered_line(text, length,
	    &session->presentation, &presentation);
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

static bool
session_centered_line(struct yt_session *session, const char *text,
    const char *operation, struct yt_error *error)
{
	return session_centered_line_bytes(session, (const uint8_t *)text,
	    strlen(text), operation, error);
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
		    || !session_forced_local_line(handler.debug,
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

static bool
xannor_victory_file_present(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	return session_present_text(context, line, length,
	    SESSION_PRESENT_LINE, "Xannor victory file row", error);
}

static bool
xannor_victory_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	return yt_text_sequential_play(path, xannor_victory_file_present,
	    session, error);
}

static bool
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

struct registration_context {
	struct yt_session *session;
	struct yt_database random;
	struct yt_text_input sequential;
	char path[512];
	bool path_resolved;
};

static bool
registration_io_error(struct registration_context *context,
    struct yt_error *error, enum yt_status status, const char *operation)
{
	if (error != NULL) {
		error->status = status;
		error->system_error = status == YT_IO_ERROR ? errno : 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		(void)snprintf(error->path, sizeof(error->path), "%s",
		    context->path_resolved ? context->path : "YT.REG");
	}
	return false;
}

static bool
registration_resolve_path(struct registration_context *context,
    struct yt_error *error)
{
	if (context->path_resolved)
		return true;
	if (!yt_resolve_case_path("YT.REG", true, context->path,
	    sizeof(context->path), error))
		return false;
	context->path_resolved = true;
	return true;
}

static bool
registration_close_file4(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	if (context->sequential.file != NULL
	    || context->sequential.orphaned_file != NULL)
		return yt_text_input_close(&context->sequential, error);
	return yt_database_random_close(&context->random, error);
}

static bool
registration_random_open(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	if (!registration_resolve_path(context, error))
		return false;
	return yt_database_open(&context->random, context->path,
	    YT_OPEN_UPDATE_CREATE, error);
}

static bool
registration_file_size(void *opaque, uint64_t *size,
    struct yt_error *error)
{
	struct registration_context *context = opaque;
	uint32_t length;

	if (context->random.file == NULL)
		return registration_io_error(context, error, YT_INVALID,
		    "registration LOF without file");
	if (!yt_database_random_lof(&context->random, &length, error))
		return false;
	*size = length;
	return true;
}

static bool
registration_delete_empty(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	return yt_file_kill(context->path, error);
}

static bool
registration_sequential_open(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	return yt_text_input_open(&context->sequential, context->path, error);
}

static bool
registration_read_line(void *opaque, uint8_t *data, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct registration_context *context = opaque;
	const uint8_t *line;
	size_t line_length;
	bool available;

	if (context->sequential.file == NULL)
		return registration_io_error(context, error, YT_INVALID,
		    "registration LINE INPUT without file");
	if (!yt_text_input_read_line(&context->sequential, &line,
	    &line_length, &available, error))
		return false;
	if (!available)
		return registration_io_error(context, error, YT_EOF,
		    "registration LINE INPUT past end");
	if (line_length > capacity)
		return registration_io_error(context, error, YT_NO_MEMORY,
		    "registration LINE INPUT string space");
	if (line_length != 0U)
		memcpy(data, line, line_length);
	*length = line_length;
	return true;
}

static bool
registration_centered(void *opaque, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct registration_context *context = opaque;

	return session_centered_line_bytes(context->session, text, length,
	    "registration centered terminal", error);
}

static bool
registration_beep(void *opaque, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_local_beep(&presentation);

	(void)opaque;
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
registration_forced_local(void *opaque, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	(void)opaque;
	return session_forced_local_line(text, length,
	    "registration forced local row", error);
}

static void
registration_close_all(void *opaque)
{
	struct registration_context *context = opaque;

	(void)yt_text_input_close(&context->sequential, NULL);
	yt_database_close(&context->random);
	(void)session_editor_close_all(context->session);
}

static void
registration_end(void *opaque)
{
	struct registration_context *context = opaque;

	/* END performs its own all-file cleanup even without explicit CLOSE ALL. */
	(void)session_editor_close_all(context->session);
	context->session->running = false;
	context->session->terminated = true;
}

static bool
registration(struct yt_session *session, struct yt_error *error)
{
	static const char *const centered[] = {
		"Yankee Trader",
		"(c)Alan Davenport",
		"Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ",
		"Strategy Guide: www.starflt.com/yt.html      ",
		"Version 3.6g * YT * Mod 02/09/2024  ",
	};
	static const struct yt_registration_ops ops = {
		registration_close_file4,
		registration_random_open,
		registration_file_size,
		registration_delete_empty,
		registration_sequential_open,
		registration_read_line,
		registration_centered,
		registration_beep,
		registration_forced_local,
		registration_close_all,
		registration_end,
	};
	struct registration_context context = {.session = session};
	struct yt_registration_state state;
	uint8_t *storage;
	bool completed;
	size_t index;

	for (index = 0U; index < 3U; ++index) {
		if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
		    "registration title blank", error))
			return false;
	}
	if (!session_centered_line(session, centered[0],
	    "registration title", error)
	    || !session_centered_line(session, centered[1],
	    "registration copyright", error)
	    || !session_centered_line(session, centered[2],
	    "registration features", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error)
	    || !session_centered_line(session, centered[3],
	    "registration strategy", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error)
	    || !session_centered_line(session, centered[4],
	    "registration version", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "registration title blank", error))
		return false;
	storage = malloc(5U * YT_REGISTRATION_STRING_MAX);
	if (storage == NULL) {
		if (error != NULL) {
			error->status = YT_NO_MEMORY;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "registration string storage");
		}
		return false;
	}
	memset(&state, 0, sizeof(state));
	for (index = 0U; index < 3U; ++index) {
		state.line[index].data = storage
		    + index * YT_REGISTRATION_STRING_MAX;
		state.line[index].capacity = YT_REGISTRATION_STRING_MAX;
	}
	for (index = 0U; index < 2U; ++index) {
		state.display[index].data = storage
		    + (index + 3U) * YT_REGISTRATION_STRING_MAX;
		state.display[index].capacity = YT_REGISTRATION_STRING_MAX;
	}
	state.beta_only = false;
	state.expected_evaluation_sum[0] = 2085U;
	state.expected_evaluation_sum[1] = 3496U;
	completed = yt_registration_run(&state, &ops, &context, error);
	session->registered = state.registered;
	if (context.sequential.file != NULL
	    || context.sequential.orphaned_file != NULL
	    || context.random.file != NULL)
		(void)registration_close_file4(&context, NULL);
	yt_text_input_destroy(&context.sequential);
	if (!completed) {
		free(storage);
		return false;
	}
	if (state.outcome == YT_REGISTRATION_INVALID_END
	    || state.outcome == YT_REGISTRATION_BETA_END) {
		free(storage);
		return true;
	}
	if (state.outcome == YT_REGISTRATION_ANTI_TAMPER_BUSY_LOOP) {
		/* Immutable shipped literals make this modeled terminal unreachable. */
		session->running = false;
		free(storage);
		return true;
	}
	for (index = 0U; index < 2U; ++index) {
		if (!session_centered_line_bytes(session, state.display[index].data,
		    state.display[index].length, "registration result row", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "registration result blank", error)) {
			free(storage);
			return false;
		}
	}
	free(storage);
	if (state.outcome == YT_REGISTRATION_REGISTERED)
		return session_wait(session, 2.0,
		    "registration registered wait", error);
	return session_wait(session, 10.0, "registration evaluation wait",
	    error);
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

	if (!build_route(session, 1, 2, NULL, false, &found, NULL, NULL,
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

struct lockout_context {
	struct yt_session *session;
	struct yt_database random;
	struct yt_text_input input;
};

static bool
lockout_open_random(void *context, const char *path, struct yt_error *error)
{
	struct lockout_context *lockout = context;

	return yt_database_open(&lockout->random, path, YT_OPEN_UPDATE_CREATE,
	    error);
}

static bool
lockout_empty(void *context, bool *empty, struct yt_error *error)
{
	struct lockout_context *lockout = context;
	uint32_t size;

	if (!yt_database_random_lof(&lockout->random, &size, error))
		return false;
	*empty = size == 0U;
	return true;
}

static bool
lockout_close(void *context, struct yt_error *error)
{
	struct lockout_context *lockout = context;

	if (lockout->random.file != NULL)
		return yt_database_random_close(&lockout->random, error);
	return yt_text_input_close(&lockout->input, error);
}

static bool
lockout_open_input(void *context, const char *path, struct yt_error *error)
{
	struct lockout_context *lockout = context;

	return yt_text_input_open(&lockout->input, path, error);
}

static bool
lockout_read(void *context, const uint8_t **line, size_t *length,
    bool *available, struct yt_error *error)
{
	struct lockout_context *lockout = context;

	return yt_text_input_read_line(&lockout->input, line, length, available,
	    error);
}

static bool
lockout_present(void *context, enum yt_startup_lockout_row row,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	struct lockout_context *lockout = context;
	enum session_present_text_kind kind = row == YT_STARTUP_LOCKOUT_BLANK
	    ? SESSION_PRESENT_LINE : SESSION_PRESENT_BOLD_LINE;
	const char *operation;

	switch (row) {
	case YT_STARTUP_LOCKOUT_BLANK:
		operation = "lockout blank";
		break;
	case YT_STARTUP_LOCKOUT_REVOKED:
		operation = "lockout revoked row";
		break;
	case YT_STARTUP_LOCKOUT_CONTACT:
		operation = "lockout contact row";
		break;
	default:
		return false;
	}
	return session_present_text(lockout->session, text, length, kind,
	    operation, error);
}

static bool
lockout_wait(void *context, float seconds, struct yt_error *error)
{
	struct lockout_context *lockout = context;

	return seconds == 10.0f
	    && session_wait(lockout->session, 10.0, "lockout denial wait",
	    error);
}

static bool
lockout_close_all(void *context, struct yt_error *error)
{
	struct lockout_context *lockout = context;
	bool ok = lockout_close(context, error);

	(void)session_editor_close_all(lockout->session);
	return ok;
}

static void
lockout_end(void *context)
{
	struct lockout_context *lockout = context;

	lockout->session->running = false;
	lockout->session->terminated = true;
}

static bool
lockout(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_startup_lockout_ops ops = {
		lockout_open_random,
		lockout_empty,
		lockout_close,
		lockout_open_input,
		lockout_read,
		lockout_present,
		lockout_wait,
		lockout_close_all,
		lockout_end,
	};
	struct lockout_context context = {
		.session = session,
	};
	struct yt_startup_lockout_state state = {
		.random_path = "LOCKOUT.DAT",
		.input_path = "LOCKOUT.DAT",
	};
	uint8_t live[300];
	size_t live_length;
	char contact[320];
	bool ok;

	if (!yt_startup_canonical_name(
	    (const uint8_t *)session->door->identity.real_first,
	    strlen(session->door->identity.real_first),
	    (const uint8_t *)session->door->identity.real_last,
	    strlen(session->door->identity.real_last), live, sizeof(live),
	    &live_length))
		return false;
	(void)snprintf(contact, sizeof(contact),
	    "Please contact your sysop %s %s.",
	    session->door->identity.sysop_first,
	    session->door->identity.sysop_last);
	state.identity = live;
	state.identity_length = live_length;
	state.contact = (const uint8_t *)contact;
	state.contact_length = strlen(contact);
	yt_text_input_init(&context.input);
	ok = yt_startup_lockout_run(&state, &ops, &context, error);
	yt_database_close(&context.random);
	yt_text_input_destroy(&context.input);
	return ok && !state.denied;
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
	if (!lockout(session, error))
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
	if (!read_player_at_fault(session, player_record, &player,
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

static bool
instruction_offer(struct yt_session *session, struct yt_error *error)
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
		clear_queue(session);
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
	    SESSION_PRESENT_BOLD_LINE, "returning self-denial row", error)
	    || !session_editor_close_all(session))
		return false;
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
			if (!session_close_file5(error)
			    || !yt_news_append_game_full(date, full, error))
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
			if (!session_close_file5(error)
			    || !yt_news_append_new_player(date, full, error))
				return false;
		}
		return instruction_offer(session, error);
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
			if (!session_close_file5(error)
			    || !yt_news_append_login_bytes(
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
radio_name_bytes(struct yt_session *session, float record, uint8_t *dest,
    size_t capacity, size_t *length, bool sender, struct yt_error *error)
{
	const uint8_t *literal;
	size_t literal_length;

	if (length == NULL)
		return false;
	*length = 0;
	if (record > 0.0f) {
		struct yt_player player;
		uint8_t stored[YT_TEXT_FIELD_SIZE];
		size_t stored_length;

		if (!scanner_read_player(session, record, &player, error))
			return false;
		if (!yt_player_stored_name(&player, stored, &stored_length, error))
			return false;
		if (stored_length > capacity)
			goto capacity_error;
		if (stored_length != 0)
			memcpy(dest, stored, stored_length);
		*length = stored_length;
		return true;
	}
	if (!sender) {
		literal = (const uint8_t *)"All";
		literal_length = strlen("All");
	}
	else if (record == -1.0f) {
		literal = (const uint8_t *)"The Xannor";
		literal_length = strlen("The Xannor");
	}
	else {
		literal = (const uint8_t *)"The Mercenaries";
		literal_length = strlen("The Mercenaries");
	}
	if (literal_length > capacity)
		goto capacity_error;
	memcpy(dest, literal, literal_length);
	*length = literal_length;
	return true;

capacity_error:
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "radio display name capacity");
	}
	return false;
}

static bool
radio_read_attach_fault(struct yt_error *error,
    enum yt_basic_fault_site site, uint16_t basic_error)
{
	if (error == NULL)
		return false;
	if (basic_error == 0U && error->basic_error_valid)
		basic_error = error->basic_error;
	if (basic_error == 0U
	    || !yt_error_attach_basic_fault_number(error, site, basic_error))
		(void)yt_error_attach_basic_fault(error, site);
	return false;
}

static bool
radio_read(struct yt_session *session, bool log_mode,
    struct yt_error *error)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t log_heading[] =
	    "Log of messages sent/recieved.";
	static const uint8_t pause[] = "[Pause]";
	static const uint8_t none[] = "None Found.";
	struct yt_radio_file file;
	struct yt_radio_pager_state pager;
	uint64_t byte_length;
	uint64_t probe_count;
	float previous_recipient = 0.0f;
	float previous_sender = 0.0f;
	float current_player = (float)session_record(session);
	bool visible = false;
	uint32_t record_number;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "radio opening blank", error)) {
		return radio_read_attach_fault(error,
		    YT_BASIC_FAULT_RADIO_OPENING_OUTPUT, 0U);
	}
	if (!session_present_text(session,
	    log_mode ? log_heading : automatic_heading,
	    log_mode ? sizeof(log_heading) - 1U
	    : sizeof(automatic_heading) - 1U, SESSION_PRESENT_LINE,
	    "radio heading", error)) {
		return radio_read_attach_fault(error,
		    log_mode ? YT_BASIC_FAULT_RADIO_LOG_HEADING_OUTPUT
		    : YT_BASIC_FAULT_RADIO_AUTO_HEADING_OUTPUT, 0U);
	}

	yt_radio_pager_begin(&pager);
	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)) {
		uint16_t basic_error = file.random.last_open.basic_error != 0U
		    ? file.random.last_open.basic_error
		    : file.random.last_close.basic_error;

		return radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_OPEN,
		    basic_error);
	}
	if (!yt_radio_file_size(&file, &byte_length, error)) {
		(void)radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_LOF,
		    file.random.last_lof.basic_error);
		goto abort;
	}
	probe_count = byte_length / YT_RADIO_RECORD_SIZE + 1U;
	if (probe_count > 0xFFFFFFU) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "radio scan bound");
			(void)snprintf(error->path, sizeof(error->path), "%s",
			    file.random.path);
		}
		goto abort;
	}

	for (record_number = 1U; record_number <= probe_count;
	    ++record_number) {
		struct yt_radio_record record;
		struct yt_radio_reader_decision decision;
		float counter;
		float recipient;
		float sender;

		if (!yt_radio_file_get(&file, record_number, &record, NULL,
		    error)) {
			(void)radio_read_attach_fault(error,
			    YT_BASIC_FAULT_RADIO_RECORD_GET,
			    file.random.last_get.basic_error);
			goto abort;
		}
		counter = yt_radio_get_number(&record, 0U);
		recipient = yt_radio_get_number(&record, 4U);
		sender = yt_radio_get_number(&record, 8U);
		if (!yt_radio_reader_decide(counter, recipient, sender,
		    current_player, log_mode ? 1.0f : 0.0f, &decision, error))
			goto abort;
		if (!decision.visible)
			continue;

		{
			uint8_t from[YT_TEXT_FIELD_SIZE];
			uint8_t to[YT_TEXT_FIELD_SIZE];
			uint8_t header[2U * YT_TEXT_FIELD_SIZE + 32U];
			size_t from_length;
			size_t to_length;
			size_t header_length;

			visible = true;
			if (!radio_name_bytes(session, recipient, to, sizeof(to),
			    &to_length, false, error)) {
				if (recipient > 0.0f)
					(void)radio_read_attach_fault(error,
					    YT_BASIC_FAULT_RADIO_RECIPIENT_GET,
					    session->door->game.database.last_get.basic_error);
				goto abort;
			}
			if (!radio_name_bytes(session, sender, from, sizeof(from),
			    &from_length, true, error)) {
				if (sender > 0.0f)
					(void)radio_read_attach_fault(error,
					    YT_BASIC_FAULT_RADIO_SENDER_GET,
					    session->door->game.database.last_get.basic_error);
				goto abort;
			}
			if (sender != previous_sender
			    || recipient != previous_recipient) {
				if (!yt_radio_reader_header(to, to_length, from,
				    from_length, header, sizeof(header),
				    &header_length)
				    || !session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "radio pair blank", error)
				    || !session_present_text(session, header,
				    header_length, SESSION_PRESENT_LINE,
				    "radio pair header", error))
					goto abort;
				yt_radio_pager_add_pair(&pager);
			}
		}

		if (!session_present_text(session, record.bytes + 12U, 74U,
		    SESSION_PRESENT_LINE, "radio body", error))
			goto abort;
		previous_sender = sender;
		previous_recipient = recipient;
		if (yt_radio_pager_add_body(&pager)) {
			if (!session_present_text(session, pause,
			    sizeof(pause) - 1U, SESSION_PRESENT_RAW,
			    "radio pause", error)) {
				(void)radio_read_attach_fault(error,
				    YT_BASIC_FAULT_RADIO_PAUSE_OUTPUT, 0U);
				goto abort;
			}
			if (!session_wait(session, 99.0,
			    "radio private-pager wait", error)) {
				(void)radio_read_attach_fault(error,
				    YT_BASIC_FAULT_RADIO_PRIVATE_WAIT, 0U);
				goto abort;
			}
			if (!session_present_text(session, NULL, 0U,
			    SESSION_PRESENT_LINE, "radio pause blank", error))
				goto abort;
		}
		if (decision.automatic_write
		    && (!yt_radio_reader_mutate(&record, counter)
		    || !yt_radio_file_put(&file, record_number, &record,
		    error)))
			goto abort;
	}

	if (!visible && !session_present_text(session, none,
	    sizeof(none) - 1U, SESSION_PRESENT_LINE, "radio none found",
	    error))
		goto abort;
	if (!yt_radio_file_close(&file, error)) {
		return radio_read_attach_fault(error,
		    YT_BASIC_FAULT_RADIO_FINAL_CLOSE,
		    file.random.last_close.basic_error);
	}
	return true;

abort:
	(void)yt_radio_file_close(&file, NULL);
	return false;
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
	if (!show_ship(session, error))
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
	if (!radio_read(session, false, error))
		return false;
	return true;
}

static float
current_minute(void)
{
	return single_div((float)yt_platform_timer(), 60.0f);
}

static bool
port_update_read_sector(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_UPDATER_SECTOR_GET, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
port_update_observe_day(void *context, float *current_day,
    struct yt_error *error)
{
	struct yt_session *session = context;
	int today;
	int adjusted_year;

	if (!session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	*current_day = (float)today;
	return true;
}

static bool
port_update_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_UPDATER_PORT_GET, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
port_update_observe_timer(void *context, float *timer_seconds,
    struct yt_error *error)
{
	(void)context;
	(void)error;
	*timer_seconds = (float)yt_platform_timer();
	return true;
}

static bool
port_update_write_port(void *context, uint32_t physical_record,
    const struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return write_database_record_at_fault(session, physical_record,
	    &port->record, YT_BASIC_FAULT_PORT_UPDATER_PORT_PUT, error);
}

static bool
port_update(struct yt_session *session, int sector_number,
    const float *sector_record_expression, const struct yt_sector *loaded_sector,
    struct yt_port_market_state *market, struct yt_error *error)
{
	static const struct yt_port_update_ops ops = {
		port_update_read_sector,
		port_update_observe_day,
		port_update_read_port,
		port_update_observe_timer,
		port_update_write_port,
	};
	struct yt_port_update_state state;

	if (market == NULL)
		return false;
	memset(&state, 0, sizeof(state));
	state.sector_number = sector_number;
	state.sector_record_offset = session_sector_offset(session);
	if (sector_record_expression != NULL) {
		state.sector_record_expression = *sector_record_expression;
		state.sector_record_supplied = true;
	}
	state.port_offset = session_port_offset(session);
	memcpy(state.base_price, session->market_bases, sizeof(state.base_price));
	if (loaded_sector != NULL) {
		state.sector = *loaded_sector;
		state.sector_loaded = true;
	}
	if (!yt_port_update_run(&state, &ops, session, error))
		return false;
	*market = state.market;
	return true;
}

struct planet_update_cache {
	float rate[10];
	double quantity[10];
	float contribution[10];
};

static bool
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

static bool
write_planet_physical(struct yt_session *session, uint32_t physical_record,
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

static bool
planet_updater_date(void *context, uint8_t current_day_raw[4],
    struct yt_error *error)
{
	struct yt_session *session = context;
	int today;
	int adjusted_year;

	if (!session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (qb_mbf32_encode((float)today, current_day_raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet updater current day MBF32");
		}
		return false;
	}
	return true;
}

static bool
planet_updater_record_expression(void *context, bool closing,
    struct yt_error *error)
{
	(void)context;
	(void)closing;
	(void)error;
	return true;
}

static bool
planet_updater_get(void *context, uint32_t physical_record,
    struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_read(&session->door->game.database,
	    (size_t)physical_record, record, error);
}

static bool
planet_updater_timer(void *context, uint8_t timer_seconds_raw[4],
    struct yt_error *error)
{
	float timer_seconds;

	(void)context;
	timer_seconds = (float)yt_platform_timer();
	if (qb_mbf32_encode(timer_seconds, timer_seconds_raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet updater TIMER MBF32");
		}
		return false;
	}
	return true;
}

static bool
planet_updater_lset(void *context, enum yt_planet_updater_stage stage,
    size_t offset, const uint8_t raw[4], struct yt_error *error)
{
	(void)context;
	(void)stage;
	(void)offset;
	(void)raw;
	(void)error;
	return true;
}

static bool
planet_updater_put(void *context, uint32_t physical_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, record, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static void
planet_updater_load_cache(struct yt_session *session,
    struct yt_planet_updater_state *state)
{
	state->raw_cache = session->planet_updater_cache;
	memcpy(state->current_day_raw, session->planet_updater_day_raw,
	    sizeof(state->current_day_raw));
}

static void
planet_updater_store_cache(struct yt_session *session,
    const struct yt_planet_updater_state *state)
{
	session->planet_updater_cache = state->raw_cache;
	memcpy(session->planet_updater_day_raw, state->current_day_raw,
	    sizeof(session->planet_updater_day_raw));
}

static bool
planet_update_cached_physical(struct yt_session *session,
    uint32_t physical_record,
    struct yt_planet *planet, struct planet_update_cache *cache,
    struct yt_error *error)
{
	static const struct yt_planet_updater_ops ops = {
		planet_updater_date,
		planet_updater_record_expression,
		planet_updater_get,
		planet_updater_timer,
		planet_updater_lset,
		planet_updater_put,
	};
	struct yt_planet_updater_state state = {0};
	float logical;
	float expression;

	logical = single_sub((float)physical_record,
	    session_planet_offset(session));
	expression = single_add(session_planet_offset(session),
	    logical);
	if (qb_brun_random_record_number(expression) != physical_record
	    || qb_mbf32_encode(logical, state.logical_planet_raw)
	    == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(session_planet_offset(session),
	    state.planet_offset_raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet updater physical record");
		}
		return false;
	}
	planet_updater_load_cache(session, &state);
	if (!yt_planet_updater_raw_run(&state, &ops, session, error)) {
		planet_updater_store_cache(session, &state);
		return false;
	}
	planet_updater_store_cache(session, &state);
	yt_planet_decode(planet, &state.field);
	memcpy(session->planet_quantity, state.cache.quantity,
	    sizeof(session->planet_quantity));
	if (cache != NULL) {
		memcpy(cache->rate, state.cache.production, sizeof(cache->rate));
		memcpy(cache->quantity, state.cache.quantity,
		    sizeof(cache->quantity));
		memcpy(cache->contribution, state.cache.contribution,
		    sizeof(cache->contribution));
	}
	return true;
}

static bool
planet_update_cached(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct planet_update_cache *cache,
    struct yt_error *error)
{
	uint32_t physical = session_planet_basic_record(session,
	    (float)logical_planet);

	return planet_update_cached_physical(session, physical,
	    planet, cache, error);
}

static bool
planet_update(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
{
	return planet_update_cached(session, logical_planet, planet, NULL, error);
}

static bool
friendship_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	return yt_game_read_player(context, player_record, player, error);
}

static bool
same_team(struct yt_session *session, int other_record,
    struct yt_error *error)
{
	bool friendly;

	if (!yt_friendship_resolve((float)other_record,
	    (float)session_record(session),
	    session_sector_offset(session),
	    friendship_read_player, &session->door->game, &friendly, error))
		return false;
	return friendly;
}

static bool
sector_force_friendly(struct yt_session *session,
    const struct yt_sector *sector, struct yt_error *error)
{
	enum yt_sector_force_route route;
	int owner;

	if (!yt_sector_force_route(sector->fighters, sector->fighter_owner,
	    session_record(session), &route, &owner, error))
		return false;
	if (route == YT_SECTOR_FORCE_FRIENDLY)
		return true;
	if (route == YT_SECTOR_FORCE_OWNER_GET)
		return same_team(session, owner, error);
	return false;
}

static bool
scanner_read_sector(struct yt_session *session, float logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = session_sector_basic_record(session, logical_sector);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
scanner_read_port(struct yt_session *session, float logical_port,
    struct yt_port *port, uint32_t *physical_record, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = session_port_basic_record(session, logical_port);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_port_decode(port, &record);
	if (physical_record != NULL)
		*physical_record = physical;
	return true;
}

static bool
scanner_write_port(struct yt_session *session, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	yt_record_set_number_if_changed(&port->record, YT_F93, port->sector);
	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
}

static bool
scanner_read_planet(struct yt_session *session, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

static bool
scanner_read_player(struct yt_session *session, float basic_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = qb_brun_random_record_number(basic_record);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

static bool
scanner_read_current_player(struct yt_session *session,
    struct yt_error *error)
{
	return yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error);
}

static bool
scanner_read_team_overlay(struct yt_session *session, float team,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = session_sector_basic_record(session, team);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_sector_decode(overlay, &record);
	return true;
}

static void
scanner_cache_hostile_sector(struct yt_session *session,
    const struct yt_sector *sector)
{
	session->hostile_deployed_fighters = (double)qb_mbf32_decode(
	    &sector->record.bytes[YT_F81]);
	session->hostile_owner = qb_mbf32_decode(
	    &sector->record.bytes[YT_F85]);
}

static bool
display_sector_one(struct yt_session *session, float logical_sector,
    struct yt_sector_pager_state *private_pager, struct yt_error *error)
{
	struct yt_sector sector;
	char sector_number[64];
	uint8_t row[512];
	size_t row_length;
	size_t slot;
	int basic;
	bool first_visible = true;
	bool first_warp = true;

	session_set_current_sector_record(session, single_add(
	    session_sector_offset(session), logical_sector));
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector leading blank", error))
		return false;
	if (qb_str_single(sector_number, sizeof(sector_number),
	    logical_sector) < 0)
		return false;
	row_length = sizeof("Sector:") - 1U;
	memcpy(row, "Sector:", row_length);
	memcpy(row + row_length, sector_number, strlen(sector_number));
	row_length += strlen(sector_number);
	if (!session_present_text(session, row, row_length,
	    SESSION_PRESENT_LINE, "sector number row", error))
		return false;
	yt_sector_pager_add(private_pager, 1.0f);
	if (session_is_disruption_sector(session, logical_sector)
	    && !session_attention(session,
	    "** Space-time disruption detected! **",
	    "sector disruption attention", error))
		return false;
	if (session_is_disruption_sector(session, logical_sector))
		yt_sector_pager_add(private_pager, 1.0f);
	if (sector.mines != 0.0f) {
		if (!yt_sector_mine_warning_row(sector.mines, row,
		    sizeof(row) - 1U, &row_length))
			return false;
		row[row_length] = '\0';
		if (!session_attention(session, (const char *)row,
		    "sector mine attention", error))
			return false;
		for (slot = 0; slot < 3; ++slot) {
			if (!session_sound(session, 4.0f,
			    "sector mine follow-up sound", error))
				return false;
		}
		yt_sector_pager_add(private_pager, 1.0f);
	}
	if (sector.port > 0.0f) {
		struct yt_port port;
		uint32_t physical_port;

		if (!scanner_read_port(session, sector.port, &port,
		    &physical_port, error)
		    || !yt_sector_port_row(&port, row, sizeof(row), &row_length,
		    error)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "sector port row", error))
			return false;
		port.sector = logical_sector;
		if (!scanner_write_port(session, physical_port, &port,
		    error))
			return false;
		yt_sector_pager_add(private_pager, 1.0f);
	}
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	if (sector.planet > 0.0f) {
		struct yt_planet planet;
		uint32_t physical_planet = session_planet_basic_record(session,
		    sector.planet);
		float saved_foreground;

		if (!planet_update_cached_physical(session, physical_planet,
		    &planet, NULL, error)
		    || !scanner_read_planet(session, physical_planet, &planet,
		    error)
		    || !yt_sector_planet_row(&planet, row, sizeof(row),
		    &row_length, error))
			return false;
		saved_foreground = session_foreground(session);
		session_set_foreground(session, 3.0f);
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "sector planet row", error))
			return false;
		session_set_foreground(session, saved_foreground);
		yt_sector_pager_add(private_pager, 1.0f);
		if (!scanner_read_sector(session, logical_sector, &sector, error))
			return false;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session_sector_offset(session); ++basic) {
		float random_value;

		if (!yt_sector_candidate_eligible(basic, session_record(session),
		    session_player_cache_value(session, basic,
		    YT_PLAYER_CACHE_SECTOR), logical_sector))
			continue;
		{
			uint8_t cloak_raw[4];

			session_player_cache_raw(session, basic,
			    YT_PLAYER_CACHE_CLOAK, cloak_raw);
			session->player_cache.cloak[basic] = qb_mbf32_decode(cloak_raw);
		}
		if (!yt_random_next(&session->door->game.random, &random_value,
		    error))
			return false;
		if (yt_sector_cloak_revealed(random_value,
		    session->player_cache.cloak[basic])) {
			static const uint8_t shimmer[] =
			    "You detect the shimmering of a cloaking device!";

			if (!session_present_text(session, shimmer,
			    sizeof(shimmer) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "sector cloak shimmer row", error))
				return false;
			yt_sector_pager_add(private_pager, 1.0f);
			{
				static const uint8_t zero[4] = {0};

				session_set_player_cache_raw(session, basic,
				    YT_PLAYER_CACHE_CLOAK, zero);
				session->player_cache.cloak[basic] = 0.0f;
			}
			if (!session_sound(session, 4.0f,
			    "sector cloak-reveal sound", error))
				return false;
		}
		if (session->player_cache.cloak[basic] == 0.0f) {
			struct yt_player other;

			yt_sector_pager_add(private_pager, 1.0f);
			if (first_visible) {
				static const uint8_t heading[] = "Other Ships: ";

				if (!session_present_text(session, heading,
				    sizeof(heading) - 1U,
				    SESSION_PRESENT_BOLD_LINE,
				    "sector other-ships heading", error))
					return false;
				first_visible = false;
			}
			if (!yt_game_read_player(&session->door->game, basic,
			    &other, error)
			    || !yt_sector_player_row(&other, row, sizeof(row),
			    &row_length, error)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "sector visible-player row", error))
				return false;
		}
	}
	if (!scanner_read_sector(session, logical_sector, &sector, error))
		return false;
	scanner_cache_hostile_sector(session, &sector);
	if (sector.fighters != 0.0f) {
		static const uint8_t heading[] = "Fighters in sector:";
		struct yt_player owner;
		struct yt_sector team_overlay;
		const struct yt_player *owner_pointer = NULL;
		const struct yt_sector *team_pointer = NULL;
		bool scratch_changed;
		bool owner_team_nonzero = false;
		size_t scratch_length = session->hostile_owner_label_length;

		if (!session_present_text(session, heading,
		    sizeof(heading) - 1U, SESSION_PRESENT_BOLD_RAW,
		    "sector fighter heading", error))
			return false;
		if (sector.fighter_owner != -1.0f
		    && sector.fighter_owner != -2.0f
		    && sector.fighter_owner != (float)session_record(session)) {
			if (!scanner_read_player(session, sector.fighter_owner,
			    &owner, error))
				return false;
			owner_pointer = &owner;
			owner_team_nonzero = owner.team != 0.0f;
			if (owner_team_nonzero) {
				uint8_t owner_name[YT_TEXT_FIELD_SIZE];
				size_t owner_name_length;
				char team_number[64];
				int team_number_length;
				static const uint8_t team_prefix[] = " Team [";

				if (!yt_player_stored_name(&owner, owner_name,
				    &owner_name_length, error))
					return false;
				team_number_length = qb_str_single(team_number,
				    sizeof(team_number), owner.team);
				if (team_number_length < 1
				    || owner_name_length + sizeof(team_prefix) - 1U
				    + (size_t)team_number_length >
				    sizeof(session->hostile_owner_label))
					return false;
				memcpy(session->hostile_owner_label, owner_name,
				    owner_name_length);
				scratch_length = owner_name_length;
				memcpy(session->hostile_owner_label + scratch_length,
				    team_prefix, sizeof(team_prefix) - 1U);
				scratch_length += sizeof(team_prefix) - 1U;
				memcpy(session->hostile_owner_label + scratch_length,
				    team_number + 1,
				    (size_t)team_number_length - 1U);
				scratch_length += (size_t)team_number_length - 1U;
				session->hostile_owner_label[scratch_length++] = ']';
				session->hostile_owner_label_length = scratch_length;
				if (!scanner_read_team_overlay(session, owner.team,
				    &team_overlay, error))
					return false;
				team_pointer = &team_overlay;
			}
		}
		if (!yt_sector_fighter_row(&sector, session_record(session),
		    owner_pointer, team_pointer, row, sizeof(row), &row_length,
		    session->hostile_owner_label,
		    sizeof(session->hostile_owner_label), &scratch_length,
		    &scratch_changed, error)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "sector fighter owner row", error))
			return false;
		if (scratch_changed)
			session->hostile_owner_label_length = scratch_length;
		yt_sector_pager_add(private_pager,
		    owner_team_nonzero ? 3.0f : 2.0f);
	}
	if (!session_present_text(session, (const uint8_t *)"Warps lead to:",
	    sizeof("Warps lead to:") - 1U, SESSION_PRESENT_RAW,
	    "sector warp heading", error))
		return false;
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] != 0.0f) {
			char warp[64];
			int warp_size;
			size_t fragment_length = 0U;

			warp_size = qb_str_single(warp, sizeof(warp),
			    sector.warps[slot]);
			if (warp_size < 0)
				return false;
			if (!first_warp)
				row[fragment_length++] = ',';
			memcpy(row + fragment_length, warp, (size_t)warp_size);
			fragment_length += (size_t)warp_size;
			if (!session_present_text(session, row, fragment_length,
			    SESSION_PRESENT_RAW,
			    "sector warp target", error))
				return false;
			first_warp = false;
		}
	}
	if (!session_present_text(session, NULL, 0U,
	    SESSION_PRESENT_LINE, "sector warp terminator", error))
		return false;
	yt_sector_pager_add(private_pager, 1.0f);
	if (yt_sector_pager_finish_sector(private_pager)) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "sector private-pause blank", error))
			return false;
		session_set_foreground(session, 7.0f);
		if (!session_present_text(session,
		    (const uint8_t *)"[ Pause ]", strlen("[ Pause ]"),
		    SESSION_PRESENT_BOLD_LINE, "sector private-pause prompt",
		    error))
			return false;
		if (!session_timed_wait(session, 15.0)) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				snprintf(error->operation, sizeof(error->operation),
				    "sector private-pause wait");
			}
			return false;
		}
		session_set_foreground(session, 1.0f);
	}
	return true;
}

static bool
display_sector(struct yt_session *session, bool adjacent,
    struct yt_error *error)
{
	float current;
	struct yt_sector_pager_state private_pager;
	float caller_warps[6];
	float targets[6];
	float saved_foreground = session_foreground(session);
	size_t target_count;
	size_t slot;

	yt_sector_pager_begin(&private_pager);
	if (!adjacent) {
		session_set_foreground(session, 1.0f);
		if (!scanner_read_current_player(session, error))
			return false;
		current = session->player.sector;
		if (!display_sector_one(session, current, &private_pager, error)
		    || !scanner_read_current_player(session, error))
			return false;
		session_set_foreground(session, saved_foreground);
		return true;
	}
	session_current_warps(session, caller_warps);
	target_count = yt_sector_sensor_targets(caller_warps, targets);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor leading blank", error))
		return false;
	session_set_foreground(session, 7.0f);
	if (!session_present_text(session,
	    (const uint8_t *)"[ Sensors Activated ]",
	    strlen("[ Sensors Activated ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor heading", error))
		return false;
	if (!session_sound(session, 4.0f,
	    "adjacent-sector sensor sound", error))
		return false;
	session_set_foreground(session, 1.0f);
	for (slot = 0; slot < target_count; ++slot) {
		if (!display_sector_one(session, targets[slot],
		    &private_pager, error))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor ending blank", error))
		return false;
	session_set_foreground(session, 7.0f);
	if (!session_present_text(session,
	    (const uint8_t *)"[ End Sensor Scan ]",
	    strlen("[ End Sensor Scan ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor ending", error)
	    || !scanner_read_current_player(session, error))
		return false;
	session_set_foreground(session, saved_foreground);
	return true;
}

static bool
display_current_sector_cached(struct yt_session *session,
    struct yt_error *error)
{
	struct yt_sector_pager_state private_pager;
	float saved_foreground = session_foreground(session);
	float current = session->player.sector;
	bool ok;

	yt_sector_pager_begin(&private_pager);
	session_set_foreground(session, 1.0f);
	ok = display_sector_one(session, current, &private_pager, error)
	    && scanner_read_current_player(session, error);
	if (ok) {
		session_set_foreground(session, saved_foreground);
	}
	return ok;
}

static bool
danger_scan_read_sector(void *context, float logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;
	uint32_t physical = session_sector_basic_record(session, logical_sector);

	if (!yt_database_read(&session->door->game.database, physical, &record,
	    error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
danger_scan_read_player(void *context, float record,
    struct yt_player *player, struct yt_error *error)
{
	return scanner_read_player(context, record, player, error);
}

static bool
danger_scan_restore_current(void *context, struct yt_error *error)
{
	return session_reload_player(context, error);
}

static bool
danger_scan_checkpoint(void *context,
    enum yt_danger_scan_checkpoint checkpoint, struct yt_error *error)
{
	(void)context;
	(void)checkpoint;
	(void)error;
	return true;
}

static bool
danger_scan_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "danger warning sound", error);
}

static bool
danger_scan_present(void *context, const uint8_t *text, size_t length,
    enum yt_danger_scan_output_kind kind, struct yt_error *error)
{
	static const enum session_present_text_kind kinds[] = {
		[YT_DANGER_SCAN_LEADING_BLANK] = SESSION_PRESENT_LINE,
		[YT_DANGER_SCAN_WARNING_RAW] = SESSION_PRESENT_BOLD_RAW,
		[YT_DANGER_SCAN_WARNING_TARGET] = SESSION_PRESENT_BOLD_LINE,
		[YT_DANGER_SCAN_WARNING_BLANK] = SESSION_PRESENT_LINE,
		[YT_DANGER_SCAN_DISRUPTION] = SESSION_PRESENT_BOLD_LINE,
		[YT_DANGER_SCAN_MINES] = SESSION_PRESENT_BOLD_LINE,
		[YT_DANGER_SCAN_FIGHTERS] = SESSION_PRESENT_BOLD_LINE,
		[YT_DANGER_SCAN_FINAL_BLANK] = SESSION_PRESENT_LINE,
		[YT_DANGER_SCAN_DEACTIVATED] = SESSION_PRESENT_BOLD_LINE,
	};
	static const char *const operations[] = {
		[YT_DANGER_SCAN_LEADING_BLANK] = "danger leading blank",
		[YT_DANGER_SCAN_WARNING_RAW] = "danger warning header",
		[YT_DANGER_SCAN_WARNING_TARGET] = "danger warning target",
		[YT_DANGER_SCAN_WARNING_BLANK] = "danger warning blank",
		[YT_DANGER_SCAN_DISRUPTION] = "danger disruption row",
		[YT_DANGER_SCAN_MINES] = "danger mines row",
		[YT_DANGER_SCAN_FIGHTERS] = "danger fighters row",
		[YT_DANGER_SCAN_FINAL_BLANK] = "danger final blank",
		[YT_DANGER_SCAN_DEACTIVATED] = "danger deactivation row",
	};

	if ((size_t)kind >= YT_ARRAY_LEN(kinds))
		return false;
	return session_present_text(context, text, length, kinds[kind],
	    operations[kind], error);
}

static float
danger_scan_foreground(void *context)
{
	return session_foreground(context);
}

static void
danger_scan_set_foreground(void *context, float value)
{
	session_set_foreground(context, value);
}

static void
danger_scan_set_background(void *context, float value)
{
	struct yt_session *session = context;

	yt_present_set_background(&session->presentation, value);
}

static void
danger_scan_set_blink(void *context, float value)
{
	struct yt_session *session = context;

	yt_present_set_blink(&session->presentation, value);
}

static void
danger_scan_store_relationship(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->shared_status = qb_mbf32_decode(raw);
}

static bool
dangerous_destination(struct yt_session *session, float target,
    bool *danger, struct yt_error *error)
{
	static const struct yt_danger_scan_ops ops = {
		danger_scan_read_sector,
		danger_scan_read_player,
		danger_scan_restore_current,
		danger_scan_checkpoint,
		danger_scan_sound,
		danger_scan_present,
		danger_scan_foreground,
		danger_scan_set_foreground,
		danger_scan_set_background,
		danger_scan_set_blink,
		danger_scan_store_relationship,
	};
	struct yt_danger_scan_state state = {
		.target = target,
		.sector_count = (float)session_sector_count(session),
		.sector_offset = session_sector_offset(session),
		.current_player_record = (float)session_record(session),
		.disruption_sectors = {
			session->disruption_sectors[0],
			session->disruption_sectors[1],
		},
	};
	bool result;

	if (danger == NULL)
		return false;
	(void)qb_mbf32_encode(session->shared_status, state.relationship_raw);
	result = yt_danger_scan_run(&state, &ops, session, error);
	*danger = state.finding_flag != 0.0f;
	return result;
}

static bool
spy_read_sector(void *context, int logical_sector, struct yt_sector *sector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector,
	    sector, error);
}

static bool
spy_update_planet(void *context, float link, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_planet planet;
	uint32_t physical = session_planet_basic_record(session, link);

	return planet_update_cached_physical(session, physical, &planet, NULL,
	    error);
}

static bool
spy_read_planet(void *context, float link, struct yt_planet *planet,
    struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = session_planet_basic_record(session, link);

	return read_planet_physical(session, physical, planet, error);
}

static bool
spy_read_player(void *context, float record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record raw;
	uint32_t physical = qb_brun_random_record_number(record);

	if (!yt_database_read(&session->door->game.database, physical, &raw,
	    error))
		return false;
	yt_player_decode(player, &raw);
	return true;
}

static bool
spy_read_team(void *context, float team, struct yt_sector *overlay,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record raw;
	uint32_t physical = session_sector_basic_record(session, team);

	if (!yt_database_read(&session->door->game.database, physical, &raw,
	    error))
		return false;
	yt_sector_decode(overlay, &raw);
	return true;
}

static bool
spy_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, selector == 9.0f
	    ? "spy finding sound" : "spy cloak sound", error);
}

static void
spy_import_presentation(struct yt_session *session,
    const struct yt_spy_sweep_state *state)
{
	session_set_foreground(session, state->foreground);
	yt_present_set_background(&session->presentation, state->background);
	yt_present_set_bold(&session->presentation, state->bold);
	yt_present_set_blink(&session->presentation, state->blink);
}

static void
spy_export_presentation(struct yt_spy_sweep_state *state,
    const struct yt_session *session)
{
	state->foreground = session_foreground(session);
	state->background = yt_present_background(&session->presentation);
	state->bold = yt_present_bold(&session->presentation);
	state->blink = yt_present_blink(&session->presentation);
}

static bool
spy_present(void *context, const uint8_t *text, size_t length,
    enum yt_spy_output_kind kind, struct yt_spy_sweep_state *state,
    struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	spy_import_presentation(session, state);
	if (kind == YT_SPY_ATTENTION)
		result = session_attention_bytes(session, text, length,
		    "spy attention row", error);
	else {
		enum session_present_text_kind session_kind;

		switch (kind) {
		case YT_SPY_LINE:
			session_kind = SESSION_PRESENT_LINE;
			break;
		case YT_SPY_BOLD_LINE:
			session_kind = SESSION_PRESENT_BOLD_LINE;
			break;
		case YT_SPY_BOLD_RAW:
			session_kind = SESSION_PRESENT_BOLD_RAW;
			break;
		default:
			return false;
		}
		result = session_present_text(session, text, length, session_kind,
		    "spy direct output", error);
	}
	spy_export_presentation(state, session);
	return result;
}

static bool
spy_pause(void *context, struct yt_spy_sweep_state *state,
    struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	spy_import_presentation(session, state);
	result = session_press_any_key(session, true, error);
	spy_export_presentation(state, session);
	return result;
}

static bool
spy_sweep(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_spy_sweep_ops ops = {
		spy_read_sector,
		spy_update_planet,
		spy_read_planet,
		spy_read_player,
		spy_read_team,
		random_value,
		spy_sound,
		spy_present,
		spy_pause,
	};
	struct yt_spy_sweep_state state;
	bool result;

	state = (struct yt_spy_sweep_state){
		.active_spies = session->spy_count,
		.spy_sectors = session->spy_sectors,
		.last_reported_sectors = session->spy_markers,
		.current_player_record = session_record(session),
		.last_player_record = session_sector_offset(session),
		.disruption_sectors = {
			session->disruption_sectors[0],
			session->disruption_sectors[1]
		},
		.player_cache = &session->player_cache,
		.found = session->spy_found,
		.foreground = session_foreground(session),
		.background = yt_present_background(&session->presentation),
		.bold = yt_present_bold(&session->presentation),
		.blink = yt_present_blink(&session->presentation),
	};
	result = yt_spy_sweep_run(&state, &ops, session, error);

	session->spy_found = state.found;
	spy_import_presentation(session, &state);
	return result;
}

static bool
fresh_no_turn_gate(struct yt_session *session, bool *denied,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Sorry but you have no turns left.";
	uint8_t result_raw[4];

	if (!session_reload_player(session, error))
		return false;
	yt_no_turn_gate_result_raw(false, result_raw);
	session->shared_status = qb_mbf32_decode(result_raw);
	*denied = yt_no_turn_gate_denied(session->player.turns);
	if (*denied) {
		yt_no_turn_gate_result_raw(true, result_raw);
		session->shared_status = qb_mbf32_decode(result_raw);
		return session_present_alert(session, notice, sizeof(notice) - 1U,
		    "no-turn gate notice", error);
	}
	return true;
}

static bool
finalize_action(struct yt_session *session, float amount,
    struct yt_error *error)
{
	static const float cloak_display_scale = 50.0f;
	static const float turn_divisor = 25.0f;
	static const float xannor_threshold = 0.99f;
	int xannor_provoker;
	float quotient;
	float draw;
	char number[64];
	char row[128];
	uint8_t turn_raw[4];
	bool anti_cloak_allows;

	(void)amount;
	if (!spy_sweep(session, error) || !session_reload_player(session, error))
		return false;
	memcpy(turn_raw, session->player.record.bytes + YT_F49,
	    sizeof(turn_raw));
	if (!yt_action_finalizer_turn_raw(turn_raw, turn_raw))
		return false;
	session->player.turns = qb_mbf32_decode(turn_raw);
	if (!yt_record_set_raw_number(&session->player.record, YT_F49, turn_raw))
		return false;
	quotient = single_div(session->player.turns, turn_divisor);
	anti_cloak_allows = !session->anti_cloak_enabled;
	if (quotient == floorf(quotient) && anti_cloak_allows) {
		float display;
		float saved_foreground;
		uint8_t cloak_arithmetic[4];
		uint8_t cloak_result[4];
		bool cloak_clamped;
		int cache_record;

		memcpy(cloak_result, session->player.record.bytes + YT_F125,
		    sizeof(cloak_result));
		if (!yt_action_finalizer_cloak_raw(cloak_result,
		    cloak_arithmetic, cloak_result, &cloak_clamped))
			return false;
		session->player.cloak = qb_mbf32_decode(cloak_result);
		if (!yt_record_set_raw_number(&session->player.record, YT_F125,
		    cloak_result))
			return false;
		cache_record = session_record(session);
		session_set_player_cache_raw(session, cache_record,
		    YT_PLAYER_CACHE_CLOAK,
		    session->player.record.bytes + YT_F125);
		display = floorf(single_mul(session->player.cloak,
		    cloak_display_scale));
		qb_str_single(number, sizeof(number), display);
		snprintf(row, sizeof(row), "Cloak at%s%%", number);
		saved_foreground = session_foreground(session);
		session_set_foreground(session, 7.0f);
		if (!session_present_timed_paged_row(session, (const uint8_t *)row, strlen(row),
		    "action-finalizer cloak row", error))
			return false;
		session_set_foreground(session, saved_foreground);
		if (session->player.cloak == 0.0f) {
			if (!session_attention(session,
			    " WARNING! CLOAK EXPIRED!",
			    "action-finalizer cloak attention", error))
				return false;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer expired trailing blank", error))
				return false;
		}
		else {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer cloak first blank", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "action-finalizer cloak second blank", error))
				return false;
		}
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error))
		return false;
	qb_str_single(number, sizeof(number), session->player.turns);
	snprintf(row, sizeof(row), "One Turn Deducted,%s left.", number);
	if (session->player.turns < 51.0f) {
		session_set_foreground(session, 3.0f);
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
	}
	if (!session_present_paged_fragment(session, (const uint8_t *)row, strlen(row)))
		return false;
	if (!random_value(session, &draw, error))
		return false;
	if (draw > xannor_threshold) {
		session_load_xannor_provoker(session, &xannor_provoker);
		if (!launch_xannor_retaliation(session, &xannor_provoker,
		    error))
			return false;
		if (session_is_destroyed(session))
			return false;
		if (!session_reload_player(session, error))
			return false;
	}
	return true;
}

static bool
random_value(void *context, float *value,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_random_next(&session->door->game.random, value, error);
}

static bool
emergency_warp(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t wormhole[] =
	    "You enter a wormhole as your engines build up to emergency power!";
	static const uint8_t temperature[] = "     * Engine Temperature *";
	static const uint8_t scale[] = "[ Normal ][ Danger ][ Overheat ]";
	static const uint8_t ruler[] = "================================";
	static const uint8_t gauge_open[] = "[";
	static const uint8_t gauge_tick[] = "*";
	static const uint8_t relief[] =
	    "You sigh in relief as you look at your scanner and find yourself in";
	static const uint8_t engines_disabled[] = "Your engines are disabled!";
	static const uint8_t repair[] =
	    "It will take a solar day to repair them.";
	float first;
	float second;
	float duration;
	float heat = 0.0f;
	float counter = 1.0f;
	float destination;
	float override;
	float turn_draw;
	float cost;
	uint8_t row[256];
	size_t row_length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp leading blank", error)
	    || !session_attention(session, " * EMERGENCY WARP ENGAGED! * ",
	    "emergency warp attention", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-title blank", error)
	    || !session_present_text(session, wormhole, sizeof(wormhole) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp wormhole row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp pre-temperature blank", error))
		return false;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, temperature,
	    sizeof(temperature) - 1U, SESSION_PRESENT_BOLD_LINE,
	    "emergency warp temperature title", error)
	    || !session_present_text(session, scale, sizeof(scale) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature scale", error))
		return false;
	session_set_foreground(session, 2.0f);
	if (!session_present_text(session, ruler, sizeof(ruler) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature ruler", error))
		return false;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, gauge_open,
	    sizeof(gauge_open) - 1U, SESSION_PRESENT_BOLD_RAW,
	    "emergency warp gauge open", error)
	    || !random_value(session, &first, error)
	    || !random_value(session, &second, error))
		return false;
	duration = yt_emergency_warp_duration(first, second);
	for (;;) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		if (draw > 0.75f)
			heat = single_add(heat, 1.0f);
		if (heat < 10.0f) {
			session_set_foreground(session, 2.0f);
		}
		else if (heat < 20.0f) {
			session_set_foreground(session, 3.0f);
		}
		else {
			session_set_foreground(session, 1.0f);
			yt_present_set_blink(&session->presentation, 1.0f);
		}
		if (!session_present_text(session, gauge_tick,
		    sizeof(gauge_tick) - 1U, SESSION_PRESENT_BOLD_RAW,
		    "emergency warp gauge tick", error))
			return false;
		if (!session_timed_wait(session, 0.33000001311302185)) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				snprintf(error->operation, sizeof(error->operation),
				    "emergency-warp heat wait");
			}
			return false;
		}
		if (heat >= 31.0f)
			break;
		counter = single_add(counter, 1.0f);
		if (counter > duration)
			break;
	}
	session_set_foreground(session, 2.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank one", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank two", error)
	    || !session_reload_player(session, error))
		return false;
	if (!random_value(session, &first, error)
	    || !random_value(session, &override, error)
	    || !random_value(session, &turn_draw, error))
		return false;
	destination = yt_emergency_warp_destination(first,
	    (float)session_sector_count(session));
	if (override > 0.949999988079071f)
		destination = session->door->game.config.headquarters;
	cost = yt_emergency_warp_cost(heat, turn_draw, session->player.turns,
	    heat >= 31.0f);
	if (heat >= 31.0f) {
		if (!session_attention(session, "MELT DOWN!",
		    "meltdown attention", error))
			return false;
		session_set_foreground(session, 1.0f);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown leading blank", error)
		    || !session_present_text(session, engines_disabled,
		    sizeof(engines_disabled) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "meltdown engines-disabled row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown middle blank", error)
		    || !session_present_text(session, repair, sizeof(repair) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "meltdown repair row", error))
			return false;
		for (int ordinal = 0; ordinal < 5; ++ordinal) {
			if (!session_sound(session, 5.0f,
			    "meltdown sound", error))
				return false;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "meltdown trailing blank", error)
		    || !yt_emergency_warp_stranded_row(destination, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "meltdown stranded row", error))
			return false;
	}
	else {
		if (!session_sound(session, 1.0f,
		    "emergency warp completion sound", error))
			return false;
		if (!session_present_text(session, relief, sizeof(relief) - 1U,
		    SESSION_PRESENT_LINE, "emergency warp relief row", error)
		    || !yt_emergency_warp_result_row(destination, cost, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "emergency warp result row", error))
			return false;
	}
	yt_emergency_warp_player_overlay(&session->player, destination, cost);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session_set_player_cache_raw(session, session_record(session),
	    YT_PLAYER_CACHE_SECTOR,
	    session->player.record.bytes + YT_F57);
	return true;
}

static bool
direct_emergency_warp(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t warning_one[] =
	    "This is a desperate move! Your engines will be drained and will take time";
	static const uint8_t warning_two[] =
	    "to recharge! You also risk a melt down! Are you sure you wish to do this?";
	static const uint8_t prompt[] = "[y/N] -=> ";
	enum yt_yes_no_answer answer;
	bool denied;

	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp leading blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	session_set_foreground(session, 7.0f);
	if (!session_present_paged_fragment(session, warning_one, sizeof(warning_one) - 1U))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_present_paged_fragment(session, warning_two, sizeof(warning_two) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp confirmation blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_confirm(session, prompt, sizeof(prompt) - 1U, &answer, error))
		return false;
	if (answer == YT_YES_NO_YES)
		return emergency_warp(session, error);
	return true;
}

static bool
movement_turn_gate(void *context, int player_record, struct yt_player *player,
    bool *denied, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session) || player == NULL
	    || !fresh_no_turn_gate(session, denied, error))
		return false;
	*player = session->player;
	return true;
}

static bool
movement_present(void *context, const uint8_t *text, size_t length,
    enum yt_movement_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_MOVEMENT_WARP_ROW:
		return session_present_paged_line(session, text, length, "movement warp row",
		    error);
	case YT_MOVEMENT_POST_WARP_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "movement post-warp blank", error);
	case YT_MOVEMENT_DESTINATION_PROMPT:
		return session_present_timed_paged_row(session, text, length,
		    "movement destination prompt", error);
	case YT_MOVEMENT_SAME_SECTOR:
		return session_present_alert(session, text, length,
		    "movement same-sector row", error);
	case YT_MOVEMENT_NOT_ADJACENT:
		return session_present_alert(session, text, length,
		    "movement not-adjacent row", error);
	case YT_MOVEMENT_ACCEPTED_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "movement accepted blank", error);
	case YT_MOVEMENT_CONFIRMATION_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "danger confirmation blank", error);
	default:
		return false;
	}
}

static bool
movement_input(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	(void)error;
	return session_read_number_command(context, response, capacity);
}

static bool
movement_danger(void *context, float target, bool *dangerous,
    struct yt_error *error)
{
	return dangerous_destination(context, target, dangerous, error);
}

static void
movement_clear_queue(void *context)
{
	clear_queue(context);
}

static bool
movement_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (accepted == NULL
	    || !session_confirm(context, prompt, length, &answer, error))
		return false;
	*accepted = answer == YT_YES_NO_YES;
	return true;
}

static bool
movement_finalize(void *context, struct yt_error *error)
{
	return finalize_action(context, 1.0f, error);
}

static void
movement_clear_self_mines(void *context)
{
	struct yt_session *session = context;

	session_set_self_mine_suppression(session, false);
}

static bool
movement_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
movement_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session))
		return false;
	session->player = *player;
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &session->player.record, error);
}

static bool
movement_flush_player(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_flush(&session->door->game.database, error);
}

static bool
movement_update_cache(void *context, int player_record, const uint8_t raw[4],
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	if (player_record < 0
	    || (size_t)player_record >= (YT_PLAYER_LAST + 1U))
		return false;
	session_set_player_cache_raw(session, player_record,
	    YT_PLAYER_CACHE_SECTOR, raw);
	return true;
}

static bool
command_move(struct yt_session *session, bool *moved,
    struct yt_error *error)
{
	static const struct yt_movement_ops ops = {
		movement_turn_gate,
		movement_present,
		movement_input,
		movement_danger,
		movement_clear_queue,
		movement_confirm,
		movement_finalize,
		movement_clear_self_mines,
		movement_hydrate,
		movement_write_player,
		movement_flush_player,
		movement_update_cache,
	};
	struct yt_movement_state state;

	if (moved == NULL)
		return false;
	*moved = false;
	state = (struct yt_movement_state){
		.current_player_record = session_record(session),
		.port_offset = session_port_offset(session),
		.sector_offset = session_sector_offset(session),
	};
	session_current_warps(session, state.warps);
	if (!yt_movement_run(&state, &ops, session, error))
		return false;
	*moved = state.route == YT_MOVEMENT_MOVED;
	return true;
}

static bool
session_load_team_cache(struct yt_session *session, int team_id,
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

static bool
team_remove_player(struct yt_session *session, int victim,
    struct yt_error *error)
{
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	struct yt_player player;
	struct yt_record overlay;
	int team_id;
	float expression;
	uint32_t physical_record;
	size_t index;

	if (!yt_game_read_player(&session->door->game, victim, &player, error))
		return false;
	team_id = (int)player.team;
	if (team_id == 0)
		return true;

	if (!session_load_team_cache(session, team_id, session_record(session),
	    NULL, NULL, NULL, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->team_cache.roster);
	    ++index) {
		if (session->team_cache.roster[index] == victim)
			session->team_cache.roster[index] = 0;
	}

	expression = single_add(session_sector_offset(session), (float)team_id);
	physical_record = qb_brun_random_record_number(expression);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &overlay, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(roster_offsets); ++index)
		(void)yt_record_set_number(&overlay, roster_offsets[index],
		    (float)session->team_cache.roster[index]);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &overlay, error)
	    || !yt_game_read_player(&session->door->game, victim, &player,
	    error))
		return false;
	player.team = 0.0f;
	(void)yt_record_set_number(&player.record, YT_F89, 0.0f);
	return yt_game_write_player(&session->door->game, victim, &player,
	    error);
}

static bool
player_death_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
player_death_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_player(&session->door->game, player_record, player,
	    error);
}

static bool
player_death_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector, sector,
	    error);
}

static bool
player_death_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_write_sector(session, logical_sector, sector,
	    error);
}

static bool
player_death_remove_team(void *context, int victim_record,
    struct yt_error *error)
{
	return team_remove_player(context, victim_record, error);
}

static bool
player_death_read_port(void *context, int logical_port, struct yt_port *port,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_port(session, logical_port, port, error);
}

static bool
player_death_write_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_write_port(session, logical_port, port,
	    error);
}

static bool
player_death_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "death title row", error);
}

static void
player_death_clear_active_cache(void *context, int victim_record,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	session_set_player_cache_raw(session, victim_record,
	    YT_PLAYER_CACHE_SECTOR, raw);
}

static void
player_death_set_current(void *context, const struct yt_player *player)
{
	struct yt_session *session = context;
	char cached_name[sizeof(session->player.name)];

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	session->player = *player;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
}

static bool
player_death_flush(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_flush(&session->door->game.database, error);
}

static bool
kill_player_run(struct yt_session *session, int victim_record,
    float killer, bool wait_for_current, struct yt_error *error)
{
	static const struct yt_player_death_ops ops = {
		player_death_clear_active_cache,
		player_death_read_player,
		player_death_write_player,
		player_death_read_sector,
		player_death_write_sector,
		player_death_remove_team,
		player_death_read_port,
		player_death_write_port,
		player_death_present,
		session_append_news_bytes,
		player_death_set_current,
		player_death_flush,
	};
	struct yt_player_death_state state = {
		.victim_record = victim_record,
		.current_player_record = session_record(session),
		.killer = killer,
		.sector_count = session_sector_count(session),
		.port_count = port_count(session),
		.last_player_record = session_sector_offset(session),
		.current_name = (const uint8_t *)session->player.name,
		.current_name_length = strlen(session->player.name),
	};

	if (!yt_player_death_run(&state, &ops, session, error))
		return false;
	if (victim_record == session_record(session) && wait_for_current) {
		if (!session_wait(session, 5.0, "common fatal wait", error))
			return false;
		session->fatal_wait_complete = true;
	}
	return true;
}

static bool
kill_player(struct yt_session *session, int victim_record,
    float killer, struct yt_error *error)
{
	return kill_player_run(session, victim_record, killer, true, error);
}

static void
common_fatal_set_foreground(void *context, float foreground,
    int pager_foreground)
{
	struct yt_session *session = context;

	(void)pager_foreground;
	session_set_foreground(session, foreground);
}

static bool
common_fatal_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_alert(context, text, length, "common fatal notice", error);
}

static bool
common_fatal_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	char cached_name[sizeof(session->player.name)];

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
	*player = session->player;
	return true;
}

static bool
common_fatal_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "fatal destruction sound",
	    error);
}

static bool
common_fatal_death(void *context, int victim_record, float killer,
    struct yt_error *error)
{
	return kill_player_run(context, victim_record, killer, false, error);
}

static bool
common_fatal_wait(void *context, float duration, struct yt_error *error)
{
	struct yt_session *session = context;

	if (!session_wait(session, duration, "common fatal wait", error))
		return false;
	session->fatal_wait_complete = true;
	return true;
}

static bool
common_fatal_self(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_common_fatal_ops ops = {
		common_fatal_set_foreground,
		common_fatal_present,
		common_fatal_read_player,
		common_fatal_sound,
		common_fatal_death,
		common_fatal_wait,
	};
	struct yt_common_fatal_state state = {
		.current_player_record = session_record(session),
		.foreground = session_foreground(session),
		.pager_foreground = session_pager_foreground(session),
	};

	return yt_common_fatal_run(&state, &ops, session, error);
}

static bool
xannor_victory_play_file(void *context, const char *path,
    struct yt_error *error)
{
	return xannor_victory_file(context, path, error);
}

static bool
xannor_victory_present(void *context, const uint8_t *text, size_t length,
    enum yt_xannor_victory_output_kind kind, const char *operation,
    struct yt_error *error)
{
	enum session_present_text_kind session_kind;

	switch (kind) {
	case YT_XANNOR_VICTORY_RAW:
		session_kind = SESSION_PRESENT_RAW;
		break;
	case YT_XANNOR_VICTORY_LINE:
		session_kind = SESSION_PRESENT_LINE;
		break;
	case YT_XANNOR_VICTORY_BOLD_LINE:
		session_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	default:
		return false;
	}
	return session_present_text(context, text, length, session_kind,
	    operation, error);
}

static bool
xannor_victory_wait(void *context, double seconds, const char *operation,
    struct yt_error *error)
{
	return seconds == 99.0
	    && session_wait(context, 99.0, operation, error);
}

static void
xannor_victory_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session_set_foreground(session, foreground);
}

static void
xannor_victory_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	yt_present_set_blink(&session->presentation, blink);
}

static void
xannor_victory_clear_queue(void *context)
{
	clear_queue(context);
}

static bool
xannor_victory_sound(void *context, float selector, const char *operation,
    struct yt_error *error)
{
	return session_sound(context, selector, operation, error);
}

static bool
xannor_victory_radio(void *context, const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error)
{
	(void)context;
	return radio_append_bytes(text, length, sender, recipient, error);
}

static bool
xannor_victory_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector, sector,
	    error);
}

static bool
xannor_victory_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_write_sector(session, logical_sector,
	    sector, error);
}

static bool
xannor_victory(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_xannor_victory_ops ops = {
		xannor_victory_play_file,
		xannor_victory_present,
		xannor_victory_wait,
		xannor_victory_set_foreground,
		xannor_victory_set_blink,
		xannor_victory_clear_queue,
		apply_player_credit_mutation,
		xannor_victory_sound,
		session_append_news_bytes,
		xannor_victory_radio,
		xannor_victory_read_sector,
		xannor_victory_write_sector,
	};
	struct yt_xannor_victory_state state = {
		.current_player = (float)session_record(session),
		.foreground = session_foreground(session),
		.pager_foreground = (float)session_pager_foreground(session),
		.blink = yt_present_blink(&session->presentation),
	};

	return yt_xannor_victory_run(&state, &ops, session, error);
}

static bool
direct_fighter_kill_sound(void *context, struct yt_error *error)
{
	return session_sound(context, 3.0f, "player kill sound", error);
}

static bool
direct_fighter_kill_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
direct_fighter_kill_name_length(void *context, float raw_length,
    size_t *length, struct yt_error *error)
{
	return port_report_length(context, raw_length, YT_TEXT_FIELD_SIZE,
	    length, "direct fighter victim name length", error);
}

static bool
direct_fighter_kill_death(void *context, int victim_record, float killer,
    struct yt_error *error)
{
	return kill_player(context, victim_record, killer, error);
}

static bool
direct_fighter_kill_salvage(void *context, int victim_record, int killer,
    struct yt_error *error)
{
	return yt_session_salvage_player(context, victim_record, killer, error);
}

static bool
direct_fighter_kill_sector_number(struct yt_session *session, float raw,
    int *logical, struct yt_error *error)
{
	bool overflow;

	*logical = (int)qb_cint_mode((double)raw,
	    session->presentation.sound.conversion_mode, &overflow);
	if (!overflow)
		return true;
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "direct fighter sector record CINT");
	}
	return false;
}

static bool
direct_fighter_kill_read_sector(void *context, float raw_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	int logical_sector;

	if (!direct_fighter_kill_sector_number(session, raw_sector,
	    &logical_sector, error))
		return false;
	return session_read_sector(session, logical_sector, sector,
	    error);
}

static bool
direct_fighter_kill_write_sector(void *context, float raw_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	int logical_sector;

	if (!direct_fighter_kill_sector_number(session, raw_sector,
	    &logical_sector, error))
		return false;
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &sector->record, error);
}

static bool
direct_fighter_kill_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return session_present_alert(context, text, length,
	    "direct fighter mine warning", error);
}

static bool
direct_fighter_kill_mine(void *context, bool *terminal,
    bool *destroyed, struct yt_error *error)
{
	struct yt_session *session = context;

	if (!mine_encounter(session, terminal, error))
		return false;
	*destroyed = session->destroyed;
	return true;
}

static bool
direct_fighter_kill_fatal(void *context, struct yt_error *error)
{
	return common_fatal_self(context, error);
}

static bool
direct_attack_combat_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session))
		return yt_game_read_player(&session->door->game, player_record,
		    player, error);
	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
direct_attack_combat_write(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record == session_record(session))
		session->player = *player;
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error);
}

static bool
direct_attack_combat_present(void *context, const uint8_t *text,
    size_t length, enum yt_direct_attack_combat_output_kind kind,
    struct yt_error *error)
{
	switch (kind) {
	case YT_DIRECT_ATTACK_COMBAT_TOO_MANY_ROW:
		return session_present_alert(context, text, length,
		    "direct Attack too-many row", error);
	case YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW:
		return session_present_paged_line(context, text, length,
		    "direct Attack attacker result", error);
	case YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW:
		return session_present_paged_fragment(context, text, length);
	case YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW:
		return session_present_paged_line(context, text, length,
		    "direct Attack eliminated row", error);
	default:
		return false;
	}
}

static bool
direct_attack_combat_sound(void *context, float selector,
    struct yt_error *error)
{
	return session_sound(context, selector, "player attack opening sound",
	    error);
}

static bool
direct_attack_combat_radio(void *context, const uint8_t *text,
    size_t length, float recipient, struct yt_error *error)
{
	(void)context;
	return radio_append_bytes(text, length, -2.0f, recipient, error);
}

static bool
direct_attack_combat_spill(void *context, double *fighters,
    float *shields, struct yt_error *error)
{
	return fighter_shield_spill(context, fighters, shields, false, error);
}

static bool
direct_attack_combat_kill(void *context, int target_record,
    int current_player_record, float current_sector, float target_shields,
    struct yt_error *error)
{
	static const struct yt_direct_fighter_kill_ops ops = {
		direct_fighter_kill_sound,
		direct_fighter_kill_read_player,
		direct_fighter_kill_name_length,
		direct_fighter_kill_death,
		direct_fighter_kill_salvage,
		direct_fighter_kill_read_sector,
		direct_fighter_kill_write_sector,
		direct_fighter_kill_present,
		session_append_news_bytes,
		direct_fighter_kill_mine,
		direct_fighter_kill_fatal,
	};
	struct yt_direct_fighter_kill_state state = {
		.target_shields = target_shields,
		.target_record = target_record,
		.current_player_record = current_player_record,
		.current_sector = current_sector,
	};

	return yt_direct_fighter_kill_run(&state, &ops, context, error);
}

static bool
attack_player(struct yt_session *session, int target_record,
    double committed, struct yt_error *error)
{
	static const struct yt_direct_attack_combat_ops ops = {
		direct_attack_combat_read,
		direct_attack_combat_write,
		direct_attack_combat_present,
		direct_attack_combat_sound,
		direct_attack_combat_radio,
		random_value,
		direct_attack_combat_spill,
		direct_attack_combat_kill,
	};
	struct yt_direct_attack_combat_state state = {
		.current_player_record = session_record(session),
		.target_record = target_record,
		.committed = committed,
	};

	return yt_direct_attack_combat_run(&state, &ops, session, error);
}

static bool
direct_attack_present(void *context, const uint8_t *text, size_t length,
    enum yt_direct_attack_output_kind kind, struct yt_error *error)
{
	switch (kind) {
	case YT_DIRECT_ATTACK_TITLE_ROW:
	case YT_DIRECT_ATTACK_TEAM_ROW:
	case YT_DIRECT_ATTACK_NONE_SELECTED_ROW:
		return session_present_paged_fragment(context, text, length);
	case YT_DIRECT_ATTACK_NO_FIGHTERS_ROW:
		return session_present_alert(context, text, length,
		    "direct Attack no-fighters row", error);
	case YT_DIRECT_ATTACK_COMMITMENT_PROMPT:
		return session_present_timed_paged_row(context, text, length,
		    "direct Attack commitment prompt", error);
	case YT_DIRECT_ATTACK_NONE_VISIBLE_ROW:
		return session_present_alert(context, text, length,
		    "direct Attack no-visible-target row", error);
	default:
		return false;
	}
}

static bool
direct_attack_confirm(void *context, const uint8_t *prompt, size_t length,
    enum yt_direct_attack_confirmation *answer, struct yt_error *error)
{
	enum yt_yes_no_answer selected;

	if (!session_confirm(context, prompt, length, &selected, error))
		return false;
	switch (selected) {
	case YT_YES_NO_NO:
		*answer = YT_DIRECT_ATTACK_CONFIRM_NO;
		return true;
	case YT_YES_NO_YES:
		*answer = YT_DIRECT_ATTACK_CONFIRM_YES;
		return true;
	case YT_YES_NO_EMPTY:
		*answer = YT_DIRECT_ATTACK_CONFIRM_EMPTY;
		return true;
	default:
		return false;
	}
}

static bool
direct_attack_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	(void)error;
	return session_read_number_command(context, response, capacity);
}

static void
direct_attack_store_target(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->shared_target_record = qb_mbf32_decode(raw);
}

static bool
direct_attack_combat(void *context, int target_record, double committed,
    struct yt_error *error)
{
	return attack_player(context, target_record, committed, error);
}

static bool
command_attack_player(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const struct yt_direct_attack_ops ops = {
		direct_attack_combat_read,
		direct_attack_store_target,
		direct_attack_present,
		direct_attack_confirm,
		direct_attack_amount,
		direct_attack_combat,
	};
	struct yt_direct_attack_state state = {
		.current_player_record = session_record(session),
		.last_player_record = session_sector_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.player_cache = &session->player_cache,
	};

	if (enter_sector == NULL)
		return false;
	*enter_sector = false;
	if (!yt_direct_attack_run(&state, &ops, session, error))
		return false;
	*enter_sector = state.enter_sector;
	return true;
}

static bool
fighter_shield_spill_present(void *context, const uint8_t *text,
    size_t length, enum yt_fighter_shield_spill_output_kind kind,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    kind == YT_FIGHTER_SHIELD_SPILL_FIGHTER_ROW
	    ? "fighter spill result" : "shield spill result", error);
}

static void
fighter_shield_spill_store(void *context,
    enum yt_fighter_shield_spill_store_kind kind, double fighters,
    float shields)
{
	struct yt_session *session = context;

	if (kind == YT_FIGHTER_SHIELD_SPILL_STORE_FIGHTERS)
		session->hostile_deployed_fighters = fighters;
	else
		session->combat_ship_shields = shields;
}

static bool
fighter_shield_spill(struct yt_session *session, double *fighters,
    float *shields, bool bind_hostile_cells, struct yt_error *error)
{
	const struct yt_fighter_shield_spill_ops ops = {
		random_value,
		fighter_shield_spill_present,
		bind_hostile_cells ? fighter_shield_spill_store : NULL,
	};
	struct yt_fighter_shield_spill_state state = {
		.fighters = *fighters,
		.shields = *shields,
	};
	bool result = yt_fighter_shield_spill_run(&state, &ops, session,
	    error);

	*fighters = state.fighters;
	*shields = state.shields;
	return result;
}

static bool
hostile_surrender_read(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	return direct_attack_combat_read(context, player_record, player, error);
}

static bool
hostile_surrender_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_surrender_output_kind kind, struct yt_error *error)
{
	switch (kind) {
	case YT_HOSTILE_SURRENDER_RADIO_ROW:
		return session_present_paged_line(context, text, length,
		    "surrender radio row", error);
	case YT_HOSTILE_SURRENDER_CAPTAIN_ROW:
		return session_present_paged_line(context, text, length,
		    "surrender captain row", error);
	case YT_HOSTILE_SURRENDER_WISH_ROW:
		return session_present_alert(context, text, length,
		    "surrender wish row", error);
	case YT_HOSTILE_SURRENDER_PROMPT_BLANK:
		return session_present_text(context, NULL, 0,
		    SESSION_PRESENT_LINE, "surrender prompt blank", error);
	case YT_HOSTILE_SURRENDER_JOINED_ROW:
		return session_present_paged_line(context, text, length,
		    "surrender joined row", error);
	case YT_HOSTILE_SURRENDER_COUNT_ROW:
		return session_present_paged_fragment(context, text, length);
	case YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW:
		return session_present_paged_fragment(context, text, length);
	case YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW:
		return session_present_paged_fragment(context, text, length);
	default:
		return false;
	}
}

static bool
hostile_surrender_sound(void *context,
    enum yt_hostile_surrender_sound_kind kind, float selector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)kind;
	return session_sound(session, selector, "hostile surrender sound", error);
}

static bool
hostile_surrender_prompt(void *context, const uint8_t *prompt, size_t length,
    enum yt_hostile_surrender_answer *answer, struct yt_error *error)
{
	enum yt_yes_no_answer selected;

	if (!session_confirm(context, prompt, length, &selected, error))
		return false;
	switch (selected) {
	case YT_YES_NO_NO:
		*answer = YT_HOSTILE_SURRENDER_ANSWER_NO;
		return true;
	case YT_YES_NO_YES:
		*answer = YT_HOSTILE_SURRENDER_ANSWER_YES;
		return true;
	case YT_YES_NO_EMPTY:
		*answer = YT_HOSTILE_SURRENDER_ANSWER_EMPTY;
		return true;
	default:
		return false;
	}
}

static void
hostile_surrender_cache_forces(void *context, double ship_fighters,
    double deployed_fighters)
{
	struct yt_session *session = context;

	session->combat_ship_fighters = ship_fighters;
	session->hostile_deployed_fighters = deployed_fighters;
}

static void
hostile_surrender_mark_checked(void *context)
{
	struct yt_session *session = context;

	session->shared_status = 1.0f;
}

static bool
hostile_attack_persistence_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	return direct_attack_combat_read(context, player_record, player, error);
}

static bool
hostile_attack_persistence_write_player(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	return direct_attack_combat_write(context, player_record, player, error);
}

static bool
hostile_attack_persistence_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
hostile_attack_persistence_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)sector_number),
	    &sector->record, error);
}

static bool
hostile_attack_persistence_blank(void *context, struct yt_error *error)
{
	return session_present_text(context, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack post-persist blank", error);
}

static bool
hostile_attack_persistence_fatal(void *context, struct yt_error *error)
{
	return common_fatal_self(context, error);
}

struct hostile_attack_tail_context {
	struct yt_session *session;
	const char *cached_player_name;
};

static bool
hostile_attack_tail_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	if (!direct_attack_combat_read(tail->session, player_record, player,
	    error))
		return false;
	(void)snprintf(tail->session->player.name,
	    sizeof(tail->session->player.name), "%s", tail->cached_player_name);
	(void)snprintf(player->name, sizeof(player->name), "%s",
	    tail->cached_player_name);
	return true;
}

static bool
hostile_attack_tail_write_player(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return direct_attack_combat_write(tail->session, player_record, player,
	    error);
}

static bool
hostile_attack_tail_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_attack_tail_output_kind kind, struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;
	struct yt_session *session = tail->session;

	if (kind == YT_HOSTILE_ATTACK_TAIL_REWARD_ROW)
		yt_present_set_bold(&session->presentation, 1.0f);
	else if (kind != YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW)
		return false;
	(void)error;
	return session_present_paged_fragment(session, text, length);
}

static bool
hostile_attack_tail_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return session_append_news_bytes(tail->session, text, length, error);
}

static bool
hostile_attack_tail_clearance(void *context, struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return clearance(tail->session, true, error);
}

static bool
hostile_attack_tail_random(void *context, float *value,
    struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return random_value(tail->session, value, error);
}

static bool
hostile_attack_tail_victory(void *context, struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return xannor_victory(tail->session, error);
}

struct hostile_attack_combat_context {
	struct yt_session *session;
	struct yt_sector *sector;
	const char *cached_player_name;
};

static bool
hostile_attack_combat_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	return session_read_sector(combat->session, sector_number,
	    sector, error);
}

static bool
hostile_attack_combat_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	if (!direct_attack_combat_read(combat->session, player_record, player,
	    error))
		return false;
	(void)snprintf(combat->session->player.name,
	    sizeof(combat->session->player.name), "%s",
	    combat->cached_player_name);
	(void)snprintf(player->name, sizeof(player->name), "%s",
	    combat->cached_player_name);
	player->fighters = (float)combat->session->combat_ship_fighters;
	player->cloak = combat->session->player.cloak;
	player->shields = combat->session->combat_ship_shields;
	return true;
}

static bool
hostile_attack_combat_sound(void *context, float selector,
    struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	return session_sound(combat->session, selector,
	    "deployed attack opening sound", error);
}

static bool
hostile_attack_combat_random(void *context, float *value,
    struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	return random_value(combat->session, value, error);
}

static void
hostile_attack_combat_store_ship(void *context, double ship_fighters)
{
	struct hostile_attack_combat_context *combat = context;

	combat->session->combat_ship_fighters = ship_fighters;
}

static bool
hostile_attack_combat_surrender(void *context,
    struct yt_hostile_surrender_state *state, struct yt_error *error)
{
	static const struct yt_hostile_surrender_ops ops = {
		hostile_surrender_read,
		hostile_surrender_present,
		hostile_surrender_sound,
		hostile_surrender_prompt,
		session_append_news_bytes,
		hostile_surrender_cache_forces,
		hostile_surrender_mark_checked,
	};
	struct hostile_attack_combat_context *combat = context;
	bool result = yt_hostile_attack_surrender_run(state, &ops,
	    combat->session, error);

	combat->session->player = state->current;
	(void)snprintf(combat->session->player.name,
	    sizeof(combat->session->player.name), "%s",
	    combat->cached_player_name);
	return result;
}

static bool
hostile_attack_combat_present(void *context, const uint8_t *text,
    size_t length, enum yt_hostile_attack_combat_output_kind kind,
    struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;
	struct yt_session *session = combat->session;

	switch (kind) {
	case YT_HOSTILE_ATTACK_COMBAT_RESULT_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "deployed attack result blank", error);
	case YT_HOSTILE_ATTACK_COMBAT_LOSS_ROW:
		return session_present_paged_fragment(session, text, length);
	case YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW:
		return session_present_paged_fragment(session, text, length);
	case YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW:
		return session_present_alert(session, text, length,
		    "deployed attack ship exposed", error);
	case YT_HOSTILE_ATTACK_COMBAT_SPILL_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "shield spill leading blank", error);
	default:
		return false;
	}
}

static void
hostile_attack_combat_cache_player(void *context,
    const struct yt_player *player)
{
	struct hostile_attack_combat_context *combat = context;

	combat->session->player = *player;
	(void)snprintf(combat->session->player.name,
	    sizeof(combat->session->player.name), "%s",
	    combat->cached_player_name);
}

static void
hostile_attack_combat_cache_sector(void *context,
    const struct yt_sector *sector, double deployed_fighters)
{
	struct hostile_attack_combat_context *combat = context;

	*combat->sector = *sector;
	combat->session->hostile_deployed_fighters = deployed_fighters;
}

static bool
hostile_attack_combat_spill(void *context, double *fighters,
    float *shields, struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	return fighter_shield_spill(combat->session, fighters, shields, true,
	    error);
}

static bool
hostile_attack_combat_persistence(void *context,
    struct yt_hostile_attack_persistence_state *state,
    struct yt_error *error)
{
	static const struct yt_hostile_attack_persistence_ops ops = {
		hostile_attack_persistence_read_player,
		hostile_attack_persistence_write_player,
		hostile_attack_persistence_read_sector,
		hostile_attack_persistence_write_sector,
		hostile_attack_persistence_blank,
		session_append_news_bytes,
		hostile_attack_persistence_fatal,
	};
	struct hostile_attack_combat_context *combat = context;
	bool result = yt_hostile_attack_persistence_run(state, &ops,
	    combat->session, error);

	if (state->route != YT_HOSTILE_ATTACK_PERSISTENCE_FATAL) {
		combat->session->player = state->current;
		(void)snprintf(combat->session->player.name,
		    sizeof(combat->session->player.name), "%s",
		    combat->cached_player_name);
	}
	if (state->sector_written)
		*combat->sector = state->sector;
	if (state->mercenaries_hurt)
		combat->session->mercenaries_hurt = true;
	return result;
}

static bool
hostile_attack_combat_tail(void *context,
    struct yt_hostile_attack_tail_state *state, struct yt_error *error)
{
	static const struct yt_hostile_attack_tail_ops ops = {
		hostile_attack_tail_read_player,
		hostile_attack_tail_write_player,
		hostile_attack_tail_present,
		hostile_attack_tail_news,
		hostile_attack_tail_clearance,
		hostile_attack_tail_random,
		hostile_attack_tail_victory,
	};
	struct hostile_attack_combat_context *combat = context;
	struct hostile_attack_tail_context tail = {
		combat->session,
		combat->cached_player_name,
	};

	return yt_hostile_attack_tail_run(state, &ops, &tail, error);
}

static bool
attack_deployed_committed(struct yt_session *session,
    struct yt_sector *sector, double commitment, bool allow_surrender,
    struct yt_error *error)
{
	static const struct yt_hostile_attack_combat_ops ops = {
		hostile_attack_combat_read_sector,
		hostile_attack_combat_read_player,
		hostile_attack_combat_sound,
		hostile_attack_combat_random,
		hostile_attack_combat_store_ship,
		hostile_attack_combat_surrender,
		hostile_attack_combat_present,
		hostile_attack_combat_cache_player,
		hostile_attack_combat_cache_sector,
		hostile_attack_combat_spill,
		hostile_attack_combat_persistence,
		hostile_attack_combat_tail,
	};
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	size_t cached_player_name_length;
	char cached_player_name_text[sizeof(session->player.name)];
	struct hostile_attack_combat_context context;
	struct yt_hostile_attack_combat_state state;
	bool result;

	if (!yt_player_stored_name(&session->player, cached_player_name,
	    &cached_player_name_length, error))
		return false;
	(void)snprintf(cached_player_name_text,
	    sizeof(cached_player_name_text), "%s", session->player.name);
	context = (struct hostile_attack_combat_context){
		session,
		sector,
		cached_player_name_text,
	};
	state = (struct yt_hostile_attack_combat_state){
		.current_player_record = session_record(session),
		.current_sector = (int)session->player.sector,
		.commitment = commitment,
		.allow_surrender = allow_surrender,
		.cached_defenders = session_hostile_deployed_fighters(session),
		.sector = *sector,
		.cached_player_name = cached_player_name,
		.cached_player_name_length = cached_player_name_length,
		.real_first_name =
		    (const uint8_t *)session->door->identity.real_first,
		.real_first_name_length =
		    strlen(session->door->identity.real_first),
		.owner_label = session->hostile_owner_label,
		.owner_label_length = session->hostile_owner_label_length,
		.turns_per_day = session->door->game.config.turns_per_day,
		.headquarters = session->door->game.config.headquarters,
	};
	result = yt_hostile_attack_combat_run(&state, &ops, &context, error);
	*sector = state.sector;
	return result;
}

static bool
attack_deployed(struct yt_session *session, struct yt_sector *sector,
    struct yt_error *error)
{
	static const uint8_t heading[] = "<Attack>";
	static const uint8_t prompt[] = "Attack with how many fighters? ";
	static const uint8_t none[] = "You don't have any fighters!";
	char response[160];
	char available[64];
	char row[128];
	struct qb_val_result parsed;
	enum qb_mbf_status status;
	enum yt_hostile_attack_admission admission;
	double cached_ship_fighters;
	float commitment;
	uint8_t commitment_raw[4];

	if (!session_present_paged_fragment(session, heading, sizeof(heading) - 1U))
		return false;
	cached_ship_fighters = session->combat_ship_fighters;
	admission = yt_hostile_attack_admit((float)cached_ship_fighters, 0.0f);
	if (admission == YT_HOSTILE_ATTACK_NO_FIGHTERS)
		return session_present_alert(session, none, sizeof(none) - 1U,
		    "hostile Attack no fighters", error);
	if (!session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "hostile Attack amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0') {
		memset(&parsed, 0, sizeof(parsed));
		parsed.valid = true;
	}
	else
		parsed = qb_val(response);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "attack:VAL");
		}
		return false;
	}
	commitment = (float)parsed.value;
	status = qb_mbf32_encode(commitment, commitment_raw);
	if (status == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "attack:amount-csng");
		}
		return false;
	}
	commitment = qb_mbf32_decode(commitment_raw);
	admission = yt_hostile_attack_admit((float)cached_ship_fighters,
	    commitment);
	if (admission == YT_HOSTILE_ATTACK_TOO_MANY) {
		if (qb_str_double(available, sizeof(available),
		    cached_ship_fighters) < 0
		    || snprintf(row, sizeof(row), "You only have%s!", available) < 0)
			return false;
		return session_present_alert(session, (const uint8_t *)row, strlen(row),
		    "hostile Attack too many", error);
	}
	if (admission == YT_HOSTILE_ATTACK_LESS_THAN_ONE)
		return true;
	return attack_deployed_committed(session, sector,
	    (double)commitment, true, error);
}

static bool
hostile_bribe_accept_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return session_present_alert(context, text, length,
	    "accepted Mercenary Bribe", error);
}

static bool
hostile_bribe_accept_sound(void *context, float selector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return session_sound(session, selector, "accepted bribe sound", error);
}

static bool
hostile_bribe_accept_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
hostile_bribe_accept_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)sector_number),
	    &sector->record, error);
}

static bool
hostile_bribe_accept_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
hostile_bribe_accept_write_player(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error);
}

struct hostile_bribe_context {
	struct yt_session *session;
	struct yt_sector *sector;
};

static bool
hostile_bribe_present(void *context, const uint8_t *text, size_t length,
    enum yt_hostile_bribe_output_kind kind, struct yt_error *error)
{
	struct hostile_bribe_context *bribe = context;

	switch (kind) {
	case YT_HOSTILE_BRIBE_ORDINARY_REFUSAL_ROW:
		return session_present_alert(bribe->session, text, length,
		    "ordinary Bribe refusal", error);
	case YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW:
		return session_present_alert(bribe->session, text, length,
		    "Mercenary planet refusal", error);
	case YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW:
		return session_present_alert(bribe->session, text, length,
		    "Mercenary life demand", error);
	case YT_HOSTILE_BRIBE_INTRODUCTION_ROW:
		return session_present_paged_line(bribe->session, text, length,
		    "Mercenary Bribe introduction", error);
	case YT_HOSTILE_BRIBE_OFFER_PROMPT:
		return session_present_timed_paged_row(bribe->session, text, length,
		    "Mercenary Bribe offer prompt", error);
	case YT_HOSTILE_BRIBE_REJECTED_ROW:
		return session_present_alert(bribe->session, text, length,
		    "Mercenary rejected offer", error);
	default:
		return false;
	}
}

static bool
hostile_bribe_random(void *context, float *value, struct yt_error *error)
{
	struct hostile_bribe_context *bribe = context;

	return random_value(bribe->session, value, error);
}

static bool
hostile_bribe_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct hostile_bribe_context *bribe = context;

	(void)error;
	return session_read_number_command(bribe->session, response, capacity);
}

static bool
hostile_bribe_accept(void *context,
    struct yt_hostile_bribe_accept_state *state, struct yt_error *error)
{
	static const struct yt_hostile_bribe_accept_ops ops = {
		hostile_bribe_accept_present,
		hostile_bribe_accept_sound,
		hostile_bribe_accept_read_sector,
		hostile_bribe_accept_write_sector,
		hostile_bribe_accept_read_player,
		hostile_bribe_accept_write_player,
	};
	struct hostile_bribe_context *bribe = context;

	return yt_hostile_bribe_accept_run(state, &ops, bribe->session, error);
}

static bool
hostile_bribe_combat(void *context, double commitment,
    struct yt_error *error)
{
	struct hostile_bribe_context *bribe = context;

	return attack_deployed_committed(bribe->session, bribe->sector,
	    commitment, true, error);
}

static bool
hostile_bribe_fatal(void *context, struct yt_error *error)
{
	struct hostile_bribe_context *bribe = context;

	return common_fatal_self(bribe->session, error);
}

static bool
bribe_deployed(struct yt_session *session, struct yt_sector *sector,
    bool *direct_hostile_menu, bool *forced_attack,
    struct yt_error *error)
{
	static const struct yt_hostile_bribe_ops ops = {
		hostile_bribe_present,
		hostile_bribe_random,
		hostile_bribe_amount,
		hostile_bribe_accept,
		hostile_bribe_combat,
		hostile_bribe_fatal,
	};
	struct hostile_bribe_context context;
	struct yt_hostile_bribe_state state;
	bool result;

	if (direct_hostile_menu == NULL || forced_attack == NULL)
		return false;
	context = (struct hostile_bribe_context){session, sector};
	state = (struct yt_hostile_bribe_state){
		.current_player_record = session_record(session),
		.current_sector = (int)session->player.sector,
		.owner = session->hostile_owner,
		.cached_defenders = session_hostile_deployed_fighters(session),
		.ship_fighters = session->combat_ship_fighters,
		.shields = session->combat_ship_shields,
		.credits = session->player.credits,
		.planet_link = sector->planet,
		.mercenaries_hurt = session->mercenaries_hurt,
		.real_first_name =
		    (const uint8_t *)session->door->identity.real_first,
		.real_first_name_length =
		    strlen(session->door->identity.real_first),
	};
	result = yt_hostile_bribe_run(&state, &ops, &context, error);
	*direct_hostile_menu = state.direct_hostile_menu;
	*forced_attack = state.forced_attack;
	return result;
}

static bool
shrink_three(struct yt_session *session, float initial, float *result,
    struct yt_error *error)
{
	float range = initial;

	return yt_random_nested_single(&session->door->game.random, 3.0f,
	    &range, result, error);
}

static bool
mine_read_current(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
mine_read_player(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
mine_write_player(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
mine_read_sector(void *context, int logical_sector, struct yt_sector *sector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector, sector,
	    error);
}

static bool
mine_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)logical_sector),
	    &sector->record, error);
}

static bool
mine_present(void *context, const uint8_t *text, size_t length,
    enum yt_sector_mine_output_kind kind, struct yt_error *error)
{
	enum session_present_text_kind session_kind;

	switch (kind) {
	case YT_SECTOR_MINE_OUTPUT_LINE:
		session_kind = SESSION_PRESENT_LINE;
		break;
	case YT_SECTOR_MINE_OUTPUT_BOLD_LINE:
		session_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	case YT_SECTOR_MINE_OUTPUT_BOLD_RAW:
		session_kind = SESSION_PRESENT_BOLD_RAW;
		break;
	default:
		return false;
	}
	return session_present_text(context, text, length, session_kind,
	    "sector mine output", error);
}

static bool
mine_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "sector mine sound", error);
}

static bool
mine_shrink(void *context, float range, float *result,
    struct yt_error *error)
{
	return shrink_three(context, range, result, error);
}

static bool
mine_warp(void *context, struct yt_error *error)
{
	return emergency_warp(context, error);
}

static void
mine_set_current(void *context, const struct yt_player *player)
{
	struct yt_session *session = context;

	session->player = *player;
}

static void
mine_style(void *context, float foreground, float background, float blink,
    int pager_foreground)
{
	struct yt_session *session = context;

	session_set_foreground(session, foreground);
	yt_present_set_background(&session->presentation, background);
	yt_present_set_blink(&session->presentation, blink);
	(void)pager_foreground;
}

static bool
mine_encounter(struct yt_session *session, bool *terminal,
    struct yt_error *error)
{
	static const struct yt_sector_mine_ops ops = {
		mine_read_current,
		mine_read_player,
		mine_write_player,
		mine_read_sector,
		mine_write_sector,
		mine_present,
		mine_sound,
		session_append_news_bytes,
		random_value,
		mine_shrink,
		mine_warp,
		mine_set_current,
		mine_style,
	};
	struct yt_sector_mine_state state = {
		.current_player_record = session_record(session),
		.current_sector = session->player.sector,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.foreground = session_foreground(session),
		.background = yt_present_background(&session->presentation),
		.blink = yt_present_blink(&session->presentation),
		.pager_foreground = session_pager_foreground(session),
		.destroyed = &session->destroyed,
	};

	if (terminal == NULL)
		return false;
	if (!yt_sector_mine_run(&state, &ops, session, error))
		return false;
	*terminal = state.terminal;
	return true;
}
static bool
hostile_menu_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = "<Help>";
	static const uint8_t attack[] = "A - <A>ttack";
	static const char *const rows[] = {
		"B - <B>ribe Fighters",
		"D - <D>rop a Mine",
		"I - <I>nformation about your ship",
		"Q - <Q>uit the game",
		"S - Display <S>ector",
		"T - <T>eam Menu",
		"W - Emergency <W>arp",
	};
	size_t index;

	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "hostile help heading", error)
	    || !session_present_paged_line(session, attack, sizeof(attack) - 1U,
	    "hostile help attack row", error))
		return false;
	for (index = 0; index < YT_ARRAY_LEN(rows); ++index) {
		if (!session_present_paged_fragment(session, (const uint8_t *)rows[index],
		    strlen(rows[index])))
			return false;
	}
	return true;
}

static bool
session_quit_confirm(struct yt_session *session, bool *confirmed,
    struct yt_error *error)
{
	static const uint8_t heading[] = "<Quit>";
	static const uint8_t prompt[] = "Are you sure (Y/N)? ";

	if (confirmed == NULL)
		return false;
	*confirmed = false;
	session_set_foreground(session, 7.0f);
	if (!session_present_paged_fragment(session, heading, sizeof(heading) - 1U))
		return false;
	for (;;) {
		char response[80];
		enum yt_yes_no_answer answer;

		if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "hostile quit prompt", error)
		    || !session_read_upper_command(session, response, sizeof(response)))
			return false;
		if (!yt_input_yes_no_candidate(response, session->output_source,
		    sizeof(session->output_source), &answer))
			return false;
		if (answer == YT_YES_NO_YES) {
			*confirmed = true;
			return true;
		}
		if (answer == YT_YES_NO_NO || answer == YT_YES_NO_EMPTY)
			return true;
		yt_present_set_bold(&session->presentation, 1.0f);
		clear_queue(session);
	}
}

static bool
sector_entry(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t hostile_warning[] =
	    "You have to defeat the fighters before you can enter this sector.";
	static const uint8_t hostile_prompt[] =
	    "Option? (A,B,D,I,Q,S,T,W,?=Help):? ";

	for (;;) {
		struct yt_sector sector;
		bool friendly;

		session->shared_status = 0.0f;
		if (!display_sector(session, false, error)
		    || !session_reload_player(session, error))
			return false;
		if (session_is_disruption_sector(session,
		    session->player.sector)) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "black hole leading blank", error)
			    || !session_attention(session,
			    "A *-BLACK HOLE-* grabs you!",
			    "black hole attention", error))
				return false;
			clear_queue(session);
			if (!emergency_warp(session, error))
				return false;
			continue;
		}
		if (!session_read_sector(session,
		    (int)session->player.sector, &sector, error))
			return false;
		if (yt_sector_mines_admitted(sector.mines,
		    session->self_mine_suppressed ? 1.0f : 0.0f)) {
			{
				bool mine_terminal;

				if (!mine_encounter(session, &mine_terminal, error))
					return false;
				/*
				 * Both ordinary return and the zero-effect post-warp
				 * return test the same raw destruction cell before the
				 * scanner back-edge.  The mine and warp children have
				 * already installed their respective durable/FIELD state.
				 */
				if (mine_terminal && session_is_destroyed(session))
					return common_fatal_self(session, error);
			}
			if (session_is_destroyed(session))
				return common_fatal_self(session, error);
			continue;
		}
		friendly = sector_force_friendly(session, &sector, error);
		if (!friendly && error != NULL && error->status != YT_OK)
			return false;
		if (sector.fighters == 0.0f || friendly)
			return true;
		if (!session_present_alert(session, hostile_warning,
		    sizeof(hostile_warning) - 1U,
		    "hostile entry warning", error))
			return false;
		for (;;) {
			uint8_t row[160];
			size_t row_length;
			bool fresh_menu = true;

			if (!session_reload_player(session, error))
				return false;
			session_set_foreground(session, 3.0f);
			if (!yt_hostile_menu_row(session->combat_ship_fighters,
			    session_hostile_deployed_fighters(session), row,
			    sizeof(row), &row_length)) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "hostile fighter row");
				}
				return false;
			}
			if (!session_present_paged_line(session, row, row_length,
			    "hostile fighter row", error))
				return false;
			while (fresh_menu) {
				char response[80];
				enum yt_hostile_menu_route route;

				if (!session_present_timed_paged_row(session, hostile_prompt,
				    sizeof(hostile_prompt) - 1U,
				    "hostile option prompt", error)
				    || !session_read_upper_command(session, response,
				    sizeof(response)))
					return false;
				if (response[0] == '\0')
					(void)snprintf(response, sizeof(response), "%s", "?");
				route = yt_hostile_menu_dispatch(response);
				switch (route) {
				case YT_HOSTILE_MENU_HELP:
					if (!hostile_menu_help(session, error))
						return false;
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_SECTOR:
					goto reenter_sector;
				case YT_HOSTILE_MENU_INFO:
					if (!show_ship(session, error))
						return false;
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_INVALID:
					if (!session_present_alert(session,
					    (const uint8_t *)"Invalid command.",
					    strlen("Invalid command."),
					    "hostile invalid command", error)
					    || !session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE,
					    "hostile invalid trailing blank", error))
						return false;
					break;
				case YT_HOSTILE_MENU_ATTACK:
					if (!attack_deployed(session, &sector, error))
						return false;
					if (session_is_destroyed(session))
						return true;
					if (session_hostile_deployed_fighters(session)
					    <= 0.0) {
						session_set_foreground(session, 1.0f);
						if (!display_sector(session, false, error))
							return false;
						return true;
					}
					fresh_menu = false;
					break;
				case YT_HOSTILE_MENU_QUIT:
				{
					bool confirmed;

					if (!session_quit_confirm(session, &confirmed,
					    error))
						return false;
					if (!confirmed) {
						fresh_menu = false;
						break;
					}
					if (!quit_session(session, error))
						return false;
					session->running = false;
					session->terminated = true;
					return false;
				}
				case YT_HOSTILE_MENU_BRIBE:
				{
					bool direct_hostile_menu;
					bool forced_attack;

					if (!bribe_deployed(session, &sector,
					    &direct_hostile_menu, &forced_attack, error))
						return false;
					if (session_is_destroyed(session))
						return true;
					if (direct_hostile_menu) {
						fresh_menu = false;
						break;
					}
					if (forced_attack) {
						if (session_hostile_deployed_fighters(session)
						    <= 0.0) {
							session_set_foreground(session, 1.0f);
							if (!display_sector(session, false, error))
								return false;
							return true;
						}
						fresh_menu = false;
						break;
					}
					goto reenter_sector;
				}
				case YT_HOSTILE_MENU_MINE:
					if (!command_mines(session, error))
						return false;
					goto reenter_sector;
				case YT_HOSTILE_MENU_WARP:
					if (!direct_emergency_warp(session, error))
						return false;
					goto reenter_sector;
				case YT_HOSTILE_MENU_TEAM:
					if (!command_team(session, error))
						return false;
					goto reenter_sector;
				}
			}
		}

reenter_sector:
		continue;
	}
}

static bool
main_fighters_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
main_fighters_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
main_fighters_write_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_write_sector(session, sector_number, sector,
	    error);
}

static bool
main_fighters_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
main_fighters_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_player(&session->door->game, player_record, player,
	    error);
}

static bool
main_fighters_present(void *context, const uint8_t *text, size_t length,
    enum yt_main_fighters_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_MAIN_FIGHTERS_TITLE:
	case YT_MAIN_FIGHTERS_AVAILABLE:
	case YT_MAIN_FIGHTERS_SUCCESS:
		return session_present_paged_fragment(session, text, length);
	case YT_MAIN_FIGHTERS_UNION_REFUSAL:
		return session_present_alert(session, text, length,
		    "fighter Union refusal", error);
	case YT_MAIN_FIGHTERS_FOREIGN_REFUSAL:
		return session_present_alert(session, text, length,
		    "fighter foreign-force refusal", error);
	case YT_MAIN_FIGHTERS_PROMPT:
		return session_present_timed_paged_row(session, text, length,
		    "fighter desired-count prompt", error);
	case YT_MAIN_FIGHTERS_INSUFFICIENT:
		return session_present_alert(session, text, length,
		    "fighter insufficient notice", error);
	default:
		return false;
	}
}

static bool
main_fighters_input(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	(void)error;
	return session_read_number_command(context, response, capacity);
}

static bool
main_fighters_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "sector fighter sound", error);
}

static bool
command_fighters(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_main_fighters_ops ops = {
		main_fighters_hydrate,
		main_fighters_read_sector,
		main_fighters_write_sector,
		main_fighters_read_player,
		main_fighters_write_player,
		main_fighters_present,
		main_fighters_input,
		main_fighters_sound,
	};
	struct yt_main_fighters_state state = {
		.current_player_record = session_record(session),
	};

	return yt_main_fighters_run(&state, &ops, session, error);
}

static bool
drop_mines_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
drop_mines_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_player(&session->door->game, player_record,
	    player, error);
}

static bool
drop_mines_flush(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_flush(&session->door->game.database, error);
}

static bool
drop_mines_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
drop_mines_write_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_write_sector(session, sector_number, sector,
	    error);
}

static bool
drop_mines_present(void *context, const uint8_t *text, size_t length,
    enum yt_drop_mines_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_DROP_MINES_NO_MINES_ROW:
		return session_present_alert(session, text, length, "no sector mines",
		    error);
	case YT_DROP_MINES_UNION_ROW:
		return session_present_alert(session, text, length,
		    "Union sector mine refusal", error);
	case YT_DROP_MINES_PROMPT_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "sector mine prompt blank", error);
	case YT_DROP_MINES_PROMPT:
		return session_present_timed_paged_row(session, text, length, "sector mine prompt",
		    error);
	case YT_DROP_MINES_SUCCESS_BLANK:
		session_set_foreground(session, 6.0f);
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "sector mine success blank", error);
	case YT_DROP_MINES_SUCCESS_ROW:
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_paged_fragment(session, text, length);
	default:
		return false;
	}
}

static bool
drop_mines_amount(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	return session_read_number_command(session, response, capacity);
}

static void
drop_mines_suppress(void *context)
{
	struct yt_session *session = context;

	session_set_self_mine_suppression(session, true);
}

static bool
drop_mines_sound(void *context, float selector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_sound(session, selector, "sector mine sound", error);
}

static bool
command_mines(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_drop_mines_ops ops = {
		drop_mines_read_player,
		drop_mines_write_player,
		drop_mines_flush,
		drop_mines_read_sector,
		drop_mines_write_sector,
		drop_mines_present,
		drop_mines_amount,
		drop_mines_suppress,
		drop_mines_sound,
	};
	struct yt_drop_mines_state state = {
		.current_player_record = session_record(session),
	};

	return yt_drop_mines_run(&state, &ops, session, error);
}

static bool
port_report_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
port_report_length(struct yt_session *session, float raw, size_t maximum,
    size_t *length, const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)raw,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow || converted < 0)
		return port_report_failure(error, operation);
	*length = (size_t)converted;
	if (*length > maximum)
		*length = maximum;
	return true;
}

static bool
port_owner_row_capture(struct yt_session *session, const struct yt_port *port,
    uint8_t *captured_name, size_t captured_capacity,
    size_t *captured_length, struct yt_error *error)
{
	enum yt_port_owner_kind kind;
	const uint8_t *owner_name = NULL;
	size_t owner_name_length = 0U;
	uint8_t row[256];
	size_t length;
	int owner_record;

	if (captured_length != NULL)
		*captured_length = 0U;
	kind = yt_port_owner_classify(port->owner, session_record(session),
	    &owner_record);
	if (kind == YT_PORT_OWNER_INVALID)
		return port_report_failure(error,
		    "port owner record conversion");
	if (kind == YT_PORT_OWNER_SILENT)
		return true;
	if (kind == YT_PORT_OWNER_OTHER) {
		struct yt_player owner;

		if (!yt_game_read_player(&session->door->game, owner_record,
		    &owner, error))
			return false;
		if (!port_report_length(session, owner.name_length,
		    YT_TEXT_FIELD_SIZE, &owner_name_length,
		    "port owner name length", error))
			return false;
		owner_name = owner.record.bytes;
		if (captured_length != NULL) {
			if (owner_name_length > captured_capacity
			    || (owner_name_length != 0U && captured_name == NULL))
				return port_report_failure(error,
				    "port owner captured name");
			if (owner_name_length != 0U)
				memcpy(captured_name, owner_name, owner_name_length);
			*captured_length = owner_name_length;
		}
	}
	if (!yt_port_owner_compose(kind, port->treasury, owner_name,
	    owner_name_length, row, sizeof(row), &length))
		return port_report_failure(error, "port owner row composition");
	return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port owner leading blank", error)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "port owner row", error);
}

static bool
port_owner_row(struct yt_session *session, const struct yt_port *port,
    struct yt_error *error)
{
	return port_owner_row_capture(session, port, NULL, 0U, NULL, error);
}

static bool
port_report_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (physical_record == (uint32_t)session_record(session)) {
		if (!session_reload_player(session, error))
			return false;
		*player = session->player;
		return true;
	}
	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_OWNER_PLAYER_GET, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

static bool
port_report_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!read_database_record_at_fault(session, physical_record, &record,
	    YT_BASIC_FAULT_PORT_REPORT_PORT_GET, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
port_report_observe_date(void *context, uint8_t date[10],
    struct yt_error *error)
{
	struct yt_clock_value now;
	char rendered[11];

	(void)context;
	if (!yt_platform_clock(&now, error))
		return false;
	yt_format_date(&now, rendered);
	memcpy(date, rendered, 10U);
	return true;
}

static bool
port_report_observe_time(void *context, uint8_t time_text[8],
    struct yt_error *error)
{
	struct yt_clock_value now;
	char rendered[9];

	(void)context;
	if (!yt_platform_clock(&now, error))
		return false;
	yt_format_time(&now, rendered);
	memcpy(time_text, rendered, 8U);
	return true;
}

static bool
port_report_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_report_output_kind kind, size_t item,
    struct yt_error *error)
{
	struct yt_session *session = context;
	const char *operation;

	(void)item;
	switch (kind) {
	case YT_PORT_REPORT_OWNER_BLANK:
		operation = "port owner leading blank";
		break;
	case YT_PORT_REPORT_OWNER_ROW:
		operation = "port owner row";
		break;
	case YT_PORT_REPORT_TITLE_BLANK:
		operation = "port report title blank";
		break;
	case YT_PORT_REPORT_HEADER_BLANK:
		operation = "port report header blank";
		break;
	case YT_PORT_REPORT_ITEM_NAME_STATUS:
		operation = "port report commodity/status";
		break;
	case YT_PORT_REPORT_ITEM_CAPACITY:
		operation = "port report stock";
		break;
	case YT_PORT_REPORT_ITEM_HOLD:
		operation = "port report player hold";
		break;
	case YT_PORT_REPORT_ITEM_PRICE:
		operation = "port report price";
		break;
	case YT_PORT_REPORT_TITLE:
	case YT_PORT_REPORT_HEADER:
	case YT_PORT_REPORT_RULE:
		return session_present_paged_row(session, text, length);
	default:
		return false;
	}
	return session_present_text(session, text, length,
	    kind == YT_PORT_REPORT_ITEM_NAME_STATUS
	    || kind == YT_PORT_REPORT_ITEM_CAPACITY
	    || kind == YT_PORT_REPORT_ITEM_HOLD
	    ? SESSION_PRESENT_RAW : SESSION_PRESENT_LINE, operation, error);
}

static void
port_report_reset_pager(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session_set_pager_line_count_raw(session, raw);
}

static void
port_report_set_bold(void *context, float bold)
{
	struct yt_session *session = context;

	yt_present_set_bold(&session->presentation, bold);
}

static void
port_report_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session_set_foreground(session, foreground);
}

static bool
port_report_capture(struct yt_session *session, int logical_port,
    const struct yt_port_market_state *market,
    struct yt_port *terminal_port, struct yt_error *error)
{
	static const struct yt_port_report_ops ops = {
		port_report_read_player,
		port_report_read_port,
		port_report_observe_date,
		port_report_observe_time,
		port_report_present,
		port_report_reset_pager,
		port_report_set_bold,
		port_report_set_foreground,
	};
	struct yt_port_report_state state;
	int physical_record;

	if (market == NULL)
		return false;
	physical_record = market->port_physical_record != 0U
	    ? (int)market->port_physical_record
	    : (int)session_port_basic_record(session, (float)logical_port);
	if (physical_record < 1)
		return port_report_failure(error,
		    "port report record conversion");
	memset(&state, 0, sizeof(state));
	state.current_player_record = session_record(session);
	state.port_physical_record = (uint32_t)physical_record;
	state.conversion_mode = session->presentation.sound.conversion_mode;
	state.market = *market;
	if (!yt_port_report_run(&state, &ops, session, error))
		return false;
	if (terminal_port != NULL)
		*terminal_port = state.report_port;
	return true;
}

static bool
port_report(struct yt_session *session, int logical_port,
    const struct yt_port_market_state *market, struct yt_error *error)
{
	return port_report_capture(session, logical_port, market, NULL, error);
}

static bool
computer_port_ordinary(struct yt_session *session, int sector_number,
    float sector_record_expression,
    const struct yt_computer_port_visibility_state *visibility,
    struct yt_error *error)
{
	static const struct yt_port_update_ops update_ops = {
		port_update_read_sector,
		port_update_observe_day,
		port_update_read_port,
		port_update_observe_timer,
		port_update_write_port,
	};
	static const struct yt_port_report_ops report_ops = {
		port_report_read_player,
		port_report_read_port,
		port_report_observe_date,
		port_report_observe_time,
		port_report_present,
		port_report_reset_pager,
		port_report_set_bold,
		port_report_set_foreground,
	};
	struct yt_port_ordinary_state state;

	memset(&state, 0, sizeof(state));
	state.update.sector_number = sector_number;
	state.update.sector_record_offset =
	    session_sector_offset(session);
	state.update.sector_record_expression = sector_record_expression;
	state.update.sector_record_supplied = true;
	state.update.port_offset = session_port_offset(session);
	memcpy(state.update.base_price, session->market_bases,
	    sizeof(state.update.base_price));
	state.report.current_player_record = session_record(session);
	state.report.conversion_mode =
	    session->presentation.sound.conversion_mode;
	if (visibility != NULL) {
		state.field_record = visibility->field_record;
		state.field = visibility->field;
		state.field_valid = visibility->field_valid;
		state.field_kind = visibility->field_kind
		    == YT_COMPUTER_PORT_FIELD_PLAYER
		    ? YT_PORT_ORDINARY_FIELD_PLAYER
		    : YT_PORT_ORDINARY_FIELD_SECTOR;
	}
	return yt_port_ordinary_run(&state, &update_ops, &report_ops,
	    session, error);
}

static bool
commodity_trade_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (physical_record != (uint32_t)session_record(session))
		return port_report_failure(error,
		    "commodity trade player record");
	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
commodity_trade_write_player(void *context, uint32_t physical_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (physical_record != (uint32_t)session_record(session))
		return port_report_failure(error,
		    "commodity trade player record");
	session->player = *player;
	return yt_database_write_durable(&session->door->game.database,
	    (size_t)physical_record, &player->record, error);
}

static bool
commodity_trade_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
commodity_trade_write_port(void *context, uint32_t physical_record,
    const struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write_durable(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
}

static bool
commodity_trade_present(void *context, const uint8_t *text, size_t length,
    enum yt_commodity_trade_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;
	const char *operation;

	switch (kind) {
	case YT_COMMODITY_TRADE_STATUS:
		operation = "commodity trade player status";
		return session_present_paged_line(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_MARKET:
		operation = "commodity trade market status";
		return session_present_paged_line(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_QUANTITY_PROMPT:
		operation = "commodity trade quantity prompt";
		return session_present_timed_paged_row(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_CAPACITY_ERROR:
		operation = "commodity trade capacity rejection";
		return session_present_alert(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_FREE_HOLDS_ERROR:
		operation = "commodity trade free-holds rejection";
		return session_present_alert(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_FREE_HOLDS_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE,
		    "commodity trade free-holds retry blank", error);
	case YT_COMMODITY_TRADE_MAXIMUM_ERROR:
		operation = "commodity trade maximum rejection";
		return session_present_alert(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_NOT_SELLING_ERROR:
		return session_present_paged_fragment(session, text, length);
	case YT_COMMODITY_TRADE_DONT_WANT_ERROR:
		operation = "commodity trade buying retry";
		return session_present_alert(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_PLAYER_AMOUNT_ERROR:
		operation = "commodity trade hold retry";
		return session_present_alert(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_AGREED:
	case YT_COMMODITY_TRADE_DECLINED:
	case YT_COMMODITY_TRADE_SUCCESS:
		return session_present_paged_fragment(session, text, length);
	case YT_COMMODITY_TRADE_OFFER:
		return session_present_paged_line(session, text, length,
		    "commodity trade offer row", error);
	default:
		return false;
	}
}

static bool
commodity_trade_input(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	(void)error;
	return session_read_upper_command(context, response, capacity);
}

static bool
commodity_trade_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (!session_confirm(context, prompt, length, &answer, error))
		return false;
	*accepted = answer != YT_YES_NO_NO;
	return true;
}

static bool
trade_commodity(struct yt_session *session,
    const struct yt_port_market_state *market, size_t commodity,
    bool *prompt_reached, struct yt_error *error)
{
	static const struct yt_commodity_trade_ops ops = {
		commodity_trade_read_player,
		commodity_trade_write_player,
		apply_player_credit_mutation,
		commodity_trade_read_port,
		commodity_trade_write_port,
		commodity_trade_present,
		commodity_trade_input,
		commodity_trade_confirm,
	};
	struct yt_commodity_trade_state transaction;

	if (market == NULL)
		return false;
	memset(&transaction, 0, sizeof(transaction));
	transaction.current_player_record = (uint32_t)session_record(session);
	transaction.port_physical_record = market->port_physical_record;
	transaction.commodity = commodity;
	transaction.market = *market;
	if (!yt_commodity_trade_run(&transaction, &ops, session, error))
		return false;
	if (prompt_reached != NULL && transaction.prompt_reached)
		*prompt_reached = true;
	return true;
}

static bool
ordinary_commerce_update(void *context, int sector_number,
    float sector_record_expression,
    struct yt_port_market_state *market, struct yt_error *error)
{
	return port_update(context, sector_number, &sector_record_expression,
	    NULL, market, error);
}

static bool
ordinary_commerce_report(void *context,
    const struct yt_port_market_state *market, struct yt_error *error)
{
	return port_report(context, (int)market->logical_port, market, error);
}

static bool
ordinary_commerce_trade(void *context,
    const struct yt_port_market_state *market, size_t commodity,
    bool *prompt_reached, struct yt_error *error)
{
	return trade_commodity(context, market, commodity, prompt_reached,
	    error);
}

static bool
ordinary_commerce_present(void *context, const uint8_t *text, size_t length,
    enum yt_ordinary_commerce_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_ORDINARY_COMMERCE_REFUSAL:
		return session_present_alert(session, text, length,
		    "port docking refusal", error);
	case YT_ORDINARY_COMMERCE_STATUS:
		return session_present_paged_line(session, text, length,
		    "port docking cargo status", error);
	default:
		return false;
	}
}

static void
ordinary_commerce_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session_set_foreground(session, foreground);
}

static bool
docking_front_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_docking_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PORT_DOCKING_LABEL:
		return session_present_paged_fragment(session, text, length);
	case YT_PORT_DOCKING_NO_PORT:
		return session_present_alert(session, text, length,
		    "port docking no port", error);
	case YT_PORT_DOCKING_LEADING_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "port docking leading blank", error);
	case YT_PORT_DOCKING_PREFIX:
		return session_present_timed_paged_row(session, text, length,
		    "port docking prelude", error);
	default:
		return false;
	}
}

static bool
docking_front_gate(void *context, bool *denied, float *current_sector,
    float *sector_record_expression, struct yt_error *error)
{
	struct yt_session *session = context;

	if (!fresh_no_turn_gate(session, denied, error))
		return false;
	*current_sector = session->player.sector;
	*sector_record_expression = session->current_sector_record;
	return true;
}

static bool
docking_front_read_sector(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
docking_front_finalize(void *context, bool *returned, float *current_sector,
    float *sector_record_expression, struct yt_error *error)
{
	struct yt_session *session = context;
	bool ok = finalize_action(session, 1.0f, error);

	*current_sector = session->player.sector;
	*sector_record_expression = session->current_sector_record;
	if (ok) {
		*returned = true;
		return true;
	}
	if (error == NULL || error->status == YT_OK) {
		*returned = false;
		return true;
	}
	return false;
}

static bool
docking_front_earth(void *context, bool *reenter_sector,
    struct yt_error *error)
{
	return earth_store(context, reenter_sector, error);
}

static bool
docking_front_ordinary(void *context, int sector_number,
    float sector_record_expression,
    struct yt_error *error)
{
	static const struct yt_ordinary_commerce_ops ops = {
		ordinary_commerce_update,
		ordinary_commerce_report,
		ordinary_commerce_trade,
		commodity_trade_read_player,
		ordinary_commerce_present,
		ordinary_commerce_foreground,
	};
	struct yt_session *session = context;
	struct yt_ordinary_commerce_state commerce;
	bool result;

	memset(&commerce, 0, sizeof(commerce));
	commerce.sector_number = sector_number;
	commerce.sector_record_expression = sector_record_expression;
	commerce.current_player_record = (uint32_t)session_record(session);
	commerce.first_name =
	    (const uint8_t *)session->door->identity.real_first;
	commerce.first_name_length = strlen(session->door->identity.real_first);
	result = yt_ordinary_commerce_run(&commerce, &ops, session, error);
	if (result)
		session->inherited_loop_index = 4.0f;
	return result;
}

static bool
command_trade(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const struct yt_port_docking_ops ops = {
		docking_front_present,
		ordinary_commerce_foreground,
		docking_front_gate,
		docking_front_read_sector,
		docking_front_finalize,
		commodity_trade_read_port,
		docking_front_earth,
		docking_front_ordinary,
	};
	struct yt_port_docking_state state;

	memset(&state, 0, sizeof(state));
	state.port_offset = session_port_offset(session);
	if (enter_sector != NULL)
		*enter_sector = false;
	if (!yt_port_docking_run(&state, &ops, session, error))
		return false;
	if (enter_sector != NULL)
		*enter_sector = state.reenter_sector;
	return true;
}

static bool
earth_receipt(struct yt_session *session, const struct yt_port *cached_earth,
    float cost,
    struct yt_error *error)
{
	struct yt_port earth;

	if (!session_mutate_player_credits(session, -cost, NULL, error))
		return false;
	if (cached_earth->owner != 0.0f) {
		float receipt = yt_earth_receipt_amount(cached_earth->owner,
		    session_record(session), cost);

		if (!session_read_port(session, 1, &earth, error))
			return false;
		earth.treasury = single_add(earth.treasury, receipt);
		if (!session_write_port(session, 1, &earth, error))
			return false;
	}
	return true;
}

static bool
earth_quantity_input(struct yt_session *session, const char *prompt,
    double *value, bool *blank, struct yt_error *error)
{
	char line[160];
	struct qb_val_result parsed;

	if (!session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "Earth purchase quantity prompt", error)
	    || !session_read_number_command(session, line, sizeof(line)))
		return false;
	*blank = line[0] == '\0';
	parsed = qb_val(line);
	if (parsed.overflow)
		return port_report_failure(error, "Earth purchase quantity VAL");
	*value = parsed.valid ? parsed.value : 0.0;
	return true;
}

static bool
earth_credit_error(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	return session_present_alert(session, (const uint8_t *)text, strlen(text),
	    "Earth purchase attention", error);
}

static bool
earth_purchase_holds(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	static const char prompt[] = "Buy how many holds? [0]? ";
	char amount[64];
	char row[128];
	double requested;
	double affordable;
	float quantity;
	float cost;
	bool blank;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth Holds leading blank", error))
		return false;
	if (session->player.holds >= session->door->game.config.maximum_holds)
		return earth_credit_error(session, "You dont need any holds.", error);
	if (qb_str_single(amount, sizeof(amount), single_sub(
	    session->door->game.config.maximum_holds,
	    session->player.holds)) < 0
	    || snprintf(row, sizeof(row), "You need%s holds.", amount) < 0)
		return port_report_failure(error, "Earth Holds needed row");
	if (!session_present_paged_line(session, (const uint8_t *)row, strlen(row),
	    "Earth Holds needed row", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (!earth_quantity_input(session, prompt, &requested, &blank, error))
		return false;
	quantity = yt_earth_purchase_quantity(requested);
	if (quantity < 1.0f)
		return true;
	if ((double)quantity > affordable)
		return earth_credit_error(session,
		    "You do not have enough credits!", error);
	if (single_add(session->player.holds, quantity)
	    > session->door->game.config.maximum_holds)
		return earth_credit_error(session,
		    "You don't need that many!", error);
	session->player.holds = single_add(session->player.holds, quantity);
	cost = single_mul(quantity, price);
	return write_player(session, error)
	    && earth_receipt(session, cached_earth, cost, error);
}

static bool
earth_purchase_supply(struct yt_session *session,
    const struct yt_port *cached_earth, int choice, float price,
    struct yt_error *error)
{
	const char *prompt;
	double requested;
	double affordable;
	float quantity;
	float cost;
	bool blank;

	if (choice == 3)
		prompt = "Buy how many fighters? [0]? ";
	else if (choice == 7)
		prompt = "Buy how many ground force units? [0]? ";
	else
		prompt = "Buy how much shield power? [0]? ";
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth supply leading blank", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (!earth_quantity_input(session, prompt, &requested, &blank, error))
		return false;
	quantity = yt_earth_purchase_quantity(requested);
	if (quantity < 1.0f)
		return true;
	if ((double)quantity > affordable)
		return earth_credit_error(session,
		    "You do not have enough credits!", error);
	cost = single_mul(quantity, price);
	yt_earth_supply_overlay(&session->player, choice, quantity);
	return write_player(session, error)
	    && earth_receipt(session, cached_earth, cost, error);
}

static bool
earth_purchase_cloak(struct yt_session *session,
    const struct yt_port *cached_earth, struct yt_error *error)
{
	for (;;) {
		char deficit_text[64];
		char default_text[64];
		char row[128];
		char prompt[192];
		double requested;
		float points;
		float deficit;
		float default_quantity;
		float quantity;
		float cost;
		bool blank;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth Cloak leading blank", error))
			return false;
		points = yt_earth_cloak_points(session->player.cloak);
		deficit = single_sub(50.0f, points);
		default_quantity = yt_earth_cloak_default(deficit,
		    session->player.credits);
		if (qb_str_single(deficit_text, sizeof(deficit_text), deficit) < 0
		    || qb_str_single(default_text, sizeof(default_text),
		    default_quantity) < 0
		    || snprintf(row, sizeof(row),
		    "Cloak energy is down by%s%%.", deficit_text) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "Buy how many points of Cloak Energy? (0 -%s) [%s ] ?",
		    deficit_text, default_text) < 0)
			return port_report_failure(error,
			    "Earth Cloak row formatting");
		if (!session_present_paged_row(session, (const uint8_t *)row, strlen(row))
		    || !earth_quantity_input(session, prompt, &requested, &blank,
		    error))
			return false;
		quantity = blank ? default_quantity
		    : yt_earth_purchase_quantity(requested);
		if (quantity < 1.0f)
			return true;
		if (single_add(points, quantity) > 50.0f) {
			if (!earth_credit_error(session,
			    "You can't have over 100% cloak!", error))
				return false;
			continue;
		}
		cost = single_mul(quantity, 1000.0f);
		if (cost > session->player.credits)
			return earth_credit_error(session,
			    "You do not have enough credits!", error);
		session->player.cloak = yt_earth_cloak_overlay(points, quantity);
		return write_player(session, error)
		    && earth_receipt(session, cached_earth, cost, error);
	}
}

static bool
earth_purchase_scanner(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	double affordable = yt_earth_affordable(session->player.credits, price);

	if (session->player.danger_scanner != 0.0f)
		return earth_credit_error(session,
		    "You already HAVE a Danger Scanner!", error);
	if (floor(affordable) < 1.0)
		return earth_credit_error(session,
		    "You cannot afford a Danger Scanner!", error);
	session->player.danger_scanner = -1.0f;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Earth Scanner leading blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_present_paged_row(session,
	    (const uint8_t *)"Danger Scanner installed in your ship!",
	    strlen("Danger Scanner installed in your ship!"))
	    || !write_player(session, error))
		return false;
	return earth_receipt(session, cached_earth, price, error);
}

static bool
earth_purchase_spies(struct yt_session *session,
    const struct yt_port *cached_earth, float price, struct yt_error *error)
{
	for (;;) {
		char active_text[64];
		char active_row[128];
		char quantity_prompt[96];
		double requested;
		double affordable;
		float quantity_value;
		float cost;
		int quantity;
		int spy_index;
		int active_count = session->spy_count;
		bool blank;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth Spies leading blank", error))
			return false;
		affordable = yt_earth_affordable(session->player.credits, price);
		if (active_count != 0) {
			if (qb_str_single(active_text, sizeof(active_text),
			    (float)active_count) < 0
			    || snprintf(active_row, sizeof(active_row),
			    "You have%s spies active already.", active_text) < 0)
				return port_report_failure(error,
				    "Earth Spies active row");
			yt_present_set_bold(&session->presentation, 1.0f);
			if (!session_present_paged_fragment(session, (const uint8_t *)active_row,
			    strlen(active_row)))
				return false;
		}
		if (snprintf(quantity_prompt, sizeof(quantity_prompt),
		    "Hire how many%s spies? [0]? ",
		    active_count != 0 ? " more" : "") < 0)
			return port_report_failure(error,
			    "Earth Spies quantity prompt");
		if (!earth_quantity_input(session, quantity_prompt, &requested,
		    &blank, error))
			return false;
		quantity_value = yt_earth_purchase_quantity(requested);
		if (quantity_value < 1.0f)
			return true;
		if ((double)quantity_value > affordable)
			return earth_credit_error(session,
			    "You do not have enough credits!", error);
		if ((float)active_count + quantity_value > 3.0f) {
			if (!earth_credit_error(session,
			    "Max spies allowed is 3!", error))
				return false;
			continue;
		}
		quantity = (int)quantity_value;
		cost = single_mul(quantity_value, price);
		for (spy_index = 0; spy_index < quantity; ++spy_index) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "Earth Spy assignment blank", error))
				return false;
			for (;;) {
				char number[64];
				char prompt[128];
				double sector_value;
				float sector;
				bool overflow;
				int32_t selected;

				if (qb_str_single(number, sizeof(number),
				    (float)(active_count + spy_index + 1)) < 0
				    || snprintf(prompt, sizeof(prompt),
				    "Start spy #%s in what sector?", number) < 0)
					return false;
				if (!earth_quantity_input(session, prompt,
				    &sector_value, &blank, error))
					return false;
				sector = yt_earth_purchase_quantity(sector_value);
				if (sector == 0.0f
				    || sector > (float)session_sector_count(session))
					continue;
				selected = qb_cint(sector, &overflow);
				if (overflow)
					return port_report_failure(error,
					    "Earth Spy sector CINT");
				session->spy_sectors[active_count + spy_index] =
				    (int)selected;
				break;
			}
		}
		session->spy_count = active_count + quantity;
		if (!computer_spies(session, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "spy purchase pause blank", error)
		    || !session_press_any_key(session, false, error)
		    || !write_player(session, error))
			return false;
		return earth_receipt(session, cached_earth, cost, error);
	}
}

static bool
earth_anti_cloak(struct yt_session *session, float price,
    struct yt_error *error)
{
	static const uint8_t activation[] =
	    "ti-Cloaking device activated!\xd4" "D";
	static const uint8_t waves[] =
	    "Waves of electromagnetic disruption flood the galaxy..."
	    "\xd4\x0e\x00\x86\xc1" " is uncl";
	static const uint8_t uncloaked[] = " is uncloaked!";
	static const uint8_t none[] = "Too bad noone was cloaked anyhow!";
	static const uint8_t fade[] = "...the effect fades.";
	static const uint8_t zero[4] = {0};
	struct yt_player field_player;
	uint8_t row[YT_TEXT_FIELD_SIZE + sizeof(uncloaked) - 1U];
	int player_terminal = (int)session_sector_offset(session);
	int player_record;
	bool field_loaded = false;
	bool reported = false;

	if (!session_present_text(session, activation,
	    sizeof(activation) - 1U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error))
		return false;
	if (session_foreground(session) != 2.0f)
		session_set_color(session, 2);
	if (!session_present_text(session, waves, sizeof(waves) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error)
	    || !session_sound(session, 2.0f,
	    "anti-cloak transaction sound", error))
		return false;
	for (player_record = YT_PLAYER_FIRST; player_record <= player_terminal;
	    ++player_record) {
		size_t name_length;

		if (session_player_cache_value(session, player_record,
		    YT_PLAYER_CACHE_CLOAK) <= 0.0f)
			continue;
		session_set_player_cache_raw(session, player_record,
		    YT_PLAYER_CACHE_CLOAK, zero);
		if (!yt_game_read_player(&session->door->game, player_record,
		    &field_player, error)) {
			if (session_foreground(session) != 6.0f)
				session_set_color(session, 6);
			if (field_loaded)
				session->player.record = field_player.record;
			return false;
		}
		field_loaded = true;
		if (field_player.killed_by != 0.0f)
			continue;
		if (!yt_player_stored_name(&field_player, row, &name_length,
		    error)) {
			if (session_foreground(session) != 6.0f)
				session_set_color(session, 6);
			session->player.record = field_player.record;
			return false;
		}
		memcpy(row + name_length, uncloaked, sizeof(uncloaked) - 1U);
		if (session_foreground(session) != 6.0f)
			session_set_color(session, 6);
		if (!session_present_text(session, row,
		    name_length + sizeof(uncloaked) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error)
		    || !session_sound(session, 1.0f,
		    "anti-cloak transaction sound", error)) {
			session->player.record = field_player.record;
			return false;
		}
		reported = true;
	}
	if (session_foreground(session) != 2.0f)
		session_set_color(session, 2);
	if (!reported
	    && (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error)
	    || !session_present_text(session, none, sizeof(none) - 1U,
	    SESSION_PRESENT_LINE, "anti-cloak transaction row", error))) {
		if (field_loaded)
			session->player.record = field_player.record;
		return false;
	}
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error)
	    || !session_present_text(session, fade, sizeof(fade) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "anti-cloak transaction row", error)
	    || !session_sound(session, 5.0f,
	    "anti-cloak transaction sound", error)) {
		if (field_loaded)
			session->player.record = field_player.record;
		return false;
	}
	{
		bool credit_loaded = false;

		if (!session_mutate_player_credits(session, -price,
		    &credit_loaded, error)) {
			if (!credit_loaded && field_loaded)
				session->player.record = field_player.record;
			return false;
		}
	}
	session_set_color(session, 3);
	return true;
}

static bool
clearance(struct yt_session *session, bool create,
    struct yt_error *error)
{
	static const char *const name[4] = {
		"Holds", "Fighters", "Shields", "Ground Forces"
	};
	bool announced = false;
	size_t index;

	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "clearance leading blank", error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->clearance_discounts);
	    ++index) {
		float discount = session->clearance_discounts[index];
		float draw;
		char percent[64];
		char row[192];
		int row_length;

		if (!random_value(session, &draw, error))
			return false;
		if (yt_clearance_candidate_needed(index, draw, discount, create)) {
			if (!random_value(session, &draw, error))
				return false;
			discount = draw;
			session->clearance_discounts[index] = discount;
		}
		if (!yt_clearance_normalize(index, &discount)) {
			session->clearance_discounts[index] = 0.0f;
			continue;
		}
		session->clearance_discounts[index] = discount;
		if (qb_str_single(percent, sizeof(percent),
		    yt_clearance_percentage(discount)) < 0)
			return false;
		row_length = snprintf(row, sizeof(row),
		    "Special clearance sale! The Trader's Guild is selling "
		    "%s for%s%% off!", name[index], percent);
		if (row_length < 0 || (size_t)row_length >= sizeof(row)
		    || !session_present_text(session, (const uint8_t *)row,
		    (size_t)row_length, SESSION_PRESENT_LINE,
		    "clearance announcement", error))
			return false;
		announced = true;
	}
	if (!announced)
		return true;
	return session_sound(session, 1.0f, "clearance sale sound", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "clearance trailing blank", error);
}

static bool
earth_report_row(struct yt_session *session, const char *label, float price,
    bool lottery_price, struct yt_error *error)
{
	char price_text[64];
	char cost[96];
	char affordable_text[80];
	char tail[96];
	double affordable;

	if (!session_fixed_width(session, label, 22.0f,
	    "Earth report item field", error))
		return false;
	if (lottery_price)
		snprintf(cost, sizeof(cost), "%s", "* 5");
	else {
		if (qb_str_single(price_text, sizeof(price_text), price) < 0
		    || snprintf(cost, sizeof(cost), "*%s ", price_text) < 0)
			return port_report_failure(error,
			    "Earth report price format");
	}
	if (!session_fixed_width(session, cost, 9.0f,
	    "Earth report cost field", error))
		return false;
	affordable = yt_earth_affordable(session->player.credits, price);
	if (qb_str_double(affordable_text, sizeof(affordable_text),
	    affordable) < 0
	    || snprintf(tail, sizeof(tail), "*%s", affordable_text) < 0)
		return port_report_failure(error,
		    "Earth report affordability format");
	return session_present_paged_row(session, (const uint8_t *)tail, strlen(tail));
}

static bool
lottery_settle(struct yt_session *session,
    const struct yt_port *cached_earth, float cost,
    struct yt_error *error)
{
	if (!session_wait(session, 3.0, "lottery caller wait", error)
	    || !write_player(session, error))
		return false;
	return earth_receipt(session, cached_earth, cost, error);
}

static bool
lottery(struct yt_session *session, const struct yt_port *cached_earth,
    struct yt_error *error)
{
	char ticket[80];
	char cached_name[sizeof(session->player.name)];
	int winning[6];
	bool matched_winning[6] = {0};
	int matches = 0;
	int index;
	float award = 0.0f;

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (!session_reload_player(session, error))
		return false;
	if (session->player.credits < 5.0f) {
		if (!earth_credit_error(session,
		    "You can't afford a lottery ticket!", error))
			return false;
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery limiter leading blank", error))
		return false;
	if (session->door->game.config.lottery_plays == 0.0f) {
		if (!session_present_text(session,
		    (const uint8_t *)
		    "Sorry, The supreme ruler has banned all gambling!",
		    strlen("Sorry, The supreme ruler has banned all gambling!"),
		    SESSION_PRESENT_BOLD_LINE, "lottery disabled row", error))
			return false;
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	{
		char limit[64];
		char row[128];

		if (qb_str_single(limit, sizeof(limit), session->door->game.config.lottery_plays) < 0
		    || snprintf(row, sizeof(row), "You may play%s times daily.",
		    limit) < 0
		    || !session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE, "lottery daily limit row",
		    error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	session->player.lottery_plays =
	    single_add(session->player.lottery_plays, 1.0f);
	if (session->player.lottery_plays > session->door->game.config.lottery_plays) {
		if (!session_present_text(session,
		    (const uint8_t *)
		    "You will be allowed to play again tomorrow.",
		    strlen("You will be allowed to play again tomorrow."),
		    SESSION_PRESENT_BOLD_LINE, "lottery daily reached row", error))
			return false;
		session->player.lottery_plays = single_sub(
		    session->player.lottery_plays, 1.0f);
		return lottery_settle(session, cached_earth, 0.0f, error);
	}
	if (!write_player(session, error))
		return false;
	{
		char plays[64];
		char row[128];

		if (qb_str_single(plays, sizeof(plays),
		    session->player.lottery_plays) < 0
		    || snprintf(row, sizeof(row),
		    "You've played%s times already.", plays) < 0
		    || !session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE,
		    "lottery already-played row", error))
			return false;
	}
	if (!clearance(session, true, error))
		return false;
	session_set_color(session, 1);
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_present_paged_row(session,
	    (const uint8_t *)"Welcome to the Intergalactic Pick-6 Lottery!",
	    strlen("Welcome to the Intergalactic Pick-6 Lottery!")))
		return false;
	for (;;) {
		bool valid = true;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "lottery ticket leading blank", error))
			return false;
		session_set_color(session, 2);
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_present_timed_paged_row(session,
		    (const uint8_t *)
		    "Enter a 6 digit number for the lottery computer -+>",
		    strlen("Enter a 6 digit number for the lottery computer -+>"),
		    "lottery ticket prompt", error)
		    || !session_read_command(session, ticket, sizeof(ticket)))
			return false;
		if (strlen(ticket) != 6U) {
			if (!session_present_text(session,
			    (const uint8_t *)"That's not 6 digits!",
			    strlen("That's not 6 digits!"), SESSION_PRESENT_LINE,
			    "lottery ticket length row", error))
				return false;
			continue;
		}
		for (index = 0; index < 6; ++index) {
			if (ticket[index] < '0' || ticket[index] > '9')
				valid = false;
		}
		if (!valid) {
			if (!session_present_text(session,
			    (const uint8_t *)"Please enter NUMBERS only!",
			    strlen("Please enter NUMBERS only!"),
			    SESSION_PRESENT_LINE, "lottery ticket digit row", error))
				return false;
			continue;
		}
		break;
	}
	for (index = 0; index < 6; ++index) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		winning[index] = (int)floorf(single_mul(draw, 10.0f));
	}
	matches = yt_lottery_match_count(winning, ticket, matched_winning);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery winning display blank", error)
	    || !session_present_text(session,
	    (const uint8_t *)"The Galactic Lottery Computer picked: ",
	    strlen("The Galactic Lottery Computer picked: "),
	    SESSION_PRESENT_RAW, "lottery winning prefix", error))
		return false;
	{
		int saved_foreground = session_pager_foreground(session);

	for (index = 0; index < 6; ++index) {
		int row;
		int column;
		int dummy;
		uint8_t digit;

		if (!session_wait(session, 0.4000000059604645,
		    "lottery digit pre-roll wait", error))
			return false;
		yt_out_cursor_position(&row, &column);
		session_set_color(session, 6);
		for (dummy = 0; dummy < 18; ++dummy) {
			float draw;
			struct yt_present_result rewind;
			enum yt_present_status status;

			if (!random_value(session, &draw, error))
				return false;
			digit = (uint8_t)('0' + (int)floorf(
			    single_mul(draw, 10.0f)));
			if (!session_present_text(session, &digit, 1U,
			    SESSION_PRESENT_RAW, "lottery dummy digit", error))
				return false;
			if (!session_wait(session, 0.004999999888241291,
			    "lottery animation wait", error))
				return false;
			status = yt_present_lottery_rewind(row, column,
			    &session->presentation, &rewind);
			if (status != YT_PRESENT_OK)
				return port_report_failure(error,
				    "lottery digit rewind");
			yt_out_present_result(&rewind);
		}
		session_set_color(session, matched_winning[index] ? 3 : 7);
		digit = (uint8_t)('0' + winning[index]);
		if (!session_present_text(session, &digit, 1U,
		    SESSION_PRESENT_RAW, "lottery actual digit", error))
			return false;
	}
	if (!session_wait(session, 0.4000000059604645,
	    "lottery post-digits wait", error))
		return false;
	session_set_color(session, saved_foreground);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery post-digits first blank", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "lottery post-digits second blank", error))
		return false;
	if (matches == 0) {
		if (!session_present_text(session,
		    (const uint8_t *)"Sorry, you didn't win this time.",
		    strlen("Sorry, you didn't win this time."),
		    SESSION_PRESENT_LINE, "lottery loss row", error))
			return false;
		return lottery_settle(session, cached_earth, 5.0f, error);
	}
	{
		char match_text[64];
		char award_text[80];
		int saved_foreground = session_pager_foreground(session);

		award = yt_lottery_award(matches);
		if (qb_str_single(match_text, sizeof(match_text),
		    (float)matches) < 0
		    || qb_str_double(award_text, sizeof(award_text),
		    (double)award) < 0
		    || !session_present_text(session,
		    (const uint8_t *)"You matched", strlen("You matched"),
		    SESSION_PRESENT_RAW, "lottery award prefix", error))
			return false;
		session_set_color(session, 3);
		if (!session_present_text(session, (const uint8_t *)match_text,
		    strlen(match_text), SESSION_PRESENT_BOLD_RAW,
		    "lottery award matches", error))
			return false;
		session_set_color(session, saved_foreground);
		if (!session_present_text(session,
		    (const uint8_t *)" digits and won", strlen(" digits and won"),
		    SESSION_PRESENT_RAW, "lottery award middle", error))
			return false;
		session_set_color(session, 3);
		if (!session_present_text(session, (const uint8_t *)award_text,
		    strlen(award_text), SESSION_PRESENT_BOLD_RAW,
		    "lottery award value", error))
			return false;
		session_set_color(session, saved_foreground);
		if (!session_present_text(session,
		    (const uint8_t *)" credits!", strlen(" credits!"),
		    SESSION_PRESENT_LINE, "lottery award suffix", error))
			return false;
	}
	if (!session_reload_player(session, error))
		return false;
	for (index = 0; index < matches; ++index) {
		if (!session_sound(session, 1.0f,
		    "lottery award sound", error))
			return false;
	}
	if (matches > 3) {
		char news[300];
		char amount[80];

		if (qb_str_double(amount, sizeof(amount), (double)award) < 0
		    || snprintf(news, sizeof(news),
		    "%s won%s credits in the lottery!", cached_name, amount) < 0)
			return port_report_failure(error, "lottery news row");
		if (!append_news(session, news, error))
			return false;
	}
	if (!session_mutate_player_credits(session, award, NULL, error)
	    || !session_wait(session, 3.0, "lottery award wait", error))
		return false;
	return lottery_settle(session, cached_earth, 5.0f, error);
}

static bool
earth_report(struct yt_session *session, struct yt_port *earth,
    float price[4], struct yt_error *error)
{
	static const uint8_t separator[] =
	    "----------------------*--------*------------";
	static const uint8_t header[] =
	    "         ITEM         *  COST  * CAN AFFORD";
	static const char *const item_label[9] = {
		"[1] Cloak Energy", "[2] Cargo Holds", "[3] Fighters",
		"[4] Play Lottery", "[5] Danger Scanner",
		"[6] Anti-Cloak Device", "[7] Ground Forces",
		"[8] Shield Power", "[9] Hire Spies (Each)"
	};
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];
	char title[128];
	float discount[4];
	size_t index;

	if (earth == NULL || price == NULL)
		return false;
	session_set_pager_line_count(session, 0.0f);
	if (!read_port_at_fault(session, 1, earth,
	    YT_BASIC_FAULT_PORT_EARTH_GET, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (!yt_platform_clock(&date_now, error)
	    || !yt_platform_clock(&time_now, error))
		return false;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	if (snprintf(title, sizeof(title),
	    "Commerce report for Earth: %s %s", date, time_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)title,
	    strlen(title), "Earth report title", error)
	    || !port_owner_row(session, earth, error))
		return false;
	memcpy(discount, session->clearance_discounts, sizeof(discount));
	yt_earth_prices(discount, price);
	if (!session->earth_report_seen) {
		if (!clearance(session, false, error))
			return false;
	}
	else if (!session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "Earth report ordinary blank", error))
		return false;
	session->earth_report_seen = true;
	if (!session_reload_player(session, error))
		return false;
	if (!session_present_paged_row(session, separator, sizeof(separator) - 1U)
	    || !session_present_paged_row(session, header, sizeof(header) - 1U)
	    || !session_present_paged_row(session, separator, sizeof(separator) - 1U))
		return false;
	for (index = 0; index < 9U; ++index) {
		float item_price;
		bool lottery_price = index == 3U;

		if (index == 0U)
			item_price = 1000.0f;
		else if (index == 1U)
			item_price = price[0];
		else if (index == 2U)
			item_price = price[1];
		else if (index == 3U)
			item_price = 5.0f;
		else if (index == 4U)
			item_price = 500000.0f;
		else if (index == 5U || index == 8U)
			item_price = 1000000000.0f;
		else if (index == 6U)
			item_price = price[3];
		else
			item_price = price[2];
		if (!earth_report_row(session, item_label[index], item_price,
		    lottery_price, error))
			return false;
	}
	return session_present_paged_row(session, separator, sizeof(separator) - 1U);
}

static bool
earth_store(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t menu[] =
	    "[I] Ship Info -=*=- [0] Leave Port";
	static const uint8_t prompt_suffix[] =
	    " -=*=- Buy Which Item? -=>";
	static const uint8_t invalid[] = "INAVLID CHOICE!";

	if (enter_sector != NULL)
		*enter_sector = false;
	for (;;) {
		struct yt_port earth;
		struct qb_val_result parsed;
		char line[80];
		char credits_text[64];
		char prompt[160];
		float price[4];
		int choice;
		float holds_price;
		float fighters_price;
		float shields_price;
		float ground_price;

		if (!earth_report(session, &earth, price, error))
			return false;
		holds_price = price[0];
		fighters_price = price[1];
		shields_price = price[2];
		ground_price = price[3];
		if (!session_present_paged_line(session, menu, sizeof(menu) - 1U,
		    "Earth report menu", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth prompt blank", error)
		    || qb_str_double(credits_text, sizeof(credits_text),
		    (double)session->player.credits) < 0
		    || snprintf(prompt, sizeof(prompt), "Credits:%s%s",
		    credits_text, prompt_suffix) < 0
		    || !session_present_timed_paged_row(session, (const uint8_t *)prompt,
		    strlen(prompt), "Earth item prompt", error)
		    || !session_read_upper_command(session, line, sizeof(line)))
			return false;
		if (line[0] == '\0')
			continue;
		parsed = qb_val(line);
		if (parsed.overflow)
			return port_report_failure(error, "Earth menu VAL");
		{
			bool overflow;
			float selected = (float)(parsed.valid ? parsed.value : 0.0);

			choice = (int)qb_cint(selected, &overflow);
			if (overflow)
				return port_report_failure(error,
				    "Earth menu CINT");
		}
		if (strcmp(line, "S") == 0) {
			if (!display_sector(session, true, error)
			    || !session_wait(session, 9.0,
			    "Earth Sensors wait", error))
				return false;
			continue;
		}
		if (strcmp(line, "I") == 0) {
			if (!show_ship(session, error)
			    || !session_wait(session, 9.0,
			    "Earth Info wait", error))
				return false;
			continue;
		}
		if (choice < 1 || choice > 9) {
			int position;

			session->earth_report_seen = false;
			position = yt_earth_selector_position(line);
			if (position == 0) {
				if (!session_present_alert(session, invalid,
				    sizeof(invalid) - 1U,
				    "Earth invalid choice", error))
					return false;
				continue;
			}
			switch (position) {
			case 1: {
				bool selected = false;

				if (!command_land(session, &selected, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = selected;
				return true;
			}
			case 2: {
				bool moved;

				if (!command_move(session, &moved, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = moved;
				return true;
			}
			case 3:
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 4: {
				bool selected = false;

				if (!computer_menu(session, &selected, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = selected;
				return true;
			}
			default:
				return false;
			}
		}
		if (choice == 4) {
			if (!lottery(session, &earth, error))
				return false;
			continue;
		}
		if (choice == 5) {
			if (!earth_purchase_scanner(session, &earth, 500000.0f,
			    error))
				return false;
			continue;
		}
		if (choice == 6) {
			static const uint8_t confirmation[] =
			    "Anti-Cloaking Device works for this logon only. "
			    "Buy one? [y/N]";
			static const uint8_t pause[] = "Hit [Enter]";
			enum yt_yes_no_answer answer;
			char response[80];

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak leading blank",
			    error))
				return false;
			if (session->player.credits < 1000000000.0f) {
				if (!earth_credit_error(session,
				    "You do not have enough credits!", error))
					return false;
				continue;
			}
			if (!session_confirm(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer != YT_YES_NO_YES)
				continue;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak accepted blank",
			    error)
			    || !earth_anti_cloak(session, 1000000000.0f, error))
				return false;
			session_enable_anti_cloak(session);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak pause blank", error)
			    || !session_present_timed_paged_row(session, pause, sizeof(pause) - 1U,
			    "Earth Anti-Cloak pause prompt", error)
			    || !session_read_command(session, response, sizeof(response)))
				return false;
			continue;
		}
		if (choice == 9) {
			if (!earth_purchase_spies(session, &earth, 1000000000.0f,
			    error))
				return false;
			continue;
		}
		if (choice == 1) {
			if (!earth_purchase_cloak(session, &earth, error))
				return false;
		}
		else if (choice == 2) {
			if (!earth_purchase_holds(session, &earth, holds_price, error))
				return false;
		}
		else if (choice == 3 || choice == 7 || choice == 8) {
			float selected_price = choice == 3 ? fighters_price
			    : choice == 7 ? ground_price : shields_price;

			if (!earth_purchase_supply(session, &earth, choice,
			    selected_price, error))
				return false;
		}
	}
}

static float *
player_item(struct yt_player *player, int item)
{
	switch (item) {
	case 1: return &player->ore;
	case 2: return &player->organics;
	case 3: return &player->equipment;
	case 4: return &player->fighters;
	case 5: return &player->missiles;
	case 6: return &player->mines;
	case 9: return &player->plasma;
	default: return NULL;
	}
}

static bool
planet_inventory(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	struct planet_update_cache cache;
	static const char *labels[9] = {
		"Ore..........", "Organics.....", "Equipment....",
		"Fighters.....", "Missiles.....", "Mines........",
		"Credits......", "Forces.......", "Plasma bolts."
	};
	static const uint8_t header[] =
	    " Item           Production     Amount    In Holds";
	static const uint8_t rule[] =
	    "=============  ============   ========  ==========";
	double held[9];
	uint8_t title[64];
	size_t title_length = 0;
	size_t name_length;
	int index;

	if (!session_reload_player(session, error)
	    || !planet_update_cached(session, logical_planet, &planet, &cache,
	    error)
	    || !session_read_planet(session, logical_planet,
	    &planet, error)
	    || !port_report_length(session, planet.name_length,
	    YT_TEXT_FIELD_SIZE, &name_length, "planet inventory name length",
	    error))
		return false;
	snprintf(session->planet_name, sizeof(session->planet_name), "%s",
	    planet.name);
	held[0] = (double)session->player.ore;
	held[1] = (double)session->player.organics;
	held[2] = (double)session->player.equipment;
	held[3] = (double)session->player.fighters;
	held[4] = (double)session->player.missiles;
	held[5] = (double)session->player.mines;
	held[6] = (double)session->player.credits;
	held[7] = (double)session->player.ground_forces;
	held[8] = (double)session->player.plasma;
	memcpy(title, "Planet: ", strlen("Planet: "));
	title_length = strlen("Planet: ");
	memcpy(title + title_length, planet.record.bytes, name_length);
	title_length += name_length;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet inventory title blank", error)
	    || !session_present_paged_fragment(session, title, title_length)
	    || !session_present_paged_line(session, header, sizeof(header) - 1U,
	    "planet inventory header", error)
	    || !session_present_paged_fragment(session, rule, sizeof(rule) - 1U))
		return false;
	for (index = 0; index < 9; ++index) {
		char production[64];
		char amount[64];
		char in_holds[64];
		double produced;
		double available;

		if (index < 6) {
			produced = (double)floorf(cache.rate[index + 1]);
			available = floor(cache.quantity[index + 1]);
			if (qb_str_single(production, sizeof(production),
			    (float)produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_double(in_holds, sizeof(in_holds),
			    held[index]) < 0)
				return port_report_failure(error,
				    "planet inventory numeric format");
		}
		else if (index == 6) {
			produced = floor(double_mul(cache.quantity[7],
			    0x1.47ae14p-7));
			available = floor(cache.quantity[7]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_double(in_holds, sizeof(in_holds), held[index]) < 0)
				return port_report_failure(error,
				    "planet inventory credit format");
		}
		else if (index == 7) {
			produced = floor(double_add(double_mul(cache.quantity[8],
			    0x1.47ae14p-7), (double)cache.contribution[8]));
			available = floor(cache.quantity[8]);
			if (qb_str_double(production, sizeof(production), produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_single(in_holds, sizeof(in_holds),
			    (float)held[index]) < 0)
				return port_report_failure(error,
				    "planet inventory force format");
		}
		else {
			produced = (double)floorf(cache.rate[9]);
			available = floor(cache.quantity[9]);
			if (qb_str_single(production, sizeof(production),
			    (float)produced) < 0
			    || qb_str_double(amount, sizeof(amount), available) < 0
			    || qb_str_single(in_holds, sizeof(in_holds),
			    (float)held[index]) < 0)
				return port_report_failure(error,
				    "planet inventory plasma format");
		}
		if (!session_present_text(session, (const uint8_t *)labels[index],
		    strlen(labels[index]), SESSION_PRESENT_RAW,
		    "planet inventory label", error)
		    || !session_right_aligned(session, production, 13.0f,
		    "planet inventory production", error)
		    || !session_right_aligned(session, amount, 11.0f,
		    "planet inventory amount", error)
		    || !session_right_aligned(session, in_holds, 12.0f,
		    "planet inventory holds", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet inventory row ending", error))
			return false;
	}
	return true;
}

static bool
planet_take_one(struct yt_session *session, int logical_planet, int item,
    struct yt_error *error)
{
	struct yt_planet planet;
	const char *title = yt_planet_take_one_title(item);
	float free_holds;
	float available;
	float maximum;
	float quantity;
	char maximum_text[64];
	char prompt[100];
	char response[160];
	struct qb_val_result parsed;

	if (title == NULL)
		return port_report_failure(error, "planet Take One item");
	if (!session_present_paged_line(session, (const uint8_t *)title,
	    strlen(title), "planet Take One title", error))
		return false;
	free_holds = (float)double_sub(double_sub(double_sub(
	    (double)session->player.holds, (double)session->player.ore),
	    (double)session->player.organics),
	    (double)session->player.equipment);
	available = (float)floor(session->planet_quantity[item]);
	maximum = item <= 3 && free_holds < available
	    ? free_holds : available;
	if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0
	    || snprintf(prompt, sizeof(prompt), "How much [%s ]? ",
	    maximum_text) < 0)
		return port_report_failure(error, "planet Take One prompt format");
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Take One prompt blank", error)
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Take One amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		quantity = maximum;
	else {
		parsed = qb_val(response);
		quantity = (float)floor(parsed.valid ? parsed.value : 0.0);
	}
	if ((double)quantity > floor(session->planet_quantity[item])
	    || quantity < 0.0f) {
		static const uint8_t stock[] = "They don't have that many.";

		return session_present_alert(session, stock, sizeof(stock) - 1U,
		    "planet Take One stock error", error);
	}
	if (quantity > maximum) {
		static const uint8_t capacity[] = "You can't take that much!";

		return session_present_alert(session, capacity, sizeof(capacity) - 1U,
		    "planet Take One capacity error", error);
	}
	if (!session_reload_player(session, error))
		return false;
	yt_planet_take_one_player_overlay(&session->player, item, quantity);
	if (!write_player(session, error))
		return false;
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_one_planet_overlay(&planet, item,
	    session->planet_quantity[item], quantity);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	session->planet_quantity[item] = double_sub(
	    session->planet_quantity[item], (double)quantity);
	return session_reload_player(session, error);
}

static bool
planet_take_all(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	static const uint8_t title[] = "<Take all>";
	static const uint8_t taking[] = "Taking:";
	static const char *const weapon_labels[4] = {
		"Fighters.....", "Missiles.....", "Mines........",
		"Plasma bolts."
	};
	static const int weapon_items[4] = {4, 5, 6, 9};
	static const char *const commodity_labels[3] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	double amount[10];
	int index;

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet take-all title", error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_take_all_weapon_player_overlay(&session->player,
	    session->planet_quantity, amount);
	if (!write_player(session, error)
	    || !session_present_paged_line(session, taking, sizeof(taking) - 1U,
	    "planet take-all taking", error))
		return false;
	for (index = 0; index < 4; ++index) {
		char number[64];
		char row[96];
		int item = weapon_items[index];

		if ((index == 0
		    ? qb_str_double(number, sizeof(number), amount[item])
		    : qb_str_single(number, sizeof(number), (float)amount[item])) < 0
		    || snprintf(row, sizeof(row), "%s%s", weapon_labels[index],
		    number) < 0)
			return port_report_failure(error,
			    "planet take-all weapon format");
		if (index == 0) {
			if (!session_present_paged_line(session, (const uint8_t *)row,
			    strlen(row), "planet take-all fighters", error))
				return false;
		}
		else if (!session_present_paged_fragment(session, (const uint8_t *)row,
		    strlen(row)))
			return false;
	}
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_all_weapon_planet_overlay(&planet,
	    session->planet_quantity, amount);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	for (index = 3; index >= 1; --index) {
		char number[64];
		char row[96];
		float commodity_amount;

		if (!session_reload_player(session, error))
			return false;
		commodity_amount = yt_planet_take_all_commodity_player_overlay(
		    &session->player, index, session->planet_quantity[index]);
		if (!write_player(session, error))
			return false;
		if (!session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_take_all_commodity_planet_overlay(&planet, index,
		    session->planet_quantity[index], commodity_amount);
		if (!session_write_planet(session,
		    logical_planet, &planet, error))
			return false;
		if (qb_str_single(number, sizeof(number), commodity_amount) < 0
		    || snprintf(row, sizeof(row), "%s%s",
		    commodity_labels[index - 1], number) < 0)
			return port_report_failure(error,
			    "planet take-all commodity format");
		if (!session_present_paged_fragment(session, (const uint8_t *)row, strlen(row)))
			return false;
	}
	return true;
}

static bool
planet_garrison(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t insufficient[] = "Insuficient forces!";
	struct yt_planet planet;
	struct qb_val_result parsed;
	char response[160];
	uint8_t prompt[160];
	uint8_t success[128];
	size_t prompt_length;
	size_t success_length;
	float old_garrison;
	float desired;
	float after;

	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	old_garrison = planet.ground_forces;
	if (!session_reload_player(session, error))
		return false;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet garrison opening blank", error)
	    || !yt_planet_garrison_prompt(session->player.ground_forces,
	    old_garrison, prompt, sizeof(prompt), &prompt_length)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "planet garrison prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	desired = (float)floor(parsed.valid ? parsed.value : 0.0);
	after = yt_planet_garrison_after(session->player.ground_forces,
	    desired, old_garrison);
	if (desired < 0.0f || after < 0.0f)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U, "planet garrison refusal", error);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_garrison_overlay(&planet, desired, 0);
	if (desired >= 1.0f) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet garrison success blank", error)
		    || !yt_planet_garrison_success_row(desired, success,
		    sizeof(success), &success_length))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_paged_fragment(session, success, success_length))
			return false;
		planet.owner = (float)session_record(session);
		if (!yt_record_set_number(&planet.record, YT_F73, planet.owner)
		    || !session_sound(session, 4.0f,
		    "planet garrison sound", error))
			return false;
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_planet_basic_record(session, (float)logical_planet),
	    &planet.record, error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_garrison_player_overlay(&session->player, after);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}

static bool
planet_bank(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t savings[] =
	    "We are a SAVINGS not a LOAN institution!";
	static const uint8_t insufficient[] =
	    "You don't have that many Credits!";
	static const uint8_t farewell[] = "Have a nice day!";
	struct yt_planet planet;
	struct qb_val_result parsed;
	char title[160];
	char available_text[64];
	char prompt[192];
	char response[160];
	char amount_text[64];
	char success[192];
	double target;
	double available;
	double remaining;
	float old_bank;
	float credit_argument;

	session_set_foreground(session, 6.0f);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	old_bank = planet.bank;
	available = yt_planet_bank_available(session->player.credits, old_bank);
	if (snprintf(title, sizeof(title),
	    "Welcome to the intergalactic bank of %s!",
	    session->planet_name) < 0
	    || qb_str_double(available_text, sizeof(available_text), available) < 0
	    || snprintf(prompt, sizeof(prompt),
	    "How many credits do you want in the account?%s Available ->",
	    available_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)title, strlen(title),
	    "planet Bank title", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Bank pre-prompt blank", error)
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Bank amount prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow)
		return port_report_failure(error, "planet Bank amount VAL");
	target = qb_int(parsed.valid ? parsed.value : 0.0);
	if (target < 0.0)
		return session_present_alert(session, savings, sizeof(savings) - 1U,
		    "planet Bank savings error", error);
	remaining = yt_planet_bank_remaining(session->player.credits, old_bank,
	    target);
	if (remaining < 0.0)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Bank credit error", error);
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_bank_planet_overlay(&planet, target);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	session->player.credits = (float)remaining;
	if (target != 0.0) {
		if (qb_str_double(amount_text, sizeof(amount_text), target) < 0
		    || snprintf(success, sizeof(success),
		    "You have%s credits on deposit at 1%% interest. %s",
		    amount_text, farewell) < 0)
			return port_report_failure(error,
			    "planet Bank success format");
	}
	else {
		memcpy(success, farewell, sizeof(farewell));
	}
	if (!session_present_paged_line(session, (const uint8_t *)success, strlen(success),
	    "planet Bank accepted", error)
	    || !session_sound(session, 4.0f, "planet bank sound", error))
		return false;
	credit_argument = yt_planet_bank_credit_argument(old_bank, target);
	return session_mutate_player_credits(session, credit_argument, NULL,
	    error);
}

static bool
planet_rename(struct yt_session *session, int logical_planet, bool *renamed,
    struct yt_error *error)
{
	static const uint8_t protected[] =
	    "You can't re-name this planet!";
	static const uint8_t prompt[] =
	    "What do you want to name this planet? -=>";
	static const uint8_t reserved[] = "I Don't think so!";
	static const uint8_t confirmation_suffix[] =
	    " Is this OK? (Y/n) [Y] ?";
	struct yt_planet planet;
	char name[YT_COMMAND_SIZE];
	uint8_t confirmation[2U + 41U + sizeof(confirmation_suffix) - 1U];
	float current_record;
	bool written;

	if (renamed != NULL)
		*renamed = false;

	for (;;) {
		enum yt_yes_no_answer answer;
		enum yt_planet_rename_name_result name_result;
		size_t name_length;
		size_t confirmation_length = 0;

		current_record = single_add(
		    session_planet_offset(session),
		    (float)logical_planet);
		if (yt_planet_rename_protected(current_record,
		    session_planet_offset(session),
		    session->door->game.config.total_records))
			return session_present_alert(session, protected,
			    sizeof(protected) - 1U,
			    "planet Rename protected", error);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Rename leading blank", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "planet Rename name prompt", error)
		    || !session_read_command(session, name, sizeof(name)))
			return false;
		name_result = yt_planet_rename_prepare_name(name, &name_length);
		if (name_result == YT_PLANET_RENAME_EMPTY)
			return true;
		if (name_result == YT_PLANET_RENAME_RESERVED)
			return session_present_alert(session, reserved,
			    sizeof(reserved) - 1U, "planet Rename reserved", error);
		confirmation[confirmation_length++] = '"';
		memcpy(confirmation + confirmation_length, name, name_length);
		confirmation_length += name_length;
		confirmation[confirmation_length++] = '"';
		memcpy(confirmation + confirmation_length, confirmation_suffix,
		    sizeof(confirmation_suffix) - 1U);
		confirmation_length += sizeof(confirmation_suffix) - 1U;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Rename confirmation blank", error)
		    || !session_confirm(session, confirmation, confirmation_length,
		    &answer, error))
			return false;
		if (answer != YT_YES_NO_NO)
			break;
	}
	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_rename_overlay(&planet, name, strlen(name));
	snprintf(session->planet_name, sizeof(session->planet_name), "%s", name);
	written = session_write_planet(session, logical_planet,
	    &planet, error);
	if (written && renamed != NULL)
		*renamed = true;
	return written;
}

static bool
planet_transfer(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Transfer items to planet>";
	static const uint8_t question[] = "Transfer which item?";
	static const uint8_t plasma_row[] = "[B] Plasma Bolts";
	static const uint8_t cargo_row[] = "[C] Cargo";
	static const uint8_t fighter_row[] = "[F] Fighters";
	static const uint8_t missile_row[] = "[S] Missiles";
	static const uint8_t mine_row[] = "[M] Mines";
	static const uint8_t selector_prompt[] = "-=>";
	struct yt_planet planet;
	char command[80];

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet Transfer title", error)
	    || !session_present_paged_line(session, question, sizeof(question) - 1U,
	    "planet Transfer question", error)
	    || !session_present_paged_line(session, plasma_row, sizeof(plasma_row) - 1U,
	    "planet Transfer plasma row", error)
	    || !session_present_paged_fragment(session, cargo_row, sizeof(cargo_row) - 1U)
	    || !session_present_paged_fragment(session, fighter_row, sizeof(fighter_row) - 1U)
	    || !session_present_paged_fragment(session, missile_row, sizeof(missile_row) - 1U)
	    || !session_present_paged_fragment(session, mine_row, sizeof(mine_row) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Transfer selector blank", error)
	    || !session_present_timed_paged_row(session, selector_prompt,
	    sizeof(selector_prompt) - 1U, "planet Transfer selector", error)
	    || !session_read_upper_command(session, command, sizeof(command)))
		return false;
	if (command[0] == '\0')
		return true;
	if (yt_planet_transfer_selector_position(command) == 0)
		return true;
	if (strcmp(command, "C") == 0) {
		struct planet_update_cache cache;
		double held[3];
		size_t index;

		if (!session_reload_player(session, error))
			return false;
		held[0] = (double)session->player.ore;
		held[1] = (double)session->player.organics;
		held[2] = (double)session->player.equipment;
		if (!planet_update_cached(session, logical_planet, &planet,
		    &cache, error))
			return false;
		if (yt_planet_transfer_cargo_empty(held)) {
			static const uint8_t empty[] =
			    "You don't have any cargo!";

			return session_present_alert(session, empty, sizeof(empty) - 1U,
			    "planet Transfer no cargo", error);
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer cargo blank", error))
			return false;
		yt_planet_transfer_cargo_cache(cache.rate, cache.quantity, held);
		for (index = 0; index < 3; ++index) {
			int item = (int)index + 1;

			session->planet_quantity[item] = cache.quantity[item];
		}
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_cargo_player_overlay(&session->player);
		if (!write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_cargo_planet_overlay(&planet, cache.rate,
		    cache.quantity, cache.contribution);
		if (!session_write_planet(session,
		    logical_planet, &planet, error))
			return false;
		{
			static const uint8_t success[] = "Cargo transferred!!";

			if (!session_present_paged_fragment(session, success, sizeof(success) - 1U))
				return false;
		}
	}
	else if (strcmp(command, "F") == 0) {
		char number[64];
		char prompt[160];
		char response[160];
		float cached_fighters = session->player.fighters;
		float amount;

		if (qb_str_double(number, sizeof(number),
		    (double)cached_fighters) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "You have%s fighters. Transfer how many -=>", number) < 0)
			return port_report_failure(error,
			    "planet Transfer fighter prompt format");
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer fighter blank", error)
		    || !session_present_timed_paged_row(session, (const uint8_t *)prompt,
		    strlen(prompt), "planet Transfer fighter prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_planet_transfer_fighter_amount(response, &amount, error))
			return false;
		if (yt_planet_transfer_fighter_rejected(amount, cached_fighters))
			return true;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_fighter_player_overlay(&session->player,
		    cached_fighters, amount);
		if (!write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_fighter_planet_overlay(&planet,
		    session->planet_quantity[4], amount);
		if (!session_write_planet(session, logical_planet,
		    &planet, error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer fighter success blank",
		    error))
			return false;
		yt_present_set_blink(&session->presentation, 1.0f);
		{
			static const uint8_t success[] = "Fighters Transferred!";

			if (!session_present_paged_fragment(session, success, sizeof(success) - 1U))
				return false;
		}
	}
	else if (strcmp(command, "B") == 0
	    || strcmp(command, "S") == 0
	    || strcmp(command, "M") == 0) {
		float *held;
		float amount;
		int item = command[0] == 'B' ? 9 : command[0] == 'S' ? 5 : 6;
		const uint8_t *success;
		size_t success_length;

		held = player_item(&session->player, item);
		amount = *held;
		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &session->player, error))
			return false;
		yt_planet_transfer_direct_player_overlay(&session->player, item);
		if (!write_player(session, error)
		    || !session_read_planet(session,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_direct_planet_overlay(&planet, item,
		    session->planet_quantity[item], amount);
		if (!session_write_planet(session, logical_planet,
		    &planet, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer weapon success blank",
		    error))
			return false;
		yt_present_set_blink(&session->presentation, 1.0f);
		if (item == 9) {
			static const uint8_t text[] = "Plasma Bolts Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		else if (item == 5) {
			static const uint8_t text[] = "Missiles Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		else {
			static const uint8_t text[] = "Mines Transferred!";

			success = text;
			success_length = sizeof(text) - 1U;
		}
		if (!session_present_paged_fragment(session, success, success_length))
			return false;
	}
	if (!session_reload_player(session, error)
	    || !planet_update(session, logical_planet, &planet, error))
		return false;
	return session_sound(session, 4.0f,
	    "planet transfer sound", error);
}

static bool
planet_productivity(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	static const uint8_t explanation[] =
	    "Productivity is increased by 1 Unit of EQU, ORG  & ORE "
	    "for each 250 credits.";
	static const uint8_t prompt[] =
	    "Spend how much to raise productivity? -+> ";
	static const uint8_t insufficient[] =
	    "You dont have that many credits!";
	static const char *const fragments[4] = {
		"Also increased: Fighters:", ", Missiles:",
		", Mines:", ", Plasma Bolts:"
	};
	struct planet_update_cache cache;
	struct yt_planet planet;
	struct qb_val_result parsed;
	char response[160];
	char credits_text[64];
	char credits_row[128];
	char units_text[64];
	char success[160];
	double spend;
	double units;
	float delta[4];
	float credit_argument;
	size_t index;

	if (!session_reload_player(session, error)
	    || !planet_update_cached(session, logical_planet, &planet, &cache,
	    error))
		return false;
	if (qb_str_double(credits_text, sizeof(credits_text),
	    (double)session->player.credits) < 0
	    || snprintf(credits_row, sizeof(credits_row),
	    "You have%s Credits.", credits_text) < 0
	    || !session_present_paged_line(session, explanation, sizeof(explanation) - 1U,
	    "planet Productivity explanation", error)
	    || !session_present_paged_line(session, (const uint8_t *)credits_row,
	    strlen(credits_row), "planet Productivity credits", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity pre-prompt blank", error)
	    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "planet Productivity spend prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return port_report_failure(error,
		    "planet Productivity amount VAL");
	spend = qb_int(parsed.valid ? parsed.value : 0.0);
	if (spend < 1.0)
		return true;
	if (spend > (double)session->player.credits)
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Productivity credit error", error);
	units = yt_planet_productivity_units(spend);
	if (qb_str_double(units_text, sizeof(units_text), units) < 0
	    || snprintf(success, sizeof(success),
	    "Productivity increased by%s units of ORE, ORG & EQU!",
	    units_text) < 0
	    || !session_present_paged_line(session, (const uint8_t *)success,
	    strlen(success), "planet Productivity accepted", error))
		return false;
	yt_planet_productivity_cache(cache.rate, units, delta);
	for (index = 0; index < 4U; ++index) {
		char delta_text[64];
		char fragment[128];

		if (delta[index] == 0.0f)
			continue;
		if (qb_str_single(delta_text, sizeof(delta_text), delta[index]) < 0
		    || snprintf(fragment, sizeof(fragment), "%s%s",
		    fragments[index], delta_text) < 0
		    || !session_present_timed_paged_row(session, (const uint8_t *)fragment,
		    strlen(fragment), "planet Productivity derived fragment",
		    error))
			return false;
	}
	if (delta[0] != 0.0f
	    && !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity derived ending", error))
		return false;
	credit_argument = yt_planet_productivity_credit_argument(units);
	if (!session_mutate_player_credits(session, credit_argument, NULL, error)
	    || !session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_productivity_planet_overlay(&planet, cache.rate,
	    cache.quantity, cache.contribution);
	if (!session_write_planet(session, logical_planet,
	    &planet, error))
		return false;
	return session_reload_player(session, error);
}

static bool
planet_assault(struct yt_session *session, uint32_t physical_planet,
    float commitment, bool *defeated, struct yt_error *error)
{
	static const uint8_t engaging[] = "Forces engaging!";
	static const uint8_t defenses[] = "Planetary defenses destroyed!";
	static const uint8_t defenses_news[] =
	    " +++ Planetary defenses destroyed!";
	static const uint8_t captured[] = "You've captured the planet!";
	struct yt_planet planet;
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[320];
	size_t player_name_length;
	size_t planet_name_length;
	size_t row_length;
	float attackers = commitment;
	float defenders;
	float saved_foreground;

	if (defeated == NULL)
		return false;
	*defeated = false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault entry blank", error))
		return false;
	saved_foreground = session_foreground(session);
	if (!planet_update_cached_physical(session, physical_planet, &planet,
	    NULL, error)
	    || !read_planet_physical(session, physical_planet, &planet, error)
	    || !yt_planet_stored_name(&planet, planet_name,
	    &planet_name_length, error)
	    || !session_reload_player(session, error))
		return false;
	defenders = floorf(planet.ground_forces);
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	yt_planet_assault_player_overlay(&session->player, commitment);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_planet_assault_attack_news(player_name, player_name_length,
	    planet_name, planet_name_length, commitment, row, sizeof(row),
	    &row_length)
	    || !session_append_news_bytes(session, row, row_length, error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, engaging, sizeof(engaging) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "planet assault engagement row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault engagement blank", error)
	    || !session_sound(session, 2.0f,
	    "planet assault engagement sound", error))
		return false;
	while (attackers > 0.0f && defenders > 0.0f) {
		float side;
		float amount;
		bool attacker_damage;

		if (!random_value(session, &side, error))
			return false;
		attacker_damage = side > 0.4000000059604645f;
		if (!random_value(session, &amount, error))
			return false;
		yt_planet_assault_round(attacker_damage, amount, &attackers,
		    &defenders);
		session_set_foreground(session, attacker_damage ? 3.0f : 4.0f);
		if (!yt_planet_assault_status_row(attacker_damage,
		    attacker_damage ? attackers : defenders, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet assault force-status row", error))
			return false;
		if (!attacker_damage
		    && !session_sound(session, 2.0f,
			    "planet assault defender sound", error))
			return false;
	}
	session_set_foreground(session, saved_foreground);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault terminal blank", error))
		return false;
	if (defenders <= 0.0f) {
		float owner = 0.0f;

		if (!session_present_text(session, defenses,
		    sizeof(defenses) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "planet assault defenses-destroyed row", error)
		    || !session_append_news_bytes(session, defenses_news,
		    sizeof(defenses_news) - 1U, error)
		    || !session_sound(session, 1.0f,
		    "planet defenses destroyed sound", error))
			return false;
		if (attackers > 0.0f) {
			owner = (float)session_record(session);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "planet assault capture blank", error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, captured,
			    sizeof(captured) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet assault capture row", error)
			    || !yt_planet_assault_capture_news(player_name,
			    player_name_length, planet_name, planet_name_length,
			    row, sizeof(row), &row_length)
			    || !session_append_news_bytes(session, row, row_length, error)
			    || !session_sound(session, 1.0f,
			    "planet capture sound", error))
				return false;
		}
		else
			attackers = 0.0f;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
		yt_planet_assault_victory_overlay(&planet, owner, attackers);
		return write_planet_physical(session, physical_planet, &planet,
		    false, error);
	}
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	yt_planet_assault_failure_overlay(&planet, defenders);
	if (!write_planet_physical(session, physical_planet, &planet, false,
	    error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!yt_planet_assault_failure_row(defenders, true, row, sizeof(row),
	    &row_length)
	    || !session_append_news_bytes(session, row, row_length, error)
	    || !yt_planet_assault_failure_row(defenders, false, row,
	    sizeof(row), &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_BOLD_LINE, "planet assault failure row", error))
		return false;
	*defeated = true;
	return true;
}

static bool
route_sector_reader(void *context, int logical_sector, float warps[6],
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_sector sector;

	if (!read_sector_at_fault(session, logical_sector, &sector,
	    YT_BASIC_FAULT_ROUTE_SECTOR_GET, error))
		return false;
	memcpy(warps, sector.warps, sizeof(sector.warps));
	return true;
}

static void
route_require_returned(enum yt_route_outcome outcome)
{
	if (outcome == YT_ROUTE_BACK_EDGE)
		yt_route_reconstruction_back_edge();
}

static bool
build_route(struct yt_session *session, float start, float destination,
    int16_t *next_hop, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error)
{
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;
	size_t index;

	success = yt_route_build(start, destination, &status,
	    session->route_avoid, session->presentation.sound.conversion_mode,
	    session->route_predecessor, session->route_second,
	    route_sector_reader, session, &outcome, error);
	if (next_hop != NULL)
		for (index = 0U; index < YT_ROUTE_CAPACITY; ++index)
			next_hop[index] = session->route_second[index];
	if (!success)
		return false;
	route_require_returned(outcome);
	*found = outcome == YT_ROUTE_FOUND || outcome == YT_ROUTE_SAME;
	if (route_outcome != NULL)
		*route_outcome = outcome;
	if (returned_status != NULL)
		*returned_status = status;
	return true;
}

static void
projectile_route_store(struct projectile_route_state *route, float origin,
    float destination, float missiles)
{
	route->origin = origin;
	route->destination = destination;
	route->amount = missiles;
}

static void
projectile_route_store_raw(struct projectile_route_state *route,
    const uint8_t origin[4],
    const uint8_t destination[4], const uint8_t missiles[4])
{
	projectile_route_store(route, qb_mbf32_decode(origin),
	    qb_mbf32_decode(destination), qb_mbf32_decode(missiles));
}

static void
projectile_route_load(const struct projectile_route_state *route, float *origin,
    float *destination, float *missiles)
{
	*origin = route->origin;
	*destination = route->destination;
	*missiles = route->amount;
}

static bool
build_projectile_route(struct yt_session *session,
    const struct projectile_route_state *route, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error)
{
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;

	success = yt_route_build(route->origin, route->destination, &status,
	    session->route_avoid, session->presentation.sound.conversion_mode,
	    session->route_predecessor, session->route_second,
	    route_sector_reader, session, &outcome, error);
	if (!success)
		return false;
	route_require_returned(outcome);
	*found = outcome == YT_ROUTE_FOUND || outcome == YT_ROUTE_SAME;
	if (route_outcome != NULL)
		*route_outcome = outcome;
	if (returned_status != NULL)
		*returned_status = status;
	return true;
}

static bool
planet_move_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;
	uint32_t owner_record;
	int last_player = (int)session_sector_offset(session);

	if (friendly == NULL)
		return false;
	*friendly = false;
	session_set_relationship(session, 0.0f);
	if (owner < 2.0f || owner > (float)last_player
	    || session_record(session) < 2 || session_record(session) > last_player)
		return true;
	if (owner == (float)session_record(session)) {
		*friendly = true;
		session_set_relationship(session, -1.0f);
		return true;
	}
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	owner_record = qb_brun_random_record_number(owner);
	if (owner_record > (uint32_t)INT_MAX
	    || !yt_game_read_player(&session->door->game, (int)owner_record,
	    &other, error))
		return false;
	*friendly = other.team == current.team;
	if (*friendly)
		session_set_relationship(session, -1.0f);
	return true;
}

static bool
planet_move_hop(struct yt_session *session, int source_number,
    int destination, bool final_hop, bool *stop, struct yt_error *error)
{
	static const uint8_t xannor_prefix[] =
	    "No way! We don't want no trouble from no Xannor ";
	static const uint8_t xannor_slogan[] =
	    "(Xannoron Movers, We're MOVEers not FIGHTers.)";
	static const uint8_t occupied[] =
	    "There is already a planet in that sector!";
	static const uint8_t wanderer[] =
	    "The Wanderer vanishes from your sensors!";
	struct yt_sector source;
	struct yt_sector target;
	struct yt_planet planet;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[512];
	size_t planet_name_length;
	size_t player_name_length;
	size_t row_length;
	float source_link;
	float moving_planet;
	float actual_destination = (float)destination;
	float draw;
	float xannor_planet = single_sub(
	    session->door->game.config.total_records,
	    session_planet_offset(session));
	uint32_t moving_record;
	int source_record;
	int destination_record;

	if (stop == NULL)
		return false;
	*stop = false;
	if (!session_read_sector(session, source_number, &source,
	    error))
		return false;
	if (source.planet == xannor_planet) {
		size_t first_name_length = strlen(session->door->identity.real_first);

		if (sizeof(xannor_prefix) - 1U + first_name_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, xannor_prefix, sizeof(xannor_prefix) - 1U);
		memcpy(row + sizeof(xannor_prefix) - 1U,
		    session->door->identity.real_first, first_name_length);
		row[sizeof(xannor_prefix) - 1U + first_name_length] = '!';
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move Xannor blank", error)
		    || !session_present_text(session, row,
		    sizeof(xannor_prefix) + first_name_length,
		    SESSION_PRESENT_LINE, "planet move Xannor refusal", error)
		    || !session_present_text(session, xannor_slogan,
		    sizeof(xannor_slogan) - 1U, SESSION_PRESENT_LINE,
		    "planet move Xannor slogan", error))
			return false;
		*stop = true;
		return true;
	}
	if (!session_read_sector(session, destination, &target,
	    error))
		return false;
	if (target.planet > 0.0f) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move occupied blank", error)
		    || !session_present_text(session, occupied,
		    sizeof(occupied) - 1U, SESSION_PRESENT_LINE,
		    "planet move occupied row", error))
			return false;
		*stop = true;
	}
	if (!session_read_sector(session, source_number, &source,
	    error))
		return false;
	source_link = source.planet;
	moving_record = session_planet_basic_record(session, source_link);
	moving_planet = single_sub(single_add(
	    session_planet_offset(session), source_link),
	    session_planet_offset(session));
	yt_planet_move_sector_overlay(&source, 0.0f);
	source_record = (int)session_sector_basic_record(session,
	    (float)source_number);
	if (!yt_database_write(
	    &session->door->game.database, (size_t)source_record,
	    &source.record, error)
	    || !read_planet_physical(session, moving_record, &planet, error)
	    || !yt_planet_stored_name(&planet, planet_name,
	    &planet_name_length, error)
	    || !random_value(session, &draw, error))
		return false;
	if (draw > 0.9950000047683716f || *stop) {
		float loss = 0.0f;

		yt_planet_move_explosion_overlay(&planet);
		if (!write_planet_physical(session, moving_record, &planet, false,
		    error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move explosion first blank", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move explosion second blank", error)
		    || !yt_planet_move_explosion_row(planet_name,
		    planet_name_length, row, sizeof(row), &row_length))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "planet move explosion row", error)
		    || !yt_player_stored_name(&session->player, player_name,
		    &player_name_length, error)
		    || !yt_planet_move_explosion_news(planet_name,
		    planet_name_length, player_name, player_name_length,
		    row, sizeof(row), &row_length)
		    || !session_append_news_bytes(session, row, row_length, error)
		    || !session_sound(session, 3.0f,
		    "planet move explosion sound", error)
		    || !session_reload_player(session, error))
			return false;
		if (session->player.fighters != 0.0f) {
			float range = session->player.fighters;

			if (!yt_random_nested_single(
			    &session->door->game.random, 2.0f, &range, &loss,
			    error))
				return false;
		}
		yt_planet_move_fighter_overlay(&session->player, loss);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &session->player.record, error))
			return false;
		if (loss > 0.0f) {
			static const uint8_t you[] = "You";

			if (!yt_planet_move_loss_row(you, sizeof(you) - 1U,
			    loss, row, sizeof(row), &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "planet move fighter loss row", error)
			    || !yt_planet_move_loss_row(player_name,
			    player_name_length, loss, row, sizeof(row), &row_length)
			    || !session_append_news_bytes(session, row, row_length, error))
				return false;
			*stop = true;
		}
		return true;
	}
	if (moving_planet == 1.0f) {
		float maximum = single_sub(
		    session_port_offset(session),
		    session_sector_offset(session));

		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move Wanderer blank", error)
		    || !session_present_text(session, wanderer,
		    sizeof(wanderer) - 1U, SESSION_PRESENT_LINE,
		    "planet move Wanderer row", error))
			return false;
		*stop = true;
		for (;;) {
			if (!random_value(session, &draw, error))
				return false;
			actual_destination = floorf(single_mul(draw, maximum)) + 1.0f;
			if (!session_read_sector(session,
			    (int)actual_destination, &target, error))
				return false;
			if (target.planet <= 0.0f)
				break;
		}
	}
	else if (final_hop) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "planet move final first blank", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet move final second blank", error)
		    || !yt_planet_move_success_row(planet_name,
		    planet_name_length, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet move final row", error)
		    || !session_sound(session, 4.0f,
		    "planet move completion sound", error))
			return false;
	}
	if (!session_read_sector(session,
	    (int)actual_destination, &target, error))
		return false;
	if (target.mines != 0.0f)
		*stop = true;
	if (target.fighters != 0.0f) {
		bool friendly;

		if (!planet_move_friendship(session, target.fighter_owner,
		    &friendly, error))
			return false;
		if (!friendly)
			*stop = true;
		if (!session_read_sector(session,
		    (int)actual_destination, &target, error))
			return false;
	}
	yt_planet_move_sector_overlay(&target, moving_planet);
	destination_record = (int)session_sector_basic_record(session,
	    actual_destination);
	if (!yt_database_write(
	    &session->door->game.database, (size_t)destination_record,
	    &target.record, error)
	    || !session_reload_player(session, error))
		return false;
	yt_planet_move_success_overlay(&session->player, (float)destination);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_record(session), &session->player.record, error);
}

static bool
planet_move(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t cost_notice[] =
	    "Moving planets costs 10 turns per sector.";
	static const uint8_t destination_prompt[] =
	    "Move planet to what sector? ";
	static const uint8_t same_sector[] =
	    "Hey, look out the window dummy!";
	static const uint8_t range_prefix[] =
	    "Valid sector numbers are from 1 to";
	static const uint8_t working[] = "Working. ";
	static const uint8_t route_failure[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t insufficient[] =
	    "Not enough turns left to move the planet that far!";
	static const uint8_t confirmation[] = "Move the planet? (Y/[N])";
	static const uint8_t engaged[] = "Planet thrusters engaged.";
	static const uint8_t moving[] = "Moving to sector:";
	char response[160];
	char number[64];
	uint8_t row[512];
	size_t row_length;
	float start = session->player.sector;
	float destination;
	float maximum = yt_planet_move_maximum(
	    session_port_offset(session),
	    session_sector_offset(session));
	float cost = 0.0f;
	int start_node;
	int destination_node;
	int cursor;
	bool conversion_overflow;
	bool found;
	bool stop = false;
	bool final = false;
	enum yt_yes_no_answer answer;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!session_present_paged_line(session, cost_notice, sizeof(cost_notice) - 1U,
	    "planet Thrusters cost notice", error)
	    || !display_sector(session, false, error)
	    || !session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U,
	    "planet Thrusters destination prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	destination = yt_planet_move_destination(response);
	if (destination == start)
		return session_present_alert(session, same_sector,
		    sizeof(same_sector) - 1U, "planet Thrusters same-sector", error);
	if (destination < 1.0f || destination > maximum) {
		int number_length = qb_str_single(number, sizeof(number), maximum);

		if (number_length < 0
		    || sizeof(range_prefix) - 1U + (size_t)number_length + 1U
		    > sizeof(row))
			return false;
		memcpy(row, range_prefix, sizeof(range_prefix) - 1U);
		memcpy(row + sizeof(range_prefix) - 1U, number,
		    (size_t)number_length);
		row[sizeof(range_prefix) - 1U + (size_t)number_length] = '!';
		return session_present_alert(session, row,
		    sizeof(range_prefix) + (size_t)number_length,
		    "planet Thrusters range", error);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters working blank", error)
	    || !session_present_timed_paged_row(session, working, sizeof(working) - 1U,
	    "planet Thrusters working", error))
		return false;
	start_node = (int)qb_cint_mode((double)start,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet Thrusters start CINT");
		}
		return false;
	}
	destination_node = (int)qb_cint_mode((double)destination,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet Thrusters destination CINT");
		}
		return false;
	}
	if (!build_route(session, start, destination, NULL, true,
	    &found, NULL, NULL, error))
		return false;
	if (!found) {
		bool ok = session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route first blank", error)
		    && session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route second blank", error);

		yt_present_set_blink(&session->presentation, 1.0f);
		if (ok)
			ok = session_present_text(session, route_failure,
			    sizeof(route_failure) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet Thrusters route failure", error);
		return ok;
	}
	if (!yt_planet_move_path_heading(start, destination, row, sizeof(row),
	    &row_length)
	    || !session_present_paged_fragment(session, row, row_length)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route leading blank", error)
	    || qb_str_single(number, sizeof(number), start) < 0
	    || !session_present_timed_paged_row(session, (const uint8_t *)number, strlen(number),
	    "planet Thrusters route start", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = session->route_second[cursor];
		int column;
		int ignored_row;
		int number_length;

		if (next == 0)
			break;
		session_set_pager_line_count(session, 0.0f);
		number_length = qb_str_single(number, sizeof(number), (float)next);
		if (number_length < 0)
			return false;
		if (next != destination_node)
			number[number_length++] = ',';
		number[number_length] = '\0';
		if (!session_present_timed_paged_row(session, (const uint8_t *)number,
		    (size_t)number_length, "planet Thrusters route token", error))
			return false;
		yt_out_cursor_position(&ignored_row, &column);
		if (column > 74 && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route wrap", error))
			return false;
		cost = yt_planet_move_add_cost(cost);
		cursor = next;
	}
	session_set_pager_line_count(session, 0.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route ending", error)
	    || !yt_planet_move_summary(cost, row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "planet Thrusters distance summary", error)
	    || !computer_prompt_hydrate(session, error))
		return false;
	if (cost > session->player.turns) {
		return session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Thrusters insufficient turns", error);
	}
	if (!yt_planet_move_turns_row(session->player.turns, row, sizeof(row),
	    &row_length)
	    || !session_present_paged_fragment(session, row, row_length)
	    || !session_confirm(session, confirmation, sizeof(confirmation) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_paged_line(session, engaged, sizeof(engaged) - 1U,
	    "planet Thrusters engaged", error))
		return false;
	session_set_pager_line_count(session, 0.0f);
	if (!session_present_timed_paged_row(session, moving, sizeof(moving) - 1U,
	    "planet Thrusters moving prefix", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = session->route_second[cursor];
		int number_length;

		if (next == 0)
			break;
		number_length = qb_str_single(number, sizeof(number),
		    (float)next);
		if (number_length < 0 || !session_present_timed_paged_row(session,
		    (const uint8_t *)number, (size_t)number_length,
		    "planet Thrusters movement token", error))
			return false;
		if (next == destination_node)
			final = true;
		if (!planet_move_hop(session, cursor, next, final, &stop, error))
			return false;
		cursor = next;
		if (stop)
			break;
	}
	if (!stop && !computer_prompt_hydrate(session, error))
		return false;
	if (enter_sector != NULL)
		*enter_sector = true;
	return true;
}

static bool
planet_menu(struct yt_session *session, int logical_planet,
    bool *enter_sector, struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] =
	    "Planet command (?=help) [A]? ";

	session->planet_record_expression = single_add(
	    session_planet_offset(session), (float)logical_planet);
	for (;;) {
		char upper[80];
		char free_text[64];
		char free_row[160];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		double free_holds;
		int position;

		session_set_pager_line_count(session, 0.0f);
		if (!session_reload_player(session, error))
			return false;
		free_holds = double_sub(double_sub(double_sub(
		    (double)session->player.holds,
		    (double)session->player.ore),
		    (double)session->player.organics),
		    (double)session->player.equipment);
		if (qb_str_double(free_text, sizeof(free_text), free_holds) < 0
		    || snprintf(free_row, sizeof(free_row),
		    "You have%s free cargo holds.", free_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)free_row,
		    strlen(free_row), "planet free-holds row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet prompt framing blank", error))
			return false;
		session_set_foreground(session, 6.0f);
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "planet prompt time capacity");
			}
			return false;
		}
		memcpy(prompt + prompt_length, session->time.text,
		    session->time.text_length);
		prompt_length += session->time.text_length;
		memcpy(prompt + prompt_length, prompt_body,
		    sizeof(prompt_body) - 1U);
		prompt_length += sizeof(prompt_body) - 1U;
		if (!session_reload_player(session, error)
		    || !planet_update(session, logical_planet,
		    &(struct yt_planet){0}, error)
		    || !session_present_timed_paged_row(session, prompt, prompt_length,
		    "planet command prompt", error)
		    || !session_read_upper_command(session, upper, sizeof(upper)))
			return false;
		if (upper[0] == '\0')
			strcpy(upper, "A");
		if (strcmp(upper, "I") == 0) {
			if (!show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(upper, "N") == 0) {
			if (!planet_rename(session, logical_planet, NULL, error))
				return false;
			continue;
		}
		if (strcmp(upper, "S") == 0) {
			if (!display_sector(session, true, error))
				return false;
			continue;
		}
		if (strcmp(upper, "Q") == 0) {
			bool confirmed;

			if (!session_quit_confirm(session, &confirmed, error))
				return false;
			if (!confirmed)
				continue;
			if (!quit_session(session, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		if (strcmp(upper, "D") == 0) {
			if (!planet_inventory(session, logical_planet, error))
				return false;
			continue;
		}
		if (strcmp(upper, "?") == 0) {
			static const uint8_t heading[] = "<Help>";
			static const char *const rows[] = {
				"1 - Take Ore",
				"2 - Take Organics",
				"3 - Take Equipment",
				"4 - Take Fighters",
				"5 - Take Missiles",
				"6 - Take Mines",
				"9 - Take Plasma Bolts",
				"A - Take <A>ll (Default)",
				"B - Planet's <B>ank",
				"D - <D>isplay Planet",
				"F - Take/Leave Ground <F>orces",
				"L - <L>eave Planet",
				"N - Re-<N>ame Planet",
				"T - <T>ransfer Cargo to Planet",
				"! - Use Planet Thrusters",
				"$ - Raise Productivity"
			};
			size_t row;

			if (!session_present_paged_line(session, heading,
			    sizeof(heading) - 1U, "planet help heading", error)
			    || !session_present_paged_line(session,
			    (const uint8_t *)rows[0], strlen(rows[0]),
			    "planet help first row", error))
				return false;
			for (row = 1; row < YT_ARRAY_LEN(rows); ++row) {
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)rows[row], strlen(rows[row])))
					return false;
			}
			continue;
		}
		position = yt_planet_menu_selector_position(upper);
		if (position == 0) {
			static const uint8_t invalid[] = "Invalid command.";

			if (!session_present_alert(session, invalid,
			    sizeof(invalid) - 1U, "planet invalid command", error))
				return false;
			continue;
		}
		switch (position - 1) {
		case 0:
			if (!planet_garrison(session, logical_planet, error))
				return false;
			break;
		case 1:
			if (!planet_move(session, enter_sector, error))
				return false;
			if (enter_sector != NULL && *enter_sector)
				return true;
			break;
		case 2:
		{
			bool moved;

			if (!command_move(session, &moved, error))
				return false;
			if (enter_sector != NULL)
				*enter_sector = moved;
			return true;
		}
		case 3:
		{
			bool selected = false;

			if (!command_trade(session, &selected, error))
				return false;
			if (selected) {
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			}
			break;
		}
		case 4:
			return computer_menu(session, enter_sector, error);
		case 5: case 6: case 7: case 8: case 9: case 10:
			if (!planet_take_one(session, logical_planet,
			    position - 5, error))
				return false;
			break;
		case 11:
			if (!planet_take_one(session, logical_planet, 9, error))
				return false;
			break;
		case 12:
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		case 13:
			if (!planet_transfer(session, logical_planet, error))
				return false;
			break;
		case 14:
			if (!planet_take_all(session, logical_planet, error))
				return false;
			break;
		case 15:
			if (!planet_bank(session, logical_planet, error))
				return false;
			break;
		case 16:
			if (!planet_productivity(session, logical_planet, error))
				return false;
			break;
		default:
			break;
		}
		if (!session->running)
			return true;
	}
}

static bool
create_planet(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_planet[] =
	    "There is no planet in this sector.";
	static const uint8_t price[] = "Planets cost 25000 credits.";
	static const uint8_t too_poor[] = "You're too poor to buy one.";
	static const uint8_t buy_prompt[] =
	    "Do you wish to buy a planet(Y/N) [N]? ";
	static const uint8_t all_taken[] =
	    "I'm sorry, but all planets are taken.";
	static const uint8_t destroy_first[] =
	    "One has to be destroyed before you can buy a planet.";
	static const uint8_t advice[] =
	    "To increase productivity on your new planet, spend credits [$] on it.";
	struct yt_sector sector;
	int logical;
	struct yt_planet planet;
	struct yt_record raw;
	uint8_t cached_trader[YT_TEXT_FIELD_SIZE];
	uint8_t row[320];
	size_t cached_trader_length;
	size_t row_length;
	uint32_t selected_physical;
	uint32_t sector_physical;
	float selected_expression;
	float selected_logical;
	float scan;
	float minute;
	int today;
	int adjusted_year;
	bool renamed;
	enum yt_yes_no_answer answer;

	if (!session_present_paged_line(session, no_planet, sizeof(no_planet) - 1U,
	    "planet creation opening", error)
	    || !session_present_paged_fragment(session, price, sizeof(price) - 1U)
	    || !yt_player_stored_name(&session->player, cached_trader,
	    &cached_trader_length, error)
	    || !session_reload_player(session, error)
	    || !yt_planet_creation_credit_row((double)session->player.credits,
	    row, sizeof(row), &row_length)
	    || !session_present_paged_fragment(session, row, row_length))
		return false;
	if (25000.0f > session->player.credits)
		return session_present_alert(session, too_poor, sizeof(too_poor) - 1U,
		    "planet creation insufficient credits", error);
	if (!session_confirm(session, buy_prompt, sizeof(buy_prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	scan = single_add(session_planet_offset(session), 2.0f);
	for (;;) {
		uint32_t physical = qb_brun_random_record_number(scan);

		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical, &raw, error))
			return false;
		yt_planet_decode(&planet, &raw);
		if (planet.name_length == 0.0f) {
			selected_expression = scan;
			selected_physical = physical;
			break;
		}
		if (scan >= session->door->game.config.total_records) {
			if (!session_present_alert(session, all_taken,
			    sizeof(all_taken) - 1U,
			    "planet creation allocation full", error)
			    || !session_present_paged_fragment(session, destroy_first,
			    sizeof(destroy_first) - 1U))
				return false;
			return true;
		}
		scan = single_add(scan, 1.0f);
		if (scan > session->door->game.config.total_records) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "planet creation stale current-planet boundary");
			}
			return false;
		}
	}
	selected_logical = single_sub(selected_expression,
	    session_planet_offset(session));
	if (selected_physical
	    != qb_brun_random_record_number(selected_expression)
	    || selected_logical < (float)INT_MIN
	    || selected_logical > (float)INT_MAX
	    || selected_logical != floorf(selected_logical))
		return false;
	logical = (int)selected_logical;
	if (!planet_rename(session, logical, &renamed, error))
		return false;
	if (!renamed)
		return true;
	if (!read_planet_physical(session, selected_physical, &planet, error))
		return false;
	yt_planet_creation_overlay(&planet, session_record(session));
	if (!write_planet_physical(session, selected_physical, &planet, false,
	    error))
		return false;
	sector_physical = session_sector_basic_record(session,
	    session->player.sector);
	if (!yt_database_read(&session->door->game.database,
	    (size_t)sector_physical, &raw, error))
		return false;
	yt_sector_decode(&sector, &raw);
	sector.planet = selected_logical;
	(void)yt_record_set_number(&sector.record, YT_F93, selected_logical);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)sector_physical, &sector.record, error)
	    || !session_current_date_serial(session, &today, &adjusted_year,
	    error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	minute = floorf(current_minute());
	if (!read_planet_physical(session, selected_physical, &planet, error))
		return false;
	yt_planet_creation_timestamp_overlay(&planet, (float)today, minute);
	if (!write_planet_physical(session, selected_physical, &planet, false,
	    error))
		return false;
	if (!session_mutate_player_credits(session, -25000.0f, NULL, error)
	    || !yt_planet_creation_news(cached_trader, cached_trader_length,
	    (const uint8_t *)session->planet_name, strlen(session->planet_name),
	    row, sizeof(row), &row_length)
	    || !session_append_news_bytes(session, row, row_length, error)
	    || !yt_planet_creation_success_row(
	    (const uint8_t *)session->planet_name, strlen(session->planet_name),
	    row, sizeof(row), &row_length)
	    || !session_present_paged_line(session, row, row_length,
	    "planet creation success row", error)
	    || !session_sound(session, 4.0f, "planet creation sound", error)
	    || !session_present_paged_line(session, advice, sizeof(advice) - 1U,
	    "planet creation advice row", error))
		return false;
	return true;
}

static bool
planet_permission_update(void *context, float logical_planet,
    struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = session_planet_basic_record(session, logical_planet);

	return planet_update_cached_physical(session, physical,
	    &(struct yt_planet){0}, NULL, error);
}

static bool
planet_permission_read_planet(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	return read_planet_physical(context, physical_record, planet, error);
}

static bool
planet_permission_write_planet(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	return write_planet_physical(context, physical_record, planet, false,
	    error);
}

static bool
planet_permission_read_player(void *context, int physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, physical_record, player,
	    error);
}

static bool
planet_permission_present(void *context, const uint8_t *text, size_t length,
    enum yt_planet_permission_output_kind kind, const char *operation,
    struct yt_error *error)
{
	enum session_present_text_kind session_kind;

	switch (kind) {
	case YT_PLANET_PERMISSION_RAW:
		session_kind = SESSION_PRESENT_RAW;
		break;
	case YT_PLANET_PERMISSION_LINE:
		session_kind = SESSION_PRESENT_LINE;
		break;
	case YT_PLANET_PERMISSION_BOLD_LINE:
		session_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	default:
		return false;
	}
	return session_present_text(context, text, length, session_kind,
	    operation, error);
}

static bool
planet_permission_sound(void *context, float selector,
    const char *operation, struct yt_error *error)
{
	return session_sound(context, selector, operation, error);
}

static bool
planet_permission_wait(void *context, double seconds, const char *operation,
    struct yt_error *error)
{
	return session_wait(context, seconds, operation, error);
}

static void
planet_permission_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session_set_foreground(session, foreground);
}

static void
planet_permission_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	yt_present_set_blink(&session->presentation, blink);
}

static bool
command_land(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const struct yt_planet_permission_ops permission_ops = {
		planet_permission_update,
		planet_permission_read_planet,
		planet_permission_write_planet,
		planet_permission_read_player,
		planet_permission_present,
		planet_permission_sound,
		planet_permission_wait,
		random_value,
		planet_permission_set_foreground,
		planet_permission_set_blink,
	};
	static const uint8_t title[] = "<Land/Create planet>";
	static const uint8_t landing[] = "Landing...";
	static const uint8_t confirmation[] =
	    "Do you wish to try to force a landing? [y/N] ";
	struct yt_sector sector;
	struct yt_planet planet;
	uint8_t row[256];
	uint8_t prompt[160];
	size_t row_length;
	size_t prompt_length;
	uint32_t physical;
	volatile float planet_record_value;
	float cached_carried;
	int logical;
	struct yt_planet_permission_state permission_state;

	if (!session_present_paged_line(session, title, sizeof(title) - 1U,
	    "planet landing title", error)
	    || !session_reload_player(session, error))
		return false;
	cached_carried = session->player.ground_forces;
	if (!session_read_sector(session,
	    (int)session->player.sector, &sector, error))
		return false;
	session->inherited_loop_index = sector.planet;
	if (sector.planet == 0.0f) {
		bool created = create_planet(session, error);

		if (created && enter_sector != NULL)
			*enter_sector = true;
		return created;
	}
	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, landing, sizeof(landing) - 1U,
	    "planet landing progress", error))
		return false;
	planet_record_value = session_planet_offset(session)
	    + sector.planet;
	session->planet_record_expression = planet_record_value;
	memset(&permission_state, 0, sizeof(permission_state));
	permission_state.planet_record_value = planet_record_value;
	permission_state.planet_offset =
	    session_planet_offset(session);
	permission_state.current_player_record = session_record(session);
	permission_state.last_player_record = YT_PLAYER_LAST;
	permission_state.foreground = session_foreground(session);
	permission_state.blink = yt_present_blink(&session->presentation);
	if (!yt_planet_permission_run(&permission_state, &permission_ops,
	    session, error))
		return false;
	physical = permission_state.physical_planet_record;
	if (permission_state.denied) {
		enum yt_yes_no_answer answer;
		char response[YT_COMMAND_SIZE];
		float commitment;
		bool defeated;

		if (!read_planet_physical(session, physical, &planet, error)
		    || !yt_planet_landing_sensor_row(planet.ground_forces,
		    cached_carried, row, sizeof(row), &row_length)
		    || !session_present_paged_line(session, row, row_length,
		    "planet landing sensor row", error))
			return false;
		if (cached_carried < 1.0f) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!session_confirm(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!yt_planet_landing_amount_prompt(cached_carried, prompt,
		    sizeof(prompt), &prompt_length)
		    || !session_present_timed_paged_row(session, prompt, prompt_length,
		    "planet landing commitment prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		commitment = yt_planet_landing_commitment(response);
		if (!yt_planet_landing_commitment_valid(commitment,
		    cached_carried)) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!planet_assault(session, physical, commitment, &defeated,
		    error))
			return false;
		if (defeated) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
	}
	if ((int64_t)physical - (int)session_planet_offset(session)
	    < INT_MIN
	    || (int64_t)physical
	    - (int)session_planet_offset(session) > INT_MAX)
		return false;
	logical = (int)((int64_t)physical
	    - (int)session_planet_offset(session));
	if (!planet_inventory(session, logical, error))
		return false;
	return planet_menu(session, logical, enter_sector, error);
}

static bool
team_load(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	struct yt_record overlay;
	bool overlay_loaded;
	bool live;
	size_t index;

	if (team != NULL) {
		memset(team, 0, sizeof(*team));
		team->id = id;
	}
	if (!session_load_team_cache(session, id, session_record(session),
	    &overlay, &overlay_loaded, &live, error))
		return false;
	if (team == NULL)
		return true;
	if (overlay_loaded)
		yt_sector_decode(&team->overlay, &overlay);
	memcpy(team->name, session->team_cache.name,
	    sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = live;
	team->full = team->live;
	for (index = 0; index < 4; ++index) {
		team->roster[index] = session->team_cache.roster[index];
		if (team->roster[index] <= 0)
			team->full = false;
	}
	return true;
}

static bool
team_store_inactive(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	if (!session_read_sector(session, team->id, &team->overlay, error))
		return false;
	yt_team_inactive_overlay(&team->overlay.record);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

static bool
team_read_overlay(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	memset(team, 0, sizeof(*team));
	team->id = id;
	if (id < 0 || id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	return session_read_sector(session, id, &team->overlay,
	    error);
}

static bool
team_store_roster(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	yt_team_roster_overlay(&team->overlay.record, team->roster);
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team->id),
	    &team->overlay.record, error);
}

static bool
team_audit(struct yt_session *session, int team_id,
    enum yt_team_audit_event event, const char *attempt,
    struct yt_error *error)
{
	uint8_t message[YT_COMMAND_SIZE + 128U];
	struct yt_clock_value now;
	char date[11] = "";
	char time_text[9] = "";
	size_t message_length;
	size_t index;

	if (event == YT_TEAM_AUDIT_JOIN || event == YT_TEAM_AUDIT_QUIT) {
		if (!yt_platform_clock(&now, error))
			return false;
		yt_format_date(&now, date);
		if (!yt_platform_clock(&now, error))
			return false;
		yt_format_time(&now, time_text);
	}
	if (!yt_team_audit_message(event, session->player.name, attempt, date,
	    time_text, message, sizeof(message), &message_length)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "team audit message length");
			error->path[0] = '\0';
		}
		return false;
	}
	if (!session_load_team_cache(session, team_id, session_record(session),
	    NULL, NULL, NULL, error))
		return false;
	for (index = 0U; index < YT_ARRAY_LEN(session->team_cache.roster);
	    ++index) {
		int recipient = session->team_cache.roster[index];

		if (recipient != 0 && recipient != session_record(session)
		    && !radio_append_bytes(message, message_length, -2.0f,
		    (float)recipient, error))
			return false;
	}
	return true;
}

static bool
info_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
info_refresh_time(struct yt_session *session, struct yt_error *error)
{
	DWORD elapsed_seconds;
	WORD elapsed_milliseconds;
	float remaining_seconds;
	enum yt_present_status status;

	od_get_time(&elapsed_seconds, &elapsed_milliseconds);
	remaining_seconds = (float)od_control.user_timelimit * 60.0f
	    - (float)(elapsed_seconds % 60U)
	    - (float)elapsed_milliseconds / 1000.0f;
	status = yt_present_format_remaining_seconds(&session->time,
	    remaining_seconds);
	if (status != YT_PRESENT_OK)
		return info_failure(error, "Info time refresh");
	return true;
}

static bool
info_line(struct yt_session *session, const void *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(session, text, length, SESSION_PRESENT_LINE,
	    "Info line presentation", error);
}

static void
info_team_result(struct yt_session *session,
    const struct yt_player *current_player, const struct yt_team *team,
    bool is_captain, struct yt_team *resolved_team,
    bool *current_is_captain)
{
	session->player.record = current_player->record;
	session->player.team = current_player->team;
	if (resolved_team != NULL)
		*resolved_team = *team;
	if (current_is_captain != NULL)
		*current_is_captain = is_captain;
}

static bool
info_team_lines(struct yt_session *session, struct yt_team *resolved_team,
    bool *current_is_captain, struct yt_error *error)
{
	static const uint8_t none[] = "Team  : None";
	static const uint8_t promoted[] =
	    "Your team has no captain! You've been promoted to Captain!";
	static const uint8_t congratulations[] =
	    "Congratulations Captain! See Team Menu for your new options!";
	struct yt_player current_player;
	struct yt_player captain;
	struct yt_player ignored;
	struct yt_team team;
	struct yt_sector fresh;
	uint8_t captain_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[256];
	size_t captain_name_length = 0U;
	size_t row_length;
	int current_record = session_record(session);
	int team_id;
	int captain_record;
	bool valid_captain = false;

	memset(&team, 0, sizeof(team));
	if (!yt_game_read_player(&session->door->game, current_record,
	    &current_player, error))
		return false;
	team_id = (int)current_player.team;
	if (team_id == 0) {
		if (!info_line(session, none, sizeof(none) - 1U, error)
		    || !info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, false,
		    resolved_team, current_is_captain);
		return true;
	}
	if (!team_load(session, team_id, &team, error)
	    || !yt_info_team_row(YT_INFO_TEAM_SUMMARY, team_id,
	    (const uint8_t *)team.name, team.name_length, row, sizeof(row),
	    &row_length))
		return false;
	if (!info_line(session, row, row_length, error)
	    || !info_line(session, NULL, 0U, error))
		return false;
	if (team.captain == current_record) {
		if (!yt_info_team_row(YT_INFO_TEAM_SELF_CAPTAIN, team_id, NULL,
		    0U, row, sizeof(row), &row_length))
			return info_failure(error, "Info team row");
		if (!info_line(session, row, row_length, error)
		    || !info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, true,
		    resolved_team, current_is_captain);
		return true;
	}
	captain_record = team.captain;
	session->shared_target_record = (float)captain_record;
	if (captain_record >= 2
	    && (float)captain_record <= session_sector_offset(session)) {
		if (!yt_game_read_player(&session->door->game, captain_record,
		    &captain, error))
			return false;
		if (captain.name_length > 0.0f) {
			if (!yt_player_stored_name(&captain, captain_name,
			    &captain_name_length, error))
				return false;
			valid_captain = captain.team == (float)team_id;
		}
	}
	if (!valid_captain) {
		session->shared_target_record = (float)current_record;
		session->team_cache.captain = current_record;
		session->team_cache.current_player_is_captain = true;
		if (!session_read_sector(session, team_id, &fresh, error)
		    || !yt_record_set_number(&fresh.record, YT_F77,
		    (float)current_record))
			return false;
		team.overlay = fresh;
		team.captain = current_record;
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session, (float)team_id),
		    &fresh.record, error)
		    || !info_line(session, promoted, sizeof(promoted) - 1U, error)
		    || !info_line(session, congratulations,
		    sizeof(congratulations) - 1U, error)
		    || !info_line(session, NULL, 0U, error))
			return false;
		info_team_result(session, &current_player, &team, true,
		    resolved_team, current_is_captain);
		return true;
	}
	if (!yt_game_read_player(&session->door->game, captain_record, &ignored,
	    error))
		return false;
	if (!yt_info_team_row(YT_INFO_TEAM_OTHER_CAPTAIN, team_id, captain_name,
	    captain_name_length, row, sizeof(row), &row_length))
		return info_failure(error, "Info team row");
	if (!info_line(session, row, row_length, error)
	    || !info_line(session, NULL, 0U, error))
		return false;
	info_team_result(session, &current_player, &team, false, resolved_team,
	    current_is_captain);
	return true;
}

static bool
info_panel_refresh(void *context, uint8_t *text, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct yt_session *session = context;

	if (length == NULL || !info_refresh_time(session, error))
		return false;
	if (session->time.text_length > capacity)
		return info_failure(error, "Info time text capacity");
	if (session->time.text_length != 0U)
		memcpy(text, session->time.text, session->time.text_length);
	*length = session->time.text_length;
	return true;
}

static bool
info_panel_team(void *context, struct yt_error *error)
{
	return info_team_lines(context, NULL, NULL, error);
}

static bool
info_panel_read_player(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (!session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
info_panel_present(void *context, const uint8_t *text, size_t length,
    enum yt_info_panel_output_kind kind, float width,
    struct yt_info_panel_state *state, struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	session_set_foreground(session, state->foreground);
	yt_present_set_background(&session->presentation, state->background);
	yt_present_set_bold(&session->presentation, state->bold);
	if (kind == YT_INFO_PANEL_LINE)
		result = info_line(session, text, length, error);
	else if (kind == YT_INFO_PANEL_FIXED)
		result = session_fixed_width_bytes(session, text, length, width,
		    "Info fixed-width presentation", error);
	else
		return info_failure(error, "Info presentation kind");
	state->foreground = session_foreground(session);
	state->background = yt_present_background(&session->presentation);
	state->bold = yt_present_bold(&session->presentation);
	return result;
}

static bool
show_ship(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_info_panel_ops ops = {
		info_panel_refresh,
		info_panel_team,
		info_panel_read_player,
		info_panel_present,
	};
	struct yt_info_panel_state state;
	bool result;

	memset(&state, 0, sizeof(state));
	state.cached_name = session->cached_player_name;
	state.cached_name_length = session->cached_player_name_length;
	state.anti_cloak = session_anti_cloak_enabled(session) ? -1.0f : 0.0f;
	state.foreground = session_foreground(session);
	state.background = yt_present_background(&session->presentation);
	state.bold = yt_present_bold(&session->presentation);
	result = yt_info_panel_run(&state, &ops, session, error);
	session_set_foreground(session, state.foreground);
	yt_present_set_background(&session->presentation, state.background);
	yt_present_set_bold(&session->presentation, state.bold);
	return result;
}

static bool
team_pick_name(struct yt_session *session, float team_id, char name[42],
    bool *accepted, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Pick a name for your Team (41 chars. max)? ";
	static const uint8_t invalid[] =
	    "Team names MUST more than 2 letters!";
	struct yt_team team;
	char response[YT_COMMAND_SIZE];
	size_t name_length;

	if (accepted == NULL)
		return false;
	*accepted = false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team name leading blank", error)
	    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
	    "team name prompt", error)
	    || !session_read_command(session, response, sizeof(response)))
		return false;
	if (!yt_team_prepare_name(response, &name_length))
		return session_present_alert(session, invalid, sizeof(invalid) - 1U,
		    "team name invalid length", error);
	if (team_id != floorf(team_id) || team_id < 0.0f
	    || team_id > (float)YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team name record number");
		}
		return false;
	}
	(void)snprintf(name, 42, "%s", response);
	if (!team_read_overlay(session, (int)team_id, &team, error))
		return false;
	(void)snprintf(team.name, sizeof(team.name), "%s", response);
	yt_team_name_overlay(&team.overlay.record,
	    (const uint8_t *)response, name_length);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team.id),
	    &team.overlay.record, error))
		return false;
	*accepted = true;
	return true;
}

static bool
team_create_password(struct yt_session *session, int team_id,
    char password[5], struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Please Pick a Password for your Team. (4 Chars.) :";
	static const uint8_t invalid[] = "Password MUST be 4 characters!";
	struct yt_team team;
	char response[80];
	char reminder[160];

	for (;;) {
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team password leading blank", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "team password prompt", error)
		    || !session_read_upper_command(session, response, sizeof(response)))
			return false;
		if (strlen(response) != 4U) {
			if (!session_present_alert(session, invalid, sizeof(invalid) - 1U,
			    "team password invalid length", error))
				return false;
			continue;
		}
		memcpy(password, response, 4);
		password[4] = '\0';
		if (snprintf(reminder, sizeof(reminder),
		    "REMEMBER YOUR TEAM PASSWORD SO OTHERS CAN JOIN! -+> %s",
		    password) < 0
		    || !session_present_alert(session, (const uint8_t *)reminder,
		    strlen(reminder), "team password reminder", error)
		    || !team_read_overlay(session, team_id, &team, error))
			return false;
		memcpy(team.password, password, 5);
		yt_team_password_overlay(&team.overlay.record,
		    (const uint8_t *)password);
		return yt_database_write(&session->door->game.database,
		    (size_t)session_sector_basic_record(session, (float)team.id),
		    &team.overlay.record, error);
	}
}

static bool
team_create(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t entering[] = "Entering a New Team...";
	struct yt_team team;
	char name[42];
	char actor_name[42];
	char password[5];
	char number[64];
	char news[300];
	char success[300];
	int id;
	float selected;
	bool name_accepted;

	(void)snprintf(actor_name, sizeof(actor_name), "%s",
	    session->player.name);
	if (!session_present_alert(session, entering, sizeof(entering) - 1U,
	    "team create heading", error))
		return false;
	selected = session->player.team;
	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!team_load(session, id, &team, error))
			return false;
		if (!team.live) {
			selected = (float)id;
			break;
		}
	}
	if (!team_pick_name(session, selected, name, &name_accepted, error))
		return false;
	if (!name_accepted)
		return true;
	id = (int)selected;
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, id);
	if (!write_player(session, error)
	    || !team_read_overlay(session, id, &team, error))
		return false;
	team.id = id;
	team.captain = session_record(session);
	team.roster[0] = session_record(session);
	team.roster[1] = 0;
	team.roster[2] = 0;
	team.roster[3] = 0;
	yt_record_set_number_if_changed(&team.overlay.record, YT_F77,
	    (float)team.captain);
	yt_team_roster_overlay(&team.overlay.record, team.roster);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)team.id),
	    &team.overlay.record, error)
	    || !team_create_password(session, id, password, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (qb_str_single(number, sizeof(number), selected) < 0
	    || snprintf(news, sizeof(news), "%s Created Team%s -=- %s",
	    actor_name, number, name) < 0
	    || !append_news(session, news, error)
	    || snprintf(success, sizeof(success),
	    "Team number [%s ] [%s] CREATED!", number, name) < 0)
		return false;
	return session_present_alert(session, (const uint8_t *)success,
	    strlen(success), "team create success", error);
}

static bool
team_join(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t selection_prompt[] =
	    "Which team do you wish to join (0=quit)? ";
	static const uint8_t dead[] = "That team is dead!";
	static const uint8_t full[] = "The Team you picked is full!!";
	static const uint8_t password_prompt[] =
	    "Please enter Password to Join Team? ";
	static const uint8_t invalid[] = "Invalid Password entered!";
	static const uint8_t success[] =
	    "Your Team info has been recorded!  Have fun!";
	struct yt_team team;
	struct qb_val_result parsed;
	char line[80];
	char actor_name[42];
	char number[64];
	char row[160];
	int id;
	int selected;
	size_t index;
	char news[300];
	bool listed;

	(void)snprintf(actor_name, sizeof(actor_name), "%s",
	    session->player.name);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team join leading blank", error))
		return false;

	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!team_load(session, id, &team, error))
			return false;
		listed = team.live;
		if (listed) {
			if (qb_str_single(number, sizeof(number), (float)id) < 0
			    || snprintf(row, sizeof(row), "%s] %s", number,
			    team.name) < 0
			    || !session_present_paged_fragment(session, (const uint8_t *)row,
			    strlen(row)))
				return false;
		}
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team join selection blank", error)
	    || !session_present_timed_paged_row(session, selection_prompt,
	    sizeof(selection_prompt) - 1U, "team join selection prompt", error)
	    || !session_read_number_command(session, line, sizeof(line)))
		return false;
	parsed = qb_val(line);
	if (!parsed.valid || parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team join:VAL");
		}
		return false;
	}
	selected = (int)floor(parsed.value);
	if (selected < 1)
		return true;
	if (!team_load(session, selected, &team, error))
		return false;
	if (!team.live)
		return session_present_alert(session, dead, sizeof(dead) - 1U,
		    "team join dead", error);
	if (team.full)
		return session_present_alert(session, full, sizeof(full) - 1U,
		    "team join full", error);
	{
		struct yt_team ignored;

		if (!team_read_overlay(session, selected, &ignored, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(row, sizeof(row), "Team #%s: %s", number,
	    team.name) < 0
	    || !session_present_paged_fragment(session, (const uint8_t *)row, strlen(row))
	    || !session_present_timed_paged_row(session, password_prompt,
	    sizeof(password_prompt) - 1U, "team join password prompt", error)
	    || !session_read_upper_command(session, line, sizeof(line)))
		return false;
	if (strlen(line) != 4U || memcmp(line, team.password, 4) != 0) {
		if (!team_audit(session, selected,
		    YT_TEAM_AUDIT_INVALID_PASSWORD, line, error))
			return false;
		return session_present_alert(session, invalid, sizeof(invalid) - 1U,
		    "invalid team password row", error);
	}
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, selected);
	if (!write_player(session, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team.roster[index] == 0) {
			team.roster[index] = session_record(session);
			break;
		}
	}
	{
		struct yt_team fresh;

		if (!team_read_overlay(session, selected, &fresh, error))
			return false;
		memcpy(fresh.roster, team.roster, sizeof(fresh.roster));
		if (!team_store_roster(session, &fresh, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(news, sizeof(news), "%s Joined Team%s",
	    actor_name, number) < 0)
		return false;
	if (!append_news(session, news, error))
		return false;
	session_set_foreground(session, 3.0f);
	if (!session_present_alert(session, success, sizeof(success) - 1U,
	    "team join success row", error))
		return false;
	return team_audit(session, selected, YT_TEAM_AUDIT_JOIN, "", error);
}

static bool
team_quit(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Are you sure you wish to quit your team? [N] ";
	static const uint8_t success[] =
	    "You have been removed from Team play";
	enum yt_yes_no_answer answer;
	struct yt_player persisted;
	struct yt_sector explicit_overlay;
	float old_team;
	size_t index;
	bool live = false;

	if (!session_confirm(session, prompt, sizeof(prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	old_team = session->player.team;
	yt_team_membership_apply_player(&session->player, 0);
	persisted = session->player;
	if (!yt_game_write_player(&session->door->game, session_record(session),
	    &persisted, error))
		return false;
	if (old_team != floorf(old_team) || old_team < 1.0f
	    || old_team > (float)YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "team quit record number");
		}
		return false;
	}
	if (!session_read_sector(session, (int)old_team,
	    &explicit_overlay, error))
		return false;
	(void)explicit_overlay;
	if (!team_load(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team->roster[index] == session_record(session))
			team->roster[index] = 0;
	}
	if (!team_store_roster(session, team, error)
	    || !team_load(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index)
		if (team->roster[index] != 0)
			live = true;
	if (!live) {
		team->captain = 0;
		memcpy(team->password, "    ", 4);
		team->password[4] = '\0';
		memset(team->roster, 0, sizeof(team->roster));
		if (!team_store_inactive(session, team, error))
			return false;
	}
	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, success, sizeof(success) - 1U,
	    "team quit success row", error)
	    || !team_audit(session, (int)old_team, YT_TEAM_AUDIT_QUIT, "",
	    error))
		return false;
	return true;
}

static bool
team_search(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t locating[] =
	    "Locating Team Members, Planets & Defenses.";
	static const uint8_t heading[] =
	    "Name                                     Sector";
	static const uint8_t rule[] =
	    "====================                     ======";
	static const uint8_t defending[] = "Defending;";
	static const uint8_t planets[] = "Planets;";
	static const uint8_t none[] = "None Found";
	const float cached_team = session->player.team;
	int player_record;
	bool found = false;

	if (!session_present_paged_line(session, locating, sizeof(locating) - 1U,
	    "team resource locating row", error))
		return false;
	for (player_record = YT_PLAYER_FIRST;
	    player_record <= (int)session_sector_offset(session);
	    ++player_record) {
		struct yt_player player;
		uint8_t row[YT_TEXT_FIELD_SIZE + 64U];
		char number[64];
		size_t number_length;
		int logical_sector;
		bool first;

		if (!yt_game_read_player(&session->door->game, player_record,
		    &player, error))
			return false;
		if (player.team != cached_team
		    || player_record == session_record(session))
			continue;
		if (qb_str_single(number, sizeof(number), player.sector) < 0)
			return false;
		number_length = strlen(number);
		memcpy(row, player.record.bytes, YT_TEXT_FIELD_SIZE);
		memcpy(row + YT_TEXT_FIELD_SIZE, number, number_length);
		if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
		    "team resource player heading", error)
		    || !session_present_paged_fragment(session, rule, sizeof(rule) - 1U)
		    || !session_present_paged_fragment(session, row,
		    YT_TEXT_FIELD_SIZE + number_length))
			return false;
		found = true;

		first = true;
		for (logical_sector = 1;
		    logical_sector <= session_sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;

			if (!session_read_sector(session,
			    logical_sector, &sector, error))
				return false;
			if (!(sector.fighters > 0.0f
			    && sector.fighter_owner == (float)player_record))
				continue;
			if (first && !session_present_text(session, defending,
			    sizeof(defending) - 1U, SESSION_PRESENT_RAW,
			    "team resource defense label", error))
				return false;
			first = false;
			if (qb_str_single(number, sizeof(number),
			    (float)logical_sector) < 0
			    || !session_present_text(session,
			    (const uint8_t *)number, strlen(number),
			    SESSION_PRESENT_RAW, "team resource defense sector",
			    error))
				return false;
		}
		if (!first && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "team resource defense terminator",
		    error))
			return false;

		first = true;
		for (logical_sector = 1;
		    logical_sector <= session_sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;
			struct yt_planet planet;

			if (!session_read_sector(session,
			    logical_sector, &sector, error))
				return false;
			if (sector.planet <= 0.0f)
				continue;
			if (!session_read_planet(session,
			    (int)sector.planet, &planet, error))
				return false;
			if (planet.owner != (float)player_record)
				continue;
			if (first && !session_present_text(session, planets,
			    sizeof(planets) - 1U, SESSION_PRESENT_RAW,
			    "team resource planet label", error))
				return false;
			first = false;
			if (qb_str_single(number, sizeof(number),
			    (float)logical_sector) < 0
			    || !session_present_text(session,
			    (const uint8_t *)number, strlen(number),
			    SESSION_PRESENT_RAW, "team resource planet sector",
			    error))
				return false;
		}
		if (!first && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "team resource planet terminator",
		    error))
			return false;
	}
	if (!found)
		return session_present_paged_fragment(session, none, sizeof(none) - 1U);
	return true;
}

static bool
team_transfer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_defense[] =
	    "There IS no defense force here!";
	static const uint8_t prompt[] =
	    "How many fighters do you wish to transfer? ";
	static const uint8_t success[] = "Fighters transferred!";
	struct yt_sector initial_sector;
	double initial_fighters;
	double initial_defense;
	int logical_sector;

	if (!session_reload_player(session, error))
		return false;
	logical_sector = (int)session->player.sector;
	initial_fighters = (double)session->player.fighters;
	if (!session_read_sector(session, logical_sector,
	    &initial_sector, error))
		return false;
	initial_defense = (double)initial_sector.fighters;
	if (initial_defense == 0.0)
		return session_present_alert(session, no_defense,
		    sizeof(no_defense) - 1U, "team transfer no defense", error);
	for (;;) {
		char fighter_text[64];
		char defense_text[64];
		char row[160];
		char response[160];
		struct qb_val_result parsed;
		enum qb_mbf_status conversion;
		uint8_t amount_raw[4];
		float amount;

		if (qb_str_double(fighter_text, sizeof(fighter_text),
		    initial_fighters) < 0
		    || qb_str_double(defense_text, sizeof(defense_text),
		    initial_defense) < 0
		    || snprintf(row, sizeof(row), "You have%s fighters.",
		    fighter_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)row, strlen(row),
		    "team transfer carried row", error)
		    || snprintf(row, sizeof(row), "There are%s fighters here.",
		    defense_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)row, strlen(row),
		    "team transfer deployed row", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "team transfer prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		parsed = qb_val(response);
		if (parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team transfer:VAL");
			}
			return false;
		}
		amount = parsed.valid ? (float)parsed.value : 0.0f;
		conversion = qb_mbf32_encode(amount, amount_raw);
		if (conversion == QB_MBF_OVERFLOW) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team transfer:amount-csng");
			}
			return false;
		}
		amount = qb_mbf32_decode(amount_raw);
		if (amount < 1.0f)
			return true;
		if ((double)amount > initial_fighters) {
			if (snprintf(row, sizeof(row), "You only have%s!",
			    fighter_text) < 0
			    || !session_present_alert(session, (const uint8_t *)row,
			    strlen(row), "team transfer too many", error))
				return false;
			continue;
		}
		{
			struct yt_sector fresh_sector;

			if (!session_read_sector(session,
			    logical_sector, &fresh_sector, error))
				return false;
			yt_team_transfer_apply_sector(&fresh_sector,
			    initial_defense, amount);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)session_sector_basic_record(session,
			    (float)logical_sector),
			    &fresh_sector.record, error))
				return false;
		}
		if (!session_reload_player(session, error))
			return false;
		yt_team_transfer_apply_player(&session->player, amount);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &session->player.record, error))
			return false;
		return session_present_alert(session, success, sizeof(success) - 1U,
		    "team transfer success", error);
	}
}

static bool
team_banish(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Banish ";
	static const uint8_t prompt_suffix[] = " (Y/[N])? ";
	static const uint8_t end[] = "End of List";
	static const uint8_t success[] =
	    "Done. Now change your Team Password!";
	int team_id;
	size_t index;

	if (!session_reload_player(session, error))
		return false;
	team_id = (int)session->player.team;
	if (!team_load(session, team_id, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		struct yt_player member;
		enum yt_yes_no_answer answer;
		uint8_t prompt[sizeof(prompt_prefix) - 1U + YT_TEXT_FIELD_SIZE
		    + sizeof(prompt_suffix) - 1U];
		size_t name_length;
		size_t prompt_length = 0;
		int member_record;

		if (team->roster[index] <= 0
		    || team->roster[index] == session_record(session))
			continue;
		member_record = team->roster[index];
		if (!yt_game_read_player(&session->door->game,
		    member_record, &member, error))
			return false;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (!yt_player_stored_name(&member, prompt + prompt_length,
		    &name_length, error))
			return false;
		prompt_length += name_length;
		memcpy(prompt + prompt_length, prompt_suffix,
		    sizeof(prompt_suffix) - 1U);
		prompt_length += sizeof(prompt_suffix) - 1U;
		if (!session_confirm(session, prompt, prompt_length, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES)
			continue;
		if (!yt_game_read_player(&session->door->game,
		    member_record, &member, error))
			return false;
		yt_team_banish_apply_player(&member);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)member_record, &member.record, error))
			return false;
		team->roster[index] = 0;
		session->team_cache.roster[index] = 0;
		if (!session_read_sector(session, team_id,
		    &team->overlay, error)
		    || !team_store_roster(session, team, error))
			return false;
		return session_present_alert(session, success, sizeof(success) - 1U,
		    "team banish success", error);
	}
	return session_present_paged_fragment(session, end, sizeof(end) - 1U);
}

static bool
command_team(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t exit_row[] = "1) Exit Team menu";
	static const char *const teamless_rows[] = {
		"2) Create a Team",
		"3) Join a Team",
	};
	static const char *const member_rows[] = {
		"4) Quit a Team",
		"5) Search for Team Members & Resources",
		"6) Transfer Fighters to Defense Force",
	};
	static const char *const captain_rows[] = {
		"7) Banish a Team Member",
		"8) Change Team Password",
		"9) Change Team Name",
	};
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] = "Team Command? ";
	static const uint8_t invalid_row[] = "Invalid Choice!";

	for (;;) {
		char line[80];
		struct yt_team team;
		bool captain = false;
		struct qb_val_result parsed;
		enum qb_mbf_status conversion;
		uint8_t numeric_raw[4];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		size_t index;
		float numeric;
		int32_t captain_cint;
		int32_t team_cint;
		bool overflow;
		bool invalid;

		session_set_foreground(session, 6.0f);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team front leading blank", error)
		    || !info_team_lines(session, &team, &captain, error))
			return false;
		session_set_pager_line_count(session, 0.0f);
		if (!session_reload_player(session, error)
		    || !session_present_paged_line(session, exit_row, sizeof(exit_row) - 1U,
		    "team exit row", error)
		    || !session_reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
			for (index = 0; index < YT_ARRAY_LEN(teamless_rows); ++index)
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)teamless_rows[index],
				    strlen(teamless_rows[index])))
					return false;
		}
		else {
			for (index = 0; index < YT_ARRAY_LEN(member_rows); ++index)
				if (!session_present_paged_fragment(session,
				    (const uint8_t *)member_rows[index],
				    strlen(member_rows[index])))
					return false;
			if (captain)
				for (index = 0; index < YT_ARRAY_LEN(captain_rows);
				    ++index)
					if (!session_present_paged_fragment(session,
					    (const uint8_t *)captain_rows[index],
					    strlen(captain_rows[index])))
						return false;
		}
		session_set_foreground(session, 6.0f);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team prompt blank", error))
			return false;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team prompt time capacity");
			}
			return false;
		}
		memcpy(prompt + prompt_length, session->time.text,
		    session->time.text_length);
		prompt_length += session->time.text_length;
		memcpy(prompt + prompt_length, prompt_body,
		    sizeof(prompt_body) - 1U);
		prompt_length += sizeof(prompt_body) - 1U;
		if (!session_present_timed_paged_row(session, prompt, prompt_length,
		    "team command prompt", error)
		    || !session_read_command(session, line, sizeof(line)))
			return false;
		parsed = qb_val(line);
		if (!parsed.valid || parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:VAL");
			}
			return false;
		}
		numeric = (float)parsed.value;
		conversion = qb_mbf32_encode(numeric, numeric_raw);
		if (conversion == QB_MBF_OVERFLOW) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:choice-csng");
			}
			return false;
		}
		numeric = qb_mbf32_decode(numeric_raw);
		captain_cint = qb_cint(captain ? -1.0 : 0.0, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "team:captain-cint");
			}
			return false;
		}
		team_cint = qb_cint_mbf32(session->player.record.bytes + YT_F89, 0U,
		    &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "team:team-cint");
			}
			return false;
		}
		invalid = yt_team_choice_rejected(numeric, session->player.team,
		    captain_cint, team_cint);
		if (invalid) {
			if (!session_present_alert(session, invalid_row,
			    sizeof(invalid_row) - 1U, "team invalid choice", error))
				return false;
			continue;
		}
		if (strcmp(line, "1") == 0)
			return true;
		if (strcmp(line, "2") == 0) {
			if (!team_create(session, error))
				return false;
		}
		else if (strcmp(line, "3") == 0) {
			if (!team_join(session, error))
				return false;
		}
		else if (strcmp(line, "4") == 0) {
			if (!team_quit(session, &team, error))
				return false;
		}
		else if (strcmp(line, "5") == 0) {
			if (!team_search(session, error))
				return false;
		}
		else if (strcmp(line, "6") == 0) {
			if (!team_transfer(session, error))
				return false;
		}
		else if (strcmp(line, "7") == 0) {
			if (!team_banish(session, &team, error))
				return false;
		}
		else if (strcmp(line, "8") == 0) {
			if (!team_create_password(session, team.id,
			    team.password, error))
				return false;
		}
		else if (strcmp(line, "9") == 0) {
			char name[42];
			bool accepted;

			if (!team_pick_name(session, (float)team.id, name,
			    &accepted, error))
				return false;
			(void)accepted;
		}
	}
}

static bool
port_name_row(void *context, enum yt_port_name_row_kind kind,
    const uint8_t *text, size_t length, struct yt_error *error)
{
	struct yt_session *session = context;
	const char *operation;

	switch (kind) {
	case YT_PORT_NAME_CURRENT_ROW:
		operation = "port name current row";
		break;
	case YT_PORT_NAME_KEEP_ROW:
		operation = "port name keep row";
		break;
	case YT_PORT_NAME_INSTRUCTION_ROW:
		operation = "port name instruction row";
		break;
	default:
		return false;
	}
	return session_present_paged_line(session, text, length, operation, error);
}

static bool
port_name_prompt(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_timed_paged_row(context, text, length, "port name prompt", error);
}

static bool
port_name_edit(void *context, uint8_t *response, size_t capacity,
    size_t *length, struct yt_error *error)
{
	(void)error;
	if (length == NULL
	    || !session_read_command(context, (char *)response, capacity))
		return false;
	*length = strlen((const char *)response);
	return true;
}

static bool
port_name_blank(void *context, struct yt_error *error)
{
	return session_present_text(context, NULL, 0U, SESSION_PRESENT_LINE,
	    "port name confirmation leading blank", error);
}

static bool
port_name_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (accepted == NULL
	    || !session_confirm(context, prompt, length, &answer, error))
		return false;
	*accepted = answer == YT_YES_NO_YES;
	return true;
}

static bool
port_name_write(void *context, int logical_port,
    const struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    record, error);
}

static bool
port_rename(struct yt_session *session, int logical_port,
    const uint8_t *cached, size_t cached_length, struct yt_port *port,
    struct yt_error *error)
{
	static const struct yt_port_name_editor_ops ops = {
		.row = port_name_row,
		.prompt = port_name_prompt,
		.edit = port_name_edit,
		.blank = port_name_blank,
		.confirm = port_name_confirm,
		.write = port_name_write,
	};
	struct yt_port_name_editor_state state = {
		.cached = cached,
		.cached_length = cached_length,
		.logical_port = logical_port,
		.port = port,
	};

	return yt_port_name_editor_run(&state, &ops, session, error);
}

static bool
port_rename_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_rename_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
port_rename_read_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_port(session, logical_port, port,
	    error);
}

static bool
port_rename_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_rename_output_kind kind, struct yt_error *error)
{
	const char *operation;

	switch (kind) {
	case YT_PORT_RENAME_NO_PORT:
		operation = "rename no-port row";
		break;
	case YT_PORT_RENAME_NOT_OWNER:
		operation = "rename ownership row";
		break;
	case YT_PORT_RENAME_EARTH:
		operation = "rename Earth row";
		break;
	default:
		return false;
	}
	return session_present_alert(context, text, length, operation, error);
}

static bool
port_rename_edit(void *context, int logical_port, const uint8_t *cached,
    size_t cached_length, struct yt_port *port, struct yt_error *error)
{
	return port_rename(context, logical_port, cached, cached_length, port,
	    error);
}

static bool
command_rename_port(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_port_rename_ops ops = {
		port_rename_hydrate,
		port_rename_read_sector,
		port_rename_read_port,
		port_rename_present,
		port_rename_edit,
	};
	struct yt_port_rename_state state = {
		.current_player_record = (float)session_record(session),
		.port_offset = session_port_offset(session),
		.conversion_mode =
		    session->presentation.sound.conversion_mode,
	};

	return yt_port_rename_run(&state, &ops, session, error);
}

static bool
command_rename_port_cycle(struct yt_session *session,
    struct yt_error *error)
{
	return command_rename_port(session, error)
	    && display_current_sector_cached(session, error);
}

static bool
port_purchase_accept_present(void *context, const uint8_t *text,
    size_t length, enum yt_port_purchase_accept_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PORT_PURCHASE_ACCEPT_SOLD_BLANK:
	case YT_PORT_PURCHASE_ACCEPT_TRANSFER_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, kind ==
		    YT_PORT_PURCHASE_ACCEPT_SOLD_BLANK
		    ? "buy sold leading blank"
		    : "buy seller transfer leading blank", error);
	case YT_PORT_PURCHASE_ACCEPT_SOLD_ROW:
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_paged_fragment(session, text, length);
	case YT_PORT_PURCHASE_ACCEPT_TRANSFER_ROW:
	case YT_PORT_PURCHASE_ACCEPT_SUCCESS_TAIL:
		return session_present_paged_fragment(session, text, length);
	case YT_PORT_PURCHASE_ACCEPT_SUCCESS_FIRST:
		return session_present_paged_line(session, text, length,
		    "buy congratulations row", error);
	default:
		return false;
	}
}

static bool
port_purchase_accept_read_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_port(session, logical_port, port,
	    error);
}

static bool
port_purchase_accept_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
port_purchase_accept_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record == session_record(session))
		session->player = *player;
	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &player->record, error);
}

static bool
port_purchase_accept_radio(void *context, const uint8_t *text,
    size_t length, float sender, float recipient, struct yt_error *error)
{
	(void)context;
	return radio_append_bytes(text, length, sender, recipient, error);
}

static bool
port_purchase_accept_rename(void *context, int logical_port,
    const uint8_t *cached, size_t cached_length, struct yt_port *port,
    struct yt_error *error)
{
	return port_rename(context, logical_port, cached, cached_length, port,
	    error);
}

static bool
port_purchase_accept_write_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_port_basic_record(session, (float)logical_port),
	    &port->record, error);
}

static bool
port_purchase_accept_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_purchase_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_purchase_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector_number, sector,
	    error);
}

static bool
port_purchase_report(void *context, int logical_port, bool earth,
    struct yt_port *early_port, struct yt_port *terminal_port,
    float production[3], struct yt_error *error)
{
	struct yt_session *session = context;

	if (earth) {
		float earth_prices[4];

		if (!earth_report(session, early_port, earth_prices, error))
			return false;
		*terminal_port = *early_port;
		memset(production, 0, 3U * sizeof(production[0]));
		return true;
	}
	{
		struct yt_port_market_state market;
		struct yt_sector updater_sector = {0};

		updater_sector.port = (float)logical_port;
		if (!port_update(session, 0, NULL, &updater_sector, &market,
		    error))
			return false;
		*early_port = market.port;
		memcpy(production, market.port.production,
		    3U * sizeof(production[0]));
		return port_report_capture(session, logical_port, &market,
		    terminal_port, error);
	}
}

static bool
port_purchase_owner(void *context, const struct yt_port *port,
    uint8_t *name, size_t capacity, size_t *length,
	struct yt_error *error)
{
	return port_owner_row_capture(context, port, name, capacity, length,
	    error);
}

static bool
port_purchase_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_purchase_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PORT_PURCHASE_NO_PORT:
		return session_present_alert(session, text, length, "buy no-port row",
		    error);
	case YT_PORT_PURCHASE_ALREADY_OWNER:
		return session_present_alert(session, text, length,
		    "buy already-owner row", error);
	case YT_PORT_PURCHASE_PRICE:
		return session_present_paged_line(session, text, length, "buy price row", error);
	case YT_PORT_PURCHASE_UNAFFORDABLE:
		return session_present_alert(session, text, length,
		    "buy unaffordable row", error);
	case YT_PORT_PURCHASE_OFFER_LEADING_BLANK:
	case YT_PORT_PURCHASE_OFFER_TRAILING_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE,
		    kind == YT_PORT_PURCHASE_OFFER_LEADING_BLANK
		    ? "buy owner offer leading blank"
		    : "buy owner offer trailing blank", error);
	case YT_PORT_PURCHASE_OFFER_ROW:
		return session_present_paged_fragment(session, text, length);
	case YT_PORT_PURCHASE_DECLINED:
		return session_present_alert(session, text, length, "buy declined row",
		    error);
	default:
		return false;
	}
}

static bool
port_purchase_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (accepted == NULL
	    || !session_confirm(context, prompt, length, &answer, error))
		return false;
	*accepted = answer == YT_YES_NO_YES;
	return true;
}

static bool
port_purchase_accept(void *context,
    struct yt_port_purchase_accept_state *state, struct yt_error *error)
{
	static const struct yt_port_purchase_accept_ops ops = {
		port_purchase_accept_present,
		port_purchase_accept_read_port,
		port_purchase_accept_read_player,
		port_purchase_accept_write_player,
		port_purchase_accept_radio,
		port_purchase_accept_rename,
		port_purchase_accept_write_port,
		port_purchase_accept_hydrate,
	};

	return yt_port_purchase_accept_run(state, &ops, context, error);
}

static bool
command_buy_port(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_port_purchase_ops ops = {
		port_purchase_hydrate,
		port_purchase_read_sector,
		port_purchase_report,
		port_purchase_owner,
		port_purchase_present,
		port_purchase_confirm,
		port_purchase_accept,
	};
	const uint8_t *first =
	    (const uint8_t *)session->door->identity.real_first;
	struct yt_port_purchase_state state = {
		.current_player_record = session_record(session),
		.port_offset = session_port_offset(session),
		.conversion_mode =
		    session->presentation.sound.conversion_mode,
		.first_name = first,
		.first_name_length = strlen((const char *)first),
	};

	return yt_port_purchase_run(&state, &ops, session, error);
}

static bool
command_buy_port_cycle(struct yt_session *session, struct yt_error *error)
{
	return command_buy_port(session, error)
	    && display_current_sector_cached(session, error);
}

static bool
treasury_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_player_decode(player, &record);
	return true;
}

static bool
treasury_read_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_port_decode(port, &record);
	return true;
}

static bool
treasury_write_port(void *context, uint32_t physical_record,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
}

static bool
treasury_present(void *context, const uint8_t *text, size_t length,
    enum yt_treasury_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_TREASURY_OPENING_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "treasury opening blank", error);
	case YT_TREASURY_NO_PORTS:
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, text, length,
		    SESSION_PRESENT_BOLD_LINE, "treasury no-owned notice", error);
	case YT_TREASURY_HEADING_PREFIX:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_RAW, "treasury heading prefix", error);
	case YT_TREASURY_HEADING_SUFFIX:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury heading suffix", error);
	case YT_TREASURY_SCAN_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "treasury scan blank", error);
	case YT_TREASURY_SECTOR_FIELD:
		return session_fixed_width_bytes(session, text, length, 14.0f,
		    "treasury sector field", error);
	case YT_TREASURY_NAME_FIELD:
		return session_fixed_width_bytes(session, text, length, 25.0f,
		    "treasury port-name field", error);
	case YT_TREASURY_CREDIT_FIELD:
		return session_fixed_width_bytes(session, text, length, 20.0f,
		    "treasury credit field", error);
	case YT_TREASURY_ROW_TOTAL:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury row total", error);
	case YT_TREASURY_NONZERO_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "treasury nonzero-total blank", error);
	case YT_TREASURY_TOTAL_PORTS:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury total ports", error);
	case YT_TREASURY_WITH_CREDITS:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury credited ports", error);
	case YT_TREASURY_BARREN_PORTS:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury barren ports", error);
	case YT_TREASURY_TOTAL_CREDITS:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury total credits", error);
	case YT_TREASURY_SUMMARY_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "treasury summary blank", error);
	case YT_TREASURY_REPORT_RESULT:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury report result", error);
	case YT_TREASURY_COLLECTION_RESULT:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "treasury collection result", error);
	default:
		return false;
	}
}

static bool
treasury_write_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &player->record, error);
}

static bool
treasury_flush_player(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_flush(&session->door->game.database, error);
}

static bool
treasury_update_cache(void *context, const struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	session->player = *player;
	return true;
}

static bool
command_collect(struct yt_session *session,
    enum yt_treasury_caller_kind caller,
    struct yt_error *error)
{
	static const struct yt_treasury_ops ops = {
		treasury_read_player,
		treasury_read_port,
		treasury_write_port,
		treasury_present,
		treasury_write_player,
		treasury_flush_player,
		treasury_update_cache,
	};
	struct yt_treasury_state state = {
		.current_player_record = (float)session_record(session),
		.port_offset = session_port_offset(session),
		.planet_offset = session_planet_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
	};

	if (caller == YT_TREASURY_CALLER_MAIN_COLLECT
	    || caller == YT_TREASURY_CALLER_COMPUTER_COLLECT)
		state.collecting = true;
	else if (caller != YT_TREASURY_CALLER_COMPUTER_REPORT)
		return false;
	return yt_treasury_run(&state, &ops, session, error);
}

static bool
genesis_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
genesis_present(void *context, const uint8_t *text, size_t length,
    enum yt_genesis_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_GENESIS_PROPHECY_FIRST:
		return session_present_paged_line(session, text, length,
		    "Genesis prophecy first row", error);
	case YT_GENESIS_PROPHECY_SECOND:
		return session_present_paged_fragment(session, text, length);
	case YT_GENESIS_PROMPT_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "Genesis prompt leading blank", error);
	case YT_GENESIS_DISABLED:
		return session_present_alert(session, text, length, "Genesis disabled row",
		    error);
	case YT_GENESIS_DECLINED:
		return session_present_paged_line(session, text, length, "Genesis declined row",
		    error);
	case YT_GENESIS_INSUFFICIENT_FIRST:
		return session_present_paged_line(session, text, length,
		    "Genesis insufficient first row", error);
	case YT_GENESIS_INSUFFICIENT_SECOND:
		return session_present_paged_fragment(session, text, length);
	case YT_GENESIS_SUCCESS_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "Genesis success leading blank", error);
	case YT_GENESIS_SUCCESS_FIRST:
	case YT_GENESIS_SUCCESS_SECOND:
		yt_present_set_bold(&session->presentation, 1.0f);
		return session_present_paged_fragment(session, text, length);
	default:
		return false;
	}
}

static bool
genesis_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (accepted == NULL
	    || !session_confirm(context, prompt, length, &answer, error))
		return false;
	*accepted = answer == YT_YES_NO_YES;
	return true;
}

static bool
genesis_handoff_open_output(struct yt_text_output *output,
    struct yt_error *error)
{
	bool opened;

	opened = yt_text_output_open(output, "RMTINIT.TMP", error);
	if (!opened && output->last_output_open.basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_OPEN_OUTPUT,
		    output->last_output_open.basic_error);
	return opened;
}

static bool
genesis_handoff_print_command(struct yt_text_output *output,
    const uint8_t *line, size_t line_length, struct yt_error *error)
{
	bool printed;

	printed = yt_text_output_write(output, line, line_length, error);
	if (!printed && output->last_write.basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_PRINT_VALUE,
		    output->last_write.basic_error);
	return printed;
}

static bool
genesis_handoff_close_all(struct yt_session *session,
    struct yt_text_output *output, struct yt_error *error)
{
	struct yt_close_all_control controls[2];
	struct yt_close_all_result close_all;
	size_t control_count = 0U;
	size_t game_index = SIZE_MAX;
	size_t output_index;
	uint16_t basic_error = 0U;
	bool closed;

	/*
	 * The database file-1 control predates the new sequential file-5
	 * control.  CLOSE with no file number therefore walks file 5 first,
	 * appending its DOS EOF, and then closes file 1 before RUN.
	 */
	if (session->door->game_open) {
		game_index = control_count;
		controls[control_count++] = (struct yt_close_all_control){
			YT_CLOSE_ALL_HEAP_FILE, 0,
			session_close_game_all, session->door};
	}
	output_index = control_count;
	controls[control_count++] = (struct yt_close_all_control){
		YT_CLOSE_ALL_HEAP_FILE, 0,
		yt_text_output_close_all_method, output};
	closed = yt_close_all_run(controls, control_count, NULL, &close_all,
	    error);
	if (closed)
		return true;
	if (close_all.failed_index == output_index)
		basic_error = output->last_close.basic_error;
	else if (close_all.failed_index == game_index)
		basic_error = session->door->game.database.last_close.basic_error;
	if (basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_CLOSE_ALL, basic_error);
	return false;
}

static bool
genesis_handoff_run_program(struct yt_session *session,
    struct yt_error *error)
{
	char sibling[1024];
	char *arguments[2];

	if (!yt_platform_sibling_program(sibling, sizeof(sibling),
	    session->executable_path, "rmt-init", error))
		return false;
	arguments[0] = sibling;
	arguments[1] = NULL;
	if (fflush(NULL) != 0) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "flush before Genesis");
		}
		return false;
	}
	yt_door_shutdown_for_replace();
	if (!yt_platform_spawn(sibling, arguments, YT_SPAWN_REPLACE, NULL,
	    error))
		return false;
	return true; /* Unreachable after a successful RUN replacement. */
}

static bool
genesis_handoff(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_text_output output;
	uint8_t line[sizeof(session->door->command_line) + 2U];
	size_t line_length;
	bool result;

	line_length = strlen(session->door->command_line) + 2U;
	memcpy(line, session->door->command_line, line_length - 2U);
	line[line_length - 2U] = '\r';
	line[line_length - 1U] = '\n';
	yt_text_output_init(&output);
	result = session_close_file5(error)
	    && genesis_handoff_open_output(&output, error)
	    && genesis_handoff_print_command(&output, line, line_length, error)
	    && genesis_handoff_close_all(session, &output, error)
	    && genesis_handoff_run_program(session, error);
	yt_text_output_destroy(&output);
	return result;
}

static bool
command_genesis(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_genesis_ops ops = {
		genesis_hydrate,
		genesis_present,
		genesis_confirm,
		genesis_handoff,
	};
	uint8_t cached_trader[sizeof(session->player.name) - 1U];
	size_t cached_trader_length = strlen(session->player.name);
	struct yt_genesis_state state;

	if (cached_trader_length > sizeof(cached_trader))
		return port_report_failure(error, "Genesis cached trader length");
	memcpy(cached_trader, session->player.name, cached_trader_length);
	state = (struct yt_genesis_state){
		.current_player_record = session_record(session),
		.required_ports = session->door->game.config.genesis_ports,
		.cached_trader = cached_trader,
		.cached_trader_length = cached_trader_length,
	};
	return yt_genesis_run(&state, &ops, session, error);
}

static bool
projectile_planet_read(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	return read_planet_physical(context, physical_record, planet, error);
}

static bool
projectile_planet_write(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	return write_planet_physical(context, physical_record, planet, false,
	    error);
}

static bool
projectile_sector_read(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static bool
projectile_sector_write(void *context, uint32_t physical_record,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, &sector->record, error);
}

static bool
projectile_planet_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "cruise missile planet impact row", error);
}

static bool
projectile_planet_sound(void *context, float selector,
    struct yt_error *error)
{
	return session_sound(context, selector,
	    "cruise missile planet destruction sound", error);
}

static bool
missile_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, float *remaining, bool *early_return,
    struct yt_error *error)
{
	static const struct yt_projectile_planet_impact_ops impact_ops = {
		random_value,
		projectile_planet_read,
		projectile_planet_write,
		projectile_sector_read,
		projectile_sector_write,
		projectile_planet_present,
		session_append_news_bytes,
		projectile_planet_sound,
	};
	struct yt_planet planet;
	struct yt_planet updater_planet;
	struct yt_projectile_planet_impact_state impact_state;
	bool overflow;
	int logical_planet;
	uint32_t physical_planet;
	uint32_t physical_sector;
	float original_ore;
	bool friendly = false;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	size_t planet_name_length;
	size_t attacker_name_length;
	size_t direct_length;
	size_t news_length;

	if (early_return == NULL)
		return false;
	*early_return = false;
	if (*remaining <= 0.0f) {
		*early_return = true;
		return true;
	}
	logical_planet = (int)qb_cint_mbf32(sector->record.bytes + YT_F93,
	    0U, &overflow);
	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "cruise missile planet-link CINT");
		}
		return false;
	}
	if (logical_planet == 0)
		return true;
	physical_planet = yt_projectile_physical_record(
	    session_planet_offset(session), sector->planet);
	physical_sector = yt_projectile_physical_record(
	    session_sector_offset(session), (float)sector_number);
	if (!planet_update_cached_physical(session, physical_planet,
	    &updater_planet, NULL, error))
		return false;
	/* DS:1A48 remains the updater's ore value across the independent GET. */
	original_ore = updater_planet.production[0];
	if (!read_planet_physical(session, physical_planet, &planet, error))
		return false;
	if (!yt_planet_stored_name(&planet, planet_name, &planet_name_length,
	    error))
		return false;
	if (planet.owner == (float)session_record(session))
		friendly = true;
	else if (planet.owner > 1.0f
	    && planet.owner <= session_sector_offset(session)) {
		if (!yt_friendship_resolve(planet.owner,
		    (float)session_record(session),
		    session_sector_offset(session),
		    friendship_read_player, &session->door->game, &friendly,
		    error))
			return false;
		if (!read_planet_physical(session, physical_planet, &planet,
		    error))
			return false;
	}
	if (friendly) {
		if (!yt_projectile_friendly_planet_row(planet_name,
		    planet_name_length, direct_row, sizeof(direct_row),
		    &direct_length))
			return false;
		return session_present_text(session, direct_row, direct_length,
		    SESSION_PRESENT_LINE,
		    "cruise missile friendly-planet row", error);
	}
	if (!yt_player_stored_name(&session->player, attacker_name,
	    &attacker_name_length, error)
	    || !yt_projectile_planet_attack_rows(false, attacker_name,
	    attacker_name_length, planet_name, planet_name_length,
	    (float)sector_number, direct_row, sizeof(direct_row),
	    &direct_length, news_row, sizeof(news_row), &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "cruise missile planet-attack row", error)
	    || !session_append_news_bytes(session, news_row, news_length, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "cruise missile planet attack sound", error))
		return false;
	impact_state.planet = &planet;
	impact_state.updater_ore = original_ore;
	impact_state.remaining = remaining;
	impact_state.physical_planet = physical_planet;
	impact_state.physical_sector = physical_sector;
	if (!yt_projectile_planet_impact_run(&impact_state, &impact_ops,
	    session, error))
		return false;
	*early_return = impact_state.early_return;
	return true;
}

static bool
deploy_victim_mines(struct yt_session *session, int sector_number,
    float mines, struct yt_error *error)
{
	struct yt_sector sector;

	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_mines_overlay(&sector, mines))
		return false;
	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)sector_number),
	    &sector.record, error);
}

static bool
plasma_fighter_owner(void *context, float owner, uint8_t *label,
    size_t *label_length, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_player defender;
	bool overflow;
	int owner_record = (int)qb_cint((double)owner, &overflow);

	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "plasma fighter owner CINT");
		}
		return false;
	}
	if (!yt_game_read_player(&session->door->game, owner_record, &defender,
	    error))
		return false;
	return yt_player_stored_name(&defender, label, label_length, error);
}

static bool
plasma_fighter_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_fighter_output_kind kind,
    struct yt_error *error)
{
	return session_present_text(context, text, length,
	    kind == YT_PROJECTILE_PLASMA_FIGHTER_ENCOUNTER
	    ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    kind == YT_PROJECTILE_PLASMA_FIGHTER_ENCOUNTER
	    ? "plasma defense report" : "plasma destroyed-defense row", error);
}

static bool
plasma_fighter_sound(void *context, float selector, struct yt_error *error)
{
	struct yt_session *session = context;

	/* The fighter model assigns through its by-reference carrier first. */
	yt_present_set_bold(&session->presentation,
	    session->presentation.bold);
	return session_sound(context, selector,
	    "plasma fighter-defense sound", error);
}

static bool
plasma_fighter_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, (int)sector, value,
	    error);
}

static bool
plasma_fighter_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, sector),
	    &value->record, error);
}

static bool
plasma_fighter_victory(void *context, struct yt_error *error)
{
	return xannor_victory(context, error);
}

static bool
plasma_mine_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "plasma sector-mine sound",
	    error);
}

static bool
plasma_mine_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length,
	    SESSION_PRESENT_BOLD_LINE, "plasma destroyed-mines row", error);
}

static bool
plasma_player_read(void *context, int player_record,
    struct yt_player *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, value,
	    error);
}

static bool
plasma_player_write(void *context, int player_record,
    const struct yt_player *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)player_record, &value->record, error);
}

static void
plasma_player_save_foreground(void *context, float *saved_foreground)
{
	struct yt_session *session = context;

	if (saved_foreground != NULL)
		*saved_foreground = session_foreground(session);
}

static void
plasma_player_color(void *context, float foreground)
{
	session_set_foreground(context, foreground);
}

static bool
plasma_player_sound(void *context, float selector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_sound(session, selector, "plasma player-attack sound",
	    error);
}

static void
plasma_player_restore_foreground(void *context, float saved_foreground)
{
	session_set_foreground(context, saved_foreground);
}

static bool
plasma_player_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_player_output_kind kind,
    struct yt_error *error)
{
	return session_present_text(context, text, length,
	    SESSION_PRESENT_BOLD_LINE,
	    kind == YT_PROJECTILE_PLASMA_PLAYER_FIRST_ROW
	    ? "plasma player attack first row"
	    : "plasma player attack second row", error);
}

static bool
plasma_killed_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_killed_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;
	const char *operation;

	/* The killed-player model assigns through its by-reference carrier. */
	yt_present_set_blink(&session->presentation,
	    session->presentation.blink);
	if (kind == YT_PROJECTILE_PLASMA_KILLED_DESTROYED_ROW)
		operation = "plasma victim-destruction row";
	else if (kind == YT_PROJECTILE_PLASMA_KILLED_SELF_DESTROYED_ROW)
		operation = "plasma self-destruction row";
	else
		operation = "plasma carried-mine warning";
	return session_present_text(context, text, length,
	    SESSION_PRESENT_BOLD_LINE, operation, error);
}

static bool
plasma_killed_read_sector(void *context, int sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, sector, value, error);
}

static bool
plasma_killed_write_sector(void *context, int sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, (float)sector),
	    &value->record, error);
}

static bool
plasma_killed_death(void *context, int victim, int shooter,
    struct yt_error *error)
{
	return kill_player(context, victim, (float)shooter, error);
}

static bool
plasma_killed_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "plasma salvage sound", error);
}

static bool
plasma_killed_salvage(void *context, int victim, int shooter,
    struct yt_error *error)
{
	return yt_session_salvage_player(context, victim, shooter, error);
}

static bool
plasma_planet_update(void *context, int logical_planet, float *stale_ore,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct planet_update_cache cache;
	struct yt_planet planet;

	if (stale_ore == NULL)
		return false;
	if (!planet_update_cached(session, logical_planet, &planet, &cache,
	    error))
		return false;
	/* B735 exposes the updater's returned P(1), not its stored P(1)-A(1). */
	*stale_ore = cache.rate[1];
	return true;
}

static bool
plasma_planet_read(void *context, int logical_planet,
    struct yt_planet *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_planet(session, logical_planet, value,
	    error);
}

static bool
plasma_planet_write(void *context, int logical_planet,
    const struct yt_planet *value, struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = session_planet_basic_record(session,
	    (float)logical_planet);

	return yt_database_write(&session->door->game.database, (size_t)physical,
	    &value->record, error);
}

static bool
plasma_planet_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_planet_output_kind kind,
    struct yt_error *error)
{
	const char *operation;

	switch (kind) {
	case YT_PROJECTILE_PLASMA_PLANET_HIT_ROW:
		operation = "plasma planet-hit row";
		break;
	case YT_PROJECTILE_PLASMA_PLANET_PRODUCTIVITY_ROW:
		operation = "plasma productivity row";
		break;
	case YT_PROJECTILE_PLASMA_PLANET_DESTROYED_ROW:
		operation = "plasma planet-destroyed row";
		break;
	case YT_PROJECTILE_PLASMA_PLANET_GROUND_ROW:
		operation = "plasma ground-force row";
		break;
	default:
		return false;
	}
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    operation, error);
}

static bool
plasma_planet_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector,
	    selector == 2.0f ? "plasma planet attack sound"
	    : "plasma planet destruction sound", error);
}

static bool
cruise_defense_owner(void *context, float owner, uint8_t *name,
    size_t *name_length, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_player defender;
	uint32_t record = qb_brun_random_record_number(owner);

	if (!yt_game_read_player(&session->door->game, (int)record, &defender,
	    error))
		return false;
	return yt_player_stored_name(&defender, name, name_length, error);
}

static bool
cruise_defense_friendship(void *context, float owner, bool *friendly,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_friendship_resolve(owner, (float)session_record(session),
	    session_sector_offset(session),
	    friendship_read_player, &session->door->game, friendly, error);
}

static bool
cruise_defense_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile defense report", error);
}

static bool
cruise_defense_sound(void *context, float selector, struct yt_error *error)
{
	struct yt_session *session = context;

	yt_present_set_bold(&session->presentation, 1.0f);
	return session_sound(session, selector,
	    "cruise missile fighter-defense sound", error);
}

static bool
cruise_defense_damage_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "cruise missile destroyed-defense row", error);
}

static bool
cruise_defense_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, (int)sector, value,
	    error);
}

static bool
cruise_defense_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, sector),
	    &value->record, error);
}

static bool
cruise_defense_victory(void *context, struct yt_error *error)
{
	return xannor_victory(context, error);
}

static bool
cruise_mine_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile sector-mine row", error);
}

static bool
cruise_mine_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector,
	    "cruise missile sector-mine sound", error);
}

static bool
cruise_mine_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, (int)sector, value,
	    error);
}

static bool
cruise_mine_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)session_sector_basic_record(session, sector),
	    &value->record, error);
}

enum missile_sector_route {
	MISSILE_SECTOR_RETURN,
	MISSILE_SECTOR_POST_IMPACT,
};

static bool
missile_sector(struct yt_session *session, int sector_number,
    float *remaining, int *counterattack, int *xannor_provoker,
	float *last_mine_news_sector, enum missile_sector_route *route,
	struct yt_error *error)
{
	static const struct yt_projectile_defense_front_ops defense_ops = {
		cruise_defense_owner,
		cruise_defense_friendship,
		cruise_defense_present,
		cruise_defense_sound,
	};
	static const struct yt_projectile_defense_combat_ops combat_ops = {
		random_value,
		cruise_defense_damage_present,
		session_append_news_bytes,
		cruise_defense_read_sector,
		cruise_defense_write_sector,
		cruise_defense_victory,
	};
	static const struct yt_projectile_sector_mine_ops mine_ops = {
		cruise_mine_read_sector,
		cruise_mine_present,
		cruise_mine_sound,
		session_append_news_bytes,
		cruise_mine_write_sector,
	};
	struct yt_sector sector;
	struct yt_projectile_defense_combat_state combat;
	struct yt_projectile_defense_front_state defense;
	struct yt_projectile_sector_mine_state mine;
	struct yt_projectile_sector_probe_state probe;
	int basic;

	if (route == NULL)
		return false;
	*route = MISSILE_SECTOR_RETURN;
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	probe.sector = &sector;
	probe.hop = (float)sector_number;
	probe.player_terminal = session_sector_offset(session);
	probe.player_cache = &session->player_cache;
	probe.xannor_provoker = (float)*xannor_provoker;
	if (!yt_projectile_sector_probe_run(&probe, error))
		return false;
	if (probe.presence == 0.0f) {
		*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
	defense.sector = (float)sector_number;
	defense.fighters = (double)sector.fighters;
	defense.owner = sector.fighter_owner;
	defense.shooter = session_record(session);
	if (!yt_projectile_defense_front_run(&defense, &defense_ops, session,
	    error))
		return false;
	if (defense.route == YT_PROJECTILE_DEFENSE_NO_DEFENSE)
		goto missile_mines;
	if (defense.route == YT_PROJECTILE_DEFENSE_FRIENDLY)
		goto missile_mines;
	combat.sector = (float)sector_number;
	combat.fighters = (double)sector.fighters;
	combat.owner = sector.fighter_owner;
	combat.shooter = session_record(session);
	combat.headquarters = session->door->game.config.headquarters;
	combat.shooter_name = (const uint8_t *)session->player.name;
	combat.shooter_name_length = strlen(session->player.name);
	combat.missiles = remaining;
	combat.xannor_provoker = xannor_provoker;
	if (!yt_projectile_defense_combat_run(&combat, &combat_ops, session,
	    error))
		return false;
	if (combat.route == YT_PROJECTILE_DEFENSE_RETURN)
		return true;

missile_mines:
	mine.sector = (float)sector_number;
	mine.shooter_name = (const uint8_t *)session->player.name;
	mine.shooter_name_length = strlen(session->player.name);
	mine.missiles = remaining;
	mine.last_news_sector = last_mine_news_sector;
	if (!yt_projectile_sector_mine_run(&mine, &mine_ops, session, error))
		return false;
	if (mine.route == YT_PROJECTILE_SECTOR_MINE_RETURN)
		return true;
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session_sector_offset(session); ++basic) {
		struct yt_player target;
		struct yt_player presentation_target;
		struct yt_projectile_damage_result damage;
		bool scanner_disabled = false;
		uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
		uint8_t victim_name[YT_TEXT_FIELD_SIZE];
		uint8_t first_news[256];
		uint8_t first_direct[256];
		size_t attacker_length;
		size_t victim_length;
		size_t first_news_length;
		size_t first_direct_length;
		char shield_text[64];
		char fighter_text[64];
		char row[256];

		enum yt_projectile_candidate_route candidate_route =
		    yt_projectile_candidate_route(basic, session_record(session),
		    session_player_cache_value(session, basic,
		    YT_PLAYER_CACHE_SECTOR), (float)sector_number,
		    *remaining);

		if (candidate_route == YT_PROJECTILE_CANDIDATE_TERMINATE)
			break;
		if (candidate_route == YT_PROJECTILE_CANDIDATE_SKIP)
			continue;
		/* YT-SUB:974F is called for its exact GET effects; its result is ignored. */
		{
			bool ignored_friendship;

			if (!yt_friendship_resolve((float)basic,
			    (float)session_record(session),
			    session_sector_offset(session),
			    friendship_read_player, &session->door->game,
			    &ignored_friendship, error))
				return false;
		}
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (!yt_projectile_candidate_admitted(basic,
		    session_player_cache_value(session, basic,
		    YT_PLAYER_CACHE_CLOAK), *xannor_provoker))
			continue;
		if (!session_sound(session, 2.0f,
		    "cruise missile player-attack sound", error))
			return false;
		if (!yt_projectile_player_damage(&target, remaining,
		    random_value, session, &damage, error))
			return false;
		scanner_disabled = damage.scanner_disabled;
		session_set_foreground(session, 5.0f);
		if (!yt_game_read_player(&session->door->game, basic,
		    &presentation_target, error))
			return false;
		qb_str_single(shield_text, sizeof(shield_text), target.shields);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    damage.fighters);
		if (!yt_player_stored_name(&session->player, attacker_name,
		    &attacker_length, error)
		    || !yt_player_stored_name(&presentation_target, victim_name,
		    &victim_length, error)
		    || !yt_projectile_attack_first_rows(false,
		    attacker_name, attacker_length, victim_name, victim_length,
		    (float)sector_number, first_news, sizeof(first_news),
		    &first_news_length, first_direct, sizeof(first_direct),
		    &first_direct_length)
		    || !session_append_news_bytes(session, first_news, first_news_length,
		    error)
		    || !session_present_text(session, first_direct,
		    first_direct_length, SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack first row", error))
			return false;
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!append_news(session, row, error))
			return false;
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_BOLD_LINE,
		    "cruise missile player attack second row", error))
			return false;
		session_set_foreground(session, 0.0f);
		if (!yt_projectile_player_survives(target.shields)) {
			float mines;
			uint8_t killed_name[YT_TEXT_FIELD_SIZE];
			uint8_t destroyed_row[128];
			uint8_t warning_row[160];
			size_t killed_name_length;
			size_t destroyed_length;
			size_t warning_length;

			if (!yt_game_read_player(&session->door->game, basic,
			    &target, error))
				return false;
			if (!yt_player_stored_name(&target, killed_name,
			    &killed_name_length, error)
			    || !yt_projectile_destroyed_rows(killed_name,
			    killed_name_length, destroyed_row, sizeof(destroyed_row),
			    &destroyed_length, warning_row, sizeof(warning_row),
			    &warning_length))
				return false;
			if (!yt_projectile_victim_mines_overlay(&target, &mines)
			    || !yt_database_write(&session->door->game.database,
			    (size_t)basic, &target.record, error))
				return false;
			yt_present_set_blink(&session->presentation, 1.0f);
			if (!session_present_text(session, destroyed_row,
			    destroyed_length, SESSION_PRESENT_BOLD_LINE,
			    "cruise missile destroyed-player row", error))
				return false;
			if (mines != 0.0f) {
				yt_present_set_blink(&session->presentation, 1.0f);
				if (!session_present_text(session, warning_row,
				    warning_length,
				    SESSION_PRESENT_BOLD_LINE,
				    "cruise missile carried-mine warning", error))
					return false;
			}
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number,
			    mines, error))
				return false;
			if (!kill_player(session, basic,
			    (float)session_record(session), error))
				return false;
			if (yt_projectile_salvage_admitted(*counterattack,
			    *xannor_provoker)) {
				if (!session_sound(session, 3.0f,
				    "cruise missile salvage sound", error)
				    || !yt_session_salvage_player(session, basic,
				    session_record(session), error))
					return false;
			}
			switch (yt_projectile_death_continuation(*remaining,
			    mines)) {
			case YT_PROJECTILE_DEATH_REENTER_MINES:
				goto missile_mines;
			case YT_PROJECTILE_DEATH_RETURN:
				return true;
			case YT_PROJECTILE_DEATH_NEXT_PLAYER:
				break;
			}
		}
		else {
			struct yt_player persistence;

			if (!yt_game_read_player(&session->door->game, basic,
			    &persistence, error))
				return false;
			if (!yt_projectile_survivor_overlay(&persistence,
			    target.shields, (double)target.fighters,
			    target.danger_scanner, scanner_disabled))
				return false;
			if (!yt_database_write(&session->door->game.database,
			    (size_t)basic, &persistence.record, error))
				return false;
			{
				uint8_t counterattack_raw[4];

				if (yt_projectile_survivor_store_counterattack(
				    session_record(session), basic, counterattack,
				    counterattack_raw))
					session_store_counterattack_player(session,
					    counterattack_raw);
			}
			return true;
		}
	}
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	{
		bool early_return;

		if (!missile_planet_impact(session, sector_number, &sector,
		    remaining, &early_return, error))
			return false;
		if (!early_return)
			*route = MISSILE_SECTOR_POST_IMPACT;
		return true;
	}
}

static bool
plasma_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, const uint8_t *attacker,
    size_t attacker_length, double *energy, struct yt_error *error)
{
	static const struct yt_projectile_plasma_planet_ops ops = {
		plasma_planet_update,
		plasma_planet_read,
		plasma_planet_write,
		plasma_killed_read_sector,
		plasma_killed_write_sector,
		plasma_planet_present,
		session_append_news_bytes,
		plasma_planet_sound,
		random_value,
	};
	struct yt_projectile_plasma_planet_state state;
	bool overflow;
	int logical_planet;

	if (*energy <= 0.0)
		return true;
	logical_planet = (int)qb_cint_mbf32(sector->record.bytes + YT_F93,
	    0U, &overflow);
	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "plasma planet-link CINT");
		}
		return false;
	}
	if (logical_planet == 0)
		return true;
	memset(&state, 0, sizeof(state));
	state.planet = logical_planet;
	state.sector = sector_number;
	state.attacker = attacker;
	state.attacker_length = attacker_length;
	state.energy = energy;
	return yt_projectile_plasma_planet_run(&state, &ops, session, error);
}

static bool
plasma_sector_loaded(struct yt_session *session, int sector_number,
    const struct yt_sector *initial, const uint8_t *attacker,
    size_t launch_attacker_length, double *energy, struct yt_error *error)
{
	static const struct yt_projectile_plasma_fighter_ops fighter_ops = {
		plasma_fighter_owner,
		plasma_fighter_present,
		plasma_fighter_sound,
		random_value,
		session_append_news_bytes,
		plasma_fighter_read_sector,
		plasma_fighter_write_sector,
		plasma_fighter_victory,
	};
	static const struct yt_projectile_plasma_mine_ops mine_ops = {
		plasma_mine_sound,
		session_append_news_bytes,
		random_value,
		plasma_mine_present,
		plasma_fighter_read_sector,
		plasma_fighter_write_sector,
	};
	static const struct yt_projectile_plasma_player_ops player_ops = {
		plasma_player_read,
		plasma_player_write,
		plasma_player_save_foreground,
		plasma_player_color,
		plasma_player_sound,
		random_value,
		session_append_news_bytes,
		plasma_player_present,
		plasma_player_restore_foreground,
	};
	static const struct yt_projectile_plasma_killed_ops killed_ops = {
		plasma_player_read,
		plasma_player_write,
		plasma_killed_present,
		plasma_killed_read_sector,
		plasma_killed_write_sector,
		plasma_killed_death,
		plasma_killed_sound,
		plasma_killed_salvage,
	};
	struct yt_sector sector;
	struct yt_projectile_plasma_fighter_state fighter;
	struct yt_projectile_plasma_mine_state mine;
	struct yt_projectile_plasma_dispatch_state dispatch;
	struct yt_projectile_plasma_player_state player;
	struct yt_projectile_plasma_killed_state killed;
	float planet_link;
	int basic;

	if (initial != NULL)
		sector = *initial;
	else if (!session_read_sector(session, sector_number,
	    &sector, error))
		return false;
	memset(&fighter, 0, sizeof(fighter));
	fighter.sector = (float)sector_number;
	fighter.fighters = (double)sector.fighters;
	fighter.owner = sector.fighter_owner;
	fighter.shooter = session_record(session);
	fighter.headquarters = session->door->game.config.headquarters;
	fighter.attacker = attacker;
	fighter.attacker_length = launch_attacker_length;
	fighter.energy = energy;
	fighter.bold = &session->presentation.bold;
	if (!yt_projectile_plasma_fighter_run(&fighter, &fighter_ops, session,
	    error))
		return false;
	if (fighter.route == YT_PROJECTILE_PLASMA_FIGHTER_FOOTER)
		return true;
plasma_reload_sector:
	/* B099 performs a new sector GET before caching mines and planet link. */
	if (!session_read_sector(session, sector_number, &sector,
	    error))
		return false;
	planet_link = sector.planet;
	memset(&mine, 0, sizeof(mine));
	mine.sector = (float)sector_number;
	mine.mines = (double)sector.mines;
	mine.attacker = attacker;
	mine.attacker_length = launch_attacker_length;
	mine.energy = energy;
	if (!yt_projectile_plasma_mine_run(&mine, &mine_ops, session, error))
		return false;
	if (mine.route == YT_PROJECTILE_PLASMA_MINE_FOOTER)
		return true;
	memset(&dispatch, 0, sizeof(dispatch));
	dispatch.sector = (float)sector_number;
	dispatch.planet_link = planet_link;
	memcpy(dispatch.planet_link_raw, sector.record.bytes + YT_F93,
	    sizeof(dispatch.planet_link_raw));
	dispatch.conversion_mode = session->presentation.sound.conversion_mode;
	dispatch.player_terminal = session_sector_offset(session);
	dispatch.player_cache = &session->player_cache;
	for (;;) {
		dispatch.energy = *energy;
		if (!yt_projectile_plasma_dispatch_run(&dispatch, error))
			return false;
		if (dispatch.route != YT_PROJECTILE_PLASMA_DISPATCH_PLAYER)
			break;
		basic = dispatch.selected_player;
		memset(&player, 0, sizeof(player));
		player.target = basic;
		player.sector = (float)sector_number;
		player.attacker = attacker;
		player.attacker_length = launch_attacker_length;
		player.energy = energy;
		player.foreground = session_foreground(session);
		if (!yt_projectile_plasma_player_run(&player, &player_ops, session,
		    error))
			return false;
		if (player.route == YT_PROJECTILE_PLASMA_PLAYER_KILLED) {
			memset(&killed, 0, sizeof(killed));
			killed.victim = basic;
			killed.shooter = session_record(session);
			killed.sector = sector_number;
			killed.energy = energy;
			killed.blink = &session->presentation.blink;
			killed.destroyed = &session->destroyed;
			killed.player_cache = &session->player_cache;
			if (!yt_projectile_plasma_killed_run(&killed, &killed_ops,
			    session, error))
				return false;
			if (killed.route ==
			    YT_PROJECTILE_PLASMA_KILLED_RELOAD_SECTOR)
				goto plasma_reload_sector;
			if (killed.route == YT_PROJECTILE_PLASMA_KILLED_FOOTER)
				return true;
		}
		if (player.route == YT_PROJECTILE_PLASMA_PLAYER_FOOTER)
			return true;
		dispatch.resume_after_player = true;
	}
	if (dispatch.route == YT_PROJECTILE_PLASMA_DISPATCH_FOOTER
	    || dispatch.route == YT_PROJECTILE_PLASMA_DISPATCH_NEXT_HOP)
		return true;
	/* The B099 dispatch cached this link before mines and the player scan. */
	sector.planet = planet_link;
	return plasma_planet_impact(session, sector_number, &sector, attacker,
	    launch_attacker_length, energy, error);
}

struct plasma_opening_context {
	struct yt_session *session;
	size_t wait_count;
};

static bool
plasma_opening_sound(void *context, float selector, struct yt_error *error)
{
	struct plasma_opening_context *opening = context;

	return session_sound(opening->session, selector, selector == 4.0f
	    ? "plasma launch sound" : "plasma bolt firing sound", error);
}

static bool
plasma_opening_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error)
{
	struct plasma_opening_context *opening = context;

	return session_present_text(opening->session, text, length,
	    kind == YT_PROJECTILE_OPENING_RAW ? SESSION_PRESENT_RAW
	    : SESSION_PRESENT_LINE, kind == YT_PROJECTILE_OPENING_RAW
	    ? "plasma loading text" : "plasma opening line", error);
}

static bool
plasma_opening_wait(void *context, float duration, struct yt_error *error)
{
	struct plasma_opening_context *opening = context;
	const char *operation;

	if (duration != 1.0f || opening->wait_count >= 2U)
		return false;
	operation = opening->wait_count++ == 0U
	    ? "plasma launch wait" : "plasma opening wait";
	return session_wait(opening->session, 1.0, operation, error);
}

static bool
projectile_opening(struct yt_session *session, float amount, bool plasma,
    float *last_mine_news_sector, double *energy, float *hop_loss,
    uint8_t *attacker, size_t attacker_capacity, size_t *attacker_length,
    struct yt_error *error)
{
	static const struct yt_projectile_plasma_opening_ops plasma_ops = {
		plasma_opening_sound,
		plasma_opening_present,
		plasma_opening_wait,
	};
	struct yt_projectile_plasma_opening_state state;
	struct plasma_opening_context opening = {
		.session = session,
	};
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	size_t player_name_length;

	if (!plasma) {
		static const uint8_t loading[] =
		    "Loading course into misile targeting computer.";
		static const uint8_t tracking[] = "*** Tracking Report ***";

		*energy = 0.0;
		*hop_loss = 0.0f;
		*attacker_length = 0U;
		if (!session_sound(session, 4.0f,
		    "cruise missile launch sound", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error)
		    || !session_present_text(session, loading,
		    sizeof(loading) - 1U, SESSION_PRESENT_RAW,
		    "cruise missile loading text", error)
		    || !session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "cruise missile opening line", error))
			return false;
		*last_mine_news_sector = 0.0f;
		return session_present_text(session, tracking,
		    sizeof(tracking) - 1U, SESSION_PRESENT_LINE,
		    "cruise missile tracking row", error);
	}
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	memset(&state, 0, sizeof(state));
	state.bolts = amount;
	state.player_name = player_name;
	state.player_name_length = player_name_length;
	if (!yt_projectile_plasma_opening_run(&state, &plasma_ops, &opening,
	    error))
		return false;
	if (state.attacker_length > attacker_capacity)
		return false;
	if (state.attacker_length != 0U)
		memcpy(attacker, state.attacker, state.attacker_length);
	*attacker_length = state.attacker_length;
	*energy = state.energy;
	*hop_loss = state.hop_loss;
	return true;
}

static bool
route_failure_report(struct yt_session *session, struct yt_error *error)
{
	uint8_t row[96];
	size_t length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "projectile route failure blank", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "projectile route failure blank", error)
	    || !yt_projectile_route_failure_row(false, row, sizeof(row),
	    &length))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "projectile route failure row", error);
}

static bool
missile_route_failure_suffix(struct yt_session *session,
    struct yt_error *error)
{
	uint8_t row[32];
	size_t length;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "cruise missile self-destruct blank", error)
	    || !yt_projectile_route_failure_row(true, row, sizeof(row),
	    &length))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile self-destruct row", error);
}

static bool
plasma_footer(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t row[] = "Plasma bolts dissipated.";

	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer leading blank", error)
	    && session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE, "plasma footer row", error)
	    && session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "plasma footer trailing blank", error);
}

static bool
missile_footer(struct yt_session *session, struct yt_error *error)
{
	uint8_t row[32];
	size_t length;

	return yt_projectile_footer_row(row, sizeof(row), &length)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "cruise missile end report", error);
}

static bool
cruise_route_entry_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

struct plasma_route_context {
	struct yt_session *session;
	struct projectile_route_state *route;
	int *xannor_provoker;
	const uint8_t *attacker;
	size_t attacker_length;
};

static bool
plasma_route_build(void *context, float *origin, float *destination,
    int16_t *route, size_t route_capacity, float *status,
    struct yt_error *error)
{
	struct plasma_route_context *route_context = context;
	struct yt_session *session = route_context->session;
	bool found;
	enum yt_route_outcome outcome;
	bool success;

	(void)route;
	(void)route_capacity;
	success = build_projectile_route(session, route_context->route, false, &found,
	    &outcome, status, error);
	*origin = route_context->route->origin;
	*destination = route_context->route->destination;
	return success;
}

static int16_t
plasma_route_read(void *context, int16_t index)
{
	struct plasma_route_context *route_context = context;

	return route_context->session->route_second[index];
}

static void
plasma_route_write(void *context, int16_t index, int16_t value)
{
	struct plasma_route_context *route_context = context;

	route_context->session->route_second[index] = value;
}

static void
plasma_route_arguments_changed(void *context, float origin,
    float destination, enum yt_projectile_plasma_argument_change change)
{
	struct plasma_route_context *route_context = context;

	if (change == YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO)
		route_context->route->origin = 0.0f;
	else if (change == YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN)
		route_context->route->origin = origin;
	else if (change == YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION)
		route_context->route->destination = destination;
}

static bool
plasma_route_line(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_route_context *route_context = context;

	return session_present_text(route_context->session, text, length,
	    SESSION_PRESENT_LINE,
	    "plasma route line", error);
}

static bool
plasma_route_attention(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_route_context *route_context = context;

	return session_attention_bytes(route_context->session, text, length,
	    "plasma black-hole attention", error);
}

static bool
plasma_route_wait(void *context, float duration, struct yt_error *error)
{
	struct plasma_route_context *route_context = context;

	return duration == 0.5f
	    && session_wait(route_context->session, 0.5, "plasma hop wait",
	    error);
}

static bool
plasma_route_random(void *context, float *value, struct yt_error *error)
{
	struct plasma_route_context *route_context = context;

	return random_value(route_context->session, value, error);
}

static bool
plasma_route_impact(void *context, int hop, double *energy,
    enum yt_projectile_plasma_impact_route *route, struct yt_error *error)
{
	struct plasma_route_context *route_context = context;
	struct yt_session *session = route_context->session;
	struct yt_sector sector;
	struct yt_projectile_sector_probe_state probe;

	if (!session_read_sector(session, hop, &sector, error))
		return false;
	memset(&probe, 0, sizeof(probe));
	probe.sector = &sector;
	probe.player_cache = &session->player_cache;
	probe.hop = (float)hop;
	probe.player_terminal = session_sector_offset(session);
	probe.xannor_provoker = route_context->xannor_provoker != NULL
	    ? (float)*route_context->xannor_provoker : 0.0f;
	if (!yt_projectile_sector_probe_run(&probe, error))
		return false;
	if (probe.presence == 0.0f) {
		*route = YT_PROJECTILE_PLASMA_NEXT_HOP;
		return true;
	}
	if (!plasma_sector_loaded(session, hop, &sector,
	    route_context->attacker, route_context->attacker_length, energy,
	    error))
		return false;
	*route = *energy < 1.0 ? YT_PROJECTILE_PLASMA_FOOTER
	    : YT_PROJECTILE_PLASMA_NEXT_HOP;
	return true;
}

static bool
plasma_route_footer(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct plasma_route_context *route_context = context;

	(void)text;
	(void)length;
	return plasma_footer(route_context->session, error);
}

static bool
launch_projectile(struct yt_session *session, float *target, float *amount,
    bool plasma, struct projectile_route_state *route,
    float *origin_alias, const uint8_t origin_raw[4],
    const uint8_t target_raw[4], const uint8_t amount_raw[4],
    int *pending_counterattack, int *pending_xannor, struct yt_error *error)
{
	float destination = *target;
	bool overflow;
	bool found;
	int cursor;
	float *missiles = amount;
	double energy;
	float hop_loss;
	uint8_t attacker[YT_PROJECTILE_ATTACKER_CAPACITY];
	size_t attacker_length;
	int local_counterattack = 0;
	int local_xannor_provoker = 0;
	int *counterattack = pending_counterattack != NULL
	    ? pending_counterattack : &local_counterattack;
	int *xannor_provoker = pending_xannor != NULL
	    ? pending_xannor : &local_xannor_provoker;
	float last_mine_news_sector;
	int start = (int)(origin_alias != NULL
	    ? *origin_alias : session->player.sector);

	if (origin_raw != NULL && target_raw != NULL && amount_raw != NULL)
		projectile_route_store_raw(route, origin_raw,
		    target_raw, amount_raw);
	else
		projectile_route_store(route, *origin_alias, *target,
		    *missiles);
	(void)qb_cint_mode((double)route->destination,
	    session->presentation.sound.conversion_mode, &overflow);
	if (overflow)
		return true;
	if (!projectile_opening(session, *amount, plasma,
	    &last_mine_news_sector, &energy, &hop_loss, attacker,
	    sizeof(attacker), &attacker_length, error))
		return false;
	if (plasma) {
		static const struct yt_projectile_plasma_route_ops ops = {
			plasma_route_build,
			plasma_route_line,
			plasma_route_attention,
			plasma_route_wait,
			plasma_route_random,
			plasma_route_impact,
			plasma_route_footer,
			plasma_route_read,
			plasma_route_write,
			plasma_route_arguments_changed,
		};
		float local_origin = (float)start;
		float *origin = origin_alias != NULL ? origin_alias : &local_origin;
		struct plasma_route_context route_context = {
			session,
			route,
			xannor_provoker,
			attacker,
			attacker_length,
		};

		struct yt_projectile_plasma_route_state state = {
			origin,
			target,
			&energy,
			hop_loss,
			{session->disruption_sectors[0],
			 session->disruption_sectors[1]},
			session_sector_offset(session),
			session_port_offset(session),
			NULL,
			0U,
			YT_ROUTE_CAPACITY * 4U,
			0.0f,
			0.0f,
			0U,
			0U,
		};
		bool result = yt_projectile_plasma_route_run(&state, &ops,
		    &route_context, error);

		return result;
	}
	for (;;) {
		bool rerouted = false;
		enum yt_route_outcome route_outcome;
		float route_status;
		struct yt_projectile_route_entry_state route_entry;

		bool route_success = build_projectile_route(session, route,
		    yt_projectile_route_avoid_enabled(plasma, *counterattack,
		    session_record(session)), &found, &route_outcome, &route_status,
		    error);

		projectile_route_load(route, origin_alias, target,
		    missiles);
		start = (int)*origin_alias;
		destination = *target;
		if (!route_success)
			return false;
		if (route_outcome == YT_ROUTE_NOT_FOUND
		    && !route_failure_report(session, error)) {
			return false;
		}
		if (route_status != 0.0f) {
			if (!missile_route_failure_suffix(session, error))
				return false;
			return true;
		}
		route_entry.shooter = session_record(session);
		route_entry.maximum_player_record =
		    session_sector_offset(session);
		route_entry.start = (float)start;
		if (!yt_projectile_route_entry_run(&route_entry,
		    cruise_route_entry_read_player, session, error))
			return false;
		cursor = (int)route_entry.current_hop;
		for (;;) {
			int next = session->route_second[cursor];

			if (!yt_projectile_route_has_next((int16_t)next))
				break;
			if (session_is_disruption_sector(session, (float)next)) {
				uint8_t row[160];
				size_t row_length;
				float draw;

				if (!session_present_text(session, NULL, 0U,
				    SESSION_PRESENT_LINE, "cruise black-hole blank", error)
				    || !yt_projectile_cruise_reroute_row((float)next, row,
				    sizeof(row), &row_length)
				    || !session_attention_bytes(session, row, row_length,
				    "cruise black-hole attention", error)) {
					projectile_route_store(route, *origin_alias,
					    *target, *missiles);
					return false;
				}
				*origin_alias = (float)next;
				projectile_route_store(route, *origin_alias, *target,
				    *missiles);
				if (!random_value(session, &draw, error))
					return false;
				*target = yt_projectile_cruise_reroute_destination(draw,
				    session_sector_offset(session),
				    session_port_offset(session));
				projectile_route_store(route, *origin_alias, *target,
				    *missiles);
				start = next;
				destination = *target;
				rerouted = true;
				break;
			}
			static const uint8_t union_police_row[] =
			    "The Union Police have destroyed the Missiles!";

			if (yt_projectile_union_police_admitted((float)next,
			    destination, *counterattack, *xannor_provoker)) {
				if (!session_present_text(session, union_police_row,
				    sizeof(union_police_row) - 1U, SESSION_PRESENT_LINE,
				    "Union Police missile row", error))
					return false;
				return true;
			}
			enum missile_sector_route sector_route;

			bool sector_success = missile_sector(session, next, missiles,
			    counterattack, xannor_provoker, &last_mine_news_sector,
			    &sector_route, error);

			route->amount = *missiles;
			if (!sector_success)
				return false;
			if (sector_route == MISSILE_SECTOR_RETURN)
				return true;
			if (yt_projectile_post_impact_route(*missiles)
			    == YT_PROJECTILE_POST_IMPACT_FOOTER)
				break;
			cursor = next;
		}
		if (!rerouted)
			break;
	}
	if (!missile_footer(session, error))
		return false;
	return true;
}

static bool
session_projectile_resolver(void *context, float *origin, float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct projectile_route_state *route = session_record(session) == -1
	    ? &session->projectile_xannor_route
	    : &session->projectile_main_route;
	bool result;

	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	result = launch_projectile(session, target, amount, plasma, route, origin,
	    NULL, NULL, NULL, counterattack, xannor_provoker, error);
	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	return result;
}

static bool
session_projectile_command_resolver(void *context, float *origin,
    uint8_t origin_raw[4], float *target, uint8_t target_raw[4],
    float *amount, uint8_t amount_raw[4], bool plasma, int *counterattack,
    int *xannor_provoker, struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	result = launch_projectile(session, target, amount, plasma,
	    &session->projectile_main_route, origin, origin_raw, target_raw,
	    amount_raw,
	    counterattack, xannor_provoker, error);

	*origin = session->projectile_main_route.origin;
	*target = session->projectile_main_route.destination;
	*amount = session->projectile_main_route.amount;
	(void)qb_mbf32_encode(*origin, origin_raw);
	(void)qb_mbf32_encode(*target, target_raw);
	(void)qb_mbf32_encode(*amount, amount_raw);
	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	return result;
}

static bool
session_random_integer(struct yt_session *session, int range, int *value,
    struct yt_error *error)
{
	return yt_random_integer(&session->door->game.random,
	    range, value, error);
}

static bool
session_nested_integer(struct yt_session *session, int count, int range,
    int *value, struct yt_error *error)
{
	return yt_random_nested_integer(&session->door->game.random,
	    count, range, value, error);
}

static bool
session_xannor_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector, sector,
	    error);
}

static bool
session_xannor_random(void *context, int count, int range, int *value,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (count == 1)
		return session_random_integer(session, range, value, error);
	return session_nested_integer(session, count, range, value, error);
}

static bool
session_xannor_present(void *context, const uint8_t *text, size_t length,
    bool bold, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_present_text(session, text, length,
	    bold ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    bold ? "Xannor retaliation row" : "Xannor retaliation blank",
	    error);
}

static bool
session_xannor_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
session_xannor_wait(void *context, float duration,
    struct yt_error *error)
{
	return session_wait(context, duration, "Xannor retaliation wait", error);
}

static bool
launch_xannor_retaliation(struct yt_session *session, int *provoking_player,
    struct yt_error *error)
{
	static const struct yt_xannor_retaliation_ops ops = {
		session_xannor_read_sector,
		session_xannor_random,
		session_xannor_present,
		session_projectile_resolver,
		session_xannor_read_player,
		session_xannor_wait,
	};
	bool result;

	session_load_xannor_provoker(session, provoking_player);
	session->player_record_carrier = session_record(session);
	struct yt_xannor_retaliation_state state = {
		&session->player,
		&session->player_record_carrier,
		&session->player_cache,
		&session->destroyed,
		&session->xannor_provoker,
		&session->door->game.config.headquarters,
		session_sector_count(session),
	};

	result = yt_xannor_retaliation_run(&state, &ops, session, error);
	*provoking_player = session->xannor_provoker;
	return result;
}

static bool
session_counterlaunch_projectile(void *context, float *origin, float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	result = launch_projectile(session, target, amount, plasma,
	    &session->projectile_counterlaunch_route, origin, NULL, NULL, NULL,
	    counterattack, xannor_provoker, error);
	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	return result;
}

static bool
launch_player_counterattack(struct yt_session *session, int *counterattacker,
    int *xannor_provoker, struct yt_error *error)
{
	struct yt_player saved_player;
	struct yt_player attacker;
	struct yt_player debit_player;
	struct yt_player final_player;
	int saved_record;
	float available;
	float target;
	float origin;
	uint8_t stored_name[YT_TEXT_FIELD_SIZE];
	uint8_t saved_name[YT_TEXT_FIELD_SIZE];
	size_t stored_name_length;
	size_t saved_name_length;
	uint8_t terminal_row[256];
	uint8_t news_row[256];
	size_t terminal_length;
	size_t news_length;
	char attacker_name[YT_TEXT_FIELD_SIZE + 1U];
	bool valid_cache;
	uint8_t saved_cloak_raw[4];

	session_load_counterattack_player(session, counterattacker);
	session->player_record_carrier = session_record(session);
	saved_record = session_record(session);
	if (*counterattacker < YT_PLAYER_FIRST
	    || *counterattacker > (int)session_sector_offset(session)
	    || *counterattacker == saved_record)
		return true;
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &attacker, error))
		return false;
	available = attacker.missiles;
	if (qb_mbf32_truth(attacker.record.bytes + YT_F45)
	    || available < 1.0f) {
		*counterattacker = 0;
		return true;
	}

	saved_player = session->player;
	target = saved_player.sector;
	saved_name_length = strlen(saved_player.name);
	if (saved_name_length > sizeof(saved_name))
		saved_name_length = sizeof(saved_name);
	memcpy(saved_name, saved_player.name, saved_name_length);
	valid_cache = yt_player_cache_contains(saved_record);
	if (valid_cache) {
		static const uint8_t zero[4] = {0};

		session_player_cache_raw(session, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
		session_set_player_cache_raw(session, saved_record,
		    YT_PLAYER_CACHE_CLOAK, zero);
	}
	session->player_record_carrier = *counterattacker;
	if (!yt_player_stored_name(&attacker, stored_name,
	    &stored_name_length, error))
		return false;
	memset(attacker_name, 0, sizeof(attacker_name));
	memcpy(attacker_name, stored_name, stored_name_length);
	memcpy(session->player.name, attacker_name, sizeof(session->player.name));

	session->counterlaunch_count = yt_counterlaunch_score_count(
	    (double)saved_player.score, session->counterlaunch_count);
	if (session->counterlaunch_count > available
	    || session->counterlaunch_count == 0.0f) {
		float draw;
		volatile float product;
		volatile float integral;
		volatile float selected;

		if (!random_value(session, &draw, error))
			return false;
		product = draw * available;
		integral = floorf(product);
		selected = integral + 1.0f;
		session->counterlaunch_count = selected;
	}
	if (!yt_game_read_player(&session->door->game, *counterattacker,
	    &debit_player, error))
		return false;
	yt_counterlaunch_debit_overlay(&debit_player, available,
	    session->counterlaunch_count);
	if (!yt_game_write_player(&session->door->game, *counterattacker,
	    &debit_player, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "player counterlaunch blank", error)
	    || !yt_counterlaunch_rows(stored_name, stored_name_length,
	    session->counterlaunch_count, saved_name, saved_name_length,
	    terminal_row, sizeof(terminal_row), &terminal_length, news_row,
	    sizeof(news_row), &news_length)
	    || !session_present_text(session, terminal_row, terminal_length,
	    SESSION_PRESENT_BOLD_LINE, "player counterlaunch row", error)
	    || !session_append_news_bytes(session, news_row, news_length, error))
		return false;
	origin = attacker.sector;
	if (!session_counterlaunch_projectile(session, &origin, &target,
	    &session->counterlaunch_count, false, counterattacker,
	    xannor_provoker, error))
		return false;

	*counterattacker = 0;
	session->player_record_carrier = saved_record;
	session->player = saved_player;
	if (valid_cache)
		session_set_player_cache_raw(session, saved_record,
		    YT_PLAYER_CACHE_CLOAK, saved_cloak_raw);
	if (!yt_game_read_player(&session->door->game, saved_record,
	    &final_player, error))
		return false;
	if (qb_mbf32_truth(final_player.record.bytes + YT_F45))
		session->destroyed = true;
	return session_wait(session, 4.0, "player counterattack wait", error);
}

static bool
projectile_command_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
	    || !session_reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
projectile_command_present(void *context, const uint8_t *text,
    size_t length, enum yt_projectile_command_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PROJECTILE_COMMAND_OPENING_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "projectile target opening blank", error);
	case YT_PROJECTILE_COMMAND_NO_TURNS_ROW:
		return session_present_alert(session, text, length, "no-turn gate notice",
		    error);
	case YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW:
		return session_present_alert(session, text, length,
		    "projectile ammunition refusal", error);
	case YT_PROJECTILE_COMMAND_TARGET_PROMPT:
		return session_present_timed_paged_row(session, text, length,
		    "projectile target prompt", error);
	case YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW:
		return session_present_alert(session, text, length,
		    "projectile invalid sector", error);
	case YT_PROJECTILE_COMMAND_QUANTITY_PROMPT:
		return session_present_timed_paged_row(session, text, length,
		    "projectile quantity prompt", error);
	case YT_PROJECTILE_COMMAND_TOO_MANY_ROW:
		return session_present_paged_fragment(session, text, length);
	case YT_PROJECTILE_COMMAND_ACCEPTED_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "projectile accepted blank", error);
	default:
		return false;
	}
}

static bool
projectile_command_input(void *context, char *response, size_t capacity,
    struct yt_error *error)
{
	(void)error;
	return session_read_number_command(context, response, capacity);
}

static bool
projectile_command_finalize(void *context, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (!finalize_action(session, 1.0f, error))
		return false;
	*player = session->player;
	return true;
}

static bool
projectile_command_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	/* The parent has already changed the live FIELD image before PUT. */
	session->player = *player;
	return yt_game_write_player(&session->door->game, player_record,
	    &session->player, error);
}

static bool
projectile_command_flush(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_flush(&session->door->game.database, error);
}

static bool
projectile_command_counterlaunch(void *context, int *counterattack,
    int *xannor_provoker, struct yt_error *error)
{
	return launch_player_counterattack(context, counterattack,
	    xannor_provoker, error);
}

static bool
projectile_command_xannor(void *context, int *xannor_provoker,
    struct yt_error *error)
{
	return launch_xannor_retaliation(context, xannor_provoker, error);
}

static bool
projectile_command_fatal(void *context, struct yt_error *error)
{
	return common_fatal_self(context, error);
}

static bool
projectile_command_destroyed_truth(void *context)
{
	return session_is_destroyed(context);
}

static bool
projectile_command_counterattack_truth(void *context)
{
	struct yt_session *session = context;

	return session->counterattack_player != 0;
}

static bool
projectile_command_xannor_truth(void *context)
{
	struct yt_session *session = context;

	return session->xannor_provoker != 0;
}

static void
projectile_command_store_turn_gate_result(void *context,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->shared_status = qb_mbf32_decode(raw);
}

static bool
command_projectile(struct yt_session *session, bool plasma,
    struct yt_error *error)
{
	static const struct yt_projectile_command_ops ops = {
		projectile_command_hydrate,
		projectile_command_present,
		projectile_command_input,
		projectile_command_finalize,
		projectile_command_write_player,
		projectile_command_flush,
		session_projectile_command_resolver,
		projectile_command_counterlaunch,
		projectile_command_xannor,
		projectile_command_fatal,
		projectile_command_destroyed_truth,
		projectile_command_counterattack_truth,
		projectile_command_xannor_truth,
		projectile_command_store_turn_gate_result,
	};
	struct yt_projectile_command_state state = {
		.current_player_record = session_record(session),
		.maximum_sector = (float)session_sector_count(session),
		.plasma = plasma,
		.displayed = plasma ? session->player.plasma
		    : session->player.missiles,
		.destroyed = &session->destroyed,
	};

	(void)qb_mbf32_encode(session->shared_status,
	    state.turn_gate_result_raw);

	return yt_projectile_command_run(&state, &ops, session, error);
}

static bool
radio_player_search(struct yt_session *session, const char *query,
    int *selected, struct yt_error *error)
{
	int basic;

	*selected = 0;
	if (query[0] == '\0')
		return true;
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session_sector_offset(session); ++basic) {
		struct yt_player player;
		enum yt_yes_no_answer answer;
		uint8_t prompt[YT_TEXT_FIELD_SIZE + sizeof(" [Y]? ") - 1U];
		size_t prompt_length;

		if (!yt_game_read_player(&session->door->game, basic, &player,
		    error))
			return false;
		if (player.record.bytes[YT_F85 + 3U] == 0
		    || !yt_fixed_text_contains(player.record.bytes,
		    (const uint8_t *)query, strlen(query)))
			continue;
		if (!yt_radio_player_prompt(&player, prompt, sizeof(prompt),
		    &prompt_length, error)
		    || !session_confirm(session, prompt, prompt_length, &answer,
		    error))
			return false;
		if (answer != YT_YES_NO_NO) {
			*selected = basic;
			return true;
		}
	}
	return session_present_paged_fragment(session, (const uint8_t *)"Not found.",
	    strlen("Not found."));
}

static bool
radio_line_prompt(struct yt_session *session, int line_number,
    const char *text, struct yt_error *error)
{
	char prompt[96];

	if (snprintf(prompt, sizeof(prompt), " %d:%s", line_number, text) < 0)
		return false;
	return session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "radio body line prompt", error);
}

static bool
radio_edit_draft(struct yt_session *session, char lines[21][76],
    int completed, struct yt_error *error)
{
	char response[80];
	struct qb_val_result parsed;
	char count_text[64];
	char prompt[128];
	int selected;

	if (qb_str_single(count_text, sizeof(count_text), (float)completed) < 0
	    || snprintf(prompt, sizeof(prompt),
	    "Edit Which line? (1 -%s) -=> ", count_text) < 0
	    || !session_present_timed_paged_row(session, (const uint8_t *)prompt, strlen(prompt),
	    "radio edit line prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "radio edit VAL");
		}
		return false;
	}
	if (!parsed.valid || !isfinite(parsed.value)
	    || floor(parsed.value) < (double)INT_MIN
	    || floor(parsed.value) > (double)INT_MAX)
		selected = 0;
	else
		selected = (int)floor(parsed.value);
	if (selected < 1 || selected > completed) {
		return session_present_alert(session,
		    (const uint8_t *)"INVALID LINE NUMBER!",
		    strlen("INVALID LINE NUMBER!"),
		    "radio edit invalid line", error);
	}

	for (;;) {
		char search[76];
		char replacement[76];
		char changed[152];
		char selected_text[64];
		char heading[128];
		char quoted[160];
		char *match;
		size_t prefix;

		if (qb_str_single(selected_text, sizeof(selected_text),
		    (float)selected) < 0
		    || snprintf(heading, sizeof(heading), "Line%s reads:",
		    selected_text) < 0
		    || snprintf(quoted, sizeof(quoted), "\"%s\"",
		    lines[selected - 1]) < 0
		    || !session_present_paged_line(session, (const uint8_t *)heading,
		    strlen(heading), "radio edit old heading", error)
		    || !session_present_paged_line(session, (const uint8_t *)quoted,
		    strlen(quoted), "radio edit old row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit search blank", error)
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)"Replace what section? -=> ",
		    strlen("Replace what section? -=> "),
		    "radio edit search prompt", error)
		    || !session_read_command(session, search, sizeof(search)))
			return false;
		if (search[0] == '\0')
			return true;
		match = strstr(lines[selected - 1], search);
		if (match == NULL) {
			char missing[256];

			if (snprintf(missing, sizeof(missing),
			    "\"%s\" NOT FOUND in line%s!", search,
			    selected_text) < 0
			    || !session_present_alert(session, (const uint8_t *)missing,
			    strlen(missing), "radio edit search miss", error))
				return false;
			continue;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit replacement blank", error)
		    || !session_present_timed_paged_row(session,
		    (const uint8_t *)"Replace it with what? -=> ",
		    strlen("Replace it with what? -=> "),
		    "radio edit replacement prompt", error)
		    || !session_read_command(session, replacement, sizeof(replacement)))
			return false;
		prefix = (size_t)(match - lines[selected - 1]);
		snprintf(changed, sizeof(changed), "%.*s%s%s", (int)prefix,
		    lines[selected - 1], replacement, match + strlen(search));
		changed[74] = '\0';

		for (;;) {
			enum yt_yes_no_answer answer;
			static const uint8_t confirmation[] =
			    "Is this OK? [Y/N]? -=> ";

			if (snprintf(heading, sizeof(heading), "Line%s now reads:",
			    selected_text) < 0
			    || snprintf(quoted, sizeof(quoted), "\"%s\"", changed) < 0
			    || !session_present_paged_line(session, (const uint8_t *)heading,
			    strlen(heading), "radio edit preview heading", error)
			    || !session_present_paged_line(session, (const uint8_t *)quoted,
			    strlen(quoted), "radio edit preview row", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio edit confirm blank", error)
			    || !session_confirm(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer == YT_YES_NO_YES) {
				snprintf(lines[selected - 1], 76, "%s", changed);
				return session_present_paged_line(session,
				    (const uint8_t *)"Change Saved!",
				    strlen("Change Saved!"),
				    "radio edit saved row", error);
			}
			if (answer == YT_YES_NO_NO)
				return session_present_alert(session,
				    (const uint8_t *)"CANCELED!",
				    strlen("CANCELED!"),
				    "radio edit canceled row", error);
		}
	}
}

static bool
radio_compose(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t warming[] = "Warming up sub-space radio.";
	static const uint8_t target_prompt[] =
	    "Send a message to who? (search string) or 'ALL' or 'TEAM'? ";
	static const uint8_t broadcast[] =
	    "This will be a broadcast message to ALL players";
	static const uint8_t limit[] =
	    "   Due to the distances involved, messages are limited to 20 lines.";
	char target[160];
	float recipients[4] = {0};
	int recipient_count = 0;
	char lines[21][76] = {{0}};
	int line_count = 0;
	size_t wrap_marker = 0;
	bool all = false;
	bool send = false;
	int index;

	if (!session_present_paged_line(session, warming, sizeof(warming) - 1U,
	    "radio warmup row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio target blank", error)
	    || !session_present_timed_paged_row(session, target_prompt,
	    sizeof(target_prompt) - 1U, "radio target prompt", error)
	    || !session_read_command(session, target, sizeof(target)))
		return false;
	if (target[0] == '\0')
		return true;
	qb_title_case(target);
	if (strcmp(target, "All") == 0) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio broadcast blank", error)
		    || !session_present_paged_fragment(session, broadcast,
		    sizeof(broadcast) - 1U))
			return false;
		recipients[0] = -2.0f;
		recipient_count = 1;
		all = true;
	}
	else if (strcmp(target, "Team") == 0) {
		static const uint8_t teamless[] =
		    "You Don't belong to a team!";

		if (!session_reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
			return session_present_alert(session, teamless,
			    sizeof(teamless) - 1U, "radio teamless row", error);
		}
		if (!session_load_team_cache(session, (int)session->player.team,
		    session_record(session), NULL, NULL, NULL, error))
			return false;
		for (index = 0; index < 4; ++index)
			recipients[index] =
			    (float)session->team_cache.roster[index];
		recipient_count = 4;
	}
	else {
		int selected;

		if (!radio_player_search(session, target, &selected, error))
			return false;
		if (selected == 0)
			return true;
		recipients[0] = (float)selected;
		recipient_count = 1;
	}
	if (!all && !session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "radio tuning blank", error))
		return false;
	for (index = 0; index < recipient_count; ++index) {
		struct yt_player target_player;
		uint8_t row[sizeof("Tuning in to ") - 1U + YT_TEXT_FIELD_SIZE
		    + sizeof("'s frequency.") - 1U];
		size_t row_length;

		if (all || recipients[index] == 0.0f)
			continue;
		if (!scanner_read_player(session, recipients[index],
		    &target_player, error)
		    || !yt_radio_tuning_row(&target_player, row, sizeof(row),
		    &row_length, error))
			return false;
		if (!session_present_paged_fragment(session, row, row_length))
			return false;
	}
	if (!session_present_paged_line(session, limit, sizeof(limit) - 1U,
	    "radio line-limit row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio body handoff blank", error))
		return false;

	while (!send) {
		bool menu = false;

		if (line_count >= 20) {
			if (!session_present_alert(session,
			    (const uint8_t *)"Message full!",
			    strlen("Message full!"), "radio message full", error))
				return false;
			menu = true;
		}
		else {
			session_set_pager_line_count(session, 0.0f);
			if (!radio_line_prompt(session, line_count + 1,
			    lines[line_count], error))
				return false;
			while (!menu) {
				int key = session_radio_body_key(session);
				enum yt_radio_body_key_action action;
				size_t length;

				if (key == EOF)
					return false;
				length = strlen(lines[line_count]);
				action = yt_input_radio_body_key((uint8_t)key, length);
				if (action == YT_RADIO_BODY_KEY_COMMIT) {
					if (!session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE, "radio body enter", error))
						return false;
					if (length == 0) {
						if (line_count == 0) {
							if (!session_present_text(session, NULL, 0,
							    SESSION_PRESENT_LINE,
							    "radio first-empty menu blank", error))
								return false;
							return true;
						}
						menu = true;
					}
					else {
						++line_count;
						wrap_marker = 0;
						if (line_count >= 20) {
							if (!session_present_alert(session,
							    (const uint8_t *)"Message full!",
							    strlen("Message full!"),
							    "radio entered message full", error))
								return false;
							menu = true;
						}
						else {
							session_set_pager_line_count(session, 0.0f);
							if (!radio_line_prompt(session,
							    line_count + 1, lines[line_count],
							    error))
								return false;
						}
					}
					continue;
				}
				if (action == YT_RADIO_BODY_KEY_BACKSPACE) {
					lines[line_count][length - 1U] = '\0';
					if (!session_radio_backspace(session,
					    line_count + 1, length - 1U, error))
						return false;
					continue;
				}
				if (action != YT_RADIO_BODY_KEY_PRINTABLE
				    || length >= 75U)
					continue;
				if (key == ' ')
					wrap_marker = length + 1U;
				lines[line_count][length] = (char)key;
				lines[line_count][length + 1U] = '\0';
				if (length + 1U <= 74U) {
					uint8_t byte = (uint8_t)key;

					if (!session_present_text(session, &byte, 1U,
					    SESSION_PRESENT_RAW, "radio body character",
					    error))
						return false;
				}
				if (length + 1U > 74U) {
					size_t split = wrap_marker != 0
					    ? wrap_marker : 74U;
					size_t carry = length + 1U - split;

					if (!session_radio_wrap_cleanup(session,
					    line_count + 1, split, error))
						return false;
					memcpy(lines[line_count + 1],
					    lines[line_count] + split, carry);
					lines[line_count + 1][carry] = '\0';
					lines[line_count][split] = '\0';
					++line_count;
					wrap_marker = 0;
					if (!session_present_text(session, NULL, 0,
					    SESSION_PRESENT_LINE, "radio body wrap blank",
					    error))
						return false;
					if (line_count >= 20) {
						if (!session_present_alert(session,
						    (const uint8_t *)"Message full!",
						    strlen("Message full!"),
						    "radio wrap message full", error))
							return false;
						menu = true;
					}
					else {
						session_set_pager_line_count(session, 0.0f);
						if (!radio_line_prompt(session,
						    line_count + 1, lines[line_count],
						    error))
							return false;
					}
				}
			}
		}

		while (menu && !send) {
			char choice[80];
			bool abort;
			static const uint8_t menu_prompt[] =
			    "[L] List [S] Send [A] Abort [C] Continue [E] Edit -=> ";

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio menu blank", error)
			    || !session_present_timed_paged_row(session, menu_prompt,
			    sizeof(menu_prompt) - 1U, "radio menu prompt", error)
			    || !session_read_upper_command(session, choice, sizeof(choice)))
				return false;
			if (choice[0] != '\0'
			    && !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio menu dispatch blank", error))
				return false;
			if (strcmp(choice, "L") == 0) {
				static const uint8_t list_dirty_zero[4] = {
					0x00U, 0x00U, 0x80U, 0x00U,
				};

				session_set_pager_line_count_raw(session,
				    list_dirty_zero);
				for (index = 0; index < line_count; ++index) {
					char row[96];

					if (snprintf(row, sizeof(row), " %d:%s", index + 1,
					    lines[index]) < 0
					    || !session_present_paged_fragment(session,
					    (const uint8_t *)row, strlen(row)))
						return false;
				}
			}
			else if (strcmp(choice, "A") == 0) {
				enum yt_yes_no_answer answer;
				static const uint8_t abort_prompt[] =
				    "Are you sure? [y/N]";

				if (!session_confirm(session, abort_prompt,
				    sizeof(abort_prompt) - 1U, &answer, error))
					return false;
				abort = answer == YT_YES_NO_YES;
				if (abort)
					return true;
			}
			else if (strcmp(choice, "C") == 0) {
				if (line_count > 0
				    && lines[line_count - 1][0] == '\0') {
					--line_count;
					if (line_count > 0)
						wrap_marker =
						    strlen(lines[line_count - 1]);
				}
				menu = false;
			}
			else if (strcmp(choice, "E") == 0) {
				if (!radio_edit_draft(session, lines, line_count, error))
					return false;
			}
			else if (strcmp(choice, "S") == 0)
				send = true;
		}
	}
	if (all) {
		static const uint8_t prefix[] = "  -  Message from: ";
		uint8_t news[sizeof(prefix) - 1U + YT_TEXT_FIELD_SIZE];
		size_t length = sizeof(prefix) - 1U
		    + session->cached_player_name_length;

		memcpy(news, prefix, sizeof(prefix) - 1U);
		memcpy(news + sizeof(prefix) - 1U,
		    session->cached_player_name,
		    session->cached_player_name_length);
		if (!session_append_news_bytes(session, news, length, error))
			return false;
	}
	for (index = 0; index < recipient_count; ++index) {
		int body;

		if (recipients[index] == 0.0f)
			continue;
		for (body = 0; body < line_count; ++body) {
			size_t length = strlen(lines[body]);

			if (all) {
				static const uint8_t prefix[] = "  -  ";
				uint8_t news[sizeof(prefix) - 1U + 75U];

				memcpy(news, prefix, sizeof(prefix) - 1U);
				memcpy(news + sizeof(prefix) - 1U, lines[body],
				    length);
				if (!session_append_news_bytes(session, news,
				    sizeof(prefix) - 1U + length, error))
					return false;
			}
			if (!radio_append_bytes((const uint8_t *)lines[body], length,
			    (float)session_record(session), recipients[index], error))
				return false;
		}
	}
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session,
	    (const uint8_t *)"Transmission successful!",
	    strlen("Transmission successful!"));
}

static bool
computer_route_cint(struct yt_session *session, float value, int *converted,
    enum yt_basic_fault_site site, const char *operation,
    struct yt_error *error)
{
	bool overflow;
	int32_t result = qb_cint_mode((double)value,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", operation);
		}
		(void)yt_error_attach_basic_fault_number(error, site, 6U);
		return false;
	}
	*converted = (int)result;
	return true;
}

static bool
computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error)
{
	static const uint8_t start_prompt[] = "Enter start for path search? ";
	static const uint8_t destination_prompt[] =
	    "What sector do you want to go to? ";
	static const uint8_t working[] = "Working. ";
	static const uint8_t same[] = "Hey, look out the window dummy!";
	static const uint8_t route_failure[] =
	    "*** You can't get there without going someplace you dont want to!";
	static const uint8_t insufficient[] =
	    "Not enough turns left to autopilot this course!";
	static const uint8_t confirmation[] =
	    "Enter course into autopilot? (Y/[N])";
	static const uint8_t engaged[] = "Autopilot Engaged.";
	static const uint8_t stop_notice[] = "Ctrl-X to Stop";
	enum yt_yes_no_answer answer;
	char response[160];
	float maximum;
	float start_value;
	float destination_value;
	float hop_count;
	uint8_t parsed_raw[4];
	uint8_t hop_count_raw[4];
	bool stale_marker = autopilot && session->path_marker == 9999.0f;
	int start;
	int destination;
	int count = session_sector_count(session);
	bool conversion_overflow;
	bool found;
	int cursor;
	enum yt_route_outcome route_outcome;

	if (!autopilot) {
		session->path_marker = 9999.0f;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_present_timed_paged_row(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response, &start_value, parsed_raw,
		    error))
			return false;
		session->route_start = start_value;
	}
	else if (!stale_marker)
		session->route_start = qb_mbf32_decode(
		    session->player.record.bytes + YT_F57);
	start_value = session->route_start;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_present_timed_paged_row(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response, &destination_value, parsed_raw,
	    error))
		return false;
	if (!yt_computer_path_maximum(session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	if (destination_value < 1.0f || destination_value > maximum
	    || start_value < 1.0f || start_value > maximum) {
		char number[64];
		char notice[128];

		if (qb_str_single(number, sizeof(number), maximum) < 0
		    || snprintf(notice, sizeof(notice),
		    "Valid sector numbers are from 1 to%s!", number) < 0)
			return false;
		return session_present_alert(session, (const uint8_t *)notice,
		    strlen(notice), "path invalid endpoint", error);
	}
	if (start_value == destination_value)
		return session_present_alert(session, same, sizeof(same) - 1U,
		    "path equal endpoint", error);
	start = (int)qb_cint_mode((double)start_value,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow)
		return false;
	destination = (int)qb_cint_mode((double)destination_value,
	    session->presentation.sound.conversion_mode, &conversion_overflow);
	if (conversion_overflow)
		return false;
	if (start < 0 || start > count || destination < 0 || destination > count)
		return true;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path working blank", error)
	    || !session_present_timed_paged_row(session, working, sizeof(working) - 1U,
	    "path working prompt", error))
		return false;
	session->shared_status = 1.0f;
	if (!yt_route_build(start_value, destination_value,
	    &session->shared_status, session->route_avoid,
	    session->presentation.sound.conversion_mode,
	    session->route_predecessor, session->route_second,
	    route_sector_reader, session, &route_outcome, error))
		return false;
	route_require_returned(route_outcome);
	found = route_outcome == YT_ROUTE_FOUND
	    || route_outcome == YT_ROUTE_SAME;
	if (!found) {
		yt_present_set_blink(&session->presentation, 1.0f);
		return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "path failure first blank", error)
		    && session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path failure second blank", error)
		    && session_present_text(session, route_failure,
		    sizeof(route_failure) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "path route failure", error);
	}
	{
		char start_text[64];
		char destination_text[64];
		char heading[192];

		if (qb_str_single(start_text, sizeof(start_text), start_value) < 0
		    || qb_str_single(destination_text, sizeof(destination_text),
		    destination_value) < 0
		    || snprintf(heading, sizeof(heading),
		    "The shortest path from sector%s to sector%s is:",
		    start_text, destination_text) < 0
		    || !session_present_paged_fragment(session, (const uint8_t *)heading,
		    strlen(heading))
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route blank", error))
			return false;
	}
	cursor = start;
	session->computer_route_scratch[0] = '1';
	session->computer_route_scratch[1] = '\0';
	session->computer_route_scratch_length = 1U;
	hop_count = 0.0f;
	(void)qb_mbf32_encode(hop_count, hop_count_raw);
	{
		char number[64];

		if (qb_str_single(number, sizeof(number), (float)start) < 0
		    || !session_present_timed_paged_row(session, (const uint8_t *)number,
		    strlen(number), "path start token", error))
			return false;
	}
	for (;;) {
		char number[64];
		char token[80];
		int column;
		int display_index;
		int ignored_row;
		int program_vertex;
		int16_t next;

		if (!computer_route_cint(session, (float)cursor, &display_index,
		    YT_BASIC_FAULT_ROUTE_DISPLAY_VERTEX_CINT,
		    "route display vertex CINT", error))
			return false;
		next = session->route_second[display_index];
		if (next == 0)
			break;
		cursor = next;
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0
		    || snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0
		    || !session_present_timed_paged_row(session, (const uint8_t *)token,
		    strlen(token), "path route token", error))
			return false;
		if (!computer_route_cint(session, (float)cursor, &program_vertex,
		    YT_BASIC_FAULT_ROUTE_PROGRAM_VERTEX_CINT,
		    "course-program vertex CINT", error))
			return false;
		if (!yt_computer_path_append_hop(
		    session->computer_route_scratch,
		    sizeof(session->computer_route_scratch),
		    &session->computer_route_scratch_length,
		    (float)program_vertex,
		    &hop_count, hop_count_raw, error))
			return false;
		yt_out_cursor_position(&ignored_row, &column);
		if (yt_computer_path_wrap_required(column)
		    && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route wrap", error))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path token terminator", error))
		return false;
	{
		char hop_text[64];
		char course[128];

		if (qb_str_single(hop_text, sizeof(hop_text), hop_count) < 0
		    || snprintf(course, sizeof(course),
		    "Course will take%s turns.", hop_text) < 0
		    || !session_present_paged_line(session, (const uint8_t *)course,
		    strlen(course), "path course row", error))
			return false;
	}
	session->path_marker = 0.0f;
	if (!autopilot || stale_marker)
		return true;
	if (!session_reload_player(session, error))
		return false;
	if (hop_count > session->player.turns) {
		if (!session_present_alert(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "autopilot insufficient turns", error))
			return false;
	}
	else {
		char turns[64];
		char row[128];

		if (qb_str_single(turns, sizeof(turns), session->player.turns) < 0
		    || snprintf(row, sizeof(row), "You have%s turns left.",
		    turns) < 0
		    || !session_present_paged_fragment(session, (const uint8_t *)row, strlen(row))
		    || !session_confirm(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer == YT_YES_NO_YES) {
			if (!session_present_paged_line(session, engaged, sizeof(engaged) - 1U,
			    "autopilot engaged row", error)
			    || !session_present_paged_line(session, stop_notice,
			    sizeof(stop_notice) - 1U,
			    "autopilot stop row", error))
				return false;
			if (!yt_input_queue_prepend_program(session->queue,
			    sizeof(session->queue), &session->queue_position,
			    &session->queue_length,
			    session->computer_route_scratch,
			    session->computer_route_scratch_length))
				return false;
		}
	}
	{
		struct yt_sector current_sector;
		size_t index;

		if (!read_sector_at_fault(session, (int)session->player.sector,
		    &current_sector, YT_BASIC_FAULT_ROUTE_FINAL_SECTOR_GET, error))
			return false;
		for (index = 0; index < 6U; ++index)
			session->current_warps[index] = qb_mbf32_decode(
			    current_sector.record.bytes + YT_F41 + index * 4U);
	}
	return true;
}

static bool
computer_planet_relation_cint(struct yt_session *session, float relationship,
    int *converted, const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t value = qb_cint_mode((double)relationship,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", operation);
		}
		return false;
	}
	*converted = (int)value;
	return true;
}

static bool
computer_planet_report(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "What sector number is the planet in? ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum = single_sub(session_port_offset(session),
	    session_sector_offset(session));

	for (;;) {
		struct qb_val_result parsed;
		struct yt_sector sector;
		struct yt_planet planet;
		char response[160];
		double sector_fighters;
		float fighter_owner;
		float last_relationship;
		float link;
		float scratch;
		float selected;
		int relation_cint;
		bool denied;
		bool fighter_friendly;
		bool last_friendly;
		bool valid_link;

		if (!fresh_no_turn_gate(session, &denied, error))
			return false;
		if (denied)
			return true;
		if (!session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "computer planet sector prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
		parsed = qb_val(response);
		if (parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "computer planet sector VAL");
			}
			return false;
		}
		selected = parsed.valid ? (float)qb_int(parsed.value) : 0.0f;
		if (selected < 1.0f)
			return true;
		if (selected > maximum) {
			char number[64];
			char notice[128];

			if (qb_str_single(number, sizeof(number), maximum) < 0
			    || snprintf(notice, sizeof(notice),
			    "Valid sector numbers are from 1 to%s.", number) < 0
			    || !session_present_alert(session, (const uint8_t *)notice,
			    strlen(notice), "computer planet invalid sector", error))
				return false;
			continue;
		}
		if (!session_read_sector(session, (int)selected, &sector, error))
			return false;
		link = qb_mbf32_decode(sector.record.bytes + YT_F93);
		{
			float maximum_planet = single_sub(
			    session->door->game.config.total_records,
			    session_planet_offset(session));

			valid_link = link > 0.0f && link <= maximum_planet;
		}
		if (valid_link) {
			bool limited_candidate;
			bool owner_differs;
			bool owner_nonzero;
			bool ground_nonzero;
			bool fighters_zero;
			bool fighters_positive;
			size_t name_length;

			session->hostile_deployed_fighters = (double)sector.fighters;
			session->shared_target_record = qb_mbf32_decode(
			    sector.record.bytes + YT_F85);
			fighter_owner = session->shared_target_record;
			if (!computer_port_friendship(session, fighter_owner,
			    &fighter_friendly, error))
				return false;
			sector_fighters = session->hostile_deployed_fighters;
			last_relationship = session->friendship_relation ? -1.0f : 0.0f;
			scratch = single_add(session_planet_offset(session), link);
			session->planet_record_expression = scratch;
			if (!session_read_planet(session, (int)link, &planet, error)
			    || !port_report_length(session, planet.name_length,
			    YT_TEXT_FIELD_SIZE, &name_length,
			    "computer planet name length", error))
				return false;
			if (!computer_planet_relation_cint(session, last_relationship,
			    &relation_cint,
			    "computer planet fighter relationship CINT", error))
				return false;
			owner_differs = (float)session_record(session) != planet.owner;
			owner_nonzero = planet.owner != 0.0f;
			ground_nonzero = planet.ground_forces != 0.0f;
			fighters_zero = sector_fighters == 0.0;
			fighters_positive = sector_fighters > 0.0;
			limited_candidate = owner_differs && owner_nonzero
			    && ground_nonzero && (fighters_zero
			    || (fighters_positive && relation_cint != 0));
			if (limited_candidate) {
				char forces[64];
				uint8_t row[192];
				size_t length = 0;

				if (!computer_port_friendship(session, planet.owner,
				    &last_friendly, error))
					return false;
				last_relationship = session->friendship_relation
				    ? -1.0f : 0.0f;
				if (!computer_planet_relation_cint(session,
				    last_relationship, &relation_cint,
				    "computer planet owner relationship CINT", error))
					return false;
				if (~relation_cint != 0) {
					static const uint8_t prefix[] = "Planet: ";
					static const uint8_t infix[] =
					    " -*- Ground Forces:";

					if (qb_str_single(forces, sizeof(forces),
					    planet.ground_forces) < 0)
						return port_report_failure(error,
						    "computer planet forces format");
					memcpy(row + length, prefix,
					    sizeof(prefix) - 1U);
					length += sizeof(prefix) - 1U;
					memcpy(row + length, planet.record.bytes,
					    name_length);
					length += name_length;
					memcpy(row + length, infix,
					    sizeof(infix) - 1U);
					length += sizeof(infix) - 1U;
					memcpy(row + length, forces,
					    strlen(forces));
					length += strlen(forces);
					return session_present_paged_line(session, row, length,
					    "computer planet limited row", error);
				}
			}
		}
		else {
			sector_fighters = session->hostile_deployed_fighters;
			fighter_owner = session->shared_target_record;
			last_relationship = session->friendship_relation ? -1.0f : 0.0f;
			scratch = link;
		}
		if (!computer_planet_relation_cint(session, last_relationship,
		    &relation_cint, "computer planet unavailable relationship CINT",
		    error))
			return false;
		{
			bool scratch_zero = scratch == 0.0f;
			bool fighters_positive = sector_fighters > 0.0;
			bool team_positive = session->player.team > 0.0f;
			bool team_zero = session->player.team == 0.0f;
			bool relation_not = ~relation_cint != 0;
			bool fighter_owner_differs =
			    (float)session_record(session) != fighter_owner;
			bool no_information = scratch_zero
			    || (fighters_positive && team_positive && relation_not)
			    || (fighters_positive && team_zero
			    && fighter_owner_differs);

			if (no_information) {
				if (!finalize_action(session, 1.0f, error))
					return false;
				return session_present_paged_line(session, unavailable,
				    sizeof(unavailable) - 1U,
				    "computer planet unavailable", error);
			}
		}
		if (!valid_link && session->planet_record_expression < 1.0f)
			return port_report_failure(error,
			    "computer planet stale current-planet record");
		return planet_inventory(session, (int)(valid_link
		    ? link : single_sub(session->planet_record_expression,
		    session_planet_offset(session))), error);
	}
}


static bool
computer_port_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;

	if (friendly == NULL)
		return false;
	*friendly = false;
	session->friendship_relation = false;
	if (owner < 2.0f
	    || owner > session_sector_offset(session)
	    || (float)session_record(session) < 2.0f
	    || (float)session_record(session)
	    > session_sector_offset(session))
		return true;
	if (owner == (float)session_record(session)) {
		*friendly = true;
		session->friendship_relation = true;
		return true;
	}
	if (!read_player_at_fault(session, session_record(session), &current,
	    YT_BASIC_FAULT_PORT_FRIENDSHIP_CURRENT_GET, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!read_player_at_fault(session, (int)owner, &other,
	    YT_BASIC_FAULT_PORT_FRIENDSHIP_CANDIDATE_GET, error))
		return false;
	*friendly = other.team == current.team;
	if (*friendly)
		session->friendship_relation = true;
	return true;
}

struct computer_port_visibility_context {
	struct yt_session *session;
	size_t player_reads;
};

static bool
computer_port_visibility_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct computer_port_visibility_context *visibility_context = context;
	struct yt_session *session = visibility_context->session;
	enum yt_basic_fault_site site = visibility_context->player_reads++ == 0U
	    ? YT_BASIC_FAULT_PORT_FRIENDSHIP_CURRENT_GET
	    : YT_BASIC_FAULT_PORT_FRIENDSHIP_CANDIDATE_GET;

	if (physical_record > (uint32_t)INT_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "computer port friendship player record");
		}
		return false;
	}
	return read_player_at_fault(session, (int)physical_record, player, site,
	    error);
}

static bool
computer_port_report(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t prompt[] = "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum;
	float cached_team = session->player.team;
	char response[80];
	float selected;
	int sector_number;
	struct yt_sector sector;
	struct yt_computer_port_visibility_state visibility;
	struct computer_port_visibility_context visibility_context = {
		session, 0U
	};
	bool denied;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!yt_computer_port_maximum(session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	for (;;) {
		enum yt_computer_port_selection_route route;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "computer port sector blank", error)
		    || !session_present_timed_paged_row(session, prompt, sizeof(prompt) - 1U,
		    "computer port sector prompt", error)
		    || !session_read_command(session, response, sizeof(response)))
			return false;
		if (!yt_computer_port_select(response, maximum, &selected,
		    &route, error))
			return false;
		if (route == YT_COMPUTER_PORT_SELECTION_EMPTY)
			return true;
		if (route == YT_COMPUTER_PORT_SELECTION_ACCEPTED)
			break;
		{
			char number[64];
			char notice[128];

			if (qb_str_single(number, sizeof(number), maximum) < 0
			    || snprintf(notice, sizeof(notice),
			    "Invalid sector number! Range is 1 -%s", number) < 0
			    || !session_present_alert(session, (const uint8_t *)notice,
			    strlen(notice), "computer port invalid sector", error))
				return false;
		}
	}
	sector_number = (int)selected;
	if (!read_sector_at_fault(session, sector_number, &sector,
	    YT_BASIC_FAULT_PORT_SELECTED_SECTOR_GET, error))
		return false;
	{
		float sector_expression = yt_port_selected_expression(
		    session_sector_offset(session), selected);
		bool visibility_ok;

		memset(&visibility, 0, sizeof(visibility));
		visibility.port_link = sector.port;
		visibility.fighter_count = sector.fighters;
		visibility.fighter_owner = sector.fighter_owner;
		visibility.cached_current_team = cached_team;
		visibility.current_player_record = (float)session_record(session);
		visibility.last_player_record =
		    session_sector_offset(session);
		visibility.planet_record_offset =
		    session_planet_offset(session);
		visibility.inherited_index = session->inherited_loop_index;
		visibility.field_kind = YT_COMPUTER_PORT_FIELD_SECTOR;
		visibility.field_record =
		    qb_brun_random_record_number(sector_expression);
		visibility.field = sector.record;
		visibility.field_valid = true;
		visibility_ok = yt_computer_port_visibility_run(&visibility,
		    computer_port_visibility_read_player, &visibility_context,
		    error);
		session->path_marker = qb_mbf32_decode(
		    visibility.marker_4d62_raw);
		session->shared_status = qb_mbf32_decode(visibility.relation_raw);
		if (visibility.scratch_written)
			session->planet_record_expression = qb_mbf32_decode(
			    visibility.scratch_19c4_raw);
		if (!visibility_ok)
			return false;
		denied = visibility.unavailable;
	}
	if (denied)
		return session_present_paged_line(session, unavailable,
		    sizeof(unavailable) - 1U,
		    "computer port unavailable", error);
	if (sector.port == 1.0f) {
		struct yt_port earth;
		float price[4];

		if (!earth_report(session, &earth, price, error))
			return false;
		session->earth_report_seen = false;
		return true;
	}
	{
		float sector_record_expression = yt_port_selected_expression(
		    session_sector_offset(session),
		    (float)sector_number);

		return computer_port_ordinary(session, sector_number,
		    sector_record_expression, &visibility, error);
	}
}

static bool
computer_avoid_cell(char *cell, size_t capacity, int slot, float value)
{
	char slot_text[64];
	char value_text[64];

	return qb_str_single(slot_text, sizeof(slot_text), (float)slot) >= 0
	    && qb_str_single(value_text, sizeof(value_text), value) >= 0
	    && snprintf(cell, capacity, "%s%s ]  -=> %s",
	    slot < 10 ? "[ " : "[", slot_text, value_text) >= 0;
}

static bool
computer_avoid(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading_one[] =
	    "You may set the autopilot to avoid up to 30 sectors";
	static const uint8_t heading_two[] = "Current sectors to avoid are:";
	static const uint8_t slot_prompt[] =
	    "Enter the number of the slot to change [1 - 30]: ";
	char response[80];
	float slot_value;
	float maximum;
	float new_value;
	float old_value;
	bool available;
	bool locked;
	enum yt_computer_avoid_selection_route route;
	int slot;
	int row;

	if (!session_present_paged_line(session, heading_one, sizeof(heading_one) - 1U,
	    "avoid first heading", error)
	    || !session_present_paged_line(session, heading_two, sizeof(heading_two) - 1U,
	    "avoid second heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid heading blank", error))
		return false;
	for (row = 0; row < 10; ++row) {
		char first[96];
		char middle[96];
		char last[96];

		if (!computer_avoid_cell(first, sizeof(first), row + 1,
		    session->route_avoid[row])
		    || !computer_avoid_cell(last, sizeof(last), row + 21,
		    session->route_avoid[row + 20])
		    || !session_fixed_width(session, first, 20.0f,
		    "avoid first cell", error)
		    || !computer_avoid_cell(middle, sizeof(middle), row + 11,
		    session->route_avoid[row + 10])
		    || !session_fixed_width(session, middle, 20.0f,
		    "avoid middle cell", error)
		    || !session_present_paged_fragment(session, (const uint8_t *)last, strlen(last)))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid slot-prompt blank", error)
	    || !session_present_timed_paged_row(session, slot_prompt, sizeof(slot_prompt) - 1U,
	    "avoid slot prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (!yt_computer_avoid_select_slot(response,
	    session->presentation.sound.conversion_mode, &slot_value, &slot,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	if (!yt_computer_avoid_maximum(
	    session_port_offset(session),
	    session_sector_offset(session), &maximum, error))
		return false;
	{
		char maximum_text[64];
		char prompt[160];

		if (qb_str_single(maximum_text, sizeof(maximum_text), maximum) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "Enter the sector you wish to avoid [1 -%s] (0 to clear): ",
		    maximum_text) < 0
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "avoid sector-prompt blank", error)
		    || !session_present_timed_paged_row(session, (const uint8_t *)prompt,
		    strlen(prompt), "avoid sector prompt", error)
		    || !session_read_number_command(session, response, sizeof(response)))
			return false;
	}
	if (!yt_computer_avoid_select_sector(response, maximum, &new_value,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	old_value = session->route_avoid[slot - 1];
	session->route_avoid[slot - 1] = new_value;
	yt_computer_avoid_transition(old_value, new_value, &locked, &available);
	session_set_foreground(session, 2.0f);
	if (locked) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), new_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now locked out.", number) < 0
		    || !session_present_paged_line(session, (const uint8_t *)status,
		    strlen(status), "avoid locked status", error))
			return false;
	}
	if (available) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), old_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now available.", number) < 0
		    || !session_present_paged_line(session, (const uint8_t *)status,
		    strlen(status), "avoid available status", error))
			return false;
	}
	session_set_foreground(session, 1.0f);
	return true;
}

static bool
computer_spies(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t none[] = "You do not have any spies!";
	size_t index;

	if (session->spy_count == 0)
		return session_present_alert(session, none, sizeof(none) - 1U,
		    "active-spy none notice", error);
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "active-spy leading blank", error))
		return false;
	for (index = 0U; index < (size_t)session->spy_count; ++index) {
		char counter[64];
		char target[64];
		uint8_t row[160];
		int counter_length;
		int target_length;
		int row_length;

		counter_length = qb_str_single(counter, sizeof(counter),
		    (float)(index + 1U));
		target_length = qb_str_integer(target, sizeof(target),
		    (int16_t)session->spy_sectors[index]);
		if (counter_length < 0 || target_length < 0)
			return false;
		row_length = snprintf((char *)row, sizeof(row),
		    "Spy #%.*s will hunt in sector%.*s.", counter_length,
		    counter, target_length, target);
		if (row_length < 0 || (size_t)row_length >= sizeof(row))
			return false;
		yt_present_set_bold(&session->presentation, 1.0f);
		if (!session_present_paged_fragment(session, row,
		    (size_t)row_length))
			return false;
	}
	return true;
}

static bool
nearest_session_read(void *context, enum yt_nearest_field_kind kind,
    float expression, uint32_t physical_record, struct yt_record *record,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)kind;
	(void)expression;
	return yt_database_read(&session->door->game.database,
	    (size_t)physical_record, record, error);
}

static bool
nearest_session_day(void *context, float *day, struct yt_error *error)
{
	struct yt_session *session = context;
	int today;
	int adjusted_year;

	if (!session_current_date_serial(session, &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	*day = (float)today;
	return true;
}

static bool
nearest_session_timer(void *context, float *seconds, struct yt_error *error)
{
	(void)context;
	(void)error;
	*seconds = (float)yt_platform_timer();
	return true;
}

static const char *
nearest_session_output_operation(enum yt_nearest_output_kind kind)
{
	static const char *const operations[] = {
		[YT_NEAREST_ENTRY_BLANK] = "nearest-port opening blank",
		[YT_NEAREST_SCANNING] = "nearest-port scanning row",
		[YT_NEAREST_SCAN_BLANK] = "nearest-port scanning blank",
		[YT_NEAREST_OWNER_INSTRUCTION] = "nearest-port ownership row",
		[YT_NEAREST_OWNER_BLANK] = "nearest-port ownership blank",
		[YT_NEAREST_DISTANCE] = "nearest-port distance heading",
		[YT_NEAREST_SECTOR] = "nearest-port sector prefix",
		[YT_NEAREST_ORE] = "nearest-port ore cell",
		[YT_NEAREST_ORGANICS] = "nearest-port organics cell",
		[YT_NEAREST_EQUIPMENT] = "nearest-port equipment cell",
		[YT_NEAREST_STOCK] = "nearest-port aggregate cell",
		[YT_NEAREST_NAME] = "nearest-port name row",
		[YT_NEAREST_PAGER_PROMPT] = "nearest-port pager prompt",
		[YT_NEAREST_PAGER_ECHO] = "nearest-port pager echo",
		[YT_NEAREST_FINAL_BLANK] = "nearest-port final blank",
	};

	if ((size_t)kind >= YT_ARRAY_LEN(operations)
	    || operations[kind] == NULL)
		return "nearest-port presentation";
	return operations[kind];
}

static bool
nearest_session_present(void *context, enum yt_nearest_output_kind kind,
    enum yt_nearest_present_mode mode, const uint8_t *text, size_t length,
    struct yt_nearest_style *style, struct yt_error *error)
{
	struct yt_session *session = context;
	enum session_present_text_kind present_kind;
	bool ok;

	switch (mode) {
	case YT_NEAREST_PRESENT_LINE:
		present_kind = SESSION_PRESENT_LINE;
		break;
	case YT_NEAREST_PRESENT_RAW:
		present_kind = SESSION_PRESENT_RAW;
		break;
	case YT_NEAREST_PRESENT_BOLD_LINE:
		present_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	case YT_NEAREST_PRESENT_BOLD_RAW:
		present_kind = SESSION_PRESENT_BOLD_RAW;
		break;
	default:
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	session_set_foreground(session, style->foreground);
	yt_present_set_bold(&session->presentation, style->bold);
	yt_present_set_blink(&session->presentation, style->blink);
	ok = session_present_text(session, text, length, present_kind,
	    nearest_session_output_operation(kind), error);
	style->foreground = session_foreground(session);
	style->bold = yt_present_bold(&session->presentation);
	style->blink = yt_present_blink(&session->presentation);
	return ok;
}

static bool
nearest_session_input(void *context, uint8_t *key, bool *available,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_input_value selected;

	(void)error;
	*available = false;
	*key = 0U;
	if (!yt_input_wait(&session->input, &selected))
		return false;
	if (selected.length != 1U)
		return true;
	*key = selected.bytes[0];
	*available = true;
	return true;
}

static void
nearest_session_uppercase(void *context, uint8_t *text, size_t length)
{
	session_compat_upper_n(context, text, length);
}

static bool
nearest_session_body(struct yt_session *session, int selector,
    uint8_t direction, struct yt_error *error)
{
	static const struct yt_nearest_ops ops = {
		nearest_session_read,
		nearest_session_day,
		nearest_session_timer,
		nearest_session_present,
		nearest_session_input,
		nearest_session_uppercase,
	};
	struct yt_nearest_state state;
	bool ok;
	size_t index;

	memset(&state, 0, sizeof(state));
	state.selector = selector;
	state.direction = direction;
	state.conversion_mode =
	    session->presentation.sound.conversion_mode;
	state.actor_number = (float)session_record(session);
	state.sector_record_offset = session_sector_offset(session);
	state.port_record_offset = session_port_offset(session);
	memcpy(state.base_price, session->market_bases, sizeof(state.base_price));
	for (index = 0U; index < YT_ARRAY_LEN(state.cached_roster); ++index)
		state.cached_roster[index] =
		    session_team_roster_value(session, index);
	state.style.foreground = session_foreground(session);
	state.style.bold = yt_present_bold(&session->presentation);
	state.style.blink = yt_present_blink(&session->presentation);
	state.field = session->player.record;
	state.field_kind = YT_NEAREST_FIELD_PLAYER;
	state.field_record =
	    qb_brun_random_record_number(state.actor_number);
	state.field_valid = true;

	ok = yt_nearest_run(&state, &ops, session, error);
	if (state.reads != 0U)
		session->player = state.player;
	return ok;
}

static bool
computer_nearest_ports(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t first_line[] =
	    "Show buying/selling [1] Equ, [2] Org, [3] Ore,";
	static const uint8_t second_line[] =
	    "[Y] Your Ports, [T] Team's Ports, [E] Enemy Ports";
	static const uint8_t filter_prompt[] =
	    "[U] Un-owned Ports OR [A] All Ports ? -=> [A] ";
	static const uint8_t no_team[] = "You dont belong to a team!";
	static const uint8_t no_ports[] = "You dont own any!";
	uint8_t direction_prompt[80];
	char response[80];
	size_t direction_prompt_length;
	size_t response_length;
	uint8_t direction = 0U;
	int selector;

	if (!session_reload_player(session, error)
	    || !session_present_paged_line(session, first_line,
	    sizeof(first_line) - 1U, "nearest-port first filter row", error)
	    || !session_present_paged_fragment(session, second_line,
	    sizeof(second_line) - 1U)
	    || !session_present_timed_paged_row(session, filter_prompt,
	    sizeof(filter_prompt) - 1U, "nearest-port filter prompt", error)
	    || !session_read_upper_command(session, response, sizeof(response)))
		return false;
	response_length = strlen(response);
	selector = yt_nearest_filter_selector((const uint8_t *)response,
	    response_length);
	if (selector == 0)
		return true;
	if (selector == 5 && session->player.team == 0.0f)
		return session_present_alert(session, no_team,
		    sizeof(no_team) - 1U, "nearest-port team rejection", error);
	if (selector == 6 && session->player.ports_owned == 0.0f)
		return session_present_alert(session, no_ports,
		    sizeof(no_ports) - 1U, "nearest-port ownership rejection", error);
	if (selector >= 1 && selector <= 3) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "nearest-port direction blank", error))
			return false;
		if (!yt_nearest_direction_prompt(selector, direction_prompt,
		    sizeof(direction_prompt), &direction_prompt_length)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "nearest direction prompt");
			}
			return false;
		}
		if (!session_present_timed_paged_row(session, direction_prompt,
		    direction_prompt_length, "nearest-port direction prompt", error)
		    || !session_read_upper_command(session, response,
		    sizeof(response)))
			return false;
		response_length = strlen(response);
		if (response_length != 1U
		    || (response[0] != 'B' && response[0] != 'S'))
			return true;
		direction = (uint8_t)response[0];
	}
	return nearest_session_body(session, selector, direction, error);
}

struct profit_session_context {
	struct yt_session *session;
};

static struct yt_session *
profit_context_session(void *context)
{
	struct profit_session_context *profit = context;

	return profit->session;
}

static bool
profit_session_read(void *context, enum yt_profit_field_kind kind,
    float expression, uint32_t physical_record, struct yt_record *record,
    struct yt_error *error)
{
	struct yt_session *session = profit_context_session(context);

	(void)kind;
	(void)expression;
	return yt_database_read(&session->door->game.database,
	    (size_t)physical_record, record, error);
}

static bool
profit_session_day(void *context, float *day, struct yt_error *error)
{
	struct yt_session *session = profit_context_session(context);
	int today;
	int adjusted_year;

	if (!session_current_date_serial(session, &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	*day = (float)today;
	return true;
}

static bool
profit_session_timer(void *context, float *seconds, struct yt_error *error)
{
	(void)context;
	(void)error;
	*seconds = (float)yt_platform_timer();
	return true;
}

static const char *
profit_session_operation(enum yt_profit_output_kind kind)
{
	static const char *const operations[] = {
		[YT_PROFIT_LEADING_BLANK] = "profit leading blank",
		[YT_PROFIT_TITLE] = "adjacent profit title",
		[YT_PROFIT_TITLE_BLANK] = "adjacent profit title blank",
		[YT_PROFIT_NO_CURRENT_PORT] = "adjacent profit no-port row",
		[YT_PROFIT_ROW] = "profit row",
		[YT_PROFIT_COLUMN_SEPARATOR] = "global profit separator",
		[YT_PROFIT_ROW_END] = "global profit row ending",
		[YT_PROFIT_NO_RESULTS] = "adjacent profit empty row",
		[YT_PROFIT_PAGER_PROMPT] = "profit pager prompt",
		[YT_PROFIT_PAGER_ECHO] = "profit pager echo",
		[YT_PROFIT_END_BANNER] = "global profit end row",
	};

	if ((size_t)kind >= YT_ARRAY_LEN(operations)
	    || operations[kind] == NULL)
		return "profit presentation";
	return operations[kind];
}

static bool
profit_session_present(void *context, enum yt_profit_output_kind kind,
    enum yt_profit_present_mode mode, const uint8_t *text, size_t length,
    struct yt_nearest_style *style, struct yt_error *error)
{
	struct yt_session *session = profit_context_session(context);
	enum session_present_text_kind present_kind;
	bool ok;

	switch (mode) {
	case YT_PROFIT_PRESENT_LINE:
		present_kind = SESSION_PRESENT_LINE;
		break;
	case YT_PROFIT_PRESENT_RAW:
		present_kind = SESSION_PRESENT_RAW;
		break;
	case YT_PROFIT_PRESENT_BOLD_LINE:
		present_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	case YT_PROFIT_PRESENT_BOLD_RAW:
		present_kind = SESSION_PRESENT_BOLD_RAW;
		break;
	default:
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	session_set_foreground(session, style->foreground);
	yt_present_set_bold(&session->presentation, style->bold);
	yt_present_set_blink(&session->presentation, style->blink);
	ok = session_present_text(session, text, length, present_kind,
	    profit_session_operation(kind), error);
	style->foreground = session_foreground(session);
	style->bold = yt_present_bold(&session->presentation);
	style->blink = yt_present_blink(&session->presentation);
	return ok;
}

static bool
profit_session_input(void *context, uint8_t *text, size_t capacity,
    size_t *length, bool *available, struct yt_error *error)
{
	struct yt_session *session = profit_context_session(context);
	struct yt_input_value selected;

	(void)error;
	*length = 0U;
	*available = false;
	if (!yt_input_wait(&session->input, &selected))
		return false;
	if (selected.length == 0U)
		return true;
	if (selected.length > capacity)
		return false;
	memcpy(text, selected.bytes, selected.length);
	*length = selected.length;
	*available = true;
	return true;
}

static void
profit_session_uppercase(void *context, uint8_t *text, size_t length)
{
	session_compat_upper_n(profit_context_session(context), text, length);
}

static bool
profit_session_checkpoint(void *context,
    enum yt_profit_checkpoint checkpoint, struct yt_error *error)
{
	(void)context;
	(void)checkpoint;
	(void)error;
	return true;
}

static bool
computer_profit_exact(struct yt_session *session, bool all,
    struct yt_error *error)
{
	static const struct yt_profit_ops ops = {
		profit_session_read,
		profit_session_day,
		profit_session_timer,
		profit_session_present,
		profit_session_input,
		profit_session_uppercase,
		profit_session_checkpoint,
	};
	struct yt_profit_state state;
	struct profit_session_context context = {session};
	bool ok;

	memset(&state, 0, sizeof(state));
	state.global = all;
	state.conversion_mode = session->presentation.sound.conversion_mode;
	state.current_sector_record = session->current_sector_record;
	state.sector_record_offset = session_sector_offset(session);
	state.port_record_offset = session_port_offset(session);
	memcpy(state.base_price, session->market_bases, sizeof(state.base_price));
	state.current_day = (float)session->door->game.today;
	state.style.foreground = session_foreground(session);
	state.style.bold = yt_present_bold(&session->presentation);
	state.style.blink = yt_present_blink(&session->presentation);
	/* The immediately preceding A41C prompt hydration owns the live FIELD. */
	state.field = session->player.record;
	state.field_record = (uint32_t)session_record(session);
	state.field_kind = YT_PROFIT_FIELD_PLAYER;
	state.field_valid = true;

	ok = yt_profit_run(&state, &ops, &context, error);
	session_set_foreground(session, state.style.foreground);
	return ok;
}

static bool
computer_activate(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t notice[] = "<Computer activated>";

	session_set_foreground(session, 1.0f);
	return session_present_paged_line(session, notice, sizeof(notice) - 1U,
	    "computer activation notice", error)
	    && session_sound(session, 4.0f, "computer activation sound", error);
}

static bool
computer_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = " Computer commands:";
	static const char *const left[8] = {
		" 1) Exit Computer", " 3) Autopilot",
		" 5) Send Radio Message",
		" 7) Set autopilot Sectors to Avoid",
		" 9) Planet Report", "11) Fighter Finder (Yours)",
		"13) Planet Finder (Yours)", "15) Show Active Spies"
	};
	static const uint8_t *const right[8] = {
		(const uint8_t *)" 2) Port Report",
		(const uint8_t *)" 4) Rank Teams & Players",
		(const uint8_t *)" 6) Radio Message Log",
		(const uint8_t *)" 8) Galactic Newspaper",
		(const uint8_t *)"10) Path Finder",
		(const uint8_t *)"12) Port(s) Treasury Report",
		(const uint8_t *)"14) Find Nearest Ports",
		(const uint8_t *)"16) Find Port Pairs"
	};
	static const uint8_t final[] =
	    "17) Check Profits of Adjacent Ports";
	size_t index;

	session_set_pager_line_count(session, 0.0f);
	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "computer help heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "computer help blank", error))
		return false;
	for (index = 0; index < 8U; ++index) {
		if (!session_fixed_width(session, left[index], 40.0f,
		    "computer help left cell", error)
		    || !session_present_paged_fragment(session, right[index], strlen(
		    (const char *)right[index])))
			return false;
	}
	return session_present_paged_fragment(session, final, sizeof(final) - 1U);
}

static bool
computer_menu_prompt(struct yt_session *session, char *command,
    size_t capacity, struct yt_error *error)
{
	uint8_t prompt[512];
	size_t prompt_length;
	size_t response_length;

	if (command == NULL || capacity < 3U
	    || !computer_prompt_hydrate(session, error))
		return false;
	session->shared_status = 0.0f;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "computer prompt leading blank", error))
		return false;
	session_set_foreground(session, 1.0f);
	if (!yt_computer_prompt_row((const uint8_t *)session->time.text,
	    session->time.text_length, prompt, sizeof(prompt), &prompt_length)
	    || !session_present_timed_paged_row(session, prompt, prompt_length,
	    "computer prompt", error)
	    || !session_read_upper_command(session, command, capacity))
		return false;
	response_length = strlen(command);
	if (response_length == 0U) {
		command[0] = '?';
		command[1] = '\0';
	}
	else if (response_length > 2U)
		command[2] = '\0';
	return true;
}

static bool
computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	if (enter_sector != NULL)
		*enter_sector = false;
	if (!computer_activate(session, error))
		return false;
	for (;;) {
		char command[80];
		int position;

		if (!computer_menu_prompt(session, command, sizeof(command), error))
			return false;

		if (strcmp(command, "I") == 0) {
			if (!show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "17") == 0) {
			if (!computer_profit_exact(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "!") == 0) {
			if (!command_collect(session,
			    YT_TREASURY_CALLER_COMPUTER_COLLECT, error))
				return false;
			continue;
		}
		if (strcmp(command, "S") == 0) {
			if (!display_sector(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "Q") == 0) {
			bool confirmed;

			if (!session_quit_confirm(session, &confirmed, error))
				return false;
			if (!confirmed)
				continue;
			if (!quit_session(session, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		if (strcmp(command, "10") == 0) {
			if (!computer_route(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "11") == 0) {
			if (!yt_session_computer_owned_fighters(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "12") == 0) {
			if (!command_collect(session,
			    YT_TREASURY_CALLER_COMPUTER_REPORT, error))
				return false;
			continue;
		}
		if (strcmp(command, "13") == 0) {
			if (!yt_session_computer_owned_planets(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "15") == 0) {
			if (!computer_spies(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "16") == 0) {
			if (!computer_profit_exact(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "14") == 0) {
			if (!computer_nearest_ports(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "7") == 0) {
			if (!computer_avoid(session, error))
				return false;
			continue;
		}

		position = yt_computer_selector_position(command);
		if (position != 0) {
			switch (position - 1) {
			case 0:
				if (!command_projectile(session, true, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 1:
				if (!command_projectile(session, false, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			case 2:
				return command_land(session, enter_sector, error);
			case 3: {
				bool moved;

				if (!command_move(session, &moved, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = moved;
				return true;
			}
			case 4:
				if (!command_trade(session, enter_sector, error))
					return false;
				return true;
			case 5:
				if (!computer_help(session, error))
					return false;
				continue;
			case 6:
			{
				static const uint8_t off[] =
				    "<Computer deactivated>";

				if (!session_present_paged_line(session, off, sizeof(off) - 1U,
				    "computer deactivation notice", error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
				return true;
			}
			case 7:
			{
				bool selected = false;

				if (!computer_port_report(session, &selected, error))
					return false;
				if (selected) {
					if (enter_sector != NULL)
						*enter_sector = true;
					return true;
				}
				continue;
			}
			case 8:
				if (!computer_route(session, true, error))
					return false;
				continue;
			case 9:
				if (!yt_session_computer_scoreboard(session, error))
					return false;
				continue;
			case 10:
				if (!radio_compose(session, error))
					return false;
				continue;
			case 11:
				if (!computer_planet_report(session, error))
					return false;
				continue;
			default:
				break;
			}
		}
		if (strcmp(command, "6") == 0) {
			session->shared_status = 1.0f;
			if (!radio_read(session, true, error))
				return false;
			continue;
		}
		if (strcmp(command, "8") == 0) {
			if (!yt_session_computer_newspaper(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "C") == 0) {
			static const uint8_t warning[] =
			    "Don't BREAK the 'ON' button!";

			if (!session_present_paged_line(session, warning,
			    sizeof(warning) - 1U,
			    "computer reactivation warning", error)
			    || !computer_activate(session, error))
				return false;
			continue;
		}
		{
			static const uint8_t invalid[] = "Does not compute";

			if (!session_present_alert(session, invalid, sizeof(invalid) - 1U,
			    "computer invalid command", error))
				return false;
		}
	}
}

static bool
show_help(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = "<Help>";
	static const char *const pairs[][2] = {
		{"[ENTER] - Re-display sector",
		    "$ - Take Credits from your ports"},
		{"! - Launch a Cruise Missile",
		    "A - <A>ttack a player's ship"},
		{"B - <B>uy a Port", "C - Ship's <C>omputer"},
		{"D - <D>rop a Sector mine",
		    "F - Take or leave <F>ighters"},
		{"G - Initiate <G>enesis", "I - <I>nfo on your ship"},
		{"L - <L>and on or create a planet",
		    "M - <M>ove to another sector"},
		{"N - Re<N>ame Port",
		    "P - Dock at a <P>ort (and trade)"},
		{"Q - <Q>uit game", "S - <S>ensors"},
		{"T - <T>eam menu", "V - <V>ersion Info"},
		{"W - Emergency <W>arp", "X - Sound Effects On/Off"},
		{"Z - Instructions", "+ - Fire Plasma Bolt"},
	};
	static const char *const narrative[] = {
		"String commands by seperating them with a semicolons (;).",
		"To place an EXTRA 'hit enter' in a string, use an extra ';'.",
		"Save a command string by placing a '/' at the end.",
		"Then hit Control-R to [R]eplay the saved command.",
		"You may repeat any command up to 20 times by putting",
		"a /R# at the end of your command. Replace the '#' with",
		"any number between 2 and 20. Example: your command/R20"
	};
	char row[128];
	size_t index;

	session_set_foreground(session, 6.0f);
	if (!session_present_paged_line(session, heading, sizeof(heading) - 1U,
	    "main help heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "main help table blank", error))
		return false;
	for (index = 0; index < sizeof(pairs) / sizeof(pairs[0]); ++index) {
		int length = snprintf(row, sizeof(row), "%-40s%s",
		    pairs[index][0], pairs[index][1]);

		if (length < 0 || (size_t)length >= sizeof(row)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "main help row capacity");
			}
			return false;
		}
		if (!session_present_text(session, (const uint8_t *)row,
		    (size_t)length, SESSION_PRESENT_LINE,
		    "main help table row", error))
			return false;
	}
	if (!session_present_paged_line(session, (const uint8_t *)narrative[0],
	    strlen(narrative[0]), "main help narrative first", error))
		return false;
	for (index = 1; index < sizeof(narrative) / sizeof(narrative[0]);
	    ++index) {
		if (!session_present_paged_fragment(session, (const uint8_t *)narrative[index],
		    strlen(narrative[index])))
			return false;
	}
	return true;
}

static bool
quit_session(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t generating[] = "Generating ScoreBoard";
	static const char reminder[] =
	    "PLEASE HELP YOUR SYSOP REGISTER THIS GAME.";
	struct yt_normal_exit_registration_result registration;
	uint8_t registered_raw[4];
	char returning[sizeof(session->door->identity.system) + 20U];
	int length;

	if (!session->door->game_open)
		return true;
	session_set_foreground(session, 1.0f);
	if (!show_ship(session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-Info blank", error)
	    || !session_present_timed_paged_row(session, generating, sizeof(generating) - 1U,
	    "normal-exit generating row", error)
	    || !yt_session_generate_scoreboard(session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-generator blank", error))
		return false;
	session_set_pager_nonstop(session, 1.0f);
	if (!session_display_game_file(session,
	    session->door->game.config.scoreboard, error))
		return false;
	if (qb_mbf32_encode(session->registered ? -1.0f : 0.0f,
	    registered_raw) != QB_MBF_OK)
		return false;
	if (!yt_normal_exit_registration_evaluate(registered_raw,
	    session->presentation.sound.conversion_mode, &registration, error))
		return false;
	if (registration.route == YT_NORMAL_EXIT_REGISTRATION_REMINDER) {
		if (!session_attention(session, reminder,
		    "normal-exit registration reminder", error)
		    || !session_wait(session, 10.0,
		    "normal-exit registration wait", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "normal-exit reminder blank", error))
			return false;
	}
	length = snprintf(returning, sizeof(returning), "Returning to %s...",
	    session->door->identity.system);
	if (length < 0 || (size_t)length >= sizeof(returning)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "normal-exit BBS row capacity");
		}
		return false;
	}
	return session_present_paged_fragment(session, (const uint8_t *)returning,
	    (size_t)length);
}

static bool
command_shell(struct yt_session *session, struct yt_error *error)
{
	while (session->running && !session_is_destroyed(session)) {
		char command[YT_COMMAND_SIZE];
		uint8_t prompt[512];
		size_t prompt_length;
		size_t response_length;
		enum yt_main_shell_route route;
		bool enter_sector = false;

		session_set_pager_line_count(session, 0.0f);
		if (!session_reload_player(session, error))
			return false;
		session_set_foreground(session, 2.0f);
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "main prompt leading blank", error))
			return false;
		session_set_relationship(session, 0.0f);
		if (!yt_main_prompt_row((const uint8_t *)session->time.text,
		    session->time.text_length, prompt, sizeof(prompt),
		    &prompt_length))
			return false;
		memcpy(session->output_source, prompt, prompt_length);
		session->output_source[prompt_length] = '\0';
		if (!session_present_timed_paged_row(session, prompt,
		    prompt_length, "main prompt low-time warning", error))
			return false;
		if (!session_read_upper_command(session, command,
		    sizeof(command)))
			return true;
		response_length = strlen(command);
		memcpy(session->output_source, command,
		    response_length + 1U);
		route = yt_main_shell_dispatch(command);
		switch (route) {
		case YT_MAIN_SHELL_DISPLAY:
			if (!session_present_paged_line(session,
			    (const uint8_t *)"<Display>",
			    strlen("<Display>"), "main display heading", error))
				return false;
			if (!display_sector(session, false, error))
				return false;
			continue;
		case YT_MAIN_SHELL_SOUND:
		{
			struct yt_present_result presentation;
			enum yt_present_status status =
			    yt_present_sound_toggle(&session->presentation,
			    &presentation);

			yt_out_present_result(&presentation);
			if (status != YT_PRESENT_OK) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation),
					    "sound toggle");
				}
				return false;
			}
			if (!display_sector(session, false, error))
				return false;
			continue;
		}
		case YT_MAIN_SHELL_SENSORS:
			if (!display_sector(session, true, error))
				return false;
			continue;
		case YT_MAIN_SHELL_VERSION:
			session_set_foreground(session, 6.0f);
			if (!registration(session, error))
				return false;
			if (!session->running)
				return true;
			continue;
		case YT_MAIN_SHELL_INFO:
			if (!show_ship(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_INSTRUCTIONS:
			if (!session_present_paged_fragment(session,
			    (const uint8_t *)"<Instructions>",
			    strlen("<Instructions>"))
			    || !instruction_offer(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_HELP:
			if (!show_help(session, error))
				return false;
			continue;
		case YT_MAIN_SHELL_INVALID:
			if (!session_present_alert(session,
			    (const uint8_t *)"Invalid command.",
			    strlen("Invalid command."),
			    "main invalid command", error))
				return false;
			continue;
		case YT_MAIN_SHELL_WARP:
			if (!direct_emergency_warp(session, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_MISSILE:
			if (!command_projectile(session, false, error))
				return false;
			if (!session_is_destroyed(session)
			    && !display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_PLASMA:
			if (!command_projectile(session, true, error))
				return false;
			if (!session_is_destroyed(session)
			    && !display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_ATTACK:
			if (!command_attack_player(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_BUY_PORT:
			if (!command_buy_port_cycle(session, error))
				return false;
			break;
		case YT_MAIN_SHELL_COMPUTER:
			if (!computer_menu(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_FIGHTERS:
			if (!command_fighters(session, error))
				return false;
			break;
		case YT_MAIN_SHELL_LAND:
			if (!command_land(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_MOVE:
			if (!command_move(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_TRADE:
			if (!command_trade(session, &enter_sector, error))
				return false;
			break;
		case YT_MAIN_SHELL_QUIT:
		{
			bool confirmed;

			if (!session_quit_confirm(session, &confirmed, error))
				return false;
			if (!confirmed)
				break;
			if (!quit_session(session, error))
				return false;
			session->running = false;
			session->terminated = true;
			return false;
		}
		case YT_MAIN_SHELL_TEAM:
			if (!command_team(session, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_MINES:
			if (!command_mines(session, error))
				return false;
			session_set_foreground(session, 1.0f);
			if (!display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_COLLECT:
			if (!command_collect(session,
			    YT_TREASURY_CALLER_MAIN_COLLECT, error))
				return false;
			enter_sector = true;
			break;
		case YT_MAIN_SHELL_GENESIS:
			if (!command_genesis(session, error))
				return false;
			break;
		case YT_MAIN_SHELL_RENAME_PORT:
			if (!command_rename_port_cycle(session, error))
				return false;
			break;
		}
		if (enter_sector && session->running && !session_is_destroyed(session)
		    && !sector_entry(session, error))
			return false;
	}
	return true;
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
	    || !registration(&session, error))
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
	else if (!sector_entry(&session, error)) {
		if (!session_handle_gameplay_fault(&session, error,
		    &resume_gameplay))
			return false;
	}
	if (session.terminated)
		return true;
	if (!session_is_destroyed(&session) && session.running) {
		for (;;) {
			bool completed = resume_gameplay
			    ? sector_entry(&session, error)
			    : command_shell(&session, error);
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
	return quit_session(&session, error);
}
