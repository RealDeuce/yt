#include "yt_session.h"

#include "qb.h"
#include "yt_file.h"
#include "yt_input.h"
#include "yt_maint.h"
#include "yt_names.h"
#include "yt_output.h"
#include "yt_pager.h"
#include "yt_platform.h"
#include "yt_score.h"
#include "yt_sound.h"
#include "yt_text.h"

#include <ctype.h>
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
	struct yt_input_splitter input;
	char saved_command[YT_COMMAND_SIZE];
	bool registered;
	bool running;
	bool terminated;
	bool destroyed;
	struct yt_present_state presentation;
	bool suppress_self_mines;
	bool mercenaries_hurt;
	float clearance_holds;
	float clearance_fighters;
	float clearance_ground;
	float clearance_shields;
	float counterlaunch_count;
	float relationship_scratch;
	bool anti_cloak;
	int spies[3];
	int spy_marker[3];
	int spy_count;
	float avoid[30];
	float session_deadline;
	struct yt_present_time_state time;
	struct yt_pager_state pager;
	int input_residue;
	char team_audit_message[YT_COMMAND_SIZE];
	float team_roster_cache[4];
};

struct yt_team {
	int id;
	struct yt_sector overlay;
	char name[42];
	char password[5];
	float captain;
	float roster[4];
	bool live;
	bool full;
};

static bool random_value(struct yt_session *session, float *value,
    struct yt_error *error);
static bool computer_spies(struct yt_session *session);
static bool mine_encounter(struct yt_session *session,
    struct yt_error *error);
static bool clearance(struct yt_session *session, bool create,
    struct yt_error *error);
static bool launch_xannor_retaliation(struct yt_session *session,
    int provoking_player, struct yt_error *error);
static void clear_queue(struct yt_session *session);
static bool show_ship(struct yt_session *session, struct yt_error *error);
static bool info_refresh_time(struct yt_session *session,
    struct yt_error *error);
static bool session_carrier(struct yt_session *session);
static bool session_b05d(struct yt_session *session, const uint8_t *text,
    size_t length);

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

static void
session_timed_wait(struct yt_session *session, double seconds)
{
	double deadline = yt_platform_timer() + seconds;

	while (yt_platform_timer() < deadline) {
		struct yt_input_value selected;

		if (!yt_input_poll_legacy(&session->input,
		    session->presentation.sound.mode, YT_INPUT_PHASE_WAIT,
		    &selected) || selected.length != 0)
			return;
		od_sleep(10);
	}
}

static bool
session_editor_end(struct yt_session *session, const uint8_t *notice,
    size_t notice_length)
{
	struct yt_present_result presentation;
	enum yt_present_status status = yt_present_line(NULL, 0,
	    &session->presentation, &presentation);

	if (status != YT_PRESENT_OK)
		return false;
	yt_out_present_result(&presentation);
	if (!session_b05d(session, notice, notice_length))
		return false;
	session->running = false;
	session->terminated = true;
	return false;
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

static int
planet_count(const struct yt_session *session)
{
	return (int)(session->door->game.config.total_records
	    - session->door->game.config.planet_offset);
}

static bool
write_player(struct yt_session *session, struct yt_error *error)
{
	return yt_game_write_player(&session->door->game,
	    session->player_record, &session->player, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
reload_player(struct yt_session *session, struct yt_error *error)
{
	return yt_game_read_player(&session->door->game,
	    session->player_record, &session->player, error);
}

static void
format_number(char *dest, size_t size, double value)
{
	if (qb_print_number(dest, size, value) < 0 && size > 0)
		dest[0] = '\0';
}

static bool
append_news(struct yt_session *session, const char *text,
    struct yt_error *error)
{
	(void)session;
	return yt_news_append(text, error);
}

static bool
radio_append(const char *text, float sender, float recipient,
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
	memset(&record, 0, sizeof(record));
	yt_radio_set_number(&record, 0, recipient == -2.0f ? 30.0f : 1.0f);
	yt_radio_set_number(&record, 4, recipient);
	yt_radio_set_number(&record, 8, sender);
	yt_radio_set_text(&record, (const uint8_t *)text, strlen(text), 74);
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
read_physical_line(FILE *file, char *dest, size_t size)
{
	size_t length;

	if (size == 0 || fgets(dest, (int)size, file) == NULL)
		return false;
	length = strlen(dest);
	while (length > 0 && (dest[length - 1] == '\r'
	    || dest[length - 1] == '\n' || dest[length - 1] == 0x1a))
		dest[--length] = '\0';
	return true;
}

static bool
read_keyboard_line(struct yt_session *session, char *dest, size_t size)
{
	static const uint8_t local_erase[] = {0x1d, ' ', 0x1d};
	static const uint8_t remote_erase[] = {'\b', ' ', '\b'};
	static const uint8_t inactivity_notice[] =
	    "\aUSER FELL ASLEEP!";
	static const uint8_t session_notice[] =
	    "\a\a\aTIME LIMIT EXCEEDED!\a\a\a";
	size_t used;
	float inactivity_deadline =
	    single_add(floorf((float)yt_platform_timer()), 180.0f);

	if (size == 0)
		return false;
	yt_pager_editor_enter(&session->pager, session->command_accumulator,
	    sizeof(session->command_accumulator));
	dest[0] = '\0';
	for (;;) {
		struct yt_input_value selected = {{0, 0}, 0, 0};
		struct yt_present_result presentation;
		enum yt_present_status status;
		bool queued = session->queue_position < session->queue_length;
		uint8_t key;

		if (session->presentation.sound.mode != 1.0f
		    && (float)yt_platform_timer() > inactivity_deadline)
			return session_editor_end(session, inactivity_notice,
			    sizeof(inactivity_notice) - 1U);
		if (!session_carrier(session))
			return false;
		if (!info_refresh_time(session, NULL))
			return false;
		if ((float)yt_platform_timer() > session->session_deadline)
			return session_editor_end(session, session_notice,
			    sizeof(session_notice) - 1U);
		if (queued) {
			selected.bytes[0] = (uint8_t)
			    session->queue[session->queue_position++];
			selected.length = 1;
			if (session->queue_position >= session->queue_length)
				clear_queue(session);
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
		used = strlen(session->command_accumulator);
		if (!queued && key == 0x12) {
			uint8_t prefix[YT_COMMAND_SIZE];

			memcpy(prefix, session->command_accumulator, used);
			session->pager.newline_flag = 1.0f;
			if (!session_b05d(session, prefix, used))
				return false;
			snprintf(session->command_accumulator,
			    sizeof(session->command_accumulator), "%s",
			    session->saved_command);
			key = '\r';
			used = strlen(session->command_accumulator);
		}
		if (key == '\r') {
			session->pager.newline_flag = 0.0f;
			status = yt_present_line(NULL, 0, &session->presentation,
			    &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			snprintf(dest, size, "%s", session->command_accumulator);
			return true;
		}
		if ((key == 8 || key == 127) && used > 0) {
			session->command_accumulator[--used] = '\0';
			status = yt_present_editor_echo(local_erase,
			    sizeof(local_erase), remote_erase,
			    sizeof(remote_erase), &session->presentation,
			    &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			continue;
		}
		if (key >= 0x20 && key <= 0x7f
		    && used + 1U < sizeof(session->command_accumulator)
		    && used + 1U < size) {
			session->command_accumulator[used++] = (char)key;
			session->command_accumulator[used] = '\0';
			status = yt_present_editor_echo(&key, 1, &key, 1,
			    &session->presentation, &presentation);
			if (status != YT_PRESENT_OK)
				return false;
			yt_out_present_result(&presentation);
			session->pager.newline_flag = 1.0f;
			if (!session_carrier(session))
				return false;
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
queue_remainder(struct yt_session *session, const char *text)
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
expand_repeat(struct yt_session *session, char *text, size_t size)
{
	char upper[YT_COMMAND_SIZE];
	char built[YT_COMMAND_SIZE];
	char base[YT_COMMAND_SIZE];
	char *repeat;
	struct qb_val_result parsed;
	float count;
	int copies;
	size_t prefix;
	size_t used = 0;
	int index;

	snprintf(upper, sizeof(upper), "%s", text);
	qb_compat_upper(upper);
	repeat = strstr(upper, "/R");
	if (repeat == NULL)
		return true;
	prefix = (size_t)(repeat - upper);
	if (prefix + 2U > sizeof(base))
		return false;
	memcpy(base, text, prefix);
	base[prefix] = ';';
	base[prefix + 1U] = '\0';
	parsed = qb_val(text + prefix + 2U);
	count = (float)qb_int(parsed.valid ? parsed.value : 0.0);
	if (count > 10.0f)
		count = 20.0f;
	copies = count >= 2.0f ? (int)count : 1;
	for (index = 0; index < copies; ++index) {
		size_t amount = strlen(base);

		if (used > 500U)
			break;
		if (used + amount + 1U >= sizeof(built))
			return false;
		memcpy(built + used, base, amount);
		used += amount;
	}
	if (used > 0)
		--used;
	built[used] = '\0';
	if (used >= size)
		return false;
	snprintf(text, size, "%s", built);
	snprintf(session->saved_command, sizeof(session->saved_command), "%s",
	    built);
	{
		char rendered[64];

		qb_str_double(rendered, sizeof(rendered), (double)count);
		yt_outf("Command Repeated%s times -+- Ctrl-R to Re-use -+- "
		    "Ctrl-X to cancel.\r\n", rendered);
	}
	yt_platform_delay(1000);
	return true;
}

static bool
session_line(struct yt_session *session, char *text, size_t size)
{
	char *semicolon;
	size_t length;

	if (!read_keyboard_line(session, text, size))
		return false;
	length = strlen(text);
	if (length > 0 && text[length - 1U] == '/') {
		text[--length] = '\0';
		clear_queue(session);
		snprintf(session->saved_command, sizeof(session->saved_command),
		    "%s", text);
		yt_out_line(
		    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.");
		yt_platform_delay(1000);
	}
	if (!expand_repeat(session, text, size))
		return false;
	semicolon = strchr(text, ';');
	if (semicolon != NULL) {
		*semicolon++ = '\0';
		if (!queue_remainder(session, semicolon))
			return false;
	}
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
	static const uint8_t notice[] =
	    "(**CARRIER DROPPED**) Returning to bbs!";
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (session->presentation.sound.mode != 0.0f || od_carrier())
		return true;
	status = yt_present_local_line(notice, sizeof(notice) - 1U,
	    &session->presentation, &presentation);
	if (status == YT_PRESENT_OK)
		yt_out_present_result(&presentation);
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
session_attention(struct yt_session *session, const char *text,
    const char *operation, struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;

	status = yt_present_attention((const uint8_t *)text, strlen(text),
	    &session->presentation, &presentation);
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
session_press_any_key(struct yt_session *session, bool drain,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	float saved_foreground;
	int saved_session_foreground = session->pager.foreground;
	float deadline;

	if (drain) {
		for (;;) {
			struct yt_input_value selected;

			if (!session_poll_merged(session, &selected))
				goto failed;
			if (selected.length == 0)
				break;
			if (!session_poll_merged(session, &selected))
				goto failed;
			session->input_residue = selected.length == 1
			    ? selected.bytes[0] : 0;
		}
	}
	status = yt_present_press_prompt(&session->presentation,
	    &presentation, &saved_foreground);
	if (status != YT_PRESENT_OK)
		goto failed;
	session->pager.foreground = 3;
	yt_out_present_result(&presentation);
	deadline = single_add((float)yt_platform_timer(), 33.0f);
	for (;;) {
		float current = (float)yt_platform_timer();
		struct yt_input_value selected;

		if (current >= deadline)
			break;
		if (!session_poll_merged(session, &selected))
			goto failed;
		if (selected.length != 0)
			break;
		od_sleep(10);
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
		error->status = YT_RANGE;
		snprintf(error->operation, sizeof(error->operation),
		    "press any key presentation");
	}
	return false;
}

static bool
session_framed_row(struct yt_session *session, const char *text,
    bool alert, const char *operation, struct yt_error *error)
{
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    operation, error))
		return false;
	if (alert) {
		session->presentation.bold = 1.0f;
		session->presentation.blink = 1.0f;
		clear_queue(session);
	}
	return session_b05d(session, (const uint8_t *)text, strlen(text));
}

static bool
session_fixed_width(struct yt_session *session, const char *text,
    float width, const char *operation, struct yt_error *error)
{
	uint8_t mutable[256];
	size_t length = strlen(text);
	struct yt_present_result presentation;
	enum yt_present_status status;

	if (length > sizeof(mutable)) {
		status = YT_PRESENT_CAPACITY;
	}
	else {
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
display_game_file(struct yt_session *session, const char *path,
    struct yt_error *error)
{
	struct yt_text_file file;
	size_t cursor = 0;

	if (!session_b05d(session, (const uint8_t *)"Cntl-X to Stop",
	    strlen("Cntl-X to Stop")))
		return false;
	if (!yt_text_read(path, &file, error)) {
		char message[640];

		yt_error_clear(error);
		snprintf(message, sizeof(message),
		    "*** GAME FILE [%s] NOT FOUND! ***", path);
		if (!session_b05d(session, (const uint8_t *)"", 0)
		    || !session_b05d(session, (const uint8_t *)message,
		    strlen(message))
		    || !append_news(session, message, error))
			return false;
		return true;
	}
	while (cursor < file.length && file.data[cursor] != 0x1a
	    && strcmp(session->pager.key, "Q") != 0) {
		size_t start = cursor;
		size_t length;
		int color = 2;

		while (cursor < file.length && file.data[cursor] != '\r'
		    && file.data[cursor] != '\n'
		    && file.data[cursor] != 0x1a)
			++cursor;
		length = cursor - start;
		if (length >= 4U
		    && memcmp(file.data + start, "  - ", 4) == 0)
			color = 3;
		else if (length >= 4U
		    && memcmp(file.data + start, " ***", 4) == 0)
			color = 4;
		else if (length >= 4U
		    && memcmp(file.data + start, " +++", 4) == 0)
			color = 1;
		else if (length >= 3U
		    && memcmp(file.data + start, "-=*", 3) == 0)
			color = 7;
		session_set_color(session, color);
		if (!session_b05d(session, file.data + start, length)) {
			yt_text_free(&file);
			return false;
		}
		if (cursor < file.length && file.data[cursor] == '\r')
			++cursor;
		if (cursor < file.length && file.data[cursor] == '\n')
			++cursor;
	}
	yt_text_free(&file);
	return true;
}

static bool
display_line_file(const char *path, struct yt_error *error)
{
	struct yt_text_file file;
	size_t cursor = 0;

	if (!yt_text_read(path, &file, error))
		return false;
	while (cursor < file.length && file.data[cursor] != 0x1a) {
		size_t start = cursor;

		while (cursor < file.length && file.data[cursor] != '\r'
		    && file.data[cursor] != '\n'
		    && file.data[cursor] != 0x1a)
			++cursor;
		yt_out_bytes(file.data + start, cursor - start);
		yt_out("\r\n");
		if (cursor < file.length && file.data[cursor] == '\r')
			++cursor;
		if (cursor < file.length && file.data[cursor] == '\n')
			++cursor;
	}
	yt_text_free(&file);
	return true;
}

static bool
session_yes_no(struct yt_session *session, const char *prompt,
    bool blank_yes, bool *answer)
{
	char line[80];

	for (;;) {
		yt_out(prompt);
		if (!session_line(session, line, sizeof(line)))
			return false;
		qb_compat_upper(line);
		if (line[0] == '\0') {
			*answer = blank_yes;
			return true;
		}
		if (line[0] == 'Y' || line[0] == 'N') {
			*answer = line[0] == 'Y';
			return true;
		}
	}
}

static bool
session_value(struct yt_session *session, const char *prompt,
    double *value, bool *blank)
{
	char line[160];
	struct qb_val_result parsed;

	yt_out(prompt);
	if (!session_line(session, line, sizeof(line)))
		return false;
	*blank = line[0] == '\0';
	parsed = qb_val(line);
	*value = parsed.valid ? parsed.value : 0.0;
	return true;
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

static bool
registration(struct yt_session *session, struct yt_error *error)
{
	char path[512];
	FILE *file;
	long size;
	char registered_to[256];
	char registered_by[256];
	char key[256];
	struct qb_val_result parsed;
	double value = 21.0;
	double expected;
	size_t index;

	yt_out_line("Yankee Trader");
	yt_out_line("(c)Alan Davenport");
	yt_out_line(
	    "Prices & Xannor fix, Anticloak, Spies, Missiles disabled  ");
	yt_out_line("Strategy Guide: www.starflt.com/yt.html      ");
	if (!yt_resolve_case_path("YT.REG", true, path, sizeof(path), error))
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
	fclose(file);
	if (size == 0) {
		if (!yt_file_delete(path, false, error))
			return false;
		session->registered = false;
		yt_out_line("UNREGISTERED EVALUATION COPY!");
		yt_out_line(
		    "PLEASE ENCOURAGE YOUR SYSOP TO REGISTER THIS GAME.");
		return true;
	}
	file = fopen(path, "rb");
	if (file == NULL)
		return false;
	if (!read_physical_line(file, registered_to, sizeof(registered_to))
	    || !read_physical_line(file, registered_by, sizeof(registered_by))
	    || !read_physical_line(file, key, sizeof(key))) {
		fclose(file);
		return false;
	}
	fclose(file);
	qb_title_case(registered_to);
	qb_title_case(registered_by);
	for (index = 0; registered_to[index] != '\0'; ++index)
		value += 3.0 * (double)(unsigned char)registered_to[index];
	value = floor(sqrt(111355062389.0 * value));
	for (index = 0; registered_by[index] != '\0'; ++index)
		value += 5.0 * (double)(unsigned char)registered_by[index];
	expected = floor(sqrt(7.0 * 111355062389.0 * value));
	parsed = qb_val(key);
	if (!parsed.valid || parsed.value != expected) {
		yt_out("\a\a\a\a");
		yt_out_line(" * INVALID REGISTRATION KEY! *");
		session->running = false;
		return true;
	}
	session->registered = true;
	yt_outf("Registered to %s\r\n", registered_to);
	yt_outf("Registered by %s\r\n", registered_by);
	return true;
}

static bool
opening_and_date(struct yt_session *session, struct yt_error *error)
{
	if (session->door->identity.ansi) {
		if (!yt_out_file("YTOPEN.ANS", error))
			return false;
	}
	else if (!display_game_file(session, "YTOPEN.ASC", error))
		return false;
	return yt_current_date_serial(session->door->game.config.epoch_year,
	    &session->door->game.today, &session->door->game.adjusted_year,
	    error);
}

static bool
lockout(struct yt_session *session, struct yt_error *error)
{
	char path[512];
	char live[300];
	FILE *file;
	long size;

	snprintf(live, sizeof(live), "%s %s",
	    session->door->identity.real_first,
	    session->door->identity.real_last);
	qb_title_case(live);
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
	for (;;) {
		char line[300];

		if (!read_physical_line(file, line, sizeof(line)))
			break;
		qb_title_case(line);
		if (strcmp(line, live) == 0) {
			fclose(file);
			yt_out("\a");
			yt_out_line("You have been locked out of Yankee Trader.");
			yt_out_line("Please contact the SysOp.");
			session->running = false;
			return true;
		}
	}
	fclose(file);
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
		bool accepted;
		size_t index;
		struct yt_name_row row;

		yt_out("Enter the name you wish to use: ");
		if (!session_line(session, alias, sizeof(alias))) {
			yt_names_free(&names);
			return false;
		}
		if (alias[0] == '\0')
			snprintf(alias, sizeof(alias), "%s %s", first, last);
		for (index = 0; alias[index] != '\0'; ++index) {
			if (alias[index] == ',' || alias[index] == '"')
				alias[index] = ' ';
		}
		qb_title_case(alias);
		if (alias[0] == '\0')
			continue;
		if (strstr(alias, "Sysop") != NULL
		    || strstr(alias, "Xannor") != NULL
		    || strstr(alias, "Mercenaries") != NULL) {
			yt_out_line("That name may not be used.");
			continue;
		}
		alias[40] = '\0';
		yt_names_split(alias, alias_first, sizeof(alias_first), alias_last,
		    sizeof(alias_last));
		qb_title_case(alias_last);
		snprintf(display, sizeof(display), "%s %s", alias_first,
		    alias_last);
		display[40] = '\0';
		if (yt_names_alias_exists(&names, alias_first, alias_last)) {
			yt_out_line("That name is already in use.");
			continue;
		}
		yt_outf("%s %s a.k.a. %s\r\n", first, last, display);
		if (!session_yes_no(session, "Is this correct? [y/N] ", false,
		    &accepted)) {
			yt_names_free(&names);
			return false;
		}
		if (!accepted)
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
		snprintf(first, 128, "%s", alias_first);
		snprintf(last, 128, "%s", alias_last);
		yt_names_free(&names);
		return true;
	}
}

static bool
admit_player(struct yt_session *session, const char *first, const char *last,
    struct yt_error *error)
{
	char full[256];
	char line[512];
	struct yt_clock_value now;
	int basic;
	int vacant = 0;
	bool returning = false;

	snprintf(full, sizeof(full), "%s %s", first, last);
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		struct yt_player candidate;
		bool overflow;
		int stored;

		if (!yt_game_read_player(&session->door->game, basic, &candidate,
		    error))
			return false;
		stored = (int)qb_cint(candidate.name_length, &overflow);
		if (!overflow && stored >= 0 && stored <= 41
		    && strncmp(candidate.name, full, (size_t)stored) == 0
		    && strlen(full) == (size_t)stored) {
			session->player_record = basic;
			session->player = candidate;
			returning = true;
			break;
		}
		if (vacant == 0 && candidate.name_length < 1.0f)
			vacant = basic;
	}
	if (!returning) {
		char instructions[80];

		if (vacant == 0) {
			char date[11];

			if (!yt_platform_clock(&now, error))
				return false;
			yt_format_date(&now, date);
			snprintf(line, sizeof(line),
			    "***%s %s: New player not allowed - game full.",
			    date, full);
			if (!append_news(session, line, error))
				return false;
			yt_out_line(
			    "I'm sorry but the game is full. Try again tomorrow.");
			session->running = false;
			return true;
		}
		{
			char days[64];

			qb_str_double(days, sizeof(days),
			    session->door->game.config.retention_days);
			yt_outf("Notice: If your ship is dead and you have not "
			    "played for%s\r\n", days);
			yt_out_line(
			    "days, it will be deleted to make room for someone else.");
		}
		session->player_record = vacant;
		if (!yt_game_read_player(&session->door->game, vacant,
		    &session->player, error))
			return false;
		yt_player_construct(&session->player,
		    &session->door->game.config,
		    (float)session->door->game.today);
		if (!write_player(session, error))
			return false;
		snprintf(session->player.name, sizeof(session->player.name), "%s",
		    full);
		session->player.name_length = (float)strlen(full);
		session->player.team = 0.0f;
		if (!write_player(session, error))
			return false;
		if (!yt_platform_clock(&now, error))
			return false;
		{
			char date[11];

			yt_format_date(&now, date);
			snprintf(line, sizeof(line),
			    "-=*=- %s %s New Player Entered -=*=-", date, full);
		}
		if (!append_news(session, line, error))
			return false;
		yt_out("Do you want instructions (Y/N) [N]? ");
		if (!session_line(session, instructions, sizeof(instructions)))
			return false;
		qb_compat_upper(instructions);
		if (instructions[0] == 'Y'
		    && !display_game_file(session, "YTINSTR.DOC", error))
			return false;
		return true;
	}
	{
		float previous_day = session->player.last_active;
		float killer = session->player.killed_by;

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
			snprintf(line, sizeof(line),
			    "-=*=- %s %s Logged on -=*=-", time_text,
			    session->player.name);
		}
		if (!append_news(session, line, error))
			return false;
		if (killer != 0.0f) {
			if (killer == (float)session->player_record
			    && previous_day == (float)session->door->game.today) {
				yt_out_line("Your ship was destroyed today.");
				yt_out_line("You may rebuild tomorrow.");
				session->running = false;
				return true;
			}
			if (killer == -1.0f)
				yt_out_line("Your ship was destroyed by the Xannor.");
			else if (killer == -2.0f)
				yt_out_line(
				    "Your ship was destroyed by the Mercenaries.");
			else if (killer == -98.0f)
				yt_out_line(
				    "Your ship was destroyed by a deleted player.");
			else if (killer >= YT_PLAYER_FIRST
			    && killer <= YT_PLAYER_LAST) {
				struct yt_player attacker;

				if (!yt_game_read_player(&session->door->game,
				    (int)killer, &attacker, error))
					return false;
				yt_outf("Your ship was destroyed by %s.\r\n",
				    attacker.name);
			}
			yt_player_construct(&session->player,
			    &session->door->game.config,
			    (float)session->door->game.today);
			if (!write_player(session, error))
				return false;
		}
	}
	return true;
}

static bool
radio_name(struct yt_session *session, float record, char *dest,
    size_t size, bool sender, struct yt_error *error)
{
	if (record > 0.0f) {
		struct yt_player player;

		if (!yt_game_read_player(&session->door->game, (int)record,
		    &player, error))
			return false;
		snprintf(dest, size, "%s", player.name);
	}
	else if (!sender)
		snprintf(dest, size, "All");
	else if (record == -1.0f)
		snprintf(dest, size, "The Xannor");
	else
		snprintf(dest, size, "The Mercenaries");
	return true;
}

static bool
radio_read(struct yt_session *session, bool log_mode,
    struct yt_error *error)
{
	char path[512];
	FILE *file;
	struct yt_radio_record record;
	long offset = 0;

	if (!yt_resolve_case_path("YTRMSG.DAT", true, path, sizeof(path),
	    error))
		return false;
	file = fopen(path, "r+b");
	if (file == NULL) {
		file = fopen(path, "w+b");
		if (file == NULL)
			return false;
	}
	while (fread(record.bytes, 1, sizeof(record.bytes), file)
	    == sizeof(record.bytes)) {
		float counter = yt_radio_get_number(&record, 0);
		float recipient = yt_radio_get_number(&record, 4);
		float sender = yt_radio_get_number(&record, 8);
		bool show = counter > 1.0f
		    || ((counter == 1.0f || log_mode)
		    && (recipient == (float)session->player_record
		    || (log_mode && sender == (float)session->player_record)));

		if (show) {
			char from[80];
			char to[80];
			char text[75];
			size_t length = 74;

			if (!radio_name(session, sender, from, sizeof(from), true,
			    error) || !radio_name(session, recipient, to,
			    sizeof(to), false, error)) {
				fclose(file);
				return false;
			}
			memcpy(text, record.bytes + 12, 74);
			while (length > 0 && text[length - 1U] == ' ')
				--length;
			text[length] = '\0';
			yt_outf("From: %s  To: %s\r\n%s\r\n", from, to, text);
			if (!log_mode) {
				counter = counter > 1.0f ? single_sub(counter, 2.0f)
				    : 0.0f;
				yt_radio_set_number(&record, 0, counter);
				if (fseek(file, offset, SEEK_SET) != 0
				    || fwrite(record.bytes, 1,
				    sizeof(record.bytes), file)
				    != sizeof(record.bytes)
				    || fseek(file, offset
				    + (long)sizeof(record.bytes), SEEK_SET) != 0) {
					fclose(file);
					return false;
				}
			}
		}
		offset += (long)sizeof(record.bytes);
	}
	if (ferror(file) || fclose(file) != 0)
		return false;
	return true;
}

static bool
post_login(struct yt_session *session, struct yt_error *error)
{
	bool changed = false;

	if (session->player.turns < 1.0f) {
		session->player.turns = 1.0f;
		changed = true;
	}
	if (session->player.holds
	    > session->door->game.config.maximum_holds) {
		session->player.ore = 0.0f;
		session->player.organics = 0.0f;
		session->player.equipment =
		    session->door->game.config.maximum_holds;
		session->player.holds =
		    session->door->game.config.maximum_holds;
		changed = true;
	}
	if (changed && !write_player(session, error))
		return false;
	if (!show_ship(session, error))
		return false;
	if (!session_press_any_key(session, false, error))
		return false;
	return radio_read(session, false, error);
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

static bool
planet_update(struct yt_session *session, int logical_planet,
    struct yt_planet *planet, struct yt_error *error)
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
	if (!yt_game_read_planet(&session->door->game, logical_planet, planet,
	    error))
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
	return yt_game_write_planet(&session->door->game, logical_planet,
	    planet, error)
	    && yt_database_flush(&session->door->game.database, error);
}

static bool
same_team(struct yt_session *session, int other_record,
    struct yt_error *error)
{
	struct yt_player other;

	if (other_record == session->player_record)
		return true;
	if (!yt_game_read_player(&session->door->game, other_record, &other,
	    error))
		return false;
	return session->player.team > 0.0f
	    && other.team == session->player.team;
}

static bool
sector_force_friendly(struct yt_session *session,
    const struct yt_sector *sector, struct yt_error *error)
{
	int owner = (int)sector->fighter_owner;

	if (sector->fighters == 0.0f || owner == session->player_record)
		return true;
	if (owner > 0)
		return same_team(session, owner, error);
	return false;
}

static bool
display_sector_one(struct yt_session *session, int logical_sector,
    struct yt_error *error)
{
	struct yt_sector sector;
	char warning[128];
	size_t slot;
	int basic;

	if (!yt_game_read_sector(&session->door->game, logical_sector,
	    &sector, error))
		return false;
	yt_out_line("");
	yt_outf("Sector %.9g\r\n", (double)logical_sector);
	if (((float)logical_sector == session->black_hole[0]
	    || (float)logical_sector == session->black_hole[1])
	    && !session_attention(session,
	    "** Space-time disruption detected! **",
	    "sector disruption attention", error))
		return false;
	if (sector.mines != 0.0f) {
		snprintf(warning, sizeof(warning),
		    "** WARNING! SECTOR HAS %.9g MINES! **",
		    (double)sector.mines);
		if (!session_attention(session, warning,
		    "sector mine attention", error))
			return false;
		for (slot = 0; slot < 3; ++slot) {
			if (!session_sound(session, 4.0f,
			    "sector mine follow-up sound", error))
				return false;
		}
	}
	if (sector.port > 0.0f) {
		struct yt_port port;
		int logical_port = (int)sector.port;
		static const char *commodities[3] = {
			"Ore", "Organics", "Equipment"
		};
		int commodity;

		if (!yt_game_read_port(&session->door->game, logical_port, &port,
		    error))
			return false;
		port.sector = (float)logical_sector;
		if (!yt_game_write_port(&session->door->game, logical_port, &port,
		    error))
			return false;
		commodity = (int)port.commodity_class - 1;
		yt_outf("Port: %s", port.name);
		if (commodity >= 0 && commodity < 3)
			yt_outf(" (sells %s)", commodities[commodity]);
		yt_out("\r\n");
	}
	if (sector.planet > 0.0f) {
		struct yt_planet planet;

		if (!planet_update(session, (int)sector.planet, &planet, error))
			return false;
		yt_outf("Planet: %s\r\n", planet.name);
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		float random_value;

		if (basic == session->player_record
		    || session->sector_cache[basic] != (float)logical_sector)
			continue;
		if (!yt_random_next(&session->door->game.random, &random_value,
		    error))
			return false;
		if (session->cloak_cache[basic] != 0.0f) {
			if (random_value <= session->cloak_cache[basic])
				continue;
			yt_out_line(
			    "You detect the shimmering of a cloaking device!");
			session->cloak_cache[basic] = 0.0f;
			if (!session_sound(session, 4.0f,
			    "sector cloak-reveal sound", error))
				return false;
		}
		{
			struct yt_player other;

			if (!yt_game_read_player(&session->door->game, basic,
			    &other, error))
				return false;
			yt_outf("Ship: %s  Team %.9g  Fighters %.9g"
			    "  Shields %.9g\r\n", other.name,
			    (double)other.team, (double)other.fighters,
			    (double)other.shields);
		}
	}
	if (sector.fighters != 0.0f) {
		if (sector.fighter_owner == -1.0f)
			yt_outf("Deployed Fighters: %.9g (The Xannor)\r\n",
			    (double)sector.fighters);
		else if (sector.fighter_owner == -2.0f)
			yt_outf("Deployed Fighters: %.9g (The Mercenaries)\r\n",
			    (double)sector.fighters);
		else if (sector.fighter_owner
		    == (float)session->player_record)
			yt_outf("Deployed Fighters: %.9g (Yours)\r\n",
			    (double)sector.fighters);
		else {
			struct yt_player owner;

			if (!yt_game_read_player(&session->door->game,
			    (int)sector.fighter_owner, &owner, error))
				return false;
			yt_outf("Deployed Fighters: %.9g (%s)\r\n",
			    (double)sector.fighters, owner.name);
		}
	}
	yt_out("Warps to Sector(s):");
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] != 0.0f)
			yt_outf(" %.9g", (double)sector.warps[slot]);
	}
	yt_out("\r\n");
	return true;
}

static bool
display_sector(struct yt_session *session, bool adjacent,
    struct yt_error *error)
{
	int current = (int)session->player.sector;
	struct yt_sector sector;
	size_t slot;

	if (!adjacent)
		return display_sector_one(session, current, error);
	if (!yt_game_read_sector(&session->door->game, current, &sector,
	    error))
		return false;
	yt_out_line("[ Sensors Activated ]");
	if (!session_sound(session, 4.0f,
	    "adjacent-sector sensor sound", error))
		return false;
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] != 0.0f
		    && !display_sector_one(session, (int)sector.warps[slot],
		    error))
			return false;
	}
	yt_out_line("[ End Sensor Scan ]");
	return display_sector_one(session, current, error);
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
	if (!reload_player(session, error))
		return false;
	session->sector_cache[session->player_record] = session->player.sector;
	if (!session->anti_cloak)
		session->cloak_cache[session->player_record] = session->player.cloak;
	*denied = session->player.turns <= 0.0f;
	if (*denied) {
		yt_out_line("");
		clear_queue(session);
		yt_out_line("Sorry but you have no turns left.");
	}
	return true;
}

static bool
finalize_action(struct yt_session *session, float amount,
    struct yt_error *error)
{
	float quotient;
	float draw;
	char number[64];
	char row[128];

	(void)amount;
	if (!spy_sweep(session, error) || !reload_player(session, error))
		return false;
	session->player.turns = single_sub(session->player.turns, 1.0f);
	quotient = single_div(session->player.turns, 25.0f);
	if (!session->anti_cloak && quotient == floorf(quotient)) {
		float display;

		session->player.cloak = single_add(session->player.cloak,
		    -0.009999999776482582f);
		if (session->player.cloak < 0.0f)
			session->player.cloak = 0.0f;
		session->cloak_cache[session->player_record] =
		    session->player.cloak;
		display = floorf(single_mul(session->player.cloak, 50.0f));
		qb_str_single(number, sizeof(number), display);
		snprintf(row, sizeof(row), "Cloak at%s%%", number);
		yt_out_line(row);
		if (session->player.cloak == 0.0f) {
			if (!session_attention(session,
			    " WARNING! CLOAK EXPIRED!",
			    "action-finalizer cloak attention", error))
				return false;
			yt_out_line("");
		}
		else {
			yt_out_line("");
			yt_out_line("");
		}
	}
	if (!write_player(session, error))
		return false;
	qb_str_single(number, sizeof(number), session->player.turns);
	snprintf(row, sizeof(row), "One Turn Deducted,%s left.", number);
	yt_out_line(row);
	if (!random_value(session, &draw, error))
		return false;
	if (draw > 0.99000000953674316f) {
		if (!launch_xannor_retaliation(session, 0, error))
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
emergency_warp(struct yt_session *session, struct yt_error *error)
{
	float first;
	float second;
	float duration;
	float heat = 0.0f;
	int ticks = 0;
	float destination;
	float override;
	float turn_draw;
	float cost;

	if (!random_value(session, &first, error)
	    || !random_value(session, &second, error))
		return false;
	duration = single_add(single_mul(first, 70.0f),
	    single_mul(second, 70.0f));
	yt_out_line("");
	if (!session_attention(session, " * EMERGENCY WARP ENGAGED! * ",
	    "emergency warp attention", error))
		return false;
	for (;;) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		if (draw > 0.75f)
			heat = single_add(heat, 1.0f);
		yt_out(".");
		yt_platform_delay(330);
		if (heat >= 31.0f)
			break;
		++ticks;
		if ((float)ticks > duration)
			break;
	}
	yt_out("\r\n");
	if (!random_value(session, &first, error)
	    || !random_value(session, &override, error)
	    || !random_value(session, &turn_draw, error))
		return false;
	destination = floorf(single_mul(first,
	    (float)sector_count(session))) + 1.0f;
	if (override > 0.949999988079071f)
		destination = session->door->game.config.headquarters;
	cost = single_add(single_mul(heat, 4.0f),
	    floorf(single_mul(turn_draw, 4.0f)));
	if (cost > session->player.turns)
		cost = session->player.turns;
	if (heat >= 31.0f) {
		cost = session->player.turns;
		if (!session_attention(session, "MELT DOWN!",
		    "meltdown attention", error))
			return false;
		yt_out_line("Your engines are disabled!");
		yt_out_line("");
		yt_out_line("It will take a solar day to repair them.");
		for (int ordinal = 0; ordinal < 5; ++ordinal) {
			if (!session_sound(session, 5.0f,
			    "meltdown sound", error))
				return false;
		}
		yt_outf("You are stranded in sector%.9g.\r\n",
		    (double)destination);
	}
	else {
		if (!session_sound(session, 1.0f,
		    "emergency warp completion sound", error))
			return false;
		yt_out_line("You sigh in relief as you look at your scanner and find"
		    " yourself in");
		yt_outf("sector%.9g. However, it takes you%.9g turns to recharge"
		    " your engines!\r\n", (double)destination, (double)cost);
	}
	session->player.turns = single_sub(session->player.turns, cost);
	session->player.sector = destination;
	session->sector_cache[session->player_record] = destination;
	clear_queue(session);
	return write_player(session, error);
}

static bool
command_move(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	char line[160];
	float target;
	struct qb_val_result parsed;
	size_t slot;
	bool adjacent = false;
	bool danger;
	bool accepted;
	bool denied;

	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	yt_out("Warps:");
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] != 0.0f)
			yt_outf(" %.9g", (double)sector.warps[slot]);
	}
	yt_out("\r\n");
	for (;;) {
		char upper[160];

		yt_out("Move to which sector? ");
		if (!session_line(session, line, sizeof(line)))
			return false;
		snprintf(upper, sizeof(upper), "%s", line);
		qb_compat_upper(upper);
		if (strchr(upper, 'E') != NULL)
			return true;
		if (strcmp(upper, "M") == 0)
			continue;
		parsed = qb_val(line);
		target = (float)(parsed.valid ? parsed.value : 0.0);
		break;
	}
	if (target < 1.0f || target > (float)sector_count(session))
		return true;
	if (target == session->player.sector) {
		yt_out_line("You're already there!");
		return true;
	}
	for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
		if (sector.warps[slot] == target)
			adjacent = true;
	}
	if (!adjacent) {
		yt_out_line("You can't get there from here.");
		return true;
	}
	if (session->player.danger_scanner != 0.0f) {
		if (!dangerous_destination(session, target, &danger, error))
			return false;
		if (danger) {
			char prompt[100];
			char target_text[40];

			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "danger confirmation blank", error))
				return false;
			clear_queue(session);
			qb_str_single(target_text, sizeof(target_text), target);
			snprintf(prompt, sizeof(prompt),
			    "Move into sector%s? [y/N] ", target_text);
			if (!session_yes_no(session, prompt, false, &accepted))
				return false;
			if (!accepted)
				return true;
		}
	}
	if (!finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	session->suppress_self_mines = false;
	session->player.sector = target;
	session->sector_cache[session->player_record] = target;
	return write_player(session, error);
}

static bool
team_remove_player(struct yt_session *session, int victim,
    struct yt_error *error)
{
	struct yt_player player;
	int team_id;
	struct yt_sector overlay;
	static const size_t roster_offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

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
	for (index = 0; index < YT_ARRAY_LEN(roster_offsets); ++index) {
		if (yt_record_get_number(&overlay.record, roster_offsets[index])
		    == (float)victim)
			yt_record_set_number(&overlay.record,
			    roster_offsets[index], 0.0f);
	}
	if (yt_record_get_number(&overlay.record, YT_F77) == (float)victim)
		yt_record_set_number(&overlay.record, YT_F77, 0.0f);
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
	char number[64];
	char news[300];
	int logical;
	int matched_ports = 0;
	float old_ports_owned;
	bool valid_killer = killer >= 2.0f
	    && killer <= session->door->game.config.sector_offset
	    && killer != (float)victim_record;

	if (!yt_game_read_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	old_ports_owned = victim.ports_owned;
	session->sector_cache[victim_record] = 0.0f;
	victim.killed_by = killer;
	victim.sector = 0.0f;
	victim.ports_owned = 0.0f;
	if (!yt_game_write_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	for (logical = 1; logical <= sector_count(session); ++logical) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&session->door->game, logical, &sector,
		    error))
			return false;
		if (sector.fighter_owner == (float)victim_record) {
			sector.fighter_owner = -2.0f;
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

		if (!yt_game_read_port(&session->door->game, logical, &port,
		    error))
			return false;
		if (port.owner != (float)victim_record)
			continue;
		++matched_ports;
		if (valid_killer) {
			port.owner = killer;
			port.last_minute = killer;
		}
		else {
			port.owner = 0.0f;
			port.treasury = 0.0f;
		}
		if (!yt_game_write_port(&session->door->game, logical, &port,
		    error))
			return false;
	}
	if (valid_killer && matched_ports > 0) {
		struct yt_player attacker;
		char row[300];

		qb_str_single(number, sizeof(number), (float)matched_ports);
		snprintf(row, sizeof(row), "The titles to%s ports of %s's are now "
		    "yours!", number, victim.name);
		yt_out_line(row);

		if (!yt_game_read_player(&session->door->game, (int)killer,
		    &attacker, error))
			return false;
		attacker.ports_owned =
		    single_add(attacker.ports_owned, (float)matched_ports);
		if (!yt_game_write_player(&session->door->game, (int)killer,
		    &attacker, error))
			return false;
	}
	if (killer == (float)victim_record) {
		snprintf(news, sizeof(news), "  -  %s was killed!",
		    session->player.name);
	}
	else {
		snprintf(news, sizeof(news), "  -  %s killed %s",
		    session->player.name, victim.name);
	}
	if (!append_news(session, news, error))
		return false;
	if (matched_ports > 0 && killer != (float)victim_record) {
		qb_str_single(number, sizeof(number), (float)matched_ports);
		snprintf(news, sizeof(news), "  -  Took%s ports from %s",
		    number, victim.name);
		if (!append_news(session, news, error))
			return false;
	}
	if (victim_record == session->player_record) {
		session->player = victim;
		session->destroyed = true;
	}
	return yt_database_flush(&session->door->game.database, error);
}

static bool
salvage_player(struct yt_session *session, int victim_record,
    struct yt_error *error)
{
	struct yt_player victim_storage;
	const struct yt_player *victim = &victim_storage;
	float draw[6];
	float awards[6];
	float cargo_awards[4] = {0};
	char number[64];
	char news[300];
	bool emitted = false;
	int index;

	if (!yt_game_read_player(&session->door->game, victim_record,
	    &victim_storage, error))
		return false;
	if (session->player_record < YT_PLAYER_FIRST
	    || session->player_record
	    > (int)session->door->game.config.sector_offset)
		return true;
	yt_out_line("");
	yt_out_line("You destroyed the ship and salvaged the following:");
	snprintf(news, sizeof(news),
	    " *** %s salvaged the following from %s's ship:",
	    session->player.name, victim->name);
	if (!append_news(session, news, error))
		return false;
	yt_out_line("");
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
	session_timed_wait(session, 1.0);
	if (!reload_player(session, error))
		return false;
	{
		static const char *const labels[5] = {
			"Credits:", "Cruise Missiles:", "Plasma Bolts:",
			"Ground Forces:", "Sector Mines:"
		};
		float *const fields[5] = {
			&session->player.credits, &session->player.missiles,
			&session->player.plasma, &session->player.ground_forces,
			&session->player.mines
		};

		for (index = 1; index < 6; ++index) {
			if (awards[index] == 0.0f)
				continue;
			session_timed_wait(session, 0.5);
			emitted = true;
			qb_str_single(number, sizeof(number), awards[index]);
			snprintf(news, sizeof(news), "  -  %s%s",
			    labels[index - 1], number);
			if (!append_news(session, news, error))
				return false;
			yt_out_line(news);
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
		float stock[4] = {
			victim->ore, victim->organics, victim->equipment,
			victim->holds - victim->ore - victim->organics
			    - victim->equipment
		};
		float remaining = victim->holds;
		int count = (int)awards[0];
		int hold;

		emitted = true;
		for (hold = 0; hold < count && remaining > 0.0f; ++hold) {
			float selected;
			float cursor = 0.0f;
			int kind;

			if (!random_value(session, &selected, error))
				return false;
			selected = floorf(single_mul(selected, remaining)) + 1.0f;
			for (kind = 0; kind < 4; ++kind) {
				cursor += stock[kind];
				if (selected <= cursor) {
					cargo_awards[kind] =
					    single_add(cargo_awards[kind], 1.0f);
					stock[kind] -= 1.0f;
					break;
				}
			}
			remaining -= 1.0f;
		}
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
		session_timed_wait(session, 0.5);
		{
			static const int order[4] = {3, 0, 1, 2};
			static const char *const suffix[4] = {
				" empty holds", " holds of ore",
				" holds of organics", " holds of equipment"
			};

			for (index = 0; index < 4; ++index) {
				int kind = order[index];

				if (cargo_awards[kind] <= 0.0f)
					continue;
				session_timed_wait(session, 0.5);
				qb_str_single(number, sizeof(number),
				    cargo_awards[kind]);
				snprintf(news, sizeof(news), "  - %s%s",
				    number, suffix[index]);
				if (!append_news(session, news, error))
					return false;
				yt_out_line(news);
			}
		}
	}
	if (!emitted) {
		session_timed_wait(session, 0.5);
		if (!append_news(session, "  -  NOTHING!", error))
			return false;
		yt_out_line("  -  NOTHING!");
	}
	session_timed_wait(session, 4.0);
	return true;
}

static bool
combat_attrition(struct yt_session *session, double committed,
    float defenders, float cloak, double *attacker_loss,
    float *defender_loss, struct yt_error *error)
{
	double lost_attacker = 0.0;
	float lost_defender = 0.0f;

	while (lost_attacker < committed && lost_defender < defenders) {
		double remaining_attacker = committed - lost_attacker;
		float remaining_defender =
		    single_sub(defenders, lost_defender);
		float minimum = remaining_attacker < (double)remaining_defender
		    ? (float)remaining_attacker : remaining_defender;
		float quantum = floorf(single_div(minimum, 20.0f));
		float draw;

		if (quantum < 1.0f)
			quantum = 1.0f;
		if (!random_value(session, &draw, error))
			return false;
		if (single_add(single_div(cloak, 10.0f), draw)
		    < 0.44999998807907104f)
			lost_attacker += (double)quantum;
		else
			lost_defender = single_add(lost_defender, quantum);
	}
	*attacker_loss = lost_attacker > committed ? committed : lost_attacker;
	*defender_loss = lost_defender > defenders ? defenders : lost_defender;
	return true;
}

static bool
xannor_victory(struct yt_session *session, struct yt_error *error)
{
	char winner[300];
	char banner[80];
	struct yt_sector overlay;

	if (!display_line_file("XannorHQ.TXT", error))
		return false;
	yt_out("[PAUSE]");
	session_timed_wait(session, 99.0);
	yt_out("\r\n");
	yt_out_line("Collect 16,000,000 credit bonus!");
	session->player.credits = floorf(single_add(
	    session->player.credits, 16000000.0f));
	if (!write_player(session, error))
		return false;
	for (int ordinal = 0; ordinal < 3; ++ordinal) {
		if (!session_sound(session, 2.0f,
		    "Xannor victory sound", error))
			return false;
	}
	memset(banner, '*', 79);
	banner[79] = '\0';
	snprintf(winner, sizeof(winner),
	    "Congratulations go to %s who defeated the Xannor HQ!!!",
	    session->player.name);
	if (!append_news(session, banner, error)
	    || !append_news(session, winner, error)
	    || !append_news(session, banner, error)
	    || !radio_append(banner, -2.0f, -2.0f, error)
	    || !radio_append(winner, -2.0f, -2.0f, error)
	    || !radio_append(banner, -2.0f, -2.0f, error))
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
	float defender_loss;
	double attacking;
	float victim_mines;
	char line[300];

	if (!reload_player(session, error)
	    || !yt_game_read_player(&session->door->game, target_record,
	    &target, error))
		return false;
	if (committed > (double)session->player.fighters) {
		yt_out_line("You don't have that many fighters!");
		return true;
	}
	if (!session_sound(session, 2.0f,
	    "player attack opening sound", error))
		return false;
	if (!combat_attrition(session, committed, target.fighters,
	    session->player.cloak, &attacker_loss, &defender_loss, error))
		return false;
	attacking = committed - attacker_loss;
	session->player.fighters =
	    (float)((double)session->player.fighters - attacker_loss);
	target.fighters = single_sub(target.fighters, defender_loss);
	if (target.fighters < 0.0f)
		target.fighters = 0.0f;
	if (defender_loss > 0.0f) {
		snprintf(line, sizeof(line), "%s destroyed %.9g of your fighters!",
		    session->player.name, (double)defender_loss);
		if (!radio_append(line, -2.0f, (float)target_record, error))
			return false;
	}
	if (target.fighters <= 0.0f && attacking > 0.0
	    && target.shields > 0.0f) {
		while (attacking > 0.0 && target.shields > 0.0f) {
			float quantum = attacking > 100.0
			    && target.shields > 100.0f ? 100.0f : 1.0f;
			float draw;

			if (!random_value(session, &draw, error))
				return false;
			if (draw <= 0.5f)
				attacking -= (double)quantum;
			else
				target.shields =
				    single_sub(target.shields, quantum);
		}
	}
	if (!yt_game_write_player(&session->door->game, target_record, &target,
	    error) || !write_player(session, error))
		return false;
	if (target.shields > 0.0f)
		return true;
	victim_mines = target.mines;
	if (!session_sound(session, 3.0f,
	    "player kill sound", error)
	    || !kill_player(session, target_record,
	    (float)session->player_record, error)
	    || !salvage_player(session, target_record, error))
		return false;
	{
		struct yt_sector sector;
		int current = (int)session->player.sector;

		if (!yt_game_read_sector(&session->door->game, current, &sector,
		    error))
			return false;
		sector.mines = single_add(sector.mines, victim_mines);
		if (!yt_game_write_sector(&session->door->game, current, &sector,
		    error))
			return false;
	}
	if (victim_mines > 0.0f) {
		snprintf(line, sizeof(line),
		    "  -  %s had sector mines! They EXPLODED!", target.name);
		if (!append_news(session, line, error))
			return false;
	}
	return mine_encounter(session, error);
}

static bool
command_attack_player(struct yt_session *session, struct yt_error *error)
{
	int basic;
	bool encountered = false;

	if (session->player.fighters < 1.0f) {
		yt_out_line("You have no fighters!");
		return true;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		struct yt_player target;
		bool accepted;
		double committed;
		bool blank;

		if (basic == session->player_record
		    || session->sector_cache[basic] != session->player.sector
		    || session->cloak_cache[basic] > 0.0f)
			continue;
		encountered = true;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (target.team > 0.0f && target.team == session->player.team) {
			yt_outf("%s is on your team.\r\n", target.name);
			continue;
		}
		{
			char prompt[160];

			snprintf(prompt, sizeof(prompt), "Attack %s? [Y/n] ",
			    target.name);
			if (!session_yes_no(session, prompt, true, &accepted))
				return false;
		}
		if (!accepted)
			continue;
		if (!session_value(session,
		    "How many fighters do you wish to commit? ",
		    &committed, &blank))
			return false;
		if (blank || committed < 1.0)
			return true;
		return attack_player(session, basic, committed, error);
	}
	yt_out_line(encountered
	    ? "There are no other ships in this sector."
	    : "There's no one here!");
	return true;
}

static bool
deployed_owner_name(struct yt_session *session, float owner, char *name,
    size_t size, struct yt_error *error)
{
	if (owner == -1.0f) {
		snprintf(name, size, "%s", "The Xannor");
		return true;
	}
	if (owner == -2.0f) {
		snprintf(name, size, "%s", "The Mercenaries");
		return true;
	}
	if (owner > 0.0f) {
		struct yt_player player;

		if (!yt_game_read_player(&session->door->game, (int)owner,
		    &player, error))
			return false;
		snprintf(name, size, "%s", player.name);
		return true;
	}
	name[0] = '\0';
	return true;
}

static bool
attack_deployed_committed(struct yt_session *session,
    struct yt_sector *sector, double commitment, bool allow_surrender,
    struct yt_error *error)
{
	double attacker_loss = 0.0;
	float defender_loss = 0.0f;
	float old_count = sector->fighters;
	float old_owner = sector->fighter_owner;
	float old_ship = session->player.fighters;
	bool surrender_checked = false;
	bool surrendered = false;
	char owner_name[80];
	char number_one[64];
	char number_two[64];
	char news[320];

	if (commitment > (double)session->player.fighters) {
		yt_out_line("You don't have that many fighters!");
		return true;
	}
	if (!deployed_owner_name(session, old_owner, owner_name,
	    sizeof(owner_name), error))
		return false;
	if (!session_sound(session, 2.0f,
	    "deployed attack opening sound", error))
		return false;
	while (attacker_loss < commitment && defender_loss < old_count) {
		double remaining_attacker = commitment - attacker_loss;
		float remaining_defender =
		    single_sub(old_count, defender_loss);
		float minimum = remaining_attacker < (double)remaining_defender
		    ? (float)remaining_attacker : remaining_defender;
		float quantum = floorf(single_div(minimum, 20.0f));
		float sample;

		if (quantum < 1.0f)
			quantum = 1.0f;
		if (!random_value(session, &sample, error))
			return false;
		if (single_add(single_div(session->player.cloak, 10.0f),
		    sample) < 0.44999998807907104f)
			attacker_loss += (double)quantum;
		else
			defender_loss = single_add(defender_loss, quantum);
		remaining_attacker = commitment - attacker_loss;
		remaining_defender = single_sub(old_count, defender_loss);
		if (!surrender_checked && allow_surrender
		    && remaining_defender > 0.0f
		    && remaining_attacker / (double)remaining_defender > 10.0) {
			surrender_checked = true;
			if (old_owner > 0.0f) {
				bool accepted;

				if (!session_sound(session, 4.0f,
				    "surrender radio sound", error))
					return false;
				if (!session_yes_no(session,
				    "The defending fighters offer to surrender."
				    " Accept? [Y/n] ", true, &accepted))
					return false;
				surrendered = accepted;
			}
			else {
				yt_out_line(old_owner == -1.0f
				    ? "The Xannor refuse to surrender!"
				    : "The Mercenaries refuse to surrender!");
				if (!session_sound(session, 5.0f,
				    "surrender refusal sound", error))
					return false;
			}
			if (surrendered)
				break;
		}
	}
	if (attacker_loss > commitment)
		attacker_loss = commitment;
	if (defender_loss > old_count)
		defender_loss = old_count;
	if (surrendered) {
		float survivors = single_sub(old_count, defender_loss);

		if (!session_sound(session, 1.0f,
		    "surrender acceptance sound", error))
			return false;
		session->player.fighters = single_add(
		    (float)((double)old_ship - attacker_loss), survivors);
		sector->fighters = 0.0f;
		sector->fighter_owner = 0.0f;
		qb_str_double(number_one, sizeof(number_one), survivors);
		qb_str_double(number_two, sizeof(number_two),
		    session->player.sector);
		snprintf(news, sizeof(news),
		    "%s fighters in sector%s surrendered to %s",
		    number_one, number_two, session->player.name);
		if (!append_news(session, news, error))
			return false;
	}
	else {
		session->player.fighters =
		    (float)((double)old_ship - attacker_loss);
		sector->fighters = single_sub(old_count, defender_loss);
		if (sector->fighters < 1.0f) {
			sector->fighters = 0.0f;
			sector->fighter_owner = 0.0f;
		}
	}
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, sector, error)
	    || !write_player(session, error))
		return false;
	yt_outf("You lost %.9g fighters; they lost %.9g.\r\n",
	    attacker_loss, (double)defender_loss);
	if (defender_loss > 0.0f && !surrendered) {
		qb_str_double(number_one, sizeof(number_one), defender_loss);
		snprintf(news, sizeof(news),
		    "%s destroyed%s fighters belonging to %s",
		    session->player.name, number_one, owner_name);
		if (!append_news(session, news, error))
			return false;
	}
	if (old_owner == -2.0f && defender_loss > 0.0f)
		session->mercenaries_hurt = true;
	if (old_owner == -1.0f && defender_loss > 0.0f) {
		float bonus = floorf(single_div(defender_loss, 256000.0f));

		if (session->player.turns + bonus
		    > session->door->game.config.turns_per_day)
			bonus = session->door->game.config.turns_per_day
			    - session->player.turns;
		if (bonus >= 1.0f) {
			session->player.turns =
			    single_add(session->player.turns, bonus);
			if (!write_player(session, error))
				return false;
			qb_str_double(number_one, sizeof(number_one), bonus);
			qb_str_double(number_two, sizeof(number_two),
			    defender_loss);
			snprintf(news, sizeof(news),
			    "%s collected%s turns bonus for destroying%s Xannor!!",
			    session->player.name, number_one, number_two);
			if (!append_news(session, news, error))
				return false;
			if (sector->fighters < 1.0f
			    && !clearance(session, true, error))
				return false;
		}
	}
	if (session->player.fighters < 1.0f
	    && session->player.shields < 1.0f) {
		if (!session_sound(session, 3.0f,
		    "fatal destruction sound", error))
			return false;
		return kill_player(session, session->player_record,
		    old_owner, error);
	}
	{
		float dominated_draw;

		if (!random_value(session, &dominated_draw, error))
			return false;
	}
	if (sector->fighters == 0.0f && old_owner == -1.0f
	    && session->player.sector
	    == session->door->game.config.headquarters
	    && !xannor_victory(session, error))
		return false;
	return true;
}

static bool
attack_deployed(struct yt_session *session, struct yt_sector *sector,
    struct yt_error *error)
{
	double commitment;
	bool blank;

	if (!session_value(session, "How many fighters do you wish to commit? ",
	    &commitment, &blank))
		return false;
	if (blank || commitment < 1.0)
		return true;
	return attack_deployed_committed(session, sector, commitment, true,
	    error);
}

static bool
bribe_deployed(struct yt_session *session, struct yt_sector *sector,
    struct yt_error *error)
{
	float owner = sector->fighter_owner;
	float first;
	float second;
	bool force_attack;

	if (owner != -2.0f) {
		if (!random_value(session, &first, error))
			return false;
		force_attack = owner == -1.0f
		    || (sector->fighters > session->player.fighters
		    && first < 0.33000001311302185f);
		if (!force_attack) {
			yt_out_line("They refuse your offer.");
			return true;
		}
		yt_out_line("They refuse your offer and attack!");
		return attack_deployed_committed(session, sector,
		    session->player.fighters, false, error);
	}
	if (sector->planet != 0.0f) {
		yt_out_line("The Mercenaries refuse your offer.");
		return true;
	}
	if (!random_value(session, &first, error)
	    || !random_value(session, &second, error))
		return false;
	force_attack = first < 0.05000000074505806f
	    || (session->player.fighters < sector->fighters
	    && second > 0.8999999761581421f)
	    || session->mercenaries_hurt;
	if (!force_attack) {
		double offer;
		bool blank;
		float threshold;

		if (!session_value(session, "How many credits do you offer? ",
		    &offer, &blank))
			return false;
		if (!random_value(session, &first, error))
			return false;
		threshold = single_mul(sector->fighters,
		    single_add(1.0f, single_mul(2.0f, first)));
		if (!blank && offer <= (double)session->player.credits
		    && offer >= (double)threshold) {
			yt_out_line("Good Deal! We join up with you!");
			if (!session_sound(session, 1.0f,
			    "accepted bribe sound", error))
				return false;
			session->player.fighters = single_add(
			    session->player.fighters, sector->fighters);
			session->player.credits = single_sub(
			    session->player.credits, (float)offer);
			sector->fighters = 0.0f;
			sector->fighter_owner = 0.0f;
			if (!yt_game_write_sector(&session->door->game,
			    (int)session->player.sector, sector, error)
			    || !write_player(session, error))
				return false;
			return true;
		}
	}
	yt_out_line("The Mercenaries reject your offer and attack!");
	return attack_deployed_committed(session, sector,
	    session->player.fighters, false, error);
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
    struct yt_error *error)
{
	float loss;

	if (*stock <= 0.0f)
		return true;
	if (!shrink_three(session, single_mul(batch, *stock), &loss, error))
		return false;
	if (loss > *stock)
		loss = *stock;
	*stock = single_sub(*stock, loss);
	return true;
}

static bool
mine_encounter(struct yt_session *session, struct yt_error *error)
{
	int current = (int)session->player.sector;
	struct yt_sector sector;
	char number[64];
	char news[300];

	if (!yt_game_read_sector(&session->door->game, current, &sector,
	    error))
		return false;
	if (sector.mines <= 0.0f)
		return true;
	yt_out_line("");
	yt_out_line("** Sector is Mined!! **");
	if (!session_sound(session, 5.0f,
	    "sector mine warning sound", error))
		return false;
	qb_str_double(number, sizeof(number), session->player.sector);
	snprintf(news, sizeof(news), "%s hit sector mines in sector%s!",
	    session->player.name, number);
	if (!append_news(session, news, error))
		return false;
	while (sector.mines > 0.0f) {
		float before = sector.mines;
		float batch = before > 19.0f
		    ? floorf(single_div(before, 10.0f)) : 1.0f;
		float draw;
		float loss;
		float empty;

		sector.mines = single_sub(sector.mines, batch);
		if (!yt_game_write_sector(&session->door->game, current, &sector,
		    error))
			return false;
		yt_outf("%.9g mines detected; struck %.9g.\r\n",
		    (double)before, (double)batch);
		if (session->player.shields > 0.0f) {
			if (!random_value(session, &draw, error))
				return false;
			loss = single_mul(floorf(single_mul(draw, 1001.0f)),
			    batch);
			if (loss > session->player.shields)
				loss = session->player.shields;
			session->player.shields =
			    single_sub(session->player.shields, loss);
			if (session->player.shields == 0.0f)
				yt_out_line("Shields disintegrated!");
		}
		if (session->player.danger_scanner != 0.0f) {
			if (!random_value(session, &draw, error))
				return false;
			if (draw > 0.949999988079071f)
				session->player.danger_scanner = 0.0f;
		}
		if (session->player.fighters > 0.0f) {
			if (!shrink_three(session, 40000.0f * batch, &loss,
			    error))
				return false;
			if (loss > session->player.fighters)
				loss = session->player.fighters;
			session->player.fighters =
			    single_sub(session->player.fighters, loss);
		}
		if (session->player.cloak > 0.0f) {
			if (!random_value(session, &draw, error))
				return false;
			loss = floorf(single_mul(single_mul(draw, batch),
			    100.0f)) / 100.0f;
			if (loss > session->player.cloak)
				loss = session->player.cloak;
			session->player.cloak =
			    single_sub(session->player.cloak, loss);
		}
		if (session->player.missiles > 0.0f) {
			if (!random_value(session, &draw, error))
				return false;
			loss = floorf(single_mul(draw,
			    single_mul(batch, session->player.missiles))) + 1.0f;
			if (loss > session->player.missiles)
				loss = session->player.missiles;
			session->player.missiles =
			    single_sub(session->player.missiles, loss);
		}
		if (!mine_stock_loss(session, batch, &session->player.mines,
		    error)
		    || !mine_stock_loss(session, batch, &session->player.ore,
		    error)
		    || !mine_stock_loss(session, batch,
		    &session->player.organics, error)
		    || !mine_stock_loss(session, batch,
		    &session->player.equipment, error))
			return false;
		empty = session->player.holds - session->player.ore
		    - session->player.organics - session->player.equipment;
		if (empty > 0.0f) {
			if (!shrink_three(session, empty, &loss, error))
				return false;
			loss = single_mul(batch, loss);
			if (loss > empty)
				loss = empty;
			session->player.holds =
			    single_sub(session->player.holds, loss);
		}
		if (session->player.holds < 1.0f) {
			session->player.holds = 0.0f;
			session->destroyed = true;
		}
		if (!write_player(session, error))
			return false;
		if (!session_sound(session, 2.0f,
		    "sector mine damage sound", error)
		    || !random_value(session, &draw, error))
			return false;
		if (draw > 0.800000011920929f
		    && session->player.holds < 10.0f) {
			if (!emergency_warp(session, error))
				return false;
			break;
		}
		if (session->destroyed)
			break;
	}
	qb_str_double(number, sizeof(number), session->player.shields);
	snprintf(news, sizeof(news), "Shields reduced to%s units!", number);
	if (!append_news(session, news, error))
		return false;
	if (session->destroyed) {
		if (!session_sound(session, 3.0f,
		    "fatal destruction sound", error))
			return false;
		return kill_player(session, session->player_record,
		    (float)session->player_record, error);
	}
	return true;
}

static bool
sector_entry(struct yt_session *session, struct yt_error *error)
{
	session->mercenaries_hurt = false;
	for (;;) {
		struct yt_sector sector;
		bool friendly;

		if (!display_sector(session, false, error)
		    || !reload_player(session, error))
			return false;
		if (session->player.sector == session->black_hole[0]
		    || session->player.sector == session->black_hole[1]) {
			yt_out_line("");
			if (!session_attention(session,
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
		if (sector.mines > 0.0f && !session->suppress_self_mines) {
			if (!mine_encounter(session, error))
				return false;
			if (session->destroyed)
				return true;
			continue;
		}
		friendly = sector_force_friendly(session, &sector, error);
		if (!friendly && error != NULL && error->status != YT_OK)
			return false;
		if (sector.fighters <= 0.0f || friendly)
			return true;
		for (;;) {
			char command[80];
			char upper[80];
			const char *position;
			const char *hostile_commands = "AQBDWT";

			yt_out("Hostile forces: [A]ttack [Q]uit [B]ribe"
			    " [D]rop mine [W]arp [T]eam [?] ");
			if (!session_line(session, command, sizeof(command)))
				return false;
			snprintf(upper, sizeof(upper), "%s", command);
			qb_compat_upper(upper);
			if (upper[0] == '\0')
				strcpy(upper, "?");
			if (strcmp(upper, "S") == 0) {
				if (!display_sector(session, true, error))
					return false;
				continue;
			}
			if (strcmp(upper, "I") == 0) {
				if (!show_ship(session, error))
					return false;
				continue;
			}
			if (strcmp(upper, "?") == 0) {
				yt_out_line("A Q B D W T S I ?");
				continue;
			}
			position = strstr(hostile_commands, upper);
			if (position == NULL) {
				yt_out_line("Invalid command.");
				continue;
			}
			switch ((int)(position - hostile_commands)) {
			case 0:
				if (!attack_deployed(session, &sector, error))
					return false;
				if (sector.fighters <= 0.0f)
					return true;
				break;
			case 1:
				session->running = false;
				return true;
			case 2:
				if (!bribe_deployed(session, &sector, error))
					return false;
				break;
			case 3:
				session->suppress_self_mines = true;
				return true;
			case 4:
				if (!emergency_warp(session, error))
					return false;
				break;
			case 5:
				return true;
			default:
				break;
			}
			break;
		}
	}
}

static bool
command_fighters(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	double value;
	bool blank;
	float desired;
	float delta;
	float remaining;

	if (session->player.sector < 8.0f) {
		yt_out_line("Fighters may not be deployed in Union sectors.");
		return true;
	}
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.fighters > 0.0f
	    && sector.fighter_owner != (float)session->player_record) {
		yt_out_line("Those fighters are not yours.");
		return true;
	}
	yt_outf("You have %.15g fighters available.\r\n",
	    (double)sector.fighters + (double)session->player.fighters);
	if (!session_value(session,
	    "How many fighters do you want in this sector? ", &value, &blank))
		return false;
	if (blank)
		return true;
	desired = (float)floor(value);
	if (desired < 0.0f)
		return true;
	delta = single_sub(sector.fighters, desired);
	remaining = (float)((double)session->player.fighters + (double)delta);
	if (remaining < 0.0f) {
		yt_out_line("You don't have that many fighters.");
		return true;
	}
	sector.fighters = desired;
	sector.fighter_owner = (float)session->player_record;
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	session->player.fighters = remaining;
	if (!write_player(session, error))
		return false;
	return session_sound(session, 4.0f,
	    "sector fighter sound", error);
}

static bool
command_mines(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	double value;
	bool blank;
	float amount;

	if (session->player.mines < 0.0f) {
		session->player.mines = 0.0f;
		if (!write_player(session, error))
			return false;
	}
	if (session->player.mines < 1.0f) {
		yt_out_line("You have no sector mines.");
		return true;
	}
	if (session->player.sector < 8.0f) {
		yt_out_line("Union sectors may not be mined.");
		return true;
	}
	if (!session_value(session, "How many mines do you wish to deploy? ",
	    &value, &blank))
		return false;
	amount = (float)value;
	if (blank || amount < 1.0f || amount > session->player.mines)
		return true;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	session->player.mines =
	    single_sub(session->player.mines, amount);
	if (!write_player(session, error))
		return false;
	sector.mines = single_add(sector.mines, amount);
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
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
port_owner_row(struct yt_session *session, const struct yt_port *port,
    struct yt_error *error)
{
	static const uint8_t prefix[] = "This port is owned by: ";
	uint8_t row[256];
	size_t length = 0;

	if (port->owner <= 1.0f)
		return true;
	memcpy(row + length, prefix, sizeof(prefix) - 1U);
	length += sizeof(prefix) - 1U;
	if (port->owner == (float)session->player_record) {
		char treasury[80];
		static const uint8_t self[] = "YOU, Credits:";

		memcpy(row + length, self, sizeof(self) - 1U);
		length += sizeof(self) - 1U;
		qb_str_double(treasury, sizeof(treasury),
		    (double)port->treasury);
		memcpy(row + length, treasury, strlen(treasury));
		length += strlen(treasury);
	}
	else {
		struct yt_player owner;
		size_t name_length;
		int owner_record;

		if (!isfinite(port->owner) || port->owner < (float)INT_MIN
		    || port->owner > (float)INT_MAX)
			return port_report_failure(error,
			    "port owner record conversion");
		owner_record = (int)port->owner;
		if (!yt_game_read_player(&session->door->game, owner_record,
		    &owner, error))
			return false;
		if (!port_report_length(session, owner.name_length,
		    YT_TEXT_FIELD_SIZE, &name_length,
		    "port owner name length", error))
			return false;
		memcpy(row + length, owner.record.bytes, name_length);
		length += name_length;
	}
	return session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "port owner leading blank", error)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "port owner row", error);
}

static bool
port_report(struct yt_session *session, int logical_port,
    const struct yt_port *port, const float prices[3],
    const double quantities[3], struct yt_error *error)
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
	return true;
}

static bool
trade_commodity(struct yt_session *session, struct yt_port *port,
    int logical_port, size_t commodity, float price,
    struct yt_error *error)
{
	static const char *names[3] = {"Ore", "Organics", "Equipment"};
	float *hold[3] = {
		&session->player.ore,
		&session->player.organics,
		&session->player.equipment
	};
	bool sells = floorf(port->factor[commodity]) > 0.0f;
	float free_holds = session->player.holds - session->player.ore
	    - session->player.organics - session->player.equipment;
	float maximum;
	char line[80];
	float quantity;
	float total;
	bool agreed;

	if (sells) {
		maximum = free_holds;
		if (floorf(port->stock[commodity]) < maximum)
			maximum = floorf(port->stock[commodity]);
		if (floorf(single_mul(price, maximum))
		    > session->player.credits) {
			float affordable =
			    (float)floor((double)session->player.credits
			    / (double)price);
			if (affordable < maximum)
				maximum = affordable;
		}
	}
	else {
		maximum = floorf(port->stock[commodity]);
		if (floorf(*hold[commodity]) < maximum)
			maximum = floorf(*hold[commodity]);
	}
	if (maximum == 0.0f)
		return true;
	for (;;) {
		yt_outf("Credits: %.9g  Free Holds: %.9g\r\n",
		    (double)session->player.credits, (double)free_holds);
		yt_outf("How many holds of %s do you want to %s [%.9g ]? ",
		    names[commodity], sells ? "buy" : "sell",
		    (double)maximum);
		if (!session_line(session, line, sizeof(line)))
			return false;
		qb_compat_upper(line);
		if (strlen(line) > 4U)
			continue;
		quantity = line[0] == '\0' ? maximum
		    : (float)floor(qb_val(line).value);
		if (quantity < 1.0f)
			return true;
		if ((double)quantity > (double)port->stock[commodity]) {
			yt_out_line(sells ? "We don't have that much!"
			    : "We don't need that much!");
			return true;
		}
		if (sells && quantity > free_holds) {
			yt_out_line("You don't have enough cargo holds.");
			continue;
		}
		if (quantity > maximum) {
			yt_out_line(sells ? "You can't afford that much!"
			    : "You don't have that much!");
			return true;
		}
		break;
	}
	total = floorf(single_add(single_mul(price, quantity), 0.5f));
	yt_outf("Agreed, %.9g units.  The port will %s them for %.9g.\r\n",
	    (double)quantity, sells ? "sell" : "buy", (double)total);
	if (!session_yes_no(session, "Do you agree? [Y/n] ", true, &agreed))
		return false;
	if (!agreed) {
		yt_out_line("Never mind!");
		return true;
	}
	{
		float direction = port->factor[commodity] > 0.0f ? 1.0f
		    : port->factor[commodity] < 0.0f ? -1.0f : 0.0f;
		float credit_delta = -single_mul(total, direction);

		session->player.credits = floorf(single_add(
		    session->player.credits, credit_delta));
		*hold[commodity] = single_add(*hold[commodity],
		    single_mul(quantity, direction));
		port->stock[commodity] =
		    single_sub(port->stock[commodity], quantity);
		if (sells && port->owner != 0.0f) {
			float receipt = total;

			if (port->owner == (float)session->player_record)
				receipt = floorf(single_mul(
				    0.009999999776482582f, total));
			port->treasury =
			    single_add(port->treasury, receipt);
		}
	}
	if (!write_player(session, error))
		return false;
	return yt_game_write_port(&session->door->game, logical_port, port,
	    error);
}

static bool
command_trade(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_port port;
	float prices[3];
	double quantities[3];
	int logical_port;
	size_t commodity;
	bool denied;

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.port <= 0.0f) {
		yt_out_line("No port here!");
		return true;
	}
	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	if (!finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	logical_port = (int)sector.port;
	if (!port_update(session, logical_port, &port, prices, quantities,
	    error))
		return false;
	if (!port_report(session, logical_port, &port, prices, quantities,
	    error))
		return false;
	for (commodity = 0; commodity < 3; ++commodity) {
		if (!trade_commodity(session, &port, logical_port, commodity,
		    prices[commodity], error))
			return false;
	}
	return true;
}

static bool
earth_receipt(struct yt_session *session, float cost,
    struct yt_error *error)
{
	struct yt_port earth;

	session->player.credits =
	    floorf(single_sub(session->player.credits, cost));
	if (!write_player(session, error))
		return false;
	if (!yt_game_read_port(&session->door->game, 1, &earth, error))
		return false;
	if (earth.owner != 0.0f) {
		float receipt = earth.owner == (float)session->player_record
		    ? floorf(single_mul(0.009999999776482582f, cost)) : cost;

		earth.treasury = single_add(earth.treasury, receipt);
		if (!yt_game_write_port(&session->door->game, 1, &earth, error))
			return false;
	}
	return true;
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
	bool reported = false;
	int basic;

	yt_out_bytes(activation, sizeof(activation) - 1U);
	yt_out("\r\n");
	yt_out_line("");
	session_set_color(session, 2);
	yt_out_bytes(waves, sizeof(waves) - 1U);
	yt_out("\r\n");
	yt_out_line("");
	if (!session_sound(session, 2.0f,
	    "anti-cloak activation sound", error))
		return false;
	session_set_color(session, 6);
	for (basic = YT_PLAYER_FIRST; basic <= YT_PLAYER_LAST; ++basic) {
		struct yt_player target;
		bool overflow;
		int name_length;
		char row[96];

		if (session->cloak_cache[basic] <= 0.0f)
			continue;
		session->cloak_cache[basic] = 0.0f;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (target.killed_by != 0.0f)
			continue;
		name_length = (int)qb_cint(target.name_length, &overflow);
		if (overflow || name_length < 0) {
			if (error != NULL) {
				error->status = YT_RANGE;
				snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "anti-cloak player name length");
			}
			return false;
		}
		if ((size_t)name_length > strlen(target.name))
			name_length = (int)strlen(target.name);
		snprintf(row, sizeof(row), "%.*s is uncloaked!", name_length,
		    target.name);
		yt_out_line(row);
		if (!session_sound(session, 1.0f,
		    "anti-cloak target sound", error))
			return false;
		reported = true;
	}
	session_set_color(session, 2);
	if (!reported) {
		yt_out_line("");
		yt_out_line("Too bad noone was cloaked anyhow!");
	}
	yt_out_line("");
	yt_out_line("...the effect fades.");
	if (!session_sound(session, 5.0f,
	    "anti-cloak fade sound", error))
		return false;
	if (!reload_player(session, error))
		return false;
	session->player.credits =
	    floorf(single_sub(session->player.credits, price));
	if (!write_player(session, error))
		return false;
	session_set_color(session, 3);
	return true;
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
	static const float trigger[4] = {
		0.7900000214576721f, 0.7900000214576721f,
		0.8399999737739563f, 0.8899999856948853f
	};
	static const float maximum[4] = {
		0.9509999752044678f, 0.9800000190734863f,
		0.800000011920929f, 0.8999999761581421f
	};
	static const char *name[4] = {
		"Cargo Holds", "Fighters", "Shields", "Ground Forces"
	};
	bool announced = false;
	size_t index;

	yt_out_line("");
	for (index = 0; index < 4; ++index) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		if (draw > trigger[index] && *discount[index] == 0.0f
		    && create) {
			if (!random_value(session, &draw, error))
				return false;
			*discount[index] = draw;
		}
		if (*discount[index] < 0.10000000149011612f
		    || *discount[index] > maximum[index])
			*discount[index] = 0.0f;
		else {
			yt_outf("Special clearance sale! The Trader's Guild is"
			    " selling %s\r\nfor %.9g%% off!\r\n", name[index],
			    (double)floorf(single_mul(100.0f,
			    *discount[index])));
			announced = true;
		}
	}
	if (announced) {
		if (!session_sound(session, 1.0f,
		    "clearance sale sound", error))
			return false;
		yt_out_line("");
	}
	return true;
}

static float
earth_price(float base, float discount, bool subtract_shape)
{
	if (subtract_shape)
		return floorf(single_sub(base, single_mul(base, discount)));
	return floorf(single_mul(base, single_sub(1.0f, discount)));
}

static bool
lottery(struct yt_session *session, struct yt_error *error)
{
	char ticket[80];
	int winning[6];
	bool used_winning[6] = {0};
	bool used_ticket[6] = {0};
	int matches = 0;
	int index;
	float award = 0.0f;

	if (session->player.credits < 5.0f) {
		yt_out_line("You need five credits to play.");
		return true;
	}
	if (session->door->game.config.lottery_plays == 0.0f) {
		yt_out_line("The lottery is closed.");
		return true;
	}
	if (session->player.lottery_plays + 1.0f
	    > session->door->game.config.lottery_plays) {
		yt_out_line("You have played the lottery enough today.");
		return true;
	}
	session->player.lottery_plays =
	    single_add(session->player.lottery_plays, 1.0f);
	if (!write_player(session, error)
	    || !clearance(session, true, error))
		return false;
	for (;;) {
		bool valid = true;

		yt_out("Enter six digits: ");
		if (!session_line(session, ticket, sizeof(ticket)))
			return false;
		if (strlen(ticket) != 6U) {
			yt_out_line("That's not 6 digits!");
			continue;
		}
		for (index = 0; index < 6; ++index) {
			if (ticket[index] < '0' || ticket[index] > '9')
				valid = false;
		}
		if (!valid) {
			yt_out_line("Please enter NUMBERS only!");
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
	for (index = 0; index < 6; ++index) {
		float time = 0.01f;

		while (time <= 0.10000000149011612f) {
			float draw;

			if (!random_value(session, &draw, error))
				return false;
			time = single_add(time, 0.005f);
		}
		yt_outf("%d", winning[index]);
	}
	yt_out("\r\n");
	for (index = 0; index < 6; ++index) {
		int candidate;

		for (candidate = 0; candidate < 6; ++candidate) {
			if (!used_winning[index] && !used_ticket[candidate]
			    && winning[index] == ticket[candidate] - '0') {
				used_winning[index] = true;
				used_ticket[candidate] = true;
				++matches;
				break;
			}
		}
	}
	if (matches > 0) {
		award = single_mul(powf(10.0f, (float)matches), 10.0f);
		if (matches == 6)
			award = single_mul(10.0f, award);
		session->player.credits =
		    floorf(single_add(session->player.credits, award));
		if (!write_player(session, error))
			return false;
		for (index = 0; index < matches; ++index) {
			if (!session_sound(session, 1.0f,
			    "lottery award sound", error))
				return false;
		}
	}
	if (matches > 3) {
		char news[300];
		char amount[80];

		format_number(amount, sizeof(amount), award);
		snprintf(news, sizeof(news), "%s won%s credits in the lottery!",
		    session->player.name, amount);
		if (!append_news(session, news, error))
			return false;
	}
	yt_outf("You matched %d and won %.9g credits.\r\n", matches,
	    (double)award);
	return earth_receipt(session, 5.0f, error);
}

static bool
earth_store(struct yt_session *session, struct yt_error *error)
{
	for (;;) {
		char line[80];
		int choice;
		float holds_price = earth_price(250.0f,
		    session->clearance_holds, true);
		float fighters_price = earth_price(50.0f,
		    session->clearance_fighters, true);
		float ground_price = earth_price(200.5f,
		    session->clearance_ground, false);
		float shields_price = earth_price(50.0f,
		    session->clearance_shields, false);

		if (!clearance(session, false, error))
			return false;
		yt_out_line("Earth Special Store");
		yt_outf("1) Cloak Energy 1000    2) Cargo Holds %.9g\r\n",
		    (double)holds_price);
		yt_outf("3) Fighters %.9g        4) Pick-6 Ticket 5\r\n",
		    (double)fighters_price);
		yt_out_line("5) Danger Scanner 500000");
		yt_out_line("6) Anti-Cloak Device 1000000000");
		yt_outf("7) Ground Forces %.9g   8) Shield Power %.9g\r\n",
		    (double)ground_price, (double)shields_price);
		yt_out_line("9) Spies 1000000000    X) Exit");
		yt_out("Selection: ");
		if (!session_line(session, line, sizeof(line)))
			return false;
		qb_compat_upper(line);
		if (line[0] == 'X' || line[0] == '\0')
			return true;
		choice = line[0] - '0';
		if (choice == 4) {
			if (!lottery(session, error))
				return false;
			continue;
		}
		if (choice == 5) {
			if (session->player.danger_scanner != 0.0f) {
				yt_out_line("You already have a danger scanner.");
				continue;
			}
			if (session->player.credits < 500000.0f) {
				yt_out_line("You can't afford that.");
				continue;
			}
			session->player.danger_scanner = -1.0f;
			if (!write_player(session, error)
			    || !earth_receipt(session, 500000.0f, error))
				return false;
			continue;
		}
		if (choice == 6) {
			bool buy;
			char response[80];

			yt_out_line("");
			if (session->player.credits < 1000000000.0f) {
				yt_out_line("You can't afford that.");
				continue;
			}
			if (!session_yes_no(session,
			    "Anti-Cloaking Device works for this logon only. "
			    "Buy one? [y/N]", false, &buy))
				return false;
			if (!buy)
				continue;
			yt_out_line("");
			if (!earth_anti_cloak(session, 1000000000.0f, error))
				return false;
			session->anti_cloak = true;
			yt_out_line("");
			yt_out("Hit [Enter]");
			if (!session_line(session, response, sizeof(response)))
				return false;
			continue;
		}
		if (choice == 9) {
			double requested;
			bool blank;
			int quantity;
			int index;
			float cost;

			if (session->spy_count >= 3) {
				yt_out_line("You may employ only three spies.");
				continue;
			}
			if (session->spy_count != 0)
				yt_outf("%.9g spies active already. more\r\n",
				    (double)session->spy_count);
			if (!session_value(session, "Hire how many spies? [0]? ",
			    &requested, &blank))
				return false;
			quantity = blank ? 0 : (int)floor(requested);
			if (quantity < 1)
				continue;
			cost = single_mul((float)quantity, 1000000000.0f);
			if (cost > session->player.credits) {
				yt_out_line("You do not have enough credits!");
				continue;
			}
			if (session->spy_count + quantity > 3) {
				yt_out_line("Max spies allowed is 3!");
				continue;
			}
			for (index = 0; index < quantity; ++index) {
				double sector;

				do {
					yt_outf("Start spy # %d in what sector?",
					    session->spy_count + index + 1);
					if (!session_value(session, " ", &sector,
					    &blank))
						return false;
					sector = floor(sector);
				} while (blank || sector == 0.0
				    || sector > (double)sector_count(session));
				session->spies[session->spy_count + index] =
				    (int)sector;
			}
			session->spy_count += quantity;
			(void)computer_spies(session);
			if (!session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "spy purchase pause blank", error)
			    || !session_press_any_key(session, false, error))
				return false;
			if (!earth_receipt(session, cost, error))
				return false;
			continue;
		}
		if (choice >= 1 && choice <= 8) {
			double requested;
			bool blank;
			float price;
			float current;
			float maximum = INFINITY;
			float quantity;
			float cost;

			if (choice == 1) {
				float points = floorf(single_mul(
				    session->player.cloak, 50.0f));
				float deficit = single_sub(50.0f, points);
				float default_quantity = deficit;

				if (single_mul(deficit, 1000.0f)
				    > session->player.credits)
					default_quantity = floorf(
					    session->player.credits / 1000.0f);
				if (!session_value(session,
				    "How many points of cloak energy? ",
				    &requested, &blank))
					return false;
				quantity = blank ? default_quantity
				    : (float)floor(requested);
				price = 1000.0f;
				if (quantity < 1.0f
				    || points + quantity > 50.0f)
					continue;
				cost = single_mul(quantity, price);
				if (cost > session->player.credits)
					continue;
				session->player.cloak = single_div(
				    floorf(single_add(points, quantity)), 50.0f);
			}
			else {
				if (choice == 2) {
					price = holds_price;
					current = session->player.holds;
					maximum = session->door->game.config.maximum_holds;
				}
				else if (choice == 3) {
					price = fighters_price;
					current = session->player.fighters;
				}
				else if (choice == 7) {
					price = ground_price;
					current = session->player.ground_forces;
				}
				else if (choice == 8) {
					price = shields_price;
					current = session->player.shields;
				}
				else
					continue;
				if (!session_value(session, "How many? ",
				    &requested, &blank))
					return false;
				quantity = (float)floor(requested);
				if (blank || quantity < 1.0f)
					continue;
				cost = single_mul(quantity, price);
				if (cost > session->player.credits) {
					yt_out_line("You can't afford that.");
					continue;
				}
				if (current + quantity > maximum) {
					yt_out_line("That is too many.");
					continue;
				}
				if (choice == 2)
					session->player.holds =
					    single_add(current, quantity);
				else if (choice == 3)
					session->player.fighters =
					    single_add(current, quantity);
				else if (choice == 7)
					session->player.ground_forces =
					    floorf(single_add(current, quantity));
				else
					session->player.shields =
					    floorf(single_add(current, quantity));
			}
			if (!write_player(session, error)
			    || !earth_receipt(session, cost, error))
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

static float *
planet_item(struct yt_planet *planet, int item)
{
	switch (item) {
	case 1: return &planet->stock[0];
	case 2: return &planet->stock[1];
	case 3: return &planet->stock[2];
	case 4: return &planet->fighters;
	case 5: return &planet->missiles;
	case 6: return &planet->mines;
	case 9: return &planet->plasma;
	default: return NULL;
	}
}

static bool
planet_inventory(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	static const char *names[9] = {
		"Ore", "Organics", "Equipment", "Fighters", "Missiles",
		"Mines", "Credits", "Forces", "Plasma bolts"
	};
	float production[9];
	float amount[9];
	float holds[9];
	float sum;
	int index;

	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	sum = single_add(single_add(planet.production[0],
	    planet.production[1]), planet.production[2]);
	production[0] = floorf(planet.production[0]);
	production[1] = floorf(planet.production[1]);
	production[2] = floorf(planet.production[2]);
	production[3] = floorf(sum);
	production[4] = floorf(single_div(sum, 2500.0f));
	production[5] = floorf(single_div(sum, 25000.0f));
	production[6] = (float)floor((double)planet.bank
	    * 0.009999999776482582);
	production[7] = (float)floor((double)planet.ground_forces
	    * 0.009999999776482582 + (double)planet.bank / 10000.0);
	production[8] = floorf(single_mul(sum, 0.00001f));
	amount[0] = floorf(planet.stock[0]);
	amount[1] = floorf(planet.stock[1]);
	amount[2] = floorf(planet.stock[2]);
	amount[3] = floorf(planet.fighters);
	amount[4] = floorf(planet.missiles);
	amount[5] = floorf(planet.mines);
	amount[6] = floorf(planet.bank);
	amount[7] = floorf(planet.ground_forces);
	amount[8] = floorf(planet.plasma);
	holds[0] = session->player.ore;
	holds[1] = session->player.organics;
	holds[2] = session->player.equipment;
	holds[3] = session->player.fighters;
	holds[4] = session->player.missiles;
	holds[5] = session->player.mines;
	holds[6] = session->player.credits;
	holds[7] = session->player.ground_forces;
	holds[8] = session->player.plasma;
	yt_outf("\r\nPlanet %s\r\n", planet.name);
	yt_out_line(" Item           Production     Amount    In Holds");
	yt_out_line("=============  ============   ========  ==========");
	for (index = 0; index < 9; ++index)
		yt_outf("%-13s %11.9g %10.9g %11.9g\r\n", names[index],
		    (double)production[index], (double)amount[index],
		    (double)holds[index]);
	return true;
}

static bool
planet_take_one(struct yt_session *session, int logical_planet, int item,
    struct yt_error *error)
{
	struct yt_planet planet;
	float *source;
	float *destination;
	float free_holds;
	float available;
	float maximum;
	double input;
	bool blank;
	float quantity;
	char prompt[100];

	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	source = planet_item(&planet, item);
	destination = player_item(&session->player, item);
	if (source == NULL || destination == NULL)
		return true;
	free_holds = single_sub(single_sub(single_sub(session->player.holds,
	    session->player.ore), session->player.organics),
	    session->player.equipment);
	available = (float)floor((double)*source);
	maximum = item <= 3 && free_holds < available
	    ? free_holds : available;
	snprintf(prompt, sizeof(prompt), "How much [%.9g ]? ",
	    (double)maximum);
	if (!session_value(session, prompt, &input, &blank))
		return false;
	quantity = blank ? maximum : (float)floor(input);
	if (quantity > floorf(*source) || quantity < 0.0f) {
		yt_out_line("They don't have that many.");
		return true;
	}
	if (quantity > maximum) {
		yt_out_line("You can't take that much!");
		return true;
	}
	*destination = single_add(*destination, quantity);
	if (!write_player(session, error))
		return false;
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	source = planet_item(&planet, item);
	*source = single_sub(*source, quantity);
	return yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error);
}

static bool
planet_take_all(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	static const int weapons[4] = {4, 5, 6, 9};
	int index;

	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	for (index = 0; index < 4; ++index) {
		float amount = floorf(*planet_item(&planet, weapons[index]));
		float *held = player_item(&session->player, weapons[index]);

		*held = single_add(*held, amount);
		*planet_item(&planet, weapons[index]) =
		    single_sub(*planet_item(&planet, weapons[index]), amount);
		yt_outf("Took %.9g.\r\n", (double)amount);
	}
	if (!write_player(session, error)
	    || !yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	for (index = 3; index >= 1; --index) {
		float free_holds;
		float amount;
		float *held;
		float *stored;

		if (!reload_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		free_holds = session->player.holds - session->player.ore
		    - session->player.organics - session->player.equipment;
		stored = planet_item(&planet, index);
		amount = (float)floor((double)*stored);
		if (free_holds < amount)
			amount = free_holds;
		held = player_item(&session->player, index);
		*held = single_add(*held, amount);
		if (!write_player(session, error))
			return false;
		if (!yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		stored = planet_item(&planet, index);
		*stored = single_sub(*stored, amount);
		if (!yt_game_write_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_outf("Took %.9g.\r\n", (double)amount);
	}
	return true;
}

static bool
planet_garrison(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	double desired_value;
	bool blank;
	float desired;
	float after;

	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error)
	    || !session_value(session,
	    "How many ground forces do you want on the planet? ",
	    &desired_value, &blank))
		return false;
	if (blank)
		return true;
	desired = (float)floor(desired_value);
	after = session->player.ground_forces - desired
	    + planet.ground_forces;
	if (desired < 0.0f || after < 0.0f)
		return true;
	planet.ground_forces = desired;
	planet.owner = desired >= 1.0f
	    ? (float)session->player_record : 0.0f;
	if (desired >= 1.0f
	    && !session_sound(session, 4.0f,
	    "planet garrison sound", error))
		return false;
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	session->player.ground_forces = floorf(after);
	return write_player(session, error);
}

static bool
planet_bank(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	double target;
	bool blank;
	double available;
	float old_bank;

	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	old_bank = planet.bank;
	available = (double)session->player.credits + (double)old_bank;
	yt_outf("%.15g credits are available.\r\n", available);
	if (!session_value(session,
	    "How many credits do you want in the account? ", &target,
	    &blank))
		return false;
	if (blank)
		return true;
	target = floor(target);
	if (target < 0.0) {
		yt_out_line("This is a savings account, not a loan.");
		return true;
	}
	if (target > available) {
		yt_out_line("You don't have that many credits.");
		return true;
	}
	planet.bank = (float)target;
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	if (target > 0.0)
		yt_outf("%.15g credits on deposit at 1%% interest.\r\n",
		    target);
	if (!session_sound(session, 4.0f, "planet bank sound", error))
		return false;
	session->player.credits = floorf(single_add(session->player.credits,
	    single_sub(old_bank, (float)target)));
	return write_player(session, error);
}

static bool
planet_rename(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	char name[160];
	bool accepted;

	if (logical_planet == 1
	    || logical_planet == planet_count(session) - 1
	    || logical_planet == planet_count(session)) {
		yt_out_line("You can't re-name this planet!");
		return true;
	}
	for (;;) {
		yt_out("New planet name: ");
		if (!session_line(session, name, sizeof(name)))
			return false;
		qb_title_case(name);
		if (name[0] == '\0')
			return true;
		if (strcmp(name, "The Wanderer") == 0
		    || strcmp(name, "Xannoron") == 0
		    || strcmp(name, "Mercenary Base") == 0) {
			yt_out_line("That name is reserved.");
			return true;
		}
		name[41] = '\0';
		yt_outf("\"%s\"\r\n", name);
		if (!session_yes_no(session, "Is this OK? (Y/n) [Y] ? ",
		    true, &accepted))
			return false;
		if (accepted)
			break;
	}
	if (!yt_game_read_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	snprintf(planet.name, sizeof(planet.name), "%s", name);
	planet.name_length = (float)strlen(name);
	return yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error);
}

static bool
planet_transfer(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	char line[80];
	char command;

	yt_out_line("[B] Plasma Bolts  [C] Cargo  [F] Fighters");
	yt_out_line("[S] Missiles      [M] Mines");
	yt_out("Selection: ");
	if (!session_line(session, line, sizeof(line)))
		return false;
	qb_compat_upper(line);
	command = line[0];
	if (strchr("CSFMB", command) == NULL)
		return true;
	if (command == 'C') {
		size_t index;

		if (!planet_update(session, logical_planet, &planet, error))
			return false;
		if (session->player.ore == 0.0f
		    && session->player.organics == 0.0f
		    && session->player.equipment == 0.0f) {
			yt_out_line("You don't have any cargo!");
			return true;
		}
		for (index = 0; index < 3; ++index) {
			float *held = index == 0 ? &session->player.ore
			    : index == 1 ? &session->player.organics
			    : &session->player.equipment;
			double total = (double)planet.stock[index]
			    + (double)*held;
			if (total > (double)single_mul(
			    planet.production[index], 10.0f))
				planet.production[index] = single_add(
				    (float)floor(total) / 10.0f, 1.0f);
			planet.stock[index] = (float)total;
			*held = 0.0f;
		}
		if (!write_player(session, error)
		    || !yt_game_write_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		yt_out_line("Cargo transferred!!");
		if (!planet_update(session, logical_planet, &planet, error))
			return false;
		return session_sound(session, 4.0f,
		    "planet transfer sound", error);
	}
	if (command == 'F') {
		double input;
		bool blank;
		float amount;

		yt_outf("You have %.9g fighters.\r\n",
		    (double)session->player.fighters);
		if (!session_value(session, "Transfer how many -=> ", &input,
		    &blank))
			return false;
		if (blank)
			return true;
		amount = (float)input;
		if (amount < 0.0f || amount > session->player.fighters)
			return true;
		session->player.fighters =
		    single_sub(session->player.fighters, amount);
		if (!write_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		planet.fighters = single_add(planet.fighters, amount);
	}
	else {
		float *held;
		float amount;
		int item = command == 'B' ? 9 : command == 'S' ? 5 : 6;

		held = player_item(&session->player, item);
		amount = *held;
		*held = 0.0f;
		if (!write_player(session, error)
		    || !yt_game_read_planet(&session->door->game,
		    logical_planet, &planet, error))
			return false;
		*planet_item(&planet, item) =
		    single_add(*planet_item(&planet, item), amount);
	}
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	yt_out_line("Transfer complete.");
	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	return session_sound(session, 4.0f,
	    "planet transfer sound", error);
}

static bool
planet_productivity(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	double credits;
	bool blank;
	double units;
	size_t index;

	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	yt_out_line("Productivity is increased by 1 Unit of EQU, ORG & ORE"
	    " for each 250 credits.");
	if (!session_value(session, "How many credits do you want to spend? ",
	    &credits, &blank))
		return false;
	credits = floor(credits);
	if (blank || credits < 1.0
	    || credits > (double)session->player.credits)
		return true;
	units = credits / 250.0;
	for (index = 0; index < 3; ++index)
		planet.production[index] = single_add(
		    planet.production[index], (float)units);
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	session->player.credits = floorf(single_sub(session->player.credits,
	    (float)(units * 250.0)));
	if (!write_player(session, error))
		return false;
	yt_outf("Productivity increased by %.15g units.\r\n", units);
	return true;
}

static bool
planet_assault(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	struct yt_planet planet;
	double commitment_value;
	bool blank;
	int attackers;
	int defenders;
	char news[300];

	if (!planet_update(session, logical_planet, &planet, error)
	    || !session_value(session,
	    "How many ground forces do you wish to commit? ",
	    &commitment_value, &blank))
		return false;
	attackers = (int)floor(commitment_value);
	if (blank || attackers < 1
	    || (float)attackers > session->player.ground_forces)
		return true;
	session->player.ground_forces =
	    single_sub(session->player.ground_forces, (float)attackers);
	if (!write_player(session, error))
		return false;
	snprintf(news, sizeof(news), "  -  %s attacked planet \"%s\".",
	    session->player.name, planet.name);
	if (!append_news(session, news, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "planet assault engagement sound", error))
		return false;
	defenders = (int)floorf(planet.ground_forces);
	while (attackers > 0 && defenders > 0) {
		float side;
		float amount;

		if (!random_value(session, &side, error)
		    || !random_value(session, &amount, error))
			return false;
		if (side > 0.4000000059604645f) {
			attackers -= (int)floorf(amount * (float)defenders);
			if (attackers < 0)
				attackers = 0;
		}
		else {
			defenders -= (int)floorf(amount * (float)attackers);
			if (defenders < 0)
				defenders = 0;
			if (!session_sound(session, 2.0f,
			    "planet assault defender sound", error))
				return false;
		}
	}
	if (defenders <= 0) {
		if (!append_news(session,
		    " +++ Planetary defenses destroyed!", error)
		    || !session_sound(session, 1.0f,
		    "planet defenses destroyed sound", error))
			return false;
		if (attackers > 0) {
			planet.owner = (float)session->player_record;
			planet.ground_forces = (float)attackers;
			snprintf(news, sizeof(news),
			    " +++ %s captured planet %s!",
			    session->player.name, planet.name);
			if (!append_news(session, news, error)
			    || !session_sound(session, 1.0f,
			    "planet capture sound", error))
				return false;
		}
		else {
			planet.owner = 0.0f;
			planet.ground_forces = 0.0f;
		}
	}
	else {
		planet.ground_forces = (float)defenders;
		snprintf(news, sizeof(news),
		    "  -  %s's assault on \"%s\" failed.",
		    session->player.name, planet.name);
		if (!append_news(session, news, error))
			return false;
	}
	return yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error);
}

static bool
build_route(struct yt_session *session, int start, int destination,
    int *next_hop, bool use_avoid, bool *found, struct yt_error *error)
{
	int count = sector_count(session);
	int *predecessor;
	int *queue;
	int head = 0;
	int tail = 0;
	int current;

	*found = false;
	if (count > 3000 || start < 0 || start > count
	    || destination < 0 || destination > count) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "route bounds");
		}
		return false;
	}
	predecessor = calloc((size_t)count + 1U, sizeof(*predecessor));
	queue = calloc((size_t)count + 2U, sizeof(*queue));
	if (predecessor == NULL || queue == NULL) {
		free(predecessor);
		free(queue);
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	memset(next_hop, 0, ((size_t)count + 1U) * sizeof(*next_hop));
	if (start == destination) {
		*found = true;
		free(predecessor);
		free(queue);
		return true;
	}
	predecessor[start] = -1;
	if (use_avoid) {
		size_t index;

		for (index = 0; index < YT_ARRAY_LEN(session->avoid); ++index) {
			bool overflow;
			int avoided = (int)qb_cint(session->avoid[index],
			    &overflow);

			if (!overflow && avoided >= 0 && avoided <= count)
				predecessor[avoided] = avoided;
			if (avoided == start || avoided == destination) {
				free(predecessor);
				free(queue);
				return true;
			}
		}
		predecessor[start] = -1;
	}
	queue[tail++] = start;
	while (head < tail && predecessor[destination] == 0) {
		struct yt_sector sector;
		size_t slot;

		current = queue[head++];
		if (!yt_game_read_sector(&session->door->game, current, &sector,
		    error)) {
			free(predecessor);
			free(queue);
			return false;
		}
		for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
			bool overflow;
			int neighbor = (int)qb_cint(sector.warps[slot], &overflow);

			if (overflow || neighbor < 0 || neighbor > count
			    || predecessor[neighbor] != 0)
				continue;
			predecessor[neighbor] = current;
			queue[tail++] = neighbor;
			if (neighbor == destination)
				break;
		}
	}
	if (predecessor[destination] != 0) {
		current = destination;
		while (predecessor[current] != -1) {
			next_hop[predecessor[current]] = current;
			current = predecessor[current];
		}
		*found = true;
	}
	free(predecessor);
	free(queue);
	return true;
}

static bool
planet_move_hop(struct yt_session *session, int logical_planet,
    int destination, bool final_hop, bool *stop, struct yt_error *error)
{
	struct yt_planet planet;
	struct yt_sector source;
	struct yt_sector target;
	float draw;
	int source_number = (int)session->player.sector;
	int actual_destination = destination;

	*stop = false;
	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	if (logical_planet == planet_count(session)) {
		yt_out_line("Xannoron cannot be moved!");
		*stop = true;
		return true;
	}
	if (!yt_game_read_sector(&session->door->game, source_number, &source,
	    error)
	    || !yt_game_read_sector(&session->door->game, destination,
	    &target, error))
		return false;
	if (target.planet > 0.0f)
		*stop = true;
	source.planet = 0.0f;
	if (!yt_game_write_sector(&session->door->game, source_number, &source,
	    error)
	    || !random_value(session, &draw, error))
		return false;
	if (draw > 0.9950000047683716f || *stop) {
		char line[300];
		char old_name[sizeof(planet.name)];
		float loss = 0.0f;

		snprintf(old_name, sizeof(old_name), "%s", planet.name);
		planet.name[0] = '\0';
		planet.name_length = 0.0f;
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &planet, error))
			return false;
		snprintf(line, sizeof(line),
		    " *** Planet %s EXPLODED while being moved by %s!!!",
		    old_name, session->player.name);
		if (!append_news(session, line, error))
			return false;
		if (!session_sound(session, 3.0f,
		    "planet move explosion sound", error))
			return false;
		if (session->player.fighters > 0.0f) {
			float first;
			float second;

			if (!random_value(session, &first, error)
			    || !random_value(session, &second, error))
				return false;
			first = floorf(single_mul(first,
			    session->player.fighters)) + 1.0f;
			loss = floorf(single_mul(second, first)) + 1.0f;
			if (loss > session->player.fighters)
				loss = session->player.fighters;
			session->player.fighters =
			    single_sub(session->player.fighters, loss);
			if (!write_player(session, error))
				return false;
			snprintf(line, sizeof(line),
			    "%s lost %.9g fighters in the explosion!",
			    session->player.name, (double)loss);
			if (!append_news(session, line, error))
				return false;
		}
		*stop = true;
		return true;
	}
	if (logical_planet == 1) {
		for (;;) {
			if (!random_value(session, &draw, error))
				return false;
			actual_destination =
			    1 + (int)floorf(single_mul(draw,
			    (float)sector_count(session)));
			if (!yt_game_read_sector(&session->door->game,
			    actual_destination, &target, error))
				return false;
			if (target.planet <= 0.0f)
				break;
		}
		*stop = true;
	}
	target.planet = (float)logical_planet;
	if (!yt_game_write_sector(&session->door->game, actual_destination,
	    &target, error))
		return false;
	session->player.turns =
	    single_sub(session->player.turns, 10.0f);
	session->player.sector = (float)destination;
	session->sector_cache[session->player_record] =
	    session->player.sector;
	if (!write_player(session, error))
		return false;
	if (logical_planet != 1 && final_hop
	    && !session_sound(session, 4.0f,
	    "planet move completion sound", error))
		return false;
	{
		bool friendly = sector_force_friendly(session, &target, error);

		if (!friendly && error != NULL && error->status != YT_OK)
			return false;
		if (target.mines != 0.0f
		    || (target.fighters != 0.0f && !friendly))
			*stop = true;
	}
	return true;
}

static bool
planet_move(struct yt_session *session, int logical_planet,
    bool one_hop, struct yt_error *error)
{
	double input;
	bool blank;
	int destination;
	int current = (int)session->player.sector;
	int count = sector_count(session);
	int *route;
	bool found;
	int hops = 0;
	int cursor;
	bool accepted;
	bool stop = false;

	if (!session_value(session, "Move planet to which sector? ", &input,
	    &blank))
		return false;
	destination = (int)floor(input);
	if (blank || destination < 1 || destination > count
	    || destination == current)
		return true;
	route = calloc((size_t)count + 1U, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (one_hop) {
		struct yt_sector sector;
		size_t slot;

		found = false;
		if (!yt_game_read_sector(&session->door->game, current, &sector,
		    error)) {
			free(route);
			return false;
		}
		for (slot = 0; slot < YT_ARRAY_LEN(sector.warps); ++slot) {
			if (sector.warps[slot] == (float)destination)
				found = true;
		}
		if (found)
			route[current] = destination;
	}
	else if (!build_route(session, current, destination, route, true, &found,
	    error)) {
		free(route);
		return false;
	}
	if (!found) {
		free(route);
		yt_out_line("*** You can't get there without going someplace"
		    " you dont want to!");
		return true;
	}
	cursor = current;
	while (route[cursor] != 0) {
		++hops;
		cursor = route[cursor];
	}
	if ((float)(hops * 10) > session->player.turns) {
		free(route);
		yt_out_line("You don't have enough turns.");
		return true;
	}
	yt_outf("This move will cost %d turns.\r\n", hops * 10);
	if (!session_yes_no(session, "Move the planet? [y/N] ", false,
	    &accepted)) {
		free(route);
		return false;
	}
	if (!accepted) {
		free(route);
		return true;
	}
	cursor = current;
	while (route[cursor] != 0 && !stop) {
		int next = route[cursor];

		if (!planet_move_hop(session, logical_planet, next,
		    next == destination, &stop,
		    error)) {
			free(route);
			return false;
		}
		cursor = next;
	}
	free(route);
	return true;
}

static bool
planet_menu(struct yt_session *session, int logical_planet,
    struct yt_error *error)
{
	for (;;) {
		char command[80];
		char upper[80];
		const char *position;
		const char *alphabet = "F!MPC1234569LTAB$";

		if (!reload_player(session, error))
			return false;
		yt_outf("Free Holds: %.9g\r\n",
		    (double)(session->player.holds - session->player.ore
		    - session->player.organics - session->player.equipment));
		if (!planet_update(session, logical_planet,
		    &(struct yt_planet){0}, error))
			return false;
		yt_out("Planet command (?=help) [A]? ");
		if (!session_line(session, command, sizeof(command)))
			return false;
		snprintf(upper, sizeof(upper), "%s", command);
		qb_compat_upper(upper);
		if (upper[0] == '\0')
			strcpy(upper, "A");
		if (strcmp(upper, "I") == 0) {
			if (!show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(upper, "N") == 0) {
			if (!planet_rename(session, logical_planet, error))
				return false;
			continue;
		}
		if (strcmp(upper, "S") == 0) {
			if (!display_sector(session, false, error))
				return false;
			continue;
		}
		if (strcmp(upper, "Q") == 0) {
			bool quit;

			if (!session_yes_no(session, "Quit Yankee Trader? [y/N] ",
			    false, &quit))
				return false;
			if (quit) {
				session->running = false;
				return true;
			}
			continue;
		}
		if (strcmp(upper, "D") == 0) {
			if (!planet_inventory(session, logical_planet, error))
				return false;
			continue;
		}
		if (strcmp(upper, "?") == 0) {
			yt_out_line("1-6,9 Take  A Take All  B Bank  D Display");
			yt_out_line("F Forces  L Leave  N Rename  T Transfer");
			yt_out_line("! Thrusters  M Move  $ Productivity");
			continue;
		}
		position = strstr(alphabet, upper);
		if (position == NULL) {
			yt_out_line("Invalid command.");
			continue;
		}
		switch ((int)(position - alphabet)) {
		case 0:
			if (!planet_garrison(session, logical_planet, error))
				return false;
			break;
		case 1:
			if (!planet_move(session, logical_planet, true, error))
				return false;
			break;
		case 2:
			if (!planet_move(session, logical_planet, false, error))
				return false;
			break;
		case 3:
			if (!command_trade(session, error))
				return false;
			break;
		case 4:
			yt_out_line("Computer functions are available from orbit.");
			break;
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
create_planet(struct yt_session *session, struct yt_sector *sector,
    struct yt_error *error)
{
	bool accepted;
	char name[160];
	int logical;
	struct yt_planet planet;
	char news[300];

	if (session->player.credits < 25000.0f) {
		yt_out_line("A Genesis Device costs 25,000 credits.");
		return true;
	}
	if (!session_yes_no(session,
	    "Create a planet here for 25,000 credits? [y/N] ",
	    false, &accepted) || !accepted)
		return accepted || error == NULL || error->status == YT_OK;
	yt_out("Planet name: ");
	if (!session_line(session, name, sizeof(name)))
		return false;
	qb_title_case(name);
	name[41] = '\0';
	if (name[0] == '\0')
		return true;
	for (logical = 2; logical <= planet_count(session); ++logical) {
		if (!yt_game_read_planet(&session->door->game, logical, &planet,
		    error))
			return false;
		if (planet.name_length == 0.0f)
			break;
	}
	if (logical > planet_count(session)) {
		yt_out_line("There is no room for another planet.");
		return true;
	}
	snprintf(planet.name, sizeof(planet.name), "%s", name);
	planet.name_length = (float)strlen(name);
	planet.last_day = (float)session->door->game.today;
	planet.last_minute = floorf(current_minute());
	planet.production[0] = 1.0f;
	planet.production[1] = 1.0f;
	planet.production[2] = 1.0f;
	planet.stock[0] = 10.0f;
	planet.stock[1] = 10.0f;
	planet.stock[2] = 10.0f;
	planet.owner = (float)session->player_record;
	planet.ground_forces = 1.0f;
	planet.fighters = 30.0f;
	planet.missiles = 0.0f;
	planet.mines = 0.0f;
	planet.plasma = 0.0f;
	planet.bank = 0.0f;
	if (!yt_game_write_planet(&session->door->game, logical, &planet,
	    error))
		return false;
	sector->planet = (float)logical;
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, sector, error))
		return false;
	session->player.credits =
	    floorf(single_sub(session->player.credits, 25000.0f));
	if (!write_player(session, error))
		return false;
	snprintf(news, sizeof(news), "  -  %s purchased planet \"%s\".",
	    session->player.name, name);
	if (!append_news(session, news, error))
		return false;
	yt_outf("Planet \"%s\" created with Genesis Device!\r\n", name);
	if (!session_sound(session, 4.0f,
	    "planet creation sound", error))
		return false;
	yt_out_line("To increase productivity on your new planet, spend credits"
	    " [$] on it.");
	return true;
}

static bool
command_land(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_planet planet;
	int logical;
	bool allowed = false;

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.planet <= 0.0f)
		return create_planet(session, &sector, error);
	logical = (int)sector.planet;
	if (!planet_update(session, logical, &planet, error))
		return false;
	if (floorf(planet.ground_forces) <= 0.0f
	    || planet.owner == (float)session->player_record)
		allowed = true;
	else if (planet.owner >= YT_PLAYER_FIRST
	    && planet.owner <= YT_PLAYER_LAST) {
		allowed = same_team(session, (int)planet.owner, error);
		if (!allowed && error != NULL && error->status != YT_OK)
			return false;
	}
	if (!allowed && (planet.owner == 0.0f
	    || (planet.owner >= YT_PLAYER_FIRST
	    && planet.owner <= YT_PLAYER_LAST))) {
		struct yt_player owner;
		bool vacant = planet.owner == 0.0f;

		if (!vacant) {
			if (!yt_game_read_player(&session->door->game,
			    (int)planet.owner, &owner, error))
				return false;
			vacant = owner.killed_by != 0.0f;
		}
		if (vacant) {
			float first;
			float second;

			yt_out_line("This planet has no governor! Hail to the new"
			    " planetary governor!!");
			if (!session_sound(session, 1.0f,
			    "vacant planet sound", error))
				return false;
			session_timed_wait(session, 2.0);
			if (!random_value(session, &first, error)
			    || !random_value(session, &second, error))
				return false;
			planet.ground_forces = floorf(single_mul(
			    single_mul(first, second),
			    planet.ground_forces));
			planet.owner = planet.ground_forces > 0.0f
			    ? (float)session->player_record : 0.0f;
			if (!yt_game_write_planet(&session->door->game,
			    logical, &planet, error))
				return false;
			allowed = true;
		}
	}
	if (!allowed) {
		bool assault = false;

		yt_out_line("Planetary traffic control denies landing.");
		if (session->player.ground_forces >= 1.0f
		    && !session_yes_no(session,
		    "Launch a ground assault? [y/N] ", false, &assault))
			return false;
		if (!assault)
			return true;
		if (!planet_assault(session, logical, error))
			return false;
		if (!yt_game_read_planet(&session->door->game, logical,
		    &planet, error))
			return false;
		if (planet.owner != (float)session->player_record)
			return true;
	}
	return planet_menu(session, logical, error);
}

static bool
team_load(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = id;
	if (id < 1 || id > YT_DEFAULT_PLAYER_COUNT)
		return true;
	if (!yt_game_read_sector(&session->door->game, id, &team->overlay,
	    error))
		return false;
	yt_record_get_text(&team->overlay.record, team->name,
	    sizeof(team->name));
	memcpy(team->password, team->overlay.record.bytes + YT_F113, 4);
	team->password[4] = '\0';
	team->captain =
	    yt_record_get_number(&team->overlay.record, YT_F77);
	team->full = true;
	for (index = 0; index < 4; ++index) {
		team->roster[index] = yt_record_get_number(
		    &team->overlay.record, offsets[index]);
		session->team_roster_cache[index] = team->roster[index];
		if (team->roster[index] != 0.0f)
			team->live = true;
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
team_store_roster(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	for (index = 0; index < 4; ++index)
		yt_record_set_number_if_changed(&team->overlay.record,
		    offsets[index], team->roster[index]);
	return yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team->id), &team->overlay.record, error);
}

static bool
team_password(struct yt_session *session, char password[5])
{
	char line[80];

	for (;;) {
		yt_out("Four-character password: ");
		if (!session_line(session, line, sizeof(line)))
			return false;
		qb_compat_upper(line);
		if (strlen(line) == 4U) {
			memcpy(password, line, 4);
			password[4] = '\0';
			return true;
		}
	}
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
team_resolve_captain(struct yt_session *session, struct yt_team *team,
    bool *captain, struct yt_error *error)
{
	struct yt_player incumbent;
	bool valid = team->captain >= YT_PLAYER_FIRST
	    && team->captain <= session->door->game.config.sector_offset;

	if (valid) {
		if (!yt_game_read_player(&session->door->game,
		    (int)team->captain, &incumbent, error))
			return false;
		valid = incumbent.name_length > 0.0f
		    && incumbent.team == (float)team->id;
	}
	if (!valid) {
		team->captain = (float)session->player_record;
		if (!team_store(session, team, error))
			return false;
	}
	*captain = team->captain == (float)session->player_record;
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
info_team_lines(struct yt_session *session, struct yt_error *error)
{
	struct yt_team team;
	struct yt_player captain;
	float team_id;
	char number[64];
	char row[256];
	int captain_record;
	int last_player = (int)session->door->game.config.sector_offset - 1;
	bool captain_valid;

	if (!reload_player(session, error))
		return false;
	team_id = session->player.team;
	if (team_id == 0.0f)
		return info_line(session, "Team  : None", 12, error)
		    && info_line(session, NULL, 0, error);
	if (team_id != floorf(team_id) || team_id < 1.0f || team_id > 50.0f)
		return info_failure(error, "Info team record");
	if (!team_load(session, (int)team_id, &team, error))
		return false;
	qb_str_single(number, sizeof(number), team_id);
	(void)snprintf(row, sizeof(row), "Team  :%s, %s", number, team.name);
	if (!info_line(session, row, strlen(row), error)
	    || !info_line(session, NULL, 0, error))
		return false;
	if (team.captain == (float)session->player_record) {
		(void)snprintf(row, sizeof(row),
		    "You are the Captain of team%s!", number);
		return info_line(session, row, strlen(row), error)
		    && info_line(session, NULL, 0, error);
	}
	captain_valid = team.captain == floorf(team.captain)
	    && team.captain >= YT_PLAYER_FIRST
	    && team.captain <= (float)last_player;
	captain_record = (int)team.captain;
	if (captain_valid) {
		if (!yt_game_read_player(&session->door->game, captain_record,
		    &captain, error))
			return false;
		captain_valid = captain.name_length > 0.0f
		    && captain.team == team_id;
	}
	if (captain_valid) {
		bool overflow;
		int32_t length;

		if (!yt_game_read_player(&session->door->game, captain_record,
		    &captain, error))
			return false;
		length = qb_cint((double)captain.name_length, &overflow);
		if (overflow || length < 0 || length > (int)YT_TEXT_FIELD_SIZE)
			return info_failure(error, "Info captain name length");
		(void)snprintf(row, sizeof(row), "Your Team Captain is: %.*s!",
		    (int)length, captain.name);
		return info_line(session, row, strlen(row), error)
		    && info_line(session, NULL, 0, error);
	}
	if (!team_load(session, (int)team_id, &team, error))
		return false;
	team.captain = (float)session->player_record;
	yt_record_set_number_if_changed(&team.overlay.record, YT_F77,
	    team.captain);
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config,
	    team.id), &team.overlay.record, error))
		return false;
	return info_line(session,
	    "Your team has no captain! You've been promoted to Captain!", 58,
	    error)
	    && info_line(session,
	    "Congratulations Captain! See Team Menu for your new options!", 60,
	    error)
	    && info_line(session, NULL, 0, error);
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
	    || !info_team_lines(session, error)
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
team_create(struct yt_session *session, struct yt_error *error)
{
	struct yt_team team;
	char name[160];
	char news[300];
	size_t raw_length;
	int id;

	yt_out("Team name: ");
	if (!session_line(session, name, sizeof(name)))
		return false;
	raw_length = strlen(name);
	if (raw_length < 3U)
		return true;
	qb_title_case(name);
	name[41] = '\0';
	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!team_load(session, id, &team, error))
			return false;
		if (!team.live)
			break;
	}
	/* The original falls through to overlay zero when all fifty are live. */
	if (id > YT_DEFAULT_PLAYER_COUNT) {
		id = 0;
		if (!team_load(session, id, &team, error))
			return false;
		team.id = 0;
	}
	snprintf(team.name, sizeof(team.name), "%s", name);
	yt_record_set_text_if_changed(&team.overlay.record,
	    (const uint8_t *)team.name, strlen(team.name));
	yt_record_set_number_if_changed(&team.overlay.record, YT_F73,
	    (float)strlen(team.name));
	if (!yt_database_write(&session->door->game.database,
	    (size_t)yt_sector_basic_record(&session->door->game.config, id),
	    &team.overlay.record, error))
		return false;
	session->player.team = (float)id;
	if (!write_player(session, error))
		return false;
	team.captain = (float)session->player_record;
	team.roster[0] = (float)session->player_record;
	team.roster[1] = 0.0f;
	team.roster[2] = 0.0f;
	team.roster[3] = 0.0f;
	if (!team_password(session, team.password)
	    || !team_store(session, &team, error))
		return false;
	snprintf(news, sizeof(news), "%s Created Team %.9g -=- %s",
	    session->player.name, (double)id, name);
	return append_news(session, news, error);
}

static bool
team_join(struct yt_session *session, struct yt_error *error)
{
	struct yt_team team;
	char line[80];
	char password[5];
	int id;
	int selected;
	size_t index;
	char news[300];

	for (id = 1; id <= YT_DEFAULT_PLAYER_COUNT; ++id) {
		if (!team_load(session, id, &team, error))
			return false;
		if (team.live)
			yt_outf("%d) %s\r\n", id, team.name);
	}
	yt_out("Join which team? ");
	if (!session_line(session, line, sizeof(line)))
		return false;
	selected = (int)floor(qb_val(line).value);
	if (selected < 1)
		return true;
	if (!team_load(session, selected, &team, error))
		return false;
	if (!team.live) {
		yt_out_line("That team does not exist.");
		return true;
	}
	if (team.full) {
		yt_out_line("That team is full.");
		return true;
	}
	if (!team_password(session, password))
		return false;
	if (memcmp(password, team.password, 4) != 0) {
		if (!team_audit(session, (float)selected, 0.0f, password,
		    error))
			return false;
		return session_framed_row(session,
		    "Invalid Password entered!", true,
		    "invalid team password row", error);
	}
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
	{
		char team_number[40];

		qb_str_single(team_number, sizeof(team_number),
		    (float)selected);
		snprintf(news, sizeof(news), "%s Joined Team%s",
		    session->player.name, team_number);
	}
	if (!append_news(session, news, error))
		return false;
	session->presentation.foreground = 3.0f;
	session->pager.foreground = 3;
	if (!session_framed_row(session,
	    "Your Team info has been recorded!  Have fun!", true,
	    "team join success row", error))
		return false;
	return team_audit(session, (float)selected, 1.0f, "", error);
}

static bool
team_quit(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	bool accepted;
	float old_team;
	size_t index;
	bool live = false;

	if (!session_yes_no(session, "Quit your team? [y/N] ", false,
	    &accepted) || !accepted)
		return accepted || error == NULL || error->status == YT_OK;
	if (!reload_player(session, error))
		return false;
	old_team = session->player.team;
	session->player.team = 0.0f;
	if (!write_player(session, error))
		return false;
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
	if (!session_framed_row(session,
	    "You have been removed from Team play", false,
	    "team quit success row", error))
		return false;
	return team_audit(session, old_team, 2.0f, "", error);
}

static bool
team_search(struct yt_session *session, struct yt_error *error)
{
	int basic;
	bool found = false;

	yt_out_line("Team resources:");
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset; ++basic) {
		struct yt_player player;

		if (!yt_game_read_player(&session->door->game, basic, &player,
		    error))
			return false;
		if (basic != session->player_record
		    && player.team == session->player.team) {
			yt_outf("%s in sector %.9g\r\n", player.name,
			    (double)player.sector);
			found = true;
		}
	}
	/* Preserve the retained-loop owner value used by both resource scans. */
	for (basic = 1; basic <= sector_count(session); ++basic) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&session->door->game, basic, &sector,
		    error))
			return false;
		if (sector.fighters > 0.0f
		    && sector.fighter_owner
		    == session->door->game.config.sector_offset + 1.0f)
			yt_outf("Fighters in sector %d\r\n", basic);
		if (sector.planet > 0.0f) {
			struct yt_planet planet;

			if (!yt_game_read_planet(&session->door->game,
			    (int)sector.planet, &planet, error))
				return false;
			if (planet.owner
			    == session->door->game.config.sector_offset + 1.0f)
				yt_outf("Planet %s in sector %d\r\n",
				    planet.name, basic);
		}
	}
	if (!found)
		yt_out_line("None Found");
	return true;
}

static bool
team_transfer_bug(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	double amount_value;
	bool blank;
	float amount;

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.fighters == 0.0f)
		return true;
	if (!session_value(session, "Transfer how many fighters? ",
	    &amount_value, &blank))
		return false;
	amount = (float)floor(amount_value);
	if (blank || amount < 1.0f || amount > session->player.fighters)
		return true;
	sector.fighters = single_add(sector.fighters, amount);
	if (!yt_game_write_sector(&session->door->game,
	    (int)session->player.sector, &sector, error)
	    || !reload_player(session, error))
		return false;
	session->player.sector =
	    single_sub(session->player.fighters, amount);
	return write_player(session, error);
}

static bool
team_banish(struct yt_session *session, struct yt_team *team,
    struct yt_error *error)
{
	size_t index;

	for (index = 0; index < 4; ++index) {
		struct yt_player member;
		bool accepted;
		char prompt[160];

		if (team->roster[index] <= 0.0f
		    || team->roster[index] == team->captain)
			continue;
		if (!yt_game_read_player(&session->door->game,
		    (int)team->roster[index], &member, error))
			return false;
		snprintf(prompt, sizeof(prompt), "Banish %s? [y/N] ",
		    member.name);
		if (!session_yes_no(session, prompt, false, &accepted))
			return false;
		if (!accepted)
			continue;
		member.team = 0.0f;
		if (!yt_game_write_player(&session->door->game,
		    (int)team->roster[index], &member, error))
			return false;
		team->roster[index] = 0.0f;
		yt_out_line("Change your password now.");
		return team_store(session, team, error);
	}
	return true;
}

static bool
command_team(struct yt_session *session, struct yt_error *error)
{
	for (;;) {
		char line[80];
		struct yt_team team;
		bool captain = false;
		double numeric;

		if (!reload_player(session, error))
			return false;
		if (session->player.team != 0.0f) {
			if (!team_load(session, (int)session->player.team, &team,
			    error)
			    || !team_resolve_captain(session, &team, &captain,
			    error))
				return false;
		}
		yt_out_line("1) Exit");
		if (session->player.team == 0.0f) {
			yt_out_line("2) Create Team");
			yt_out_line("3) Join Team");
		}
		else {
			yt_out_line("4) Quit Team");
			yt_out_line("5) Search Team Resources");
			yt_out_line("6) Transfer Fighters");
			if (captain) {
				yt_out_line("7) Banish Member");
				yt_out_line("8) Change Password");
				yt_out_line("9) Change Team Name");
			}
		}
		yt_out("Choice: ");
		if (!session_line(session, line, sizeof(line)))
			return false;
		numeric = qb_val(line).value;
		if ((numeric > 3.0 && session->player.team == 0.0f)
		    || (numeric > 6.0 && !captain)
		    || (numeric > 1.0 && numeric < 4.0
		    && session->player.team != 0.0f)
		    || numeric < 1.0 || numeric > 10.0)
			continue;
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
			if (!team_transfer_bug(session, error))
				return false;
		}
		else if (strcmp(line, "7") == 0) {
			if (!team_banish(session, &team, error))
				return false;
		}
		else if (strcmp(line, "8") == 0) {
			if (!team_password(session, team.password)
			    || !team_store(session, &team, error))
				return false;
		}
		else if (strcmp(line, "9") == 0) {
			char name[160];

			yt_out("New team name: ");
			if (!session_line(session, name, sizeof(name)))
				return false;
			if (strlen(name) < 3U)
				continue;
			qb_title_case(name);
			name[41] = '\0';
			snprintf(team.name, sizeof(team.name), "%s", name);
			if (!team_store(session, &team, error))
				return false;
		}
	}
}

static bool
port_rename(struct yt_session *session, int logical_port,
    struct yt_port *port, struct yt_error *error)
{
	char name[160];
	bool accepted;

	if (logical_port == 1)
		return true;
	for (;;) {
		yt_out("Enter a new port name: ");
		if (!session_line(session, name, sizeof(name)))
			return false;
		qb_title_case(name);
		name[41] = '\0';
		if (name[0] == '\0')
			snprintf(name, sizeof(name), "%s", port->name);
		yt_outf("\"%s\"\r\n", name);
		if (!session_yes_no(session, "Is this OK? [y/N] ", false,
		    &accepted))
			return false;
		if (!accepted)
			continue;
		snprintf(port->name, sizeof(port->name), "%s", name);
		port->name_length = (float)strlen(name);
		return yt_game_write_port(&session->door->game, logical_port,
		    port, error);
	}
}

static bool
command_rename_port(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_port port;
	int logical_port;

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.port <= 0.0f) {
		yt_out_line("No port here!");
		return true;
	}
	logical_port = (int)sector.port;
	if (!yt_game_read_port(&session->door->game, logical_port, &port,
	    error))
		return false;
	if (port.owner != (float)session->player_record) {
		yt_out_line("You don't own this port.");
		return true;
	}
	if (logical_port == 1) {
		yt_out_line("Earth cannot be renamed.");
		return true;
	}
	return port_rename(session, logical_port, &port, error);
}

static bool
command_buy_port(struct yt_session *session, struct yt_error *error)
{
	struct yt_sector sector;
	struct yt_port port;
	float prices[3];
	double quantities[3];
	double price;
	int logical_port;
	bool accepted;

	if (!yt_game_read_sector(&session->door->game,
	    (int)session->player.sector, &sector, error))
		return false;
	if (sector.port <= 0.0f) {
		yt_out_line("No port here!");
		return true;
	}
	logical_port = (int)sector.port;
	if (logical_port == 1) {
		if (!earth_store(session, error))
			return false;
		if (!reload_player(session, error))
			return false;
		if (!yt_game_read_port(&session->door->game, 1, &port, error))
			return false;
		price = 1000000000.0;
	}
	else {
		float sum;

		if (!port_update(session, logical_port, &port, prices, quantities,
		    error))
			return false;
		if (!port_report(session, logical_port, &port, prices,
		    quantities, error))
			return false;
		sum = single_add(single_add(port.production[0],
		    port.production[1]), port.production[2]);
		price = (double)single_add(floorf(single_div(sum, 10.0f)),
		    1.0f);
	}
	if (port.owner == (float)session->player_record) {
		yt_out_line("You already own this port.");
		return true;
	}
	if ((double)session->player.credits < price) {
		yt_out_line("You can't afford this port.");
		return true;
	}
	yt_outf("This port costs %.15g credits.\r\n", price);
	if (!session_yes_no(session, "Buy this port? [y/N] ", false,
	    &accepted))
		return false;
	if (!accepted)
		return true;
	if (port.owner != 0.0f) {
		struct yt_player seller;
		char message[300];

		if (!yt_game_read_player(&session->door->game, (int)port.owner,
		    &seller, error))
			return false;
		seller.credits = floorf(single_add(seller.credits,
		    single_add(port.treasury, (float)price)));
		seller.ports_owned =
		    single_sub(seller.ports_owned, 1.0f);
		if (!yt_game_write_player(&session->door->game,
		    (int)port.owner, &seller, error))
			return false;
		snprintf(message, sizeof(message),
		    "%s purchased your port %s.", session->player.name,
		    port.name);
		if (!radio_append(message, (float)session->player_record,
		    port.owner, error))
			return false;
	}
	if (logical_port != 1
	    && !port_rename(session, logical_port, &port, error))
		return false;
	port.owner = (float)session->player_record;
	port.treasury = 0.0f;
	if (!yt_game_write_port(&session->door->game, logical_port, &port,
	    error))
		return false;
	session->player.credits =
	    floorf(single_sub(session->player.credits, (float)price));
	session->player.ports_owned =
	    single_add(session->player.ports_owned, 1.0f);
	return write_player(session, error);
}

static bool
command_collect(struct yt_session *session, bool collecting,
    struct yt_error *error)
{
	struct yt_player current;
	int logical;
	double collected = 0.0;
	float owned = 0.0f;
	float credited = 0.0f;
	char number_one[64];
	char number_two[64];
	char text[160];

	yt_out_line("");
	if (!yt_game_read_player(&session->door->game, session->player_record,
	    &current, error))
		return false;
	if (current.ports_owned < 1.0f) {
		yt_out_line("You don't OWN any ports!!!");
		return true;
	}
	yt_out(collecting ? "Sending out armored cargo ships to"
	    : "Checking galactic bank statement for");
	yt_out_line(" ports with credits...");
	yt_out_line("");

	for (logical = 1; logical <= port_count(session); ++logical) {
		struct yt_port port;

		if (!yt_game_read_port(&session->door->game, logical, &port,
		    error))
			return false;
		if (port.owner != (float)session->player_record)
			continue;
		owned = single_add(owned, 1.0f);
		if (port.treasury == 0.0f)
			continue;
		credited = single_add(credited, 1.0f);
		collected += (double)port.treasury;
		qb_str_single(number_one, sizeof(number_one), port.sector);
		snprintf(text, sizeof(text), "Sector:%s", number_one);
		if (!session_fixed_width(session, text, 14.0f,
		    "treasury sector field", error))
			return false;
		{
			bool overflow;
			int length = (int)qb_cint(port.name_length, &overflow);
			char name[42];

			if (overflow || length < 0)
				length = 0;
			if ((size_t)length > strlen(port.name))
				length = (int)strlen(port.name);
			snprintf(name, sizeof(name), "%.*s", length, port.name);
			if (!session_fixed_width(session, name, 25.0f,
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
		yt_out_line(text);
		if (collecting) {
			port.treasury = 0.0f;
			if (!yt_game_write_port(&session->door->game, logical,
			    &port, error))
				return false;
		}
	}
	if (collected != 0.0)
		yt_out_line("");
	qb_str_single(number_one, sizeof(number_one), owned);
	snprintf(text, sizeof(text), "Total ports...:%s", number_one);
	yt_out_line(text);
	qb_str_single(number_one, sizeof(number_one), credited);
	snprintf(text, sizeof(text), "With credits..:%s", number_one);
	yt_out_line(text);
	qb_str_single(number_one, sizeof(number_one),
	    single_sub(owned, credited));
	snprintf(text, sizeof(text), "Barren ports..:%s", number_one);
	yt_out_line(text);
	qb_str_double(number_two, sizeof(number_two), collected);
	snprintf(text, sizeof(text), "Total credits.:%s", number_two);
	yt_out_line(text);
	yt_out_line("");
	if (!collecting) {
		snprintf(text, sizeof(text), "You have%s credits in your port "
		    "accounts.", number_two);
		yt_out_line(text);
		return true;
	}
	snprintf(text, sizeof(text), "You collected a total of%s credits.",
	    number_two);
	yt_out_line(text);
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
	bool accepted;
	char sibling[1024];
	char *arguments[2];

	if (!session_yes_no(session,
	    "Initiate whole-universe Genesis? [y/N] ", false, &accepted))
		return false;
	if (session->door->game.config.genesis_ports > 300.0f) {
		yt_out_line("*FUNCTION DISABLED*");
		accepted = false;
	}
	if (!accepted)
		return true;
	if (session->player.ports_owned
	    < session->door->game.config.genesis_ports) {
		yt_out_line("You do not own enough ports.");
		return true;
	}
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

static bool
missile_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, float *remaining, struct yt_error *error)
{
	struct yt_planet planet;
	int logical_planet = (int)sector->planet;
	float original_ore;
	float old_total;
	bool friendly = false;
	char sector_text[64];
	char number_one[64];
	char number_two[64];
	char row[256];

	if (*remaining <= 0.0f || sector->planet == 0.0f)
		return true;
	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	if (planet.owner == (float)session->player_record)
		friendly = true;
	else if (planet.owner > 0.0f)
		friendly = same_team(session, (int)planet.owner, error);
	if (!friendly && error != NULL && error->status != YT_OK)
		return false;
	if (friendly) {
		snprintf(row, sizeof(row),
		    "NOT attacking friendly planet \"%s\"!", planet.name);
		yt_out_line(row);
		return true;
	}
	qb_str_single(sector_text, sizeof(sector_text), (float)sector_number);
	snprintf(row, sizeof(row), "The Missiles attacked planet %s in sector%s!",
	    planet.name, sector_text);
	yt_out_line(row);
	snprintf(row, sizeof(row), "%s's Missiles attacked planet %s in sector%s!",
	    session->player.name, planet.name, sector_text);
	if (!append_news(session, row, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "cruise missile planet attack sound", error))
		return false;
	while (planet.ground_forces > 0.0f && *remaining > 0.0f) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		planet.ground_forces =
		    single_sub(planet.ground_forces,
		    single_mul(draw, 25.0f));
		*remaining = single_sub(*remaining, 1.0f);
	}
	planet.ground_forces = floorf(planet.ground_forces);
	if (planet.ground_forces < 1.0f) {
		planet.ground_forces = 0.0f;
		planet.owner = 0.0f;
	}
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	qb_str_single(number_one, sizeof(number_one), planet.ground_forces);
	snprintf(row, sizeof(row), "Ground forces reduced to%s!", number_one);
	yt_out_line(row);
	if (!append_news(session, row, error))
		return false;
	if (*remaining <= 0.0f)
		return *remaining <= 0.0f
		    && (error == NULL || error->status == YT_OK);
	original_ore = planet.production[0];
	old_total = single_add(single_add(planet.production[0],
	    planet.production[1]), planet.production[2]);
	while ((original_ore > 0.0f || planet.production[1] > 0.0f
	    || planet.production[2] > 0.0f) && *remaining > 0.0f) {
		size_t index;

		for (index = 0; index < 3; ++index) {
			float draw;

			if (!random_value(session, &draw, error))
				return false;
			planet.production[index] =
			    single_sub(planet.production[index],
			    single_mul(draw, 2000.0f));
		}
		*remaining = single_sub(*remaining, 1.0f);
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
		yt_out_line(row);
		if (!append_news(session, row, error))
			return false;
	}
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	if (planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f) {
		planet.name_length = 0.0f;
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &planet, error))
			return false;
		sector->planet = 0.0f;
		if (!yt_game_write_sector(&session->door->game, sector_number,
		    sector, error))
			return false;
		yt_out_line("The planet was destroyed!!");
		if (!session_sound(session, 3.0f,
		    "cruise missile planet destruction sound", error)
		    || !append_news(session, "The planet was destroyed!!", error))
			return false;
	}
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
	sector.mines = single_add(sector.mines, mines);
	return yt_game_write_sector(&session->door->game, sector_number,
	    &sector, error);
}

static bool
projectile_fighter_owner(struct yt_session *session, float owner,
    bool missile, char *label, size_t size, bool *friendly,
    struct yt_error *error)
{
	*friendly = false;
	snprintf(label, size, "%s", "The Xannor");
	if (owner == -2.0f)
		snprintf(label, size, "%s", "Mercenaries");
	else if (owner > 1.0f
	    && owner <= session->door->game.config.sector_offset) {
		struct yt_player defender;
		bool overflow;
		int length;

		if (!yt_game_read_player(&session->door->game, (int)owner,
		    &defender, error))
			return false;
		length = (int)qb_cint(defender.name_length, &overflow);
		if (overflow || length < 0)
			length = 0;
		if ((size_t)length > strlen(defender.name))
			length = (int)strlen(defender.name);
		snprintf(label, size, "%.*s", length, defender.name);
		if (missile)
			*friendly = session->player.team > 0.0f
			    && defender.team == session->player.team;
	}
	if (owner == (float)session->player_record) {
		snprintf(label, size, "%s",
		    missile && session->player_record == -1 ? "THEM" : "YOU");
		*friendly = true;
	}
	return true;
}

static bool
missile_sector(struct yt_session *session, int sector_number,
    float *remaining, int *counterattack, int *xannor_provoker,
	int *last_mine_news_sector, struct yt_error *error)
{
	struct yt_sector sector;
	float old_fighter_owner;
	int basic;

	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	old_fighter_owner = sector.fighter_owner;
	if (sector.fighters > 0.0f) {
		float destroyed = 0.0f;
		bool friendly;
		char owner[64];
		char sector_text[64];
		char fighter_text[64];
		char row[256];

		if (!projectile_fighter_owner(session, sector.fighter_owner,
		    true, owner, sizeof(owner), &friendly, error))
			return false;
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)sector.fighters);
		snprintf(row, sizeof(row), "Sector:%s defended by %s with%s fighters.",
		    sector_text, owner, fighter_text);
		yt_out_line(row);
		if (friendly)
			goto missile_mines;

		if (!session_sound(session, 2.0f,
		    "cruise missile fighter-defense sound", error))
			return false;
		while (*remaining > 0.0f && destroyed < sector.fighters) {
			float draw;

			if (!random_value(session, &draw, error))
				return false;
			destroyed = floorf(single_add(
			    single_mul(draw, 5000.0f), destroyed));
			*remaining = single_sub(*remaining, 1.0f);
		}
		if (destroyed > sector.fighters)
			destroyed = sector.fighters;
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)destroyed);
		snprintf(row, sizeof(row), "The Missiles destroyed%s fighters!",
		    fighter_text);
		yt_out_line(row);
		if (destroyed > 9.0f) {
			snprintf(row, sizeof(row), "%s's Missiles destroyed%s fighters "
			    "in sector%s!", session->player.name, fighter_text,
			    sector_text);
			if (!append_news(session, row, error))
				return false;
		}
		sector.fighters =
		    single_sub(sector.fighters, destroyed);
		if (sector.fighters == 0.0f)
			sector.fighter_owner = 0.0f;
		if (old_fighter_owner == -1.0f && sector.fighters > 0.0f
		    && session->player_record != -1)
			*xannor_provoker = session->player_record;
		if (!yt_game_write_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
		if (sector.fighters == 0.0f && old_fighter_owner == -1.0f
		    && (float)sector_number
		    == session->door->game.config.headquarters
		    && !xannor_victory(session, error))
			return false;
	}

missile_mines:
	if (sector.mines > 0.0f) {
		float destroyed = *remaining < sector.mines
		    ? *remaining : sector.mines;
		char mine_text[64];
		char sector_text[64];
		char row[256];
		const char *suffix = destroyed > 1.0f ? "s" : "";

		qb_str_double(mine_text, sizeof(mine_text),
		    (double)sector.mines);
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		snprintf(row, sizeof(row), "The missiles hit%s SECTOR MINES in "
		    "sector%s!", mine_text, sector_text);
		yt_out_line(row);

		if (!session_sound(session, 5.0f,
		    "cruise missile sector-mine sound", error))
			return false;
		if (*last_mine_news_sector != sector_number) {
			snprintf(row, sizeof(row), "%s's Missiles hit sector mines in "
			    "sector%s!", session->player.name, sector_text);
			if (!append_news(session, row, error))
				return false;
			*last_mine_news_sector = sector_number;
		}
		qb_str_single(mine_text, sizeof(mine_text), destroyed);
		snprintf(row, sizeof(row), "The missile%s destroyed%s mine%s!",
		    suffix, mine_text, suffix);
		yt_out_line(row);
		sector.mines = single_sub(sector.mines, destroyed);
		*remaining = single_sub(*remaining, destroyed);
		if (!yt_game_write_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
	}
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset
	    && *remaining > 0.0f; ++basic) {
		struct yt_player target;
		float fighter_damage = 0.0f;
		float shield_damage = 0.0f;
		char sector_text[64];
		char shield_text[64];
		char fighter_text[64];
		char row[256];

		if (basic == session->player_record)
			continue;
		if (session->sector_cache[basic] != (float)sector_number)
			continue;
		if (session->cloak_cache[basic] != 0.0f
		    && basic != *xannor_provoker)
			continue;
		if (*xannor_provoker != 0 && basic != *xannor_provoker)
			continue;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (target.killed_by != 0.0f || target.sector != (float)sector_number)
			continue;
		if (!session_sound(session, 2.0f,
		    "cruise missile player-attack sound", error))
			return false;
		while (*remaining > 0.0f) {
			float draw;

			*remaining = single_sub(*remaining, 1.0f);
			if (!random_value(session, &draw, error))
				return false;
			if (single_mul(draw, *remaining) > 100.0f
			    && target.danger_scanner != 0.0f)
				target.danger_scanner = 0.0f;
			if (!random_value(session, &draw, error))
				return false;
			fighter_damage = floorf(single_add(
			    single_mul(draw, 4001.0f), fighter_damage));
			if (!random_value(session, &draw, error))
				return false;
			if (single_mul(draw, target.fighters)
			    < fighter_damage) {
				if (!random_value(session, &draw, error))
					return false;
				shield_damage = single_add(shield_damage,
				    floorf(single_mul(draw, 1001.0f)));
			}
			if (fighter_damage >= target.fighters
			    && shield_damage >= target.shields)
				break;
		}
		if (fighter_damage > target.fighters)
			fighter_damage = target.fighters;
		if (shield_damage > target.shields)
			shield_damage = target.shields;
		target.fighters =
		    single_sub(target.fighters, fighter_damage);
		target.shields =
		    single_sub(target.shields, shield_damage);
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		qb_str_single(shield_text, sizeof(shield_text), target.shields);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)fighter_damage);
		snprintf(row, sizeof(row), "%s's missiles attacked %s in%s reducing",
		    session->player.name, target.name, sector_text);
		if (!append_news(session, row, error))
			return false;
		snprintf(row, sizeof(row), "The missiles attacked %s in%s reducing",
		    target.name, sector_text);
		yt_out_line(row);
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!append_news(session, row, error))
			return false;
		yt_out_line(row);
		if (target.shields < 1.0f) {
			float mines = target.mines;

			snprintf(row, sizeof(row), "%s was destroyed!", target.name);
			yt_out_line(row);
			if (mines != 0.0f) {
				snprintf(row, sizeof(row),
				    "*** WARNING, %s had sector mines!", target.name);
				yt_out_line(row);
			}
			target.mines = 0.0f;
			if (!yt_game_write_player(&session->door->game, basic,
			    &target, error))
				return false;
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number,
			    mines, error))
				return false;
			if (!kill_player(session, basic,
			    (float)session->player_record, error))
				return false;
			if (*counterattack == 0 && *xannor_provoker == 0) {
				if (!session_sound(session, 3.0f,
				    "cruise missile salvage sound", error)
				    || !salvage_player(session, basic, error))
					return false;
			}
		}
		else {
			if (!yt_game_write_player(&session->door->game, basic,
			    &target, error))
				return false;
			if (session->player_record != -1)
				*counterattack = basic;
		}
	}
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	return missile_planet_impact(session, sector_number, &sector,
	    remaining, error);
}

static bool
plasma_planet_impact(struct yt_session *session, int sector_number,
    struct yt_sector *sector, double *energy, struct yt_error *error)
{
	struct yt_planet planet;
	int logical_planet = (int)sector->planet;
	float original_ore;
	float old_total;
	float old_ground;
	char sector_text[64];
	char number_one[64];
	char number_two[64];
	char row[256];

	if (*energy <= 0.0 || sector->planet == 0.0f)
		return true;
	if (!planet_update(session, logical_planet, &planet, error))
		return false;
	qb_str_single(sector_text, sizeof(sector_text), (float)sector_number);
	snprintf(row, sizeof(row), "The plasma bolts hit planet %s in sector%s!",
	    planet.name, sector_text);
	yt_out_line(row);
	snprintf(row, sizeof(row), "%s's plasma bolts hit planet %s in sector%s!",
	    session->player.name, planet.name, sector_text);
	if (!append_news(session, row, error))
		return false;
	if (!session_sound(session, 2.0f,
	    "plasma planet attack sound", error))
		return false;
	original_ore = planet.production[0];
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
	if (planet.ground_forces < 1.0f) {
		planet.ground_forces = 0.0f;
		planet.owner = 0.0f;
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
		yt_out_line(row);
		if (!append_news(session, row, error))
			return false;
	}
	if (!yt_game_write_planet(&session->door->game, logical_planet,
	    &planet, error))
		return false;
	if (planet.production[0] == 0.0f
	    && planet.production[1] == 0.0f
	    && planet.production[2] == 0.0f) {
		planet.name_length = 0.0f;
		if (!yt_game_write_planet(&session->door->game, logical_planet,
		    &planet, error))
			return false;
		sector->planet = 0.0f;
		if (!yt_game_write_sector(&session->door->game, sector_number,
		    sector, error))
			return false;
		yt_out_line("The planet was destroyed!!");
		if (!session_sound(session, 3.0f,
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
		yt_out_line(row);
		if (!append_news(session, row, error))
			return false;
	}
	return true;
}

static bool
plasma_sector(struct yt_session *session, int sector_number,
    double *energy, struct yt_error *error)
{
	struct yt_sector sector;
	bool headquarters_wiped = false;
	int basic;

	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	if (sector.fighters > 0.0f) {
		float destroyed = 0.0f;
		bool ignored_friendly;
		char owner[64];
		char sector_text[64];
		char fighter_text[64];
		char row[256];

		if (!projectile_fighter_owner(session, sector.fighter_owner,
		    false, owner, sizeof(owner), &ignored_friendly, error))
			return false;
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)sector.fighters);
		snprintf(row, sizeof(row), "Sector:%s defended by %s with%s fighters.",
		    sector_text, owner, fighter_text);
		yt_out_line(row);

		if (!session_sound(session, 2.0f,
		    "plasma fighter-defense sound", error))
			return false;
		while (*energy > 0.0 && destroyed < sector.fighters) {
			float draw;

			destroyed = single_add(destroyed,
			    (float)floor(*energy / 5000.0) + 1.0f);
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (*energy < 0.0)
			*energy = 0.0;
		if (destroyed > sector.fighters)
			destroyed = sector.fighters;
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)destroyed);
		snprintf(row, sizeof(row), "The plasma bolts destroyed%s fighters!",
		    fighter_text);
		yt_out_line(row);
		if (destroyed > 9.0f) {
			snprintf(row, sizeof(row), "%s's plasma bolts destroyed%s "
			    "fighters in sector%s!", session->player.name,
			    fighter_text, sector_text);
			if (!append_news(session, row, error))
				return false;
		}
		sector.fighters =
		    single_sub(sector.fighters, destroyed);
		if (sector.fighters == 0.0f)
			sector.fighter_owner = 0.0f;
		headquarters_wiped = sector.fighters == 0.0f
		    && (float)sector_number
		    == session->door->game.config.headquarters;
	}
	if (*energy > 0.0 && sector.mines > 0.0f) {
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

			destroyed = single_add(destroyed,
			    (float)floor(*energy * 0.000001) + 1.0f);
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
		yt_out_line(row);
		sector.mines = single_sub(sector.mines, destroyed);
	}
	if (!yt_game_write_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	if (headquarters_wiped && !xannor_victory(session, error))
		return false;
	for (basic = YT_PLAYER_FIRST;
	    basic <= (int)session->door->game.config.sector_offset
	    && *energy > 0.0; ++basic) {
		struct yt_player target;
		float fighter_damage = 0.0f;
		float shield_damage = 0.0f;
		int saved_foreground;
		char sector_text[64];
		char shield_text[64];
		char fighter_text[64];
		char row[256];

		if (session->sector_cache[basic] != (float)sector_number)
			continue;
		if (!yt_game_read_player(&session->door->game, basic, &target,
		    error))
			return false;
		if (target.killed_by != 0.0f || target.sector != (float)sector_number)
			continue;
		saved_foreground = session->pager.foreground;
		session_set_color(session, 5);
		if (!session_sound(session, 2.0f,
		    "plasma player-attack sound", error))
			return false;
		while (*energy > 0.0 && fighter_damage < target.fighters) {
			float draw;

			fighter_damage = single_add(fighter_damage,
			    (float)floor(*energy / 5000.0) + 1.0f);
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (fighter_damage > target.fighters)
			fighter_damage = target.fighters;
		while (*energy > 0.0 && shield_damage < target.shields) {
			float draw;

			shield_damage = single_add(shield_damage,
			    (float)floor(*energy / 10000.0) + 1.0f);
			if (!random_value(session, &draw, error))
				return false;
			*energy -= (double)single_mul(draw, 25000.0f);
		}
		if (shield_damage > target.shields)
			shield_damage = target.shields;
		target.fighters =
		    single_sub(target.fighters, fighter_damage);
		target.shields =
		    single_sub(target.shields, shield_damage);
		qb_str_single(sector_text, sizeof(sector_text),
		    (float)sector_number);
		qb_str_single(shield_text, sizeof(shield_text), target.shields);
		qb_str_double(fighter_text, sizeof(fighter_text),
		    (double)fighter_damage);
		snprintf(row, sizeof(row), "%s's plasma bolts hit %s in%s reducing",
		    session->player.name, target.name, sector_text);
		if (!append_news(session, row, error))
			return false;
		snprintf(row, sizeof(row), "The plasma bolts hit %s in%s reducing",
		    target.name, sector_text);
		yt_out_line(row);
		snprintf(row, sizeof(row), "shields to%s units and destroying%s "
		    "fighters!", shield_text, fighter_text);
		if (!append_news(session, row, error))
			return false;
		yt_out_line(row);
		session_set_color(session, saved_foreground);
		if (target.shields < 1.0f) {
			float mines = target.mines;

			if (basic == session->player_record)
				yt_out_line("YOU were destroyed!");
			else {
				snprintf(row, sizeof(row), "%s was destroyed!", target.name);
				yt_out_line(row);
			}
			if (mines != 0.0f) {
				snprintf(row, sizeof(row),
				    "*** WARNING, %s had sector mines!", target.name);
				yt_out_line(row);
			}
			target.mines = 0.0f;
			target.danger_scanner = 0.0f;
			if (!yt_game_write_player(&session->door->game, basic,
			    &target, error))
				return false;
			if (mines != 0.0f
			    && !deploy_victim_mines(session, sector_number, mines,
			    error))
				return false;
			if (basic == session->player_record) {
				session->player = target;
				session->destroyed = true;
				session->sector_cache[basic] = 0.0f;
			}
			else {
				if (!kill_player(session, basic,
				    (float)session->player_record, error))
					return false;
				if (!session_sound(session, 3.0f,
				    "plasma salvage sound", error)
				    || !salvage_player(session, basic, error))
					return false;
			}
		}
		else if (!yt_game_write_player(&session->door->game, basic,
		    &target, error))
			return false;
	}
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	return plasma_planet_impact(session, sector_number, &sector, energy,
	    error);
}

static bool
projectile_opening(struct yt_session *session, float amount, double energy,
    bool plasma, struct yt_error *error)
{
	char number[64];
	char row[160];

	if (!plasma) {
		if (!session_sound(session, 4.0f,
		    "cruise missile launch sound", error))
			return false;
		yt_out_line("");
		yt_out("Loading course into misile targeting computer.");
		yt_out_line("");
		yt_out_line("*** Tracking Report ***");
		return true;
	}
	yt_out_line("");
	yt_out("Loading course into targeting computer.");
	yt_out_line("");
	if (!session_sound(session, 4.0f, "plasma launch sound", error))
		return false;
	session_timed_wait(session, 1.0);
	qb_str_double(number, sizeof(number), energy);
	snprintf(row, sizeof(row),
	    "Plasma bolts targeted... firing%s megawatts!", number);
	yt_out_line(row);
	yt_out_line("");
	session_timed_wait(session, 1.0);
	{
		float counter;

		for (counter = 1.0f; counter <= amount;
		    counter = single_add(counter, 1.0f)) {
			qb_str_single(number, sizeof(number), counter);
			snprintf(row, sizeof(row), "Firing%s!", number);
			yt_out_line(row);
			if (!session_sound(session, 7.0f,
			    "plasma bolt firing sound", error))
				return false;
		}
	}
	yt_out_line("");
	yt_out_line("* Tracking Report *");
	yt_out_line("");
	return true;
}

static void
route_failure_report(void)
{
	yt_out_line("");
	yt_out_line("");
	yt_out_line(
	    "*** You can't get there without going someplace you dont want to!");
}

static void
plasma_footer(void)
{
	yt_out_line("");
	yt_out_line("Plasma bolts dissipated.");
	yt_out_line("");
}

static void
plasma_hop_report(struct yt_session *session, int sector_number,
    double energy)
{
	char sector_text[64];
	char energy_text[64];
	char row[192];

	qb_str_single(sector_text, sizeof(sector_text), (float)sector_number);
	qb_str_double(energy_text, sizeof(energy_text), floor(energy));
	snprintf(row, sizeof(row), "Bolt entering sector%s.%s Megawatts remaining.",
	    sector_text, energy_text);
	yt_out_line(row);
	session_timed_wait(session, 0.5);
}

static bool
launch_projectile(struct yt_session *session, float target, float amount,
    bool plasma, float *returned_missiles, float *origin_alias,
    int *pending_counterattack, int *pending_xannor,
    struct yt_error *error)
{
	int count = sector_count(session);
	int destination;
	bool overflow;
	int *route;
	bool found;
	int cursor;
	float local_missiles = amount;
	float *missiles = returned_missiles != NULL
	    ? returned_missiles : &local_missiles;
	double energy = 2500000.0 * (double)amount;
	double hop_loss = energy / 50.0;
	int counterattack = pending_counterattack != NULL
	    ? *pending_counterattack : 0;
	int xannor_provoker = pending_xannor != NULL
	    ? *pending_xannor : 0;
	int last_mine_news_sector = 0;
	int start = (int)(origin_alias != NULL
	    ? *origin_alias : session->player.sector);

	*missiles = amount;

	destination = (int)qb_cint(target, &overflow);
	if (overflow)
		return true;
	if (!projectile_opening(session, amount, energy, plasma, error))
		return false;
	route = calloc((size_t)count + 1U, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (start == destination) {
		if (plasma) {
			plasma_hop_report(session, start, energy);
			found = plasma_sector(session, start, &energy, error);
			if (found)
				plasma_footer();
			free(route);
			return found;
		}
		if (counterattack == 0 && session->player_record != -1) {
			yt_out_line("");
			yt_out_line("Missles self destructed!");
			free(route);
			return true;
		}
		yt_out_line("*** End of Report ***");
		free(route);
		return true;
	}
	for (;;) {
		bool rerouted = false;

		if (!build_route(session, start, destination, route,
		    !plasma && counterattack == 0
		    && session->player_record != -1, &found, error)) {
			free(route);
			return false;
		}
		if (!found) {
			route_failure_report();
			if (plasma)
				plasma_footer();
			else {
				yt_out_line("");
				yt_out_line("Missles self destructed!");
			}
			free(route);
			return true;
		}
		cursor = start;
		while (route[cursor] != 0
		    && (plasma ? energy >= 1.0 : *missiles > 0.0f)) {
		int next = route[cursor];

		if (plasma) {
			energy -= hop_loss;
			if (energy < 1.0)
				break;
			plasma_hop_report(session, next, energy);
		}
		if ((float)next == session->black_hole[0]
		    || (float)next == session->black_hole[1]) {
			float draw;
			char old_text[64];
			char new_text[64];
			char row[192];

			if (!random_value(session, &draw, error)) {
				free(route);
				return false;
			}
			start = next;
			if (origin_alias != NULL)
				*origin_alias = (float)next;
			destination = 1
			    + (int)floorf(single_mul(draw, (float)count));
			qb_str_single(old_text, sizeof(old_text), (float)next);
			qb_str_single(new_text, sizeof(new_text),
			    (float)destination);
			yt_out_line("");
			if (plasma)
				snprintf(row, sizeof(row), "The plasma bolt is "
				    "deflected by a black hole in sector%s to "
				    "sector%s!", old_text, new_text);
			else
				snprintf(row, sizeof(row), "The missiles are "
				    "deflected by a black hole in sector%s!",
				    old_text);
			if (!session_attention(session, row,
			    plasma ? "plasma black-hole attention"
			    : "missile black-hole attention", error)) {
				free(route);
				return false;
			}
			rerouted = true;
			break;
		}
		if (!plasma && next < 8 && destination < 8
		    && counterattack == 0 && xannor_provoker == 0) {
			yt_out_line("The Union Police have destroyed the Missiles!");
			free(route);
			return true;
		}
		if (plasma) {
			if (!plasma_sector(session, next, &energy, error)) {
				free(route);
				return false;
			}
		}
		else if (!missile_sector(session, next, missiles,
		    &counterattack, &xannor_provoker, &last_mine_news_sector,
		    error)) {
			free(route);
			return false;
		}
		cursor = next;
		}
		if (!rerouted)
			break;
	}
	free(route);
	if (pending_counterattack != NULL)
		*pending_counterattack = counterattack;
	if (pending_xannor != NULL)
		*pending_xannor = xannor_provoker;
	if (plasma)
		plasma_footer();
	else if (*missiles > 0.0f)
		yt_out_line("*** End of Report ***");
	return true;
}

static bool
session_random_integer(struct yt_session *session, int range, int *value,
    struct yt_error *error)
{
	float draw;

	if (range < 1) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	if (!random_value(session, &draw, error))
		return false;
	*value = (int)floorf(single_mul(draw, (float)range)) + 1;
	return true;
}

static bool
session_nested_integer(struct yt_session *session, int count, int range,
    int *value, struct yt_error *error)
{
	int index;
	int current = range;

	if (count < 1 || range < 1) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	for (index = 0; index < count; ++index) {
		if (!session_random_integer(session, current, &current, error))
			return false;
	}
	*value = current;
	return true;
}

static bool
launch_xannor_retaliation(struct yt_session *session, int provoking_player,
    struct yt_error *error)
{
	struct yt_player saved_player = session->player;
	struct yt_sector headquarters;
	int saved_record = session->player_record;
	float saved_cloak = 0.0f;
	int target;
	int amount;
	int ignored_counterattack = 0;
	int ignored_xannor = provoking_player;
	bool result;
	char amount_text[64];
	char target_text[64];
	char row[192];

	if (provoking_player == 0 && session->player.score < 25000000.0f)
		return true;
	if (!yt_game_read_sector(&session->door->game,
	    (int)session->door->game.config.headquarters, &headquarters,
	    error))
		return false;
	if (headquarters.fighters == 0.0f
	    || headquarters.fighter_owner != -1.0f)
		return true;
	if (!session_nested_integer(session, 3, 100, &amount, error)
	    || !session_random_integer(session, sector_count(session), &target,
	    error))
		return false;
	yt_out_line("");
	if (saved_record >= 0 && saved_record <= YT_PLAYER_LAST) {
		saved_cloak = session->cloak_cache[saved_record];
		if (provoking_player != 0)
			session->cloak_cache[saved_record] = 0.0f;
	}
	if (provoking_player != 0)
		target = (int)saved_player.sector;

	session->player_record = -1;
	snprintf(session->player.name, sizeof(session->player.name),
	    "%s", "The Xannor");
	session->player.sector = session->door->game.config.headquarters;
	qb_str_single(amount_text, sizeof(amount_text), (float)amount);
	qb_str_single(target_text, sizeof(target_text), (float)target);
	snprintf(row, sizeof(row), "The Xannor have launched%s missiles at "
	    "sector%s!", amount_text, target_text);
	yt_out_line(row);
	result = launch_projectile(session, (float)target, (float)amount,
	    false, NULL, &session->door->game.config.headquarters,
	    &ignored_counterattack, &ignored_xannor, error);
	session->player_record = saved_record;
	session->player = saved_player;
	if (saved_record >= 0 && saved_record <= YT_PLAYER_LAST)
		session->cloak_cache[saved_record] = saved_cloak;
	if (!result || !reload_player(session, error))
		return false;
	if (session->player.killed_by != 0.0f) {
		session->destroyed = true;
		session->sector_cache[session->player_record] = 0.0f;
	}
	session_timed_wait(session, 4.0);
	return true;
}

static bool
launch_player_counterattack(struct yt_session *session, int counterattacker,
    struct yt_error *error)
{
	struct yt_player saved_player = session->player;
	struct yt_player attacker;
	int saved_record = session->player_record;
	float saved_cloak = 0.0f;
	float amount;
	int target = (int)saved_player.sector;
	int ignored_counterattack = 0;
	int xannor_provoker = 0;
	bool result;
	bool overflow;
	int name_length;
	char attacker_name[42];
	char amount_text[64];
	char row[256];

	if (counterattacker < YT_PLAYER_FIRST
	    || counterattacker > YT_PLAYER_LAST
	    || counterattacker == saved_record)
		return true;
	if (!yt_game_read_player(&session->door->game, counterattacker,
	    &attacker, error))
		return false;
	if (attacker.killed_by != 0.0f || attacker.missiles <= 0.0f)
		return true;
	name_length = (int)qb_cint(attacker.name_length, &overflow);
	if (overflow || name_length < 0)
		name_length = 0;
	if ((size_t)name_length > strlen(attacker.name))
		name_length = (int)strlen(attacker.name);
	snprintf(attacker_name, sizeof(attacker_name), "%.*s", name_length,
	    attacker.name);
	if (saved_record >= 0 && saved_record <= YT_PLAYER_LAST) {
		saved_cloak = session->cloak_cache[saved_record];
		session->cloak_cache[saved_record] = 0.0f;
	}
	session->player_record = counterattacker;
	session->player = attacker;
	snprintf(session->player.name, sizeof(session->player.name), "%s",
	    attacker_name);
	if (saved_player.score > 0.0f)
		session->counterlaunch_count = (float)(floor(
		    (double)saved_player.score * 0.00001) + 1.0);
	amount = session->counterlaunch_count;
	if (amount > attacker.missiles || amount == 0.0f) {
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		amount = single_add(floorf(single_mul(draw, attacker.missiles)),
		    1.0f);
		session->counterlaunch_count = amount;
	}
	attacker.missiles = single_sub(attacker.missiles, amount);
	if (!yt_game_write_player(&session->door->game, counterattacker,
	    &attacker, error))
		return false;

	qb_str_single(amount_text, sizeof(amount_text), amount);
	yt_out_line("");
	snprintf(row, sizeof(row), "%s shot back with%s missiles at you!",
	    attacker_name, amount_text);
	yt_out_line(row);
	snprintf(row, sizeof(row), "%s shot back with%s missiles at %s!",
	    attacker_name, amount_text, saved_player.name);
	if (!append_news(session, row, error))
		return false;
	result = launch_projectile(session, (float)target, amount,
	    false, &session->counterlaunch_count, NULL,
	    &ignored_counterattack, &xannor_provoker, error);
	session->player_record = saved_record;
	session->player = saved_player;
	if (saved_record >= 0 && saved_record <= YT_PLAYER_LAST)
		session->cloak_cache[saved_record] = saved_cloak;
	if (!result || !reload_player(session, error))
		return false;
	if (session->player.killed_by != 0.0f) {
		session->destroyed = true;
		session->sector_cache[session->player_record] = 0.0f;
	}
	session_timed_wait(session, 4.0);
	if (xannor_provoker != 0
	    && !launch_xannor_retaliation(session, xannor_provoker, error))
		return false;
	return true;
}

static bool
command_projectile(struct yt_session *session, bool plasma,
    struct yt_error *error)
{
	float available;
	double target_value;
	double amount_value;
	bool blank;
	float target;
	float amount;
	int counterattack = 0;
	int xannor_provoker = 0;
	bool denied;

	if (!fresh_no_turn_gate(session, &denied, error))
		return false;
	if (denied)
		return true;
	available = plasma ? session->player.plasma : session->player.missiles;
	if (available < 1.0f) {
		yt_out_line(plasma ? "You have no plasma bolts."
		    : "You have no cruise missiles.");
		return true;
	}
	if (!session_value(session, "Target sector? ", &target_value, &blank))
		return false;
	target = (float)target_value;
	if (blank || target < 1.0f
	    || target > (float)sector_count(session))
		return true;
	if (!session_value(session, "How many? ", &amount_value, &blank))
		return false;
	amount = (float)floor(amount_value);
	if (blank || amount < 1.0f)
		return true;
	if (amount > available) {
		yt_out_line("You don't have that many.");
		return true;
	}
	if (!finalize_action(session, 1.0f, error))
		return error == NULL || error->status == YT_OK;
	if (plasma)
		session->player.plasma =
		    single_sub(session->player.plasma, amount);
	else
		session->player.missiles =
		    single_sub(session->player.missiles, amount);
	if (!write_player(session, error))
		return false;
	if (!launch_projectile(session, target, amount, plasma, NULL, NULL,
	    &counterattack, &xannor_provoker, error)
	    || !launch_player_counterattack(session, counterattack, error)
	    || !launch_xannor_retaliation(session, xannor_provoker, error))
		return false;
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
		bool accepted;
		char prompt[160];

		if (!yt_game_read_player(&session->door->game, basic, &player,
		    error))
			return false;
		if (player.name_length == 0.0f
		    || strstr(player.name, query) == NULL)
			continue;
		snprintf(prompt, sizeof(prompt), "%s [Y]? ", player.name);
		if (!session_yes_no(session, prompt, true, &accepted))
			return false;
		if (accepted) {
			*selected = basic;
			return true;
		}
	}
	yt_out_line("Not found.");
	return true;
}

static void
radio_compatibility_upper(char *text)
{
	uint8_t *cursor = (uint8_t *)text;

	while (*cursor != 0) {
		if (*cursor > (uint8_t)'@')
			*cursor &= UINT8_C(0xdf);
		++cursor;
	}
}

static void
radio_line_prompt(int line_number, const char *text)
{
	yt_outf(" %d:%s", line_number, text);
}

static bool
radio_edit_draft(struct yt_session *session, char lines[21][76],
    int completed)
{
	char response[80];
	struct qb_val_result parsed;
	int selected;

	yt_outf("Edit Which line? (1 - %d) -=> ", completed);
	if (!session_line(session, response, sizeof(response)))
		return false;
	radio_compatibility_upper(response);
	if (strchr(response, 'E') != NULL || response[0] == '\0')
		return true;
	parsed = qb_val(response);
	if (!parsed.valid || !isfinite(parsed.value)
	    || floor(parsed.value) < (double)INT_MIN
	    || floor(parsed.value) > (double)INT_MAX)
		selected = 0;
	else
		selected = (int)floor(parsed.value);
	if (selected < 1 || selected > completed) {
		yt_out_line("INVALID LINE NUMBER!");
		return true;
	}

	for (;;) {
		char search[76];
		char replacement[76];
		char changed[152];
		char *match;
		size_t prefix;

		yt_outf("Line %d reads:\r\n%s\r\n", selected,
		    lines[selected - 1]);
		yt_out("Replace what section? -=> ");
		if (!session_line(session, search, sizeof(search)))
			return false;
		if (search[0] == '\0')
			return true;
		match = strstr(lines[selected - 1], search);
		if (match == NULL) {
			yt_outf("%s NOT FOUND in line %d\r\n", search, selected);
			continue;
		}
		yt_out("Replace it with what? -=> ");
		if (!session_line(session, replacement, sizeof(replacement)))
			return false;
		prefix = (size_t)(match - lines[selected - 1]);
		snprintf(changed, sizeof(changed), "%.*s%s%s", (int)prefix,
		    lines[selected - 1], replacement, match + strlen(search));
		changed[74] = '\0';

		for (;;) {
			yt_outf("Line %d now reads:\r\n%s\r\n", selected, changed);
			yt_out("Is this OK? [Y/N]? -=> ");
			if (!session_line(session, response, sizeof(response)))
				return false;
			radio_compatibility_upper(response);
			if (response[0] == 'Y') {
				snprintf(lines[selected - 1], 76, "%s", changed);
				yt_out_line("Change Saved!");
				return true;
			}
			if (response[0] == 'N') {
				yt_out_line("CANCELED!");
				return true;
			}
		}
	}
}

static bool
radio_compose(struct yt_session *session, struct yt_error *error)
{
	char target[160];
	float recipients[4] = {0};
	int recipient_count = 0;
	char lines[21][76] = {{0}};
	int line_count = 0;
	size_t wrap_marker = 0;
	bool all = false;
	bool send = false;
	int index;

	yt_out_line("Warming up sub-space radio.");
	yt_out("Send a message to who? (search string) or 'ALL' or 'TEAM'? ");
	if (!session_line(session, target, sizeof(target)))
		return false;
	if (target[0] == '\0')
		return true;
	qb_title_case(target);
	if (strcmp(target, "All") == 0) {
		recipients[0] = -2.0f;
		recipient_count = 1;
		all = true;
		yt_out_line("This will be a broadcast message to ALL players");
	}
	else if (strcmp(target, "Team") == 0) {
		struct yt_team team;

		if (session->player.team == 0.0f) {
			yt_out_line("You Don't belong to a team!");
			return true;
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
	for (index = 0; index < recipient_count; ++index) {
		char name[80];

		if (all || recipients[index] == 0.0f)
			continue;
		if (!radio_name(session, recipients[index], name, sizeof(name),
		    false, error))
			return false;
		yt_outf("Tuning in to %s's frequency.\r\n", name);
	}
	yt_out_line(
	    "   Due to the distances involved, messages are limited to 20 lines.");

	while (!send) {
		bool menu = false;

		if (line_count >= 20) {
			yt_out_line("Message full!");
			menu = true;
		}
		else {
			radio_line_prompt(line_count + 1, lines[line_count]);
			while (!menu) {
				int key = session_input_key(session);
				size_t length;

				if (key == EOF)
					return false;
				length = strlen(lines[line_count]);
				if (key == '\r' || key == '\n') {
					yt_out("\r\n");
					if (length == 0) {
						if (line_count == 0)
							return true;
						menu = true;
					}
					else {
						++line_count;
						wrap_marker = 0;
						if (line_count >= 20) {
							yt_out_line("Message full!");
							menu = true;
						}
						else
							radio_line_prompt(line_count + 1,
							    lines[line_count]);
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
				yt_outf("%c", key);
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
					yt_out("\r\n");
					if (line_count >= 20) {
						yt_out_line("Message full!");
						menu = true;
					}
					else
						radio_line_prompt(line_count + 1,
						    lines[line_count]);
				}
			}
		}

		while (menu && !send) {
			char choice[80];
			bool abort;

			yt_out(
			    "[L] List [S] Send [A] Abort [C] Continue [E] Edit -=> ");
			if (!session_line(session, choice, sizeof(choice)))
				return false;
			radio_compatibility_upper(choice);
			if (strcmp(choice, "L") == 0) {
				for (index = 0; index < line_count; ++index) {
					radio_line_prompt(index + 1, lines[index]);
					yt_out("\r\n");
				}
			}
			else if (strcmp(choice, "A") == 0) {
				if (!session_yes_no(session,
				    "Are you sure? [y/N]", false, &abort))
					return false;
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
				if (!radio_edit_draft(session, lines, line_count))
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
	yt_out_line("Transmission successful!");
	return true;
}

static bool
computer_route(struct yt_session *session, bool autopilot,
    struct yt_error *error)
{
	double start_value = session->player.sector;
	double destination_value;
	bool blank;
	int start;
	int destination;
	int count = sector_count(session);
	int *route;
	bool found;
	int cursor;
	int hops = 0;

	if (!autopilot
	    && !session_value(session, "Start sector? ", &start_value,
	    &blank))
		return false;
	if (!session_value(session, "Destination sector? ",
	    &destination_value, &blank))
		return false;
	start = (int)floor(start_value);
	destination = (int)floor(destination_value);
	if (blank || start < 1 || start > count
	    || destination < 1 || destination > count)
		return true;
	route = calloc((size_t)count + 1U, sizeof(*route));
	if (route == NULL) {
		if (error != NULL)
			error->status = YT_NO_MEMORY;
		return false;
	}
	if (!build_route(session, start, destination, route, true, &found,
	    error)) {
		free(route);
		return false;
	}
	if (!found) {
		free(route);
		yt_out_line("*** You can't get there without going someplace"
		    " you dont want to!");
		return true;
	}
	cursor = start;
	yt_outf("Course: %d", start);
	while (route[cursor] != 0) {
		cursor = route[cursor];
		++hops;
		yt_outf(" -> %d", cursor);
	}
	yt_out("\r\n");
	if (autopilot) {
		bool accepted;
		char commands[YT_COMMAND_SIZE] = "";
		size_t used = 0;

		if ((float)hops > session->player.turns) {
			free(route);
			yt_out_line("You don't have enough turns.");
			return true;
		}
		if (!session_yes_no(session, "Engage autopilot? [y/N] ", false,
		    &accepted)) {
			free(route);
			return false;
		}
		if (!accepted) {
			free(route);
			return true;
		}
		cursor = start;
		while (route[cursor] != 0) {
			int written;

			cursor = route[cursor];
			written = snprintf(commands + used,
			    sizeof(commands) - used, "%sM;%d",
			    used == 0 ? "" : ";", cursor);
			if (written < 0 || (size_t)written
			    >= sizeof(commands) - used) {
				free(route);
				return false;
			}
			used += (size_t)written;
		}
		if (!queue_remainder(session, commands)) {
			free(route);
			return false;
		}
	}
	free(route);
	return true;
}

static bool
computer_planet_report(struct yt_session *session, struct yt_error *error)
{
	double value;
	bool blank;
	int sector_number;
	struct yt_sector sector;

	if (session->player.turns < 1.0f) {
		yt_out_line("Sorry but you have no turns left.");
		return true;
	}
	if (!session_value(session, "Report on which sector? ", &value,
	    &blank))
		return false;
	sector_number = (int)floor(value);
	if (blank || sector_number == 0)
		return true;
	if (sector_number > sector_count(session)) {
		yt_outf("Valid sector numbers are from 1 to %.9g.\r\n",
		    (double)sector_count(session));
		return true;
	}
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	if (sector.planet == 0.0f) {
		yt_out_line("No information available.");
		return finalize_action(session, 1.0f, error)
		    || error == NULL || error->status == YT_OK;
	}
	return planet_inventory(session, (int)sector.planet, error);
}

static bool
computer_owned_fighters(struct yt_session *session, struct yt_error *error)
{
	int sector_number;
	bool found = false;

	yt_out_line("Searching;");
	for (sector_number = 1; sector_number <= sector_count(session);
	    ++sector_number) {
		struct yt_sector sector;

		if (!yt_game_read_sector(&session->door->game, sector_number,
		    &sector, error))
			return false;
		if (sector.fighters > 0.0f
		    && sector.fighter_owner == (float)session->player_record) {
			if (!found)
				yt_out_line(" Sector          Amount\r\n--------*--------");
			yt_outf("%8d %15.9g\r\n", sector_number,
			    (double)sector.fighters);
			found = true;
		}
	}
	if (!found)
		yt_out_line(" NONE found!");
	return true;
}

static bool
computer_owned_planets(struct yt_session *session, struct yt_error *error)
{
	int sector_number;
	bool found = false;

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
			yt_outf("Planet: %-41s Sector: %.9g\r\n", planet.name,
			    (double)sector_number);
			found = true;
		}
	}
	if (!found)
		yt_out_line("None found!");
	return true;
}

static bool
computer_port_report(struct yt_session *session, struct yt_error *error)
{
	double input;
	bool blank;
	int sector_number;
	struct yt_sector sector;
	struct yt_port port;
	float prices[3];
	double quantities[3];

	if (!session_value(session, "Port report for which sector? ", &input,
	    &blank))
		return false;
	sector_number = (int)floor(input);
	if (blank || sector_number < 1
	    || sector_number > sector_count(session))
		return true;
	if (!yt_game_read_sector(&session->door->game, sector_number, &sector,
	    error))
		return false;
	if (sector.port <= 0.0f) {
		yt_out_line("No port information available.");
		return true;
	}
	if (!port_update(session, (int)sector.port, &port, prices, quantities,
	    error))
		return false;
	return port_report(session, (int)sector.port, &port, prices,
	    quantities, error);
}

static bool
computer_avoid(struct yt_session *session)
{
	for (;;) {
		double slot_value;
		double sector_value;
		bool blank;
		int slot;

		yt_out_line("Avoided sectors:");
		for (slot = 0; slot < 30; ++slot) {
			if (session->avoid[slot] != 0.0f)
				yt_outf("%d) %.9g\r\n", slot + 1,
				    (double)session->avoid[slot]);
		}
		if (!session_value(session, "Change which slot (0 exits)? ",
		    &slot_value, &blank))
			return false;
		slot = (int)floor(slot_value);
		if (blank || slot < 1 || slot > 30)
			return true;
		if (!session_value(session, "Sector (0 clears)? ",
		    &sector_value, &blank))
			return false;
		session->avoid[slot - 1] = (float)sector_value;
	}
}

static bool
computer_spies(struct yt_session *session)
{
	int index;

	if (session->spy_count == 0) {
		yt_out_line("You do not have any spies!");
		return true;
	}
	for (index = 0; index < session->spy_count; ++index)
		yt_outf("Spy # %d will hunt in sector %d.\r\n", index + 1,
		    session->spies[index]);
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
	const char *alphabet = "123ATYEU";
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

	yt_out_line("Show buying/selling [1] Equ, [2] Org, [3] Ore,");
	yt_out_line("[Y] Your Ports, [T] Team's Ports, [E] Enemy Ports");
	yt_out("[U] Un-owned Ports OR [A] All Ports ? -=> [A] ");
	if (!session_line(session, response, sizeof(response)))
		return false;
	qb_compat_upper(response);
	if (response[0] == '\0')
		strcpy(response, "A");
	match = strstr(alphabet, response);
	if (match == NULL)
		return true;
	selector = (int)(match - alphabet) + 1;
	filter = selector;
	if (selector == 5 && session->player.team == 0.0f) {
		yt_out_line("You dont belong to a team!");
		return true;
	}
	if (selector == 6 && session->player.ports_owned == 0.0f) {
		yt_out_line("You dont own any!");
		return true;
	}
	if (selector >= 1 && selector <= 3) {
		yt_out("Find ports [B] Buying or [S] Selling  -=> ");
		if (!session_line(session, response, sizeof(response)))
			return false;
		qb_compat_upper(response);
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
					    == session->team_roster_cache[index])
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
profit_emit_row(struct yt_session *session, int source_number,
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
	qb_str_single(number, sizeof(number), (float)source_number);
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
	int source_start = all ? 2 : (int)session->player.sector;
	int source_end = all ? sector_count(session)
	    : (int)session->player.sector;
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
		project_port_market(session, &source_port, source_price, NULL);
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
			project_port_market(session, &target_port, target_price, NULL);
			if (!profit_emit_row(session, source_number, target_number,
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
computer_menu(struct yt_session *session, struct yt_error *error)
{
	for (;;) {
		char command[80];

		yt_out("Computer (?=help): ");
		if (!session_line(session, command, sizeof(command)))
			return false;
		qb_compat_upper(command);
		command[2] = '\0';
		if (command[0] == '\0')
			strcpy(command, "?");
		if (strcmp(command, "?") == 0) {
			yt_out_line("1 Off 2 Port 3 Autopilot 4 Scoreboard 5 Radio");
			yt_out_line("6 Radio Log 7 Avoid 8 News 9 Planet Report");
			yt_out_line("10 Path 11 Fighters 12 Ports 13 Planets");
			yt_out_line("14 Nearest 15 Spies 16/17 Profit");
		}
		else if (strcmp(command, "1") == 0) {
			yt_out_line("<Computer deactivated>");
			return true;
		}
		else if (strcmp(command, "2") == 0) {
			if (!computer_port_report(session, error))
				return false;
		}
		else if (strcmp(command, "3") == 0) {
			if (!computer_route(session, true, error))
				return false;
			return true;
		}
		else if (strcmp(command, "4") == 0) {
			char choice[80];

			yt_out("[O]ld or [U]pdated scoreboard? ");
			if (!session_line(session, choice, sizeof(choice)))
				return false;
			qb_compat_upper(choice);
			if (choice[0] != 'O'
			    && !yt_score_generate(&session->door->game, error))
				return false;
			if (!display_game_file(session,
			    session->door->game.config.scoreboard,
			    error))
				return false;
		}
		else if (strcmp(command, "5") == 0) {
			if (!radio_compose(session, error))
				return false;
		}
		else if (strcmp(command, "6") == 0) {
			if (!radio_read(session, true, error))
				return false;
		}
		else if (strcmp(command, "7") == 0) {
			if (!computer_avoid(session))
				return false;
		}
		else if (strcmp(command, "8") == 0) {
			char choice[80];

			for (;;) {
				yt_out("[T]oday or [Y]esterday? ");
				if (!session_line(session, choice,
				    sizeof(choice)))
					return false;
				qb_compat_upper(choice);
				if (strcmp(choice, "T") == 0
				    || strcmp(choice, "Y") == 0)
					break;
			}
			if (!display_game_file(session, choice[0] == 'T'
			    ? "YTNEWS.DAT" : "YTYNEWS.DAT", error))
				return false;
		}
		else if (strcmp(command, "9") == 0) {
			if (!computer_planet_report(session, error))
				return false;
		}
		else if (strcmp(command, "10") == 0) {
			if (!computer_route(session, false, error))
				return false;
		}
		else if (strcmp(command, "11") == 0) {
			if (!computer_owned_fighters(session, error))
				return false;
		}
		else if (strcmp(command, "12") == 0) {
			if (!command_collect(session, false, error))
				return false;
		}
		else if (strcmp(command, "13") == 0) {
			if (!computer_owned_planets(session, error))
				return false;
		}
		else if (strcmp(command, "14") == 0) {
			if (!computer_nearest_ports(session, error))
				return false;
		}
		else if (strcmp(command, "15") == 0)
			(void)computer_spies(session);
		else if (strcmp(command, "16") == 0) {
			if (!computer_profit(session, true, error))
				return false;
		}
		else if (strcmp(command, "17") == 0) {
			if (!computer_profit(session, false, error))
				return false;
		}
		else if (strcmp(command, "I") == 0) {
			if (!show_ship(session, error))
				return false;
		}
		else if (strcmp(command, "S") == 0) {
			if (!display_sector(session, true, error))
				return false;
		}
		else if (strcmp(command, "Q") == 0) {
			bool quit;

			if (!session_yes_no(session, "Quit? [y/N] ", false,
			    &quit))
				return false;
			if (quit) {
				session->running = false;
				return true;
			}
		}
		else if (strcmp(command, "!") == 0) {
			if (!command_collect(session, true, error))
				return false;
		}
		else if (strcmp(command, "+") == 0) {
			if (!command_projectile(session, true, error))
				return false;
		}
		else if (strcmp(command, "L") == 0) {
			if (!command_land(session, error))
				return false;
		}
		else if (strcmp(command, "M") == 0) {
			if (!command_move(session, error))
				return false;
		}
		else if (strcmp(command, "P") == 0) {
			if (!command_trade(session, error))
				return false;
		}
		else if (strcmp(command, "C") == 0)
			yt_out_line("Don't BREAK the 'ON' button!");
		else
			yt_out_line("Does not compute");
		if (!session->running)
			return true;
	}
}

static void
show_help(void)
{
	static const char *const lines[] = {
		"[ENTER] - Re-display sector",
		"$ - Take Credits from your ports",
		"! - Launch a Cruise Missile",
		"A - <A>ttack a player's ship",
		"B - <B>uy a Port",
		"C - Ship's <C>omputer",
		"D - <D>rop a Sector mine",
		"F - Take or leave <F>ighters",
		"G - Initiate <G>enesis",
		"I - <I>nfo on your ship",
		"L - <L>and on or create a planet",
		"M - <M>ove to another sector",
		"N - Re<N>ame Port",
		"P - Dock at a <P>ort (and trade)",
		"Q - <Q>uit game",
		"S - <S>ensors",
		"T - <T>eam menu",
		"V - <V>ersion Info",
		"W - Emergency <W>arp",
		"X - Sound Effects On/Off",
		"Z - Instructions",
		"+ - Fire Plasma Bolt",
		"String commands by seperating them with a semicolons (;).",
		"To place an EXTRA 'hit enter' in a string, use an extra ';'.",
		"Save a command string by placing a '/' at the end.",
		"Then hit Control-R to [R]eplay the saved command.",
		"You may repeat any command up to 20 times by putting",
		"a /R# at the end of your command. Replace the '#' with",
		"any number between 2 and 20. Example: your command/R20"
	};
	size_t index;

	for (index = 0; index < sizeof(lines) / sizeof(lines[0]); ++index)
		yt_out_line(lines[index]);
}

static void
format_time_left(const struct yt_session *session, char *dest, size_t size)
{
	float minutes = single_sub(single_div(session->session_deadline, 60.0f),
	    single_div((float)yt_platform_timer(), 60.0f));
	float whole = floorf(minutes);
	int seconds = (int)floorf(single_mul(
	    single_sub(minutes, whole), 60.0f));
	char rendered[64];

	qb_str_double(rendered, sizeof(rendered), (double)whole);
	snprintf(dest, size, "%s:%02d  ", rendered, seconds);
}

static bool
quit_session(struct yt_session *session, struct yt_error *error)
{
	if (!session->door->game_open)
		return true;
	if (!reload_player(session, error))
		return false;
	if (!show_ship(session, error))
		return false;
	yt_out_line("Generating ScoreBoard");
	if (!yt_score_generate(&session->door->game, error))
		return false;
	if (!display_game_file(session,
	    session->door->game.config.scoreboard, error))
		return false;
	if (!session->registered)
		yt_out_line("PLEASE HELP YOUR SYSOP REGISTER THIS GAME.");
	yt_outf("Returning to %s...\r\n", session->door->identity.system);
	return true;
}

static bool
command_shell(struct yt_session *session, struct yt_error *error)
{
	while (session->running && !session->destroyed) {
		char command[YT_COMMAND_SIZE];
		char upper[YT_COMMAND_SIZE];
		char time_left[64];
		char key;
		const char *dispatch = "W)+ABCFLMPQTD$GN";
		const char *position;
		bool enter_sector = false;

		session->pager.line_count = 0.0f;
		format_time_left(session, time_left, sizeof(time_left));
		yt_outf("Time:%sMain Command (?=Help)? ", time_left);
		if (!session_line(session, command, sizeof(command)))
			return true;
		snprintf(upper, sizeof(upper), "%s", command);
		qb_compat_upper(upper);
		if (upper[0] == '\0') {
			yt_out_line("");
			yt_out_line("<Display>");
			if (!display_sector(session, false, error))
				return false;
			continue;
		}
		if (strcmp(upper, "X") == 0) {
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
		if (strcmp(upper, "S") == 0) {
			if (!display_sector(session, true, error))
				return false;
			continue;
		}
		if (strcmp(upper, "V") == 0) {
			if (!registration(session, error))
				return false;
			if (!session->running)
				return true;
			continue;
		}
		if (strcmp(upper, "I") == 0) {
			if (!reload_player(session, error))
				return false;
			if (!show_ship(session, error))
				return false;
			continue;
		}
		if (strcmp(upper, "Z") == 0) {
			yt_out_line("");
			yt_out_line("<Instructions>");
			if (!display_game_file(session, "YTINSTR.DOC", error))
				return false;
			continue;
		}
		if (strcmp(upper, "?") == 0) {
			show_help();
			continue;
		}
		key = upper[0];
		position = strchr(dispatch, key);
		if (position == NULL) {
			yt_out_line("Invalid command.");
			continue;
		}
		switch ((int)(position - dispatch)) {
		case 0:
			if (!emergency_warp(session, error))
				return false;
			enter_sector = true;
			break;
		case 1:
			if (!command_projectile(session, false, error))
				return false;
			enter_sector = true;
			break;
		case 2:
			if (!command_projectile(session, true, error))
				return false;
			enter_sector = true;
			break;
		case 3:
			if (!command_attack_player(session, error))
				return false;
			enter_sector = true;
			break;
		case 4:
			if (!command_buy_port(session, error))
				return false;
			break;
		case 5:
			if (!session_sound(session, 4.0f,
			    "computer activation sound", error))
				return false;
			if (!computer_menu(session, error))
				return false;
			break;
		case 6:
			if (!command_fighters(session, error))
				return false;
			break;
		case 7:
			if (!command_land(session, error))
				return false;
			break;
		case 8:
			if (!command_move(session, error))
				return false;
			enter_sector = true;
			break;
		case 9:
			if (!command_trade(session, error))
				return false;
			break;
		case 10:
		{
			bool quit;

			yt_out_line("");
			yt_out_line("<Quit>");
			if (!session_yes_no(session,
			    "Are you sure (Y/N)? ", false, &quit))
				return false;
			if (quit)
				session->running = false;
			break;
		}
		case 11:
			if (!command_team(session, error))
				return false;
			break;
		case 12:
			if (!command_mines(session, error))
				return false;
			break;
		case 13:
			if (!command_collect(session, true, error))
				return false;
			break;
		case 14:
			if (!command_genesis(session, error))
				return false;
			break;
		case 15:
			if (!command_rename_port(session, error))
				return false;
			break;
		default:
			break;
		}
		if (enter_sector && session->running && !session->destroyed
		    && !sector_entry(session, error))
			return false;
		if (session->door->game_open
		    && !reload_player(session, error))
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
	if (!load_configuration(&session, error)
	    || !registration(&session, error))
		return session.terminated;
	if (!session.running)
		return true;
	session.presentation.sound.snoop =
	    session.door->game.config.local_screen;
	if (!opening_and_date(&session, error)
	    || !lockout(&session, error))
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
	if (!session.destroyed && session.running
	    && !command_shell(&session, error))
		return session.terminated;
	return quit_session(&session, error);
}
