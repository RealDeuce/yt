#include "yt_session.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_input.h"
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

struct yt_session {
	struct yt_door *door;
	const char *executable_path;
	int player_record;
	struct yt_player player;
	float sector_cache[YT_PLAYER_LAST + 1];
	float cloak_cache[YT_PLAYER_LAST + 1];
	float black_hole[2];
	float market_base[3];
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
	bool mercenaries_hurt;
	float clearance_holds;
	float clearance_fighters;
	float clearance_ground;
	float clearance_shields;
	float counterlaunch_count;
	float current_sector_record;
	float earth_report_seen;
	float relationship_scratch;
	float attack_commitment;
	uint8_t attack_commitment_raw[4];
	bool anti_cloak;
	int spies[3];
	int spy_marker[3];
	int spy_count;
	float avoid[30];
	float computer_path_marker;
	float computer_path_start;
	float current_planet;
	char planet_name[42];
	float current_warps[6];
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
static bool mine_encounter(struct yt_session *session,
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
static bool earth_store(struct yt_session *session, struct yt_error *error);
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

static double
double_div(double left, double right)
{
	volatile double result = left / right;
	return result;
}

static int
player_count(const struct yt_session *session)
{
	return (int)session->door->game.config.sector_offset - 1;
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
computer_prompt_hydrate(struct yt_session *session, struct yt_error *error)
{
	return reload_player(session, error);
}

static bool
append_news(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	(void)session;
	return yt_news_append(text, error);
}

static bool
append_news_bytes(struct yt_session *session, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	(void)session;
	return yt_news_append_bytes(text, length, error);
}

static bool
radio_append_bytes(const uint8_t *text, size_t length, float sender,
    float recipient,
    struct yt_error *error)
{
	char path[512];
	FILE *file;
	struct yt_radio_record record;

	if (!yt_resolve_case_path("YTRMSG.DAT", true, path, sizeof(path),
	    error))
		return false;
	file = fopen(path, "ab");
	if (file == NULL) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "append radio");
			snprintf(error->path, sizeof(error->path), "%s", path);
		}
		return false;
	}
	if (!yt_radio_message_record(&record, text, length, sender, recipient)) {
		(void)fclose(file);
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "construct radio record");
		}
		return false;
	}
	if (fwrite(record.bytes, 1, sizeof(record.bytes), file)
	    != sizeof(record.bytes) || fclose(file) != 0) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			snprintf(error->operation, sizeof(error->operation),
			    "append radio");
			snprintf(error->path, sizeof(error->path), "%s", path);
		}
		return false;
	}
	return true;
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
	session->queue_position = 0;
	session->queue_length = 0;
	session->queue[0] = '\0';
}

static bool
queue_commands(struct yt_session *session, const char *text)
{
	size_t length = strlen(text);
	size_t pending = session->queue_length - session->queue_position;
	size_t index;
	char prior[YT_COMMAND_SIZE];

	if (length + 1U + pending >= sizeof(session->queue))
		return false;
	if (pending > 0)
		memcpy(prior, session->queue + session->queue_position, pending);
	for (index = 0; index < length; ++index)
		session->queue[index] = text[index] == ';' ? '\r' : text[index];
	session->queue[length] = '\r';
	if (pending > 0)
		memcpy(session->queue + length + 1U, prior, pending);
	session->queue[length + 1U + pending] = '\0';
	session->queue_length = length + 1U + pending;
	session->queue_position = 0;
	return true;
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
	return session_line(session, text, size);
}

static bool
session_0357(struct yt_session *session, char *text, size_t size)
{
	if (!session_0345(session, text, size))
		return false;
	qb_compat_upper(text);
	return true;
}

static bool
session_036f(struct yt_session *session, char *text, size_t size)
{
	if (!session_0345(session, text, size))
		return false;
	yt_input_numeric_response(text);
	return true;
}

static bool session_pager_step(struct yt_session *session);

static bool
append_sampled_key(struct yt_session *session,
    const struct yt_input_value *key)
{
	struct yt_b05d_key_state state = {
		.accumulator = session->command_accumulator,
		.accumulator_capacity = sizeof(session->command_accumulator),
		.queue = session->queue,
		.queue_capacity = sizeof(session->queue),
		.queue_position = &session->queue_position,
		.queue_length = &session->queue_length,
		.pager_key = session->pager.key,
		.pager_key_capacity = sizeof(session->pager.key),
	};

	return yt_b05d_process_key(key, &state);
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
session_b05d(struct yt_session *session, const uint8_t *text, size_t length)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	struct yt_input_value sampled;

	if (!session_carrier(session))
		return false;
	if (!yt_input_poll_legacy(&session->input,
	    session->presentation.sound.mode, YT_INPUT_PHASE_B05D, &sampled)
	    || !append_sampled_key(session, &sampled))
		return false;
	status = yt_present_paged_text(text, length, &session->presentation,
	    &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	if (!session_carrier(session))
		return false;
	status = yt_present_paged_finish(session->pager.newline_flag != 0.0f,
	    &session->presentation, &presentation);
	yt_out_present_result(&presentation);
	if (status != YT_PRESENT_OK)
		return false;
	if (!session_pager_step(session))
		return false;
	session->pager.newline_flag = 0.0f;
	return true;
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
session_pager_step(struct yt_session *session)
{
	char response[80];
	int saved_foreground;
	bool notice;

	if (!yt_pager_advance(&session->pager, &session->presentation,
	    &saved_foreground))
		return true;
	if (!session_b05d(session,
	    (const uint8_t *)
	    "[ENTER] for more, [E] to end, or [NS] for Non-stop ",
	    strlen("[ENTER] for more, [E] to end, or [NS] for Non-stop ")))
		return false;
	if (!read_keyboard_line(session, response, sizeof(response)))
		return false;
	notice = yt_pager_accept_response(&session->pager, response,
	    sizeof(response));
	if (notice) {
		if (!session_b05d(session, (const uint8_t *)"Ctrl-X to Stop",
		    strlen("Ctrl-X to Stop")))
			return false;
	}
	yt_pager_complete(&session->pager, &session->presentation,
	    saved_foreground);
	return true;
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
	struct yt_text_file file;
	float saved_foreground = session->presentation.foreground;
	int saved_pager_foreground = session->pager.foreground;

	if (!yt_file_viewer_entry(session->pager.key,
	    &session->pager.line_count, session_file_viewer_entry_present,
	    session, error))
		return false;
	if (!yt_text_read(path, &file, error)) {
		yt_error_clear(error);
		return yt_file_viewer_missing((const uint8_t *)path, strlen(path),
		    session_file_viewer_missing_present,
		    session_file_viewer_missing_news, session, error);
	}
	{
		struct yt_file_viewer_play_state state = {
			&session->presentation.foreground,
			&session->pager.foreground,
			&session->presentation.bold,
			&session->pager.line_count,
			session->pager.key,
			saved_foreground,
			saved_pager_foreground,
		};

		if (!yt_file_viewer_play(file.data, file.length, &state,
		    session_file_viewer_present, session, error)) {
			yt_text_free(&file);
			return false;
		}
	}
	yt_text_free(&file);
	return true;
}

static bool
xannor_victory_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	struct yt_text_file file;
	uint8_t *line;
	size_t cursor = 0U;
	bool ok = true;

	if (!yt_text_read(path, &file, error))
		return false;
	line = malloc(file.length == 0U ? 1U : file.length);
	if (line == NULL) {
		yt_text_free(&file);
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	for (;;) {
		size_t length;
		bool available;

		if (!yt_text_line_input_next(file.data, file.length, &cursor,
		    line, file.length, &length, &available)) {
			ok = false;
			break;
		}
		if (!available)
			break;
		if (!session_present_text(session, line, length,
		    SESSION_PRESENT_LINE, "Xannor victory file row", error)) {
			ok = false;
			break;
		}
	}
	free(line);
	yt_text_free(&file);
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
load_configuration(struct yt_session *session, struct yt_error *error)
{
	struct yt_game *game = &session->door->game;
	int basic;

	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	if (!yt_random_market_bases(&game->random, session->market_base, error))
		return false;
	if (!yt_database_open(&game->database, "YTDATA.DAT", YT_OPEN_UPDATE,
	    error) || !yt_config_load(&game->database, &game->config, error)) {
		yt_database_close(&game->database);
		return false;
	}
	session->door->game_open = true;
	if (game->config.headquarters == 0.0f) {
		game->config.headquarters = 85.0f;
		if (!yt_config_store(&game->database, &game->config, error))
			return false;
	}
	yt_config_normalize_game(&game->config, session->door->identity.local);
	if (player_count(session) > YT_DEFAULT_PLAYER_COUNT
	    || player_count(session) < 1) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "player cache size");
		}
		return false;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)game->config.sector_offset; ++basic) {
		struct yt_player player;

		if (!yt_game_read_player(game, basic, &player, error))
			return false;
		session->sector_cache[basic] = player.sector;
		if (player.cloak < 0.0f || player.cloak > 1.0f) {
			player.cloak = 1.0f;
			if (!yt_game_write_player(game, basic, &player, error))
				return false;
		}
		session->cloak_cache[basic] = player.cloak;
	}
	for (basic = 0; basic < 2; ++basic) {
		float random_value;
		float span = (float)(sector_count(session) - 2);

		if (!yt_random_next(&game->random, &random_value, error))
			return false;
		session->black_hole[basic] =
		    floorf(single_mul(random_value, span)) + 2.0f;
	}
	return yt_database_flush(&game->database, error);
}

struct registration_context {
	struct yt_session *session;
	FILE *file;
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
	FILE *file = context->file;

	if (file == NULL)
		return true;
	context->file = NULL;
	if (fclose(file) != 0)
		return registration_io_error(context, error, YT_IO_ERROR,
		    "close registration file");
	return true;
}

static bool
registration_random_open(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	if (!registration_resolve_path(context, error))
		return false;
	errno = 0;
	context->file = fopen(context->path, "a+b");
	if (context->file == NULL)
		return registration_io_error(context, error, YT_IO_ERROR,
		    "open registration random");
	return true;
}

static bool
registration_file_size(void *opaque, uint64_t *size,
    struct yt_error *error)
{
	struct registration_context *context = opaque;
	long position;

	if (context->file == NULL)
		return registration_io_error(context, error, YT_INVALID,
		    "registration LOF without file");
	if (fseek(context->file, 0L, SEEK_END) != 0
	    || (position = ftell(context->file)) < 0L)
		return registration_io_error(context, error, YT_IO_ERROR,
		    "registration LOF");
	*size = (uint64_t)position;
	return true;
}

static bool
registration_delete_empty(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	return yt_file_delete(context->path, false, error);
}

static bool
registration_sequential_open(void *opaque, struct yt_error *error)
{
	struct registration_context *context = opaque;

	errno = 0;
	context->file = fopen(context->path, "rb");
	if (context->file == NULL)
		return registration_io_error(context, error, YT_IO_ERROR,
		    "open registration input");
	return true;
}

static bool
registration_read_line(void *opaque, uint8_t *data, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct registration_context *context = opaque;
	enum yt_text_stream_line_status status;

	if (context->file == NULL)
		return registration_io_error(context, error, YT_INVALID,
		    "registration LINE INPUT without file");
	status = yt_text_stream_line_input_next(context->file, data, capacity,
	    length);
	if (status == YT_TEXT_STREAM_LINE_EOF)
		return registration_io_error(context, error, YT_EOF,
		    "registration LINE INPUT past end");
	if (status == YT_TEXT_STREAM_LINE_TOO_LONG)
		return registration_io_error(context, error, YT_NO_MEMORY,
		    "registration LINE INPUT string space");
	if (status == YT_TEXT_STREAM_LINE_IO_ERROR)
		return registration_io_error(context, error, YT_IO_ERROR,
		    "registration LINE INPUT");
	return status == YT_TEXT_STREAM_LINE_OK;
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
	struct yt_present_result presentation;
	enum yt_present_status status;

	(void)opaque;
	status = yt_present_forced_local_line(text, length, &presentation);
	if (status == YT_PRESENT_OK) {
		yt_out_present_result(&presentation);
		return true;
	}
	if (error != NULL) {
		error->status = YT_RANGE;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    "registration forced local row");
	}
	return false;
}

static void
registration_close_all(void *opaque)
{
	struct registration_context *context = opaque;

	if (context->file != NULL) {
		(void)fclose(context->file);
		context->file = NULL;
	}
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
	struct registration_context context = {session, NULL, {0}, false};
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
	if (context.file != NULL)
		(void)registration_close_file4(&context, NULL);
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
opening_poll(void *context, bool *local_key, bool *remote_pending)
{
	struct yt_session *session = context;
	struct yt_input_value local = {{0, 0}, 0, 0, false};

	if (!yt_input_poll_source(&session->input, false, &local))
		return false;
	*local_key = local.length != 0U;
	*remote_pending = session->input.remote.position
	    < session->input.remote.length;
	return true;
}

static bool
opening_and_date(struct yt_session *session, struct yt_error *error)
{
	int16_t route[YT_ROUTE_CAPACITY];
	bool found;
	char real_name[258];
	struct yt_present_result presentation;
	enum yt_present_status status;
	enum yt_opening_exit opening_exit;

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
		    session->presentation.sound.snoop, opening_poll, session,
		    &opening_exit, error)
		    || (opening_exit == YT_OPENING_EXIT_EOF
		    && !session_wait(session, 3.0,
		    "ANSI opening EOF wait", error)))
			return false;
		status = yt_present_opening_cleanup(
		    session->presentation.sound.mode,
		    session->presentation.sound.snoop, &presentation);
		if (status != YT_PRESENT_OK) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "startup opening cleanup");
			}
			return false;
		}
		yt_out_present_result(&presentation);
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

static bool
lockout(struct yt_session *session, struct yt_error *error)
{
	char path[512];
	uint8_t live[300];
	size_t live_length;
	uint8_t *data;
	size_t lines_read;
	bool matched;
	FILE *file;
	long size;

	if (!yt_startup_canonical_name(
	    (const uint8_t *)session->door->identity.real_first,
	    strlen(session->door->identity.real_first),
	    (const uint8_t *)session->door->identity.real_last,
	    strlen(session->door->identity.real_last), live, sizeof(live),
	    &live_length))
		return false;
	if (!yt_resolve_case_path("lockout.dat", true, path, sizeof(path),
	    error))
		return false;
	file = fopen(path, "ab");
	if (file == NULL)
		return false;
	if (fclose(file) != 0)
		return false;
	file = fopen(path, "rb");
	if (file == NULL)
		return false;
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
		fclose(file);
		return false;
	}
	if (size == 0) {
		fclose(file);
		return true;
	}
	rewind(file);
	data = malloc((size_t)size);
	if (data == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (fread(data, 1, (size_t)size, file) != (size_t)size
	    || !yt_startup_lockout_scan(data, (size_t)size, live, live_length,
	    &matched, &lines_read)) {
		free(data);
		return false;
	}
	free(data);
	(void)lines_read;
	if (matched) {
		char contact[320];
		static const uint8_t revoked[] =
		    "\aYOUR ACCESS TO THIS GAME HAS BEEN REVOKED!\a";

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "lockout blank", error))
			return false;
		if (!session_present_text(session, revoked,
		    sizeof(revoked) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "lockout revoked row", error))
			return false;
		snprintf(contact, sizeof(contact),
		    "Please contact your sysop %s %s.",
		    session->door->identity.sysop_first,
		    session->door->identity.sysop_last);
		if (!session_present_text(session,
		    (const uint8_t *)contact, strlen(contact),
		    SESSION_PRESENT_BOLD_LINE, "lockout contact row", error))
			return false;
		if (!session_wait(session, 10.0,
		    "lockout denial wait", error))
			return false;
		if (fclose(file) != 0)
			return false;
		session->running = false;
		session->terminated = true;
		return false;
	}
	fclose(file);
	return true;
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

		if (!yt_game_read_player(&session->door->game, basic, &candidate,
		    error)
		    || !yt_player_name_matches(&candidate, (const uint8_t *)full,
		    strlen(full), &matches, error))
			return false;
		if (matches) {
			session->player_record = basic;
			session->player = candidate;
			returning = true;
			break;
		}
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
			if (!yt_news_append_game_full(date, full, error))
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
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
		if (!yt_platform_clock(&now, error))
			return false;
		{
			char date[11];

			yt_format_date(&now, date);
			if (!yt_news_append_new_player(date, full, error))
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
			if (!yt_news_append_login(time_text,
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

static void
radio_io_error(struct yt_error *error, const char *operation,
    const char *path)
{
	if (error == NULL)
		return;
	error->status = YT_IO_ERROR;
	(void)snprintf(error->operation, sizeof(error->operation), "%s",
	    operation);
	(void)snprintf(error->path, sizeof(error->path), "%s", path);
}

static bool
radio_read(struct yt_session *session, float reader_mode,
    struct yt_error *error)
{
	static const uint8_t automatic_heading[] =
	    "Checking for Radio Messages.";
	static const uint8_t log_heading[] =
	    "Log of messages sent/recieved.";
	char path[512];
	FILE *file;
	struct yt_radio_record record;
	struct yt_radio_pager_state private_pager;
	long offset = 0;
	bool visible = false;
	bool read_failed = false;
	bool close_failed;
	bool synthetic = false;
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

	if (!yt_resolve_case_path("YTRMSG.DAT", true, path, sizeof(path),
	    error))
		return false;
	file = fopen(path, "r+b");
	if (file == NULL) {
		file = fopen(path, "w+b");
		if (file == NULL) {
			radio_io_error(error, "open radio reader", path);
			return false;
		}
	}
	for (;;) {
		size_t read_length = fread(record.bytes, 1,
		    sizeof(record.bytes), file);
		float counter;
		float recipient;
		float sender;
		struct yt_radio_reader_decision decision;

		if (read_length != sizeof(record.bytes)) {
			if (read_length == 0 && feof(file) && !synthetic) {
				memset(&record, 0, sizeof(record));
				synthetic = true;
			}
			else {
				read_failed = true;
				break;
			}
		}
		counter = yt_radio_get_number(&record, 0);
		recipient = yt_radio_get_number(&record, 4);
		sender = yt_radio_get_number(&record, 8);
		if (!yt_radio_reader_decide(counter, recipient, sender,
		    (float)session->player_record, reader_mode, &decision, error)) {
			(void)fclose(file);
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
				(void)fclose(file);
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
					(void)fclose(file);
					return false;
				}
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "radio pair blank", error)
				    || !session_present_text(session, header,
				    header_length, SESSION_PRESENT_LINE,
				    "radio pair header", error)) {
					(void)fclose(file);
					return false;
				}
				yt_radio_pager_add_pair(&private_pager);
			}
			if (!session_present_text(session, record.bytes + 12, 74,
			    SESSION_PRESENT_LINE, "radio body", error)) {
				(void)fclose(file);
				return false;
			}
			previous_sender = sender;
			previous_recipient = recipient;
			if (yt_radio_pager_add_body(&private_pager)) {
				if (!session_present_text(session,
				    (const uint8_t *)"[Pause]", strlen("[Pause]"),
				    SESSION_PRESENT_RAW, "radio pause", error)) {
					(void)fclose(file);
					return false;
				}
				if (!session_wait(session, 99.0,
				    "radio private-pager wait", error)) {
					(void)fclose(file);
					return false;
				}
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "radio pause blank", error)) {
					(void)fclose(file);
					return false;
				}
			}
			if (decision.automatic_write) {
				if (!yt_radio_reader_mutate(&record, counter)
				    || fseek(file, offset, SEEK_SET) != 0
				    || fwrite(record.bytes, 1,
				    sizeof(record.bytes), file)
				    != sizeof(record.bytes)
				    || fseek(file, offset
				    + (long)sizeof(record.bytes), SEEK_SET) != 0) {
					radio_io_error(error, "write radio reader", path);
					(void)fclose(file);
					return false;
				}
			}
		}
		if (synthetic)
			break;
		offset += (long)sizeof(record.bytes);
	}
	if (!visible && !session_present_text(session,
	    (const uint8_t *)"None Found.", strlen("None Found."),
	    SESSION_PRESENT_LINE, "radio none found", error)) {
		(void)fclose(file);
		return false;
	}
	close_failed = fclose(file) != 0;
	if (read_failed || close_failed) {
		radio_io_error(error, read_failed ? "read radio reader"
		    : "close radio reader", path);
		return false;
	}
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
port_update(struct yt_session *session, int logical_port,
    struct yt_port *port, float prices[3], double quantities[3],
    struct yt_error *error)
{
	float minute;
	float elapsed;
	int today;
	int adjusted_year;
	size_t index;

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!yt_game_read_port(&session->door->game, logical_port, port, error))
		return false;
	minute = current_minute();
	elapsed = single_add(
	    single_sub((float)session->door->game.today, port->last_day),
	    single_div(single_sub(minute, port->last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	for (index = 0; index < 3; ++index) {
		double capacity = double_add((double)port->stock[index],
		    (double)single_mul(port->production[index], elapsed));
		double production = (double)port->production[index];
		double comparison;
		double numerator;
		double denominator;
		double ratio;
		double scale;
		double raw;

		if (quantities != NULL)
			quantities[index] = capacity;
		comparison = double_div(capacity, 10.0);
		if (comparison > production) {
			port->production[index] =
			    (float)double_div(capacity, 10.0);
			production = (double)port->production[index];
		}
		port->stock[index] = (float)capacity;
		numerator = double_mul((double)port->factor[index], capacity);
		denominator = double_mul(production, 1000.0);
		if (denominator == 0.0) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation),
				    "ordinary port price division");
			}
			return false;
		}
		ratio = double_div(numerator, denominator);
		scale = double_sub(1.0, ratio);
		raw = double_mul((double)session->market_base[index], scale);
		prices[index] = (float)floor(double_add(raw, 0.5));
		if (prices[index] < 1.0f)
			prices[index] = 1.0f;
	}
	port->last_day = (float)session->door->game.today;
	port->last_minute = minute;
	return yt_game_write_port(&session->door->game, logical_port, port,
	    error) && yt_database_flush(&session->door->game.database, error);
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
planet_update_cached_physical(struct yt_session *session,
    uint32_t physical_record,
    struct yt_planet *planet, struct planet_update_cache *cache,
    struct yt_error *error)
{
	float rate[10] = {0};
	float contribution[10] = {0};
	double quantity[10] = {0};
	float elapsed;
	float minute;
	float sum;
	float one_percent = 0.009999999776482582f;
	int today;
	int adjusted_year;
	size_t index;

	if (!yt_current_date_serial(session->door->game.config.epoch_year,
	    &today, &adjusted_year, error))
		return false;
	session->door->game.today = today;
	session->door->game.adjusted_year = adjusted_year;
	if (!read_planet_physical(session, physical_record, planet, error))
		return false;
	for (index = 1; index <= 3; ++index) {
		rate[index] = planet->production[index - 1U];
		quantity[index] = (double)planet->stock[index - 1U];
	}
	quantity[4] = (double)planet->fighters;
	quantity[5] = (double)planet->missiles;
	quantity[6] = (double)planet->mines;
	quantity[7] = (double)planet->bank;
	quantity[8] = (double)planet->ground_forces;
	quantity[9] = (double)planet->plasma;
	sum = single_add(single_add(rate[1], rate[2]), rate[3]);
	rate[4] = floorf(sum);
	sum = single_add(single_add(rate[1], rate[2]), rate[3]);
	rate[5] = floorf(single_div(sum, 2500.0f));
	sum = single_add(single_add(rate[1], rate[2]), rate[3]);
	rate[6] = floorf(single_div(sum, 25000.0f));
	sum = single_add(single_add(rate[1], rate[2]), rate[3]);
	rate[9] = floorf(single_mul(sum, 0.00001f));
	minute = current_minute();
	elapsed = single_add(
	    single_sub((float)today, planet->last_day),
	    single_div(single_sub(minute, planet->last_minute), 1440.0f));
	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	contribution[1] = (float)(quantity[7] / 10000.0);
	contribution[2] = (float)(quantity[7] / 20000.0);
	contribution[3] = (float)(quantity[7] / 30000.0);
	contribution[4] = (float)(quantity[7] / 500.0);
	contribution[5] = (float)(quantity[7] * 0.00001);
	contribution[6] = (float)(quantity[7] * 0.000004);
	contribution[8] = (float)(quantity[7] / 10000.0);
	contribution[9] = (float)(quantity[7] * 0.00000004);
	{
		float fraction = single_mul(elapsed, one_percent);

		quantity[7] = floor(quantity[7]
		    + quantity[7] * (double)fraction);
	}
	quantity[8] = floor(quantity[8]
	    + (double)elapsed * quantity[8] * (double)one_percent
	    + (double)single_mul(contribution[8], elapsed));
	for (index = 1; index <= 3; ++index)
		rate[index] = single_add(rate[index],
		    single_mul(single_mul(rate[index], elapsed), one_percent));
	for (index = 1; index <= 6; ++index) {
		rate[index] = single_add(rate[index], contribution[index]);
		quantity[index] +=
		    (double)single_mul(rate[index], elapsed);
		if (index <= 3
		    && quantity[index]
		    > (double)single_mul(rate[index], 10.0f))
			rate[index] = (float)(quantity[index] / 10.0
			    + (double)contribution[index]);
	}
	rate[9] = single_add(rate[9], contribution[9]);
	quantity[9] += (double)single_mul(rate[9], elapsed);
	for (index = 1; index <= 3; ++index) {
		planet->production[index - 1U] =
		    single_sub(rate[index], contribution[index]);
		planet->stock[index - 1U] = (float)quantity[index];
	}
	planet->fighters = (float)quantity[4];
	planet->missiles = (float)quantity[5];
	planet->mines = (float)quantity[6];
	planet->bank = (float)quantity[7];
	planet->ground_forces = (float)quantity[8];
	planet->plasma = (float)quantity[9];
	planet->last_day = (float)today;
	planet->last_minute = minute;
	memcpy(session->planet_quantity, quantity,
	    sizeof(session->planet_quantity));
	if (cache != NULL) {
		memcpy(cache->rate, rate, sizeof(cache->rate));
		memcpy(cache->quantity, quantity, sizeof(cache->quantity));
		memcpy(cache->contribution, contribution,
		    sizeof(cache->contribution));
	}
	return write_planet_physical(session, physical_record, planet, true,
	    error)
	    && yt_database_flush(&session->door->game.database, error);
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
spy_sweep(struct yt_session *session, struct yt_error *error)
{
	int spy;

	for (spy = 0; spy < session->spy_count; ++spy) {
		int sector_number = session->spies[spy];
		struct yt_sector sector;
		bool found = false;
		int basic;

		if (sector_number == 0)
			continue;
		if (session->spy_marker[spy] != sector_number) {
			if (!yt_game_read_sector(&session->door->game,
			    sector_number, &sector, error))
				return false;
			if ((float)sector_number == session->black_hole[0]
			    || (float)sector_number == session->black_hole[1]) {
				if (!session_sound(session, 9.0f,
				    "spy finding sound", error))
					return false;
				yt_outf("*** RADIO MESSAGE FROM SPY # %d!"
				    " The following was found in sector %d:\r\n",
				    spy + 1, sector_number);
				yt_out_line("Electromagnetic disruption detected.");
				found = true;
			}
			if (sector.mines != 0.0f) {
				if (!found) {
					if (!session_sound(session, 9.0f,
					    "spy finding sound", error))
						return false;
					yt_outf("*** RADIO MESSAGE FROM SPY # %d!"
					    " The following was found in sector"
					    " %d:\r\n", spy + 1,
					    sector_number);
				}
				yt_outf("Sector mines: %.9g\r\n",
				    (double)sector.mines);
				found = true;
			}
			if (sector.planet > 0.0f) {
				struct yt_planet planet;

				if (!planet_update(session, (int)sector.planet,
				    &planet, error))
					return false;
				if (!found) {
					if (!session_sound(session, 9.0f,
					    "spy finding sound", error))
						return false;
					yt_outf("*** RADIO MESSAGE FROM SPY # %d!"
					    " The following was found in sector"
					    " %d:\r\n", spy + 1,
					    sector_number);
				}
				yt_outf("Planet: %s  Ground Forces: %.9g\r\n",
				    planet.name,
				    (double)floorf(planet.ground_forces));
				found = true;
			}
			for (basic = YT_PLAYER_FIRST;
			    basic <= (int)session->door->game.config.sector_offset;
			    ++basic) {
				float draw;

				if (basic == session->player_record
				    || session->sector_cache[basic]
				    != (float)sector_number)
					continue;
				if (!random_value(session, &draw, error))
					return false;
				if (session->cloak_cache[basic] != 0.0f
				    && draw <= session->cloak_cache[basic])
					continue;
				if (!found) {
					if (!session_sound(session, 9.0f,
					    "spy finding sound", error))
						return false;
					yt_outf("*** RADIO MESSAGE FROM SPY # %d!"
					    " The following was found in sector"
					    " %d:\r\n", spy + 1,
					    sector_number);
				}
				if (session->cloak_cache[basic] != 0.0f) {
					if (!session_sound(session, 4.0f,
					    "spy cloak sound", error))
						return false;
					yt_out_line(
					    "A shimmering cloaking device was detected.");
					session->cloak_cache[basic] = 0.0f;
				}
				{
					struct yt_player player;

					if (!yt_game_read_player(
					    &session->door->game, basic,
					    &player, error))
						return false;
					yt_outf("Ship: %s  Team %.9g"
					    "  Fighters %.9g  Shields %.9g\r\n",
					    player.name, (double)player.team,
					    (double)player.fighters,
					    (double)player.shields);
				}
				found = true;
			}
			if (sector.fighters != 0.0f
			    && sector.fighter_owner
			    != (float)session->player_record) {
				if (!found) {
					if (!session_sound(session, 9.0f,
					    "spy finding sound", error))
						return false;
					yt_outf("*** RADIO MESSAGE FROM SPY # %d!"
					    " The following was found in sector"
					    " %d:\r\n", spy + 1,
					    sector_number);
				}
				yt_outf("Deployed fighters: %.9g  Owner %.9g\r\n",
				    (double)sector.fighters,
				    (double)sector.fighter_owner);
				found = true;
			}
			if (found) {
				session->spy_marker[spy] = sector_number;
				if (!session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "spy pause blank", error)
				    || !session_press_any_key(session, true, error))
					return false;
			}
		}
		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
		for (;;) {
			float draw;
			int slot;

			if (!random_value(session, &draw, error))
				return false;
			slot = (int)floorf(single_mul(draw, 6.0f));
			if (sector.warps[slot] != 0.0f) {
				session->spies[spy] = (int)sector.warps[slot];
				break;
			}
		}
	}
	return true;
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
command_move(struct yt_session *session, bool *moved,
    struct yt_error *error)
{
	static const uint8_t prompt[] = "Move to which sector? ";
	static const uint8_t same_sector[] =
	    "That was quick! Felt like we didn't even move!";
	static const uint8_t not_adjacent[] =
	    "You can't get there from here.";
	char line[YT_COMMAND_SIZE];
	float target;
	float maximum;
	struct qb_val_result parsed;
	uint8_t row[256];
	size_t row_length;
	size_t slot;
	bool adjacent = false;
	bool danger;
	bool denied;

	if (moved == NULL)
		return false;
	*moved = false;
	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!yt_movement_warp_row(session->current_warps, row,
	    sizeof(row), &row_length)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "movement warp row");
		}
		return false;
	}
	if (!session_0317(session, row, row_length, "movement warp row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "movement post-warp blank", error))
		return false;
	for (;;) {
		if (!session_031f(session, prompt, sizeof(prompt) - 1U,
		    "movement destination prompt", error)
		    || !session_036f(session, line, sizeof(line)))
			return false;
		if (strcmp(line, "M") == 0)
			continue;
		parsed = qb_val(line);
		target = (float)(parsed.valid ? parsed.value : 0.0);
		break;
	}
	maximum = single_sub(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);
	if (target < 1.0f || target > maximum)
		return true;
	if (target == session->player.sector) {
		return session_02db(session, same_sector,
		    sizeof(same_sector) - 1U, "movement same-sector row", error);
	}
	for (slot = 0; slot < YT_ARRAY_LEN(session->current_warps); ++slot) {
		if (session->current_warps[slot] == target) {
			adjacent = true;
			break;
		}
	}
	if (!adjacent)
		return session_02db(session, not_adjacent,
		    sizeof(not_adjacent) - 1U, "movement not-adjacent row", error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "movement accepted blank", error))
		return false;
	if (session->player.danger_scanner != 0.0f) {
		if (!dangerous_destination(session, target, &danger, error))
			return false;
		if (danger) {
			enum yt_yes_no_answer answer;

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "danger confirmation blank", error))
				return false;
			clear_queue(session);
			if (!yt_movement_confirmation_prompt(target, row,
			    sizeof(row), &row_length)
			    || !session_a8d2(session, row, row_length, &answer, error))
				return false;
			if (answer != YT_YES_NO_YES)
				return true;
		}
	}
	if (!finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	session->suppress_self_mines = false;
	if (!reload_player(session, error))
		return false;
	yt_movement_player_overlay(&session->player, target);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->sector_cache[session->player_record] = target;
	*moved = true;
	return true;
}

static bool
team_remove_player(struct yt_session *session, int victim,
    struct yt_error *error)
{
	struct yt_player player;
	int team_id;
	struct yt_sector overlay;

	if (!yt_game_read_player(&session->door->game, victim, &player, error))
		return false;
	team_id = (int)player.team;
	if (team_id == 0)
		return true;
	if (team_id < 1 || team_id > YT_DEFAULT_PLAYER_COUNT) {
		player.team = 0.0f;
		return yt_game_write_player(&session->door->game, victim,
		    &player, error);
	}
	if (!yt_game_read_sector(&session->door->game, team_id, &overlay,
	    error))
		return false;
	yt_death_team_roster_overlay(&overlay.record, (float)victim);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team_id), &overlay.record, error)
	    || !yt_game_read_player(&session->door->game, victim, &player,
	    error))
		return false;
	player.team = 0.0f;
	return yt_game_write_player(&session->door->game, victim, &player,
	    error);
}

static bool
kill_player(struct yt_session *session, int victim_record,
    float killer, struct yt_error *error)
{
	struct yt_player victim;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t current_name[YT_TEXT_FIELD_SIZE];
	size_t victim_name_length;
	size_t current_name_length;
	int logical;
	int matched_ports = 0;
	float old_ports_owned;
	bool valid_killer = killer >= 2.0f
	    && killer <= session->door->game.config.sector_offset
	    && killer != (float)victim_record;

	if (!yt_game_read_player(&session->door->game, victim_record, &victim,
	    error)
	    || !yt_player_stored_name(&victim, victim_name,
	    &victim_name_length, error)
	    || !yt_player_stored_name(&session->player, current_name,
	    &current_name_length, error))
		return false;
	old_ports_owned = victim.ports_owned;
	session->sector_cache[victim_record] = 0.0f;
	yt_death_player_overlay(&victim, killer);
	if (!yt_game_write_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	for (logical = 1; logical <= sector_count(session); ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&session->door->game, logical, &sector,
		    error))
			return false;
		if (yt_death_sector_overlay(&sector, (float)victim_record)) {
			if (!yt_game_write_sector(&session->door->game, logical,
			    &sector, error))
				return false;
		}
	}
	if (!team_remove_player(session, victim_record, error))
		return false;
	for (logical = 1; old_ports_owned != 0.0f
	    && logical <= port_count(session); ++logical) {
		struct yt_port port;
		enum yt_death_port_route route;

		if (!yt_game_read_port(&session->door->game, logical, &port,
		    error))
			return false;
		route = yt_death_port_overlay(&port, (float)victim_record,
		    killer, session->door->game.config.sector_offset);
		if (route == YT_DEATH_PORT_UNMATCHED)
			continue;
		++matched_ports;
		if (!yt_game_write_port(&session->door->game, logical, &port,
		    error))
			return false;
	}
	if (valid_killer && matched_ports > 0) {
		struct yt_player attacker;
		uint8_t row[300];
		size_t row_length;

		if (!yt_death_title_row(victim_name, victim_name_length,
		    (float)matched_ports, row, sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "death title row", error))
			return false;

		if (!yt_game_read_player(&session->door->game, (int)killer,
		    &attacker, error))
			return false;
		yt_death_killer_credit_overlay(&attacker, (float)matched_ports);
		if (!yt_game_write_player(&session->door->game, (int)killer,
		    &attacker, error))
			return false;
	}
	{
		bool self = killer == (float)victim_record;
		uint8_t news[300];
		size_t news_length;

		if (!self) {
			struct yt_player final_victim;

			if (!yt_game_read_player(&session->door->game, victim_record,
			    &final_victim, error))
				return false;
		}
		if (!yt_death_kill_news_row(current_name, current_name_length,
		    victim_name, victim_name_length, self, news, sizeof(news),
		    &news_length)
		    || !append_news_bytes(session, news, news_length, error))
			return false;
		if (matched_ports > 0 && !self) {
			if (!yt_death_port_news_row(victim_name, victim_name_length,
			    (float)matched_ports, news, sizeof(news), &news_length)
			    || !append_news_bytes(session, news, news_length, error))
				return false;
		}
	}
	if (victim_record == session->player_record) {
		session->player = victim;
		session->destroyed = true;
	}
	if (!yt_database_flush(&session->door->game.database, error))
		return false;
	if (victim_record == session->player_record) {
		if (!session_wait(session, 5.0, "common fatal wait", error))
			return false;
		session->fatal_wait_complete = true;
	}
	return true;
}

static bool
common_fatal_self(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t notice[] = "Your ship has been destroyed!";

	session->pager.foreground = 3;
	session->presentation.foreground = 3.0f;
	if (!session_02db(session, notice, sizeof(notice) - 1U,
	    "common fatal notice", error)
	    || !reload_player(session, error)
	    || !session_sound(session, 3.0f, "fatal destruction sound", error))
		return false;
	return kill_player(session, session->player_record,
	    (float)session->player_record, error);
}

static bool
salvage_player(struct yt_session *session, int victim_record, float killer,
    struct yt_error *error)
{
	static const uint8_t title[] =
	    "You destroyed the ship and salvaged the following:";
	static const uint8_t nothing[] = "  -  NOTHING!";
	struct yt_player victim_storage;
	const struct yt_player *victim = &victim_storage;
	float draw[6];
	float awards[6];
	float cargo_awards[4] = {0};
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t current_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[300];
	size_t victim_name_length;
	size_t current_name_length;
	size_t row_length;
	bool emitted = false;
	int index;

	if (!yt_game_read_player(&session->door->game, victim_record,
	    &victim_storage, error))
		return false;
	if (killer < (float)YT_PLAYER_FIRST
	    || killer > session->door->game.config.sector_offset)
		return true;
	if (!yt_player_stored_name(victim, victim_name, &victim_name_length,
	    error)
	    || !yt_player_stored_name(&session->player, current_name,
	    &current_name_length, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage opening blank", error)
	    || !session_present_text(session, title, sizeof(title) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "salvage title", error)
	    || !yt_salvage_header_row(current_name, current_name_length,
	    victim_name, victim_name_length, row, sizeof(row), &row_length)
	    || !append_news_bytes(session, row, row_length, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage post-header blank", error))
		return false;
	for (index = 0; index < 6; ++index) {
		if (!random_value(session, &draw[index], error))
			return false;
	}
	awards[0] = floorf(single_mul(draw[0], victim->holds));
	awards[1] = floorf(single_mul(draw[1], victim->credits));
	awards[2] = floorf(single_mul(draw[2], victim->missiles));
	awards[3] = floorf(single_mul(draw[3], victim->plasma));
	awards[4] = floorf(single_mul(draw[4], victim->ground_forces));
	awards[5] = floorf(single_mul(draw[5], victim->mines));
	if (!session_wait(session, 1.0, "salvage initial wait", error))
		return false;
	if (!reload_player(session, error))
		return false;
	{
		float *const fields[5] = {
			&session->player.credits, &session->player.missiles,
			&session->player.plasma, &session->player.ground_forces,
			&session->player.mines
		};

		for (index = 1; index < 6; ++index) {
			if (awards[index] == 0.0f)
				continue;
			if (!session_wait(session, 0.5,
			    "salvage simple-award wait", error))
				return false;
			emitted = true;
			if (!yt_salvage_simple_row(
			    (enum yt_salvage_simple_kind)(index - 1), awards[index],
			    row, sizeof(row), &row_length)
			    || !append_news_bytes(session, row, row_length, error)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "salvage simple row", error))
				return false;
			*fields[index - 1] = single_add(*fields[index - 1],
			    awards[index]);
		}
	}
	if (!write_player(session, error))
		return false;
	if (session->player.holds + awards[0]
	    > session->door->game.config.maximum_holds)
		awards[0] = session->door->game.config.maximum_holds
		    - session->player.holds;
	/*
	 * Empty holds are real salvage.  Commodity selection is performed
	 * without replacement from a working copy of the victim's hold
	 * population.
	 */
	if (awards[0] > 0.0f) {
		struct yt_salvage_cargo_state cargo = {
			awards[0],
			{victim->ore, victim->organics, victim->equipment},
			victim->holds,
			{0},
		};

		emitted = true;
		if (!yt_salvage_cargo_sample(&cargo, session_random_one_based,
		    session, error))
			return false;
		memcpy(cargo_awards, cargo.awards, sizeof(cargo_awards));
		if (!reload_player(session, error))
			return false;
		for (index = 0; index < 4; ++index)
			session->player.holds = single_add(session->player.holds,
			    cargo_awards[index]);
		session->player.ore = single_add(session->player.ore,
		    cargo_awards[0]);
		session->player.organics = single_add(session->player.organics,
		    cargo_awards[1]);
		session->player.equipment = single_add(session->player.equipment,
		    cargo_awards[2]);
		if (!write_player(session, error))
			return false;
		if (!session_wait(session, 0.5,
		    "salvage post-cargo wait", error))
			return false;
		{
			static const int order[4] = {3, 0, 1, 2};
			static const enum yt_salvage_cargo_kind row_kind[4] = {
				YT_SALVAGE_EMPTY_HOLDS, YT_SALVAGE_ORE,
				YT_SALVAGE_ORGANICS, YT_SALVAGE_EQUIPMENT
			};

			for (index = 0; index < 4; ++index) {
				int award_kind = order[index];

				if (cargo_awards[award_kind] <= 0.0f)
					continue;
				if (!session_wait(session, 0.5,
				    "salvage cargo-row wait", error))
					return false;
				if (!yt_salvage_cargo_row(row_kind[index],
				    cargo_awards[award_kind], row, sizeof(row), &row_length)
				    || !append_news_bytes(session, row, row_length, error)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_LINE, "salvage cargo row", error))
					return false;
			}
		}
	}
	if (!emitted) {
		if (!session_wait(session, 0.5, "salvage nothing wait", error))
			return false;
		if (!append_news_bytes(session, nothing, sizeof(nothing) - 1U,
		    error)
		    || !session_present_text(session, nothing,
		    sizeof(nothing) - 1U, SESSION_PRESENT_LINE,
		    "salvage nothing row", error))
			return false;
	}
	return session_wait(session, 4.0, "salvage final wait", error);
}

static bool
combat_attrition(struct yt_session *session, double committed,
    double defenders, float cloak, double *attacker_loss,
    double *defender_loss, struct yt_error *error)
{
	double lost_attacker = 0.0;
	double lost_defender = 0.0;

	while (lost_attacker < committed && lost_defender < defenders) {
		double remaining_attacker = committed - lost_attacker;
		double remaining_defender = defenders - lost_defender;
		double minimum = remaining_attacker < remaining_defender
		    ? remaining_attacker : remaining_defender;
		float quantum = (float)floor(minimum / 20.0);
		float draw;

		if (quantum < 1.0f)
			quantum = 1.0f;
		if (!random_value(session, &draw, error))
			return false;
		if (single_add(single_div(cloak, 10.0f), draw)
		    < 0.44999998807907104f)
			lost_attacker += (double)quantum;
		else
			lost_defender += (double)quantum;
	}
	*attacker_loss = lost_attacker > committed ? committed : lost_attacker;
	*defender_loss = lost_defender > defenders ? defenders : lost_defender;
	return true;
}

static bool
xannor_victory(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t pause[] = "[PAUSE]";
	static const uint8_t bonus[] =
	    "Collect 16,000,000 credit bonus!";
	uint8_t player_name[YT_TEXT_FIELD_SIZE];
	uint8_t winner[128];
	uint8_t banner[79];
	size_t player_name_length;
	size_t winner_length;
	struct yt_sector overlay;

	session->presentation.foreground = 7.0f;
	session->pager.foreground = 7;
	if (!xannor_victory_file(session, "XannorHQ.TXT", error))
		return false;
	if (!session_present_text(session, pause, sizeof(pause) - 1U,
	    SESSION_PRESENT_RAW, "Xannor victory pause", error)
	    || !session_wait(session, 99.0, "Xannor victory wait", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Xannor victory post-wait blank", error))
		return false;
	session->presentation.blink = 1.0f;
	if (!session_present_text(session, bonus, sizeof(bonus) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "Xannor victory bonus", error))
		return false;
	clear_queue(session);
	if (!reload_player(session, error))
		return false;
	session->player.credits = floorf(single_add(session->player.credits,
	    16000000.0f));
	if (!write_player(session, error))
		return false;
	for (int ordinal = 0; ordinal < 3; ++ordinal) {
		if (!session_sound(session, 2.0f,
		    "Xannor victory sound", error))
			return false;
	}
	memset(banner, '*', 79);
	if (!yt_player_stored_name(&session->player, player_name,
	    &player_name_length, error)
	    || !yt_xannor_victory_winner(player_name, player_name_length,
	    winner, sizeof(winner), &winner_length)
	    || !append_news_bytes(session, banner, sizeof(banner), error)
	    || !append_news_bytes(session, winner, winner_length, error)
	    || !append_news_bytes(session, banner, sizeof(banner), error)
	    || !radio_append_bytes(banner, sizeof(banner), -2.0f, -2.0f,
	    error)
	    || !radio_append_bytes(winner, winner_length, -2.0f, -2.0f,
	    error)
	    || !radio_append_bytes(banner, sizeof(banner), -2.0f, -2.0f,
	    error))
		return false;
	if (!yt_game_read_sector(&session->door->game, 21, &overlay, error))
		return false;
	overlay.metadata = (float)session->player_record;
	return yt_game_write_sector(&session->door->game, 21, &overlay,
	    error);
}

static bool
attack_player(struct yt_session *session, int target_record,
    double committed, struct yt_error *error)
{
	struct yt_player target;
	double attacker_loss;
	double defender_loss;
	double attacking;
	double defenders;
	double cached_reserve;
	float victim_mines;
	float current_sector;
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t line[300];
	uint8_t second_line[300];
	size_t victim_name_length;
	size_t line_length;
	size_t second_line_length;

	if (!yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	defenders = (double)target.fighters;
	if (!reload_player(session, error))
		return false;
	if (committed > (double)session->player.fighters) {
		if (!yt_direct_attack_too_many_row(
		    (double)session->player.fighters, line, sizeof(line),
		    &line_length)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "direct Attack too-many row");
			}
			return false;
		}
		return session_02db(session, line, line_length,
		    "direct Attack too-many row", error);
	}
	cached_reserve = (double)session->player.fighters - committed;
	yt_direct_attack_fighter_overlay(&session->player,
	    (float)cached_reserve);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "player attack opening sound", error))
		return false;
	if (!combat_attrition(session, committed, defenders,
	    session->player.cloak, &attacker_loss, &defender_loss, error))
		return false;
	if (defender_loss > 0.0) {
		uint8_t stored_name[YT_TEXT_FIELD_SIZE];
		uint8_t radio_text[160];
		size_t name_length;
		size_t radio_length;

		if (!yt_player_stored_name(&session->player, stored_name,
		    &name_length, error)
		    || !yt_direct_attack_radio_text(stored_name, name_length,
		    (double)defender_loss, radio_text, sizeof(radio_text),
		    &radio_length)
		    || !radio_append_bytes(radio_text, radio_length, -2.0f,
		    (float)target_record, error))
			return false;
	}
	if (!reload_player(session, error))
		return false;
	cached_reserve = (double)session->player.fighters;
	attacking = committed - attacker_loss;
	defenders -= defender_loss;
	yt_direct_attack_fighter_overlay(&session->player,
	    (float)(cached_reserve + attacking));
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	current_sector = session->player.sector;
	yt_direct_attack_fighter_overlay(&target, (float)defenders);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)target_record, &target.record, error))
		return false;
	if (!yt_direct_attack_result_rows(attacker_loss, cached_reserve,
	    defender_loss, defenders, line, sizeof(line), &line_length,
	    second_line, sizeof(second_line), &second_line_length)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "direct Attack result rows");
		}
		return false;
	}
	if (!session_0317(session, line, line_length,
	    "direct Attack attacker result", error)
	    || !session_02fc(session, second_line, second_line_length))
		return false;
	if (defenders > 0.0 || attacking < 1.0)
		return true;
	{
		static const uint8_t eliminated[] =
		    "Fighters eliminated! Attacking the ship!";

		if (!session_0317(session, eliminated,
		    sizeof(eliminated) - 1U,
		    "direct Attack eliminated row", error))
			return false;
	}
	if (target.shields > 0.0f
	    && !fighter_shield_spill(session, &attacking, &target.shields,
	    error))
		return false;
	{
		float remaining_shields = target.shields;

		if (!yt_game_read_player(&session->door->game, target_record,
		    &target, error))
			return false;
		yt_direct_attack_shield_overlay(&target, remaining_shields);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)target_record, &target.record, error)
		    || !reload_player(session, error))
			return false;
	}
	yt_direct_attack_fighter_overlay(&session->player,
	    (float)(cached_reserve + attacking));
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error))
		return false;
	if (target.shields > 0.0f)
		return true;
	if (!session_sound(session, 3.0f, "player kill sound", error)
	    || !yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	victim_mines = target.mines;
	if (!port_report_length(session, target.name_length,
	    YT_TEXT_FIELD_SIZE, &victim_name_length,
	    "direct fighter victim name length", error))
		return false;
	if (victim_name_length != 0U)
		memcpy(victim_name, target.record.bytes, victim_name_length);
	if (!kill_player(session, target_record,
	    (float)session->player_record, error))
		return false;
	if (!salvage_player(session, target_record,
	    (float)session->player_record, error))
		return false;
	if (!(victim_mines > 0.0f))
		return true;
	{
		struct yt_sector sector;
		bool overflow;
		int current = (int)qb_cint_mode((double)current_sector,
		    session->presentation.sound.conversion_mode, &overflow);

		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "direct fighter sector record CINT");
			}
			return false;
		}
		if (!yt_game_read_sector(&session->door->game, current, &sector,
		    error))
			return false;
		yt_sector_mine_sector_overlay(&sector,
		    single_add(sector.mines, victim_mines));
		if (!yt_database_write(&session->door->game.database,
		    (size_t)yt_sector_basic_record(&session->door->game.config,
		    current), &sector.record, error))
			return false;
	}
	if (!yt_direct_fighter_mine_warning(victim_name, victim_name_length,
	    line, sizeof(line), &line_length)
	    || !session_02db(session, line, line_length,
	    "direct fighter mine warning", error)
	    || !append_news_bytes(session, line, line_length, error)
	    || !mine_encounter(session, error))
		return false;
	if (session->destroyed)
		return common_fatal_self(session, error);
	return true;
}

static bool
command_attack_player(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Attack>";
	static const uint8_t no_fighters[] =
	    "You don't have any fighters.";
	static const uint8_t none_visible[] = "There's no one here!";
	static const uint8_t none_selected[] =
	    "There are no other ships in this sector.";
	int basic;
	bool encountered = false;
	uint8_t row[300];
	size_t row_length;

	if (enter_sector == NULL)
		return false;
	*enter_sector = false;
	if (!session_02fc(session, title, sizeof(title) - 1U)
	    || !reload_player(session, error))
		return false;
	if (session->player.fighters < 1.0f) {
		return session_02db(session, no_fighters,
		    sizeof(no_fighters) - 1U, "direct Attack no-fighters row",
		    error);
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		struct yt_player target;
		enum yt_yes_no_answer answer;
		double committed;
		char response[YT_COMMAND_SIZE];
		struct qb_val_result parsed;
		uint8_t target_name[YT_TEXT_FIELD_SIZE];
		size_t target_name_length;

		if (basic == session->player_record
		    || session->sector_cache[basic] != session->player.sector
		    || session->cloak_cache[basic] > 0.0f)
			continue;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error)
		    || !yt_player_stored_name(&target, target_name,
		    &target_name_length, error))
			return false;
		if (target.team > 0.0f && target.team == session->player.team) {
			if (!yt_direct_attack_team_row(target_name,
			    target_name_length, row, sizeof(row), &row_length)
			    || !session_02fc(session, row, row_length))
				return false;
			encountered = true;
			continue;
		}
		encountered = true;
		if (!yt_direct_attack_candidate_prompt(target_name,
		    target_name_length, row, sizeof(row), &row_length)
		    || !session_a8d2(session, row, row_length, &answer, error))
			return false;
		if (answer == YT_YES_NO_NO)
			continue;
		if (!yt_direct_attack_commitment_prompt(
		    (double)session->player.fighters, row, sizeof(row),
		    &row_length)
		    || !session_031f(session, row, row_length,
		    "direct Attack commitment prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		parsed = qb_val(response);
		committed = parsed.valid ? parsed.value : 0.0;
		if (committed < 1.0)
			return true;
		return attack_player(session, basic, committed, error);
	}
	*enter_sector = true;
	if (encountered)
		return session_02fc(session, none_selected,
		    sizeof(none_selected) - 1U);
	return session_02db(session, none_visible, sizeof(none_visible) - 1U,
	    "direct Attack no-visible-target row", error);
}

static bool
fighter_shield_spill(struct yt_session *session, double *fighters,
    float *shields, struct yt_error *error)
{
	uint8_t fighter_row[128];
	uint8_t shield_row[128];
	size_t fighter_length;
	size_t shield_length;

	while (*fighters > 0.0 && *shields > 0.0f) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		if (!yt_fighter_shield_spill_step(fighters, shields, draw))
			return false;
	}
	if (!yt_fighter_shield_spill_rows(*fighters, *shields,
	    fighter_row, sizeof(fighter_row), &fighter_length,
	    shield_row, sizeof(shield_row), &shield_length)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "fighter/shield spill rows");
		}
		return false;
	}
	return session_present_text(session, fighter_row, fighter_length,
	    SESSION_PRESENT_LINE, "fighter spill result", error)
	    && session_present_text(session, shield_row, shield_length,
	    SESSION_PRESENT_LINE, "shield spill result", error);
}

static bool
attack_deployed_committed(struct yt_session *session,
    struct yt_sector *sector, double commitment, bool allow_surrender,
    struct yt_error *error)
{
	double attacker_loss = 0.0;
	double defender_loss = 0.0;
	double old_count = (double)sector->fighters;
	double old_ship;
	double ship_fighters;
	double deployed_remaining;
	float old_owner;
	bool surrender_checked = false;
	bool surrendered = false;
	struct yt_sector opened_sector;
	struct yt_sector persisted_sector;
	uint8_t owner_name[160];
	char number_one[64];
	char number_two[64];
	char loss_row[128];
	char destroyed_row[128];
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	size_t cached_player_name_length;
	char cached_player_name_text[sizeof(session->player.name)];

	if (!yt_player_stored_name(&session->player, cached_player_name,
	    &cached_player_name_length, error))
		return false;
	(void)snprintf(cached_player_name_text,
	    sizeof(cached_player_name_text), "%s", session->player.name);

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &opened_sector, error))
		return false;
	old_owner = opened_sector.fighter_owner;
	if (!reload_player(session, error))
		return false;
	(void)snprintf(session->player.name, sizeof(session->player.name),
	    "%s", cached_player_name_text);
	old_ship = (double)session->player.fighters;
	if (!session_sound(session, 2.0f,
	    "deployed attack opening sound", error))
		return false;
	do {
		double remaining_attacker = commitment - attacker_loss;
		double remaining_defender = old_count - defender_loss;
		float quantum = yt_hostile_attack_quantum(remaining_attacker,
		    remaining_defender);
		float sample;

		if (remaining_defender == 0.0) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "attack:surrender-ratio-divide");
			}
			return false;
		}
		if (!surrender_checked && allow_surrender
		    && remaining_attacker / remaining_defender > 10.0) {
			static const uint8_t radio[] = "RADIO MESSAGE COMING IN!";
			static const uint8_t wish[] = "WE WISH TO SURRENDER!!!";
			static const uint8_t surrender_prompt[] =
			    "Will you accept our surrender? [Y]/N -=>";
			char sector_number[64];
			char captain[160];

			if (!reload_player(session, error))
				return false;
			(void)snprintf(session->player.name,
			    sizeof(session->player.name), "%s",
			    cached_player_name_text);
			old_ship = (double)session->player.fighters;
			if (qb_str_single(sector_number, sizeof(sector_number),
			    session->player.sector) < 0
			    || snprintf(captain, sizeof(captain),
			    "This is the captain of the fighter group in sector%s",
			    sector_number) < 0
			    || !session_0317(session, radio, sizeof(radio) - 1U,
			    "surrender radio row", error)
			    || !session_sound(session, 4.0f,
			    "surrender radio sound", error)
			    || !session_0317(session, (const uint8_t *)captain,
			    strlen(captain), "surrender captain row", error))
				return false;
			switch (yt_hostile_surrender_route(old_owner)) {
			case YT_HOSTILE_SURRENDER_PLAYER:
			{
				enum yt_yes_no_answer answer;

				if (!session_02db(session, wish, sizeof(wish) - 1U,
				    "surrender wish row", error)
				    || !session_present_text(session, NULL, 0,
				    SESSION_PRESENT_LINE, "surrender prompt blank", error)
				    || !session_a8d2(session, surrender_prompt,
				    sizeof(surrender_prompt) - 1U, &answer, error))
					return false;
				surrendered = answer == YT_YES_NO_YES
				    || answer == YT_YES_NO_EMPTY;
				break;
			}
			case YT_HOSTILE_SURRENDER_XANNOR:
			{
				static const uint8_t refusal[] =
				    "Whee fyte to the deeth hoo-man slyme!";

				if (!session_02fc(session, refusal,
				    sizeof(refusal) - 1U)
				    || !session_sound(session, 5.0f,
				    "Xannor surrender refusal sound", error))
					return false;
				break;
			}
			case YT_HOSTILE_SURRENDER_MERCENARY:
			{
				char refusal[256];
				int written = snprintf(refusal, sizeof(refusal),
				    "We'll DIE before joining with a slyme like you %s!",
				    session->door->identity.real_first);

				if (written < 0 || (size_t)written >= sizeof(refusal)
				    || !session_02fc(session,
				    (const uint8_t *)refusal, (size_t)written)
				    || !session_sound(session, 5.0f,
				    "Mercenary surrender refusal sound", error))
					return false;
				break;
			}
			case YT_HOSTILE_SURRENDER_QUIET:
				break;
			}
			surrender_checked = true;
			if (surrendered)
				break;
		}
		if (!random_value(session, &sample, error))
			return false;
		if (yt_hostile_attack_loses_attacker(session->player.cloak,
		    sample))
			attacker_loss = double_add(attacker_loss, (double)quantum);
		else
			defender_loss = double_add(defender_loss, (double)quantum);
	} while (attacker_loss < commitment && defender_loss < old_count);
	if (attacker_loss > commitment)
		attacker_loss = commitment;
	if (defender_loss > old_count)
		defender_loss = old_count;
	if (surrendered) {
		static const uint8_t joined[] = " We join your forces!";
		double survivors = double_sub(old_count, defender_loss);
		uint8_t surrender_news[320];
		uint8_t surrendered_row[128];
		size_t surrender_news_length;
		size_t surrendered_row_length;
		int surrendered_number_length;
		int sector_number_length;

		if (!session_0317(session, joined, sizeof(joined) - 1U,
		    "surrender joined row", error)
		    || !session_sound(session, 1.0f,
		    "surrender acceptance sound", error))
			return false;
		surrendered_number_length = qb_str_double(number_one,
		    sizeof(number_one), survivors);
		sector_number_length = qb_str_single(number_two,
		    sizeof(number_two), session->player.sector);
		if (surrendered_number_length < 0 || sector_number_length < 0)
			return false;
		surrender_news_length = (size_t)surrendered_number_length
		    + sizeof(" fighters in sector") - 1U
		    + (size_t)sector_number_length
		    + sizeof(" surrendered to ") - 1U
		    + cached_player_name_length;
		surrendered_row_length = (size_t)surrendered_number_length
		    + sizeof(" fighters surrendered!") - 1U;
		if (surrender_news_length > sizeof(surrender_news)
		    || surrendered_row_length > sizeof(surrendered_row))
			return false;
		memcpy(surrender_news, number_one,
		    (size_t)surrendered_number_length);
		memcpy(surrender_news + (size_t)surrendered_number_length,
		    " fighters in sector", sizeof(" fighters in sector") - 1U);
		memcpy(surrender_news + (size_t)surrendered_number_length
		    + sizeof(" fighters in sector") - 1U, number_two,
		    (size_t)sector_number_length);
		memcpy(surrender_news + (size_t)surrendered_number_length
		    + sizeof(" fighters in sector") - 1U
		    + (size_t)sector_number_length, " surrendered to ",
		    sizeof(" surrendered to ") - 1U);
		if (cached_player_name_length != 0U)
			memcpy(surrender_news + surrender_news_length
			    - cached_player_name_length, cached_player_name,
			    cached_player_name_length);
		memcpy(surrendered_row, number_one,
		    (size_t)surrendered_number_length);
		memcpy(surrendered_row + (size_t)surrendered_number_length,
		    " fighters surrendered!",
		    sizeof(" fighters surrendered!") - 1U);
		if (!append_news_bytes(session, surrender_news,
		    surrender_news_length, error))
			return false;
		ship_fighters = double_add(
		    double_sub(old_ship, attacker_loss), survivors);
		session->player.fighters = (float)ship_fighters;
		deployed_remaining = 0.0;
		sector->fighter_owner = 0.0f;
		if (!session_02fc(session, surrendered_row,
		    surrendered_row_length))
			return false;
	}
	else {
		ship_fighters = double_sub(old_ship, attacker_loss);
		session->player.fighters = (float)ship_fighters;
		deployed_remaining = double_sub(old_count, defender_loss);
	}
	if (qb_str_double(number_one, sizeof(number_one), attacker_loss) < 0
	    || qb_str_double(number_two, sizeof(number_two), defender_loss) < 0
	    || snprintf(loss_row, sizeof(loss_row), " You lost%s fighter(s)",
	    number_one) < 0
	    || snprintf(destroyed_row, sizeof(destroyed_row),
	    " You destroyed%s enemy fighters.", number_two) < 0)
		return false;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack result blank", error)
	    || !session_02fc(session, (const uint8_t *)loss_row,
	    strlen(loss_row))
	    || !session_02fc(session, (const uint8_t *)destroyed_row,
	    strlen(destroyed_row)))
		return false;
	if (ship_fighters < 1.0 && deployed_remaining > 0.0) {
		static const uint8_t exposed[] =
		    "Fighters gone! Enemy attacking your ship!";

		if (!session_02db(session, exposed, sizeof(exposed) - 1U,
		    "deployed attack ship exposed", error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "shield spill leading blank", error)
		    || !fighter_shield_spill(session, &deployed_remaining,
		    &session->player.shields, error))
			return false;
	}
	sector->fighters = (float)deployed_remaining;
	{
		float final_shields = session->player.shields;
		float final_fighters = (float)ship_fighters;

		if (!reload_player(session, error))
			return false;
		(void)snprintf(session->player.name, sizeof(session->player.name),
		    "%s", cached_player_name_text);
		yt_deployed_attack_player_overlay(&session->player,
		    final_shields, final_fighters);
	}
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &persisted_sector, error))
		return false;
	yt_deployed_attack_sector_overlay(&persisted_sector, sector->fighters);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    (int)session->player.sector), &persisted_sector.record, error))
		return false;
	*sector = persisted_sector;
	if (ship_fighters < 1.0
	    && session->player.shields < 1.0f)
		return common_fatal_self(session, error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "deployed attack post-persist blank", error))
		return false;
	if (defender_loss > 0.0f) {
		uint8_t loss_news[320];
		size_t owner_length;
		size_t loss_news_length;
		int loss_number_length;

		if (!reload_player(session, error))
			return false;
		ship_fighters = (double)session->player.fighters;
		(void)snprintf(session->player.name, sizeof(session->player.name),
		    "%s", cached_player_name_text);
		owner_length = session->hostile_owner_label_length;
		if (owner_length > sizeof(owner_name))
			return false;
		if (owner_length != 0U)
			memcpy(owner_name, session->hostile_owner_label,
			    owner_length);
		loss_number_length = qb_str_double(number_one,
		    sizeof(number_one), defender_loss);
		if (loss_number_length < 0)
			return false;
		loss_news_length = cached_player_name_length
		    + sizeof(" destroyed") - 1U + (size_t)loss_number_length
		    + sizeof(" fighters belonging to ") - 1U + owner_length;
		if (loss_news_length > sizeof(loss_news))
			return false;
		if (cached_player_name_length != 0U)
			memcpy(loss_news, cached_player_name,
			    cached_player_name_length);
		memcpy(loss_news + cached_player_name_length, " destroyed",
		    sizeof(" destroyed") - 1U);
		memcpy(loss_news + cached_player_name_length
		    + sizeof(" destroyed") - 1U, number_one,
		    (size_t)loss_number_length);
		memcpy(loss_news + cached_player_name_length
		    + sizeof(" destroyed") - 1U + (size_t)loss_number_length,
		    " fighters belonging to ",
		    sizeof(" fighters belonging to ") - 1U);
		memcpy(loss_news + loss_news_length - owner_length, owner_name,
		    owner_length);
		if (!append_news_bytes(session, loss_news, loss_news_length, error))
			return false;
	}
	if (old_owner == -2.0f && defender_loss > 0.0f)
		session->mercenaries_hurt = true;
	if (old_owner == -1.0f && defender_loss > 0.0f) {
		float bonus;
		uint8_t display[240];
		uint8_t news_row[300];
		size_t display_length;
		size_t news_length;

		if (!reload_player(session, error))
			return false;
		ship_fighters = (double)session->player.fighters;
		(void)snprintf(session->player.name, sizeof(session->player.name),
		    "%s", cached_player_name_text);
		bonus = yt_xannor_attack_bonus(defender_loss,
		    session->player.turns,
		    session->door->game.config.turns_per_day);
		if (bonus >= 1.0f) {
			session->player.turns =
			    single_add(session->player.turns, bonus);
			(void)yt_record_set_number(&session->player.record,
			    YT_F49, session->player.turns);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)session->player_record,
			    &session->player.record, error))
				return false;
			if (!yt_xannor_attack_reward_rows(cached_player_name,
			    cached_player_name_length, bonus, defender_loss,
			    display, sizeof(display), &display_length,
			    news_row, sizeof(news_row), &news_length))
				return false;
			session->presentation.bold = 1.0f;
			if (!session_02fc(session, display, display_length)
			    || !append_news_bytes(session, news_row, news_length,
			    error))
				return false;
			if (deployed_remaining < 1.0
			    && !clearance(session, true, error))
				return false;
		}
	}
	{
		float dominated_draw;

		if (!random_value(session, &dominated_draw, error))
			return false;
	}
	if (deployed_remaining <= 0.0) {
		uint8_t defeated[160];
		size_t defeated_length;

		if (!yt_hostile_defeated_row(ship_fighters,
		    defeated, sizeof(defeated), &defeated_length)
		    || !session_02fc(session, defeated, defeated_length))
			return false;
		if (old_owner == -1.0f && session->player.sector
		    == session->door->game.config.headquarters
		    && !xannor_victory(session, error))
			return false;
	}
	return true;
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
bribe_forced_attack(struct yt_session *session, struct yt_sector *sector,
    bool mercenary_fatal_gate, bool *direct_hostile_menu,
    struct yt_error *error)
{
	enum qb_mbf_status conversion;
	enum yt_bribe_forced_admission admission;
	double cached_ship_fighters = (double)session->player.fighters;

	*direct_hostile_menu = false;
	session->attack_commitment = (float)cached_ship_fighters;
	conversion = qb_mbf32_encode(session->attack_commitment,
	    session->attack_commitment_raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "bribe:commitment-csng");
		}
		return false;
	}
	session->attack_commitment = qb_mbf32_decode(
	    session->attack_commitment_raw);
	admission = yt_bribe_forced_admit(cached_ship_fighters,
	    session->player.shields, mercenary_fatal_gate,
	    session->attack_commitment);
	if (admission == YT_BRIBE_FORCED_FATAL)
		return common_fatal_self(session, error);
	if (admission == YT_BRIBE_FORCED_LESS_THAN_ONE) {
		*direct_hostile_menu = true;
		return true;
	}
	return attack_deployed_committed(session, sector,
	    (double)session->attack_commitment, true, error);
}

static bool
bribe_deployed(struct yt_session *session, struct yt_sector *sector,
    bool *direct_hostile_menu, bool *forced_attack,
    struct yt_error *error)
{
	float owner = sector->fighter_owner;
	float first;
	float second;
	bool force_attack;
	char row[256];

	if (direct_hostile_menu == NULL || forced_attack == NULL)
		return false;
	*direct_hostile_menu = false;
	*forced_attack = false;

	if (owner != -2.0f) {
		(void)snprintf(row, sizeof(row),
		    "We don't accept no Bribes %s!",
		    session->door->identity.real_first);
		if (!session_02db(session, (const uint8_t *)row, strlen(row),
		    "ordinary Bribe refusal", error))
			return false;
		if (!random_value(session, &first, error))
			return false;
		force_attack = yt_bribe_ordinary_forces(owner, sector->fighters,
		    session->player.fighters, first);
		if (!force_attack)
			return true;
		*forced_attack = true;
		return bribe_forced_attack(session, sector, false,
		    direct_hostile_menu, error);
	}
	if (qb_mbf32_truth(sector->record.bytes + YT_F93)) {
		(void)snprintf(row, sizeof(row),
		    "Scram %s, This planet is OURS!",
		    session->door->identity.real_first);
		return session_02db(session, (const uint8_t *)row, strlen(row),
		    "Mercenary planet refusal", error);
	}
	if (!random_value(session, &first, error)
	    || !random_value(session, &second, error))
		return false;
	force_attack = yt_bribe_mercenary_forces(sector->fighters,
	    session->player.fighters, first, second, session->mercenaries_hurt);
	if (force_attack) {
		(void)snprintf(row, sizeof(row),
		    "We just want your miserable life %s!",
		    session->door->identity.real_first);
		if (!session_02db(session, (const uint8_t *)row, strlen(row),
		    "Mercenary life demand", error))
			return false;
		*forced_attack = true;
		return bribe_forced_attack(session, sector, true,
		    direct_hostile_menu, error);
	}
	{
		static const uint8_t introduction_prefix[] =
		    "We MAY join up if you pay us enough ";
		char introduction[256];
		char prompt[256];
		char credits[64];
		char response[160];
		struct qb_val_result parsed;
		enum qb_mbf_status conversion;
		float offer;
		double threshold;
		bool above_credits;

		(void)snprintf(introduction, sizeof(introduction), "%s%s!",
		    introduction_prefix, session->door->identity.real_first);
		if (qb_str_double(credits, sizeof(credits),
		    (double)session->player.credits) < 0
		    || snprintf(prompt, sizeof(prompt),
		    "You have%s credits. How much do you offer? -+>", credits) < 0
		    || !session_0317(session, (const uint8_t *)introduction,
		    strlen(introduction), "Mercenary Bribe introduction", error)
		    || !session_031f(session, (const uint8_t *)prompt, strlen(prompt),
		    "Mercenary Bribe offer prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		if (!parsed.valid || parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "bribe:VAL");
			}
			return false;
		}
		offer = (float)parsed.value;
		{
			uint8_t raw_offer[4];

			conversion = qb_mbf32_encode(offer, raw_offer);
			if (conversion == QB_MBF_OVERFLOW) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "bribe:offer-csng");
				}
				return false;
			}
			offer = qb_mbf32_decode(raw_offer);
		}
		above_credits = (double)offer > (double)session->player.credits;
		if (!random_value(session, &first, error))
			return false;
		threshold = yt_bribe_offer_threshold(sector->fighters, first);
		if (!above_credits
		    && yt_bribe_offer_accepted(offer, session->player.credits,
		    threshold)) {
			struct yt_sector persisted;
			struct yt_player player_overlay;
			float cached_defenders = sector->fighters;

			if (!session_02db(session,
			    (const uint8_t *)"Good Deal! We join up with you!",
			    strlen("Good Deal! We join up with you!"),
			    "accepted Mercenary Bribe", error))
				return false;
			if (!session_sound(session, 1.0f,
			    "accepted bribe sound", error))
				return false;
			if (!yt_game_read_sector(&session->door->game,
			    (int)session->player.sector, &persisted, error))
				return false;
			yt_bribe_sector_overlay(&persisted);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)yt_sector_basic_record(
			    &session->door->game.config,
			    (int)session->player.sector), &persisted.record, error)
			    || !reload_player(session, error))
				return false;
			player_overlay = session->player;
			yt_bribe_player_overlay(&player_overlay, (float)double_add(
			    (double)session->player.fighters,
			    (double)cached_defenders), (float)double_sub(
			    (double)session->player.credits, (double)offer));
			if (!yt_database_write(&session->door->game.database,
			    (size_t)session->player_record,
			    &player_overlay.record, error))
				return false;
			return true;
		}
	}
	(void)snprintf(row, sizeof(row), "You insult us %s! Prepare to DIE!",
	    session->door->identity.real_first);
	if (!session_02db(session, (const uint8_t *)row, strlen(row),
	    "Mercenary rejected offer", error))
		return false;
	*forced_attack = true;
	return bribe_forced_attack(session, sector, true,
	    direct_hostile_menu, error);
}

static bool
shrink_three(struct yt_session *session, float initial, float *result,
    struct yt_error *error)
{
	float value = initial;
	int index;

	for (index = 0; index < 3; ++index) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		value = floorf(single_mul(draw, value)) + 1.0f;
	}
	*result = value;
	return true;
}

static bool
mine_stock_loss(struct yt_session *session, float batch, float *stock,
    float *lost, struct yt_error *error)
{
	float loss;

	if (*stock == 0.0f) {
		*lost = 0.0f;
		return true;
	}
	if (!shrink_three(session, single_mul(batch, *stock), &loss, error))
		return false;
	if (loss > *stock)
		loss = *stock;
	*stock = single_sub(*stock, loss);
	*lost = loss;
	return true;
}

static bool
mine_encounter(struct yt_session *session, struct yt_error *error)
{
	float current_sector = session->player.sector;
	bool overflow;
	int current = (int)qb_cint_mode((double)current_sector,
	    session->presentation.sound.conversion_mode, &overflow);
	struct yt_sector sector;
	uint8_t row[300];
	size_t row_length;
	size_t player_name_length;
	static const uint8_t warning[] = "** Sector is Mined!! **";
	static const uint8_t shields_destroyed[] = "Shields disintegrated!";
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";

	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "sector mine sector record CINT");
		}
		return false;
	}

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine entry blank", error))
		return false;
	session->presentation.blink = 1.0f;
	if (!session_present_text(session, warning, sizeof(warning) - 1U,
	    SESSION_PRESENT_LINE, "sector mine warning", error))
		return false;
	if (!session_sound(session, 5.0f,
	    "sector mine warning sound", error))
		return false;
	if (!reload_player(session, error)
	    || !port_report_length(session, session->player.name_length,
	    YT_TEXT_FIELD_SIZE, &player_name_length,
	    "sector mine player name length", error)
	    || !yt_sector_mine_entry_news(session->player.record.bytes,
	    player_name_length, current_sector, row, sizeof(row),
	    &row_length)
	    || !append_news_bytes(session, row, row_length, error))
		return false;
	for (;;) {
		struct yt_player working;
		struct yt_player persisted;
		unsigned touched = 0U;
		float saved_foreground;
		float before;
		float batch;
		float draw;
		float loss;
		float empty;

		if (!yt_game_read_sector(&session->door->game, current, &sector,
		    error))
			return false;
		before = sector.mines;
		batch = yt_sector_mine_batch(before);
		yt_sector_mine_sector_overlay(&sector,
		    single_sub(before, batch));
		if (!yt_database_write(&session->door->game.database,
		    (size_t)yt_sector_basic_record(&session->door->game.config,
		    current), &sector.record, error))
			return false;
		saved_foreground = session->presentation.foreground;
		session->presentation.foreground = 3.0f;
		session->presentation.background = 0.0f;
		session->presentation.blink = 0.0f;
		session->pager.foreground = 3;
		if (!yt_sector_mine_explosion_row(before, batch, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_RAW, "sector mine explosion", error))
			return false;
		session->presentation.background = 1.0f;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "sector mine explosion terminator", error)
		    || !reload_player(session, error))
			return false;
		working = session->player;
		if (working.shields > 0.0f) {
			if (!random_value(session, &draw, error))
				return false;
			working.shields = yt_sector_mine_shield_result(
			    working.shields, batch, draw);
			touched |= YT_SECTOR_MINE_DAMAGE_SHIELDS;
			if (working.shields == 0.0f) {
				session->presentation.foreground = 7.0f;
				session->presentation.blink = 1.0f;
				session->pager.foreground = 7;
				if (!session_present_text(session, shields_destroyed,
				    sizeof(shields_destroyed) - 1U,
				    SESSION_PRESENT_BOLD_LINE,
				    "sector mine shields destroyed", error))
					return false;
				session->presentation.foreground = saved_foreground;
				session->pager.foreground = (int)saved_foreground;
			}
			else {
				if (!yt_sector_mine_shields_row(working.shields, row,
				    sizeof(row), &row_length)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_BOLD_LINE,
				    "sector mine shields remaining", error)
				    || !random_value(session, &draw, error))
					return false;
				if (working.danger_scanner != 0.0f
				    && draw > 0.949999988079071f) {
					working.danger_scanner = 0.0f;
					touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
					session->presentation.foreground = 7.0f;
					session->presentation.blink = 1.0f;
					session->pager.foreground = 7;
					if (!session_present_text(session,
					    scanner_destroyed,
					    sizeof(scanner_destroyed) - 1U,
					    SESSION_PRESENT_BOLD_LINE,
					    "sector mine scanner destroyed", error))
						return false;
					session->presentation.foreground =
					    saved_foreground;
					session->pager.foreground =
					    (int)saved_foreground;
				}
			}
		}
		else {
			if (working.fighters != 0.0f) {
				if (!shrink_three(session, single_mul(40000.0f,
				    batch), &loss, error))
					return false;
				if (loss > working.fighters)
					loss = working.fighters;
				working.fighters = single_sub(working.fighters,
				    loss);
				touched |= YT_SECTOR_MINE_DAMAGE_FIGHTERS;
				if (!yt_sector_mine_loss_row(
				    YT_SECTOR_MINE_LOSS_FIGHTERS, loss, row,
				    sizeof(row), &row_length)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_LINE, "sector mine fighter loss",
				    error))
					return false;
			}
			if (working.cloak != 0.0f) {
				if (!random_value(session, &draw, error))
					return false;
				loss = yt_sector_mine_cloak_loss(working.cloak,
				    batch, draw);
				working.cloak = single_sub(working.cloak, loss);
				touched |= YT_SECTOR_MINE_DAMAGE_CLOAK;
				if (!yt_sector_mine_loss_row(
				    YT_SECTOR_MINE_LOSS_CLOAK,
				    single_mul(loss, 100.0f), row, sizeof(row),
				    &row_length)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_LINE, "sector mine cloak loss",
				    error))
					return false;
			}
			if (working.missiles != 0.0f) {
				if (!random_value(session, &draw, error))
					return false;
				loss = yt_sector_mine_missile_loss(working.missiles,
				    batch, draw);
				working.missiles = single_sub(working.missiles,
				    loss);
				touched |= YT_SECTOR_MINE_DAMAGE_MISSILES;
				if (!yt_sector_mine_loss_row(
				    YT_SECTOR_MINE_LOSS_MISSILES, loss, row,
				    sizeof(row), &row_length)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_LINE, "sector mine missile loss",
				    error))
					return false;
			}
			if (working.danger_scanner != 0.0f) {
				working.danger_scanner = 0.0f;
				touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
				session->presentation.foreground = 7.0f;
				session->presentation.blink = 1.0f;
				session->pager.foreground = 7;
				if (!session_present_text(session, scanner_destroyed,
				    sizeof(scanner_destroyed) - 1U,
				    SESSION_PRESENT_BOLD_LINE,
				    "sector mine scanner destroyed", error))
					return false;
				session->presentation.foreground = saved_foreground;
				session->pager.foreground = (int)saved_foreground;
			}
#define MINE_STOCK(field, flag, kind, operation) do { \
	if (working.field != 0.0f) { \
		if (!mine_stock_loss(session, batch, &working.field, &loss, \
		    error)) \
			return false; \
		touched |= (flag); \
		if (!yt_sector_mine_loss_row((kind), loss, row, sizeof(row), \
		    &row_length) || !session_present_text(session, row, \
		    row_length, SESSION_PRESENT_LINE, (operation), error)) \
			return false; \
	} \
} while (0)
			MINE_STOCK(mines, YT_SECTOR_MINE_DAMAGE_CARRIED_MINES,
			    YT_SECTOR_MINE_LOSS_MINES,
			    "sector mine carried-mine loss");
			MINE_STOCK(ore, YT_SECTOR_MINE_DAMAGE_ORE,
			    YT_SECTOR_MINE_LOSS_ORE, "sector mine ore loss");
			MINE_STOCK(organics, YT_SECTOR_MINE_DAMAGE_ORGANICS,
			    YT_SECTOR_MINE_LOSS_ORGANICS,
			    "sector mine organics loss");
			MINE_STOCK(equipment, YT_SECTOR_MINE_DAMAGE_EQUIPMENT,
			    YT_SECTOR_MINE_LOSS_EQUIPMENT,
			    "sector mine equipment loss");
#undef MINE_STOCK
			empty = yt_sector_mine_empty_holds(&working);
			if (empty > 0.0f) {
				if (!shrink_three(session, empty, &loss, error))
					return false;
				loss = single_mul(loss, batch);
				if (loss > empty)
					loss = empty;
				working.holds = single_sub(working.holds, loss);
				if (working.holds < 1.0f) {
					working.holds = 0.0f;
					session->destroyed = true;
				}
				touched |= YT_SECTOR_MINE_DAMAGE_HOLDS;
				if (!yt_sector_mine_loss_row(
				    YT_SECTOR_MINE_LOSS_EMPTY_HOLDS, loss, row,
				    sizeof(row), &row_length)
				    || !session_present_text(session, row, row_length,
				    SESSION_PRESENT_LINE,
				    "sector mine empty-hold loss", error))
					return false;
			}
		}
		if (!yt_game_read_player(&session->door->game,
		    session->player_record, &persisted, error))
			return false;
		yt_sector_mine_player_overlay(&persisted, &working, touched);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session->player_record, &persisted.record, error)
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
		working.record = persisted.record;
		session->player = working;
		if (!session_sound(session, 2.0f,
		    "sector mine damage sound", error)
		    || !random_value(session, &draw, error))
			return false;
		if (draw > 0.800000011920929f && working.holds < 10.0f) {
			if (!emergency_warp(session, error))
				return false;
			return true;
		}
		if (sector.mines > 0.0f && !session->destroyed)
			continue;
		break;
	}
	if (!yt_sector_mine_final_news(session->player.shields, row,
	    sizeof(row), &row_length)
	    || !append_news_bytes(session, row, row_length, error)
	    || !yt_game_read_sector(&session->door->game, current, &sector,
	    error))
		return false;
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

	session->mercenaries_hurt = false;
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
			if (!mine_encounter(session, error))
				return false;
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
command_fighters(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t title[] = "<Drop/Take Fighters>";
	static const uint8_t union_refusal[] =
	    "You can't leave fighters in the Union (sectors 1-7)";
	static const uint8_t foreign_refusal[] =
	    "There are already fighters in this sector!";
	static const uint8_t prompt[] =
	    "Defend this sector with how many? ";
	static const uint8_t insufficient[] = "You don't have that many!";
	struct yt_sector sector;
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	uint8_t desired_raw[4];
	char response[160];
	char number[64];
	char row[160];
	double available;
	float desired;
	float delta;
	float remaining;

	if (!session_02fc(session, title, sizeof(title) - 1U)
	    || !reload_player(session, error))
		return false;
	if (session->player.sector < 8.0f) {
		return session_02db(session, union_refusal,
		    sizeof(union_refusal) - 1U, "fighter Union refusal", error);
	}
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.fighters > 0.0f
	    && sector.fighter_owner != (float)session->player_record) {
		return session_02db(session, foreign_refusal,
		    sizeof(foreign_refusal) - 1U,
		    "fighter foreign-force refusal", error);
	}
	available = double_add((double)sector.fighters,
	    (double)session->player.fighters);
	if (qb_str_double(number, sizeof(number), available) < 0
	    || snprintf(row, sizeof(row), "You have%s fighters available.",
	    number) < 0
	    || !session_02fc(session, (const uint8_t *)row, strlen(row))
	    || !session_031f(session, prompt, sizeof(prompt) - 1U,
	    "fighter desired-count prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "fighter desired-count VAL");
		}
		return false;
	}
	desired = (float)floor(parsed.valid ? parsed.value : 0.0);
	conversion = qb_mbf32_encode(desired, desired_raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "fighter desired-count CSNG");
		}
		return false;
	}
	desired = qb_mbf32_decode(desired_raw);
	if (desired < 0.0f)
		return true;
	delta = single_sub(sector.fighters, desired);
	remaining = (float)double_add((double)session->player.fighters,
	    (double)delta);
	if (remaining < 0.0f) {
		return session_02db(session, insufficient,
		    sizeof(insufficient) - 1U, "fighter insufficient notice",
		    error);
	}
	{
		struct yt_sector fresh_sector;
		struct yt_player fresh_player;
		int logical_sector = (int)session->player.sector;

		if (!yt_game_read_sector(&session->door->game, logical_sector,
		    &fresh_sector, error))
			return false;
		fresh_sector.fighters = desired;
		fresh_sector.fighter_owner = (float)session->player_record;
		if (!yt_game_write_sector(&session->door->game, logical_sector,
		    &fresh_sector, error)
		    || !yt_game_read_player(&session->door->game,
		    session->player_record, &fresh_player, error))
			return false;
		fresh_player.fighters = remaining;
		if (!yt_game_write_player(&session->door->game,
		    session->player_record, &fresh_player, error))
			return false;
	}
	if (qb_str_single(number, sizeof(number), remaining) < 0
	    || snprintf(row, sizeof(row),
	    "Done.  You have%s fighters left.", number) < 0
	    || !session_02fc(session, (const uint8_t *)row, strlen(row)))
		return false;
	return session_sound(session, 4.0f,
	    "sector fighter sound", error);
}

static bool
command_mines(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_player persisted_player;
	struct qb_val_result parsed;
	enum qb_mbf_status conversion;
	enum yt_sector_mine_admission admission;
	char carried_text[64];
	char prompt[160];
	char response[160];
	char sector_text[64];
	char row[160];
	float carried;
	float amount;
	uint8_t amount_raw[4];

	if (!reload_player(session, error))
		return false;
	carried = session->player.mines;
	if (carried < 0.0f) {
		persisted_player = session->player;
		persisted_player.mines = 0.0f;
		if (!yt_game_write_player(&session->door->game,
		    session->player_record, &persisted_player, error)
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
	}
	if (carried < 1.0f)
		return session_02db(session,
		    (const uint8_t *)"You don't HAVE any!",
		    strlen("You don't HAVE any!"), "no sector mines", error);
	if (session->player.sector < 8.0f)
		return session_02db(session,
		    (const uint8_t *)
		    "The Union doesnt like the home 7 sectors mined!",
		    strlen("The Union doesnt like the home 7 sectors mined!"),
		    "Union sector mine refusal", error);
	if (qb_str_single(carried_text, sizeof(carried_text), carried) < 0
	    || snprintf(prompt, sizeof(prompt),
	    "You have%s mines. Drop how many? [0] -=>", carried_text) < 0
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine prompt blank", error)
	    || !session_031f(session, (const uint8_t *)prompt, strlen(prompt),
	    "sector mine prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		amount = 0.0f;
	else {
		parsed = qb_val(response);
		if (!parsed.valid || parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "mines:VAL");
			}
			return false;
		}
		amount = (float)parsed.value;
	}
	conversion = qb_mbf32_encode(amount, amount_raw);
	if (conversion == QB_MBF_OVERFLOW) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "mines:amount-csng");
		}
		return false;
	}
	amount = qb_mbf32_decode(amount_raw);
	admission = yt_sector_mine_admit(carried, amount);
	if (admission != YT_SECTOR_MINE_ACCEPTED)
		return true;
	session->suppress_self_mines = true;
	persisted_player = session->player;
	persisted_player.mines = single_sub(carried, amount);
	if (!yt_game_write_player(&session->door->game,
	    session->player_record, &persisted_player, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	sector.mines = single_add(sector.mines, amount);
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, &sector, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->presentation.foreground = 6.0f;
	session->pager.foreground = 6;
	if (qb_str_single(sector_text, sizeof(sector_text),
	    session->player.sector) < 0
	    || snprintf(row, sizeof(row), "Sector%s is now mined!",
	    sector_text) < 0
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine success blank", error))
		return false;
	session->presentation.bold = 1.0f;
	session->presentation.blink = 1.0f;
	if (!session_02fc(session, (const uint8_t *)row, strlen(row)))
		return false;
	return session_sound(session, 4.0f,
	    "sector mine sound", error);
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

static void
port_report_right(char *dest, size_t size, const char *source, size_t width)
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
port_report_capture(struct yt_session *session, int logical_port,
    const struct yt_port *port, const float prices[3],
    const double quantities[3], struct yt_port *terminal_port,
    struct yt_error *error)
{
	static const char *commodity[3] = {
		"Ore..........", "Organics.....", "Equipment...."
	};
	static const uint8_t header[] =
	    " Items         Status      # units    in holds   Cost";
	static const uint8_t rule[] =
	    "=======       =========   =========   ========   ====";
	struct yt_port final_port;
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];
	uint8_t title[256];
	size_t title_length = 0;
	size_t name_length;
	float holds[3];
	size_t index;

	session->pager.line_count = 0.0f;
	if (!port_owner_row(session, port, error)
	    || !reload_player(session, error)
	    || !yt_game_read_port(&session->door->game, logical_port,
	    &final_port, error)
	    || !port_report_length(session, final_port.name_length,
	    YT_TEXT_FIELD_SIZE, &name_length, "port report name length", error)
	    || !yt_platform_clock(&date_now, error)
	    || !yt_platform_clock(&time_now, error))
		return false;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	memcpy(title + title_length, "Commerce report for ",
	    sizeof("Commerce report for ") - 1U);
	title_length += sizeof("Commerce report for ") - 1U;
	memcpy(title + title_length, final_port.record.bytes, name_length);
	title_length += name_length;
	title[title_length++] = ':';
	memcpy(title + title_length, date, 10);
	title_length += 10;
	title[title_length++] = ' ';
	memcpy(title + title_length, time_text, 8);
	title_length += 8;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port report title blank", error)
	    || !session_b05d(session, title, title_length)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port report header blank", error)
	    || !session_b05d(session, header, sizeof(header) - 1U))
		return false;
	session->presentation.bold = 1.0f;
	if (!session_b05d(session, rule, sizeof(rule) - 1U))
		return false;
	holds[0] = session->player.ore;
	holds[1] = session->player.organics;
	holds[2] = session->player.equipment;
	for (index = 0; index < 3; ++index) {
		const char *status;
		char fragment[128];
		char number[80];
		char aligned[32];

		if (port->factor[index] < 0.0f) {
			status = "  Buying ";
			session->pager.foreground = 3;
			session->presentation.foreground = 3.0f;
		}
		else {
			status = "  Selling";
			session->pager.foreground = 2;
			session->presentation.foreground = 2.0f;
		}
		(void)snprintf(fragment, sizeof(fragment), "%s%s",
		    commodity[index], status);
		if (!session_present_text(session, (const uint8_t *)fragment,
		    strlen(fragment), SESSION_PRESENT_RAW,
		    "port report commodity/status", error))
			return false;
		qb_str_double(number, sizeof(number), floor(quantities[index]));
		port_report_right(aligned, sizeof(aligned), number, 12);
		if (!session_present_text(session, (const uint8_t *)aligned, 12,
		    SESSION_PRESENT_RAW, "port report stock", error))
			return false;
		qb_str_double(number, sizeof(number), (double)holds[index]);
		port_report_right(aligned, sizeof(aligned), number, 11);
		if (!session_present_text(session, (const uint8_t *)aligned, 11,
		    SESSION_PRESENT_RAW, "port report player hold", error))
			return false;
		qb_str_single(number, sizeof(number), prices[index]);
		(void)snprintf(fragment, sizeof(fragment), "%s    ", number);
		if (!session_present_text(session, (const uint8_t *)fragment,
		    strlen(fragment), SESSION_PRESENT_LINE,
		    "port report price", error))
			return false;
	}
	session->pager.foreground = 3;
	session->presentation.foreground = 3.0f;
	if (terminal_port != NULL)
		*terminal_port = final_port;
	return true;
}

static bool
port_report(struct yt_session *session, int logical_port,
    const struct yt_port *port, const float prices[3],
    const double quantities[3], struct yt_error *error)
{
	return port_report_capture(session, logical_port, port, prices,
	    quantities, NULL, error);
}

static bool
trade_commodity(struct yt_session *session, const struct yt_port *cached_port,
    int logical_port, size_t commodity, float price, double cached_quantity,
    bool *prompt_reached, struct yt_error *error)
{
	static const char *const names[3] = {"Ore", "Organics", "Equipment"};
	static const uint8_t confirmation[] = "Do you agree? [Y/n] ";
	static const uint8_t never_mind[] = "Never mind!";
	static const uint8_t yours[] = "It's Yours!";
	static const uint8_t take[] = "We'll take them!";
	struct qb_val_result parsed;
	enum yt_yes_no_answer answer;
	double displayed_hold;
	double credits;
	double free_double;
	float free_holds;
	float maximum;
	float quantity;
	float total;
	bool port_sells;
	char number_one[64];
	char number_two[64];
	char row[256];
	char response[80];

	if (!reload_player(session, error))
		return false;
	switch (commodity) {
	case 0:
		displayed_hold = (double)session->player.ore;
		break;
	case 1:
		displayed_hold = (double)session->player.organics;
		break;
	case 2:
		displayed_hold = (double)session->player.equipment;
		break;
	default:
		return false;
	}
	credits = (double)session->player.credits;
	free_double = double_sub(
	    double_sub(double_sub((double)session->player.holds,
	    (double)session->player.ore), (double)session->player.organics),
	    (double)session->player.equipment);
	free_holds = (float)free_double;
	port_sells = floorf(cached_port->factor[commodity]) > 0.0f;
	if (port_sells) {
		float required;

		maximum = free_holds;
		if ((double)maximum > floor(cached_quantity))
			maximum = (float)floor(cached_quantity);
		required = floorf(single_mul(price, maximum));
		if ((double)required > credits)
			maximum = (float)floor(credits / (double)price);
	}
	else {
		maximum = (float)floor(cached_quantity);
		if ((double)maximum > floor(displayed_hold))
			maximum = (float)floor(displayed_hold);
	}
	if (maximum == 0.0f)
		return true;

	if (qb_str_double(number_one, sizeof(number_one), credits) < 0
	    || qb_str_single(number_two, sizeof(number_two), free_holds) < 0
	    || snprintf(row, sizeof(row),
	    "You have%s credits and%s empty cargo holds.",
	    number_one, number_two) < 0
	    || !session_0317(session, (const uint8_t *)row, strlen(row),
	    "commodity trade player status", error)
	    || qb_str_double(number_one, sizeof(number_one),
	    floor(cached_quantity)) < 0
	    || qb_str_double(number_two, sizeof(number_two), displayed_hold) < 0
	    || snprintf(row, sizeof(row),
	    "We are %sing up to%s.  You have%s in your holds.",
	    port_sells ? "sell" : "buy", number_one, number_two) < 0
	    || !session_0317(session, (const uint8_t *)row, strlen(row),
	    "commodity trade market status", error))
		return false;

	for (;;) {
		if (qb_str_single(number_one, sizeof(number_one), maximum) < 0
		    || snprintf(row, sizeof(row),
		    "How many holds of %s do you want to %s [%s ]? ",
		    names[commodity], port_sells ? "buy" : "sell",
		    number_one) < 0)
			return false;
		if (prompt_reached != NULL)
			*prompt_reached = true;
		if (!session_031f(session, (const uint8_t *)row, strlen(row),
		    "commodity trade quantity prompt", error)
		    || !session_0357(session, response, sizeof(response)))
			return false;
		if (strlen(response) > 4U)
			continue;
		if (response[0] == '\0')
			quantity = maximum;
		else {
			parsed = qb_val(response);
			if (parsed.overflow) {
				if (error != NULL) {
					error->status = YT_RANGE;
					(void)snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "commodity trade:VAL");
				}
				return false;
			}
			quantity = parsed.valid
			    ? (float)floor(parsed.value) : 0.0f;
		}
		if (quantity < 1.0f)
			return true;
		if ((double)quantity > cached_quantity)
			return session_02db(session,
			    (const uint8_t *)(port_sells
			    ? "We don't have that much!"
			    : "We don't need that much!"),
			    strlen(port_sells
			    ? "We don't have that much!"
			    : "We don't need that much!"),
			    "commodity trade capacity rejection", error);
		if (port_sells && quantity > free_holds) {
			static const uint8_t no_holds[] =
			    "You don't have enough cargo holds.";

			if (!session_02db(session, no_holds,
			    sizeof(no_holds) - 1U,
			    "commodity trade free-holds rejection", error)
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE,
			    "commodity trade free-holds retry blank", error))
				return false;
			continue;
		}
		if (quantity > maximum)
			return session_02db(session,
			    (const uint8_t *)(port_sells
			    ? "You can't afford that much!"
			    : "You don't have that much!"),
			    strlen(port_sells
			    ? "You can't afford that much!"
			    : "You don't have that much!"),
			    "commodity trade maximum rejection", error);
		if (port_sells && (double)quantity > floor(cached_quantity)) {
			static const uint8_t many[] =
			    "We're not selling that many.";

			if (!session_02fc(session, many, sizeof(many) - 1U))
				return false;
			continue;
		}
		if (!port_sells && (double)quantity > floor(cached_quantity)) {
			static const uint8_t many[] =
			    "We don't want that many.";

			if (!session_02db(session, many, sizeof(many) - 1U,
			    "commodity trade buying retry", error))
				return false;
			continue;
		}
		if (!port_sells && (double)quantity > displayed_hold) {
			static const uint8_t many[] =
			    "You don't have that much!";

			if (!session_02db(session, many, sizeof(many) - 1U,
			    "commodity trade hold retry", error))
				return false;
			continue;
		}
		break;
	}

	total = floorf(single_add(single_mul(price, quantity), 0.5f));
	if (qb_str_single(number_one, sizeof(number_one), quantity) < 0
	    || snprintf(row, sizeof(row), "Agreed,%s units.", number_one) < 0
	    || !session_02fc(session, (const uint8_t *)row, strlen(row))
	    || qb_str_single(number_one, sizeof(number_one), total) < 0
	    || snprintf(row, sizeof(row), "We'll %s them for%s credits.",
	    port_sells ? "sell" : "buy", number_one) < 0
	    || !session_0317(session, (const uint8_t *)row, strlen(row),
	    "commodity trade offer row", error)
	    || !session_a8d2(session, confirmation,
	    sizeof(confirmation) - 1U, &answer, error))
		return false;
	if (answer == YT_YES_NO_NO)
		return session_02fc(session, never_mind,
		    sizeof(never_mind) - 1U);
	if (!session_02fc(session, port_sells ? yours : take,
	    port_sells ? sizeof(yours) - 1U : sizeof(take) - 1U))
		return false;

	{
		float factor = cached_port->factor[commodity];
		float direction = factor > 0.0f ? 1.0f
		    : factor < 0.0f ? -1.0f : 0.0f;
		float credit_delta = -single_mul(total, direction);

		if (port_sells && cached_port->owner != 0.0f) {
			struct yt_port fresh_port;
			float receipt = total;

			if (cached_port->owner == (float)session->player_record)
				receipt = floorf(single_mul(
				    0.009999999776482582f, total));
			if (!yt_game_read_port(&session->door->game,
			    logical_port, &fresh_port, error))
				return false;
			yt_trade_treasury_overlay(&fresh_port, receipt);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)yt_port_basic_record(
			    &session->door->game.config, logical_port),
			    &fresh_port.record, error))
				return false;
		}
		if (!reload_player(session, error))
			return false;
		yt_trade_credit_overlay(&session->player, credit_delta);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session->player_record, &session->player.record,
		    error)
		    || !reload_player(session, error))
			return false;
		yt_trade_holds_overlay(&session->player, commodity, quantity,
		    direction);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session->player_record, &session->player.record,
		    error))
			return false;
		{
			struct yt_port fresh_port;

			if (!yt_game_read_port(&session->door->game,
			    logical_port, &fresh_port, error))
				return false;
			yt_trade_stock_overlay(&fresh_port, commodity,
			    cached_quantity, quantity);
			if (!yt_database_write(&session->door->game.database,
			    (size_t)yt_port_basic_record(
			    &session->door->game.config, logical_port),
			    &fresh_port.record, error))
				return false;
		}
	}
	return true;
}
static bool
command_trade(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t heading[] = "<Port>";
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t docking[] = "Docking, ";
	struct yt_sector gate_sector;
	struct yt_port port;
	struct yt_port selected_port;
	float prices[3];
	double quantities[3];
	volatile float selected_expression;
	int logical_port;
	size_t commodity;
	size_t schedule[3];
	size_t scheduled_count;
	size_t scheduled_index;
	bool denied;
	bool prompt_reached = false;

	if (!session_02fc(session, heading, sizeof(heading) - 1U))
		return false;
	session->presentation.foreground = 3.0f;
	session->pager.foreground = 3;
	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &gate_sector, error))
		return false;
	selected_expression = single_add(
	    session->door->game.config.port_offset, gate_sector.port);
	(void)selected_expression;
	if (yt_port_link_missing(gate_sector.port))
		return session_02db(session, no_port, sizeof(no_port) - 1U,
		    "port docking no port", error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port docking leading blank", error)
	    || !session_031f(session, docking, sizeof(docking) - 1U,
	    "port docking prelude", error))
		return false;
	if (!finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	logical_port = (int)gate_sector.port;
	if (!yt_game_read_port(&session->door->game, logical_port,
	    &selected_port, error))
		return false;
	(void)selected_port;
	if (session->player.sector == 1.0f)
		return earth_store(session, error);
	{
		struct yt_sector updater_sector;

		if (!yt_game_read_sector(&session->door->game,
		    (int)session->player.sector, &updater_sector, error))
			return false;
		logical_port = (int)updater_sector.port;
	}
	if (!port_update(session, logical_port, &port, prices, quantities,
	    error))
		return false;
	if (!port_report(session, logical_port, &port, prices, quantities,
	    error))
		return false;
	scheduled_count = yt_port_trade_schedule(port.factor, schedule);
	for (scheduled_index = 0; scheduled_index < scheduled_count;
	    ++scheduled_index) {
		commodity = schedule[scheduled_index];
		if (!trade_commodity(session, &port, logical_port, commodity,
		    prices[commodity], quantities[commodity], &prompt_reached,
		    error))
			return false;
	}
	if (!prompt_reached) {
		char refusal[256];

		session->presentation.foreground = 6.0f;
		session->pager.foreground = 6;
		if (snprintf(refusal, sizeof(refusal),
		    "We don't want your goods and you can't buy ours %s!",
		    session->door->identity.real_first) < 0
		    || !session_02db(session, (const uint8_t *)refusal,
		    strlen(refusal), "port docking refusal", error))
			return false;
	}
	if (!reload_player(session, error))
		return false;
	{
		char credits[64];
		char empty[64];
		char row[192];
		double free_holds = double_sub(
		    double_sub(double_sub((double)session->player.holds,
		    (double)session->player.ore),
		    (double)session->player.organics),
		    (double)session->player.equipment);

		if (qb_str_double(credits, sizeof(credits),
		    (double)session->player.credits) < 0
		    || qb_str_double(empty, sizeof(empty), free_holds) < 0
		    || snprintf(row, sizeof(row),
		    "You have%s credits and%s empty cargo holds.",
		    credits, empty) < 0
		    || !session_0317(session, (const uint8_t *)row, strlen(row),
		    "port docking cargo status", error))
			return false;
	}
	return true;
}

static bool
earth_receipt(struct yt_session *session, const struct yt_port *cached_earth,
    float cost,
    struct yt_error *error)
{
	struct yt_port earth;

	if (!reload_player(session, error))
		return false;
	session->player.credits =
	    floorf(single_sub(session->player.credits, cost));
	if (!write_player(session, error))
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
earth_anti_cloak_write_player(void *context, float record,
    const struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	uint32_t physical = qb_brun_random_record_number(record);

	return yt_database_write(&session->door->game.database, (size_t)physical,
	    &player->record, error)
	    && yt_database_flush(&session->door->game.database, error);
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
		earth_anti_cloak_write_player,
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
	if (!reload_player(session, error))
		return false;
	session->player.credits = floorf(single_add(session->player.credits,
	    award));
	if (!write_player(session, error)
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
	session->pager.line_count = 0.0f;
	if (!yt_game_read_port(&session->door->game, 1, earth, error))
		return false;
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
	    || !port_owner_row(session, earth, error))
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
	session->earth_report_seen = 1.0f;
	if (!reload_player(session, error)
	    || !session_b05d(session, separator, sizeof(separator) - 1U)
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
earth_store(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t menu[] =
	    "[I] Ship Info -=*=- [0] Leave Port";
	static const uint8_t prompt_suffix[] =
	    " -=*=- Buy Which Item? -=>";
	static const uint8_t invalid[] = "INAVLID CHOICE!";
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

			session->earth_report_seen = 0.0f;
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
				bool enter_sector = false;

				return command_land(session, &enter_sector, error);
			}
			case 2: {
				bool moved;

				return command_move(session, &moved, error);
			}
			case 3:
				return true;
			case 4: {
				bool enter_sector = false;

				return computer_menu(session, &enter_sector, error);
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
	if (!reload_player(session, error))
		return false;
	yt_planet_bank_credit_overlay(&session->player, credit_argument);
	return write_player(session, error);
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
	if (!reload_player(session, error))
		return false;
	yt_planet_bank_credit_overlay(&session->player, credit_argument);
	if (!write_player(session, error)
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
			float first;
			float second;

			if (!random_value(session, &first, error)
			    || !random_value(session, &second, error))
				return false;
			loss = yt_planet_move_fighter_loss(
			    session->player.fighters, first, second);
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
		const char *position;
		const char *alphabet = "F!MPC1234569LTAB$";

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
		position = strstr(alphabet, upper);
		if (position == NULL) {
			static const uint8_t invalid[] = "Invalid command.";

			if (!session_02db(session, invalid,
			    sizeof(invalid) - 1U, "planet invalid command", error))
				return false;
			continue;
		}
		switch ((int)(position - alphabet)) {
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
			if (!command_trade(session, error))
				return false;
			break;
		case 4:
			return computer_menu(session, enter_sector, error);
		case 5: case 6: case 7: case 8: case 9: case 10:
			if (!planet_take_one(session, logical_planet,
			    (int)(position - alphabet) - 4, error))
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
	    error)
	    || !reload_player(session, error))
		return false;
	yt_planet_creation_credit_overlay(&session->player, -25000.0f);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
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
planet_landing_same_team(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	struct yt_player current;
	struct yt_player other;
	int owner_record;

	if (friendly == NULL)
		return false;
	*friendly = false;
	if (session->player_record < YT_PLAYER_FIRST
	    || session->player_record > YT_PLAYER_LAST
	    || !yt_planet_landing_valid_owner(owner, YT_PLAYER_LAST,
	    &owner_record))
		return true;
	if (!yt_game_read_player(&session->door->game, session->player_record,
	    &current, error))
		return false;
	if (current.team == 0.0f)
		return true;
	if (!yt_game_read_player(&session->door->game, owner_record, &other,
	    error))
		return false;
	*friendly = yt_sector_force_same_team(current.team, other.team);
	return true;
}

static bool
command_land(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t title[] = "<Land/Create planet>";
	static const uint8_t landing[] = "Landing...";
	static const uint8_t governor[] =
	    "This planet has no governor! Hail to the new planetary governor!!";
	static const uint8_t unrest[] =
	    "Due to the unrest caused by the lack of planetary govornment, ";
	static const uint8_t permission[] = "Permission to land is ";
	static const uint8_t denied[] = "DENIED!";
	static const uint8_t confirmation[] =
	    "Do you wish to try to force a landing? [y/N] ";
	struct yt_sector sector;
	struct yt_planet planet;
	uint8_t name[YT_TEXT_FIELD_SIZE];
	uint8_t row[256];
	uint8_t prompt[160];
	size_t name_length;
	size_t row_length;
	size_t prompt_length;
	uint32_t physical;
	float updater_logical;
	float cached_carried;
	float cached_ground;
	int logical;
	bool allowed = false;

	if (!session_0317(session, title, sizeof(title) - 1U,
	    "planet landing title", error)
	    || !reload_player(session, error))
		return false;
	cached_carried = session->player.ground_forces;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.planet == 0.0f) {
		bool created = create_planet(session, error);

		if (created && enter_sector != NULL)
			*enter_sector = true;
		return created;
	}
	session->pager.foreground = 6;
	session->presentation.foreground = 6.0f;
	if (!session_0317(session, landing, sizeof(landing) - 1U,
	    "planet landing progress", error)
	    || !yt_planet_landing_record(
	    session->door->game.config.planet_offset, sector.planet,
	    &physical, &updater_logical))
		return false;
	(void)updater_logical;
	if (!planet_update_cached_physical(session, physical, &planet, NULL,
	    error)
	    || !read_planet_physical(session, physical, &planet, error)
	    || !yt_planet_stored_name(&planet, name, &name_length, error))
		return false;
	cached_ground = planet.ground_forces;
	if (yt_planet_landing_immediate_allow(cached_ground, planet.owner,
	    session->player_record))
		allowed = true;
	else if (!planet_landing_same_team(session, planet.owner, &allowed,
	    error))
		return false;
	if (!allowed && !session_present_text(session, NULL, 0,
	    SESSION_PRESENT_LINE, "planet permission late blank", error))
		return false;
	if (!allowed) {
		struct yt_player owner;
		bool vacant = planet.owner == 0.0f;
		int owner_record = 0;

		if (!vacant && yt_planet_landing_valid_owner(planet.owner,
		    YT_PLAYER_LAST, &owner_record)) {
			if (!yt_game_read_player(&session->door->game,
			    owner_record, &owner, error))
				return false;
			vacant = yt_planet_landing_vacant(planet.owner,
			    owner.killed_by, YT_PLAYER_LAST);
		}
		if (vacant) {
			struct yt_planet fresh;
			float first;
			float second;
			float new_ground;

			if (!session_present_text(session, governor,
			    sizeof(governor) - 1U, SESSION_PRESENT_LINE,
			    "vacant planet governor row", error)
			    || !session_sound(session, 1.0f,
			    "vacant planet sound", error)
			    || !session_wait(session, 2.0,
			    "vacant-planet governor wait", error))
				return false;
			if (!read_planet_physical(session, physical, &fresh, error)
			    || !random_value(session, &first, error)
			    || !random_value(session, &second, error))
				return false;
			new_ground = yt_planet_landing_attrition(first, second,
			    cached_ground);
			if (!session_present_text(session, unrest,
			    sizeof(unrest) - 1U, SESSION_PRESENT_LINE,
			    "vacant planet unrest row", error)
			    || !yt_planet_landing_unrest_row(new_ground,
			    cached_ground, row, sizeof(row), &row_length)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "vacant planet reduction row", error))
				return false;
			yt_planet_landing_vacancy_overlay(&fresh, new_ground,
			    session->player_record);
			if (!write_planet_physical(session, physical, &fresh, false,
			    error)
			    || !session_wait(session, 5.0,
			    "vacant-planet unrest wait", error))
				return false;
			planet = fresh;
			allowed = true;
		}
	}
	if (!allowed) {
		enum yt_yes_no_answer answer;
		char response[YT_COMMAND_SIZE];
		float commitment;
		bool defeated;

		if (!yt_planet_landing_traffic_row(name, name_length, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "planet permission traffic row", error)
		    || !session_present_text(session, permission,
		    sizeof(permission) - 1U, SESSION_PRESENT_RAW,
		    "planet permission prefix", error))
			return false;
		session->presentation.blink = 1.0f;
		session->pager.foreground = 3;
		session->presentation.foreground = 3.0f;
		if (!session_present_text(session, denied, sizeof(denied) - 1U,
		    SESSION_PRESENT_BOLD_LINE, "planet permission denial", error))
			return false;
		session->pager.foreground = 6;
		session->presentation.foreground = 6.0f;
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
	enum yt_team_loader_route route = YT_TEAM_LOADER_OUT_OF_RANGE;
	bool needs_overlay;
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = id;
	yt_team_loader_begin((float)id, &session->team_cache, &needs_overlay);
	if (!needs_overlay)
		goto loaded;
	if (!yt_game_read_sector(&session->door->game, id, &team->overlay,
	    error))
		return false;
	if (!yt_team_loader_finish(&team->overlay.record,
	    (float)session->player_record,
	    session->presentation.sound.conversion_mode,
	    &session->team_cache, &route, error))
		return false;

loaded:
	memcpy(team->name, session->team_cache.name,
	    sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = route == YT_TEAM_LOADER_LIVE;
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
info_fixed(struct yt_session *session, const char *text, float width,
    struct yt_error *error)
{
	return session_fixed_width(session, text, width,
	    "Info fixed-width presentation", error);
}

static void
info_cell(char *dest, size_t size, const char *label, const char *value)
{
	if (size == 0)
		return;
	dest[0] = (char)0xba;
	if (size == 1)
		return;
	(void)snprintf(dest + 1, size - 1U, "%s%s", label, value);
}

static bool
info_ordinary_row(struct yt_session *session, const char *left_label,
    const char *left_value, const char *right_label, const char *right_value,
    struct yt_error *error)
{
	char left[160];
	char right[160];
	static const uint8_t bar = 0xba;

	info_cell(left, sizeof(left), left_label, left_value);
	info_cell(right, sizeof(right), right_label, right_value);
	return info_fixed(session, left, 26.0f, error)
	    && info_fixed(session, right, 23.0f, error)
	    && info_line(session, &bar, 1, error);
}

static bool
info_commodity_row(struct yt_session *session, const char *left_label,
    const char *left_value, const char *right_label, float right_value,
    struct yt_error *error)
{
	char left[160];
	char right[160];
	char number[64];
	static const uint8_t bar = 0xba;

	info_cell(left, sizeof(left), left_label, left_value);
	info_cell(right, sizeof(right), right_label, "");
	qb_str_single(number, sizeof(number), right_value);
	if (!info_fixed(session, left, 26.0f, error)
	    || !info_fixed(session, right, 17.0f, error))
		return false;
	if (right_value != 0.0f) {
		session->presentation.bold = 1.0f;
		session->presentation.foreground = 7.0f;
		session->presentation.background = 4.0f;
		session->pager.foreground = 7;
	}
	if (!info_fixed(session, number, 6.0f, error))
		return false;
	session->presentation.foreground = 2.0f;
	session->presentation.background = 0.0f;
	session->pager.foreground = 2;
	return info_line(session, &bar, 1, error);
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
	enum yt_team_loader_route route = YT_TEAM_LOADER_OUT_OF_RANGE;
	bool needs_overlay;
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = (int)team_id;
	session->team_cache.captain_flag = *captain_flag;
	yt_team_loader_begin(team_id, &session->team_cache, &needs_overlay);
	if (needs_overlay) {
		struct yt_record raw;
		float expression = single_add(
		    session->door->game.config.sector_offset, team_id);
		uint32_t physical = qb_brun_random_record_number(expression);

		if (!yt_database_read(&session->door->game.database,
		    (size_t)physical, &raw, error))
			return false;
		yt_sector_decode(&team->overlay, &raw);
		if (!yt_team_loader_finish(&team->overlay.record, current_record,
		    session->presentation.sound.conversion_mode,
		    &session->team_cache, &route, error))
			return false;
	}
	memcpy(team->name, session->team_cache.name, sizeof(team->name));
	team->name_length = session->team_cache.name_length;
	memcpy(team->password, session->team_cache.password,
	    sizeof(team->password));
	team->captain = session->team_cache.captain;
	team->live = route == YT_TEAM_LOADER_LIVE;
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
show_ship(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t top[50] = {
		0xc9, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcb, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbb
	};
	static const uint8_t bottom[50] = {
		0xc8, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xca, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
		0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xbc
	};
	char cached_name[sizeof(session->player.name)];
	char row[256];
	char left[64];
	char right[64];
	int saved_foreground;
	float cloak_percent;

	if (!info_refresh_time(session, error))
		return false;
	(void)snprintf(cached_name, sizeof(cached_name), "%s",
	    session->player.name);
	saved_foreground = session->pager.foreground;
	session->pager.foreground = 2;
	session->presentation.foreground = 2.0f;
	if (!info_line(session, NULL, 0, error)
	    || !info_fixed(session, "", 20.0f, error)
	    || !info_line(session, "[ Info ]", 8, error)
	    || !info_line(session, NULL, 0, error))
		return false;
	(void)snprintf(row, sizeof(row), "Name  : %s", cached_name);
	if (!info_line(session, row, strlen(row), error))
		return false;
	if (session->time.text_length > sizeof(row) - 8U)
		return info_failure(error, "Info time text capacity");
	memcpy(row, "Time  :", 7);
	memcpy(row + 7, session->time.text, session->time.text_length);
	if (!info_line(session, row, 7U + session->time.text_length, error)
	    || !info_team_lines(session, NULL, NULL, error)
	    || !reload_player(session, error)
	    || !info_line(session, top, sizeof(top), error))
		return false;
	qb_str_double(left, sizeof(left), (double)session->player.credits);
	qb_str_single(right, sizeof(right), session->player.sector);
	if (!info_ordinary_row(session, " Credits.. :", left,
	    " Sector....... :", right, error))
		return false;
	qb_str_single(left, sizeof(left), session->player.turns);
	qb_str_single(right, sizeof(right), session->player.holds);
	if (!info_ordinary_row(session, " Turns.... :", left,
	    " Holds........ :", right, error))
		return false;
	qb_str_double(left, sizeof(left), (double)session->player.fighters);
	if (!info_commodity_row(session, " Fighters. :", left,
	    " Ore.......... :", session->player.ore, error))
		return false;
	qb_str_single(left, sizeof(left), session->player.mines);
	if (!info_commodity_row(session, " Mines.... :", left,
	    " Organics..... :", session->player.organics, error))
		return false;
	qb_str_single(left, sizeof(left), session->player.missiles);
	if (!info_commodity_row(session, " Missiles. :", left,
	    " Equipment.... :", session->player.equipment, error))
		return false;
	(void)snprintf(left, sizeof(left), "%s",
	    session->player.danger_scanner == 0.0f ? " NONE" : " Installed");
	qb_str_single(right, sizeof(right), session->player.ports_owned);
	if (!info_ordinary_row(session, " Scanner.. :", left,
	    " Ports Owned.. :", right, error))
		return false;
	qb_str_double(left, sizeof(left), (double)session->player.shields);
	if (session->anti_cloak)
		(void)snprintf(right, sizeof(right), " FAIL");
	else {
		cloak_percent = floorf(single_mul(session->player.cloak, 100.0f));
		qb_str_single(right, sizeof(right), cloak_percent);
		(void)strncat(right, "%", sizeof(right) - strlen(right) - 1U);
	}
	if (!info_ordinary_row(session, " Shields.. :", left,
	    " Cloak Energy. :", right, error))
		return false;
	qb_str_single(left, sizeof(left), session->player.ground_forces);
	qb_str_single(right, sizeof(right), session->player.plasma);
	if (!info_ordinary_row(session, " Forces... :", left,
	    " Plasma Bolts. :", right, error)
	    || !info_line(session, bottom, sizeof(bottom), error))
		return false;
	session->pager.foreground = saved_foreground;
	session->presentation.foreground = (float)saved_foreground;
	return true;
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
command_rename_port(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t not_owner[] = "This isn't your port!";
	static const uint8_t earth[] = "Can't rename Earth!";
	struct yt_sector sector;
	struct yt_port port;
	uint8_t cached[YT_TEXT_FIELD_SIZE];
	int logical_port;
	float relative_port;
	size_t cached_length;

	if (!reload_player(session, error)
	    || !yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (!qb_mbf32_truth(sector.record.bytes + YT_F65))
		return session_02db(session, no_port, sizeof(no_port) - 1U,
		    "rename no-port row", error);
	if (!yt_port_rename_record(session->door->game.config.port_offset,
	    sector.port, &logical_port, &relative_port))
		return port_report_failure(error, "rename port record conversion");
	if (!yt_game_read_port(&session->door->game, logical_port, &port,
	    error))
		return false;
	if (port.owner != (float)session->player_record)
		return session_02db(session, not_owner,
		    sizeof(not_owner) - 1U, "rename ownership row", error);
	if (relative_port == 1.0f)
		return session_02db(session, earth, sizeof(earth) - 1U,
		    "rename Earth row", error);
	if (!port_report_length(session, port.name_length,
	    YT_TEXT_FIELD_SIZE, &cached_length, "port name length", error))
		return false;
	memcpy(cached, port.record.bytes, cached_length);
	return port_rename(session, logical_port, cached, cached_length, &port,
	    error);
}

static bool
command_buy_port(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_port[] = "No port here!";
	static const uint8_t unaffordable[] =
	    "Come back when you can afford it!";
	static const uint8_t prompt[] = "Do you wish to buy it? [y/N]";
	static const uint8_t declined[] = "What a shame.. it's a nice port!";
	static const uint8_t sold[] = "Sold!";
	static const uint8_t success_tail[] =
	    "will go into the port treasury for you to take out later!";
	struct yt_sector sector;
	struct yt_port port;
	float prices[3];
	double quantities[3];
	double price;
	int logical_port;
	float relative_port;
	float old_owner;
	float purchase_production[3] = {0.0f, 0.0f, 0.0f};
	float cached_buyer_credits;
	float cached_buyer_sector;
	uint8_t cached_trader[YT_TEXT_FIELD_SIZE];
	uint8_t old_name[YT_TEXT_FIELD_SIZE];
	uint8_t owner_name[YT_TEXT_FIELD_SIZE];
	size_t cached_trader_length;
	size_t old_name_length;
	size_t owner_name_length = 0U;
	enum yt_yes_no_answer answer;
	char number_one[64];
	char number_two[64];
	uint8_t row[512];
	size_t row_length;

	if (!reload_player(session, error))
		return false;
	cached_buyer_credits = session->player.credits;
	cached_buyer_sector = session->player.sector;
	if (!yt_player_stored_name(&session->player, cached_trader,
	    &cached_trader_length, error)
	    || !yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (!qb_mbf32_truth(sector.record.bytes + YT_F65))
		return session_02db(session, no_port, sizeof(no_port) - 1U,
		    "buy no-port row", error);
	if (!yt_port_rename_record(session->door->game.config.port_offset,
	    sector.port, &logical_port, &relative_port))
		return port_report_failure(error, "buy port record conversion");
	if (sector.port == 1.0f) {
		float earth_prices[4];

		price = 1000000000.0;
		if (!earth_report(session, &port, earth_prices, error))
			return false;
		old_owner = port.owner;
		memcpy(old_name, "Earth", sizeof("Earth") - 1U);
		old_name_length = sizeof("Earth") - 1U;
	}
	else {
		struct yt_port terminal_port;

		if (!port_update(session, logical_port, &port, prices, quantities,
		    error))
			return false;
		old_owner = port.owner;
		memcpy(purchase_production, port.production,
		    sizeof(purchase_production));
		if (!port_report_capture(session, logical_port, &port, prices,
		    quantities, &terminal_port, error))
			return false;
		port = terminal_port;
		if (!port_report_length(session, port.name_length,
		    YT_TEXT_FIELD_SIZE, &old_name_length,
		    "buy old port name length", error))
			return false;
		memcpy(old_name, port.record.bytes, old_name_length);
		price = yt_port_purchase_price(purchase_production);
	}
	if (old_owner == (float)session->player_record) {
		static const uint8_t prefix[] = "You already OWN this port ";
		const char *first = session->door->identity.real_first;
		size_t first_length = strlen(first);

		row_length = sizeof(prefix) - 1U + first_length + 1U;
		memcpy(row, prefix, sizeof(prefix) - 1U);
		memcpy(row + sizeof(prefix) - 1U, first, first_length);
		row[row_length - 1U] = '!';
		return session_02db(session, row, row_length,
		    "buy already-owner row", error);
	}
	if (qb_str_double(number_one, sizeof(number_one), price) < 0
	    || qb_str_double(number_two, sizeof(number_two),
	    (double)cached_buyer_credits) < 0)
		return port_report_failure(error, "buy price formatting");
	row_length = (size_t)snprintf((char *)row, sizeof(row),
	    "This port is for sale for%s credits. You have%s credits.",
	    number_one, number_two);
	if (row_length >= sizeof(row)
	    || !session_0317(session, row, row_length, "buy price row", error))
		return false;
	if ((double)cached_buyer_credits < price)
		return session_02db(session, unaffordable,
		    sizeof(unaffordable) - 1U, "buy unaffordable row", error);
	if (old_owner != 0.0f) {
		struct yt_port display_port = port;

		display_port.owner = old_owner;
		if (!port_owner_row_capture(session, &display_port, owner_name,
		    sizeof(owner_name), &owner_name_length, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "buy owner offer leading blank", error))
			return false;
		memcpy(row, "You may buy it from ",
		    sizeof("You may buy it from ") - 1U);
		row_length = sizeof("You may buy it from ") - 1U;
		memcpy(row + row_length, owner_name, owner_name_length);
		row_length += owner_name_length;
		memcpy(row + row_length, " if you wish.",
		    sizeof(" if you wish.") - 1U);
		row_length += sizeof(" if you wish.") - 1U;
		if (!session_02fc(session, row, row_length)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "buy owner offer trailing blank", error))
			return false;
	}
	if (!session_a8d2(session, prompt, sizeof(prompt) - 1U, &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return session_02db(session, declined, sizeof(declined) - 1U,
		    "buy declined row", error);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "buy sold leading blank", error))
		return false;
	session->presentation.bold = 1.0f;
	session->presentation.blink = 1.0f;
	if (!session_02fc(session, sold, sizeof(sold) - 1U)
	    || !yt_game_read_port(&session->door->game, logical_port, &port,
	    error))
		return false;
	if (old_owner != 0.0f) {
		struct yt_player seller;
		int seller_record = (int)old_owner;
		char sector_text[64];
		char price_text[64];
		uint8_t message[256];
		size_t message_length = 0U;

		memcpy(row, "Credits transferred to ",
		    sizeof("Credits transferred to ") - 1U);
		row_length = sizeof("Credits transferred to ") - 1U;
		memcpy(row + row_length, owner_name, owner_name_length);
		row_length += owner_name_length;
		memcpy(row + row_length, "'s account!",
		    sizeof("'s account!") - 1U);
		row_length += sizeof("'s account!") - 1U;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "buy seller transfer leading blank", error)
		    || !session_02fc(session, row, row_length)
		    || !yt_game_read_player(&session->door->game, seller_record,
		    &seller, error))
			return false;
		seller.credits = yt_port_purchase_seller_credit(port.treasury,
		    seller.credits, price);
		seller.ports_owned = single_sub(seller.ports_owned, 1.0f);
		if (!yt_record_set_number_if_changed(&seller.record, YT_F81,
		    seller.credits)
		    || !yt_record_set_number_if_changed(&seller.record, YT_F117,
		    seller.ports_owned)
		    || !yt_database_write(&session->door->game.database,
		    (size_t)seller_record, &seller.record, error))
			return false;
		if (qb_str_single(sector_text, sizeof(sector_text),
		    cached_buyer_sector) < 0
		    || qb_str_double(price_text, sizeof(price_text), price) < 0)
			return port_report_failure(error, "buy radio formatting");
		memcpy(message + message_length, cached_trader,
		    cached_trader_length);
		message_length += cached_trader_length;
		memcpy(message + message_length, " bought your port \"",
		    sizeof(" bought your port \"") - 1U);
		message_length += sizeof(" bought your port \"") - 1U;
		memcpy(message + message_length, old_name, old_name_length);
		message_length += old_name_length;
		memcpy(message + message_length, "\" in",
		    sizeof("\" in") - 1U);
		message_length += sizeof("\" in") - 1U;
		memcpy(message + message_length, sector_text, strlen(sector_text));
		message_length += strlen(sector_text);
		memcpy(message + message_length, " for", sizeof(" for") - 1U);
		message_length += sizeof(" for") - 1U;
		memcpy(message + message_length, price_text, strlen(price_text));
		message_length += strlen(price_text);
		memcpy(message + message_length, " credits",
		    sizeof(" credits") - 1U);
		message_length += sizeof(" credits") - 1U;
		if (!radio_append_bytes(message, message_length, -2.0f,
		    old_owner, error)
		    || !yt_game_read_port(&session->door->game, logical_port,
		    &port, error))
			return false;
	}
	if (relative_port > 1.0f
	    && !port_rename(session, logical_port, old_name, old_name_length,
	    &port, error))
		return false;
	if (!yt_game_read_port(&session->door->game, logical_port, &port,
	    error))
		return false;
	port.owner = (float)session->player_record;
	port.treasury = 0.0f;
	if (!yt_record_set_number_if_changed(&port.record, YT_F97,
	    port.owner)
	    || !yt_record_set_number_if_changed(&port.record, YT_F89,
	    port.treasury))
		return port_report_failure(error, "port purchase FIELD overlay");
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_port_basic_record(&session->door->game.config,
	    logical_port), &port.record, error))
		return false;
	if (!reload_player(session, error))
		return false;
	session->player.credits = yt_port_purchase_buyer_credit(
	    session->player.credits, price);
	session->player.ports_owned = single_add(session->player.ports_owned,
	    1.0f);
	if (!yt_record_set_number_if_changed(&session->player.record, YT_F81,
	    session->player.credits)
	    || !yt_record_set_number_if_changed(&session->player.record,
	    YT_F117, session->player.ports_owned)
	    || !yt_database_write(&session->door->game.database,
	    (size_t)session->player_record, &session->player.record, error))
		return false;
	row_length = (size_t)snprintf((char *)row, sizeof(row),
	    "Congratulations %s! When others trade at your port their CREDITS",
	    session->door->identity.real_first);
	return row_length < sizeof(row)
	    && session_0317(session, row, row_length,
	    "buy congratulations row", error)
	    && session_02fc(session, success_tail, sizeof(success_tail) - 1U);
}

static bool
command_collect(struct yt_session *session, bool collecting,
    struct yt_error *error)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x20, 0x00};
	static const uint8_t no_ports[] = "You don't OWN any ports!!!";
	static const uint8_t collect_prefix[] =
	    "Sending out armored cargo ships to";
	static const uint8_t report_prefix[] =
	    "Checking galactic bank statement for";
	static const uint8_t heading_suffix[] = " ports with credits...";
	struct yt_player current;
	int logical;
	double collected = 0.0;
	float owned = 0.0f;
	float credited = 0.0f;
	char number_one[64];
	char number_two[64];
	char text[160];

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "treasury opening blank", error))
		return false;
	if (!yt_game_read_player(&session->door->game, session->player_record,
	    &current, error))
		return false;
	if (current.ports_owned < 1.0f) {
		session->presentation.blink = 1.0f;
		return session_present_text(session, no_ports,
		    sizeof(no_ports) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "treasury no-owned notice", error);
	}
	if (!session_present_text(session,
	    collecting ? collect_prefix : report_prefix,
	    collecting ? sizeof(collect_prefix) - 1U
	    : sizeof(report_prefix) - 1U,
	    SESSION_PRESENT_RAW, "treasury heading prefix", error)
	    || !session_present_text(session, heading_suffix,
	    sizeof(heading_suffix) - 1U, SESSION_PRESENT_LINE,
	    "treasury heading suffix", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "treasury scan blank", error))
		return false;

	for (logical = 1; logical <= port_count(session); ++logical) {
		struct yt_port port;

		if (!yt_game_read_port(&session->door->game, logical, &port,
		    error))
			return false;
		if (port.owner != (float)session->player_record)
			continue;
		owned = single_add(owned, 1.0f);
		if (port.record.bytes[YT_F89 + 3U] == 0)
			continue;
		credited = single_add(credited, 1.0f);
		collected += (double)port.treasury;
		qb_str_single(number_one, sizeof(number_one), port.sector);
		snprintf(text, sizeof(text), "Sector:%s", number_one);
		if (!session_fixed_width(session, text, 14.0f,
		    "treasury sector field", error))
			return false;
		{
			size_t name_length;

			if (!port_report_length(session, port.name_length,
			    YT_TEXT_FIELD_SIZE, &name_length,
			    "treasury port-name length", error)
			    || !session_fixed_width_bytes(session, port.record.bytes,
			    name_length, 25.0f,
			    "treasury port-name field", error))
				return false;
		}
		qb_str_single(number_one, sizeof(number_one), port.treasury);
		snprintf(text, sizeof(text), " Credits:%s", number_one);
		if (!session_fixed_width(session, text, 20.0f,
		    "treasury credit field", error))
			return false;
		qb_str_double(number_two, sizeof(number_two), collected);
		snprintf(text, sizeof(text), " Total:%s", number_two);
		if (!session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE, "treasury row total",
		    error))
			return false;
		if (collecting) {
			port.treasury = 0.0f;
			if (!yt_record_set_raw_number(&port.record, YT_F89,
			    dirty_zero)
			    || !yt_game_write_port(&session->door->game, logical,
			    &port, error))
				return false;
		}
	}
	if (collected != 0.0
	    && !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "treasury nonzero-total blank", error))
		return false;
	qb_str_single(number_one, sizeof(number_one), owned);
	snprintf(text, sizeof(text), "Total ports...:%s", number_one);
	if (!session_present_text(session, (const uint8_t *)text, strlen(text),
	    SESSION_PRESENT_LINE, "treasury total ports", error))
		return false;
	qb_str_single(number_one, sizeof(number_one), credited);
	snprintf(text, sizeof(text), "With credits..:%s", number_one);
	if (!session_present_text(session, (const uint8_t *)text, strlen(text),
	    SESSION_PRESENT_LINE, "treasury credited ports", error))
		return false;
	qb_str_single(number_one, sizeof(number_one),
	    single_sub(owned, credited));
	snprintf(text, sizeof(text), "Barren ports..:%s", number_one);
	if (!session_present_text(session, (const uint8_t *)text, strlen(text),
	    SESSION_PRESENT_LINE, "treasury barren ports", error))
		return false;
	qb_str_double(number_two, sizeof(number_two), collected);
	snprintf(text, sizeof(text), "Total credits.:%s", number_two);
	if (!session_present_text(session, (const uint8_t *)text, strlen(text),
	    SESSION_PRESENT_LINE, "treasury total credits", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "treasury summary blank", error))
		return false;
	if (!collecting) {
		snprintf(text, sizeof(text), "You have%s credits in your port "
		    "accounts.", number_two);
		return session_present_text(session, (const uint8_t *)text,
		    strlen(text), SESSION_PRESENT_LINE, "treasury report result",
		    error);
	}
	snprintf(text, sizeof(text), "You collected a total of%s credits.",
	    number_two);
	if (!session_present_text(session, (const uint8_t *)text, strlen(text),
	    SESSION_PRESENT_LINE, "treasury collection result", error))
		return false;
	if (!yt_game_read_player(&session->door->game, session->player_record,
	    &current, error))
		return false;
	current.ports_owned = owned;
	current.credits = (float)((double)current.credits + collected);
	if (!yt_game_write_player(&session->door->game, session->player_record,
	    &current, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	session->player = current;
	return true;
}

static bool
command_genesis(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prophecy_one[] =
	    "It has been written that one day a Trader Baron will rise up";
	static const uint8_t prophecy_two[] =
	    "and wipe the universe clean of the evil that infests it.";
	static const uint8_t disabled[] = "*FUNCTION DISABLED*";
	static const uint8_t declined[] =
	    "Alas, today is not the day that the prophesy will be fullfilled.";
	static const uint8_t success_one[] =
	    "...and so it was written, that one day a trader baron would emerge who";
	static const uint8_t success_two[] =
	    "would wipe away the all of the evil in the universe.....";
	uint8_t cached_trader[sizeof(session->player.name) - 1U];
	uint8_t prompt[128];
	uint8_t first[160];
	uint8_t second[160];
	size_t cached_trader_length = strlen(session->player.name);
	size_t prompt_length;
	size_t first_length;
	size_t second_length;
	enum yt_yes_no_answer answer;
	char sibling[1024];
	char *arguments[2];

	if (cached_trader_length > sizeof(cached_trader))
		return port_report_failure(error, "Genesis cached trader length");
	memcpy(cached_trader, session->player.name, cached_trader_length);
	if (!reload_player(session, error))
		return false;
	if (!session_0317(session, prophecy_one, sizeof(prophecy_one) - 1U,
	    "Genesis prophecy first row", error)
	    || !session_02fc(session, prophecy_two, sizeof(prophecy_two) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Genesis prompt leading blank", error)
	    || !yt_genesis_confirmation_prompt(cached_trader,
	    cached_trader_length, prompt, sizeof(prompt), &prompt_length)
	    || !session_a8d2(session, prompt, prompt_length, &answer, error))
		return false;
	if (session->door->game.config.genesis_ports > 300.0f) {
		if (!session_02db(session, disabled, sizeof(disabled) - 1U,
		    "Genesis disabled row", error))
			return false;
		answer = YT_YES_NO_NO;
	}
	if (answer != YT_YES_NO_YES)
		return session_0317(session, declined, sizeof(declined) - 1U,
		    "Genesis declined row", error);
	if (session->player.ports_owned
	    < session->door->game.config.genesis_ports) {
		if (!yt_genesis_insufficient_rows(
		    session->door->game.config.genesis_ports,
		    session->player.ports_owned, first, sizeof(first), &first_length,
		    second, sizeof(second), &second_length))
			return port_report_failure(error,
			    "Genesis insufficient row composition");
		return session_0317(session, first, first_length,
		    "Genesis insufficient first row", error)
		    && session_02fc(session, second, second_length);
	}
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "Genesis success leading blank", error))
		return false;
	session->presentation.bold = 1.0f;
	if (!session_02fc(session, success_one, sizeof(success_one) - 1U))
		return false;
	session->presentation.bold = 1.0f;
	if (!session_02fc(session, success_two, sizeof(success_two) - 1U))
		return false;
	/*
	 * QuickBASIC PRINT # inserts CR/LF before the DOS EOF written on close.
	 * Build that exact shape before handing off.
	 */
	{
		size_t length = strlen(session->door->command_line);
		uint8_t *line = malloc(length + 2U);
		bool ok;

		if (line == NULL) {
			if (error != NULL)
				error->status = YT_NO_MEMORY;
			return false;
		}
		memcpy(line, session->door->command_line, length);
		line[length] = '\r';
		line[length + 1U] = '\n';
		ok = yt_text_write("RMTINIT.TMP", line, length + 2U, true,
		    error);
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
projectile_plasma_fighter_owner(struct yt_session *session, float owner,
    uint8_t *label, size_t size, size_t *label_length,
    struct yt_error *error)
{
	static const uint8_t xannor[] = "The Xannor";
	static const uint8_t mercenaries[] = "Mercenaries";
	static const uint8_t you[] = "YOU";
	const uint8_t *initial = xannor;
	size_t initial_length = sizeof(xannor) - 1U;

	if (label == NULL || label_length == NULL
	    || size < YT_TEXT_FIELD_SIZE)
		return false;
	if (owner == -2.0f) {
		initial = mercenaries;
		initial_length = sizeof(mercenaries) - 1U;
	}
	memcpy(label, initial, initial_length);
	*label_length = initial_length;
	if (owner != -2.0f && owner > 1.0f
	    && owner <= session->door->game.config.sector_offset) {
		struct yt_player defender;
		bool overflow;
		int owner_record;

		owner_record = (int)qb_cint((double)owner, &overflow);
		if (overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "projectile defense-owner CINT");
			}
			return false;
		}
		if (!yt_game_read_player(&session->door->game, owner_record,
		    &defender, error))
			return false;
		if (!yt_player_stored_name(&defender, label, label_length, error))
			return false;
	}
	if (owner == (float)session->player_record) {
		memcpy(label, you, sizeof(you) - 1U);
		*label_length = sizeof(you) - 1U;
	}
	return true;
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
    struct yt_sector *sector, double *energy, struct yt_error *error)
{
	struct yt_planet planet;
	struct yt_planet updater_planet;
	bool overflow;
	int logical_planet;
	float original_ore;
	float old_total;
	float old_ground;
	uint8_t planet_name[YT_TEXT_FIELD_SIZE];
	uint8_t attacker_name[YT_TEXT_FIELD_SIZE];
	uint8_t direct_row[256];
	uint8_t news_row[256];
	size_t planet_name_length;
	size_t attacker_name_length;
	size_t direct_length;
	size_t news_length;
	char number_one[64];
	char number_two[64];
	char row[256];

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
	if (!planet_update(session, logical_planet, &updater_planet, error))
		return false;
	original_ore = updater_planet.production[0];
	if (!yt_game_read_planet(&session->door->game, logical_planet, &planet,
	    error))
		return false;
	if (!yt_planet_stored_name(&planet, planet_name, &planet_name_length,
	    error)
	    || !yt_player_stored_name(&session->player, attacker_name,
	    &attacker_name_length, error)
	    || !yt_projectile_planet_attack_rows(true, attacker_name,
	    attacker_name_length, planet_name, planet_name_length,
	    (float)sector_number, direct_row, sizeof(direct_row),
	    &direct_length, news_row, sizeof(news_row), &news_length)
	    || !session_present_text(session, direct_row, direct_length,
	    SESSION_PRESENT_LINE, "plasma planet-hit row", error)
	    || !append_news_bytes(session, news_row, news_length, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "plasma planet attack sound", error))
		return false;
	old_total = single_add(single_add(planet.production[0],
	    planet.production[1]), planet.production[2]);
	old_ground = planet.ground_forces;
	while ((original_ore > 0.0f || planet.production[1] > 0.0f
	    || planet.production[2] > 0.0f) && *energy > 0.0) {
		float quantity = (float)(*energy * 0.000004);
		float draw;
		size_t index;

		planet.ground_forces =
		    single_sub(planet.ground_forces, quantity);
		for (index = 0; index < 3; ++index)
			planet.production[index] =
			    single_sub(planet.production[index], quantity);
		if (!random_value(session, &draw, error))
			return false;
		*energy -= (double)single_mul(draw, 25000.0f);
	}
	{
		size_t index;

		for (index = 0; index < 3; ++index) {
			if (planet.production[index] < 0.0f)
				planet.production[index] = 0.0f;
			if (planet.stock[index]
			    > single_mul(planet.production[index], 10.0f))
				planet.stock[index] =
				    single_mul(planet.production[index], 10.0f);
		}
	}
	{
		float new_total = single_add(single_add(planet.production[0],
		    planet.production[1]), planet.production[2]);

		qb_str_single(number_one, sizeof(number_one),
		    single_sub(old_total, new_total));
		qb_str_single(number_two, sizeof(number_two), new_total);
		snprintf(row, sizeof(row), "Productivity reduced by%s units to%s "
		    "units!", number_one, number_two);
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE,
		    "plasma productivity row", error)
		    || !append_news(session, row, error))
			return false;
	}
	{
		struct yt_planet persistence;
		size_t index;

		if (!yt_game_read_planet(&session->door->game, logical_planet,
		    &persistence, error))
			return false;
		for (index = 0; index < 3; ++index) {
			persistence.production[index] = planet.production[index];
			persistence.stock[index] = planet.stock[index];
			if (!yt_record_set_number(&persistence.record,
			    YT_F45 + index * 4U, planet.production[index])
			    || !yt_record_set_number(&persistence.record,
			    YT_F57 + index * 4U, planet.stock[index])) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "plasma productivity overlay");
				}
				return false;
			}
		}
		planet.ground_forces = floorf(planet.ground_forces);
		if (planet.ground_forces < 1.0f) {
			planet.ground_forces = 0.0f;
			persistence.owner = 0.0f;
			if (!yt_record_set_number(&persistence.record, YT_F73,
			    0.0f)) {
				if (error != NULL) {
					error->status = YT_RANGE;
					snprintf(error->operation,
					    sizeof(error->operation), "%s",
					    "plasma planet-owner overlay");
				}
				return false;
			}
		}
		persistence.ground_forces = planet.ground_forces;
		if (!yt_record_set_number(&persistence.record, YT_F77,
		    planet.ground_forces)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation, sizeof(error->operation), "%s",
				    "plasma ground-force overlay");
			}
			return false;
		}
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &persistence, error))
			return false;
	}
	if (planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f) {
		struct yt_planet destruction;
		struct yt_sector unlink;

		if (!yt_game_read_planet(&session->door->game, logical_planet,
		    &destruction, error))
			return false;
		destruction.name_length = 0.0f;
		if (!yt_record_set_number(&destruction.record, YT_F85, 0.0f)
		    || !yt_game_write_planet(&session->door->game,
		    logical_planet, &destruction, error))
			return false;
		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &unlink, error))
			return false;
		unlink.planet = 0.0f;
		if (!yt_record_set_number(&unlink.record, YT_F93, 0.0f)
		    || !yt_game_write_sector(&session->door->game, sector_number,
		    &unlink, error))
			return false;
		if (!session_present_text(session,
		    (const uint8_t *)"The planet was destroyed!!",
		    strlen("The planet was destroyed!!"), SESSION_PRESENT_LINE,
		    "plasma planet-destroyed row", error)
		    || !session_sound(session, 3.0f,
		    "plasma planet destruction sound", error)
		    || !append_news(session, "The planet was destroyed!!", error))
			return false;
	}
	else if (old_ground != 0.0f) {
		qb_str_single(number_one, sizeof(number_one),
		    single_sub(old_ground, planet.ground_forces));
		qb_str_single(number_two, sizeof(number_two),
		    planet.ground_forces);
		snprintf(row, sizeof(row), "Ground forces reduced by%s units to%s!",
		    number_one, number_two);
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_LINE,
		    "plasma ground-force row", error)
		    || !append_news(session, row, error))
			return false;
	}
	return true;
}

static bool
plasma_sector_loaded(struct yt_session *session, int sector_number,
    const struct yt_sector *initial, double *energy, struct yt_error *error)
{
	struct yt_sector sector;
	float planet_link;
	int basic;

	if (initial != NULL)
		sector = *initial;
	else if (!yt_game_read_sector(&session->door->game, sector_number,
	    &sector, error))
		return false;
	if (sector.fighters > 0.0f) {
		double original_fighters = (double)sector.fighters;
		double destroyed = 0.0;
		uint8_t owner[YT_TEXT_FIELD_SIZE];
		size_t owner_length;
		size_t row_length;
		char sector_text[64];
		char fighter_text[64];
		uint8_t row[256];

		if (!projectile_plasma_fighter_owner(session,
		    sector.fighter_owner, owner, sizeof(owner), &owner_length,
		    error))
			return false;
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		if (!yt_projectile_defense_row((float)sector_number, owner,
		    owner_length, (double)sector.fighters, row, sizeof(row),
		    &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_LINE,
		    "plasma defense report", error))
			return false;

		session->presentation.bold = 1.0f;
		if (!session_sound(session, 2.0f,
		    "plasma fighter-defense sound", error))
			return false;
		while (*energy > 0.0 && destroyed < original_fighters) {
			float draw;

			destroyed += floor(*energy / 5000.0) + 1.0;
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (*energy < 0.0)
			*energy = 0.0;
		if (destroyed > original_fighters)
			destroyed = original_fighters;
		qb_str_double(fighter_text, sizeof(fighter_text), destroyed);
		snprintf((char *)row, sizeof(row),
		    "The plasma bolts destroyed%s fighters!",
		    fighter_text);
		if (!session_present_text(session, row,
		    strlen((const char *)row), SESSION_PRESENT_LINE,
		    "plasma destroyed-defense row", error))
			return false;
		if (destroyed > 9.0) {
			snprintf((char *)row, sizeof(row), "%s's plasma bolts destroyed%s "
			    "fighters in sector%s!", session->player.name,
			    fighter_text, sector_text);
			if (!append_news(session, (const char *)row, error))
				return false;
		}
		{
			static const uint8_t defense_zero[4] = {
				0x00, 0x00, 0x10, 0x00
			};
			struct yt_sector persistence;
			float remaining = (float)(original_fighters - destroyed);

			if (!yt_game_read_sector(&session->door->game,
			    sector_number, &persistence, error))
				return false;
			persistence.fighters = remaining;
			if (remaining == 0.0f) {
				persistence.fighter_owner = 0.0f;
				if (!yt_record_set_raw_number(&persistence.record,
				    YT_F81, defense_zero)
				    || !yt_record_set_raw_number(&persistence.record,
				    YT_F85, defense_zero)) {
					if (error != NULL) {
						error->status = YT_RANGE;
						snprintf(error->operation,
						    sizeof(error->operation), "%s",
						    "plasma defense zero overlay");
					}
					return false;
				}
			}
			if (!yt_game_write_sector(&session->door->game,
			    sector_number, &persistence, error))
				return false;
			if (remaining == 0.0f
			    && (float)sector_number
			    == session->door->game.config.headquarters
			    && !xannor_victory(session, error))
				return false;
		}
		if (*energy < 1.0)
			return true;
	}
plasma_reload_sector:
	/* B099 performs a new sector GET before caching mines and planet link. */
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	planet_link = sector.planet;
	if (*energy > 0.0 && sector.mines > 0.0f) {
		float original_mines = sector.mines;
		float destroyed = 0.0f;
		char destroyed_text[64];
		char sector_text[64];
		char row[256];

		if (!session_sound(session, 5.0f,
		    "plasma sector-mine sound", error))
			return false;
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		snprintf(row, sizeof(row), "%s's Plasma Bolts hit sector mines in "
		    "sector%s!", session->player.name, sector_text);
		if (!append_news(session, row, error))
			return false;
		while (*energy > 0.0 && destroyed < sector.mines) {
			float draw;

			destroyed = (float)((double)destroyed
			    + floor(*energy * 0.000001) + 1.0);
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (destroyed > sector.mines)
			destroyed = sector.mines;
		if (*energy < 0.0)
			*energy = 0.0;
		qb_str_single(destroyed_text, sizeof(destroyed_text), destroyed);
		snprintf(row, sizeof(row), "%s's plasma bolts destroyed%s mines in "
		    "sector%s!", session->player.name, destroyed_text,
		    sector_text);
		if (!append_news(session, row, error))
			return false;
		snprintf(row, sizeof(row), "The plasma bolts destroyed%s mines in "
		    "sector%s!", destroyed_text, sector_text);
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_BOLD_LINE,
		    "plasma destroyed-mines row", error))
			return false;
		{
			struct yt_sector persistence;

			if (!yt_game_read_sector(&session->door->game,
			    sector_number, &persistence, error))
				return false;
			persistence.mines = single_sub(original_mines, destroyed);
			if (!yt_record_set_number(&persistence.record, YT_F129,
			    persistence.mines)
			    || !yt_game_write_sector(&session->door->game,
			    sector_number, &persistence, error))
				return false;
		}
		if (*energy < 1.0)
			return true;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset
	    && *energy > 0.0; ++basic) {
		struct yt_player target;
		double original_fighters;
		float original_shields;
		double fighter_damage = 0.0;
		float shield_damage = 0.0f;
		int saved_foreground;
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

		if (session->sector_cache[basic] != (float)sector_number)
			continue;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		original_fighters = (double)target.fighters;
		original_shields = target.shields;
		saved_foreground = session->pager.foreground;
		session_set_color(session, 5);
		if (!session_sound(session, 2.0f,
		    "plasma player-attack sound", error))
			return false;
		while (*energy > 0.0 && fighter_damage < original_fighters) {
			float draw;

			fighter_damage += floor(*energy / 5000.0) + 1.0;
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (fighter_damage > original_fighters)
			fighter_damage = original_fighters;
		while (*energy > 0.0 && shield_damage < original_shields) {
			float draw;

			shield_damage = (float)((double)shield_damage
			    + floor(*energy / 10000.0) + 1.0);
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (shield_damage > original_shields)
			shield_damage = original_shields;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		qb_str_single(shield_text, sizeof(shield_text),
		    single_sub(original_shields, shield_damage));
		qb_str_double(fighter_text, sizeof(fighter_text), fighter_damage);
		if (!yt_player_stored_name(&session->player, attacker_name,
		    &attacker_length, error)
		    || !yt_player_stored_name(&target, victim_name,
		    &victim_length, error)
		    || !yt_projectile_attack_first_rows(true,
		    attacker_name, attacker_length, victim_name, victim_length,
		    (float)sector_number, first_news, sizeof(first_news),
		    &first_news_length, first_direct, sizeof(first_direct),
		    &first_direct_length)
		    || !append_news_bytes(session, first_news, first_news_length,
		    error)
		    || !session_present_text(session, first_direct,
		    first_direct_length, SESSION_PRESENT_BOLD_LINE,
		    "plasma player attack first row", error))
			return false;
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!append_news(session, row, error))
			return false;
		if (!session_present_text(session, (const uint8_t *)row,
		    strlen(row), SESSION_PRESENT_BOLD_LINE,
		    "plasma player attack second row", error))
			return false;
		session_set_color(session, saved_foreground);
		if (single_sub(original_shields, shield_damage) < 1.0f) {
			struct yt_player victim;
			float mines;
			uint8_t killed_name[YT_TEXT_FIELD_SIZE];
			uint8_t destroyed_row[128];
			uint8_t warning_row[160];
			size_t killed_name_length;
			size_t destroyed_length;
			size_t warning_length;

			if (!yt_game_read_player(&session->door->game, basic,
			    &victim, error))
				return false;
			mines = victim.mines;
			if (basic != session->player_record
			    && (!yt_player_stored_name(&victim, killed_name,
			    &killed_name_length, error)
			    || !yt_projectile_destroyed_rows(killed_name,
			    killed_name_length, destroyed_row, sizeof(destroyed_row),
			    &destroyed_length, warning_row, sizeof(warning_row),
			    &warning_length)))
				return false;
			victim.mines = 0.0f;
			victim.danger_scanner = 0.0f;
			if (!yt_record_set_number(&victim.record, YT_F129, 0.0f)
			    || !yt_record_set_number(&victim.record, YT_F93, 0.0f)
			    || !yt_game_write_player(&session->door->game, basic,
			    &victim, error))
				return false;

			session->presentation.blink = 1.0f;
			if (basic == session->player_record) {
				if (!session_present_text(session,
				    (const uint8_t *)"YOU were destroyed!",
				    strlen("YOU were destroyed!"),
				    SESSION_PRESENT_BOLD_LINE,
				    "plasma self-destruction row", error))
					return false;
			}
			else {
				if (!session_present_text(session, destroyed_row,
				    destroyed_length, SESSION_PRESENT_BOLD_LINE,
				    "plasma victim-destruction row", error))
					return false;
			}
			if (mines != 0.0f) {
				if (!yt_player_stored_name(&victim, killed_name,
				    &killed_name_length, error)
				    || !yt_projectile_destroyed_rows(killed_name,
				    killed_name_length, destroyed_row,
				    sizeof(destroyed_row), &destroyed_length,
				    warning_row, sizeof(warning_row),
				    &warning_length))
					return false;
				session->presentation.blink = 1.0f;
				if (!session_present_text(session, warning_row,
				    warning_length, SESSION_PRESENT_BOLD_LINE,
				    "plasma carried-mine warning", error))
					return false;
			}
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number, mines,
			    error))
				return false;
			if (basic == session->player_record) {
				session->player = victim;
				session->destroyed = true;
				session->sector_cache[basic] = 0.0f;
			}
			else {
				if (!kill_player(session, basic,
				    (float)session->player_record, error))
					return false;
				if (!session_sound(session, 3.0f,
				    "plasma salvage sound", error)
				    || !salvage_player(session, basic,
				    (float)session->player_record, error))
					return false;
			}
			if (*energy > 0.0 && mines > 0.0f)
				goto plasma_reload_sector;
		}
		else {
			struct yt_player persistence;

			if (!yt_game_read_player(&session->door->game, basic,
			    &persistence, error))
				return false;
			persistence.shields =
			    single_sub(original_shields, shield_damage);
			persistence.fighters =
			    (float)(original_fighters - fighter_damage);
			if (!yt_record_set_number(&persistence.record, YT_F53,
			    persistence.shields)
			    || !yt_record_set_number(&persistence.record, YT_F61,
			    persistence.fighters)
			    || !yt_game_write_player(&session->door->game, basic,
			    &persistence, error))
				return false;
		}
	}
	/* The B099 dispatch cached this link before mines and the player scan. */
	sector.planet = planet_link;
	return plasma_planet_impact(session, sector_number, &sector, energy,
	    error);
}

static bool
plasma_sector(struct yt_session *session, int sector_number,
    double *energy, struct yt_error *error)
{
	return plasma_sector_loaded(session, sector_number, NULL, energy, error);
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
plasma_footer(struct yt_session *session, struct yt_error *error)
{
	return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "plasma footer leading blank", error)
	    && session_present_text(session,
	    (const uint8_t *)"Plasma bolts dissipated.",
	    strlen("Plasma bolts dissipated."), SESSION_PRESENT_LINE,
	    "plasma footer row", error)
	    && session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "plasma footer trailing blank", error);
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
	if (!plasma_sector_loaded(session, hop, &sector, energy, error))
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
	    &last_mine_news_sector, &energy, &hop_loss, error))
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
			found = plasma_sector(session, start, &energy, error);
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
			if (!plasma_sector(session, next, &energy, error)) {
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
command_projectile(struct yt_session *session, bool plasma,
    struct yt_error *error)
{
	static const uint8_t no_ammunition[] = "You dont have any!";
	static const uint8_t invalid_sector[] = "Invalid Sector number!";
	static const uint8_t excessive[] = "You dont have that many!";
	static const uint8_t quantity_prompt[] = "Send how many? [0] ?";
	float displayed = plasma ? session->player.plasma
	    : session->player.missiles;
	float available = 0.0f;
	float target;
	float amount;
	float origin;
	int counterattack = 0;
	int xannor_provoker = 0;

	for (;;) {
		uint8_t prompt[192];
		char response[160];
		bool denied;
		size_t prompt_length;
		enum yt_projectile_target_result target_result;

		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "projectile target opening blank", error)
		    || !reload_player(session, error)
		    || !fresh_no_turn_gate(session, &denied, error))
			return false;
		if (denied)
			return true;
		available = plasma ? session->player.plasma
		    : session->player.missiles;
		if (available < 1.0f)
			return session_02db(session, no_ammunition,
			    sizeof(no_ammunition) - 1U,
			    "projectile ammunition refusal", error);
		if (!yt_projectile_target_prompt(plasma, displayed,
		    (float)sector_count(session), prompt, sizeof(prompt),
		    &prompt_length)
		    || !session_031f(session, prompt, prompt_length,
		    "projectile target prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		target_result = yt_projectile_target_response(response,
		    (float)sector_count(session), &target);
		if (target_result == YT_PROJECTILE_TARGET_CANCEL)
			return true;
		if (target_result == YT_PROJECTILE_TARGET_ACCEPT)
			break;
		if (!session_02db(session, invalid_sector,
		    sizeof(invalid_sector) - 1U,
		    "projectile invalid sector", error))
			return false;
	}
	{
		char response[160];

		if (!session_031f(session, quantity_prompt,
		    sizeof(quantity_prompt) - 1U, "projectile quantity prompt",
		    error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		amount = yt_projectile_quantity_response(response);
	}
	if (amount < 1.0f)
		return true;
	if (amount > available) {
		return session_02fc(session, excessive, sizeof(excessive) - 1U);
	}
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "projectile accepted blank", error)
	    || !finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	origin = session->player.sector;
	if (!yt_projectile_commit(&session->door->game, session->player_record,
	    &session->player, plasma, &origin, target, amount,
	    &session->destroyed, &counterattack, &xannor_provoker,
	    session_projectile_resolver, session, error)
	    || !launch_player_counterattack(session, &counterattack,
	    &xannor_provoker, error)
	    || !launch_xannor_retaliation(session, &xannor_provoker, error))
		return false;
	if (session->destroyed)
		return common_fatal_self(session, error);
	return true;
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
							yt_out_line("Message full!");
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
						yt_out("\b \b");
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
	struct qb_val_result parsed;
	enum yt_yes_no_answer answer;
	char response[160];
	float maximum = single_sub(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);
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
	int hops = 0;

	if (!autopilot) {
		session->computer_path_marker = 9999.0f;
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_031f(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		if (parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s", "path start VAL");
			}
			return false;
		}
		session->computer_path_start =
		    parsed.valid ? (float)qb_int(parsed.value) : 0.0f;
	}
	else if (!stale_marker)
		session->computer_path_start = session->player.sector;
	start_value = session->computer_path_start;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_031f(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s", "path destination VAL");
		}
		return false;
	}
	destination_value = parsed.valid ? (float)qb_int(parsed.value) : 0.0f;
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
	if (!build_route(session, start_value, destination_value, route, true,
	    &found, NULL, NULL, error)) {
		free(route);
		return false;
	}
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

		cursor = route[cursor];
		++hops;
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0
		    || snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0
		    || !session_031f(session, (const uint8_t *)token,
		    strlen(token), "path route token", error)) {
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

		if (qb_str_single(hop_text, sizeof(hop_text), (float)hops) < 0
		    || snprintf(course, sizeof(course),
		    "Course will take%s turns.", hop_text) < 0
		    || !session_0317(session, (const uint8_t *)course,
		    strlen(course), "path course row", error)) {
			free(route);
			return false;
		}
	}
	session->computer_path_marker = 0.0f;
	if (!autopilot || stale_marker) {
		free(route);
		return true;
	}
	if (!reload_player(session, error)) {
		free(route);
		return false;
	}
	if ((float)hops > session->player.turns) {
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
			char commands[YT_COMMAND_SIZE] = "1";
			size_t used = 1U;

			if (!session_0317(session, engaged, sizeof(engaged) - 1U,
			    "autopilot engaged row", error)
			    || !session_0317(session, stop_notice,
			    sizeof(stop_notice) - 1U,
			    "autopilot stop row", error)) {
				free(route);
				return false;
			}
			cursor = start;
			while (route[cursor] != 0) {
				char number[64];
				int written;

				cursor = route[cursor];
				if (qb_str_single(number, sizeof(number),
				    (float)cursor) < 0) {
					free(route);
					return false;
				}
				written = snprintf(commands + used,
				    sizeof(commands) - used, ";M;%s", number);
				if (written < 0 || (size_t)written
				    >= sizeof(commands) - used) {
					free(route);
					return false;
				}
				used += (size_t)written;
			}
			if (!queue_commands(session, commands)) {
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
		for (index = 0; index < 6U; ++index)
			session->current_warps[index] = current_sector.warps[index];
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
computer_owned_planets(struct yt_session *session, struct yt_error *error)
{
	int sector_number;
	bool found = false;

	session_set_color(session, 2);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "owned-planet opening blank", error)
	    || !session_present_text(session, (const uint8_t *)"Scanning...",
	    strlen("Scanning..."), SESSION_PRESENT_LINE,
	    "owned-planet scanning row", error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "owned-planet scanning blank", error))
		return false;
	session_set_color(session, 3);
	for (sector_number = 1; sector_number <= sector_count(session);
	    ++sector_number) {
		struct yt_sector sector;
		struct yt_planet planet;

		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
		if (sector.planet == 0.0f)
			continue;
		if (!yt_game_read_planet(&session->door->game,
		    (int)sector.planet, &planet, error))
			return false;
		if (planet.owner == (float)session->player_record) {
			static const uint8_t prefix[] = "Planet: ";
			static const uint8_t infix[] = " Sector:";
			uint8_t row[128];
			char number[64];
			size_t length = 0;

			if (qb_str_single(number, sizeof(number),
			    (float)sector_number) < 0)
				return port_report_failure(error,
				    "owned-planet sector format");
			memcpy(row + length, prefix, sizeof(prefix) - 1U);
			length += sizeof(prefix) - 1U;
			memcpy(row + length, planet.record.bytes,
			    YT_TEXT_FIELD_SIZE);
			length += YT_TEXT_FIELD_SIZE;
			memcpy(row + length, infix, sizeof(infix) - 1U);
			length += sizeof(infix) - 1U;
			memcpy(row + length, number, strlen(number));
			length += strlen(number);
			if (!session_present_text(session, row, length,
			    SESSION_PRESENT_BOLD_LINE,
			    "owned-planet match row", error))
				return false;
			found = true;
		}
	}
	if (!found) {
		session->presentation.blink = 1.0f;
		return session_present_text(session,
		    (const uint8_t *)"None found!", strlen("None found!"),
		    SESSION_PRESENT_BOLD_LINE, "owned-planet none row", error);
	}
	return true;
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
computer_port_report(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] = "Enter sector number port is in -=> ";
	static const uint8_t unavailable[] = "No information available.";
	float maximum = single_sub(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);
	float cached_team = session->player.team;
	char response[80];
	float selected;
	int sector_number;
	struct yt_sector sector;
	bool friendly;
	bool denied;

	for (;;) {
		struct qb_val_result parsed;

		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "computer port sector blank", error)
		    || !session_031f(session, prompt, sizeof(prompt) - 1U,
		    "computer port sector prompt", error)
		    || !session_0345(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		parsed = qb_val(response);
		if (parsed.overflow) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "computer port sector VAL");
			}
			return false;
		}
		selected = parsed.valid ? (float)qb_int(parsed.value) : 0.0f;
		if (selected <= maximum && selected >= 1.0f)
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
	if (!computer_port_friendship(session, sector.fighter_owner,
	    &friendly, error))
		return false;
	denied = sector.port == 0.0f
	    || (sector.fighters > 0.0f && cached_team > 0.0f && !friendly)
	    || (sector.fighters > 0.0f && cached_team == 0.0f
	    && (float)session->player_record != sector.fighter_owner);
	if (denied)
		return session_0317(session, unavailable,
		    sizeof(unavailable) - 1U,
		    "computer port unavailable", error);
	if (sector.port == 1.0f)
		return earth_store(session, error);
	{
		struct yt_sector updater_sector;
		struct yt_port port;
		float prices[3];
		double quantities[3];
		int logical_port;

		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &updater_sector, error))
			return false;
		logical_port = (int)updater_sector.port;
		if (!port_update(session, logical_port, &port, prices, quantities,
		    error))
			return false;
		return port_report(session, logical_port, &port, prices,
		    quantities, error);
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
	struct qb_val_result parsed;
	char response[80];
	float slot_value;
	float maximum;
	float new_value;
	float old_value;
	bool overflow;
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
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "avoid slot VAL");
		}
		return false;
	}
	slot_value = parsed.valid ? (float)parsed.value : 0.0f;
	if (slot_value < 1.0f || slot_value > 30.0f)
		return true;
	slot = (int)qb_cint_mode((double)slot_value,
	    session->presentation.sound.conversion_mode, &overflow);
	if (overflow || slot < 1 || slot > 30) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "avoid slot CINT");
		}
		return false;
	}
	maximum = single_sub(session->door->game.config.port_offset,
	    session->door->game.config.sector_offset);
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
	parsed = qb_val(response);
	if (parsed.overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "avoid sector VAL");
		}
		return false;
	}
	new_value = parsed.valid ? (float)parsed.value : 0.0f;
	if (new_value < 0.0f || new_value > maximum)
		return true;
	old_value = session->avoid[slot - 1];
	session->avoid[slot - 1] = new_value;
	session->presentation.foreground = 2.0f;
	session->pager.foreground = 2;
	if (new_value != 0.0f) {
		char number[64];
		char status[128];

		if (qb_str_single(number, sizeof(number), new_value) < 0
		    || snprintf(status, sizeof(status),
		    "Sector%s now locked out.", number) < 0
		    || !session_0317(session, (const uint8_t *)status,
		    strlen(status), "avoid locked status", error))
			return false;
	}
	if (old_value != 0.0f && old_value != new_value) {
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

static bool
computer_scoreboard(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Enter 'O' to see OLD scoreboard or press [ENTER] for UPDATED one. -=>";
	static const uint8_t heading[] = "P l a y e r  R a n k i n g s";
	char response[80];

	session->pager.key[0] = '\0';
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "scoreboard selector leading blank", error)
	    || !session_031f(session, prompt, sizeof(prompt) - 1U,
	    "scoreboard selector prompt", error)
	    || !session_0357(session, response, sizeof(response)))
		return false;
	session->pager.line_count = 0.0f;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "scoreboard selector trailing blank", error))
		return false;
	if (strcmp(response, "O") != 0) {
		if (!session_031f(session, heading, sizeof(heading) - 1U,
		    "scoreboard update heading", error)
		    || !yt_score_generate_progress(&session->door->game,
		    computer_scoreboard_progress, session, error)
		    || !session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "scoreboard post-generator blank",
		    error))
			return false;
	}
	return display_game_file(session,
	    session->door->game.config.scoreboard, error);
}

static bool
computer_newspaper(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "Do you want to read [T]oday's or [Y]esterday's news? [T/Y] -=> ";
	char response[80] = "";

	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "newspaper selector leading blank", error))
		return false;
	while (strcmp(response, "T") != 0 && strcmp(response, "Y") != 0) {
		if (!session_031f(session, prompt, sizeof(prompt) - 1U,
		    "newspaper selector prompt", error)
		    || !session_0357(session, response, sizeof(response)))
			return false;
	}
	return display_game_file(session,
	    strcmp(response, "T") == 0 ? "ytnews.dat" : "YTYNEWS.DAT",
	    error);
}

static bool
computer_menu(struct yt_session *session, bool *enter_sector,
    struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] = "Computer command (?=help)? ";

	if (enter_sector != NULL)
		*enter_sector = false;
	if (!computer_activate(session, error))
		return false;
	for (;;) {
		char command[80];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		int position;

		if (!computer_prompt_hydrate(session, error))
			return false;
		session->relationship_scratch = 0.0f;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "computer prompt leading blank", error))
			return false;
		session->presentation.foreground = 1.0f;
		session->pager.foreground = 1;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "computer prompt time capacity");
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
		    "computer prompt", error)
		    || !session_0357(session, command, sizeof(command)))
			return false;
		if (command[0] == '\0')
			strcpy(command, "?");
		command[2] = '\0';

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
				if (!command_trade(session, error))
					return false;
				if (enter_sector != NULL)
					*enter_sector = true;
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
				if (!computer_port_report(session, error))
					return false;
				continue;
			case 8:
				if (!computer_route(session, true, error))
					return false;
				return true;
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

static bool
command_shell(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt_prefix[] = "Time:";
	static const uint8_t prompt_body[] = "Main Command (?=Help)? ";

	while (session->running && !session->destroyed) {
		char command[YT_COMMAND_SIZE];
		uint8_t prompt[sizeof(prompt_prefix) - 1U
		    + sizeof(session->time.text) + sizeof(prompt_body) - 1U];
		size_t prompt_length = 0;
		enum yt_main_shell_route route;
		bool enter_sector = false;

		session->pager.line_count = 0.0f;
		if (!reload_player(session, error))
			return false;
		session->presentation.foreground = 2.0f;
		session->pager.foreground = 2;
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "main prompt leading blank", error))
			return false;
		session->relationship_scratch = 0.0f;
		memcpy(prompt + prompt_length, prompt_prefix,
		    sizeof(prompt_prefix) - 1U);
		prompt_length += sizeof(prompt_prefix) - 1U;
		if (session->time.text_length > sizeof(session->time.text)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "main prompt time capacity");
			}
			return false;
		}
		memcpy(prompt + prompt_length, session->time.text,
		    session->time.text_length);
		prompt_length += session->time.text_length;
		memcpy(prompt + prompt_length, prompt_body,
		    sizeof(prompt_body) - 1U);
		prompt_length += sizeof(prompt_body) - 1U;
		memcpy(session->output_source, prompt, prompt_length);
		session->output_source[prompt_length] = '\0';
		if (!session_031f(session,
		    (const uint8_t *)session->output_source, prompt_length,
		    "main prompt low-time warning", error))
			return false;
		if (!session_0357(session, command, sizeof(command)))
			return true;
		memcpy(session->output_source, command, strlen(command) + 1U);
		route = yt_main_shell_dispatch(session->output_source);
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
			if (!command_buy_port(session, error))
				return false;
			if (!display_current_sector_cached(session, error))
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
			if (!command_trade(session, error))
				return false;
			enter_sector = true;
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
			if (!command_rename_port(session, error))
				return false;
			if (!display_current_sector_cached(session, error))
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
    struct yt_error *error)
{
	struct yt_session session;
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
	session.door = door;
	session.executable_path = executable_path;
	session.running = true;
	session.presentation.sound.ansi = door->identity.ansi ? -1.0f : 0.0f;
	session.presentation.sound.mode = door->identity.local ? 1.0f : 0.0f;
	session.presentation.sound.user_sound = -1.0f;
	session.presentation.sound.local_sound =
	    door->identity.local ? -1.0f : 0.0f;
	session.presentation.foreground = 7.0f;
	session.pager.foreground = 7;
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
