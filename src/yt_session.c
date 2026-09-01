#include "yt_session.h"

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
#include "yt_score.h"
#include "yt_sound.h"
#include "yt_startup_model.h"
#include "yt_text.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define YT_PLAYER_FIRST 2
#define YT_PLAYER_LAST 51
#define YT_COMMAND_SIZE 4096U

enum navigation_field_kind {
	NAVIGATION_FIELD_NONE,
	NAVIGATION_FIELD_ENTRY_PLAYER,
	NAVIGATION_FIELD_ROUTE_SECTOR,
	NAVIGATION_FIELD_INNER_PLAYER,
	NAVIGATION_FIELD_FINAL_SECTOR,
	NAVIGATION_FIELD_RETURN_PLAYER,
};

struct yt_session {
	struct yt_door *door;
	const char *executable_path;
	int player_record;
	struct yt_player player;
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	size_t cached_player_name_length;
	float sector_cache[YT_PLAYER_LAST + 1];
	float cloak_cache[YT_PLAYER_LAST + 1];
	float black_hole[2];
	float market_base[3];
	struct yt_startup_main_prefix startup_prefix;
	float startup_cache_guard;
	char queue[YT_COMMAND_SIZE];
	size_t queue_length;
	size_t queue_position;
	char command_accumulator[YT_COMMAND_SIZE];
	char paged_text[YT_COMMAND_SIZE];
	char output_source[YT_COMMAND_SIZE];
	struct yt_input_splitter input;
	char saved_command[YT_COMMAND_SIZE];
	bool registered;
	bool running;
	bool terminated;
	bool destroyed;
	bool fatal_wait_complete;
	struct yt_present_state presentation;
	bool suppress_self_mines;
	uint8_t mercenaries_hurt_raw[4];
	float clearance_holds;
	float clearance_fighters;
	float clearance_ground;
	float clearance_shields;
	float counterlaunch_count;
	float current_sector_record;
	float earth_report_seen;
	uint8_t earth_report_seen_raw[4];
	float phase_scratch;
	uint8_t phase_scratch_raw[4];
	float relationship_scratch;
	uint8_t relationship_scratch_raw[4];
	float shared_loop_scratch;
	float planet_record_scratch;
	float attack_commitment;
	uint8_t attack_commitment_raw[4];
	bool anti_cloak;
	int spies[3];
	int spy_marker[3];
	int spy_count;
	float spy_found_scratch;
	float spy_dead_counter_scratch;
	float spy_warp_destination_scratch;
	float avoid[30];
	float computer_path_marker;
	uint8_t computer_path_marker_raw[4];
	float computer_path_start;
	uint8_t computer_path_start_raw[4];
	float computer_path_destination;
	uint8_t computer_path_destination_raw[4];
	float computer_path_hops;
	uint8_t computer_path_hops_raw[4];
	char computer_route_scratch[YT_COMMAND_SIZE];
	size_t computer_route_scratch_length;
	bool navigation_field_active;
	enum navigation_field_kind navigation_field_kind;
	int navigation_field_record;
	struct yt_record navigation_field;
	float current_planet;
	char planet_name[42];
	float current_warps[6];
	uint8_t current_warps_raw[6][4];
	double planet_quantity[10];
	float session_deadline;
	struct yt_present_time_state time;
	float low_time_remembered;
	struct yt_pager_state pager;
	struct yt_timed_wait_state wait;
	struct yt_input_value input_residue;
	char team_audit_message[YT_COMMAND_SIZE];
	uint8_t hostile_owner_label[160];
	size_t hostile_owner_label_length;
	struct yt_team_loader_cache team_cache;
};

static bool random_value(struct yt_session *session, float *value,
    struct yt_error *error);
static bool projectile_damage_draw(void *context, float *value,
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
static bool session_carrier(struct yt_session *session);
static bool session_b05d(struct yt_session *session, const uint8_t *text,
    size_t length);
static bool session_store_output_source(struct yt_session *session,
    const uint8_t *text, size_t length);
static bool session_0317(struct yt_session *session, const uint8_t *text,
    size_t length, const char *operation, struct yt_error *error);
static bool computer_port_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error);
static bool computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error);
static bool port_report_length(struct yt_session *session, float raw,
    size_t maximum, size_t *length, const char *operation,
    struct yt_error *error);
static bool fighter_shield_spill(struct yt_session *session,
    double *fighters, float *shields, struct yt_error *error);

static bool
session_poll_merged(struct yt_session *session,
    struct yt_input_value *selected)
{
	return yt_input_poll_merged(&session->input, selected);
}

static int
session_input_key(struct yt_session *session)
{
	for (;;) {
		struct yt_input_value selected;

		if (!session_poll_merged(session, &selected))
			return EOF;
		if (selected.length == 1)
			return selected.bytes[0];
		if (!od_carrier() && !od_control.od_force_local
		    && !session->door->identity.local)
			return EOF;
		od_sleep(10);
	}
}

static bool
session_timed_wait(struct yt_session *session, double seconds)
{
	enum yt_timed_wait_reason reason;

	if (!yt_timed_wait_begin(&session->wait, (float)seconds,
	    (float)yt_platform_timer()))
		return false;
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, 0, false};

		reason = yt_timed_wait_timer(&session->wait,
		    (float)yt_platform_timer());
		if (reason != YT_TIMED_WAIT_CONTINUE)
			return reason != YT_TIMED_WAIT_ERROR;
		if (!yt_input_poll_legacy(&session->input,
		    session->presentation.sound.mode, YT_INPUT_PHASE_WAIT,
		    &selected))
			return false;
		reason = yt_timed_wait_input(&session->wait,
		    session->presentation.sound.mode, &selected);
		if (reason != YT_TIMED_WAIT_CONTINUE)
			return reason != YT_TIMED_WAIT_ERROR;
	}
}

static bool
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
session_editor_notice(void *context, const uint8_t *notice, size_t length)
{
	return session_0317(context, notice, length,
	    "editor terminal notice", NULL);
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
session_editor_end(struct yt_session *session,
    enum yt_ab36_terminal_kind kind)
{
	(void)yt_input_ab36_terminal_run(kind, &session->running,
	    &session->terminated, session_editor_notice,
	    session_editor_close_all, session);
	return false;
}

static bool
session_ab36_repeat_emit(void *context, const uint8_t *prefix, size_t length)
{
	return session_b05d(context, prefix, length);
}

static bool
session_ab36_submit_line(void *context)
{
	struct yt_session *session = context;
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_line(NULL, 0, &session->presentation,
	    &presentation);
	if (status != YT_PRESENT_OK)
		return false;
	yt_out_present_result(&presentation);
	return true;
}

static bool
session_ab36_editor_echo(void *context, const uint8_t *local,
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
session_ab36_printable_carrier(void *context)
{
	return session_carrier(context);
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

static int
sector_count(const struct yt_session *session)
{
	return (int)(session->door->game.config.port_offset
	    - session->door->game.config.sector_offset);
}

static int
port_count(const struct yt_session *session)
{
	return (int)(session->door->game.config.planet_offset
	    - session->door->game.config.port_offset);
}

static bool
write_player(struct yt_session *session, struct yt_error *error)
{
	return yt_game_write_player(&session->door->game,
	    session->player_record, &session->player, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
session_hydration_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
reload_player(struct yt_session *session, struct yt_error *error)
{
	struct yt_current_player_hydration_state state = {
		&session->player,
		session->player_record,
		(int)session->door->game.config.sector_offset,
		session->door->game.config.sector_offset,
		&session->current_sector_record,
		session->sector_cache,
		session->cloak_cache,
		YT_ARRAY_LEN(session->sector_cache),
		session->anti_cloak,
	};

	return yt_current_player_hydrate_run(&state,
	    session_hydration_read_player, session, error);
}

static bool
credit_mutation_write_player(void *context, int player_record,
    const struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write_durable(&session->door->game.database,
	    (size_t)player_record, record, error);
}

static bool
mutate_player_credits_observed(struct yt_session *session, float argument,
    bool *hydrated, struct yt_error *error)
{
	static const struct yt_credit_mutation_ops ops = {
		session_hydration_read_player,
		credit_mutation_write_player,
	};
	struct yt_credit_mutation_state state;

	memset(&state, 0, sizeof(state));
	state.hydration.player = &session->player;
	state.hydration.player_record = session->player_record;
	state.hydration.last_player_record =
	    (int)session->door->game.config.sector_offset;
	state.hydration.sector_record_offset =
	    session->door->game.config.sector_offset;
	state.hydration.current_sector_record =
	    &session->current_sector_record;
	state.hydration.sector_cache = session->sector_cache;
	state.hydration.cloak_cache = session->cloak_cache;
	state.hydration.cache_count = YT_ARRAY_LEN(session->sector_cache);
	state.hydration.anti_cloak = session->anti_cloak;
	state.argument = argument;
	if (!yt_credit_mutation_run(&state, &ops, session, error)) {
		if (hydrated != NULL)
			*hydrated = state.hydrated;
		return false;
	}
	if (hydrated != NULL)
		*hydrated = state.hydrated;
	return true;
}

static bool
mutate_player_credits(struct yt_session *session, float argument,
    struct yt_error *error)
{
	return mutate_player_credits_observed(session, argument, NULL, error);
}

static bool
apply_player_credit_mutation(void *context, float player_record,
    float argument, struct yt_player *player, bool *hydrated,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != (float)session->player_record) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "credit mutation player record");
		}
		return false;
	}
	if (hydrated != NULL)
		*hydrated = false;
	if (!mutate_player_credits_observed(session, argument, hydrated,
	    error)) {
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
	if (!reload_player(session, error))
		return false;
	if (session->navigation_field_active) {
		session->navigation_field_kind = NAVIGATION_FIELD_RETURN_PLAYER;
		session->navigation_field_record = session->player_record;
		session->navigation_field = session->player.record;
		session->navigation_field_active = false;
	}
	return true;
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

static bool
append_news_bytes(struct yt_session *session, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	(void)session;
	return session_close_file5(error)
	    && yt_news_append_bytes(text, length, error);
}

static bool
radio_append_bytes(const uint8_t *text, size_t length, float sender,
    float recipient,
    struct yt_error *error)
{
	struct yt_radio_file file;
	struct yt_radio_record record;
	struct yt_error close_error;
	uint32_t basic_record;

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error)
	    || !yt_radio_file_next_record(&file, &basic_record, error)
	    || !yt_radio_file_get(&file, basic_record, &record, NULL,
	    error))
		goto failed;
	if (!yt_radio_message_record(&record, text, length, sender, recipient)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "construct radio record");
		}
		goto failed;
	}
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
radio_append(const char *text, float sender, float recipient,
    struct yt_error *error)
{
	return radio_append_bytes((const uint8_t *)text, strlen(text), sender,
	    recipient, error);
}

static bool
read_keyboard_line(struct yt_session *session, char *dest, size_t size)
{
	float inactivity_deadline;

	if (size == 0)
		return false;
	yt_pager_editor_enter(&session->pager, session->command_accumulator,
	    sizeof(session->command_accumulator));
	inactivity_deadline = single_add(
	    floorf((float)yt_platform_timer()), 180.0f);
	dest[0] = '\0';
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, 0, false};
		bool queued = session->queue_position < session->queue_length;
		uint8_t key;

		if (yt_input_ab36_inactivity_expired(
		    (float)yt_platform_timer(), inactivity_deadline,
		    session->presentation.sound.mode))
			return session_editor_end(session,
			    YT_AB36_TERMINAL_INACTIVITY);
		if (!session_carrier(session))
			return false;
		if (!info_refresh_time(session, NULL))
			return false;
		if (yt_input_ab36_session_expired((float)yt_platform_timer(),
		    session->session_deadline))
			return session_editor_end(session,
			    YT_AB36_TERMINAL_SESSION_LIMIT);
		if (queued) {
			if (!yt_input_ab36_queue_pop(session->queue,
			    sizeof(session->queue), &session->queue_position,
			    &session->queue_length, &selected))
				return false;
		}
		else {
			if (!yt_input_poll_legacy(&session->input,
			    session->presentation.sound.mode,
			    YT_INPUT_PHASE_AB36, &selected))
				return false;
			if (selected.length == 0) {
				od_sleep(10);
				continue;
			}
		}
		if (selected.length != 1)
			continue;
		key = selected.bytes[0];
		if (yt_input_ab36_repeat_requested(queued, &selected)) {
			if (!yt_input_ab36_repeat_run(
			    session->command_accumulator,
			    sizeof(session->command_accumulator),
			    session->saved_command,
			    sizeof(session->saved_command),
			    session->paged_text,
			    sizeof(session->paged_text),
			    &session->pager.newline_flag, &key,
			    session_ab36_repeat_emit, session))
				return false;
		}
		if (yt_input_ab36_submit_requested(key)) {
			if (!yt_input_ab36_submit_run(
			    &session->pager.newline_flag,
			    session_ab36_submit_line, session))
				return false;
			snprintf(dest, size, "%s", session->command_accumulator);
			return true;
		}
		{
			bool handled;

			if (!yt_input_ab36_backspace_run(key,
			    session->command_accumulator,
			    sizeof(session->command_accumulator), &handled,
			    session_ab36_editor_echo, session))
				return false;
			if (handled)
				continue;
		}
		{
			bool handled;

			if (!yt_input_ab36_printable_run(key,
			    session->command_accumulator,
			    sizeof(session->command_accumulator), size,
			    session->paged_text, sizeof(session->paged_text),
			    &session->pager.newline_flag, &handled,
			    session_ab36_editor_echo,
			    session_ab36_printable_carrier, session))
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
session_command_notice(struct yt_session *session, const char *text,
    bool bold)
{
	if (bold)
		session->presentation.bold = 1.0f;
	if (!session_0317(session, (const uint8_t *)text, strlen(text),
	    "command notice", NULL))
		return false;
	return session_timed_wait(session, 1.0);
}

static bool
expand_repeat(struct yt_session *session, char *text, size_t size)
{
	struct yt_repeat_transform result;
	char notice[128];
	char rendered[64];
	int notice_length;

	if (!yt_input_expand_repeat(text, size, session->saved_command,
	    sizeof(session->saved_command), &result))
		return false;
	if (!result.emit_notice)
		return true;
	if (qb_str_double(rendered, sizeof(rendered), (double)result.count) < 0)
		return false;
	notice_length = snprintf(notice, sizeof(notice),
	    "Command Repeated%s times -+- Ctrl-R to Re-use -+- "
	    "Ctrl-X to cancel.", rendered);
	if (notice_length < 0 || (size_t)notice_length >= sizeof(notice))
		return false;
	return session_command_notice(session, notice, true);
}

static bool
session_line(struct yt_session *session, char *text, size_t size)
{
	bool save_requested;
	size_t length;

	if (!read_keyboard_line(session, text, size))
		return false;
	if (!yt_input_command_save_requested(text, size, &save_requested))
		return false;
	length = strlen(text);
	if (save_requested) {
		text[--length] = '\0';
		clear_queue(session);
		snprintf(session->saved_command, sizeof(session->saved_command),
		    "%s", text);
		if (!session_command_notice(session,
		    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.",
		    false))
			return false;
	}
	if (!expand_repeat(session, text, size))
		return false;
	return yt_input_split_semicolon(text, session->queue,
	    sizeof(session->queue), &session->queue_position,
	    &session->queue_length);
}

static bool
session_0345(struct yt_session *session, char *text, size_t size)
{
	return session_line(session, text, size)
	    && session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_0357(struct yt_session *session, char *text, size_t size)
{
	if (!session_0345(session, text, size))
		return false;
	qb_compat_upper(text);
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_036f(struct yt_session *session, char *text, size_t size)
{
	if (!session_0345(session, text, size))
		return false;
	yt_input_numeric_response(text);
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_carrier(struct yt_session *session)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	bool carrier_detected;

	carrier_detected = od_carrier();
	if (yt_input_carrier_returns(session->presentation.sound.mode,
	    carrier_detected))
		return true;
	status = yt_present_carrier_drop(&session->presentation, &presentation);
	if (status == YT_PRESENT_OK)
		yt_out_present_result(&presentation);
	(void)session_editor_close_all(session);
	session->running = false;
	session->terminated = true;
	return false;
}

static bool
session_paged_carrier(void *context)
{
	return session_carrier(context);
}

static bool
session_paged_sample(void *context, struct yt_input_value *sampled)
{
	struct yt_session *session = context;

	return yt_input_poll_legacy(&session->input,
	    session->presentation.sound.mode, YT_INPUT_PHASE_B05D, sampled);
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
session_b05d(struct yt_session *session, const uint8_t *text, size_t length)
{
	static const struct yt_paged_row_ops ops = {
		session_paged_carrier,
		session_paged_sample,
		session_paged_present,
		session_paged_finish,
		session_paged_response,
	};
	struct yt_b05d_key_state key_state = {
		.accumulator = session->command_accumulator,
		.accumulator_capacity = sizeof(session->command_accumulator),
		.queue = session->queue,
		.queue_capacity = sizeof(session->queue),
		.queue_position = &session->queue_position,
		.queue_length = &session->queue_length,
		.pager_key = session->pager.key,
		.pager_key_capacity = sizeof(session->pager.key),
	};

	if (!session_store_output_source(session, text, length))
		return false;
	return yt_paged_row_run(&session->pager, &session->presentation,
	    &key_state, text, length, &ops, session);
}

static void
session_set_color(struct yt_session *session, int logical)
{
	static const int pc_color[8] = {0, 4, 2, 6, 1, 5, 3, 7};

	session->pager.foreground = logical;
	session->presentation.foreground = (float)logical;
	session->presentation.background = 0.0f;
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

enum session_present_text_kind {
	SESSION_PRESENT_LINE,
	SESSION_PRESENT_RAW,
	SESSION_PRESENT_BOLD_LINE,
	SESSION_PRESENT_BOLD_RAW
};

static bool
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
session_02fc(struct yt_session *session, const uint8_t *text, size_t length)
{
	session->pager.newline_flag = 0.0f;
	return session_b05d(session, text, length);
}

static bool
session_0317(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	return session_02fc(session, text, length);
}

static bool
session_02db(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	session->presentation.bold = 1.0f;
	session->presentation.blink = 1.0f;
	clear_queue(session);
	return session_02fc(session, text, length);
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

static bool
session_031f(struct yt_session *session, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	if (!session_low_time(session, operation, error))
		return false;
	session->pager.newline_flag = 1.0f;
	return session_b05d(session, text, length);
}

static bool
session_drain_pending_input(struct yt_session *session)
{
	struct yt_input_drain_state drain;
	enum yt_input_drain_reason reason;

	if (!yt_input_drain_begin(&drain, &session->input_residue))
		return false;
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, 0, false};

		if (!yt_input_poll_source(&session->input, false, &selected))
			return false;
		reason = yt_input_drain_local(&drain, &selected);
		if (reason == YT_INPUT_DRAIN_ERROR)
			return false;
		if (reason == YT_INPUT_DRAIN_LOCAL_COMPLETE)
			break;
	}
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, 0, false};

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
	int saved_session_foreground = session->pager.foreground;

	if (drain && !session_drain_pending_input(session)) {
		failure_status = YT_IO_ERROR;
		failure_operation = "press any key input drain";
		goto failed;
	}
	status = yt_present_press_prompt(&session->presentation,
	    &presentation, &saved_foreground);
	if (status != YT_PRESENT_OK)
		goto failed;
	session->pager.foreground = 3;
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
	session->pager.foreground = saved_session_foreground;
	return true;

failed:
	if (error != NULL) {
		error->status = failure_status;
		snprintf(error->operation, sizeof(error->operation),
		    "%s", failure_operation);
	}
	return false;
}

static bool
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

static bool
session_file_viewer_entry_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct yt_session *session = context;

	if (paged)
		return session_0317(session, text, length,
		    "file viewer notice", error);
	return session_present_text(session, text, length,
	    SESSION_PRESENT_LINE, "file viewer pre-open blank", error);
}

static bool
session_file_viewer_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct yt_session *session = context;

	if (paged)
		return session_b05d(session, text, length);
	return session_present_text(session, text, length,
	    SESSION_PRESENT_LINE, "file viewer final blank", error);
}

struct session_file_viewer_context {
	struct yt_session *session;
	struct yt_text_input input;
};

static bool
session_file_viewer_close(void *context, struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	return yt_text_input_close(&viewer->input, error);
}

static bool
session_file_viewer_open(void *context, const char *path,
    struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	return yt_text_input_open(&viewer->input, path, error);
}

static bool
session_file_viewer_eof(void *context, bool *eof, struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	return yt_text_input_eof(&viewer->input, eof, error);
}

static bool
session_file_viewer_read(void *context, const uint8_t **line,
    size_t *length, bool *available, struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	return yt_text_input_read_line(&viewer->input, line, length, available,
	    error);
}

static bool
session_file_viewer_stream_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct session_file_viewer_context *viewer = context;

	return session_file_viewer_present(viewer->session, text, length, paged,
	    error);
}

static bool
session_file_viewer_missing_present(void *context, const uint8_t *text,
    size_t length, bool paged, struct yt_error *error)
{
	struct yt_session *session = context;

	(void)paged;
	return session_0317(session, text, length,
	    "file viewer missing row", error);
}

static bool
session_file_viewer_missing_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	static const struct yt_file_viewer_stream_ops ops = {
		session_file_viewer_close,
		session_file_viewer_open,
		session_file_viewer_eof,
		session_file_viewer_read,
		session_file_viewer_stream_present,
	};
	struct session_file_viewer_context context = {
		.session = session,
	};
	struct yt_error local_error;
	struct yt_error *active_error = error == NULL ? &local_error : error;
	float saved_foreground = session->presentation.foreground;
	int saved_pager_foreground = session->pager.foreground;
	struct yt_file_viewer_stream_state state = {
		.play = {
			.foreground = &session->presentation.foreground,
			.pager_foreground = &session->pager.foreground,
			.bold = &session->presentation.bold,
			.line_count = &session->pager.line_count,
			.pager_key = session->pager.key,
			.saved_foreground = saved_foreground,
			.saved_pager_foreground = saved_pager_foreground,
		},
		.path = path,
	};
	bool ok;

	if (error == NULL)
		yt_error_clear(&local_error);
	if (!yt_file_viewer_entry(session->pager.key,
	    &session->pager.line_count, session_file_viewer_entry_present,
	    session, active_error))
		return false;
	yt_text_input_init(&context.input);
	ok = yt_file_viewer_stream_run(&state, &ops, &context, active_error);
	yt_text_input_destroy(&context.input);
	if (!ok && active_error->status == YT_NOT_FOUND) {
		struct yt_main_error_result handler;

		if (!yt_main_error_compose(53, 40000,
		    (const uint8_t *)path, strlen(path), NULL, 0U, NULL, 0U,
		    &handler)
		    || handler.route != YT_MAIN_ERROR_MISSING_FILE
		    || !session_forced_local_line(handler.debug,
		    handler.debug_length, "file viewer missing debug row",
		    active_error))
			return false;
		yt_error_clear(active_error);
		return yt_file_viewer_missing((const uint8_t *)path, strlen(path),
		    session_file_viewer_missing_present,
		    session_file_viewer_missing_news, session, active_error);
	}
	return ok;
}

struct xannor_victory_file_context {
	struct yt_session *session;
	struct yt_text_input input;
};

static bool
xannor_victory_file_close(void *context, struct yt_error *error)
{
	struct xannor_victory_file_context *file_context = context;

	return yt_text_input_close(&file_context->input, error);
}

static bool
xannor_victory_file_open(void *context, const char *path,
    struct yt_error *error)
{
	struct xannor_victory_file_context *file_context = context;

	return yt_text_input_open(&file_context->input, path, error);
}

static bool
xannor_victory_file_read(void *context, const uint8_t **line,
    size_t *length, bool *available, struct yt_error *error)
{
	struct xannor_victory_file_context *file_context = context;

	return yt_text_input_read_line(&file_context->input, line, length,
	    available, error);
}

static bool
xannor_victory_file_present(void *context, const uint8_t *line,
    size_t length, struct yt_error *error)
{
	struct xannor_victory_file_context *file_context = context;

	return session_present_text(file_context->session, line, length,
	    SESSION_PRESENT_LINE, "Xannor victory file row", error);
}

static bool
xannor_victory_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	static const struct yt_text_sequential_play_ops ops = {
		xannor_victory_file_close,
		xannor_victory_file_open,
		xannor_victory_file_read,
		xannor_victory_file_present,
	};
	struct xannor_victory_file_context context = {
		.session = session,
	};
	struct yt_text_sequential_play_state state = {
		.path = path,
	};
	bool ok;

	yt_text_input_init(&context.input);
	ok = yt_text_sequential_play_run(&state, &ops, &context, error);
	yt_text_input_destroy(&context.input);
	return ok;
}

static bool
session_a8d2(struct yt_session *session, const uint8_t *prompt,
    size_t prompt_length, enum yt_yes_no_answer *answer,
    struct yt_error *error)
{
	if (answer == NULL)
		return false;
	for (;;) {
		char response[YT_COMMAND_SIZE];

		if (!session_present_text(session, prompt, prompt_length,
		    SESSION_PRESENT_RAW, "yes/no prompt", error)
		    || !session_0357(session, response, sizeof(response))
		    || !yt_input_yes_no_candidate(response, session->output_source,
		    sizeof(session->output_source), answer))
			return false;
		if (*answer != YT_YES_NO_INVALID)
			return true;
		session->presentation.bold = 1.0f;
		clear_queue(session);
	}
}

static bool
startup_configuration_close(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	bool closed = yt_database_random_close(&session->door->game.database,
	    error);

	if (closed)
		session->door->game_open = false;
	return closed;
}

static bool
startup_configuration_open(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	bool opened = yt_database_open(&session->door->game.database,
	    "YTDATA.DAT", YT_OPEN_UPDATE, error);

	if (opened)
		session->door->game_open = true;
	return opened;
}

static bool
startup_configuration_load(void *context, struct yt_config *config,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_config_load(&session->door->game.database, config, error);
}

static bool
startup_configuration_store(void *context, const struct yt_config *config,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database, 1U,
	    &config->record, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
startup_configuration_read_player(void *context, int basic,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, basic, player, error);
}

static bool
startup_configuration_write_player(void *context, int basic,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database, (size_t)basic,
	    &player->record, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
startup_configuration_random(void *context, float *value,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_random_next(&session->door->game.random, value, error);
}

static bool
load_configuration(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_startup_configuration_ops ops = {
		startup_configuration_close,
		startup_configuration_open,
		startup_configuration_load,
		startup_configuration_store,
		startup_configuration_read_player,
		startup_configuration_write_player,
		startup_configuration_random,
	};
	struct yt_game *game = &session->door->game;
	struct yt_startup_configuration_state state;
	bool ok;

	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	memset(&state, 0, sizeof(state));
	state.config = &game->config;
	state.local_mode = session->door->identity.local ? -1.0f : 0.0f;
	state.cache_guard = session->startup_cache_guard;
	state.sector_cache = session->sector_cache;
	state.cloak_cache = session->cloak_cache;
	state.cache_count = YT_ARRAY_LEN(session->sector_cache);
	state.black_hole[0] = session->black_hole[0];
	state.black_hole[1] = session->black_hole[1];
	ok = yt_startup_configuration_run(&state, &ops, session, error);
	session->startup_cache_guard = state.cache_guard;
	session->black_hole[0] = state.black_hole[0];
	session->black_hole[1] = state.black_hole[1];
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

	return yt_file_kill(context->path, NULL, error);
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
	/* A478 clears the live validated flag before the first file operation. */
	session->registered = false;
	completed = yt_registration_run(&state, &ops, &context, error);
	if (context.sequential.file != NULL
	    || context.sequential.orphaned_file != NULL
	    || context.random.file != NULL
	    || context.random.orphaned_file != NULL)
		(void)registration_close_file4(&context, NULL);
	yt_text_input_destroy(&context.sequential);
	if (!completed) {
		free(storage);
		return false;
	}
	session->registered = state.registered;
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
	return session_wait(session,
	    state.outcome == YT_REGISTRATION_REGISTERED ? 2.0 : 10.0,
	    state.outcome == YT_REGISTRATION_REGISTERED
	    ? "registration registered wait" : "registration evaluation wait",
	    error);
}

static bool
opening_poll_local(void *context, bool *ready, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_input_value local = {{0, 0}, 0, 0, false};

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
	*ready = session->input.remote.position
	    < session->input.remote.length;
	return true;
}

static bool
opening_wait(void *context, float seconds, struct yt_error *error)
{
	return session_wait(context, seconds, "ANSI opening EOF wait", error);
}

static bool
opening_and_date(struct yt_session *session, struct yt_error *error)
{
	int16_t route[YT_ROUTE_CAPACITY];
	bool found;
	char real_name[258];
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (!build_route(session, 1, 2, route, false, &found, NULL, NULL,
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
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
		if (!session_present_text(session, diagnostic,
		    sizeof(diagnostic) - 1U, SESSION_PRESENT_LINE,
		    "startup route failure diagnostic", error))
			return false;
	}
	if (session->door->identity.ansi) {
		if (!yt_out_opening_file("YTOPEN.ANS",
		    session->presentation.sound.mode,
		    session->presentation.sound.snoop, opening_poll_local,
		    opening_poll_remote, opening_wait, session, error))
			return false;
	}
	snprintf(real_name, sizeof(real_name), "%s %s",
	    session->door->identity.real_first,
	    session->door->identity.real_last);
	status = yt_present_status_row((const uint8_t *)real_name,
	    strlen(real_name), (const uint8_t *)"", 0,
	    &session->presentation, &presentation);
	if (status != YT_PRESENT_OK) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "startup status row");
		}
		return false;
	}
	yt_out_present_result(&presentation);
	session->pager.nonstop = 1.0f;
	return display_game_file(session, "YTOPEN.ASC", error);
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

	return session_wait(lockout->session, seconds, "lockout denial wait",
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
		.random_path = "lockout.dat",
		.input_path = "Lockout.dat",
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

	session->presentation.foreground = 5.0f;
	session->pager.foreground = 5;
	if (!session_0317(session, (const uint8_t *)"Initializing...",
	    strlen("Initializing..."), "startup initializing row", error)
	    || !yt_current_date_serial(
	    session->door->game.config.epoch_year,
	    &session->door->game.today, &session->door->game.adjusted_year,
	    error)
	    || !lockout(session, error))
		return false;
	snprintf(welcome, sizeof(welcome), "Welcome %s!",
	    session->door->identity.real_first);
	if (!session_0317(session, (const uint8_t *)welcome, strlen(welcome),
	    "startup welcome row", error)
	    || !session_02fc(session,
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

		session->presentation.foreground = 2.0f;
		session->pager.foreground = 2;
		if (!session_0317(session,
		    (const uint8_t *)"You are a new player.",
		    strlen("You are a new player."), "new alias notice", error)
		    || !session_0317(session,
		    (const uint8_t *)
		    "Enter the FULL alias you wish to use in the game.",
		    strlen("Enter the FULL alias you wish to use in the game."),
		    "new alias instruction", error)
		    || !session_0317(session,
		    (const uint8_t *)"Press [ENTER] to use your real name.",
		    strlen("Press [ENTER] to use your real name."),
		    "new alias real-name instruction", error)
		    || !session_031f(session, (const uint8_t *)"-+> ", 4,
		    "new alias prompt", error)
		    || !session_0345(session, alias, sizeof(alias))) {
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
			if (!session_02fc(session,
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
			if (!session_02fc(session, (const uint8_t *)collision,
			    strlen(collision))) {
				yt_names_free(&names);
				return false;
			}
			continue;
		}
		session->presentation.foreground = 3.0f;
		session->pager.foreground = 3;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "new alias identity blank", error)) {
			yt_names_free(&names);
			return false;
		}
		session->presentation.bold = 1.0f;
		{
			char identity[560];

			snprintf(identity, sizeof(identity), "%s %s a.k.a. %s",
			    first, last, display);
			if (!session_02fc(session, (const uint8_t *)identity,
			    strlen(identity))
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "new alias confirmation blank", error)) {
				yt_names_free(&names);
				return false;
			}
		}
		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
		if (!session_031f(session,
		    (const uint8_t *)"Is this OK (Y/[N])? ",
		    strlen("Is this OK (Y/[N])? "),
		    "new alias confirmation prompt", error)
		    || !session_0357(session, confirmation, sizeof(confirmation))) {
			yt_names_free(&names);
			return false;
		}
		if (strcmp(confirmation, "Y") != 0)
			continue;
		memset(&row, 0, sizeof(row));
		snprintf(row.real_first, sizeof(row.real_first), "%s", first);
		snprintf(row.real_last, sizeof(row.real_last), "%s", last);
		snprintf(row.alias_first, sizeof(row.alias_first), "%s",
		    alias_first);
		snprintf(row.alias_last, sizeof(row.alias_last), "%s",
		    alias_last);
		if (!yt_names_append("YTNAME.DAT", &row, error)) {
			yt_names_free(&names);
			return false;
		}
		if (!session_02db(session,
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
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "player constructor blank", error)
	    || !session_present_text(session,
	    (const uint8_t *)"Your ship has been built.",
	    strlen("Your ship has been built."), SESSION_PRESENT_LINE,
	    "player constructor row", error))
		return false;
	return yt_game_construct_player(&session->door->game,
	    session->player_record, (float)session->door->game.today,
	    &session->player, error);
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
		    || !session_0345(session, response, sizeof(response))
		    || !yt_input_yes_no_candidate(session->command_accumulator,
		    session->output_source, sizeof(session->output_source),
		    &answer))
			return false;
		if (answer == YT_YES_NO_EMPTY || answer == YT_YES_NO_NO)
			return true;
		if (answer == YT_YES_NO_YES)
			return display_game_file(session, "YTINSTR.DOC", error);
		session->presentation.bold = 1.0f;
		clear_queue(session);
	}
}

static bool
admit_player(struct yt_session *session, const char *first, const char *last,
    struct yt_error *error)
{
	char full[256];
	struct yt_clock_value now;
	int basic;
	bool returning = false;

	snprintf(full, sizeof(full), "%s %s", first, last);
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		struct yt_player candidate;
		bool matches;

		session->shared_loop_scratch = (float)basic;
		if (!yt_game_read_player(&session->door->game, basic, &candidate,
		    error)
		    || !yt_player_name_matches(&candidate, (const uint8_t *)full,
		    strlen(full), &matches, error))
			return false;
		if (matches) {
			session->player_record = basic;
			session->player = candidate;
			if (!yt_player_stored_name(&candidate,
			    session->cached_player_name,
			    &session->cached_player_name_length, error))
				return false;
			returning = true;
			break;
		}
		session->shared_loop_scratch = (float)(basic + 1);
	}
	if (!returning) {
		int vacant = 0;

		session->presentation.foreground = 5.0f;
		session->pager.foreground = 5;
		if (!session_0317(session,
		    (const uint8_t *)"Entering a new player...",
		    strlen("Entering a new player..."),
		    "new player entering row", error))
			return false;
		for (basic = YT_PLAYER_FIRST;
		    basic <= (int)session->door->game.config.sector_offset;
		    ++basic) {
			struct yt_player candidate;

			if (!yt_game_read_player(&session->door->game, basic,
			    &candidate, error))
				return false;
			if (candidate.name_length < 1.0f) {
				vacant = basic;
				break;
			}
		}

		if (vacant == 0) {
			char date[11];

			if (!session_02db(session,
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
		{
			char days[64];
			char retention[512];

			if (qb_str_single(days, sizeof(days),
			    session->door->game.config.retention_days) < 0)
				return false;
			snprintf(retention, sizeof(retention),
			    "Notice: If your ship is dead and you have not played for%s",
			    days);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "new player retention blank", error)
			    || !session_02fc(session, (const uint8_t *)retention,
			    strlen(retention))
			    || !session_02fc(session,
			    (const uint8_t *)
			    "days, it will be deleted to make room for someone else.",
			    strlen("days, it will be deleted to make room for someone else."))
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "new player retention blank", error))
				return false;
		}
		session->player_record = vacant;
		if (!construct_player_visible(session, error)
		    || !yt_game_set_player_identity(&session->door->game, vacant,
		    (const uint8_t *)full, strlen(full), &session->player, error)
		    || !yt_player_stored_name(&session->player,
		    session->cached_player_name,
		    &session->cached_player_name_length, error)
		    || !yt_database_flush(&session->door->game.database, error))
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
	session->presentation.foreground = 2.0f;
	session->pager.foreground = 2;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "returning player blank", error))
		return false;
	{
		float previous_day = session->player.last_active;
		float killer = session->player.killed_by;
		bool self_kill = killer == (float)session->player_record;

		if (previous_day == (float)session->door->game.today
		    && !session_present_text(session,
		    (const uint8_t *)"You have been on today.",
		    strlen("You have been on today."), SESSION_PRESENT_LINE,
		    "returning same-day row", error))
			return false;
		session->player.last_active = (float)session->door->game.today;
		if (previous_day != (float)session->door->game.today) {
			if (session->player.turns
			    < session->door->game.config.turns_per_day)
				session->player.turns =
				    session->door->game.config.turns_per_day;
			session->player.lottery_plays = 0.0f;
		}
		if (!write_player(session, error))
			return false;
		if (!yt_platform_clock(&now, error))
			return false;
		{
			char time_text[9];

			yt_format_time(&now, time_text);
			if (!session_close_file5(error)
			    || !yt_news_append_login(time_text,
			    session->player.name, error))
				return false;
		}
		if (killer != 0.0f) {
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "returning death blank", error))
				return false;
			if (!self_kill)
				session->presentation.blink = 1.0f;
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
			    && killer <= session->door->game.config.sector_offset) {
				struct yt_player attacker;
				uint8_t attacker_row[YT_TEXT_FIELD_SIZE
				    + sizeof(" destroyed your ship!") - 1U];
				size_t attacker_length;
				bool emit;

				if (!yt_game_read_player(&session->door->game,
				    (int)killer, &attacker, error))
					return false;
				if (!yt_player_killer_row(&attacker, attacker_row,
				    sizeof(attacker_row), &attacker_length, &emit, error))
					return false;
				if (emit && !session_present_text(session, attacker_row,
				    attacker_length, SESSION_PRESENT_BOLD_LINE,
				    "returning player death row", error))
					return false;
			}
			if (self_kill
			    && previous_day == (float)session->door->game.today) {
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE,
				    "returning self-denial blank", error))
					return false;
				session->presentation.foreground = 7.0f;
				session->pager.foreground = 7;
				session->presentation.blink = 1.0f;
				if (!session_present_text(session,
				    (const uint8_t *)
				    "You will be allowed to play again tomorrow!",
				    strlen("You will be allowed to play again tomorrow!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "returning self-denial row", error))
					return false;
				session->running = false;
				session->terminated = true;
				return false;
			}
			if (!construct_player_visible(session, error))
				return false;
			if (!session_wait(session, 5.0,
			    "returning-player rebuild wait", error))
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

		if (!yt_game_read_player(&session->door->game, (int)record,
		    &player, error)
		    || !yt_player_stored_name(&player, stored, &stored_length,
		    error))
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
radio_name(struct yt_session *session, float record, char *dest,
    size_t size, bool sender, struct yt_error *error)
{
	size_t length;

	if (size == 0
	    || !radio_name_bytes(session, record, (uint8_t *)dest, size - 1U,
	    &length, sender, error))
		return false;
	dest[length] = '\0';
	return true;
}

static bool
radio_read(struct yt_session *session, float reader_mode,
    struct yt_error *error)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t log_heading[] =
	    "Log of messages sent/recieved.";
	struct yt_radio_file file;
	struct yt_radio_record record;
	struct yt_radio_pager_state private_pager;
	uint64_t length;
	uint64_t probe_count;
	uint32_t basic_record;
	bool visible = false;
	float previous_recipient = 0.0f;
	float previous_sender = 0.0f;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio opening blank", error)
	    || !session_present_text(session,
	    reader_mode != 0.0f ? log_heading : automatic_heading,
	    reader_mode != 0.0f ? sizeof(log_heading) - 1U
	    : sizeof(automatic_heading) - 1U, SESSION_PRESENT_LINE,
	    "radio heading", error))
		return false;
	yt_radio_pager_begin(&private_pager);

	yt_radio_file_init(&file);
	if (!yt_radio_file_open(&file, "YTRMSG.DAT", error))
		return false;
	if (!yt_radio_file_size(&file, &length, error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	probe_count = length / YT_RADIO_RECORD_SIZE + 1U;
	if (probe_count > 0xFFFFFFU) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "radio scan bound");
			(void)snprintf(error->path, sizeof(error->path), "%s",
			    file.random.path);
		}
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	for (basic_record = 1U; basic_record <= probe_count; ++basic_record) {
		float counter;
		float recipient;
		float sender;
		struct yt_radio_reader_decision decision;

		if (!yt_radio_file_get(&file, basic_record, &record, NULL,
		    error)) {
			(void)yt_radio_file_close(&file, NULL);
			return false;
		}
		counter = yt_radio_get_number(&record, 0);
		recipient = yt_radio_get_number(&record, 4);
		sender = yt_radio_get_number(&record, 8);
		if (!yt_radio_reader_decide(counter, recipient, sender,
		    (float)session->player_record, reader_mode, &decision, error)) {
			(void)yt_radio_file_close(&file, NULL);
			return false;
		}

		if (decision.visible) {
			uint8_t from[YT_TEXT_FIELD_SIZE];
			uint8_t to[YT_TEXT_FIELD_SIZE];
			uint8_t header[2U * YT_TEXT_FIELD_SIZE + 32U];
			size_t from_length;
			size_t to_length;
			size_t header_length = 0;

			visible = true;
			if (!radio_name_bytes(session, recipient, to, sizeof(to),
			    &to_length, false, error)
			    || !radio_name_bytes(session, sender, from, sizeof(from),
			    &from_length, true, error)) {
				(void)yt_radio_file_close(&file, NULL);
				return false;
			}
			if (sender != previous_sender
			    || recipient != previous_recipient) {
				if (!yt_radio_reader_header(to, to_length, from,
				    from_length, header, sizeof(header), &header_length)) {
					if (error != NULL) {
						error->status = YT_RANGE;
						(void)snprintf(error->operation,
						    sizeof(error->operation), "%s",
						    "radio pair header capacity");
					}
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "radio pair blank", error)
				    || !session_present_text(session, header,
				    header_length, SESSION_PRESENT_LINE,
				    "radio pair header", error)) {
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
				yt_radio_pager_add_pair(&private_pager);
			}
			if (!session_present_text(session, record.bytes + 12, 74,
			    SESSION_PRESENT_LINE, "radio body", error)) {
				(void)yt_radio_file_close(&file, NULL);
				return false;
			}
			previous_sender = sender;
			previous_recipient = recipient;
			if (yt_radio_pager_add_body(&private_pager)) {
				if (!session_present_text(session,
				    (const uint8_t *)"[Pause]", strlen("[Pause]"),
				    SESSION_PRESENT_RAW, "radio pause", error)) {
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
				if (!session_wait(session, 99.0,
				    "radio private-pager wait", error)) {
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "radio pause blank", error)) {
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
			}
			if (decision.automatic_write) {
				if (!yt_radio_reader_mutate(&record, counter)
				    || !yt_radio_file_put(&file, basic_record, &record,
				    error)) {
					(void)yt_radio_file_close(&file, NULL);
					return false;
				}
			}
		}
	}
	if (!visible && !session_present_text(session,
	    (const uint8_t *)"None Found.", strlen("None Found."),
	    SESSION_PRESENT_LINE, "radio none found", error)) {
		(void)yt_radio_file_close(&file, NULL);
		return false;
	}
	if (!yt_radio_file_close(&file, error))
		return false;
	return true;
}

static bool
post_login(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] = "[ Press any Key ]";
	char real_name[258];
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (!yt_game_post_login_repairs(&session->door->game,
	    session->player_record, session->door->game.config.maximum_holds,
	    &session->player, NULL, error))
		return false;
	(void)snprintf(real_name, sizeof(real_name), "%s %s",
	    session->door->identity.real_first,
	    session->door->identity.real_last);
	status = yt_present_status_row((const uint8_t *)real_name,
	    strlen(real_name), (const uint8_t *)session->player.name,
	    strlen(session->player.name), &session->presentation,
	    &presentation);
	if (status != YT_PRESENT_OK) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "post-login status row");
		}
		return false;
	}
	yt_out_present_result(&presentation);
	if (!show_ship(session, error))
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login Info trailing blank", error))
		return false;
	if (!session_031f(session, prompt, sizeof(prompt) - 1U,
	    "post-login low-time warning", error)) {
		if (error != NULL && error->status == YT_OK) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "post-login press prompt");
		}
		return false;
	}
	if (!session_timed_wait(session, 99.0)) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "post-login press wait");
		}
		return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "post-login press trailing blank", error))
		return false;
	return radio_read(session, 0.0f, error);
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

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
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

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
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

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
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

	return yt_database_write_durable(&session->door->game.database,
	    (size_t)physical_record, &port->record, error);
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
	state.sector_record_offset = session->door->game.config.sector_offset;
	if (sector_record_expression != NULL) {
		state.sector_record_expression = *sector_record_expression;
		state.sector_record_supplied = true;
	}
	state.port_offset = session->door->game.config.port_offset;
	memcpy(state.base_price, session->market_base,
	    sizeof(state.base_price));
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

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
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
	    session->door->game.config.planet_offset);
	expression = single_add(session->door->game.config.planet_offset,
	    logical);
	if (qb_brun_random_record_number(expression) != physical_record
	    || qb_mbf32_encode(logical, state.logical_planet_raw)
	    == QB_MBF_OVERFLOW
	    || qb_mbf32_encode(session->door->game.config.planet_offset,
	    state.planet_offset_raw) == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet updater physical record");
		}
		return false;
	}
	if (!yt_planet_updater_run(&state, &ops, session, error))
		return false;
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
	int physical = yt_planet_basic_record(&session->door->game.config,
	    logical_planet);

	if (physical < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "planet update physical record");
		}
		return false;
	}
	return planet_update_cached_physical(session, (uint32_t)physical,
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
	    (float)session->player_record,
	    session->door->game.config.sector_offset,
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
	    session->player_record, &route, &owner, error))
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
	float expression = single_add(session->door->game.config.sector_offset,
	    logical_sector);
	uint32_t physical = qb_brun_random_record_number(expression);

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
	float expression = single_add(session->door->game.config.port_offset,
	    logical_port);
	uint32_t physical = qb_brun_random_record_number(expression);

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
	return yt_game_read_player(&session->door->game, session->player_record,
	    &session->player, error);
}

static bool
scanner_read_team_overlay(struct yt_session *session, float team,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct yt_record record;
	float expression = single_add(session->door->game.config.sector_offset,
	    team);
	uint32_t physical = qb_brun_random_record_number(expression);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &record, error))
		return false;
	yt_sector_decode(overlay, &record);
	return true;
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

	session->current_sector_record = logical_sector;
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
	if ((logical_sector == session->black_hole[0]
	    || logical_sector == session->black_hole[1])
	    && !session_attention(session,
	    "** Space-time disruption detected! **",
	    "sector disruption attention", error))
		return false;
	if (logical_sector == session->black_hole[0]
	    || logical_sector == session->black_hole[1])
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
		float expression = single_add(
		    session->door->game.config.planet_offset, sector.planet);
		uint32_t physical_planet =
		    qb_brun_random_record_number(expression);
		float saved_foreground;
		int saved_pager_foreground;

		if (!planet_update_cached_physical(session, physical_planet,
		    &planet, NULL, error)
		    || !scanner_read_planet(session, physical_planet, &planet,
		    error)
		    || !yt_sector_planet_row(&planet, row, sizeof(row),
		    &row_length, error))
			return false;
		saved_foreground = session->presentation.foreground;
		saved_pager_foreground = session->pager.foreground;
		session->presentation.foreground = 3.0f;
		session->pager.foreground = 3;
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "sector planet row", error))
			return false;
		session->presentation.foreground = saved_foreground;
		session->pager.foreground = saved_pager_foreground;
		yt_sector_pager_add(private_pager, 1.0f);
		if (!scanner_read_sector(session, logical_sector, &sector, error))
			return false;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		float random_value;

		if (!yt_sector_candidate_eligible(basic, session->player_record,
		    session->sector_cache[basic], logical_sector))
			continue;
		if (!yt_random_next(&session->door->game.random, &random_value,
		    error))
			return false;
		if (yt_sector_cloak_revealed(random_value,
		    session->cloak_cache[basic])) {
			static const uint8_t shimmer[] =
			    "You detect the shimmering of a cloaking device!";

			if (!session_present_text(session, shimmer,
			    sizeof(shimmer) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "sector cloak shimmer row", error))
				return false;
			yt_sector_pager_add(private_pager, 1.0f);
			session->cloak_cache[basic] = 0.0f;
			if (!session_sound(session, 4.0f,
			    "sector cloak-reveal sound", error))
				return false;
		}
		if (session->cloak_cache[basic] == 0.0f) {
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
		    && sector.fighter_owner != (float)session->player_record) {
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
		if (!yt_sector_fighter_row(&sector, session->player_record,
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
		session->pager.foreground = 7;
		session->presentation.foreground = 7.0f;
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
		session->pager.foreground = 1;
		session->presentation.foreground = 1.0f;
	}
	return true;
}

static bool
display_sector(struct yt_session *session, bool adjacent,
    struct yt_error *error)
{
	float current;
	struct yt_sector_pager_state private_pager;
	float caller_warps[YT_ARRAY_LEN(session->current_warps)];
	float targets[YT_ARRAY_LEN(session->current_warps)];
	float saved_foreground = session->presentation.foreground;
	int saved_pager_foreground = session->pager.foreground;
	size_t target_count;
	size_t slot;

	yt_sector_pager_begin(&private_pager);
	if (!adjacent) {
		session->presentation.foreground = 1.0f;
		session->pager.foreground = 1;
		if (!scanner_read_current_player(session, error))
			return false;
		current = session->player.sector;
		if (!display_sector_one(session, current, &private_pager, error)
		    || !scanner_read_current_player(session, error))
			return false;
		session->presentation.foreground = saved_foreground;
		session->pager.foreground = saved_pager_foreground;
		return true;
	}
	memcpy(caller_warps, session->current_warps, sizeof(caller_warps));
	target_count = yt_sector_sensor_targets(caller_warps, targets);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor leading blank", error))
		return false;
	session->presentation.foreground = 7.0f;
	session->pager.foreground = 7;
	if (!session_present_text(session,
	    (const uint8_t *)"[ Sensors Activated ]",
	    strlen("[ Sensors Activated ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor heading", error))
		return false;
	if (!session_sound(session, 4.0f,
	    "adjacent-sector sensor sound", error))
		return false;
	session->presentation.foreground = 1.0f;
	session->pager.foreground = 1;
	for (slot = 0; slot < target_count; ++slot) {
		if (!display_sector_one(session, targets[slot],
		    &private_pager, error))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "adjacent-sector sensor ending blank", error))
		return false;
	session->presentation.foreground = 7.0f;
	session->pager.foreground = 7;
	if (!session_present_text(session,
	    (const uint8_t *)"[ End Sensor Scan ]",
	    strlen("[ End Sensor Scan ]"), SESSION_PRESENT_BOLD_LINE,
	    "adjacent-sector sensor ending", error)
	    || !scanner_read_current_player(session, error))
		return false;
	session->presentation.foreground = saved_foreground;
	session->pager.foreground = saved_pager_foreground;
	return true;
}

static bool
display_current_sector_cached(struct yt_session *session,
    struct yt_error *error)
{
	struct yt_sector_pager_state private_pager;
	float saved_foreground = session->presentation.foreground;
	int saved_pager_foreground = session->pager.foreground;
	float current = session->player.sector;
	bool ok;

	yt_sector_pager_begin(&private_pager);
	session->presentation.foreground = 1.0f;
	session->pager.foreground = 1;
	ok = display_sector_one(session, current, &private_pager, error)
	    && scanner_read_current_player(session, error);
	if (ok) {
		session->presentation.foreground = saved_foreground;
		session->pager.foreground = saved_pager_foreground;
	}
	return ok;
}

static bool
danger_first_warning(struct yt_session *session, float target,
    bool already_visible, struct yt_error *error)
{
	char number[40];
	char row[160];
	int length;

	if (already_visible)
		return true;
	if (!session_sound(session, 8.0f, "danger warning sound", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "danger leading blank", error))
		return false;
	session->presentation.blink = 1.0f;
	if (!session_present_text(session,
	    (const uint8_t *)"*** WARNING! ***", strlen("*** WARNING! ***"),
	    SESSION_PRESENT_BOLD_RAW, "danger warning header", error))
		return false;
	qb_str_single(number, sizeof(number), target);
	length = snprintf(row, sizeof(row),
	    "%s Danger Scanner has detected the following in sector!", number);
	if (length < 0 || (size_t)length >= sizeof(row)
	    || !session_present_text(session, (const uint8_t *)row,
	    (size_t)length, SESSION_PRESENT_BOLD_LINE,
	    "danger warning target", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "danger warning blank", error))
		return false;
	return true;
}

static bool
dangerous_destination(struct yt_session *session, float target,
    bool *danger, struct yt_error *error)
{
	struct yt_sector sector;
	float saved_foreground;
	int saved_session_foreground;
	char number[80];
	uint8_t row[256];
	size_t row_length;
	bool overflow;

	*danger = false;
	if (target < 1.0f || target > (float)sector_count(session))
		return true;
	saved_foreground = session->presentation.foreground;
	saved_session_foreground = session->pager.foreground;
	session->presentation.foreground = 3.0f;
	session->presentation.background = 4.0f;
	session->pager.foreground = 3;
	if (!yt_game_read_sector(&session->door->game, (int)target, &sector,
	    error))
		return false;

	if (target == session->black_hole[0]
	    || target == session->black_hole[1]) {
		if (!danger_first_warning(session, target, *danger, error))
			return false;
		if (!session_present_text(session,
		    (const uint8_t *)"** Space-time disruption! **",
		    strlen("** Space-time disruption! **"),
		    SESSION_PRESENT_BOLD_LINE, "danger disruption row", error))
			return false;
		*danger = true;
	}
	if (sector.mines != 0.0f) {
		if (!danger_first_warning(session, target, *danger, error))
			return false;
		qb_str_single(number, sizeof(number), sector.mines);
		row_length = (size_t)snprintf((char *)row, sizeof(row),
		    "**%s SECTOR MINES! **", number);
		if (row_length >= sizeof(row)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "danger mines row", error))
			return false;
		*danger = true;
	}
	if (sector.fighters > 0.0f) {
		float owner = sector.fighter_owner;
		bool hostile;

		qb_str_double(number, sizeof(number), (double)sector.fighters);
		row_length = (size_t)snprintf((char *)row, sizeof(row),
		    "***%s Fighters Belonging to ", number);
		if (row_length >= sizeof(row))
			return false;
		if (owner == -1.0f) {
			memcpy(row + row_length, "The Xannor",
			    strlen("The Xannor"));
			row_length += strlen("The Xannor");
		}
		else if (owner == -2.0f) {
			memcpy(row + row_length, "Mercenaries",
			    strlen("Mercenaries"));
			row_length += strlen("Mercenaries");
		}
		else {
			struct yt_player owner_player;
			int owner_record = (int)qb_cint((double)owner, &overflow);
			int name_length;

			if (overflow) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation),
					    "danger owner record");
				}
				return false;
			}
			if (!yt_game_read_player(&session->door->game, owner_record,
			    &owner_player, error))
				return false;
			name_length = (int)qb_cint((double)owner_player.name_length,
			    &overflow);
			if (overflow || name_length < 0) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation),
					    "danger owner name length");
				}
				return false;
			}
			if ((size_t)name_length > YT_TEXT_FIELD_SIZE)
				name_length = (int)YT_TEXT_FIELD_SIZE;
			if ((size_t)name_length > sizeof(row) - row_length)
				return false;
			memcpy(row + row_length, owner_player.record.bytes,
			    (size_t)name_length);
			row_length += (size_t)name_length;
			if (owner_player.team != 0.0f) {
				struct yt_player current;
				struct yt_player candidate;
				struct yt_sector team_overlay;
				int team_record;
				int team_name_length;

				session->relationship_scratch = 0.0f;
				if (owner >= 2.0f
				    && owner <= session->door->game.config.sector_offset
				    && session->player_record >= YT_PLAYER_FIRST
				    && (float)session->player_record
				    <= session->door->game.config.sector_offset) {
					if (owner == (float)session->player_record)
						session->relationship_scratch = -1.0f;
					else {
						if (!yt_game_read_player(
						    &session->door->game,
						    session->player_record, &current,
						    error))
							return false;
						if (current.team != 0.0f) {
							if (!yt_game_read_player(
							    &session->door->game,
							    owner_record, &candidate,
							    error))
								return false;
							if (candidate.team == current.team)
								session->relationship_scratch =
								    -1.0f;
						}
					}
				}
				qb_str_single(number, sizeof(number),
				    owner_player.team);
				{
					int amount = snprintf((char *)row + row_length,
					    sizeof(row) - row_length, " * Team [%s]",
					    number[0] == '\0' ? number : number + 1);

					if (amount < 0 || (size_t)amount
					    >= sizeof(row) - row_length)
						return false;
					row_length += (size_t)amount;
				}
				team_record = (int)qb_cint((double)owner_player.team,
				    &overflow);
				if (overflow
				    || !yt_game_read_sector(&session->door->game,
				    team_record, &team_overlay, error))
					return false;
				team_name_length = (int)qb_cint((double)
				    yt_record_get_number(&team_overlay.record, YT_F73),
				    &overflow);
				if (overflow || team_name_length < 0) {
					if (error != NULL) {
						error->status = YT_RANGE;
						snprintf(error->operation,
						    sizeof(error->operation),
						    "danger team name length");
					}
					return false;
				}
				if (team_name_length > 0) {
					size_t amount = (size_t)team_name_length;

					if (amount > YT_TEXT_FIELD_SIZE)
						amount = YT_TEXT_FIELD_SIZE;
					if (amount + 3U > sizeof(row) - row_length)
						return false;
					row[row_length++] = ' ';
					row[row_length++] = '[';
					memcpy(row + row_length,
					    team_overlay.record.bytes, amount);
					row_length += amount;
					row[row_length++] = ']';
				}
			}
		}
		hostile = owner < 0.0f
		    || (owner > 1.0f
		    && owner <= session->door->game.config.sector_offset
		    && owner != (float)session->player_record
		    && session->relationship_scratch == 0.0f);
		if (hostile) {
			if (!danger_first_warning(session, target, *danger, error))
				return false;
			*danger = true;
			if (!session_present_text(session, row, row_length,
			    SESSION_PRESENT_BOLD_LINE, "danger fighters row", error))
				return false;
		}
	}
	if (!reload_player(session, error))
		return false;
	if (*danger) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "danger final blank", error))
			return false;
		session->presentation.blink = 1.0f;
		if (!session_present_text(session,
		    (const uint8_t *)"*** WARP DRIVE DEACTIVATED ***",
		    strlen("*** WARP DRIVE DEACTIVATED ***"),
		    SESSION_PRESENT_BOLD_LINE, "danger deactivation row", error))
			return false;
	}
	session->presentation.foreground = saved_foreground;
	session->presentation.background = 0.0f;
	session->pager.foreground = saved_session_foreground;
	return true;
}

static bool
spy_read_sector(void *context, int logical_sector, struct yt_sector *sector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, logical_sector,
	    sector, error);
}

static bool
spy_update_planet(void *context, float link, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_planet planet;
	uint32_t physical = qb_brun_random_record_number(single_add(
	    session->door->game.config.planet_offset, link));

	return planet_update_cached_physical(session, physical, &planet, NULL,
	    error);
}

static bool
spy_read_planet(void *context, float link, struct yt_planet *planet,
    struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = qb_brun_random_record_number(single_add(
	    session->door->game.config.planet_offset, link));

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
	uint32_t physical = qb_brun_random_record_number(single_add(
	    session->door->game.config.sector_offset, team));

	if (!yt_database_read(&session->door->game.database, physical, &raw,
	    error))
		return false;
	yt_sector_decode(overlay, &raw);
	return true;
}

static bool
spy_random(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
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
	session->presentation.foreground = state->foreground;
	session->presentation.background = state->background;
	session->presentation.bold = state->bold;
	session->presentation.blink = state->blink;
	session->pager.foreground = (int)state->foreground;
}

static void
spy_export_presentation(struct yt_spy_sweep_state *state,
    const struct yt_session *session)
{
	state->foreground = session->presentation.foreground;
	state->background = session->presentation.background;
	state->bold = session->presentation.bold;
	state->blink = session->presentation.blink;
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
		spy_random,
		spy_sound,
		spy_present,
		spy_pause,
	};
	struct yt_spy_sweep_state state = {
		.active_spies = (float)session->spy_count,
		.spy_sectors = session->spies,
		.last_reported_sectors = session->spy_marker,
		.spy_capacity = YT_ARRAY_LEN(session->spies),
		.current_player_record = session->player_record,
		.last_player_record = session->door->game.config.sector_offset,
		.disruption_sectors = {
			session->black_hole[0], session->black_hole[1]
		},
		.sector_cache = session->sector_cache,
		.cloak_cache = session->cloak_cache,
		.cache_count = YT_ARRAY_LEN(session->sector_cache),
		.found_scratch = session->spy_found_scratch,
		.dead_counter_scratch = session->spy_dead_counter_scratch,
		.warp_destination_scratch =
		    session->spy_warp_destination_scratch,
		.foreground = session->presentation.foreground,
		.background = session->presentation.background,
		.bold = session->presentation.bold,
		.blink = session->presentation.blink,
	};
	bool result = yt_spy_sweep_run(&state, &ops, session, error);

	session->spy_found_scratch = state.found_scratch;
	session->spy_dead_counter_scratch = state.dead_counter_scratch;
	session->spy_warp_destination_scratch = state.warp_destination_scratch;
	spy_import_presentation(session, &state);
	return result;
}

static bool
fresh_no_turn_gate(struct yt_session *session, bool *denied,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Sorry but you have no turns left.";

	if (!reload_player(session, error))
		return false;
	session->sector_cache[session->player_record] = session->player.sector;
	if (!session->anti_cloak)
		 session->cloak_cache[session->player_record] = session->player.cloak;
	*denied = yt_no_turn_gate_denied(session->player.turns);
	if (*denied)
		return session_02db(session, notice, sizeof(notice) - 1U,
		    "no-turn gate notice", error);
	return true;
}

static bool
finalize_action(struct yt_session *session, float amount,
    struct yt_error *error)
{
	int xannor_provoker = 0;
	float quotient;
	float draw;
	char number[64];
	char row[128];

	(void)amount;
	if (!spy_sweep(session, error) || !reload_player(session, error))
		return false;
	session->player.turns = single_sub(session->player.turns, 1.0f);
	if (!yt_record_set_number(&session->player.record, YT_F49,
	    session->player.turns))
		return false;
	quotient = single_div(session->player.turns, 25.0f);
	if (!session->anti_cloak && quotient == floorf(quotient)) {
		static const uint8_t dirty_zero[4] = {0x00, 0x00, 0xa3, 0x00};
		float display;
		float saved_foreground = session->presentation.foreground;
		int saved_pager_foreground = session->pager.foreground;

		session->player.cloak = single_add(session->player.cloak,
		    -0.009999999776482582f);
		if (session->player.cloak < 0.0f) {
			session->player.cloak = 0.0f;
			if (!yt_record_set_raw_number(&session->player.record,
			    YT_F125, dirty_zero))
				return false;
		}
		else if (!yt_record_set_number(&session->player.record, YT_F125,
		    session->player.cloak))
			return false;
		session->cloak_cache[session->player_record] =
		    session->player.cloak;
		display = floorf(single_mul(session->player.cloak, 50.0f));
		qb_str_single(number, sizeof(number), display);
		snprintf(row, sizeof(row), "Cloak at%s%%", number);
		session->presentation.foreground = 7.0f;
		session->pager.foreground = 7;
		if (!session_031f(session, (const uint8_t *)row, strlen(row),
		    "action-finalizer cloak row", error))
			return false;
		session->presentation.foreground = saved_foreground;
		session->pager.foreground = saved_pager_foreground;
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
	    (size_t)session->player_record, &session->player.record, error))
		return false;
	qb_str_single(number, sizeof(number), session->player.turns);
	snprintf(row, sizeof(row), "One Turn Deducted,%s left.", number);
	if (session->player.turns < 51.0f) {
		session->presentation.foreground = 3.0f;
		session->pager.foreground = 3;
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
	}
	if (!session_02fc(session, (const uint8_t *)row, strlen(row)))
		return false;
	if (!random_value(session, &draw, error))
		return false;
	if (draw > 0.99000000953674316f) {
		if (!launch_xannor_retaliation(session, &xannor_provoker,
		    error))
			return false;
		if (session->destroyed)
			return false;
		if (!reload_player(session, error))
			return false;
	}
	return true;
}

static bool
random_value(struct yt_session *session, float *value,
    struct yt_error *error)
{
	return yt_random_next(&session->door->game.random, value, error);
}

static bool
session_random_one_based(void *context, float range,
    float *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_random_one_based_single(&session->door->game.random, range,
	    value, error);
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
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (!session_present_text(session, temperature,
	    sizeof(temperature) - 1U, SESSION_PRESENT_BOLD_LINE,
	    "emergency warp temperature title", error)
	    || !session_present_text(session, scale, sizeof(scale) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature scale", error))
		return false;
	session->presentation.foreground = 2.0f;
	session->pager.foreground = 2;
	if (!session_present_text(session, ruler, sizeof(ruler) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "emergency warp temperature ruler", error))
		return false;
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
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
			session->presentation.foreground = 2.0f;
			session->pager.foreground = 2;
		}
		else if (heat < 20.0f) {
			session->presentation.foreground = 3.0f;
			session->pager.foreground = 3;
		}
		else {
			session->presentation.foreground = 1.0f;
			session->presentation.blink = 1.0f;
			session->pager.foreground = 1;
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
	session->presentation.foreground = 2.0f;
	session->pager.foreground = 2;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank one", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp post-gauge blank two", error)
	    || !reload_player(session, error))
		return false;
	if (!random_value(session, &first, error)
	    || !random_value(session, &override, error)
	    || !random_value(session, &turn_draw, error))
		return false;
	destination = yt_emergency_warp_destination(first,
	    (float)sector_count(session));
	if (override > 0.949999988079071f)
		destination = session->door->game.config.headquarters;
	cost = yt_emergency_warp_cost(heat, turn_draw, session->player.turns,
	    heat >= 31.0f);
	if (heat >= 31.0f) {
		if (!session_attention(session, "MELT DOWN!",
		    "meltdown attention", error))
			return false;
		session->presentation.foreground = 1.0f;
		session->pager.foreground = 1;
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
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->sector_cache[session->player_record] = destination;
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
	session->presentation.bold = 1.0f;
	session->presentation.foreground = 7.0f;
	session->pager.foreground = 7;
	if (!session_02fc(session, warning_one, sizeof(warning_one) - 1U))
		return false;
	session->presentation.bold = 1.0f;
	if (!session_02fc(session, warning_two, sizeof(warning_two) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp confirmation blank", error))
		return false;
	session->presentation.bold = 1.0f;
	if (!session_a8d2(session, prompt, sizeof(prompt) - 1U, &answer, error))
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

	if (player_record != session->player_record || player == NULL
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
		return session_0317(session, text, length, "movement warp row",
		    error);
	case YT_MOVEMENT_POST_WARP_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "movement post-warp blank", error);
	case YT_MOVEMENT_DESTINATION_PROMPT:
		return session_031f(session, text, length,
		    "movement destination prompt", error);
	case YT_MOVEMENT_SAME_SECTOR:
		return session_02db(session, text, length,
		    "movement same-sector row", error);
	case YT_MOVEMENT_NOT_ADJACENT:
		return session_02db(session, text, length,
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
	return session_036f(context, response, capacity);
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
	    || !session_a8d2(context, prompt, length, &answer, error))
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

	session->suppress_self_mines = false;
}

static bool
movement_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
movement_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record)
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
movement_update_cache(void *context, int player_record, float target,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	if (player_record < 0
	    || (size_t)player_record >= YT_ARRAY_LEN(session->sector_cache))
		return false;
	session->sector_cache[player_record] = target;
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
		.current_player_record = session->player_record,
		.port_offset = session->door->game.config.port_offset,
		.sector_offset = session->door->game.config.sector_offset,
	};
	memcpy(state.warps, session->current_warps, sizeof(state.warps));
	if (!yt_movement_run(&state, &ops, session, error))
		return false;
	*moved = state.route == YT_MOVEMENT_MOVED;
	return true;
}

static bool
death_team_read_player(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
death_team_write_player(void *context, int player_record,
	struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_player(&session->door->game, player_record, player,
	    error);
}

static bool
session_read_physical_record(void *context, uint32_t physical_record,
	struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_read(&session->door->game.database,
	    (size_t)physical_record, record, error);
}

static bool
death_team_write_record(void *context, uint32_t physical_record,
	const struct yt_record *record, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)physical_record, record, error);
}

static bool
team_remove_player(struct yt_session *session, int victim,
    struct yt_error *error)
{
	static const struct yt_death_team_remove_ops ops = {
		death_team_read_player,
		death_team_write_player,
		session_read_physical_record,
		death_team_write_record,
	};
	struct yt_death_team_remove_state state = {
		.victim_record = victim,
		.current_player_record = (float)session->player_record,
		.sector_record_offset = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cache = &session->team_cache,
	};

	return yt_death_team_remove_run(&state, &ops, session, error);
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

	return yt_game_read_sector(&session->door->game, logical_sector, sector,
	    error);
}

static bool
player_death_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_sector(&session->door->game, logical_sector, sector,
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

	return yt_game_read_port(&session->door->game, logical_port, port, error);
}

static bool
player_death_write_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_port(&session->door->game, logical_port, port,
	    error);
}

static bool
player_death_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "death title row", error);
}

static bool
player_death_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static void
player_death_clear_active_cache(void *context, int victim_record)
{
	struct yt_session *session = context;

	session->sector_cache[victim_record] = 0.0f;
}

static void
player_death_set_current(void *context, const struct yt_player *player)
{
	struct yt_session *session = context;
	char cached_name[sizeof(session->player.name)];

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	session->player = *player;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
	session->destroyed = true;
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
		player_death_news,
		player_death_set_current,
		player_death_flush,
	};
	struct yt_player_death_state state = {
		.victim_record = victim_record,
		.current_player_record = session->player_record,
		.killer = killer,
		.sector_count = sector_count(session),
		.port_count = port_count(session),
		.last_player_record = session->door->game.config.sector_offset,
		.current_name = (const uint8_t *)session->player.name,
		.current_name_length = strlen(session->player.name),
	};

	if (!yt_player_death_run(&state, &ops, session, error))
		return false;
	if (victim_record == session->player_record && wait_for_current) {
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

	session->presentation.foreground = foreground;
	session->pager.foreground = pager_foreground;
}

static bool
common_fatal_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_02db(context, text, length, "common fatal notice", error);
}

static bool
common_fatal_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	char cached_name[sizeof(session->player.name)];

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
	*player = session->player;
	return true;
}

static bool
common_fatal_sound(void *context, struct yt_error *error)
{
	return session_sound(context, 3.0f, "fatal destruction sound", error);
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

	if (!session_wait(session, (double)duration, "common fatal wait", error))
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
		.current_player_record = session->player_record,
		.foreground = session->presentation.foreground,
		.pager_foreground = session->pager.foreground,
	};

	return yt_common_fatal_run(&state, &ops, session, error);
}

static bool
salvage_read_victim(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
salvage_read_killer(void *context, float player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = qb_brun_random_record_number(player_record);
	struct yt_record raw;

	if (physical == (uint32_t)session->player_record) {
		if (!reload_player(session, error))
			return false;
		*player = session->player;
		return true;
	}
	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &raw, error))
		return false;
	yt_player_decode(player, &raw);
	return true;
}

static bool
salvage_write_killer(void *context, float player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = qb_brun_random_record_number(player_record);

	yt_player_encode(player);
	if (!yt_database_write(&session->door->game.database, (size_t)physical,
	    &player->record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	if (physical == (uint32_t)session->player_record)
		session->player = *player;
	return true;
}

static bool
salvage_random(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
}

static bool
salvage_wait(void *context, float duration, struct yt_error *error)
{
	return session_wait(context, (double)duration, "ship salvage wait",
	    error);
}

static bool
salvage_present(void *context, const uint8_t *text, size_t length,
    bool bold, struct yt_error *error)
{
	return session_present_text(context, text, length,
	    bold ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    bold ? "salvage title" : "salvage result row", error);
}

static bool
salvage_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
salvage_player(struct yt_session *session, int victim_record, float killer,
    struct yt_error *error)
{
	static const struct yt_salvage_ops ops = {
		salvage_read_victim,
		salvage_read_killer,
		salvage_write_killer,
		salvage_random,
		session_random_one_based,
		salvage_wait,
		salvage_present,
		salvage_news,
	};
	struct yt_salvage_state state = {
		.victim_record = victim_record,
		.killer_record = killer,
		.last_player_record = session->door->game.config.sector_offset,
		.maximum_holds = session->door->game.config.maximum_holds,
		.current_name = (const uint8_t *)session->player.name,
		.current_name_length = strlen(session->player.name),
	};

	return yt_salvage_run(&state, &ops, session, error);
}

static bool
direct_attack_attrition_draw(void *context, float *value,
    struct yt_error *error)
{
	return random_value(context, value, error);
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
	return session_wait(context, seconds, operation, error);
}

static void
xannor_victory_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session->presentation.foreground = foreground;
	session->pager.foreground = (int)foreground;
}

static void
xannor_victory_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	session->presentation.blink = blink;
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
xannor_victory_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
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

	return yt_game_read_sector(&session->door->game, logical_sector, sector,
	    error);
}

static bool
xannor_victory_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_sector(&session->door->game, logical_sector,
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
		xannor_victory_news,
		xannor_victory_radio,
		xannor_victory_read_sector,
		xannor_victory_write_sector,
	};
	struct yt_xannor_victory_state state = {
		.current_player = (float)session->player_record,
		.foreground = session->presentation.foreground,
		.pager_foreground = (float)session->pager.foreground,
		.blink = session->presentation.blink,
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
direct_fighter_kill_salvage(void *context, int victim_record, float killer,
    struct yt_error *error)
{
	return salvage_player(context, victim_record, killer, error);
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
	return yt_game_read_sector(&session->door->game, logical_sector, sector,
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
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    logical_sector), &sector->record, error);
}

static bool
direct_fighter_kill_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return session_02db(context, text, length,
	    "direct fighter mine warning", error);
}

static bool
direct_fighter_kill_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
direct_fighter_kill_mine(void *context, bool *terminal,
    uint8_t destroyed_raw[4], struct yt_error *error)
{
	struct yt_session *session = context;

	if (!mine_encounter(session, terminal, error))
		return false;
	return qb_mbf32_encode(session->destroyed ? -1.0f : 0.0f,
	    destroyed_raw) == QB_MBF_OK;
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

	if (player_record != session->player_record)
		return yt_game_read_player(&session->door->game, player_record,
		    player, error);
	if (!reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
direct_attack_combat_write(void *context, int player_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record == session->player_record)
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
		return session_02db(context, text, length,
		    "direct Attack too-many row", error);
	case YT_DIRECT_ATTACK_COMBAT_ATTACKER_ROW:
		return session_0317(context, text, length,
		    "direct Attack attacker result", error);
	case YT_DIRECT_ATTACK_COMBAT_DEFENDER_ROW:
		return session_02fc(context, text, length);
	case YT_DIRECT_ATTACK_COMBAT_ELIMINATED_ROW:
		return session_0317(context, text, length,
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
	return fighter_shield_spill(context, fighters, shields, error);
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
		direct_fighter_kill_news,
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
		direct_attack_attrition_draw,
		direct_attack_combat_spill,
		direct_attack_combat_kill,
	};
	struct yt_direct_attack_combat_state state = {
		.current_player_record = session->player_record,
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
		return session_02fc(context, text, length);
	case YT_DIRECT_ATTACK_NO_FIGHTERS_ROW:
		return session_02db(context, text, length,
		    "direct Attack no-fighters row", error);
	case YT_DIRECT_ATTACK_COMMITMENT_PROMPT:
		return session_031f(context, text, length,
		    "direct Attack commitment prompt", error);
	case YT_DIRECT_ATTACK_NONE_VISIBLE_ROW:
		return session_02db(context, text, length,
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

	if (!session_a8d2(context, prompt, length, &selected, error))
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
	return session_036f(context, response, capacity);
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
		direct_attack_present,
		direct_attack_confirm,
		direct_attack_amount,
		direct_attack_combat,
	};
	struct yt_direct_attack_state state = {
		.current_player_record = session->player_record,
		.last_player_record = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.sector_cache = session->sector_cache,
		.cloak_cache = session->cloak_cache,
		.cache_count = YT_ARRAY_LEN(session->sector_cache),
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

static bool
fighter_shield_spill(struct yt_session *session, double *fighters,
    float *shields, struct yt_error *error)
{
	static const struct yt_fighter_shield_spill_ops ops = {
		direct_attack_attrition_draw,
		fighter_shield_spill_present,
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
		return session_0317(context, text, length,
		    "surrender radio row", error);
	case YT_HOSTILE_SURRENDER_CAPTAIN_ROW:
		return session_0317(context, text, length,
		    "surrender captain row", error);
	case YT_HOSTILE_SURRENDER_WISH_ROW:
		return session_02db(context, text, length,
		    "surrender wish row", error);
	case YT_HOSTILE_SURRENDER_PROMPT_BLANK:
		return session_present_text(context, NULL, 0,
		    SESSION_PRESENT_LINE, "surrender prompt blank", error);
	case YT_HOSTILE_SURRENDER_JOINED_ROW:
		return session_0317(context, text, length,
		    "surrender joined row", error);
	case YT_HOSTILE_SURRENDER_COUNT_ROW:
		return session_02fc(context, text, length);
	case YT_HOSTILE_SURRENDER_XANNOR_REFUSAL_ROW:
		return session_02fc(context, text, length);
	case YT_HOSTILE_SURRENDER_MERCENARY_REFUSAL_ROW:
		return session_02fc(context, text, length);
	default:
		return false;
	}
}

static bool
hostile_surrender_sound(void *context, float selector,
    struct yt_error *error)
{
	return session_sound(context, selector, "hostile surrender sound", error);
}

static bool
hostile_surrender_prompt(void *context, const uint8_t *prompt, size_t length,
    enum yt_hostile_surrender_answer *answer, struct yt_error *error)
{
	enum yt_yes_no_answer selected;

	if (!session_a8d2(context, prompt, length, &selected, error))
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

static bool
hostile_surrender_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
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

	return yt_game_read_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
hostile_attack_persistence_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    sector_number), &sector->record, error);
}

static bool
hostile_attack_persistence_blank(void *context, struct yt_error *error)
{
	return session_present_text(context, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack post-persist blank", error);
}

static bool
hostile_attack_persistence_news(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
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
		session->presentation.bold = 1.0f;
	else if (kind != YT_HOSTILE_ATTACK_TAIL_DEFEATED_ROW)
		return false;
	(void)error;
	return session_02fc(session, text, length);
}

static bool
hostile_attack_tail_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	struct hostile_attack_tail_context *tail = context;

	return append_news_bytes(tail->session, text, length, error);
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

	return yt_game_read_sector(&combat->session->door->game, sector_number,
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

static bool
hostile_attack_combat_surrender(void *context,
    struct yt_hostile_surrender_state *state, struct yt_error *error)
{
	static const struct yt_hostile_surrender_ops ops = {
		hostile_surrender_read,
		hostile_surrender_present,
		hostile_surrender_sound,
		hostile_surrender_prompt,
		hostile_surrender_news,
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
		return session_02fc(session, text, length);
	case YT_HOSTILE_ATTACK_COMBAT_DESTROYED_ROW:
		return session_02fc(session, text, length);
	case YT_HOSTILE_ATTACK_COMBAT_EXPOSED_ROW:
		return session_02db(session, text, length,
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
    const struct yt_sector *sector)
{
	struct hostile_attack_combat_context *combat = context;

	*combat->sector = *sector;
}

static bool
hostile_attack_combat_spill(void *context, double *fighters,
    float *shields, struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	return fighter_shield_spill(combat->session, fighters, shields, error);
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
		hostile_attack_persistence_news,
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
		(void)qb_mbf32_encode(-1.0f,
		    combat->session->mercenaries_hurt_raw);
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
		.current_player_record = session->player_record,
		.current_sector = (int)session->player.sector,
		.commitment = commitment,
		.allow_surrender = allow_surrender,
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

	if (!session_02fc(session, heading, sizeof(heading) - 1U))
		return false;
	admission = yt_hostile_attack_admit(session->player.fighters, 0.0f);
	if (admission == YT_HOSTILE_ATTACK_NO_FIGHTERS)
		return session_02db(session, none, sizeof(none) - 1U,
		    "hostile Attack no fighters", error);
	if (!session_031f(session, prompt, sizeof(prompt) - 1U,
	    "hostile Attack amount prompt", error)
	    || !session_036f(session, response, sizeof(response)))
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
	session->attack_commitment = (float)parsed.value;
	status = qb_mbf32_encode(session->attack_commitment,
	    session->attack_commitment_raw);
	if (status == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "attack:amount-csng");
		}
		return false;
	}
	session->attack_commitment = qb_mbf32_decode(
	    session->attack_commitment_raw);
	admission = yt_hostile_attack_admit(session->player.fighters,
	    session->attack_commitment);
	if (admission == YT_HOSTILE_ATTACK_TOO_MANY) {
		if (qb_str_double(available, sizeof(available),
		    (double)session->player.fighters) < 0
		    || snprintf(row, sizeof(row), "You only have%s!", available) < 0)
			return false;
		return session_02db(session, (const uint8_t *)row, strlen(row),
		    "hostile Attack too many", error);
	}
	if (admission == YT_HOSTILE_ATTACK_LESS_THAN_ONE)
		return true;
	return attack_deployed_committed(session, sector,
	    (double)session->attack_commitment, true, error);
}

static bool
hostile_bribe_accept_present(void *context, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	return session_02db(context, text, length,
	    "accepted Mercenary Bribe", error);
}

static bool
hostile_bribe_accept_sound(void *context, float selector,
    struct yt_error *error)
{
	return session_sound(context, selector, "accepted bribe sound", error);
}

static bool
hostile_bribe_accept_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
hostile_bribe_accept_write_sector(void *context, int sector_number,
    const struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    sector_number), &sector->record, error);
}

static bool
hostile_bribe_accept_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
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
		return session_02db(bribe->session, text, length,
		    "ordinary Bribe refusal", error);
	case YT_HOSTILE_BRIBE_PLANET_REFUSAL_ROW:
		return session_02db(bribe->session, text, length,
		    "Mercenary planet refusal", error);
	case YT_HOSTILE_BRIBE_LIFE_DEMAND_ROW:
		return session_02db(bribe->session, text, length,
		    "Mercenary life demand", error);
	case YT_HOSTILE_BRIBE_INTRODUCTION_ROW:
		return session_0317(bribe->session, text, length,
		    "Mercenary Bribe introduction", error);
	case YT_HOSTILE_BRIBE_OFFER_PROMPT:
		return session_031f(bribe->session, text, length,
		    "Mercenary Bribe offer prompt", error);
	case YT_HOSTILE_BRIBE_REJECTED_ROW:
		return session_02db(bribe->session, text, length,
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
	return session_036f(bribe->session, response, capacity);
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
		.current_player_record = session->player_record,
		.current_sector = (int)session->player.sector,
		.owner = sector->fighter_owner,
		.cached_defenders = sector->fighters,
		.ship_fighters = (double)session->player.fighters,
		.shields = session->player.shields,
		.credits = (double)session->player.credits,
		.real_first_name =
		    (const uint8_t *)session->door->identity.real_first,
		.real_first_name_length =
		    strlen(session->door->identity.real_first),
	};
	memcpy(state.planet_link_raw, sector->record.bytes + YT_F93,
	    sizeof(state.planet_link_raw));
	memcpy(state.mercenaries_hurt_raw, session->mercenaries_hurt_raw,
	    sizeof(state.mercenaries_hurt_raw));
	result = yt_hostile_bribe_run(&state, &ops, &context, error);
	if (state.commitment_stored) {
		session->attack_commitment = state.commitment;
		memcpy(session->attack_commitment_raw, state.commitment_raw,
		    sizeof(session->attack_commitment_raw));
	}
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

	if (!reload_player(session, error))
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

	return yt_game_read_sector(&session->door->game, logical_sector, sector,
	    error);
}

static bool
mine_write_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    logical_sector), &sector->record, error);
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
mine_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
mine_random(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
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

	session->presentation.foreground = foreground;
	session->presentation.background = background;
	session->presentation.blink = blink;
	session->pager.foreground = pager_foreground;
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
		mine_news,
		mine_random,
		mine_shrink,
		mine_warp,
		mine_set_current,
		mine_style,
	};
	struct yt_sector_mine_state state = {
		.current_player_record = session->player_record,
		.current_sector = session->player.sector,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.foreground = session->presentation.foreground,
		.background = session->presentation.background,
		.blink = session->presentation.blink,
		.pager_foreground = session->pager.foreground,
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

	if (!session_0317(session, heading, sizeof(heading) - 1U,
	    "hostile help heading", error)
	    || !session_0317(session, attack, sizeof(attack) - 1U,
	    "hostile help attack row", error))
		return false;
	for (index = 0; index < YT_ARRAY_LEN(rows); ++index) {
		if (!session_02fc(session, (const uint8_t *)rows[index],
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
	session->presentation.foreground = 7.0f;
	session->pager.foreground = 7;
	if (!session_02fc(session, heading, sizeof(heading) - 1U))
		return false;
	for (;;) {
		char response[80];
		enum yt_yes_no_answer answer;

		if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "hostile quit prompt", error)
		    || !session_0357(session, response, sizeof(response)))
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
		session->presentation.bold = 1.0f;
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

		if (!display_sector(session, false, error)
		    || !reload_player(session, error))
			return false;
		if (yt_sector_is_black_hole(session->player.sector,
		    session->black_hole[0], session->black_hole[1])) {
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
		if (!yt_game_read_sector(&session->door->game,
		    (int)session->player.sector, &sector, error))
			return false;
		if (yt_sector_mines_admitted(sector.mines,
		    session->suppress_self_mines ? -1.0f : 0.0f)) {
			{
				bool mine_terminal;

				if (!mine_encounter(session, &mine_terminal, error))
					return false;
				if (mine_terminal)
					continue;
			}
			if (session->destroyed)
				return common_fatal_self(session, error);
			continue;
		}
		friendly = sector_force_friendly(session, &sector, error);
		if (!friendly && error != NULL && error->status != YT_OK)
			return false;
		if (sector.fighters == 0.0f || friendly)
			return true;
		if (!session_02db(session, hostile_warning,
		    sizeof(hostile_warning) - 1U,
		    "hostile entry warning", error))
			return false;
		for (;;) {
			uint8_t row[160];
			size_t row_length;
			bool fresh_menu = true;

			if (!reload_player(session, error))
				return false;
			session->presentation.foreground = 3.0f;
			session->pager.foreground = 3;
			if (!yt_hostile_menu_row((double)session->player.fighters,
			    (double)sector.fighters, row, sizeof(row), &row_length)) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "hostile fighter row");
				}
				return false;
			}
			if (!session_0317(session, row, row_length,
			    "hostile fighter row", error))
				return false;
			while (fresh_menu) {
				char response[80];
				enum yt_hostile_menu_route route;

				if (!session_031f(session, hostile_prompt,
				    sizeof(hostile_prompt) - 1U,
				    "hostile option prompt", error)
				    || !session_0357(session, response,
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
					if (!session_02db(session,
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
					if (session->destroyed)
						return true;
					if (sector.fighters <= 0.0f) {
						session->presentation.foreground = 1.0f;
						session->pager.foreground = 1;
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
					if (session->destroyed)
						return true;
					if (direct_hostile_menu) {
						fresh_menu = false;
						break;
					}
					if (forced_attack) {
						if (sector.fighters <= 0.0f) {
							session->presentation.foreground = 1.0f;
							session->pager.foreground = 1;
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

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
main_fighters_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
main_fighters_write_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_sector(&session->door->game, sector_number, sector,
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
		return session_02fc(session, text, length);
	case YT_MAIN_FIGHTERS_UNION_REFUSAL:
		return session_02db(session, text, length,
		    "fighter Union refusal", error);
	case YT_MAIN_FIGHTERS_FOREIGN_REFUSAL:
		return session_02db(session, text, length,
		    "fighter foreign-force refusal", error);
	case YT_MAIN_FIGHTERS_PROMPT:
		return session_031f(session, text, length,
		    "fighter desired-count prompt", error);
	case YT_MAIN_FIGHTERS_INSUFFICIENT:
		return session_02db(session, text, length,
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
	return session_036f(context, response, capacity);
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
		.current_player_record = session->player_record,
	};

	return yt_main_fighters_run(&state, &ops, session, error);
}

static bool
drop_mines_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
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

	return yt_game_read_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
drop_mines_write_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
drop_mines_present(void *context, const uint8_t *text, size_t length,
    enum yt_drop_mines_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_DROP_MINES_NO_MINES_ROW:
		return session_02db(session, text, length, "no sector mines",
		    error);
	case YT_DROP_MINES_UNION_ROW:
		return session_02db(session, text, length,
		    "Union sector mine refusal", error);
	case YT_DROP_MINES_PROMPT_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "sector mine prompt blank", error);
	case YT_DROP_MINES_PROMPT:
		return session_031f(session, text, length, "sector mine prompt",
		    error);
	case YT_DROP_MINES_SUCCESS_BLANK:
		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "sector mine success blank", error);
	case YT_DROP_MINES_SUCCESS_ROW:
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
		return session_02fc(session, text, length);
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
	return session_036f(session, response, capacity);
}

static void
drop_mines_suppress(void *context)
{
	struct yt_session *session = context;

	session->suppress_self_mines = true;
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
		.current_player_record = session->player_record,
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

static const uint8_t earth_report_seen_one[4] = {0x00, 0x00, 0x00, 0x81};
static const uint8_t earth_report_fallback_zero[4] = {
	0x00, 0x00, 0x01, 0x00
};

static void
session_set_earth_report_seen(struct yt_session *session,
    const uint8_t raw[4])
{
	memcpy(session->earth_report_seen_raw, raw,
	    sizeof(session->earth_report_seen_raw));
	session->earth_report_seen = qb_mbf32_decode(raw);
}

static void
computer_port_earth_field(struct yt_computer_port_earth_state *state,
    enum yt_computer_port_earth_field_kind kind, uint32_t record,
    const struct yt_record *field)
{
	if (state == NULL || field == NULL)
		return;
	state->field_kind = kind;
	state->field_record = record;
	state->field = *field;
	state->field_valid = true;
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
    size_t *captured_length, struct yt_computer_port_earth_state *earth_state,
    struct yt_error *error)
{
	enum yt_port_owner_kind kind;
	const uint8_t *owner_name = NULL;
	size_t owner_name_length = 0U;
	uint8_t row[256];
	size_t length;
	int owner_record;

	if (captured_length != NULL)
		*captured_length = 0U;
	kind = yt_port_owner_classify(port->owner, session->player_record,
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
		computer_port_earth_field(earth_state,
		    YT_COMPUTER_PORT_EARTH_FIELD_PLAYER, (uint32_t)owner_record,
		    &owner.record);
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
    struct yt_computer_port_earth_state *earth_state,
    struct yt_error *error)
{
	return port_owner_row_capture(session, port, NULL, 0U, NULL,
	    earth_state, error);
}

static bool
port_report_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (physical_record == (uint32_t)session->player_record) {
		if (!reload_player(session, error))
			return false;
		*player = session->player;
		return true;
	}
	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
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

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
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
		return session_b05d(session, text, length);
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

	session->pager.line_count = qb_mbf32_decode(raw);
}

static void
port_report_set_bold(void *context, float bold)
{
	struct yt_session *session = context;

	session->presentation.bold = bold;
}

static void
port_report_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session->presentation.foreground = foreground;
	session->pager.foreground = (int)foreground;
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
	    : yt_port_basic_record(&session->door->game.config, logical_port);
	if (physical_record < 1)
		return port_report_failure(error,
		    "port report record conversion");
	memset(&state, 0, sizeof(state));
	state.current_player_record = session->player_record;
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
	    session->door->game.config.sector_offset;
	state.update.sector_record_expression = sector_record_expression;
	state.update.sector_record_supplied = true;
	state.update.port_offset = session->door->game.config.port_offset;
	memcpy(state.update.base_price, session->market_base,
	    sizeof(state.update.base_price));
	state.report.current_player_record = session->player_record;
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

	if (physical_record != (uint32_t)session->player_record)
		return port_report_failure(error,
		    "commodity trade player record");
	if (!reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
commodity_trade_write_player(void *context, uint32_t physical_record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (physical_record != (uint32_t)session->player_record)
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
		return session_0317(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_MARKET:
		operation = "commodity trade market status";
		return session_0317(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_QUANTITY_PROMPT:
		operation = "commodity trade quantity prompt";
		return session_031f(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_CAPACITY_ERROR:
		operation = "commodity trade capacity rejection";
		return session_02db(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_FREE_HOLDS_ERROR:
		operation = "commodity trade free-holds rejection";
		return session_02db(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_FREE_HOLDS_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE,
		    "commodity trade free-holds retry blank", error);
	case YT_COMMODITY_TRADE_MAXIMUM_ERROR:
		operation = "commodity trade maximum rejection";
		return session_02db(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_NOT_SELLING_ERROR:
		return session_02fc(session, text, length);
	case YT_COMMODITY_TRADE_DONT_WANT_ERROR:
		operation = "commodity trade buying retry";
		return session_02db(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_PLAYER_AMOUNT_ERROR:
		operation = "commodity trade hold retry";
		return session_02db(session, text, length, operation, error);
	case YT_COMMODITY_TRADE_AGREED:
	case YT_COMMODITY_TRADE_DECLINED:
	case YT_COMMODITY_TRADE_SUCCESS:
		return session_02fc(session, text, length);
	case YT_COMMODITY_TRADE_OFFER:
		return session_0317(session, text, length,
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
	return session_0357(context, response, capacity);
}

static bool
commodity_trade_confirm(void *context, const uint8_t *prompt, size_t length,
    bool *accepted, struct yt_error *error)
{
	enum yt_yes_no_answer answer;

	if (!session_a8d2(context, prompt, length, &answer, error))
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
	transaction.current_player_record = (uint32_t)session->player_record;
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
		return session_02db(session, text, length,
		    "port docking refusal", error);
	case YT_ORDINARY_COMMERCE_STATUS:
		return session_0317(session, text, length,
		    "port docking cargo status", error);
	default:
		return false;
	}
}

static void
ordinary_commerce_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session->presentation.foreground = foreground;
	session->pager.foreground = (int)foreground;
}

static void
ordinary_commerce_loop_index(void *context, float index)
{
	struct yt_session *session = context;

	session->shared_loop_scratch = index;
}

static bool
docking_front_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_docking_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PORT_DOCKING_LABEL:
		return session_02fc(session, text, length);
	case YT_PORT_DOCKING_NO_PORT:
		return session_02db(session, text, length,
		    "port docking no port", error);
	case YT_PORT_DOCKING_LEADING_BLANK:
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "port docking leading blank", error);
	case YT_PORT_DOCKING_PREFIX:
		return session_031f(session, text, length,
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
		ordinary_commerce_loop_index,
	};
	struct yt_session *session = context;
	struct yt_ordinary_commerce_state commerce;

	memset(&commerce, 0, sizeof(commerce));
	commerce.sector_number = sector_number;
	commerce.sector_record_expression = sector_record_expression;
	commerce.current_player_record = (uint32_t)session->player_record;
	commerce.first_name =
	    (const uint8_t *)session->door->identity.real_first;
	commerce.first_name_length = strlen(session->door->identity.real_first);
	return yt_ordinary_commerce_run(&commerce, &ops, session, error);
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
	state.port_offset = session->door->game.config.port_offset;
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

	if (!mutate_player_credits(session, -cost, error))
		return false;
	if (cached_earth->owner != 0.0f) {
		float receipt = yt_earth_receipt_amount(cached_earth->owner,
		    session->player_record, cost);

		if (!yt_game_read_port(&session->door->game, 1, &earth, error))
			return false;
		earth.treasury = single_add(earth.treasury, receipt);
		if (!yt_game_write_port(&session->door->game, 1, &earth, error))
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

	if (!session_031f(session, (const uint8_t *)prompt, strlen(prompt),
	    "Earth purchase quantity prompt", error)
	    || !session_036f(session, line, sizeof(line)))
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
	return session_02db(session, (const uint8_t *)text, strlen(text),
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
	if (session->player.holds
	    >= session->door->game.config.maximum_holds)
		return earth_credit_error(session, "You dont need any holds.", error);
	if (qb_str_single(amount, sizeof(amount), single_sub(
	    session->door->game.config.maximum_holds,
	    session->player.holds)) < 0
	    || snprintf(row, sizeof(row), "You need%s holds.", amount) < 0)
		return port_report_failure(error, "Earth Holds needed row");
	if (!session_0317(session, (const uint8_t *)row, strlen(row),
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
		if (!session_b05d(session, (const uint8_t *)row, strlen(row))
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
	session->presentation.bold = 1.0f;
	if (!session_b05d(session,
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
		bool blank;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth Spies leading blank", error))
			return false;
		affordable = yt_earth_affordable(session->player.credits, price);
		if (session->spy_count != 0) {
			if (qb_str_single(active_text, sizeof(active_text),
			    (float)session->spy_count) < 0
			    || snprintf(active_row, sizeof(active_row),
			    "You have%s spies active already.", active_text) < 0)
				return port_report_failure(error,
				    "Earth Spies active row");
			session->presentation.bold = 1.0f;
			if (!session_02fc(session, (const uint8_t *)active_row,
			    strlen(active_row)))
				return false;
		}
		if (snprintf(quantity_prompt, sizeof(quantity_prompt),
		    "Hire how many%s spies? [0]? ",
		    session->spy_count != 0 ? " more" : "") < 0)
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
		if ((float)session->spy_count + quantity_value > 3.0f) {
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
				    (float)(session->spy_count + spy_index + 1)) < 0
				    || snprintf(prompt, sizeof(prompt),
				    "Start spy #%s in what sector?", number) < 0)
					return false;
				if (!earth_quantity_input(session, prompt,
				    &sector_value, &blank, error))
					return false;
				sector = yt_earth_purchase_quantity(sector_value);
				if (sector == 0.0f
				    || sector > (float)sector_count(session))
					continue;
				selected = qb_cint(sector, &overflow);
				if (overflow)
					return port_report_failure(error,
					    "Earth Spy sector CINT");
				session->spies[session->spy_count + spy_index] =
				    selected;
				break;
			}
		}
		session->spy_count += quantity;
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
earth_anti_cloak_read_player(void *context, float record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record raw;
	uint32_t physical = qb_brun_random_record_number(record);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &raw, error))
		return false;
	yt_player_decode(player, &raw);
	return true;
}

static bool
earth_anti_cloak_present(void *context, const uint8_t *text, size_t length,
    float foreground, bool bold, struct yt_error *error)
{
	struct yt_session *session = context;

	if (session->presentation.foreground != foreground)
		session_set_color(session, (int)foreground);
	return session_present_text(session, text, length,
	    bold ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    "anti-cloak transaction row", error);
}

static bool
earth_anti_cloak_sound(void *context, float selector,
    struct yt_error *error)
{
	return session_sound(context, selector, "anti-cloak transaction sound",
	    error);
}

static bool
earth_anti_cloak(struct yt_session *session, float price,
    struct yt_error *error)
{
	static const struct yt_earth_anti_cloak_ops ops = {
		earth_anti_cloak_read_player,
		apply_player_credit_mutation,
		earth_anti_cloak_present,
		earth_anti_cloak_sound,
	};
	struct yt_earth_anti_cloak_state state = {
		.price = price,
		.current_record = (float)session->player_record,
		.player_terminal = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cloak_cache = session->cloak_cache,
		.cloak_cache_count = YT_ARRAY_LEN(session->cloak_cache),
		.foreground = session->presentation.foreground,
	};
	bool completed = yt_earth_anti_cloak_run(&state, &ops, session, error);

	if (session->presentation.foreground != state.foreground)
		session_set_color(session, (int)state.foreground);
	if (state.field_record != 0.0f)
		session->player.record = state.field_player.record;
	if (state.credit_loaded)
		session->player.credits = state.field_player.credits;
	return completed;
}

static bool
clearance(struct yt_session *session, bool create,
    struct yt_error *error)
{
	float *discount[4] = {
		&session->clearance_holds,
		&session->clearance_fighters,
		&session->clearance_shields,
		&session->clearance_ground
	};
	static const char *name[4] = {
		"Holds", "Fighters", "Shields", "Ground Forces"
	};
	bool announced = false;
	size_t index;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "clearance leading blank", error))
		return false;
	for (index = 0; index < 4; ++index) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		if (yt_clearance_candidate_needed(index, draw, *discount[index],
		    create)) {
			if (!random_value(session, &draw, error))
				return false;
			*discount[index] = draw;
		}
		if (yt_clearance_normalize(index, discount[index])) {
			char percent[64];
			char row[192];

			if (qb_str_single(percent, sizeof(percent),
			    yt_clearance_percentage(*discount[index])) < 0
			    || snprintf(row, sizeof(row),
			    "Special clearance sale! The Trader's Guild is selling "
			    "%s for%s%% off!", name[index], percent) < 0
			    || !session_present_text(session, (const uint8_t *)row,
			    strlen(row), SESSION_PRESENT_LINE,
			    "clearance announcement", error))
				return false;
			announced = true;
		}
	}
	if (announced) {
		if (!session_sound(session, 1.0f,
		    "clearance sale sound", error))
			return false;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "clearance trailing blank", error))
			return false;
	}
	return true;
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
	return session_b05d(session, (const uint8_t *)tail, strlen(tail));
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
	if (!reload_player(session, error))
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

		if (qb_str_single(limit, sizeof(limit),
		    session->door->game.config.lottery_plays) < 0
		    || snprintf(row, sizeof(row), "You may play%s times daily.",
		    limit) < 0
		    || !session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE, "lottery daily limit row",
		    error))
			return false;
	}
	if (!reload_player(session, error))
		return false;
	session->player.lottery_plays =
	    single_add(session->player.lottery_plays, 1.0f);
	if (session->player.lottery_plays
	    > session->door->game.config.lottery_plays) {
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
	session->presentation.bold = 1.0f;
	if (!session_b05d(session,
	    (const uint8_t *)"Welcome to the Intergalactic Pick-6 Lottery!",
	    strlen("Welcome to the Intergalactic Pick-6 Lottery!")))
		return false;
	for (;;) {
		bool valid = true;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "lottery ticket leading blank", error))
			return false;
		session_set_color(session, 2);
		session->presentation.bold = 1.0f;
		if (!session_031f(session,
		    (const uint8_t *)
		    "Enter a 6 digit number for the lottery computer -+>",
		    strlen("Enter a 6 digit number for the lottery computer -+>"),
		    "lottery ticket prompt", error)
		    || !session_0345(session, ticket, sizeof(ticket)))
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
		int saved_foreground = session->pager.foreground;

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
		int saved_foreground = session->pager.foreground;

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
	if (!reload_player(session, error))
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
	if (!mutate_player_credits(session, award, error)
	    || !session_wait(session, 3.0, "lottery award wait", error))
		return false;
	return lottery_settle(session, cached_earth, 5.0f, error);
}

static bool
earth_report(struct yt_session *session, struct yt_port *earth,
    float price[4], struct yt_computer_port_earth_state *earth_state,
    struct yt_error *error)
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
	session->pager.line_count = 0.0f;
	if (!yt_game_read_port(&session->door->game, 1, earth, error))
		return false;
	computer_port_earth_field(earth_state,
	    YT_COMPUTER_PORT_EARTH_FIELD_PORT,
	    (uint32_t)yt_port_basic_record(&session->door->game.config, 1),
	    &earth->record);
	session->presentation.foreground = 3.0f;
	session->pager.foreground = 3;
	if (!yt_platform_clock(&date_now, error)
	    || !yt_platform_clock(&time_now, error))
		return false;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	if (snprintf(title, sizeof(title),
	    "Commerce report for Earth: %s %s", date, time_text) < 0
	    || !session_0317(session, (const uint8_t *)title,
	    strlen(title), "Earth report title", error)
	    || !port_owner_row(session, earth, earth_state, error))
		return false;
	discount[0] = session->clearance_holds;
	discount[1] = session->clearance_fighters;
	discount[2] = session->clearance_shields;
	discount[3] = session->clearance_ground;
	yt_earth_prices(discount, price);
	if (session->earth_report_seen == 0.0f) {
		if (!clearance(session, false, error))
			return false;
	}
	else if (!session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "Earth report ordinary blank", error))
		return false;
	session_set_earth_report_seen(session, earth_report_seen_one);
	if (earth_state != NULL)
		memcpy(earth_state->report_seen_raw, earth_report_seen_one,
		    sizeof(earth_state->report_seen_raw));
	if (!reload_player(session, error))
		return false;
	computer_port_earth_field(earth_state,
	    YT_COMPUTER_PORT_EARTH_FIELD_PLAYER,
	    (uint32_t)session->player_record, &session->player.record);
	if (!session_b05d(session, separator, sizeof(separator) - 1U)
	    || !session_b05d(session, header, sizeof(header) - 1U)
	    || !session_b05d(session, separator, sizeof(separator) - 1U))
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
	return session_b05d(session, separator, sizeof(separator) - 1U);
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

		if (!earth_report(session, &earth, price, NULL, error))
			return false;
		holds_price = price[0];
		fighters_price = price[1];
		shields_price = price[2];
		ground_price = price[3];
		if (!session_0317(session, menu, sizeof(menu) - 1U,
		    "Earth report menu", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "Earth prompt blank", error)
		    || qb_str_double(credits_text, sizeof(credits_text),
		    (double)session->player.credits) < 0
		    || snprintf(prompt, sizeof(prompt), "Credits:%s%s",
		    credits_text, prompt_suffix) < 0
		    || !session_031f(session, (const uint8_t *)prompt,
		    strlen(prompt), "Earth item prompt", error)
		    || !session_0357(session, line, sizeof(line)))
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

			session_set_earth_report_seen(session,
			    earth_report_fallback_zero);
			position = yt_earth_selector_position(line);
			if (position == 0) {
				if (!session_02db(session, invalid,
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
			if (!session_a8d2(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer != YT_YES_NO_YES)
				continue;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak accepted blank",
			    error)
			    || !earth_anti_cloak(session, 1000000000.0f, error))
				return false;
			session->anti_cloak = true;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "Earth Anti-Cloak pause blank", error)
			    || !session_031f(session, pause, sizeof(pause) - 1U,
			    "Earth Anti-Cloak pause prompt", error)
			    || !session_0345(session, response, sizeof(response)))
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

	session->current_planet = (float)logical_planet;
	if (!reload_player(session, error)
	    || !planet_update_cached(session, logical_planet, &planet, &cache,
	    error)
	    || !yt_game_read_planet(&session->door->game, logical_planet,
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
	    || !session_02fc(session, title, title_length)
	    || !session_0317(session, header, sizeof(header) - 1U,
	    "planet inventory header", error)
	    || !session_02fc(session, rule, sizeof(rule) - 1U))
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
	if (!session_0317(session, (const uint8_t *)title,
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
	    || !session_031f(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Take One amount prompt", error)
	    || !session_036f(session, response, sizeof(response)))
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

		return session_02db(session, stock, sizeof(stock) - 1U,
		    "planet Take One stock error", error);
	}
	if (quantity > maximum) {
		static const uint8_t capacity[] = "You can't take that much!";

		return session_02db(session, capacity, sizeof(capacity) - 1U,
		    "planet Take One capacity error", error);
	}
	if (!reload_player(session, error))
		return false;
	yt_planet_take_one_player_overlay(&session->player, item, quantity);
	if (!write_player(session, error))
		return false;
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_one_planet_overlay(&planet, item,
	    session->planet_quantity[item], quantity);
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	session->planet_quantity[item] = double_sub(
	    session->planet_quantity[item], (double)quantity);
	return reload_player(session, error);
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

	if (!session_0317(session, title, sizeof(title) - 1U,
	    "planet take-all title", error)
	    || !reload_player(session, error))
		return false;
	yt_planet_take_all_weapon_player_overlay(&session->player,
	    session->planet_quantity, amount);
	if (!write_player(session, error)
	    || !session_0317(session, taking, sizeof(taking) - 1U,
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
			if (!session_0317(session, (const uint8_t *)row,
			    strlen(row), "planet take-all fighters", error))
				return false;
		}
		else if (!session_02fc(session, (const uint8_t *)row,
		    strlen(row)))
			return false;
	}
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_take_all_weapon_planet_overlay(&planet,
	    session->planet_quantity, amount);
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	for (index = 3; index >= 1; --index) {
		char number[64];
		char row[96];
		float commodity_amount;

		if (!reload_player(session, error))
			return false;
		commodity_amount = yt_planet_take_all_commodity_player_overlay(
		    &session->player, index, session->planet_quantity[index]);
		if (!write_player(session, error))
			return false;
		if (!yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_planet_take_all_commodity_planet_overlay(&planet, index,
		    session->planet_quantity[index], commodity_amount);
		if (!yt_game_write_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		if (qb_str_single(number, sizeof(number), commodity_amount) < 0
		    || snprintf(row, sizeof(row), "%s%s",
		    commodity_labels[index - 1], number) < 0)
			return port_report_failure(error,
			    "planet take-all commodity format");
		if (!session_02fc(session, (const uint8_t *)row, strlen(row)))
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

	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	old_garrison = planet.ground_forces;
	if (!reload_player(session, error))
		return false;
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet garrison opening blank", error)
	    || !yt_planet_garrison_prompt(session->player.ground_forces,
	    old_garrison, prompt, sizeof(prompt), &prompt_length)
	    || !session_031f(session, prompt, prompt_length,
	    "planet garrison prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	desired = (float)floor(parsed.valid ? parsed.value : 0.0);
	after = yt_planet_garrison_after(session->player.ground_forces,
	    desired, old_garrison);
	if (desired < 0.0f || after < 0.0f)
		return session_02db(session, insufficient,
		    sizeof(insufficient) - 1U, "planet garrison refusal", error);
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_garrison_overlay(&planet, desired, 0);
	if (desired >= 1.0f) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet garrison success blank", error)
		    || !yt_planet_garrison_success_row(desired, success,
		    sizeof(success), &success_length))
			return false;
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
		if (!session_02fc(session, success, success_length))
			return false;
		planet.owner = (float)session->player_record;
		if (!yt_record_set_number(&planet.record, YT_F73, planet.owner)
		    || !session_sound(session, 4.0f,
		    "planet garrison sound", error))
			return false;
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_planet_basic_record(&session->door->game.config,
	    logical_planet), &planet.record, error)
	    || !reload_player(session, error))
		return false;
	yt_planet_garrison_player_overlay(&session->player, after);
	return yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error);
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

	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (!yt_game_read_planet(&session->door->game, logical_planet,
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
	    || !session_0317(session, (const uint8_t *)title, strlen(title),
	    "planet Bank title", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Bank pre-prompt blank", error)
	    || !session_031f(session, (const uint8_t *)prompt, strlen(prompt),
	    "planet Bank amount prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow)
		return port_report_failure(error, "planet Bank amount VAL");
	target = qb_int(parsed.valid ? parsed.value : 0.0);
	if (target < 0.0)
		return session_02db(session, savings, sizeof(savings) - 1U,
		    "planet Bank savings error", error);
	remaining = yt_planet_bank_remaining(session->player.credits, old_bank,
	    target);
	if (remaining < 0.0)
		return session_02db(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Bank credit error", error);
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_bank_planet_overlay(&planet, target);
	if (!yt_game_write_planet(&session->door->game, logical_planet,
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
	if (!session_0317(session, (const uint8_t *)success, strlen(success),
	    "planet Bank accepted", error)
	    || !session_sound(session, 4.0f, "planet bank sound", error))
		return false;
	credit_argument = yt_planet_bank_credit_argument(old_bank, target);
	return mutate_player_credits(session, credit_argument, error);
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
		    session->door->game.config.planet_offset,
		    (float)logical_planet);
		if (yt_planet_rename_protected(current_record,
		    session->door->game.config.planet_offset,
		    session->door->game.config.total_records))
			return session_02db(session, protected,
			    sizeof(protected) - 1U,
			    "planet Rename protected", error);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Rename leading blank", error)
		    || !session_031f(session, prompt, sizeof(prompt) - 1U,
		    "planet Rename name prompt", error)
		    || !session_0345(session, name, sizeof(name)))
			return false;
		name_result = yt_planet_rename_prepare_name(name, &name_length);
		if (name_result == YT_PLANET_RENAME_EMPTY)
			return true;
		if (name_result == YT_PLANET_RENAME_RESERVED)
			return session_02db(session, reserved,
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
		    || !session_a8d2(session, confirmation, confirmation_length,
		    &answer, error))
			return false;
		if (answer != YT_YES_NO_NO)
			break;
	}
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_rename_overlay(&planet, name, strlen(name));
	snprintf(session->planet_name, sizeof(session->planet_name), "%s", name);
	written = yt_game_write_planet(&session->door->game, logical_planet,
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

	if (!session_0317(session, title, sizeof(title) - 1U,
	    "planet Transfer title", error)
	    || !session_0317(session, question, sizeof(question) - 1U,
	    "planet Transfer question", error)
	    || !session_0317(session, plasma_row, sizeof(plasma_row) - 1U,
	    "planet Transfer plasma row", error)
	    || !session_02fc(session, cargo_row, sizeof(cargo_row) - 1U)
	    || !session_02fc(session, fighter_row, sizeof(fighter_row) - 1U)
	    || !session_02fc(session, missile_row, sizeof(missile_row) - 1U)
	    || !session_02fc(session, mine_row, sizeof(mine_row) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Transfer selector blank", error)
	    || !session_031f(session, selector_prompt,
	    sizeof(selector_prompt) - 1U, "planet Transfer selector", error)
	    || !session_0357(session, command, sizeof(command)))
		return false;
	if (command[0] == '\0')
		return true;
	if (yt_planet_transfer_selector_position(command) == 0)
		return true;
	if (strcmp(command, "C") == 0) {
		struct planet_update_cache cache;
		double held[3];
		size_t index;

		if (!reload_player(session, error))
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

			return session_02db(session, empty, sizeof(empty) - 1U,
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
		if (!reload_player(session, error))
			return false;
		yt_planet_transfer_cargo_player_overlay(&session->player);
		if (!write_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_cargo_planet_overlay(&planet, cache.rate,
		    cache.quantity, cache.contribution);
		if (!yt_game_write_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		{
			static const uint8_t success[] = "Cargo transferred!!";

			if (!session_02fc(session, success, sizeof(success) - 1U))
				return false;
		}
	}
	else if (strcmp(command, "F") == 0) {
		char number[64];
		char prompt[160];
		char response[160];
		struct qb_val_result parsed;
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
		    || !session_031f(session, (const uint8_t *)prompt,
		    strlen(prompt), "planet Transfer fighter prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		amount = (float)(parsed.valid ? parsed.value : 0.0);
		if (yt_planet_transfer_fighter_rejected(amount, cached_fighters))
			return true;
		if (!reload_player(session, error))
			return false;
		yt_planet_transfer_fighter_player_overlay(&session->player,
		    cached_fighters, amount);
		if (!write_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_fighter_planet_overlay(&planet,
		    session->planet_quantity[4], amount);
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &planet, error))
			return false;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer fighter success blank",
		    error))
			return false;
		session->presentation.blink = 1.0f;
		{
			static const uint8_t success[] = "Fighters Transferred!";

			if (!session_02fc(session, success, sizeof(success) - 1U))
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
		if (!reload_player(session, error))
			return false;
		yt_planet_transfer_direct_player_overlay(&session->player, item);
		if (!write_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_planet_transfer_direct_planet_overlay(&planet, item,
		    session->planet_quantity[item], amount);
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &planet, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Transfer weapon success blank",
		    error))
			return false;
		session->presentation.blink = 1.0f;
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
		if (!session_02fc(session, success, success_length))
			return false;
	}
	if (!reload_player(session, error)
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

	if (!reload_player(session, error)
	    || !planet_update_cached(session, logical_planet, &planet, &cache,
	    error))
		return false;
	if (qb_str_double(credits_text, sizeof(credits_text),
	    (double)session->player.credits) < 0
	    || snprintf(credits_row, sizeof(credits_row),
	    "You have%s Credits.", credits_text) < 0
	    || !session_0317(session, explanation, sizeof(explanation) - 1U,
	    "planet Productivity explanation", error)
	    || !session_0317(session, (const uint8_t *)credits_row,
	    strlen(credits_row), "planet Productivity credits", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity pre-prompt blank", error)
	    || !session_031f(session, prompt, sizeof(prompt) - 1U,
	    "planet Productivity spend prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	parsed = qb_val(response);
	if (parsed.overflow)
		return port_report_failure(error,
		    "planet Productivity amount VAL");
	spend = qb_int(parsed.valid ? parsed.value : 0.0);
	if (spend < 1.0)
		return true;
	if (spend > (double)session->player.credits)
		return session_02db(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Productivity credit error", error);
	units = yt_planet_productivity_units(spend);
	if (qb_str_double(units_text, sizeof(units_text), units) < 0
	    || snprintf(success, sizeof(success),
	    "Productivity increased by%s units of ORE, ORG & EQU!",
	    units_text) < 0
	    || !session_0317(session, (const uint8_t *)success,
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
		    || !session_031f(session, (const uint8_t *)fragment,
		    strlen(fragment), "planet Productivity derived fragment",
		    error))
			return false;
	}
	if (delta[0] != 0.0f
	    && !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Productivity derived ending", error))
		return false;
	credit_argument = yt_planet_productivity_credit_argument(units);
	if (!mutate_player_credits(session, credit_argument, error)
	    || !yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_planet_productivity_planet_overlay(&planet, cache.rate,
	    cache.quantity, cache.contribution);
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	return reload_player(session, error);
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
	saved_foreground = session->presentation.foreground;
	if (!planet_update_cached_physical(session, physical_planet, &planet,
	    NULL, error)
	    || !read_planet_physical(session, physical_planet, &planet, error)
	    || !yt_planet_stored_name(&planet, planet_name,
	    &planet_name_length, error)
	    || !reload_player(session, error))
		return false;
	defenders = floorf(planet.ground_forces);
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	yt_planet_assault_player_overlay(&session->player, commitment);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_planet_assault_attack_news(player_name, player_name_length,
	    planet_name, planet_name_length, commitment, row, sizeof(row),
	    &row_length)
	    || !append_news_bytes(session, row, row_length, error))
		return false;
	session->presentation.blink = 1.0f;
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
		session->presentation.foreground = attacker_damage ? 3.0f : 4.0f;
		session->pager.foreground = attacker_damage ? 3 : 4;
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
	session->presentation.foreground = saved_foreground;
	session->pager.foreground = (int)saved_foreground;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet assault terminal blank", error))
		return false;
	if (defenders <= 0.0f) {
		float owner = 0.0f;

		if (!session_present_text(session, defenses,
		    sizeof(defenses) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "planet assault defenses-destroyed row", error)
		    || !append_news_bytes(session, defenses_news,
		    sizeof(defenses_news) - 1U, error)
		    || !session_sound(session, 1.0f,
		    "planet defenses destroyed sound", error))
			return false;
		if (attackers > 0.0f) {
			owner = (float)session->player_record;
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "planet assault capture blank", error))
				return false;
			session->presentation.blink = 1.0f;
			if (!session_present_text(session, captured,
			    sizeof(captured) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet assault capture row", error)
			    || !yt_planet_assault_capture_news(player_name,
			    player_name_length, planet_name, planet_name_length,
			    row, sizeof(row), &row_length)
			    || !append_news_bytes(session, row, row_length, error)
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
	session->presentation.blink = 1.0f;
	if (!yt_planet_assault_failure_row(defenders, true, row, sizeof(row),
	    &row_length)
	    || !append_news_bytes(session, row, row_length, error)
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

	if (!yt_game_read_sector(&session->door->game, logical_sector, &sector,
	    error))
		return false;
	session->navigation_field_kind = NAVIGATION_FIELD_ROUTE_SECTOR;
	session->navigation_field_record = yt_sector_basic_record(
	    &session->door->game.config, logical_sector);
	session->navigation_field = sector.record;
	memcpy(warps, sector.warps, sizeof(sector.warps));
	return true;
}

static bool
build_route(struct yt_session *session, float start, float destination,
    int16_t *next_hop, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error)
{
	int16_t *predecessor;
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;

	predecessor = calloc(YT_ROUTE_CAPACITY, sizeof(*predecessor));
	if (predecessor == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	success = yt_route_build(start, destination, &status, session->avoid,
	    session->presentation.sound.conversion_mode, predecessor, next_hop,
	    route_sector_reader, session, &outcome, error);
	free(predecessor);
	if (!success)
		return false;
	*found = outcome != YT_ROUTE_NOT_FOUND;
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
	int last_player = (int)session->door->game.config.sector_offset;

	if (friendly == NULL)
		return false;
	*friendly = false;
	session->relationship_scratch = 0.0f;
	if (owner < 2.0f || owner > (float)last_player
	    || session->player_record < 2 || session->player_record > last_player)
		return true;
	if (owner == (float)session->player_record) {
		*friendly = true;
		session->relationship_scratch = -1.0f;
		return true;
	}
	if (!yt_game_read_player(&session->door->game, session->player_record,
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
		session->relationship_scratch = -1.0f;
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
	    session->door->game.config.planet_offset);
	uint32_t moving_record;
	int source_record;
	int destination_record;

	if (stop == NULL)
		return false;
	*stop = false;
	if (!yt_game_read_sector(&session->door->game, source_number, &source,
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
	if (!yt_game_read_sector(&session->door->game, destination, &target,
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
	if (!yt_game_read_sector(&session->door->game, source_number, &source,
	    error))
		return false;
	source_link = source.planet;
	moving_record = qb_brun_random_record_number(single_add(
	    session->door->game.config.planet_offset, source_link));
	moving_planet = single_sub(single_add(
	    session->door->game.config.planet_offset, source_link),
	    session->door->game.config.planet_offset);
	yt_planet_move_sector_overlay(&source, 0.0f);
	source_record = yt_sector_basic_record(&session->door->game.config,
	    source_number);
	if (source_record < 0 || !yt_database_write(
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
		session->presentation.bold = 1.0f;
		if (!session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE, "planet move explosion row", error)
		    || !yt_player_stored_name(&session->player, player_name,
		    &player_name_length, error)
		    || !yt_planet_move_explosion_news(planet_name,
		    planet_name_length, player_name, player_name_length,
		    row, sizeof(row), &row_length)
		    || !append_news_bytes(session, row, row_length, error)
		    || !session_sound(session, 3.0f,
		    "planet move explosion sound", error)
		    || !reload_player(session, error))
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
		    (size_t)session->player_record, &session->player.record, error))
			return false;
		if (loss > 0.0f) {
			static const uint8_t you[] = "You";

			if (!yt_planet_move_loss_row(you, sizeof(you) - 1U,
			    loss, row, sizeof(row), &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "planet move fighter loss row", error)
			    || !yt_planet_move_loss_row(player_name,
			    player_name_length, loss, row, sizeof(row), &row_length)
			    || !append_news_bytes(session, row, row_length, error))
				return false;
			*stop = true;
		}
		return true;
	}
	if (moving_planet == 1.0f) {
		float maximum = single_sub(
		    session->door->game.config.port_offset,
		    session->door->game.config.sector_offset);

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
			if (!yt_game_read_sector(&session->door->game,
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
	if (!yt_game_read_sector(&session->door->game,
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
		if (!yt_game_read_sector(&session->door->game,
		    (int)actual_destination, &target, error))
			return false;
	}
	yt_planet_move_sector_overlay(&target, moving_planet);
	destination_record = yt_sector_basic_record(&session->door->game.config,
	    (int)actual_destination);
	if (destination_record < 0 || !yt_database_write(
	    &session->door->game.database, (size_t)destination_record,
	    &target.record, error)
	    || !reload_player(session, error))
		return false;
	yt_planet_move_success_overlay(&session->player, (float)destination);
	session->sector_cache[session->player_record] = (float)destination;
	return yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error);
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
	    session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);
	float cost = 0.0f;
	int start_node;
	int destination_node;
	int16_t *route;
	int cursor;
	bool conversion_overflow;
	bool found;
	bool stop = false;
	bool final = false;
	enum yt_yes_no_answer answer;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!session_0317(session, cost_notice, sizeof(cost_notice) - 1U,
	    "planet Thrusters cost notice", error)
	    || !display_sector(session, false, error)
	    || !session_031f(session, destination_prompt,
	    sizeof(destination_prompt) - 1U,
	    "planet Thrusters destination prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	destination = yt_planet_move_destination(response);
	if (destination == start)
		return session_02db(session, same_sector,
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
		return session_02db(session, row,
		    sizeof(range_prefix) + (size_t)number_length,
		    "planet Thrusters range", error);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters working blank", error)
	    || !session_031f(session, working, sizeof(working) - 1U,
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
	route = calloc(YT_ROUTE_CAPACITY, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (!build_route(session, start, destination, route, true,
	    &found, NULL, NULL, error)) {
		free(route);
		return false;
	}
	if (!found) {
		bool ok = session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route first blank", error)
		    && session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route second blank", error);

		session->presentation.blink = 1.0f;
		if (ok)
			ok = session_present_text(session, route_failure,
			    sizeof(route_failure) - 1U, SESSION_PRESENT_BOLD_LINE,
			    "planet Thrusters route failure", error);
		free(route);
		return ok;
	}
	if (!yt_planet_move_path_heading(start, destination, row, sizeof(row),
	    &row_length)
	    || !session_02fc(session, row, row_length)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route leading blank", error)
	    || qb_str_single(number, sizeof(number), start) < 0
	    || !session_031f(session, (const uint8_t *)number, strlen(number),
	    "planet Thrusters route start", error)) {
		free(route);
		return false;
	}
	cursor = start_node;
	while (route[cursor] != 0) {
		int next = route[cursor];
		int column;
		int ignored_row;
		int number_length;

		session->pager.line_count = 0.0f;
		number_length = qb_str_single(number, sizeof(number), (float)next);
		if (number_length < 0) {
			free(route);
			return false;
		}
		if (next != destination_node)
			number[number_length++] = ',';
		number[number_length] = '\0';
		if (!session_031f(session, (const uint8_t *)number,
		    (size_t)number_length, "planet Thrusters route token", error)) {
			free(route);
			return false;
		}
		yt_out_cursor_position(&ignored_row, &column);
		if (column > 74 && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet Thrusters route wrap", error)) {
			free(route);
			return false;
		}
		cost = yt_planet_move_add_cost(cost);
		cursor = next;
	}
	session->pager.line_count = 0.0f;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route ending", error)
	    || !yt_planet_move_summary(cost, row, sizeof(row), &row_length)
	    || !session_0317(session, row, row_length,
	    "planet Thrusters distance summary", error)
	    || !computer_prompt_hydrate(session, error)) {
		free(route);
		return false;
	}
	if (cost > session->player.turns) {
		bool ok = session_02db(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Thrusters insufficient turns", error);

		free(route);
		return ok;
	}
	if (!yt_planet_move_turns_row(session->player.turns, row, sizeof(row),
	    &row_length)
	    || !session_02fc(session, row, row_length)
	    || !session_a8d2(session, confirmation, sizeof(confirmation) - 1U,
	    &answer, error)) {
		free(route);
		return false;
	}
	if (answer != YT_YES_NO_YES) {
		free(route);
		return true;
	}
	session->presentation.bold = 1.0f;
	session->presentation.blink = 1.0f;
	if (!session_0317(session, engaged, sizeof(engaged) - 1U,
	    "planet Thrusters engaged", error)) {
		free(route);
		return false;
	}
	session->pager.line_count = 0.0f;
	if (!session_031f(session, moving, sizeof(moving) - 1U,
	    "planet Thrusters moving prefix", error)) {
		free(route);
		return false;
	}
	cursor = start_node;
	while (route[cursor] != 0) {
		int next = route[cursor];
		int number_length = qb_str_single(number, sizeof(number),
		    (float)next);

		if (number_length < 0 || !session_031f(session,
		    (const uint8_t *)number, (size_t)number_length,
		    "planet Thrusters movement token", error)) {
			free(route);
			return false;
		}
		if (next == destination_node)
			final = true;
		if (!planet_move_hop(session, cursor, next, final, &stop, error)) {
			free(route);
			return false;
		}
		cursor = next;
		if (stop)
			break;
	}
	if (!stop && !computer_prompt_hydrate(session, error)) {
		free(route);
		return false;
	}
	free(route);
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

	session->current_planet = (float)logical_planet;
	for (;;) {
		char upper[80];
		char free_text[64];
		char free_row[160];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		double free_holds;
		int position;

		session->pager.line_count = 0.0f;
		if (!reload_player(session, error))
			return false;
		free_holds = double_sub(double_sub(double_sub(
		    (double)session->player.holds,
		    (double)session->player.ore),
		    (double)session->player.organics),
		    (double)session->player.equipment);
		if (qb_str_double(free_text, sizeof(free_text), free_holds) < 0
		    || snprintf(free_row, sizeof(free_row),
		    "You have%s free cargo holds.", free_text) < 0
		    || !session_0317(session, (const uint8_t *)free_row,
		    strlen(free_row), "planet free-holds row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "planet prompt framing blank", error))
			return false;
		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
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
		if (!reload_player(session, error)
		    || !planet_update(session, logical_planet,
		    &(struct yt_planet){0}, error)
		    || !session_031f(session, prompt, prompt_length,
		    "planet command prompt", error)
		    || !session_0357(session, upper, sizeof(upper)))
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

			if (!session_0317(session, heading,
			    sizeof(heading) - 1U, "planet help heading", error)
			    || !session_0317(session,
			    (const uint8_t *)rows[0], strlen(rows[0]),
			    "planet help first row", error))
				return false;
			for (row = 1; row < YT_ARRAY_LEN(rows); ++row) {
				if (!session_02fc(session,
				    (const uint8_t *)rows[row], strlen(rows[row])))
					return false;
			}
			continue;
		}
		position = yt_planet_menu_selector_position(upper);
		if (position == 0) {
			static const uint8_t invalid[] = "Invalid command.";

			if (!session_02db(session, invalid,
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

	if (!session_0317(session, no_planet, sizeof(no_planet) - 1U,
	    "planet creation opening", error)
	    || !session_02fc(session, price, sizeof(price) - 1U)
	    || !yt_player_stored_name(&session->player, cached_trader,
	    &cached_trader_length, error)
	    || !reload_player(session, error)
	    || !yt_planet_creation_credit_row((double)session->player.credits,
	    row, sizeof(row), &row_length)
	    || !session_02fc(session, row, row_length))
		return false;
	if (25000.0f > session->player.credits)
		return session_02db(session, too_poor, sizeof(too_poor) - 1U,
		    "planet creation insufficient credits", error);
	if (!session_a8d2(session, buy_prompt, sizeof(buy_prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	scan = single_add(session->door->game.config.planet_offset, 2.0f);
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
			if (!session_02db(session, all_taken,
			    sizeof(all_taken) - 1U,
			    "planet creation allocation full", error)
			    || !session_02fc(session, destroy_first,
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
	    session->door->game.config.planet_offset);
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
	yt_planet_creation_overlay(&planet, session->player_record);
	if (!write_planet_physical(session, selected_physical, &planet, false,
	    error))
		return false;
	sector_physical = qb_brun_random_record_number(single_add(
	    session->door->game.config.sector_offset, session->player.sector));
	if (!yt_database_read(&session->door->game.database,
	    (size_t)sector_physical, &raw, error))
		return false;
	yt_sector_decode(&sector, &raw);
	sector.planet = selected_logical;
	(void)yt_record_set_number(&sector.record, YT_F93, selected_logical);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)sector_physical, &sector.record, error)
	    || !yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
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
	if (!mutate_player_credits(session, -25000.0f, error)
	    || !yt_planet_creation_news(cached_trader, cached_trader_length,
	    (const uint8_t *)session->planet_name, strlen(session->planet_name),
	    row, sizeof(row), &row_length)
	    || !append_news_bytes(session, row, row_length, error)
	    || !yt_planet_creation_success_row(
	    (const uint8_t *)session->planet_name, strlen(session->planet_name),
	    row, sizeof(row), &row_length)
	    || !session_0317(session, row, row_length,
	    "planet creation success row", error)
	    || !session_sound(session, 4.0f, "planet creation sound", error)
	    || !session_0317(session, advice, sizeof(advice) - 1U,
	    "planet creation advice row", error))
		return false;
	return true;
}

static bool
planet_permission_update(void *context, float logical_planet,
    struct yt_error *error)
{
	struct yt_session *session = context;
	volatile float record_value = session->door->game.config.planet_offset
	    + logical_planet;
	uint32_t physical = qb_brun_random_record_number(record_value);

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

static bool
planet_permission_random(void *context, float *value,
    struct yt_error *error)
{
	return random_value(context, value, error);
}

static void
planet_permission_set_foreground(void *context, float foreground)
{
	struct yt_session *session = context;

	session->pager.foreground = (int)foreground;
	session->presentation.foreground = foreground;
}

static void
planet_permission_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	session->presentation.blink = blink;
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
		planet_permission_random,
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

	if (!session_0317(session, title, sizeof(title) - 1U,
	    "planet landing title", error)
	    || !reload_player(session, error))
		return false;
	cached_carried = session->player.ground_forces;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	session->shared_loop_scratch = sector.planet;
	if (sector.planet == 0.0f) {
		bool created = create_planet(session, error);

		if (created && enter_sector != NULL)
			*enter_sector = true;
		return created;
	}
	session->pager.foreground = 6;
	session->presentation.foreground = 6.0f;
	if (!session_0317(session, landing, sizeof(landing) - 1U,
	    "planet landing progress", error))
		return false;
	planet_record_value = session->door->game.config.planet_offset
	    + sector.planet;
	session->planet_record_scratch = planet_record_value;
	memset(&permission_state, 0, sizeof(permission_state));
	permission_state.planet_record_value = planet_record_value;
	permission_state.planet_offset =
	    session->door->game.config.planet_offset;
	permission_state.current_player_record = session->player_record;
	permission_state.last_player_record = YT_PLAYER_LAST;
	permission_state.foreground = session->presentation.foreground;
	permission_state.blink = session->presentation.blink;
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
		    || !session_0317(session, row, row_length,
		    "planet landing sensor row", error))
			return false;
		if (cached_carried < 1.0f) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!session_a8d2(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer != YT_YES_NO_YES) {
			if (enter_sector != NULL)
				*enter_sector = true;
			return true;
		}
		if (!yt_planet_landing_amount_prompt(cached_carried, prompt,
		    sizeof(prompt), &prompt_length)
		    || !session_031f(session, prompt, prompt_length,
		    "planet landing commitment prompt", error)
		    || !session_036f(session, response, sizeof(response)))
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
	if ((int64_t)physical - (int)session->door->game.config.planet_offset
	    < INT_MIN
	    || (int64_t)physical
	    - (int)session->door->game.config.planet_offset > INT_MAX)
		return false;
	logical = (int)((int64_t)physical
	    - (int)session->door->game.config.planet_offset);
	if (!planet_inventory(session, logical, error))
		return false;
	return planet_menu(session, logical, enter_sector, error);
}

static bool
team_load(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	struct yt_team_loader_state loader = {
		.team_id = (float)id,
		.current_player_record = (float)session->player_record,
		.sector_record_offset = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cache = &session->team_cache,
	};
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = id;
	if (!yt_team_loader_run(&loader, session_read_physical_record, session,
	    error))
		return false;
	if (loader.overlay_loaded)
		yt_sector_decode(&team->overlay, &loader.overlay);
	memcpy(team->name, session->team_cache.name,
	    sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = loader.route == YT_TEAM_LOADER_LIVE;
	team->full = team->live;
	for (index = 0; index < 4; ++index) {
		team->roster[index] = session->team_cache.roster[index];
		if (team->roster[index] <= 0.0f)
			team->full = false;
	}
	return true;
}

static bool
team_store(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	yt_record_set_text_if_changed(&team->overlay.record,
	    (const uint8_t *)team->name, strlen(team->name));
	yt_record_set_number_if_changed(&team->overlay.record, YT_F73,
	    (float)strlen(team->name));
	yt_record_set_number_if_changed(&team->overlay.record, YT_F77,
	    team->captain);
	memcpy(team->overlay.record.bytes + YT_F113, team->password, 4);
	for (index = 0; index < 4; ++index)
		yt_record_set_number_if_changed(&team->overlay.record,
		    offsets[index], team->roster[index]);
	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team->id), &team->overlay.record, error);
}

static bool
team_read_overlay(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	memset(team, 0, sizeof(*team));
	team->id = id;
	if (id < 1 || id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	return yt_game_read_sector(&session->door->game, id, &team->overlay,
	    error);
}

static bool
team_store_roster(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	yt_team_roster_overlay(&team->overlay.record, team->roster);
	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team->id), &team->overlay.record, error);
}

static bool
team_audit(struct yt_session *session, float team_id, float event,
    const char *attempt, struct yt_error *error)
{
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	struct yt_team team;
	char date[11];
	char time_text[9];
	size_t index;

	if (event == 2.0f || event == 1.0f) {
		if (!yt_platform_clock(&date_now, error))
			return false;
		yt_format_date(&date_now, date);
		if (!yt_platform_clock(&time_now, error))
			return false;
		yt_format_time(&time_now, time_text);
	}
	if (event == 2.0f)
		snprintf(session->team_audit_message,
		    sizeof(session->team_audit_message),
		    "%s ***  QUIT your team on %s at %s!",
		    session->player.name, date, time_text);
	else if (event == 1.0f)
		snprintf(session->team_audit_message,
		    sizeof(session->team_audit_message),
		    "%s ***  joined your team on %s at %s!",
		    session->player.name, date, time_text);
	else if (event == 0.0f)
		snprintf(session->team_audit_message,
		    sizeof(session->team_audit_message),
		    "%s ***  entered invalid password for your team: %s!",
		    session->player.name, attempt);
	if (!team_load(session, (int)team_id, &team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		bool overflow;
		int32_t converted = qb_cint((double)team.roster[index],
		    &overflow);
		int unequal = team.roster[index]
		    != (float)session->player_record ? -1 : 0;

		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation),
				    "team audit recipient CINT");
			}
			return false;
		}
		if ((((int)converted) & unequal) != 0
		    && !radio_append(session->team_audit_message, -2.0f,
		    team.roster[index], error))
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
	float reads[5];
	float deadline = session->time.deadline;
	float next_refresh = session->time.next_refresh;
	size_t count = 0;
	size_t used;
	int row = 1;
	int column = 1;
	bool updated;
	struct yt_present_result presentation;
	enum yt_present_status status;

	reads[count++] = (float)yt_platform_timer();
	if (single_sub(deadline, reads[0]) > 70000.0f) {
		deadline = single_sub(deadline, 86400.0f);
		reads[count] = (float)yt_platform_timer();
		next_refresh = single_add(reads[count++], 1.0f);
	}
	reads[count] = (float)yt_platform_timer();
	if (reads[count++] >= next_refresh) {
		reads[count++] = (float)yt_platform_timer();
		reads[count++] = (float)yt_platform_timer();
		yt_out_cursor_position(&row, &column);
	}
	status = yt_present_refresh_time(&session->time, reads, count, &used,
	    row, column, &session->presentation, &presentation, &updated);
	if (status != YT_PRESENT_OK)
		return info_failure(error, "Info time refresh");
	yt_out_present_result(&presentation);
	session->session_deadline = session->time.deadline;
	return true;
}

static bool
info_line(struct yt_session *session, const void *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(session, text, length, SESSION_PRESENT_LINE,
	    "Info line presentation", error);
}

static bool
info_team_read_player(void *context, float record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record raw;
	uint32_t physical = qb_brun_random_record_number(record);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &raw, error))
		return false;
	yt_player_decode(player, &raw);
	return true;
}

static bool
info_team_load_team(void *context, float team_id, float current_record,
    float *captain_flag, struct yt_team *team, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_team_loader_state loader = {
		.team_id = team_id,
		.current_player_record = current_record,
		.sector_record_offset = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cache = &session->team_cache,
	};
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = (int)team_id;
	session->team_cache.captain_flag = *captain_flag;
	if (!yt_team_loader_run(&loader, session_read_physical_record, session,
	    error))
		return false;
	if (loader.overlay_loaded)
		yt_sector_decode(&team->overlay, &loader.overlay);
	memcpy(team->name, session->team_cache.name, sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = loader.route == YT_TEAM_LOADER_LIVE;
	team->full = team->live;
	for (index = 0; index < YT_ARRAY_LEN(team->roster); ++index) {
		team->roster[index] = session->team_cache.roster[index];
		if (team->roster[index] <= 0.0f)
			team->full = false;
	}
	*captain_flag = session->team_cache.captain_flag;
	return true;
}

static bool
info_team_read_overlay(void *context, float team_id,
    struct yt_sector *overlay, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record raw;
	float expression = single_add(
	    session->door->game.config.sector_offset, team_id);
	uint32_t physical = qb_brun_random_record_number(expression);

	if (!yt_database_read(&session->door->game.database, (size_t)physical,
	    &raw, error))
		return false;
	yt_sector_decode(overlay, &raw);
	return true;
}

static bool
info_team_write_overlay(void *context, float team_id,
    const struct yt_sector *overlay, struct yt_error *error)
{
	struct yt_session *session = context;
	float expression = single_add(
	    session->door->game.config.sector_offset, team_id);
	uint32_t physical = qb_brun_random_record_number(expression);

	return yt_database_write(&session->door->game.database,
	    (size_t)physical, &overlay->record, error);
}

static bool
info_team_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return info_line(context, text, length, error);
}

static bool
info_team_lines(struct yt_session *session, struct yt_team *resolved_team,
    bool *current_is_captain, struct yt_error *error)
{
	static const struct yt_info_team_ops ops = {
		info_team_read_player,
		info_team_load_team,
		info_team_read_overlay,
		info_team_write_overlay,
		info_team_present,
	};
	struct yt_info_team_state state = {
		.current_record = (float)session->player_record,
		.sector_offset = session->door->game.config.sector_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
	};

	if (!yt_info_team_resolver_run(&state, &ops, session, error))
		return false;
	session->player.record = state.current_player.record;
	session->player.team = state.current_player.team;
	if (resolved_team != NULL)
		*resolved_team = state.team;
	if (current_is_captain != NULL)
		*current_is_captain = state.current_is_captain;
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

	if (!reload_player(session, error))
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

	session->presentation.foreground = state->foreground;
	session->presentation.background = state->background;
	session->presentation.bold = state->bold;
	session->pager.foreground = (int)state->foreground;
	if (kind == YT_INFO_PANEL_LINE)
		result = info_line(session, text, length, error);
	else if (kind == YT_INFO_PANEL_FIXED)
		result = session_fixed_width_bytes(session, text, length, width,
		    "Info fixed-width presentation", error);
	else
		return info_failure(error, "Info presentation kind");
	state->foreground = session->presentation.foreground;
	state->background = session->presentation.background;
	state->bold = session->presentation.bold;
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
	state.anti_cloak = session->anti_cloak ? -1.0f : 0.0f;
	state.foreground = session->presentation.foreground;
	state.background = session->presentation.background;
	state.bold = session->presentation.bold;
	result = yt_info_panel_run(&state, &ops, session, error);
	session->presentation.foreground = state.foreground;
	session->presentation.background = state.background;
	session->presentation.bold = state.bold;
	session->pager.foreground = (int)state.foreground;
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
	    || !session_031f(session, prompt, sizeof(prompt) - 1U,
	    "team name prompt", error)
	    || !session_0345(session, response, sizeof(response)))
		return false;
	if (!yt_team_prepare_name(response, &name_length))
		return session_02db(session, invalid, sizeof(invalid) - 1U,
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
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team.id), &team.overlay.record, error))
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
		    || !session_031f(session, prompt, sizeof(prompt) - 1U,
		    "team password prompt", error)
		    || !session_0357(session, response, sizeof(response)))
			return false;
		if (strlen(response) != 4U) {
			if (!session_02db(session, invalid, sizeof(invalid) - 1U,
			    "team password invalid length", error))
				return false;
			continue;
		}
		memcpy(password, response, 4);
		password[4] = '\0';
		if (snprintf(reminder, sizeof(reminder),
		    "REMEMBER YOUR TEAM PASSWORD SO OTHERS CAN JOIN! -+> %s",
		    password) < 0
		    || !session_02db(session, (const uint8_t *)reminder,
		    strlen(reminder), "team password reminder", error)
		    || !team_read_overlay(session, team_id, &team, error))
			return false;
		memcpy(team.password, password, 5);
		yt_team_password_overlay(&team.overlay.record,
		    (const uint8_t *)password);
		return yt_database_write(&session->door->game.database,
		    (size_t)yt_sector_basic_record(&session->door->game.config,
		    team.id), &team.overlay.record, error);
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
	if (!session_02db(session, entering, sizeof(entering) - 1U,
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
	if (!reload_player(session, error))
		return false;
	session->player.team = selected;
	if (!write_player(session, error)
	    || !team_read_overlay(session, id, &team, error))
		return false;
	team.id = id;
	team.captain = (float)session->player_record;
	team.roster[0] = (float)session->player_record;
	team.roster[1] = 0.0f;
	team.roster[2] = 0.0f;
	team.roster[3] = 0.0f;
	yt_record_set_number_if_changed(&team.overlay.record, YT_F77,
	    team.captain);
	yt_team_roster_overlay(&team.overlay.record, team.roster);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team.id), &team.overlay.record, error)
	    || !team_create_password(session, id, password, error))
		return false;
	session->presentation.foreground = 3.0f;
	session->pager.foreground = 3;
	if (qb_str_single(number, sizeof(number), selected) < 0
	    || snprintf(news, sizeof(news), "%s Created Team%s -=- %s",
	    actor_name, number, name) < 0
	    || !append_news(session, news, error)
	    || snprintf(success, sizeof(success),
	    "Team number [%s ] [%s] CREATED!", number, name) < 0)
		return false;
	return session_02db(session, (const uint8_t *)success,
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
			    || !session_02fc(session, (const uint8_t *)row,
			    strlen(row)))
				return false;
		}
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "team join selection blank", error)
	    || !session_031f(session, selection_prompt,
	    sizeof(selection_prompt) - 1U, "team join selection prompt", error)
	    || !session_036f(session, line, sizeof(line)))
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
		return session_02db(session, dead, sizeof(dead) - 1U,
		    "team join dead", error);
	if (team.full)
		return session_02db(session, full, sizeof(full) - 1U,
		    "team join full", error);
	{
		struct yt_team ignored;

		if (!team_load(session, selected, &ignored, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), (float)selected) < 0
	    || snprintf(row, sizeof(row), "Team #%s: %s", number,
	    team.name) < 0
	    || !session_02fc(session, (const uint8_t *)row, strlen(row))
	    || !session_031f(session, password_prompt,
	    sizeof(password_prompt) - 1U, "team join password prompt", error)
	    || !session_0357(session, line, sizeof(line)))
		return false;
	if (strlen(line) != 4U || memcmp(line, team.password, 4) != 0) {
		if (!team_audit(session, (float)selected, 0.0f, line,
		    error))
			return false;
		return session_02db(session, invalid, sizeof(invalid) - 1U,
		    "invalid team password row", error);
	}
	if (!reload_player(session, error))
		return false;
	session->player.team = (float)selected;
	if (!write_player(session, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team.roster[index] == 0.0f) {
			team.roster[index] = (float)session->player_record;
			break;
		}
	}
	{
		struct yt_team fresh;

		if (!team_load(session, selected, &fresh, error))
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
	session->presentation.foreground = 3.0f;
	session->pager.foreground = 3;
	if (!session_02db(session, success, sizeof(success) - 1U,
	    "team join success row", error))
		return false;
	return team_audit(session, (float)selected, 1.0f, "", error);
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

	if (!session_a8d2(session, prompt, sizeof(prompt) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	if (!reload_player(session, error))
		return false;
	old_team = session->player.team;
	persisted = session->player;
	persisted.team = 0.0f;
	if (!yt_game_write_player(&session->door->game, session->player_record,
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
	if (!yt_game_read_sector(&session->door->game, (int)old_team,
	    &explicit_overlay, error))
		return false;
	(void)explicit_overlay;
	if (!team_load(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team->roster[index] == (float)session->player_record)
			team->roster[index] = 0.0f;
	}
	if (!team_store_roster(session, team, error)
	    || !team_load(session, (int)old_team, team, error))
		return false;
	for (index = 0; index < 4; ++index)
		if (team->roster[index] != 0.0f)
			live = true;
	if (!live) {
		team->captain = 0.0f;
		memcpy(team->password, "    ", 4);
		team->password[4] = '\0';
		memset(team->roster, 0, sizeof(team->roster));
		if (!team_store(session, team, error))
			return false;
	}
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (!session_0317(session, success, sizeof(success) - 1U,
	    "team quit success row", error)
	    || !team_audit(session, old_team, 2.0f, "", error))
		return false;
	session->player.team = 0.0f;
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

	if (!session_0317(session, locating, sizeof(locating) - 1U,
	    "team resource locating row", error))
		return false;
	for (player_record = YT_PLAYER_FIRST;
	    player_record <= (int)session->door->game.config.sector_offset;
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
		    || player_record == session->player_record)
			continue;
		if (qb_str_single(number, sizeof(number), player.sector) < 0)
			return false;
		number_length = strlen(number);
		memcpy(row, player.record.bytes, YT_TEXT_FIELD_SIZE);
		memcpy(row + YT_TEXT_FIELD_SIZE, number, number_length);
		if (!session_0317(session, heading, sizeof(heading) - 1U,
		    "team resource player heading", error)
		    || !session_02fc(session, rule, sizeof(rule) - 1U)
		    || !session_02fc(session, row,
		    YT_TEXT_FIELD_SIZE + number_length))
			return false;
		found = true;

		first = true;
		for (logical_sector = 1; logical_sector <= sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;

			if (!yt_game_read_sector(&session->door->game,
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
		for (logical_sector = 1; logical_sector <= sector_count(session);
		    ++logical_sector) {
			struct yt_sector sector;
			struct yt_planet planet;

			if (!yt_game_read_sector(&session->door->game,
			    logical_sector, &sector, error))
				return false;
			if (sector.planet <= 0.0f)
				continue;
			if (!yt_game_read_planet(&session->door->game,
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
		return session_02fc(session, none, sizeof(none) - 1U);
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

	if (!reload_player(session, error))
		return false;
	logical_sector = (int)session->player.sector;
	initial_fighters = (double)session->player.fighters;
	if (!yt_game_read_sector(&session->door->game, logical_sector,
	    &initial_sector, error))
		return false;
	initial_defense = (double)initial_sector.fighters;
	if (initial_defense == 0.0)
		return session_02db(session, no_defense,
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
		    || !session_0317(session, (const uint8_t *)row, strlen(row),
		    "team transfer carried row", error)
		    || snprintf(row, sizeof(row), "There are%s fighters here.",
		    defense_text) < 0
		    || !session_0317(session, (const uint8_t *)row, strlen(row),
		    "team transfer deployed row", error)
		    || !session_031f(session, prompt, sizeof(prompt) - 1U,
		    "team transfer prompt", error)
		    || !session_036f(session, response, sizeof(response)))
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
			    || !session_02db(session, (const uint8_t *)row,
			    strlen(row), "team transfer too many", error))
				return false;
			continue;
		}
		{
			struct yt_sector fresh_sector;

			if (!yt_game_read_sector(&session->door->game,
			    logical_sector, &fresh_sector, error))
				return false;
			yt_team_transfer_apply_sector(&fresh_sector,
			    initial_defense, amount);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)yt_sector_basic_record(
			    &session->door->game.config, logical_sector),
			    &fresh_sector.record, error))
				return false;
		}
		if (!reload_player(session, error))
			return false;
		yt_team_transfer_apply_player(&session->player, amount);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session->player_record, &session->player.record, error))
			return false;
		return session_02db(session, success, sizeof(success) - 1U,
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

	if (!reload_player(session, error))
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

		if (team->roster[index] <= 0.0f
		    || team->roster[index] == (float)session->player_record)
			continue;
		member_record = (int)team->roster[index];
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
		if (!session_a8d2(session, prompt, prompt_length, &answer, error))
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
		team->roster[index] = 0.0f;
		session->team_cache.roster[index] = 0.0f;
		if (!yt_game_read_sector(&session->door->game, team_id,
		    &team->overlay, error)
		    || !team_store_roster(session, team, error))
			return false;
		return session_02db(session, success, sizeof(success) - 1U,
		    "team banish success", error);
	}
	return session_02fc(session, end, sizeof(end) - 1U);
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

		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team front leading blank", error)
		    || !info_team_lines(session, &team, &captain, error))
			return false;
		session->pager.line_count = 0.0f;
		if (!reload_player(session, error)
		    || !session_0317(session, exit_row, sizeof(exit_row) - 1U,
		    "team exit row", error)
		    || !reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
			for (index = 0; index < YT_ARRAY_LEN(teamless_rows); ++index)
				if (!session_02fc(session,
				    (const uint8_t *)teamless_rows[index],
				    strlen(teamless_rows[index])))
					return false;
		}
		else {
			for (index = 0; index < YT_ARRAY_LEN(member_rows); ++index)
				if (!session_02fc(session,
				    (const uint8_t *)member_rows[index],
				    strlen(member_rows[index])))
					return false;
			if (captain)
				for (index = 0; index < YT_ARRAY_LEN(captain_rows);
				    ++index)
					if (!session_02fc(session,
					    (const uint8_t *)captain_rows[index],
					    strlen(captain_rows[index])))
						return false;
		}
		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
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
		if (!session_031f(session, prompt, prompt_length,
		    "team command prompt", error)
		    || !session_0345(session, line, sizeof(line)))
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
		team_cint = qb_cint((double)session->player.team, &overflow);
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
			if (!session_02db(session, invalid_row,
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
	return session_0317(session, text, length, operation, error);
}

static bool
port_name_prompt(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_031f(context, text, length, "port name prompt", error);
}

static bool
port_name_edit(void *context, uint8_t *response, size_t capacity,
    size_t *length, struct yt_error *error)
{
	(void)error;
	if (length == NULL
	    || !session_0345(context, (char *)response, capacity))
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
	    || !session_a8d2(context, prompt, length, &answer, error))
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
	    (size_t)yt_port_basic_record(&session->door->game.config,
	    logical_port), record, error);
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

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_rename_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, sector_number, sector,
	    error);
}

static bool
port_rename_read_port(void *context, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_port(&session->door->game, logical_port, port,
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
	return session_02db(context, text, length, operation, error);
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
		.current_player_record = (float)session->player_record,
		.port_offset = session->door->game.config.port_offset,
		.conversion_mode =
		    session->presentation.sound.conversion_mode,
	};

	return yt_port_rename_run(&state, &ops, session, error);
}

static bool
port_rename_cycle_rename(void *context, struct yt_error *error)
{
	return command_rename_port(context, error);
}

static bool
port_rename_cycle_scanner(void *context, struct yt_error *error)
{
	return display_current_sector_cached(context, error);
}

static bool
command_rename_port_cycle(struct yt_session *session,
    struct yt_error *error)
{
	static const struct yt_port_rename_cycle_ops ops = {
		port_rename_cycle_rename,
		port_rename_cycle_scanner,
	};
	struct yt_port_rename_cycle_state state;

	return yt_port_rename_cycle_run(&state, &ops, session, error);
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
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
		return session_02fc(session, text, length);
	case YT_PORT_PURCHASE_ACCEPT_TRANSFER_ROW:
	case YT_PORT_PURCHASE_ACCEPT_SUCCESS_TAIL:
		return session_02fc(session, text, length);
	case YT_PORT_PURCHASE_ACCEPT_SUCCESS_FIRST:
		return session_0317(session, text, length,
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

	return yt_game_read_port(&session->door->game, logical_port, port,
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

	if (player_record == session->player_record)
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
	    (size_t)yt_port_basic_record(&session->door->game.config,
	    logical_port), &port->record, error);
}

static bool
port_purchase_accept_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_purchase_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
port_purchase_read_sector(void *context, int sector_number,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, sector_number, sector,
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

		if (!earth_report(session, early_port, earth_prices, NULL, error))
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
	    NULL, error);
}

static bool
port_purchase_present(void *context, const uint8_t *text, size_t length,
    enum yt_port_purchase_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_PORT_PURCHASE_NO_PORT:
		return session_02db(session, text, length, "buy no-port row",
		    error);
	case YT_PORT_PURCHASE_ALREADY_OWNER:
		return session_02db(session, text, length,
		    "buy already-owner row", error);
	case YT_PORT_PURCHASE_PRICE:
		return session_0317(session, text, length, "buy price row", error);
	case YT_PORT_PURCHASE_UNAFFORDABLE:
		return session_02db(session, text, length,
		    "buy unaffordable row", error);
	case YT_PORT_PURCHASE_OFFER_LEADING_BLANK:
	case YT_PORT_PURCHASE_OFFER_TRAILING_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE,
		    kind == YT_PORT_PURCHASE_OFFER_LEADING_BLANK
		    ? "buy owner offer leading blank"
		    : "buy owner offer trailing blank", error);
	case YT_PORT_PURCHASE_OFFER_ROW:
		return session_02fc(session, text, length);
	case YT_PORT_PURCHASE_DECLINED:
		return session_02db(session, text, length, "buy declined row",
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
	    || !session_a8d2(context, prompt, length, &answer, error))
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
		.current_player_record = session->player_record,
		.port_offset = session->door->game.config.port_offset,
		.conversion_mode =
		    session->presentation.sound.conversion_mode,
		.first_name = first,
		.first_name_length = strlen((const char *)first),
	};

	return yt_port_purchase_run(&state, &ops, session, error);
}

static bool
port_purchase_cycle_purchase(void *context, struct yt_error *error)
{
	return command_buy_port(context, error);
}

static bool
port_purchase_cycle_scanner(void *context, struct yt_error *error)
{
	return display_current_sector_cached(context, error);
}

static bool
command_buy_port_cycle(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_port_purchase_cycle_ops ops = {
		port_purchase_cycle_purchase,
		port_purchase_cycle_scanner,
	};
	struct yt_port_purchase_cycle_state state;

	return yt_port_purchase_cycle_run(&state, &ops, session, error);
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
		session->presentation.blink = 1.0f;
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
command_collect(struct yt_session *session, bool collecting,
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
		.current_player_record = (float)session->player_record,
		.port_offset = session->door->game.config.port_offset,
		.planet_offset = session->door->game.config.planet_offset,
		.conversion_mode = session->presentation.sound.conversion_mode,
	};
	static const uint8_t true_raw[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t false_raw[4] = {0x00, 0xae, 0x03, 0x00};

	memcpy(state.collecting_raw, collecting ? true_raw : false_raw,
	    sizeof(state.collecting_raw));
	return yt_treasury_run(&state, &ops, session, error);
}

static bool
genesis_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
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
		return session_0317(session, text, length,
		    "Genesis prophecy first row", error);
	case YT_GENESIS_PROPHECY_SECOND:
		return session_02fc(session, text, length);
	case YT_GENESIS_PROMPT_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "Genesis prompt leading blank", error);
	case YT_GENESIS_DISABLED:
		return session_02db(session, text, length, "Genesis disabled row",
		    error);
	case YT_GENESIS_DECLINED:
		return session_0317(session, text, length, "Genesis declined row",
		    error);
	case YT_GENESIS_INSUFFICIENT_FIRST:
		return session_0317(session, text, length,
		    "Genesis insufficient first row", error);
	case YT_GENESIS_INSUFFICIENT_SECOND:
		return session_02fc(session, text, length);
	case YT_GENESIS_SUCCESS_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "Genesis success leading blank", error);
	case YT_GENESIS_SUCCESS_FIRST:
	case YT_GENESIS_SUCCESS_SECOND:
		session->presentation.bold = 1.0f;
		return session_02fc(session, text, length);
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
	    || !session_a8d2(context, prompt, length, &answer, error))
		return false;
	*accepted = answer == YT_YES_NO_YES;
	return true;
}

static bool
genesis_handoff(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	char sibling[1024];
	char *arguments[2];

	if (!session_close_file5(error))
		return false;
	/*
	 * The database file-1 control predates the new sequential file-5
	 * control.  CLOSE with no file number therefore walks file 5 first,
	 * appending its DOS EOF, and then closes file 1 before RUN.
	 */
	{
		struct yt_close_all_control controls[2];
		struct yt_close_all_result close_all;
		struct yt_text_output handoff;
		size_t length = strlen(session->door->command_line);
		uint8_t *line = malloc(length + 2U);
		size_t control_count = 0U;
		bool ok = false;

		if (line == NULL) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		memcpy(line, session->door->command_line, length);
		line[length] = '\r';
		line[length + 1U] = '\n';
		yt_text_output_init(&handoff);
		if (!yt_text_output_open(&handoff, "RMTINIT.TMP", error)
		    || !yt_text_output_write(&handoff, line, length + 2U, error))
			goto handoff_done;
		if (session->door->game_open) {
			controls[control_count++] = (struct yt_close_all_control){
				YT_CLOSE_ALL_HEAP_FILE, 0,
				session_close_game_all, session->door};
		}
		controls[control_count++] = (struct yt_close_all_control){
			YT_CLOSE_ALL_HEAP_FILE, 0,
			yt_text_output_close_all_method, &handoff};
		ok = yt_close_all_run(controls, control_count, NULL, &close_all,
		    error);

handoff_done:
		yt_text_output_destroy(&handoff);
		free(line);
		if (!ok)
			return false;
	}
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
	yt_door_cleanup();
	if (!yt_platform_spawn(sibling, arguments, YT_SPAWN_REPLACE, NULL,
	    error))
		return false;
	return true; /* Unreachable after a successful RUN replacement. */
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
		.current_player_record = session->player_record,
		.required_ports = session->door->game.config.genesis_ports,
		.cached_trader = cached_trader,
		.cached_trader_length = cached_trader_length,
	};
	return yt_genesis_run(&state, &ops, session, error);
}

static bool projectile_damage_draw(void *context, float *value,
    struct yt_error *error);

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
projectile_planet_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
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
		projectile_damage_draw,
		projectile_planet_read,
		projectile_planet_write,
		projectile_sector_read,
		projectile_sector_write,
		projectile_planet_present,
		projectile_planet_news,
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
	logical_planet = (int)qb_cint((double)sector->planet, &overflow);
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
	    session->door->game.config.planet_offset, sector->planet);
	physical_sector = yt_projectile_physical_record(
	    session->door->game.config.sector_offset, (float)sector_number);
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
	if (planet.owner == (float)session->player_record)
		friendly = true;
	else if (planet.owner > 1.0f
	    && planet.owner <= session->door->game.config.sector_offset) {
		if (!yt_friendship_resolve(planet.owner,
		    (float)session->player_record,
		    session->door->game.config.sector_offset,
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
	    || !append_news_bytes(session, news_row, news_length, error))
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

	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	if (!yt_projectile_sector_mines_overlay(&sector, mines))
		return false;
	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    sector_number), &sector.record, error);
}

static bool
projectile_damage_draw(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
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
	return session_sound(context, selector,
	    "plasma fighter-defense sound", error);
}

static bool
plasma_fighter_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
plasma_fighter_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, (int)sector, value,
	    error);
}

static bool
plasma_fighter_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    (int)sector), &value->record, error);
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
plasma_player_color(void *context, float foreground)
{
	session_set_color(context, (int)foreground);
}

static bool
plasma_player_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "plasma player-attack sound",
	    error);
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
	const char *operation;

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

	return yt_game_read_sector(&session->door->game, sector, value, error);
}

static bool
plasma_killed_write_sector(void *context, int sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config, sector),
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
	return salvage_player(context, victim, (float)shooter, error);
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

	return yt_game_read_planet(&session->door->game, logical_planet, value,
	    error);
}

static bool
plasma_planet_write(void *context, int logical_planet,
    const struct yt_planet *value, struct yt_error *error)
{
	struct yt_session *session = context;
	int physical = yt_planet_basic_record(&session->door->game.config,
	    logical_planet);

	if (physical < 0) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation), "%s",
			    "plasma planet physical record");
		}
		return false;
	}
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

	return yt_friendship_resolve(owner, (float)session->player_record,
	    session->door->game.config.sector_offset,
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

	session->presentation.bold = 1.0f;
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
cruise_defense_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
cruise_defense_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, (int)sector, value,
	    error);
}

static bool
cruise_defense_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    (int)sector), &value->record, error);
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
cruise_mine_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
cruise_mine_read_sector(void *context, float sector,
    struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, (int)sector, value,
	    error);
}

static bool
cruise_mine_write_sector(void *context, float sector,
    const struct yt_sector *value, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    (int)sector), &value->record, error);
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
		projectile_damage_draw,
		cruise_defense_damage_present,
		cruise_defense_news,
		cruise_defense_read_sector,
		cruise_defense_write_sector,
		cruise_defense_victory,
	};
	static const struct yt_projectile_sector_mine_ops mine_ops = {
		cruise_mine_read_sector,
		cruise_mine_present,
		cruise_mine_sound,
		cruise_mine_news,
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
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	probe.sector = &sector;
	probe.hop = (float)sector_number;
	probe.player_terminal = session->door->game.config.sector_offset;
	probe.sector_cache = session->sector_cache;
	probe.cloak_cache = session->cloak_cache;
	probe.cache_count = YT_ARRAY_LEN(session->sector_cache);
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
	defense.shooter = session->player_record;
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
	combat.shooter = session->player_record;
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
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
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
		    yt_projectile_candidate_route(basic, session->player_record,
		    session->sector_cache[basic], (float)sector_number,
		    *remaining);

		if (candidate_route == YT_PROJECTILE_CANDIDATE_TERMINATE)
			break;
		if (candidate_route == YT_PROJECTILE_CANDIDATE_SKIP)
			continue;
		/* YT-SUB:974F is called for its exact GET effects; its result is ignored. */
		{
			bool ignored_friendship;

			if (!yt_friendship_resolve((float)basic,
			    (float)session->player_record,
			    session->door->game.config.sector_offset,
			    friendship_read_player, &session->door->game,
			    &ignored_friendship, error))
				return false;
		}
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (!yt_projectile_candidate_admitted(basic,
		    session->cloak_cache[basic], *xannor_provoker))
			continue;
		if (!session_sound(session, 2.0f,
		    "cruise missile player-attack sound", error))
			return false;
		if (!yt_projectile_player_damage(&target, remaining,
		    projectile_damage_draw, session, &damage, error))
			return false;
		scanner_disabled = damage.scanner_disabled;
		session->presentation.foreground = 5.0f;
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
		    || !append_news_bytes(session, first_news, first_news_length,
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
		session->presentation.foreground = 0.0f;
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
			session->presentation.blink = 1.0f;
			if (!session_present_text(session, destroyed_row,
			    destroyed_length, SESSION_PRESENT_BOLD_LINE,
			    "cruise missile destroyed-player row", error))
				return false;
			if (mines != 0.0f) {
				session->presentation.blink = 1.0f;
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
			    (float)session->player_record, error))
				return false;
			if (yt_projectile_salvage_admitted(*counterattack,
			    *xannor_provoker)) {
				if (!session_sound(session, 3.0f,
				    "cruise missile salvage sound", error)
				    || !salvage_player(session, basic,
				    (float)session->player_record, error))
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
			if (yt_projectile_survivor_sets_counterattack(
			    session->player_record))
				*counterattack = basic;
			return true;
		}
	}
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
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
		plasma_fighter_news,
		plasma_planet_sound,
		projectile_damage_draw,
	};
	struct yt_projectile_plasma_planet_state state;
	bool overflow;
	int logical_planet;

	if (*energy <= 0.0)
		return true;
	logical_planet = (int)qb_cint((double)sector->planet, &overflow);
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
		projectile_damage_draw,
		plasma_fighter_news,
		plasma_fighter_read_sector,
		plasma_fighter_write_sector,
		plasma_fighter_victory,
	};
	static const struct yt_projectile_plasma_mine_ops mine_ops = {
		plasma_mine_sound,
		plasma_fighter_news,
		projectile_damage_draw,
		plasma_mine_present,
		plasma_fighter_read_sector,
		plasma_fighter_write_sector,
	};
	static const struct yt_projectile_plasma_player_ops player_ops = {
		plasma_player_read,
		plasma_player_write,
		plasma_player_color,
		plasma_player_sound,
		projectile_damage_draw,
		plasma_fighter_news,
		plasma_player_present,
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
	else if (!yt_game_read_sector(&session->door->game, sector_number,
	    &sector, error))
		return false;
	memset(&fighter, 0, sizeof(fighter));
	fighter.sector = (float)sector_number;
	fighter.fighters = (double)sector.fighters;
	fighter.owner = sector.fighter_owner;
	fighter.shooter = session->player_record;
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
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
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
	dispatch.player_terminal = session->door->game.config.sector_offset;
	dispatch.sector_cache = session->sector_cache;
	dispatch.cache_count = YT_ARRAY_LEN(session->sector_cache);
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
		player.foreground = session->presentation.foreground;
		if (!yt_projectile_plasma_player_run(&player, &player_ops, session,
		    error))
			return false;
		if (player.route == YT_PROJECTILE_PLASMA_PLAYER_KILLED) {
			memset(&killed, 0, sizeof(killed));
			killed.victim = basic;
			killed.shooter = session->player_record;
			killed.sector = sector_number;
			killed.energy = energy;
			killed.blink = &session->presentation.blink;
			killed.destroyed = &session->destroyed;
			killed.sector_cache = session->sector_cache;
			killed.cache_count = YT_ARRAY_LEN(session->sector_cache);
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

static bool
plasma_sector(struct yt_session *session, int sector_number,
    const uint8_t *attacker, size_t attacker_length, double *energy,
    struct yt_error *error)
{
	return plasma_sector_loaded(session, sector_number, NULL, attacker,
	    attacker_length, energy, error);
}

static bool
cruise_opening_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "cruise missile launch sound",
	    error);
}

static bool
cruise_opening_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;
	enum session_present_text_kind session_kind;
	const char *operation;

	if (kind == YT_PROJECTILE_OPENING_RAW) {
		session_kind = SESSION_PRESENT_RAW;
		operation = "cruise missile loading text";
	}
	else {
		session_kind = SESSION_PRESENT_LINE;
		operation = length == 0U ? "cruise missile opening line"
		    : "cruise missile tracking row";
	}
	return session_present_text(session, text, length, session_kind,
	    operation, error);
}

static bool
plasma_opening_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, selector == 4.0f
	    ? "plasma launch sound" : "plasma bolt firing sound", error);
}

static bool
plasma_opening_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_opening_output_kind kind, struct yt_error *error)
{
	return session_present_text(context, text, length,
	    kind == YT_PROJECTILE_OPENING_RAW ? SESSION_PRESENT_RAW
	    : SESSION_PRESENT_LINE, kind == YT_PROJECTILE_OPENING_RAW
	    ? "plasma loading text" : "plasma opening line", error);
}

static bool
plasma_opening_wait(void *context, float duration, struct yt_error *error)
{
	return session_wait(context, duration, duration == 1.0f
	    ? "plasma launch wait" : "plasma opening wait", error);
}

static bool
projectile_opening(struct yt_session *session, float amount, bool plasma,
    float *last_mine_news_sector, double *energy, float *hop_loss,
    uint8_t *attacker, size_t attacker_capacity, size_t *attacker_length,
    struct yt_error *error)
{
	static const struct yt_projectile_cruise_opening_ops cruise_ops = {
		cruise_opening_sound,
		cruise_opening_present,
	};
	static const struct yt_projectile_plasma_opening_ops plasma_ops = {
		plasma_opening_sound,
		plasma_opening_present,
		plasma_opening_wait,
	};
	struct yt_projectile_plasma_opening_state state;
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	size_t player_name_length;

	if (!plasma) {
		*energy = 0.0;
		*hop_loss = 0.0f;
		*attacker_length = 0U;
		return yt_projectile_cruise_opening_run(last_mine_news_sector,
		    &cruise_ops, session, error);
	}
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error))
		return false;
	memset(&state, 0, sizeof(state));
	state.bolts = amount;
	state.player_name = player_name;
	state.player_name_length = player_name_length;
	if (!yt_projectile_plasma_opening_run(&state, &plasma_ops, session,
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
	session->presentation.blink = 1.0f;
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
	session->presentation.blink = 1.0f;
	return session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_LINE, "cruise missile self-destruct row", error);
}

static bool
plasma_footer_present(void *context, const uint8_t *text, size_t length,
    enum yt_projectile_plasma_footer_output_kind kind,
    struct yt_error *error)
{
	const char *operation;

	if (kind == YT_PROJECTILE_PLASMA_FOOTER_LEADING_BLANK)
		operation = "plasma footer leading blank";
	else if (kind == YT_PROJECTILE_PLASMA_FOOTER_TEXT)
		operation = "plasma footer row";
	else if (kind == YT_PROJECTILE_PLASMA_FOOTER_TRAILING_BLANK)
		operation = "plasma footer trailing blank";
	else
		return false;
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    operation, error);
}

static bool
plasma_footer(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_projectile_plasma_footer_ops ops = {
		plasma_footer_present,
	};

	return yt_projectile_plasma_footer_run(&ops, session, error);
}

static bool
plasma_hop_report(struct yt_session *session, int sector_number,
    double energy, struct yt_error *error)
{
	char sector_text[64];
	char energy_text[64];
	char row[192];

	qb_str_single(sector_text, sizeof(sector_text), (float)sector_number);
	qb_str_double(energy_text, sizeof(energy_text), floor(energy));
	snprintf(row, sizeof(row), "Bolt entering sector%s.%s Megawatts remaining.",
	    sector_text, energy_text);
	return session_present_text(session, (const uint8_t *)row, strlen(row),
	    SESSION_PRESENT_LINE, "plasma hop row", error)
	    && session_wait(session, 0.5, "plasma hop wait", error);
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

static bool
cruise_reroute_line(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "cruise black-hole blank", error);
}

static bool
cruise_reroute_attention(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_attention_bytes(context, text, length,
	    "cruise black-hole attention", error);
}

static bool
cruise_reroute_random(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
}

static bool
cruise_union_police_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    "Union Police missile row", error);
}

struct plasma_route_context {
	struct yt_session *session;
	int *xannor_provoker;
	const uint8_t *attacker;
	size_t attacker_length;
};

static bool
plasma_route_build(void *context, float origin, float destination,
    int16_t *route, size_t route_capacity, float *status,
    struct yt_error *error)
{
	struct plasma_route_context *route_context = context;
	struct yt_session *session = route_context->session;
	bool found;
	enum yt_route_outcome outcome;

	if (route_capacity != YT_ROUTE_CAPACITY)
		return false;
	return build_route(session, origin, destination, route, false, &found,
	    &outcome, status, error);
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

	return session_wait(route_context->session, duration, "plasma hop wait",
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

	if (!yt_game_read_sector(&session->door->game, hop, &sector, error))
		return false;
	memset(&probe, 0, sizeof(probe));
	probe.sector = &sector;
	probe.sector_cache = session->sector_cache;
	probe.cloak_cache = session->cloak_cache;
	probe.cache_count = YT_ARRAY_LEN(session->sector_cache);
	probe.hop = (float)hop;
	probe.player_terminal = session->door->game.config.sector_offset;
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
launch_projectile(struct yt_session *session, float target, float amount,
    bool plasma, float *returned_missiles, float *origin_alias,
    int *pending_counterattack, int *pending_xannor,
	struct yt_error *error)
{
	int count = sector_count(session);
	float destination = target;
	bool overflow;
	int16_t *route;
	bool found;
	int cursor;
	float local_missiles = amount;
	float *missiles = returned_missiles != NULL
	    ? returned_missiles : &local_missiles;
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

	*missiles = amount;

	(void)qb_cint(target, &overflow);
	if (overflow)
		return true;
	if (!projectile_opening(session, amount, plasma,
	    &last_mine_news_sector, &energy, &hop_loss, attacker,
	    sizeof(attacker), &attacker_length, error))
		return false;
	route = calloc(YT_ROUTE_CAPACITY, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (plasma) {
		static const struct yt_projectile_plasma_route_ops ops = {
			plasma_route_build,
			plasma_route_line,
			plasma_route_attention,
			plasma_route_wait,
			plasma_route_random,
			plasma_route_impact,
			plasma_route_footer,
		};
		float local_origin = (float)start;
		float *origin = origin_alias != NULL ? origin_alias : &local_origin;
		struct plasma_route_context route_context = {
			session,
			xannor_provoker,
			attacker,
			attacker_length,
		};
		struct yt_projectile_plasma_route_state state = {
			origin,
			&destination,
			&energy,
			hop_loss,
			{session->black_hole[0], session->black_hole[1]},
			session->door->game.config.sector_offset,
			session->door->game.config.port_offset,
			route,
			YT_ROUTE_CAPACITY,
			YT_ROUTE_CAPACITY * 4U,
			0.0f,
			0.0f,
			0U,
			0U,
		};
		bool result = yt_projectile_plasma_route_run(&state, &ops,
		    &route_context, error);

		free(route);
		return result;
	}
	if ((float)start == destination && plasma) {
			if (origin_alias != NULL)
				*origin_alias = 0.0f;
			if (!plasma_hop_report(session, start, energy, error)) {
				free(route);
				return false;
			}
			found = plasma_sector(session, start, attacker,
			    attacker_length, &energy, error);
			if (found)
				found = plasma_footer(session, error);
			free(route);
			return found;
	}
	for (;;) {
		bool rerouted = false;
		enum yt_route_outcome route_outcome;
		float route_status;
		struct yt_projectile_route_entry_state route_entry;

		if (!build_route(session, (float)start, destination, route,
		    yt_projectile_route_avoid_enabled(plasma, *counterattack,
		    session->player_record), &found, &route_outcome, &route_status,
		    error)) {
			free(route);
			return false;
		}
		if (route_outcome == YT_ROUTE_NOT_FOUND
		    && !route_failure_report(session, error)) {
			free(route);
			return false;
		}
		if (route_status != 0.0f) {
			if (plasma) {
				if (!plasma_footer(session, error)) {
					free(route);
					return false;
				}
			}
			else {
				if (!missile_route_failure_suffix(session, error)) {
					free(route);
					return false;
				}
			}
			free(route);
			return true;
		}
		if (!plasma) {
			route_entry.shooter = session->player_record;
			route_entry.maximum_player_record =
			    session->door->game.config.sector_offset;
			route_entry.start = (float)start;
			if (!yt_projectile_route_entry_run(&route_entry,
			    cruise_route_entry_read_player, session, error)) {
				free(route);
				return false;
			}
			cursor = (int)route_entry.current_hop;
		}
		else
			cursor = start;
		while (yt_projectile_route_has_next(route[cursor])
		    && (!plasma || energy >= 1.0)) {
		int next = route[cursor];

		if (plasma) {
			if (cursor != start)
				energy -= (double)hop_loss;
			if (energy < 1.0)
				break;
			if (!plasma_hop_report(session, next, energy, error)) {
				free(route);
				return false;
			}
		}
		if (yt_projectile_is_black_hole((float)next,
		    session->black_hole[0], session->black_hole[1])) {
			float draw;
			char old_text[64];
			char new_text[64];
			char row[192];
			float local_origin = (float)start;
			float local_destination = destination;
			static const struct yt_projectile_cruise_reroute_ops
			    cruise_ops = {
				cruise_reroute_line,
				cruise_reroute_attention,
				cruise_reroute_random,
			};

			if (!plasma) {
				struct yt_projectile_cruise_reroute_state state = {
					(float)next,
					session->door->game.config.sector_offset,
					session->door->game.config.port_offset,
					origin_alias != NULL ? origin_alias : &local_origin,
					&local_destination,
				};

				if (!yt_projectile_cruise_reroute_run(&state,
				    &cruise_ops, session, error)) {
					free(route);
					return false;
				}
				start = (int)*state.origin;
				destination = *state.destination;
				rerouted = true;
				break;
			}

			start = next;
			if (origin_alias != NULL)
				*origin_alias = (float)next;
			if (!random_value(session, &draw, error)) {
				free(route);
				return false;
			}
			destination = (float)(1
			    + (int)floorf(single_mul(draw, (float)count)));
			qb_str_single(old_text, sizeof(old_text), (float)next);
			qb_str_single(new_text, sizeof(new_text),
			    (float)destination);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "projectile black-hole blank", error)) {
				free(route);
				return false;
			}
			snprintf(row, sizeof(row), "The plasma bolt is "
			    "deflected by a black hole in sector%s to "
			    "sector%s!", old_text, new_text);
			if (!session_attention(session, row,
			    "plasma black-hole attention", error)) {
				free(route);
				return false;
			}
			rerouted = true;
			break;
		}
		if (!plasma) {
			struct yt_projectile_union_police_state police = {
				(float)next,
				destination,
				*counterattack,
				*xannor_provoker,
				false,
			};

			if (!yt_projectile_union_police_run(&police,
			    cruise_union_police_present, session, error)) {
				free(route);
				return false;
			}
			if (police.intercepted) {
				free(route);
				return true;
			}
		}
		if (plasma) {
			if (!plasma_sector(session, next, attacker,
			    attacker_length, &energy, error)) {
				free(route);
				return false;
			}
		}
		else {
			enum missile_sector_route sector_route;

			if (!missile_sector(session, next, missiles,
			    counterattack, xannor_provoker, &last_mine_news_sector,
			    &sector_route, error)) {
				free(route);
				return false;
			}
			if (sector_route == MISSILE_SECTOR_RETURN) {
				free(route);
				return true;
			}
			if (yt_projectile_post_impact_route(*missiles)
			    == YT_PROJECTILE_POST_IMPACT_FOOTER)
				break;
		}
		cursor = next;
		}
		if (!rerouted)
			break;
	}
	free(route);
	if (plasma) {
		if (!plasma_footer(session, error))
			return false;
	}
	else if (!missile_footer(session, error))
		return false;
	return true;
}

static bool
session_projectile_resolver(void *context, float *origin, float target,
    float amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return launch_projectile(session, target, amount, plasma, NULL, origin,
	    counterattack, xannor_provoker, error);
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

	return yt_game_read_sector(&session->door->game, logical_sector, sector,
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
session_xannor_wait(void *context, double seconds, struct yt_error *error)
{
	return session_wait(context, seconds, "Xannor retaliation wait", error);
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
	struct yt_xannor_retaliation_state state = {
		&session->player,
		&session->player_record,
		session->sector_cache,
		session->cloak_cache,
		YT_ARRAY_LEN(session->sector_cache),
		&session->destroyed,
		provoking_player,
		&session->door->game.config.headquarters,
		sector_count(session),
	};

	return yt_xannor_retaliation_run(&state, &ops, session, error);
}

static bool
session_counterlaunch_random(void *context, float *value,
    struct yt_error *error)
{
	return random_value(context, value, error);
}

static bool
session_counterlaunch_write_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_write_player(&session->door->game, player_record, player,
	    error);
}

static bool
session_counterlaunch_present(void *context, const uint8_t *text,
    size_t length, bool bold, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_present_text(session, text, length,
	    bold ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    bold ? "player counterlaunch row" : "player counterlaunch blank",
	    error);
}

static bool
session_counterlaunch_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
session_counterlaunch_projectile(void *context, float *origin, float target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return launch_projectile(session, target, *amount, plasma, amount, origin,
	    counterattack, xannor_provoker, error);
}

static bool
session_counterlaunch_wait(void *context, double seconds,
    struct yt_error *error)
{
	return session_wait(context, seconds, "player counterattack wait", error);
}

static bool
launch_player_counterattack(struct yt_session *session, int *counterattacker,
    int *xannor_provoker, struct yt_error *error)
{
	static const struct yt_counterlaunch_ops ops = {
		session_xannor_read_player,
		session_counterlaunch_random,
		session_counterlaunch_write_player,
		session_counterlaunch_present,
		session_counterlaunch_news,
		session_counterlaunch_projectile,
		session_counterlaunch_wait,
	};
	struct yt_counterlaunch_state state = {
		&session->player,
		&session->player_record,
		session->sector_cache,
		session->cloak_cache,
		YT_ARRAY_LEN(session->sector_cache),
		&session->destroyed,
		&session->counterlaunch_count,
		counterattacker,
		xannor_provoker,
		(int)session->door->game.config.sector_offset,
	};

	return yt_counterlaunch_run(&state, &ops, session, error);
}

static bool
projectile_command_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
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
		return session_02db(session, text, length, "no-turn gate notice",
		    error);
	case YT_PROJECTILE_COMMAND_NO_AMMUNITION_ROW:
		return session_02db(session, text, length,
		    "projectile ammunition refusal", error);
	case YT_PROJECTILE_COMMAND_TARGET_PROMPT:
		return session_031f(session, text, length,
		    "projectile target prompt", error);
	case YT_PROJECTILE_COMMAND_INVALID_SECTOR_ROW:
		return session_02db(session, text, length,
		    "projectile invalid sector", error);
	case YT_PROJECTILE_COMMAND_QUANTITY_PROMPT:
		return session_031f(session, text, length,
		    "projectile quantity prompt", error);
	case YT_PROJECTILE_COMMAND_TOO_MANY_ROW:
		return session_02fc(session, text, length);
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
	return session_036f(context, response, capacity);
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
		session_projectile_resolver,
		projectile_command_counterlaunch,
		projectile_command_xannor,
		projectile_command_fatal,
	};
	struct yt_projectile_command_state state = {
		.current_player_record = session->player_record,
		.maximum_sector = (float)sector_count(session),
		.plasma = plasma,
		.displayed = plasma ? session->player.plasma
		    : session->player.missiles,
		.destroyed = &session->destroyed,
	};

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
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
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
		    || !session_a8d2(session, prompt, prompt_length, &answer,
		    error))
			return false;
		if (answer != YT_YES_NO_NO) {
			*selected = basic;
			return true;
		}
	}
	return session_02fc(session, (const uint8_t *)"Not found.",
	    strlen("Not found."));
}

static bool
radio_line_prompt(struct yt_session *session, int line_number,
    const char *text, struct yt_error *error)
{
	char prompt[96];

	if (snprintf(prompt, sizeof(prompt), " %d:%s", line_number, text) < 0)
		return false;
	return session_031f(session, (const uint8_t *)prompt, strlen(prompt),
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
	    || !session_031f(session, (const uint8_t *)prompt, strlen(prompt),
	    "radio edit line prompt", error)
	    || !session_036f(session, response, sizeof(response)))
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
		return session_02db(session,
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
		    || !session_0317(session, (const uint8_t *)heading,
		    strlen(heading), "radio edit old heading", error)
		    || !session_0317(session, (const uint8_t *)quoted,
		    strlen(quoted), "radio edit old row", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit search blank", error)
		    || !session_031f(session,
		    (const uint8_t *)"Replace what section? -=> ",
		    strlen("Replace what section? -=> "),
		    "radio edit search prompt", error)
		    || !session_0345(session, search, sizeof(search)))
			return false;
		if (search[0] == '\0')
			return true;
		match = strstr(lines[selected - 1], search);
		if (match == NULL) {
			char missing[256];

			if (snprintf(missing, sizeof(missing),
			    "\"%s\" NOT FOUND in line%s!", search,
			    selected_text) < 0
			    || !session_02db(session, (const uint8_t *)missing,
			    strlen(missing), "radio edit search miss", error))
				return false;
			continue;
		}
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio edit replacement blank", error)
		    || !session_031f(session,
		    (const uint8_t *)"Replace it with what? -=> ",
		    strlen("Replace it with what? -=> "),
		    "radio edit replacement prompt", error)
		    || !session_0345(session, replacement, sizeof(replacement)))
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
			    || !session_0317(session, (const uint8_t *)heading,
			    strlen(heading), "radio edit preview heading", error)
			    || !session_0317(session, (const uint8_t *)quoted,
			    strlen(quoted), "radio edit preview row", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio edit confirm blank", error)
			    || !session_a8d2(session, confirmation,
			    sizeof(confirmation) - 1U, &answer, error))
				return false;
			if (answer == YT_YES_NO_YES) {
				snprintf(lines[selected - 1], 76, "%s", changed);
				return session_0317(session,
				    (const uint8_t *)"Change Saved!",
				    strlen("Change Saved!"),
				    "radio edit saved row", error);
			}
			if (answer == YT_YES_NO_NO)
				return session_02db(session,
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

	if (!session_0317(session, warming, sizeof(warming) - 1U,
	    "radio warmup row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio target blank", error)
	    || !session_031f(session, target_prompt,
	    sizeof(target_prompt) - 1U, "radio target prompt", error)
	    || !session_0345(session, target, sizeof(target)))
		return false;
	if (target[0] == '\0')
		return true;
	qb_title_case(target);
	if (strcmp(target, "All") == 0) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "radio broadcast blank", error)
		    || !session_02fc(session, broadcast,
		    sizeof(broadcast) - 1U))
			return false;
		recipients[0] = -2.0f;
		recipient_count = 1;
		all = true;
	}
	else if (strcmp(target, "Team") == 0) {
		struct yt_team team;
		static const uint8_t teamless[] =
		    "You Don't belong to a team!";

		if (!reload_player(session, error))
			return false;
		if (session->player.team == 0.0f) {
			return session_02db(session, teamless,
			    sizeof(teamless) - 1U, "radio teamless row", error);
		}
		if (!team_load(session, (int)session->player.team, &team, error))
			return false;
		for (index = 0; index < 4; ++index)
			recipients[index] = team.roster[index];
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
		char name[80];
		char row[160];

		if (all || recipients[index] == 0.0f)
			continue;
		if (!radio_name(session, recipients[index], name, sizeof(name),
		    false, error))
			return false;
		if (snprintf(row, sizeof(row), "Tuning in to %s's frequency.",
		    name) < 0
		    || !session_02fc(session, (const uint8_t *)row, strlen(row)))
			return false;
	}
	if (!session_0317(session, limit, sizeof(limit) - 1U,
	    "radio line-limit row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "radio body handoff blank", error))
		return false;

	while (!send) {
		bool menu = false;

		if (line_count >= 20) {
			if (!session_02db(session,
			    (const uint8_t *)"Message full!",
			    strlen("Message full!"), "radio message full", error))
				return false;
			menu = true;
		}
		else {
			session->pager.line_count = 0.0f;
			if (!radio_line_prompt(session, line_count + 1,
			    lines[line_count], error))
				return false;
			while (!menu) {
				int key = session_input_key(session);
				size_t length;

				if (key == EOF)
					return false;
				length = strlen(lines[line_count]);
				if (key == '\r' || key == '\n') {
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
							yt_out_plain_line("Message full!");
							menu = true;
						}
						else {
							session->pager.line_count = 0.0f;
							if (!radio_line_prompt(session,
							    line_count + 1, lines[line_count],
							    error))
								return false;
						}
					}
					continue;
				}
				if (key == 8 || key == 127) {
					if (length != 0) {
						lines[line_count][length - 1U] = '\0';
						yt_out_plain("\b \b");
					}
					continue;
				}
				if (key < 0x20 || key > 0x7e || length >= 75U)
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
						if (!session_02db(session,
						    (const uint8_t *)"Message full!",
						    strlen("Message full!"),
						    "radio wrap message full", error))
							return false;
						menu = true;
					}
					else {
						session->pager.line_count = 0.0f;
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
			    || !session_031f(session, menu_prompt,
			    sizeof(menu_prompt) - 1U, "radio menu prompt", error)
			    || !session_0357(session, choice, sizeof(choice)))
				return false;
			if (choice[0] != '\0'
			    && !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "radio menu dispatch blank", error))
				return false;
			if (strcmp(choice, "L") == 0) {
				for (index = 0; index < line_count; ++index) {
					char row[96];

					if (snprintf(row, sizeof(row), " %d:%s", index + 1,
					    lines[index]) < 0
					    || !session_02fc(session,
					    (const uint8_t *)row, strlen(row)))
						return false;
				}
			}
			else if (strcmp(choice, "A") == 0) {
				enum yt_yes_no_answer answer;
				static const uint8_t abort_prompt[] =
				    "Are you sure? [y/N]";

				if (!session_a8d2(session, abort_prompt,
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
		char header[300];

		snprintf(header, sizeof(header), "  -  Message from: %s",
		    session->player.name);
		if (!append_news(session, header, error))
			return false;
	}
	for (index = 0; index < recipient_count; ++index) {
		int body;

		if (recipients[index] == 0.0f)
			continue;
		for (body = 0; body < line_count; ++body) {
			if (all) {
				char news[100];

				snprintf(news, sizeof(news), "  -  %s",
				    lines[body]);
				if (!append_news(session, news, error))
					return false;
			}
			if (!radio_append(lines[body],
			    (float)session->player_record, recipients[index],
			    error))
				return false;
		}
	}
	session->presentation.bold = 1.0f;
	session->presentation.blink = 1.0f;
	return session_02fc(session,
	    (const uint8_t *)"Transmission successful!",
	    strlen("Transmission successful!"));
}

static bool
computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error)
{
	static const uint8_t marker_entry_raw[4] = {0x00, 0x3c, 0x1c, 0x8e};
	static const uint8_t marker_success_raw[4] = {0x00, 0x00, 0x1c, 0x00};
	static const uint8_t status_one_raw[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t status_zero_raw[4] = {0x00, 0x00, 0x00, 0x00};
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
	bool stale_marker = autopilot && session->computer_path_marker == 9999.0f;
	int start;
	int destination;
	int count = sector_count(session);
	int16_t *route;
	bool conversion_overflow;
	bool found;
	int cursor;
	float route_status;

	session->navigation_field_active = true;
	session->navigation_field_kind = NAVIGATION_FIELD_ENTRY_PLAYER;
	session->navigation_field_record = session->player_record;
	session->navigation_field = session->player.record;

	if (!autopilot) {
		session->computer_path_marker = 9999.0f;
		memcpy(session->computer_path_marker_raw, marker_entry_raw,
		    sizeof(marker_entry_raw));
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_031f(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response,
		    &session->computer_path_start,
		    session->computer_path_start_raw, error))
			return false;
	}
	else if (!stale_marker) {
		session->computer_path_start = session->player.sector;
		memcpy(session->computer_path_start_raw,
		    session->player.record.bytes + YT_F57,
		    sizeof(session->computer_path_start_raw));
	}
	start_value = session->computer_path_start;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_031f(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response,
	    &session->computer_path_destination,
	    session->computer_path_destination_raw, error))
		return false;
	destination_value = session->computer_path_destination;
	if (!yt_computer_path_maximum(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset, &maximum, error))
		return false;
	if (destination_value < 1.0f || destination_value > maximum
	    || start_value < 1.0f || start_value > maximum) {
		char number[64];
		char notice[128];

		if (qb_str_single(number, sizeof(number), maximum) < 0
		    || snprintf(notice, sizeof(notice),
		    "Valid sector numbers are from 1 to%s!", number) < 0)
			return false;
		return session_02db(session, (const uint8_t *)notice,
		    strlen(notice), "path invalid endpoint", error);
	}
	if (start_value == destination_value)
		return session_02db(session, same, sizeof(same) - 1U,
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
	route = calloc(YT_ROUTE_CAPACITY, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path working blank", error)
	    || !session_031f(session, working, sizeof(working) - 1U,
	    "path working prompt", error)) {
		free(route);
		return false;
	}
	session->relationship_scratch = 1.0f;
	memcpy(session->relationship_scratch_raw, status_one_raw,
	    sizeof(status_one_raw));
	if (!build_route(session, start_value, destination_value, route, true,
	    &found, NULL, &route_status, error)) {
		free(route);
		return false;
	}
	session->relationship_scratch = route_status;
	memcpy(session->relationship_scratch_raw,
	    found ? status_zero_raw : status_one_raw,
	    sizeof(session->relationship_scratch_raw));
	if (!found) {
		free(route);
		session->presentation.blink = 1.0f;
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
		    || !session_02fc(session, (const uint8_t *)heading,
		    strlen(heading))
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route blank", error)) {
			free(route);
			return false;
		}
	}
	cursor = start;
	session->computer_route_scratch[0] = '1';
	session->computer_route_scratch[1] = '\0';
	session->computer_route_scratch_length = 1U;
	session->computer_path_hops = 0.0f;
	(void)qb_mbf32_encode(session->computer_path_hops,
	    session->computer_path_hops_raw);
	{
		char number[64];

		if (qb_str_single(number, sizeof(number), (float)start) < 0
		    || !session_031f(session, (const uint8_t *)number,
		    strlen(number), "path start token", error)) {
			free(route);
			return false;
		}
	}
	while (route[cursor] != 0) {
		char number[64];
		char token[80];
		int column;
		int ignored_row;

		cursor = route[cursor];
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0
		    || snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0
		    || !session_031f(session, (const uint8_t *)token,
		    strlen(token), "path route token", error)) {
			free(route);
			return false;
		}
		if (!yt_computer_path_append_hop(
		    session->computer_route_scratch,
		    sizeof(session->computer_route_scratch),
		    &session->computer_route_scratch_length, (float)cursor,
		    &session->computer_path_hops,
		    session->computer_path_hops_raw, error)) {
			free(route);
			return false;
		}
		yt_out_cursor_position(&ignored_row, &column);
		if (yt_computer_path_wrap_required(column)
		    && !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path route wrap", error)) {
			free(route);
			return false;
		}
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path token terminator", error)) {
		free(route);
		return false;
	}
	{
		char hop_text[64];
		char course[128];

		if (qb_str_single(hop_text, sizeof(hop_text),
		    session->computer_path_hops) < 0
		    || snprintf(course, sizeof(course),
		    "Course will take%s turns.", hop_text) < 0
		    || !session_0317(session, (const uint8_t *)course,
		    strlen(course), "path course row", error)) {
			free(route);
			return false;
		}
	}
	session->computer_path_marker = 0.0f;
	memcpy(session->computer_path_marker_raw, marker_success_raw,
	    sizeof(marker_success_raw));
	if (!autopilot || stale_marker) {
		free(route);
		return true;
	}
	if (!reload_player(session, error)) {
		free(route);
		return false;
	}
	session->navigation_field_kind = NAVIGATION_FIELD_INNER_PLAYER;
	session->navigation_field_record = session->player_record;
	session->navigation_field = session->player.record;
	if (session->computer_path_hops > session->player.turns) {
		if (!session_02db(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "autopilot insufficient turns", error)) {
			free(route);
			return false;
		}
	}
	else {
		char turns[64];
		char row[128];

		if (qb_str_single(turns, sizeof(turns), session->player.turns) < 0
		    || snprintf(row, sizeof(row), "You have%s turns left.",
		    turns) < 0
		    || !session_02fc(session, (const uint8_t *)row, strlen(row))
		    || !session_a8d2(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error)) {
			free(route);
			return false;
		}
		if (answer == YT_YES_NO_YES) {
			if (!session_0317(session, engaged, sizeof(engaged) - 1U,
			    "autopilot engaged row", error)
			    || !session_0317(session, stop_notice,
			    sizeof(stop_notice) - 1U,
			    "autopilot stop row", error)) {
				free(route);
				return false;
			}
			if (!yt_input_queue_prepend_program(session->queue,
			    sizeof(session->queue), &session->queue_position,
			    &session->queue_length,
			    session->computer_route_scratch,
			    session->computer_route_scratch_length)) {
				free(route);
				return false;
			}
		}
	}
	{
		struct yt_sector current_sector;
		size_t index;

		if (!yt_game_read_sector(&session->door->game,
		    (int)session->player.sector, &current_sector, error)) {
			free(route);
			return false;
		}
		session->navigation_field_kind = NAVIGATION_FIELD_FINAL_SECTOR;
		session->navigation_field_record = yt_sector_basic_record(
		    &session->door->game.config, (int)session->player.sector);
		session->navigation_field = current_sector.record;
		for (index = 0; index < 6U; ++index) {
			session->current_warps[index] = current_sector.warps[index];
			memcpy(session->current_warps_raw[index],
			    current_sector.record.bytes + YT_F41 + index * 4U, 4U);
		}
	}
	free(route);
	return true;
}

static bool
computer_planet_report(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "What sector number is the planet in? ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum = single_sub(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);

	for (;;) {
		struct qb_val_result parsed;
		struct yt_sector sector;
		struct yt_planet planet;
		char response[160];
		float selected;
		bool denied;
		bool fighter_friendly;
		bool last_friendly;
		bool valid_link;

		if (!fresh_no_turn_gate(session, &denied, error))
			return false;
		if (denied)
			return true;
		if (!session_031f(session, prompt, sizeof(prompt) - 1U,
		    "computer planet sector prompt", error)
		    || !session_036f(session, response, sizeof(response)))
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
			    || !session_02db(session, (const uint8_t *)notice,
			    strlen(notice), "computer planet invalid sector", error))
				return false;
			continue;
		}
		if (!yt_game_read_sector(&session->door->game, (int)selected,
		    &sector, error)
		    || !computer_port_friendship(session, sector.fighter_owner,
		    &fighter_friendly, error))
			return false;
		last_friendly = fighter_friendly;
		valid_link = sector.planet > 0.0f
		    && sector.planet <= single_sub(
		    session->door->game.config.total_records,
		    session->door->game.config.planet_offset);
		if (valid_link) {
			size_t name_length;

			session->current_planet = sector.planet;
			if (!yt_game_read_planet(&session->door->game,
			    (int)sector.planet, &planet, error)
			    || !port_report_length(session, planet.name_length,
			    YT_TEXT_FIELD_SIZE, &name_length,
			    "computer planet name length", error))
				return false;
			if ((float)session->player_record != planet.owner
			    && planet.owner != 0.0f
			    && planet.ground_forces != 0.0f
			    && (sector.fighters == 0.0f
			    || (sector.fighters > 0.0f && fighter_friendly))) {
				char forces[64];
				uint8_t row[192];
				size_t length = 0;

				if (!computer_port_friendship(session, planet.owner,
				    &last_friendly, error))
					return false;
				if (!last_friendly) {
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
					return session_0317(session, row, length,
					    "computer planet limited row", error);
				}
			}
		}
		if ((!valid_link && sector.planet == 0.0f)
		    || (sector.fighters > 0.0f && session->player.team > 0.0f
		    && !last_friendly)
		    || (sector.fighters > 0.0f && session->player.team == 0.0f
		    && (float)session->player_record != sector.fighter_owner)) {
			if (!finalize_action(session, 1.0f, error))
				return false;
			return session_0317(session, unavailable,
			    sizeof(unavailable) - 1U,
			    "computer planet unavailable", error);
		}
		if (!valid_link && session->current_planet < 1.0f)
			return port_report_failure(error,
			    "computer planet stale current-planet record");
		return planet_inventory(session, (int)(valid_link
		    ? sector.planet : session->current_planet), error);
	}
}

static bool
computer_owned_fighters(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t searching[] = "Searching;";
	static const uint8_t amount[] = "Amount";
	static const uint8_t rule[] = "--------*--------";
	int sector_number;
	bool found = false;

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "owned-fighter opening blank", error)
	    || !session_031f(session, searching, sizeof(searching) - 1U,
	    "owned-fighter searching row", error))
		return false;
	for (sector_number = 1; sector_number <= sector_count(session);
	    ++sector_number) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
		if (sector.fighters > 0.0f
		    && sector.fighter_owner == (float)session->player_record) {
			char number[64];

			if (!found) {
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE,
				    "owned-fighter searching ending", error)
				    || !session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE,
				    "owned-fighter heading blank", error)
				    || !session_fixed_width(session, " Sector", 10.0f,
				    "owned-fighter heading sector", error)
				    || !session_02fc(session, amount,
				    sizeof(amount) - 1U)
				    || !session_02fc(session, rule, sizeof(rule) - 1U))
					return false;
			}
			if (qb_str_single(number, sizeof(number),
			    (float)sector_number) < 0
			    || !session_fixed_width(session, number, 9.0f,
			    "owned-fighter sector field", error)
			    || qb_str_single(number, sizeof(number),
			    sector.fighters) < 0
			    || !session_02fc(session, (const uint8_t *)number,
			    strlen(number)))
				return false;
			found = true;
			if (strcmp(session->pager.key, "Q") == 0)
				break;
		}
	}
	if (!found)
		return session_present_text(session,
		    (const uint8_t *)" NONE found!", strlen(" NONE found!"),
		    SESSION_PRESENT_LINE, "owned-fighter none row", error);
	return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "owned-fighter trailing blank", error);
}

static bool
owned_planets_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_game_read_sector(&session->door->game, logical_sector,
	    sector, error);
}

static bool
owned_planets_read_planet(void *context, uint32_t physical_record,
    struct yt_planet *planet, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_record record;

	if (!yt_database_read(&session->door->game.database,
	    (size_t)physical_record, &record, error))
		return false;
	yt_planet_decode(planet, &record);
	return true;
}

static bool
owned_planets_present(void *context, const uint8_t *text, size_t length,
    bool bold, const char *operation, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_present_text(session, text, length,
	    bold ? SESSION_PRESENT_BOLD_LINE : SESSION_PRESENT_LINE,
	    operation, error);
}

static void
owned_planets_set_color(void *context, int foreground)
{
	session_set_color(context, foreground);
}

static void
owned_planets_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	session->presentation.blink = blink;
}

static bool
computer_owned_planets(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_owned_planets_ops ops = {
		owned_planets_read_sector,
		owned_planets_read_planet,
		owned_planets_present,
		owned_planets_set_color,
		owned_planets_set_blink,
	};
	struct yt_owned_planets_state state = {
		.maximum_sector = sector_count(session),
		.planet_record_base = session->door->game.config.planet_offset,
		.current_player = (float)session->player_record,
		.blink = session->presentation.blink,
	};

	return yt_owned_planets_run(&state, &ops, session, error);
}

static bool
computer_port_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;

	*friendly = false;
	if (owner < 2.0f
	    || owner > session->door->game.config.sector_offset
	    || (float)session->player_record < 2.0f
	    || (float)session->player_record
	    > session->door->game.config.sector_offset)
		return true;
	if (owner == (float)session->player_record) {
		*friendly = true;
		return true;
	}
	if (!yt_game_read_player(&session->door->game,
	    session->player_record, &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!yt_game_read_player(&session->door->game, (int)owner,
	    &other, error))
		return false;
	*friendly = other.team == current.team;
	return true;
}

static bool
computer_port_visibility_read_player(void *context, uint32_t physical_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (physical_record > (uint32_t)INT_MAX) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "computer port friendship player record");
		}
		return false;
	}
	return yt_game_read_player(&session->door->game, (int)physical_record,
	    player, error);
}

static bool
computer_port_earth_report(void *context,
    struct yt_computer_port_earth_state *state, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_port earth;
	float price[4];

	return earth_report(session, &earth, price, state, error);
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
	bool denied;

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!yt_computer_port_maximum(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset, &maximum, error))
		return false;
	for (;;) {
		enum yt_computer_port_selection_route route;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "computer port sector blank", error)
		    || !session_031f(session, prompt, sizeof(prompt) - 1U,
		    "computer port sector prompt", error)
		    || !session_0345(session, response, sizeof(response)))
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
			    || !session_02db(session, (const uint8_t *)notice,
			    strlen(notice), "computer port invalid sector", error))
				return false;
		}
	}
	sector_number = (int)selected;
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	{
		float sector_expression = yt_port_selected_expression(
		    session->door->game.config.sector_offset, selected);
		bool visibility_ok;

		memset(&visibility, 0, sizeof(visibility));
		visibility.port_link = sector.port;
		visibility.fighter_count = sector.fighters;
		visibility.fighter_owner = sector.fighter_owner;
		visibility.cached_current_team = cached_team;
		visibility.current_player_record = (float)session->player_record;
		visibility.last_player_record =
		    session->door->game.config.sector_offset;
		visibility.planet_record_offset =
		    session->door->game.config.planet_offset;
		visibility.inherited_index = session->shared_loop_scratch;
		visibility.field_kind = YT_COMPUTER_PORT_FIELD_SECTOR;
		visibility.field_record =
		    qb_brun_random_record_number(sector_expression);
		visibility.field = sector.record;
		visibility.field_valid = true;
		visibility_ok = yt_computer_port_visibility_run(&visibility,
		    computer_port_visibility_read_player, session, error);
		session->phase_scratch = visibility.marker_4d62;
		memcpy(session->phase_scratch_raw, visibility.marker_4d62_raw,
		    sizeof(session->phase_scratch_raw));
		session->relationship_scratch = visibility.relation;
		memcpy(session->relationship_scratch_raw, visibility.relation_raw,
		    sizeof(session->relationship_scratch_raw));
		if (visibility.scratch_written)
			session->planet_record_scratch = visibility.scratch_19c4;
		if (!visibility_ok)
			return false;
		denied = visibility.unavailable;
	}
	if (denied)
		return session_0317(session, unavailable,
		    sizeof(unavailable) - 1U,
		    "computer port unavailable", error);
	if (sector.port == 1.0f) {
		struct yt_computer_port_earth_state earth_state;

		memset(&earth_state, 0, sizeof(earth_state));
		earth_state.field_kind = visibility.field_kind
		    == YT_COMPUTER_PORT_FIELD_PLAYER
		    ? YT_COMPUTER_PORT_EARTH_FIELD_PLAYER
		    : YT_COMPUTER_PORT_EARTH_FIELD_SECTOR;
		earth_state.field_record = visibility.field_record;
		earth_state.field = visibility.field;
		earth_state.field_valid = visibility.field_valid;
		memcpy(earth_state.report_seen_raw,
		    session->earth_report_seen_raw,
		    sizeof(earth_state.report_seen_raw));
		if (!yt_computer_port_earth_run(&earth_state,
		    computer_port_earth_report, session, error))
			return false;
		session_set_earth_report_seen(session,
		    earth_state.report_seen_raw);
		return true;
	}
	{
		float sector_record_expression = yt_port_selected_expression(
		    session->door->game.config.sector_offset,
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

	if (!session_0317(session, heading_one, sizeof(heading_one) - 1U,
	    "avoid first heading", error)
	    || !session_0317(session, heading_two, sizeof(heading_two) - 1U,
	    "avoid second heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid heading blank", error))
		return false;
	for (row = 0; row < 10; ++row) {
		char first[96];
		char middle[96];
		char last[96];

		if (!computer_avoid_cell(first, sizeof(first), row + 1,
		    session->avoid[row])
		    || !computer_avoid_cell(last, sizeof(last), row + 21,
		    session->avoid[row + 20])
		    || !session_fixed_width(session, first, 20.0f,
		    "avoid first cell", error)
		    || !computer_avoid_cell(middle, sizeof(middle), row + 11,
		    session->avoid[row + 10])
		    || !session_fixed_width(session, middle, 20.0f,
		    "avoid middle cell", error)
		    || !session_02fc(session, (const uint8_t *)last, strlen(last)))
			return false;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "avoid slot-prompt blank", error)
	    || !session_031f(session, slot_prompt, sizeof(slot_prompt) - 1U,
	    "avoid slot prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (!yt_computer_avoid_select_slot(response,
	    session->presentation.sound.conversion_mode, &slot_value, &slot,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	if (!yt_computer_avoid_maximum(
	    session->door->game.config.port_offset,
	    session->door->game.config.sector_offset, &maximum, error))
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
		    || !session_031f(session, (const uint8_t *)prompt,
		    strlen(prompt), "avoid sector prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
	}
	if (!yt_computer_avoid_select_sector(response, maximum, &new_value,
	    &route, error))
		return false;
	if (route != YT_COMPUTER_AVOID_SELECTION_ACCEPTED)
		return true;
	old_value = session->avoid[slot - 1];
	session->avoid[slot - 1] = new_value;
	yt_computer_avoid_transition(old_value, new_value, &locked, &available);
	session->presentation.foreground = 2.0f;
	session->pager.foreground = 2;
	if (locked) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), new_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now locked out.", number) < 0
		    || !session_0317(session, (const uint8_t *)status,
		    strlen(status), "avoid locked status", error))
			return false;
	}
	if (available) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), old_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now available.", number) < 0
		    || !session_0317(session, (const uint8_t *)status,
		    strlen(status), "avoid available status", error))
			return false;
	}
	session->presentation.foreground = 1.0f;
	session->pager.foreground = 1;
	return true;
}

static bool
computer_spies(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t none[] = "You do not have any spies!";
	int index;

	if (session->spy_count == 0)
		return session_02db(session, none, sizeof(none) - 1U,
		    "active-spy none notice", error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "active-spy leading blank", error))
		return false;
	for (index = 0; index < session->spy_count; ++index) {
		char counter[64];
		char target[64];
		char row[160];

		if (qb_str_single(counter, sizeof(counter), (float)(index + 1)) < 0
		    || qb_str_integer(target, sizeof(target),
		    (int16_t)session->spies[index]) < 0
		    || snprintf(row, sizeof(row), "Spy #%s will hunt in sector%s.",
		    counter, target) < 0)
			return false;
		session->presentation.bold = 1.0f;
		if (!session_02fc(session, (const uint8_t *)row, strlen(row)))
			return false;
	}
	return true;
}

static void
project_port_market(const struct yt_session *session,
    const struct yt_port *port, float prices[4], float projected_stock[3])
{
	float minute = current_minute();
	float elapsed;
	size_t index;

	memset(prices, 0, 4U * sizeof(*prices));
	elapsed = single_add(
	    single_sub((float)session->door->game.today, port->last_day),
	    single_div(single_sub(minute, port->last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	for (index = 0; index < 3; ++index) {
		float production = port->production[index];
		float stock = single_add(port->stock[index],
		    single_mul(production, elapsed));
		float candidate = single_div(stock, 10.0f);
		float numerator;
		float denominator;
		float ratio;
		float scale;
		float raw;

		if (projected_stock != NULL)
			projected_stock[index] = stock;

		if (candidate > production)
			production = candidate;
		numerator = single_mul(port->factor[index], stock);
		denominator = single_mul(production, 1000.0f);
		ratio = single_div(numerator, denominator);
		scale = single_sub(1.0f, ratio);
		raw = single_mul(session->market_base[index], scale);
		prices[index + 1U] = floorf(single_add(raw, 0.5f));
		if (prices[index + 1U] < 1.0f)
			prices[index + 1U] = 1.0f;
	}
}

static bool
profit_project_port_market(struct yt_session *session,
    const struct yt_port *port, float prices[4], struct yt_error *error)
{
	int today;
	int adjusted_year;

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	project_port_market(session, port, prices, NULL);
	return true;
}

static bool
nearest_failure(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
nearest_cint(struct yt_session *session, float value, int *result,
    const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow)
		return nearest_failure(error, operation);
	*result = (int)converted;
	return true;
}

static int
nearest_descending(const void *left, const void *right)
{
	const int a = *(const int *)left;
	const int b = *(const int *)right;

	return a < b ? 1 : a > b ? -1 : 0;
}

static void
nearest_right(char *dest, size_t size, const char *source, size_t width)
{
	size_t length;
	size_t amount;
	size_t padding;

	if (size == 0)
		return;
	if (width >= size)
		width = size - 1U;
	length = strlen(source);
	amount = length < width ? length : width;
	padding = width - amount;
	memset(dest, ' ', padding);
	memcpy(dest + padding, source + length - amount, amount);
	dest[width] = '\0';
}

static bool
nearest_market_cell(struct yt_session *session, const char *cell, bool sold,
    struct yt_error *error)
{
	session->pager.foreground = sold ? 7 : 6;
	session->presentation.foreground = sold ? 7.0f : 6.0f;
	return session_present_text(session, (const uint8_t *)cell,
	    strlen(cell), SESSION_PRESENT_BOLD_RAW,
	    "nearest-port market cell", error);
}

static bool
nearest_more(struct yt_session *session, bool *stop, bool *continuous,
    struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "More? [Y]es [N]o [+] Continuous [Y] ";

	*stop = false;
	session->pager.foreground = 3;
	session->presentation.foreground = 3.0f;
	if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
	    SESSION_PRESENT_BOLD_RAW, "nearest-port pager prompt", error))
		return false;
	for (;;) {
		struct yt_input_value selected;
		uint8_t response;

		if (!session_carrier(session))
			return false;
		if (!session_poll_merged(session, &selected))
			return false;
		if (selected.length != 1) {
			od_sleep(10);
			continue;
		}
		response = selected.bytes[0];
		if (response == '\r')
			response = 'Y';
		qb_compat_upper_n(&response, 1);
		if (response != 'Y' && response != 'N' && response != '+')
			continue;
		if (!session_present_text(session, &response, 1,
		    SESSION_PRESENT_LINE, "nearest-port pager echo", error))
			return false;
		if (response == '+')
			*continuous = true;
		*stop = response == 'N';
		return true;
	}
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
	const char *alphabet = "123ATYEU";
	static const char *const commodities[3] = {
		"Equipment", "Organics", "Ore"
	};
	char response[80];
	const char *match;
	int selector;
	int filter;
	char direction = '\0';
	bool *visited = NULL;
	int *current_layer = NULL;
	int *next_layer = NULL;
	size_t current_count = 0;
	int distance = 0;
	float page_count = 4.0f;
	bool continuous = false;

	if (!reload_player(session, error)
	    || !session_0317(session, first_line, sizeof(first_line) - 1U,
	    "nearest-port first filter row", error)
	    || !session_02fc(session, second_line, sizeof(second_line) - 1U)
	    || !session_031f(session, filter_prompt,
	    sizeof(filter_prompt) - 1U, "nearest-port filter prompt", error)
	    || !session_0357(session, response, sizeof(response)))
		return false;
	match = response[0] == '\0' ? alphabet + 3 : strstr(alphabet, response);
	if (match == NULL)
		return true;
	selector = (int)(match - alphabet) + 1;
	filter = selector;
	if (selector == 5 && session->player.team == 0.0f) {
		static const uint8_t no_team[] = "You dont belong to a team!";

		return session_02db(session, no_team, sizeof(no_team) - 1U,
		    "nearest-port team rejection", error);
	}
	if (selector == 6 && session->player.ports_owned == 0.0f) {
		static const uint8_t none_owned[] = "You dont own any!";

		return session_02db(session, none_owned, sizeof(none_owned) - 1U,
		    "nearest-port ownership rejection", error);
	}
	if (selector >= 1 && selector <= 3) {
		char prompt[128];

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "nearest-port direction blank", error)
		    || snprintf(prompt, sizeof(prompt),
		    "Find ports [B] Buying or [S] Selling %s -=> ",
		    commodities[selector - 1]) < 0
		    || !session_031f(session, (const uint8_t *)prompt,
		    strlen(prompt), "nearest-port direction prompt", error)
		    || !session_0357(session, response, sizeof(response)))
			return false;
		if (strcmp(response, "B") != 0 && strcmp(response, "S") != 0)
			return true;
		direction = response[0];
	}

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "nearest-port opening blank", error))
		return false;
	session->pager.foreground = 3;
	session->presentation.foreground = 3.0f;
	if (!session_present_text(session,
	    (const uint8_t *)"Scanning Starmap Database...", 28,
	    SESSION_PRESENT_BOLD_LINE, "nearest-port scanning row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "nearest-port scanning blank", error))
		return false;
	session->pager.foreground = 7;
	session->presentation.foreground = 7.0f;
	if (!session_present_text(session, (const uint8_t *)
	    "Owned ports show the name of the owner preceeded by a \">\".",
	    sizeof("Owned ports show the name of the owner preceeded by a \">\".")
	    - 1U, SESSION_PRESENT_BOLD_LINE,
	    "nearest-port ownership row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "nearest-port ownership blank", error)
	    || !reload_player(session, error))
		return false;

	visited = calloc(3001U, sizeof(*visited));
	current_layer = malloc(3001U * sizeof(*current_layer));
	next_layer = malloc(3001U * sizeof(*next_layer));
	if (visited == NULL || current_layer == NULL || next_layer == NULL) {
		free(visited);
		free(current_layer);
		free(next_layer);
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (!nearest_cint(session, session->player.sector, &selector,
	    "nearest-port start-sector CINT", error))
		goto failure;
	if (selector < 0 || selector > 3000) {
		(void)nearest_failure(error, "nearest-port start workspace");
		goto failure;
	}
	if (session->player.sector != 0.0f) {
		visited[selector] = true;
		current_layer[current_count++] = selector;
	}
	while (current_count != 0) {
		size_t layer_index;
		size_t next_count = 0;
		bool heading_emitted = false;

		for (layer_index = 0; layer_index < current_count; ++layer_index) {
			int sector_number = current_layer[layer_index];
			struct yt_sector sector;
			struct yt_port port;
			bool accepted = false;
			bool member = false;
			size_t index;
			int today;
			int adjusted_year;

			if (!yt_game_read_sector(&session->door->game,
			    sector_number, &sector, error))
				goto failure;
			for (index = 0; index < 6; ++index) {
				int target;

				if (sector.warps[index] == 0.0f)
					continue;
				if (!nearest_cint(session, sector.warps[index], &target,
				    "nearest-port warp CINT", error))
					goto failure;
				if (target < 0 || target > 3000) {
					(void)nearest_failure(error,
					    "nearest-port warp workspace");
					goto failure;
				}
				if (visited[target])
					continue;
				visited[target] = true;
				next_layer[next_count++] = target;
			}
			if (sector.port == 0.0f)
				continue;
			if (!yt_current_date_serial(
			    session->door->game.config.epoch_year, &today,
			    &adjusted_year, error))
				goto failure;
			session->door->game.today = today;
			session->door->game.adjusted_year = adjusted_year;
			if (!yt_game_read_port(&session->door->game,
			    (int)sector.port, &port, error))
				goto failure;
			if (session->player.team != 0.0f) {
				for (index = 0; index < 4; ++index) {
					if (port.owner
					    == session->team_cache.roster[index])
						member = true;
				}
			}
			if (filter >= 1 && filter <= 3)
				accepted = direction == 'S'
				    ? port.commodity_class == (float)filter
				    : port.commodity_class != (float)filter;
			else if (filter == 4)
				accepted = true;
			else if (filter == 5)
				accepted = port.owner > 0.0f
				    && port.owner != (float)session->player_record
				    && member;
			else if (filter == 6)
				accepted = port.owner
				    == (float)session->player_record;
			else if (filter == 7)
				accepted = port.owner > 0.0f
				    && port.owner
				    != (float)session->player_record
				    && (session->player.team == 0.0f
				    || (session->player.team > 0.0f && !member));
			else if (filter == 8)
				accepted = port.owner == 0.0f;
			if (!accepted || port.commodity_class == 0.0f)
				continue;
			{
				float prices[4];
				float stocks[3];
				float total;
				float aggregate;
				char number[64];
				char source[96];
				char sector_cell[14];
				char ore_cell[20];
				char organics_cell[20];
				char equipment_cell[20];
				char stock_cell[8];
				uint8_t name[128];
				size_t name_length;
				bool overflow;
				int32_t stored_length;
				int owner_record;
				bool stop;

				project_port_market(session, &port, prices, stocks);
				if (!heading_emitted) {
					qb_str_single(number, sizeof(number),
					    (float)distance);
					(void)snprintf(source, sizeof(source),
					    "Distance:%s", number);
					session->pager.foreground = 1;
					session->presentation.foreground = 1.0f;
					if (!session_present_text(session,
					    (const uint8_t *)source, strlen(source),
					    SESSION_PRESENT_BOLD_LINE,
					    "nearest-port distance heading", error))
						goto failure;
					page_count = single_add(page_count, 1.0f);
					heading_emitted = true;
				}
				stored_length = qb_cint_mode((double)port.name_length,
				    session->presentation.sound.conversion_mode,
				    &overflow);
				if (overflow || stored_length < 0) {
					(void)nearest_failure(error,
					    "nearest-port name length");
					goto failure;
				}
				name_length = (size_t)stored_length;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				memcpy(name, port.record.bytes, name_length);

				qb_str_single(number, sizeof(number),
				    (float)sector_number);
				(void)snprintf(source, sizeof(source), "%s      ",
				    number);
				(void)snprintf(sector_cell, sizeof(sector_cell),
				    "Sector:%.6s", source);
				qb_str_single(number, sizeof(number), prices[1]);
				(void)snprintf(source, sizeof(source), " %s", number);
				nearest_right(response, sizeof(response), source, 3);
				(void)snprintf(ore_cell, sizeof(ore_cell),
				    " Ore @%c%s  ",
				    port.commodity_class == 3.0f ? 'S' : 'B',
				    response);
				qb_str_single(number, sizeof(number), prices[2]);
				(void)snprintf(source, sizeof(source), " %s", number);
				nearest_right(response, sizeof(response), source, 3);
				(void)snprintf(organics_cell, sizeof(organics_cell),
				    " Org @%c%s  ",
				    port.commodity_class == 2.0f ? 'S' : 'B',
				    response);
				qb_str_single(number, sizeof(number), prices[3]);
				(void)snprintf(source, sizeof(source), " %s", number);
				nearest_right(response, sizeof(response), source, 3);
				(void)snprintf(equipment_cell,
				    sizeof(equipment_cell), " Equ @%c%s",
				    port.commodity_class == 1.0f ? 'S' : 'B',
				    response);
				total = single_add(single_add(stocks[0], stocks[1]),
				    stocks[2]);
				aggregate = floorf(single_add(
				    single_div(total, 10000.0f), 0.5f));
				qb_str_double(number, sizeof(number),
				    (double)aggregate);
				(void)snprintf(source, sizeof(source),
				    "      %sK  ", number);
				nearest_right(stock_cell, sizeof(stock_cell), source, 7);
				if (sector_number == 1) {
					ore_cell[0] = '\0';
					organics_cell[0] = '\0';
					equipment_cell[0] = '\0';
					stock_cell[0] = '\0';
				}
				if (port.owner != 0.0f)
					session->presentation.bold = 1.0f;
				session->pager.foreground = 2;
				session->presentation.foreground = 2.0f;
				if (!session_present_text(session,
				    (const uint8_t *)sector_cell,
				    strlen(sector_cell), SESSION_PRESENT_RAW,
				    "nearest-port sector prefix", error))
					goto failure;
				if (!nearest_market_cell(session, ore_cell,
				    port.commodity_class == 3.0f, error)
				    || !nearest_market_cell(session, organics_cell,
				    port.commodity_class == 2.0f, error)
				    || !nearest_market_cell(session, equipment_cell,
				    port.commodity_class == 1.0f, error))
					goto failure;
				session->pager.foreground = 2;
				session->presentation.foreground = 2.0f;
				if (!session_present_text(session,
				    (const uint8_t *)stock_cell,
				    strlen(stock_cell), SESSION_PRESENT_RAW,
				    "nearest-port aggregate cell", error))
					goto failure;
				if (!nearest_cint(session, port.owner, &owner_record,
				    "nearest-port owner CINT", error))
					goto failure;
				session->pager.foreground = 3;
				session->presentation.foreground = 3.0f;
				if (sector_number == 1) {
					memcpy(name, "** Earth **", 11);
					name_length = 11;
					session->presentation.blink = 1.0f;
				}
				else if (owner_record != 0) {
					struct yt_player owner;

					if (!yt_game_read_player(&session->door->game,
					    (int)port.owner, &owner, error))
						goto failure;
					memmove(name + 2, owner.record.bytes,
					    YT_TEXT_FIELD_SIZE);
					memcpy(name, "> ", 2);
					name_length = qb_title_case_n(name,
					    YT_TEXT_FIELD_SIZE + 2U);
					if (name_length > 26U)
						name_length = 26U;
					if (port.owner
					    == (float)session->player_record) {
						session->pager.foreground = 5;
						session->presentation.foreground = 5.0f;
					}
				}
				if (!session_present_text(session, name, name_length,
				    SESSION_PRESENT_BOLD_LINE,
				    "nearest-port name row", error))
					goto failure;
				page_count = single_add(page_count, 1.0f);
				if (continuous)
					page_count = 0.0f;
				if (page_count > 22.0f) {
					page_count = 0.0f;
					if (!nearest_more(session, &stop, &continuous,
					    error))
						goto failure;
					if (stop) {
						if (!session_present_text(session, NULL, 0,
						    SESSION_PRESENT_LINE,
						    "nearest-port final blank", error))
							goto failure;
						free(visited);
						free(current_layer);
						free(next_layer);
						return true;
					}
				}
			}
		}
		qsort(next_layer, next_count, sizeof(*next_layer),
		    nearest_descending);
		{
			int *swap = current_layer;

			current_layer = next_layer;
			next_layer = swap;
		}
		current_count = next_count;
		++distance;
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "nearest-port final blank", error))
		goto failure;
	free(visited);
	free(current_layer);
	free(next_layer);
	return true;

failure:
	free(visited);
	free(current_layer);
	free(next_layer);
	return false;
}

static bool
profit_range_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
	}
	return false;
}

static bool
profit_cint(struct yt_session *session, float value, int *result,
    const char *operation, struct yt_error *error)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value,
	    session->presentation.sound.conversion_mode, &overflow);

	if (overflow)
		return profit_range_error(error, operation);
	*result = (int)converted;
	return true;
}

static void
profit_set_pair_color(struct yt_session *session, float source, float target)
{
	int logical = -1;

	if ((source == 1.0f && target == 2.0f)
	    || (source == 2.0f && target == 1.0f))
		logical = 3;
	else if ((source == 1.0f && target == 3.0f)
	    || (source == 3.0f && target == 1.0f))
		logical = 2;
	else if ((source == 2.0f && target == 3.0f)
	    || (source == 3.0f && target == 2.0f))
		logical = 1;
	if (logical >= 0) {
		session->presentation.foreground = (float)logical;
		session->pager.foreground = logical;
	}
}

static size_t
profit_right_four(uint8_t dest[4], const char *number)
{
	char joined[80];
	size_t length;

	snprintf(joined, sizeof(joined), "  %s", number);
	length = strlen(joined);
	if (length > 4U) {
		memcpy(dest, joined + length - 4U, 4U);
		return 4U;
	}
	memcpy(dest, joined, length);
	return length;
}

static bool
profit_emit_row(struct yt_session *session, float source_number,
    int target_number, const struct yt_port *source_port,
    const struct yt_port *target_port, const float source_price[4],
    const float target_price[4], bool all, int *result_count,
    struct yt_error *error)
{
	static const char *arrows[3] = {"Equ -> ", "Org -> ", "Ore -> "};
	static const char *labels[3] = {"Equ", "Org", "Ore"};
	uint8_t row[64];
	char number[80];
	char profit_text[80];
	size_t length = 0;
	int source_index;
	int target_index;
	float profit;
	const char *arrow = source_port->commodity_class == 1.0f
	    ? arrows[0] : source_port->commodity_class == 2.0f
	    ? arrows[1] : arrows[2];
	const char *label = target_port->commodity_class == 1.0f
	    ? labels[0] : target_port->commodity_class == 2.0f
	    ? labels[1] : labels[2];

	if (all)
		++*result_count;
	if (!profit_cint(session,
	    single_sub(4.0f, source_port->commodity_class), &source_index,
	    "profit source class index", error)
	    || !profit_cint(session,
	    single_sub(4.0f, target_port->commodity_class), &target_index,
	    "profit target class index", error))
		return false;
	if (source_index < 0 || source_index > 3
	    || target_index < 0 || target_index > 3)
		return profit_range_error(error, "profit raw price-array index");
	profit = single_add(fabsf(single_sub(source_price[source_index],
	    target_price[source_index])), fabsf(single_sub(
	    target_price[target_index], source_price[target_index])));
	profit_set_pair_color(session, source_port->commodity_class,
	    target_port->commodity_class);
	qb_str_single(number, sizeof(number), source_number);
	length += profit_right_four(row + length, number);
	row[length++] = ',';
	qb_str_integer(number, sizeof(number), (int16_t)target_number);
	length += profit_right_four(row + length, number);
	row[length++] = ' ';
	memcpy(row + length, arrow, strlen(arrow));
	length += strlen(arrow);
	memcpy(row + length, label, strlen(label));
	length += strlen(label);
	memcpy(row + length, " @ Profit of", strlen(" @ Profit of"));
	length += strlen(" @ Profit of");
	qb_str_single(profit_text, sizeof(profit_text), profit);
	{
		char field[96];

		snprintf(field, sizeof(field), "%s   ", profit_text);
		memcpy(row + length, field, 4U);
		length += 4U;
	}
	if (!all)
		return session_present_text(session, row, length,
		    SESSION_PRESENT_BOLD_LINE, "adjacent profit row", error);
	if (!session_present_text(session, row, length,
	    SESSION_PRESENT_BOLD_RAW, "global profit row", error))
		return false;
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if ((*result_count & 1) != 0) {
		static const uint8_t separator[] = {' ', 0xba, ' '};

		if (!session_present_text(session, separator, sizeof(separator),
		    SESSION_PRESENT_BOLD_RAW, "global profit separator", error))
			return false;
	}
	else if (!session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "global profit row ending", error))
		return false;
	return true;
}

static bool
profit_more(struct yt_session *session, bool *stop, struct yt_error *error)
{
	static const uint8_t prompt[] = "More? [Y/n] ";

	*stop = false;
	for (;;) {
		struct yt_input_value selected;
		int key;
		uint8_t response;

		if (!session_present_text(session, prompt, sizeof(prompt) - 1U,
		    SESSION_PRESENT_RAW, "profit pager prompt", error))
			return false;
		do {
			if (!session_carrier(session))
				return false;
			if (!session_poll_merged(session, &selected))
				return false;
			key = selected.length == 1 ? selected.bytes[0] : 0;
			if (key == 0)
				od_sleep(10);
		} while (key == 0);
		if (key == '\r' || key == '\n')
			key = 'Y';
		response = (uint8_t)key;
		qb_compat_upper_n(&response, 1);
		if (!session_present_text(session, &response, 1,
		    SESSION_PRESENT_LINE, "profit pager echo", error))
			return false;
		if (response == 'Y')
			return true;
		if (response == 'N') {
			*stop = true;
			return true;
		}
	}
}

static bool
computer_profit(struct yt_session *session, bool all,
    struct yt_error *error)
{
	float adjacent_source = session->player.sector;
	int source_start = all ? 2 : (int)adjacent_source;
	int source_end = all ? sector_count(session)
	    : (int)adjacent_source;
	int source_number;
	int result_count = 0;
	bool found = false;

	if (all) {
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "global profit leading blank", error))
			return false;
	}
	else {
		session->presentation.foreground = 7.0f;
		session->pager.foreground = 7;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "adjacent profit leading blank", error)
		    || !session_present_text(session,
		    (const uint8_t *)
		    "Profits of a two way trade to ports in adjacent sectors.",
		    strlen("Profits of a two way trade to ports in adjacent sectors."),
		    SESSION_PRESENT_BOLD_LINE, "adjacent profit title", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "adjacent profit title blank", error))
			return false;
	}
	for (source_number = source_start; source_number <= source_end;
	    ++source_number) {
		struct yt_sector source_sector;
		struct yt_port source_port;
		float source_price[4];
		size_t slot;

		if (!yt_game_read_sector(&session->door->game, source_number,
		    &source_sector, error))
			return false;
		if ((!all && (source_sector.port == 0.0f
		    || source_sector.port == 1.0f))
		    || (all && source_sector.port <= 0.0f)) {
			if (!all)
				return session_present_text(session,
				    (const uint8_t *)
				    "NO trading port in your sector!",
				    strlen("NO trading port in your sector!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "adjacent profit no-port row", error);
			continue;
		}
		if (!yt_game_read_port(&session->door->game,
		    (int)source_sector.port, &source_port, error))
			return false;
		if (!profit_project_port_market(session, &source_port,
		    source_price, error))
			return false;
		for (slot = 0; slot < 6; ++slot) {
			int target_number;
			struct yt_sector target_sector;
			struct yt_port target_port;
			float target_price[4];
			bool stop;

			if (!profit_cint(session, source_sector.warps[slot],
			    &target_number, "profit warp target CINT", error))
				return false;
			if (target_number <= 1)
				continue;
			if (!yt_game_read_sector(&session->door->game,
			    target_number, &target_sector, error))
				return false;
			if (target_sector.port == 0.0f
			    || (all && target_number <= source_number))
				continue;
			if (!yt_game_read_port(&session->door->game,
			    (int)target_sector.port, &target_port, error))
				return false;
			if (target_port.commodity_class
			    == source_port.commodity_class)
				continue;
			if (!profit_project_port_market(session, &target_port,
			    target_price, error)
			    || !profit_emit_row(session,
			    all ? (float)source_number : adjacent_source,
			    target_number,
			    &source_port, &target_port, source_price, target_price,
			    all, &result_count, error))
				return false;
			found = true;
			if (all && result_count % 44 == 0) {
				if (!profit_more(session, &stop, error))
					return false;
				if (stop)
					return true;
			}
		}
	}
	if (!all && !found)
		return session_present_text(session,
		    (const uint8_t *)
		    "No ports you can trade with in adjacent sectors!",
		    strlen("No ports you can trade with in adjacent sectors!"),
		    SESSION_PRESENT_BOLD_LINE, "adjacent profit empty row", error);
	if (all) {
		session->presentation.foreground = 7.0f;
		session->pager.foreground = 7;
		return session_present_text(session,
		    (const uint8_t *)" *-[ End of List ]-*",
		    strlen(" *-[ End of List ]-*"), SESSION_PRESENT_BOLD_LINE,
		    "global profit end row", error);
	}
	return true;
}

static bool
computer_activate(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t notice[] = "<Computer activated>";

	session->presentation.foreground = 1.0f;
	session->pager.foreground = 1;
	return session_0317(session, notice, sizeof(notice) - 1U,
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

	session->pager.line_count = 0.0f;
	if (!session_0317(session, heading, sizeof(heading) - 1U,
	    "computer help heading", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "computer help blank", error))
		return false;
	for (index = 0; index < 8U; ++index) {
		if (!session_fixed_width(session, left[index], 40.0f,
		    "computer help left cell", error)
		    || !session_02fc(session, right[index], strlen(
		    (const char *)right[index])))
			return false;
	}
	return session_02fc(session, final, sizeof(final) - 1U);
}

static bool
computer_scoreboard_progress(void *context, unsigned phase,
    struct yt_error *error)
{
	static const uint8_t dot[] = ".";
	struct yt_session *session = context;

	(void)phase;
	return session_present_text(session, dot, sizeof(dot) - 1U,
	    SESSION_PRESENT_RAW, "scoreboard progress dot", error);
}

static void
computer_scoreboard_clear_pager(void *context)
{
	struct yt_session *session = context;

	session->pager.key[0] = '\0';
}

static bool
computer_scoreboard_present(void *context, const uint8_t *text,
    size_t length, enum yt_computer_scoreboard_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (kind == YT_COMPUTER_SCOREBOARD_SELECTOR_PROMPT)
		return session_031f(session, text, length,
		    "scoreboard selector prompt", error);
	if (kind == YT_COMPUTER_SCOREBOARD_UPDATED_HEADING)
		return session_031f(session, text, length,
		    "scoreboard update heading", error);
	return session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    kind == YT_COMPUTER_SCOREBOARD_LEADING_BLANK
	    ? "scoreboard selector leading blank"
	    : kind == YT_COMPUTER_SCOREBOARD_TRAILING_BLANK
	    ? "scoreboard selector trailing blank"
	    : "scoreboard post-generator blank", error);
}

static bool
computer_raw_upper_edit(void *context, char *response, size_t capacity,
    size_t *length, bool *available, struct yt_error *error)
{
	(void)error;
	if (length == NULL || available == NULL)
		return false;
	*available = session_0345(context, response, capacity);
	*length = *available ? strlen(response) : 0U;
	if (*available) {
		struct yt_session *session = context;

		qb_compat_upper_n((uint8_t *)session->output_source, *length);
	}
	return true;
}

static void
computer_scoreboard_reset_pager(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->pager.line_count = qb_mbf32_decode(raw);
}

static bool
computer_scoreboard_generate(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_score_generate_progress(&session->door->game,
	    computer_scoreboard_progress, session, error);
}

static bool
computer_scoreboard_view(void *context, const char *pathname,
    struct yt_error *error)
{
	return display_game_file(context, pathname, error);
}

static bool
computer_scoreboard(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_computer_scoreboard_ops ops = {
		computer_scoreboard_clear_pager,
		computer_scoreboard_present,
		computer_raw_upper_edit,
		computer_scoreboard_reset_pager,
		computer_scoreboard_generate,
		computer_scoreboard_view,
	};
	struct yt_computer_scoreboard_state state = {
		.pathname = session->door->game.config.scoreboard,
	};

	return yt_computer_scoreboard_run(&state, &ops, session, error);
}

static bool
computer_newspaper_present(void *context, const uint8_t *text,
    size_t length, enum yt_computer_newspaper_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (kind == YT_COMPUTER_NEWSPAPER_LEADING_BLANK)
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "newspaper selector leading blank",
		    error);
	return session_031f(session, text, length,
	    "newspaper selector prompt", error);
}

static bool
computer_newspaper_view(void *context, const char *pathname,
    struct yt_error *error)
{
	return display_game_file(context, pathname, error);
}

static bool
computer_newspaper(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_computer_newspaper_ops ops = {
		computer_newspaper_present,
		computer_raw_upper_edit,
		computer_newspaper_view,
	};
	struct yt_computer_newspaper_state state;

	return yt_computer_newspaper_run(&state, &ops, session, error);
}

static void
computer_menu_prompt_effect(void *context,
    enum yt_computer_prompt_effect effect)
{
	static const uint8_t scanner_zero[4] = {0x00, 0x00, 0x03, 0x00};
	struct yt_session *session = context;

	if (effect == YT_COMPUTER_PROMPT_RESET_SCANNER) {
		session->relationship_scratch = 0.0f;
		memcpy(session->relationship_scratch_raw, scanner_zero,
		    sizeof(scanner_zero));
	}
	else if (effect == YT_COMPUTER_PROMPT_SET_FOREGROUND) {
		session->presentation.foreground = 1.0f;
		session->pager.foreground = 1;
	}
}

static bool
computer_menu_prompt_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !computer_prompt_hydrate(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
computer_menu_prompt_present(void *context, const uint8_t *text,
    size_t length, enum yt_computer_prompt_output_kind kind,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (kind == YT_COMPUTER_PROMPT_LEADING_BLANK)
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "computer prompt leading blank", error);
	if (kind == YT_COMPUTER_PROMPT_TEXT)
		return session_031f(session, text, length, "computer prompt",
		    error);
	return false;
}

static bool
computer_menu_prompt_edit(void *context, char *response, size_t capacity,
    size_t *length, bool *available, struct yt_error *error)
{
	(void)error;
	if (length == NULL || available == NULL)
		return false;
	*available = session_0357(context, response, capacity);
	*length = *available ? strlen(response) : 0U;
	return true;
}

static bool
computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const struct yt_computer_prompt_ops prompt_ops = {
		computer_menu_prompt_effect,
		computer_menu_prompt_hydrate,
		computer_menu_prompt_present,
		computer_menu_prompt_edit,
	};

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!computer_activate(session, error))
		return false;
	for (;;) {
		char command[80];
		struct yt_computer_prompt_state prompt = {
			.current_player_record = session->player_record,
			.time_text = (const uint8_t *)session->time.text,
			.time_text_length = session->time.text_length,
			.time_text_capacity = sizeof(session->time.text),
			.response = command,
			.response_capacity = sizeof(command),
		};
		int position;

		if (!yt_computer_prompt_run(&prompt, &prompt_ops, session, error))
			return false;
		if (!prompt.input_available)
			return false;

		if (strcmp(command, "I") == 0) {
			if (!show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "17") == 0) {
			if (!computer_profit(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "!") == 0) {
			if (!command_collect(session, true, error))
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
			if (!computer_owned_fighters(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "12") == 0) {
			if (!command_collect(session, false, error))
				return false;
			continue;
		}
		if (strcmp(command, "13") == 0) {
			if (!computer_owned_planets(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "15") == 0) {
			if (!computer_spies(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "16") == 0) {
			if (!computer_profit(session, true, error))
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

				if (!session_0317(session, off, sizeof(off) - 1U,
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
				if (!computer_scoreboard(session, error))
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
			if (!radio_read(session, 1.0f, error))
				return false;
			continue;
		}
		if (strcmp(command, "8") == 0) {
			if (!computer_newspaper(session, error))
				return false;
			continue;
		}
		if (strcmp(command, "C") == 0) {
			static const uint8_t warning[] =
			    "Don't BREAK the 'ON' button!";

			if (!session_0317(session, warning,
			    sizeof(warning) - 1U,
			    "computer reactivation warning", error)
			    || !computer_activate(session, error))
				return false;
			continue;
		}
		{
			static const uint8_t invalid[] = "Does not compute";

			if (!session_02db(session, invalid, sizeof(invalid) - 1U,
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

	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (!session_0317(session, heading, sizeof(heading) - 1U,
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
	if (!session_0317(session, (const uint8_t *)narrative[0],
	    strlen(narrative[0]), "main help narrative first", error))
		return false;
	for (index = 1; index < sizeof(narrative) / sizeof(narrative[0]);
	    ++index) {
		if (!session_02fc(session, (const uint8_t *)narrative[index],
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
	char returning[sizeof(session->door->identity.system) + 20U];
	int length;

	if (!session->door->game_open)
		return true;
	session->presentation.foreground = 1.0f;
	session->pager.foreground = 1;
	if (!show_ship(session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-Info blank", error)
	    || !session_031f(session, generating, sizeof(generating) - 1U,
	    "normal-exit generating row", error)
	    || !yt_score_generate_progress(&session->door->game,
	    computer_scoreboard_progress, session, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-generator blank", error))
		return false;
	session->pager.nonstop = 1.0f;
	if (!display_game_file(session,
	    session->door->game.config.scoreboard, error))
		return false;
	if (!session->registered) {
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
	return session_02fc(session, (const uint8_t *)returning,
	    (size_t)length);
}

static void
main_prompt_effect(void *context, enum yt_main_prompt_effect effect)
{
	struct yt_session *session = context;

	switch (effect) {
	case YT_MAIN_PROMPT_RESET_PAGER:
		session->pager.line_count = 0.0f;
		break;
	case YT_MAIN_PROMPT_SET_FOREGROUND:
		session->presentation.foreground = 2.0f;
		session->pager.foreground = 2;
		break;
	case YT_MAIN_PROMPT_RESET_SCANNER:
		session->relationship_scratch = 0.0f;
		break;
	}
}

static bool
main_prompt_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session->player_record
	    || !reload_player(session, error))
		return false;
	*player = session->player;
	return true;
}

static bool
main_prompt_present(void *context, const uint8_t *text, size_t length,
    enum yt_main_prompt_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	if (kind == YT_MAIN_PROMPT_LEADING_BLANK)
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "main prompt leading blank", error);
	if (kind == YT_MAIN_PROMPT_TEXT) {
		if (length >= sizeof(session->output_source))
			return false;
		memcpy(session->output_source, text, length);
		session->output_source[length] = '\0';
		return session_031f(session, text, length,
		    "main prompt low-time warning", error);
	}
	return false;
}

static bool
main_prompt_edit(void *context, char *response, size_t capacity,
    size_t *length, bool *available, struct yt_error *error)
{
	(void)error;
	if (length == NULL || available == NULL)
		return false;
	*available = session_0357(context, response, capacity);
	*length = *available ? strlen(response) : 0U;
	return true;
}

static bool
command_shell(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_main_prompt_ops prompt_ops = {
		main_prompt_effect,
		main_prompt_hydrate,
		main_prompt_present,
		main_prompt_edit,
	};

	while (session->running && !session->destroyed) {
		char command[YT_COMMAND_SIZE];
		struct yt_main_prompt_state prompt = {
			.current_player_record = session->player_record,
			.time_text = (const uint8_t *)session->time.text,
			.time_text_length = session->time.text_length,
			.time_text_capacity = sizeof(session->time.text),
			.response = command,
			.response_capacity = sizeof(command),
		};
		enum yt_main_shell_route route;
		bool enter_sector = false;

		if (!yt_main_prompt_run(&prompt, &prompt_ops, session, error))
			return false;
		if (!prompt.input_available)
			return true;
		memcpy(session->output_source, command,
		    prompt.response_length + 1U);
		route = prompt.route;
		switch (route) {
		case YT_MAIN_SHELL_DISPLAY:
			if (!session_0317(session,
			    (const uint8_t *)"<Display>",
			    strlen("<Display>"), "main display heading", error))
				return false;
			if (!display_sector(session, false, error))
				return false;
			continue;
		case YT_MAIN_SHELL_SOUND:
		{
			struct yt_present_result presentation;
			enum yt_present_status status = yt_present_sound_toggle(
			    &session->presentation, &presentation);

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
			session->presentation.foreground = 6.0f;
			session->pager.foreground = 6;
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
			if (!session_02fc(session,
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
			if (!session_02db(session,
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
			if (!session->destroyed
			    && !display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_PLASMA:
			if (!command_projectile(session, true, error))
				return false;
			if (!session->destroyed
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
			session->presentation.foreground = 1.0f;
			session->pager.foreground = 1;
			if (!display_sector(session, false, error))
				return false;
			break;
		case YT_MAIN_SHELL_COLLECT:
			if (!command_collect(session, true, error))
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
		if (enter_sector && session->running && !session->destroyed
		    && !sector_entry(session, error))
			return false;
	}
	return true;
}

bool
yt_session_run(struct yt_door *door, const char *executable_path,
    const struct yt_startup_main_prefix *startup_prefix,
    struct yt_error *error)
{
	struct yt_session session;
	struct yt_random launch_random;
	char first[128];
	char last[128];

	if (door == NULL || startup_prefix == NULL
	    || !startup_prefix->serial_setup_entered
	    || startup_prefix->continuation != 0x0409U) {
		if (error != NULL) {
			error->status = YT_INVALID;
			snprintf(error->operation, sizeof(error->operation),
			    "player session");
		}
		return false;
	}
	memset(&session, 0, sizeof(session));
	session.door = door;
	session.executable_path = executable_path;
	session.startup_prefix = *startup_prefix;
	session.running = true;
	session.presentation.sound.ansi = door->identity.ansi ? -1.0f : 0.0f;
	session.presentation.sound.mode = door->identity.local ? 1.0f : 0.0f;
	session.presentation.sound.user_sound = -1.0f;
	session.presentation.sound.local_sound =
	    door->identity.local ? -1.0f : 0.0f;
	session.presentation.foreground = 7.0f;
	session.pager.foreground = 7;
	yt_random_init(&launch_random);
	if (!yt_random_market_bases(&launch_random, session.market_base, error))
		return false;
	{
		float requested = single_add(single_add(
		    floorf((float)yt_platform_timer()),
		    single_mul(60.0f, (float)door->identity.minutes)), -3.0f);
		float maximum = single_add(
		    floorf((float)yt_platform_timer()), 10800.0f);

		session.session_deadline = requested < maximum
		    ? requested : maximum;
		session.time.deadline = session.session_deadline;
	}
	if (!load_configuration(&session, error))
		return session.terminated;
	session.presentation.foreground = 6.0f;
	session.pager.foreground = 6;
	if (!session_present_text(&session, NULL, 0, SESSION_PRESENT_LINE,
	    "startup pre-title blank", error)
	    || !registration(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	session.presentation.sound.snoop =
	    session.door->game.config.local_screen;
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
	session.sector_cache[session.player_record] = session.player.sector;
	session.cloak_cache[session.player_record] = session.player.cloak;
	if (!post_login(&session, error)
	    || !sector_entry(&session, error))
		return session.terminated;
	if (session.terminated)
		return true;
	if (!session.destroyed && session.running
	    && !command_shell(&session, error))
		return session.terminated;
	if (session.destroyed && !session.fatal_wait_complete) {
		if (!session_wait(&session, 5.0, "common fatal wait", error))
			return false;
		session.fatal_wait_complete = true;
	}
	return quit_session(&session, error);
}
