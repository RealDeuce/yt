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

#define YT_PLAYER_FIRST YT_PLAYER_FIRST_RECORD
#define YT_PLAYER_LAST YT_PLAYER_LAST_RECORD
#define YT_COMMAND_SIZE 4096U
#define YT_ANTI_CLOAK_ADDRESS 0x1854U
#define YT_BACKGROUND_ADDRESS 0x1870U
#define YT_MARKET_BASE_ADDRESS 0x1860U
#define YT_DISRUPTION_SECTOR_ADDRESS 0x1878U
#define YT_BLINK_ADDRESS 0x1880U
#define YT_DESTROYED_ADDRESS 0x18B4U
#define YT_CURRENT_WARPS_ADDRESS 0x1898U
#define YT_PLANET_RECORD_SCRATCH_ADDRESS 0x19C4U
#define YT_COMPUTER_PLANET_LINK_ADDRESS 0x5184U
#define YT_NUMERIC_TEMP_DOUBLE_ADDRESS 0x0016U
#define YT_NUMERIC_TEMP_SINGLE_ADDRESS 0x001AU
#define YT_UPPERCASE_LENGTH_ADDRESS 0x536AU
#define YT_UPPERCASE_INDEX_ADDRESS 0x536EU
#define YT_LOCAL_SCREEN_ADDRESS 0x4B6CU
#define YT_CLEARANCE_HOLDS_ADDRESS 0x4B54U
#define YT_CLEARANCE_FIGHTERS_ADDRESS 0x4B58U
#define YT_CLEARANCE_GROUND_ADDRESS 0x4B5CU
#define YT_CLEARANCE_SHIELDS_ADDRESS 0x4B60U
#define YT_SPY_SECTORS_ADDRESS 0x4B76U
#define YT_SPY_MARKERS_ADDRESS 0x4B7EU
#define YT_SHARED_LOOP_SCRATCH_ADDRESS 0x4CD2U
#define YT_FRIENDSHIP_RELATION_ADDRESS 0x4BC4U
#define YT_SELF_MINE_SUPPRESSION_ADDRESS 0x4D0AU
#define YT_COMPUTER_ROUTE_STATUS_ADDRESS 0x4CF2U
#define YT_ATTACK_COMMITMENT_ADDRESS 0x4D1AU
#define YT_MERCENARIES_HURT_ADDRESS 0x4D5EU
#define YT_COMPUTER_PATH_MARKER_ADDRESS 0x4D62U
#define YT_COMPUTER_ROUTE_DESTINATION_ADDRESS 0x4E12U
#define YT_COMPUTER_ROUTE_START_ADDRESS 0x4E1AU
#define YT_COMPUTER_PATH_HOPS_ADDRESS 0x4E82U
#define YT_EARTH_REPORT_SEEN_ADDRESS 0x5006U
#define YT_SPY_COUNT_ADDRESS 0x50AEU
#define YT_LOW_TIME_REMEMBERED_ADDRESS 0x59CEU
#define YT_COLOR_INITIALIZED_ADDRESS 0x556AU
#define YT_COLOR_TABLE_ADDRESS 0x556EU
#define YT_CACHED_FOREGROUND_ADDRESS 0x559EU
#define YT_CACHED_BACKGROUND_ADDRESS 0x55A2U
#define YT_CLEARANCE_ANNOUNCED_ADDRESS 0x55B6U
#define YT_CLEARANCE_VALUE_ADDRESS 0x55BEU
#define YT_CLEARANCE_SOUND_SELECTOR_ADDRESS 0x55C6U
#define YT_HOSTILE_SURRENDER_RADIO_SELECTOR_ADDRESS 0x4D36U
#define YT_HOSTILE_SURRENDER_XANNOR_SELECTOR_ADDRESS 0x4D3EU
#define YT_HOSTILE_SURRENDER_MERCENARY_SELECTOR_ADDRESS 0x4D42U
#define YT_HOSTILE_SURRENDER_JOINED_SELECTOR_ADDRESS 0x4D46U
#define YT_HOSTILE_ATTACK_SOUND_SELECTOR_ADDRESS 0x4D2EU
#define YT_HOSTILE_BRIBE_SOUND_SELECTOR_ADDRESS 0x4D7AU
#define YT_HOSTILE_PLANET_LINK_ADDRESS 0x4CFAU
#define YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS 0x4CFEU
#define YT_HOSTILE_ATTACK_OWNER_ADDRESS 0x4D06U
#define YT_HOSTILE_ATTACKER_LOSSES_ADDRESS 0x4D1EU
#define YT_HOSTILE_DEFENDER_LOSSES_ADDRESS 0x4D26U
#define YT_HOSTILE_ATTACK_QUANTUM_ADDRESS 0x4D32U
#define YT_STATIC_DOUBLE_ZERO_ADDRESS 0x66D6U
#define YT_STATIC_SINGLE_ZERO_ADDRESS 0x62F4U
#define YT_STATIC_SINGLE_ONE_ADDRESS 0x628AU
#define YT_SESSION_MODE_ADDRESS 0x19C8U
#define YT_ANSI_ADDRESS 0x19A8U
#define YT_BOLD_ADDRESS 0x19BCU
#define YT_LOCAL_SOUND_ADDRESS 0x4B70U
#define YT_GAME_SOUND_ADDRESS 0x4BD4U
#define YT_SOUND_TOGGLE_SELECTOR_ADDRESS 0x5B32U
#define YT_COMPUTER_ACTIVATION_SELECTOR_ADDRESS 0x50D2U
#define YT_FATAL_SOUND_SELECTOR_ADDRESS 0x4CE2U
#define YT_PLASMA_PLAYER_SAVED_FOREGROUND_ADDRESS 0x5D9EU
#define YT_PLASMA_PLAYER_SOUND_SELECTOR_ADDRESS 0x5DA2U
#define YT_RETURNING_OLD_DAY_ADDRESS 0x5316U
#define YT_RETURNING_KILLER_ADDRESS 0x531EU
#define YT_RETURNING_TURNS_ADDRESS 0x5322U
#define YT_POST_LOGIN_RADIO_MODE_ADDRESS 0x64C8U
#define YT_POST_LOGIN_SCANNER_MODE_ADDRESS 0x64CCU
#define YT_COUNTERLAUNCH_COUNT_ADDRESS 0x5BC6U
#define YT_SPY_DESTINATION_SCRATCH_ADDRESS 0x5FE4U
#define YT_SPY_FOUND_SCRATCH_ADDRESS 0x5FE8U
#define YT_SPY_DEAD_COUNTER_SCRATCH_ADDRESS 0x6018U
#define YT_PLANET_UPDATER_QUANTITY_ADDRESS 0x19F0U
#define YT_PLANET_UPDATER_PRODUCTION_ADDRESS 0x1A44U
#define YT_PLANET_UPDATER_CONTRIBUTION_ADDRESS 0x1C14U
#define YT_PLANET_UPDATER_DAY_ADDRESS 0x5E90U
#define YT_PLANET_UPDATER_MINUTE_ADDRESS 0x5E94U
#define YT_PLANET_UPDATER_ELAPSED_ADDRESS 0x5E98U
#define YT_TIME_SAVED_CURSOR_ROW_ADDRESS 0x5372U
#define YT_TIME_SAVED_CURSOR_COLUMN_ADDRESS 0x5376U
#define YT_ACTION_FOREGROUND_SAVE_ADDRESS 0x51A8U
#define YT_ACTION_CLOAK_DISPLAY_SCALE_ADDRESS 0x8C36U
#define YT_ACTION_TURN_DIVISOR_ADDRESS 0x9E68U
#define YT_ACTION_XANNOR_THRESHOLD_ADDRESS 0x9EBCU
#define YT_TIME_REMAINING_MINUTES_ADDRESS 0x537AU
#define YT_TEAM_AUDIT_LOOP_ADDRESS 0x5F94U
#define YT_TEAM_AUDIT_SENDER_ADDRESS 0x5F98U
#define YT_SHARED_TARGET_RECORD_ADDRESS 0x1A40U
#define YT_SCOREBOARD_TEAM_SCRATCH_ADDRESS 0x4B8CU
#define YT_COUNTERATTACK_PLAYER_ADDRESS 0x1C10U
#define YT_XANNOR_PROVOKER_ADDRESS 0x4BDCU
#define YT_FOREGROUND_ADDRESS 0x1934U
#define YT_PAGER_NONSTOP_ADDRESS 0x4C9EU
#define YT_PAGER_NEWLINE_ADDRESS 0x4CB6U
#define YT_PAGER_LINE_COUNT_ADDRESS 0x4D7EU
#define YT_PAGER_SAVED_FOREGROUND_ADDRESS 0x51D0U
#define YT_FILE_VIEWER_SAVED_FOREGROUND_ADDRESS 0x51A8U

enum navigation_field_kind {
	NAVIGATION_FIELD_NONE,
	NAVIGATION_FIELD_ENTRY_PLAYER,
	NAVIGATION_FIELD_ROUTE_SECTOR,
	NAVIGATION_FIELD_INNER_PLAYER,
	NAVIGATION_FIELD_FINAL_SECTOR,
	NAVIGATION_FIELD_RETURN_PLAYER,
	NAVIGATION_FIELD_SCOREBOARD_PLAYER,
	NAVIGATION_FIELD_SCOREBOARD_SECTOR,
	NAVIGATION_FIELD_SCOREBOARD_TEAM,
	NAVIGATION_FIELD_RADIO_RECIPIENT,
	NAVIGATION_FIELD_RADIO_SENDER,
	NAVIGATION_FIELD_NEAREST_PLAYER,
	NAVIGATION_FIELD_NEAREST_SECTOR,
	NAVIGATION_FIELD_NEAREST_PORT,
	NAVIGATION_FIELD_NEAREST_OWNER,
	NAVIGATION_FIELD_PROFIT_PLAYER,
	NAVIGATION_FIELD_PROFIT_SECTOR,
	NAVIGATION_FIELD_PROFIT_PORT,
};

struct yt_session {
	struct yt_door *door;
	struct yt_error *error;
	const char *executable_path;
	int player_record_carrier;
	struct yt_player player;
	float current_sector_record;
	double combat_ship_fighters;
	float combat_ship_shields;
	uint8_t cached_player_name[YT_TEXT_FIELD_SIZE];
	size_t cached_player_name_length;
	struct yt_player_cache player_cache;
	char queue[YT_COMMAND_SIZE];
	size_t queue_length;
	size_t queue_position;
	char command_accumulator[YT_COMMAND_SIZE];
	char paged_text[YT_COMMAND_SIZE];
	char output_source[YT_COMMAND_SIZE];
	struct yt_input input;
	char saved_command[YT_COMMAND_SIZE];
	bool running;
	bool terminated;
	bool registered;
	bool fatal_wait_complete;
	struct yt_present_state presentation;
	struct yt_route_process route_process;
	char computer_route_scratch[YT_COMMAND_SIZE];
	size_t computer_route_scratch_length;
	bool navigation_field_active;
	enum navigation_field_kind navigation_field_kind;
	int navigation_field_record;
	struct yt_record navigation_field;
	bool radio_field_valid;
	uint32_t radio_field_record;
	struct yt_radio_record radio_field;
	char planet_name[42];
	double planet_quantity[10];
	struct yt_present_time_state time;
	struct yt_pager_state pager;
	struct yt_input_value input_residue;
	uint8_t team_audit_message[YT_TEAM_AUDIT_MESSAGE_MAX];
	size_t team_audit_message_length;
	uint8_t hostile_owner_label[160];
	size_t hostile_owner_label_length;
	struct yt_team_loader_cache team_cache;
};

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
	return session->team_cache.roster[index];
}

static bool
session_is_destroyed(const struct yt_session *session)
{
	uint8_t raw[4];

	yt_route_process_raw_single(&session->route_process,
	    YT_DESTROYED_ADDRESS, raw);
	return qb_mbf32_truth(raw);
}

static float
session_sector_offset(const struct yt_session *session)
{
	return session->door->game.config.sector_offset;
}

static float
session_port_offset(const struct yt_session *session)
{
	return session->door->game.config.port_offset;
}

static float
session_planet_offset(const struct yt_session *session)
{
	return session->door->game.config.planet_offset;
}

static float
session_foreground(const struct yt_session *session)
{
	return yt_route_process_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS);
}

static float
session_mode(const struct yt_session *session)
{
	return yt_route_process_single(&session->route_process,
	    YT_SESSION_MODE_ADDRESS);
}

static float
session_ansi(const struct yt_session *session)
{
	return yt_route_process_single(&session->route_process,
	    YT_ANSI_ADDRESS);
}

static int
session_pager_foreground(const struct yt_session *session)
{
	return (int)session_foreground(session);
}

static void
session_bind_pager_process(struct yt_session *session)
{
	yt_pager_bind_process_cells(&session->pager,
	    &session->route_process.bytes[YT_PAGER_LINE_COUNT_ADDRESS],
	    &session->route_process.bytes[YT_PAGER_NONSTOP_ADDRESS],
	    &session->route_process.bytes[YT_PAGER_NEWLINE_ADDRESS],
	    &session->route_process.bytes[YT_FOREGROUND_ADDRESS],
	    &session->route_process.bytes[YT_PAGER_SAVED_FOREGROUND_ADDRESS],
	    &session->route_process.bytes[YT_NUMERIC_TEMP_SINGLE_ADDRESS],
	    &session->route_process.bytes[YT_UPPERCASE_LENGTH_ADDRESS],
	    &session->route_process.bytes[YT_UPPERCASE_INDEX_ADDRESS]);
}

static void
session_set_pager_line_count_raw(struct yt_session *session,
    const uint8_t raw[4])
{
	yt_pager_set_line_count_raw(&session->pager, raw);
}

static void
session_set_pager_line_count(struct yt_session *session, float value)
{
	yt_pager_set_line_count(&session->pager, value);
}

static void
session_set_pager_nonstop(struct yt_session *session, float value)
{
	yt_pager_set_nonstop(&session->pager, value);
}

static void
session_set_pager_newline(struct yt_session *session, float value)
{
	yt_pager_set_newline(&session->pager, value);
}

static void
session_set_foreground_raw(struct yt_session *session, const uint8_t raw[4])
{
	float value = qb_mbf32_decode(raw);

	yt_route_process_set_raw_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS, raw);
	session->presentation.foreground = value;
	session->pager.foreground = (int)value;
}

static void
session_set_foreground(struct yt_session *session, float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) == QB_MBF_OK)
		session_set_foreground_raw(session, raw);
}

static void
session_store_destroyed(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_DESTROYED_ADDRESS, raw);
}

static void
session_store_current_player_record(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session->player_record_carrier = (int)qb_mbf32_decode(raw);
}

static bool
session_current_date_serial(struct yt_session *session, int *serial,
    int *adjusted_year, struct yt_error *error)
{
	return yt_current_date_serial(session->door->game.config.epoch_year,
	    serial, adjusted_year, error);
}

static int
session_record(const struct yt_session *session)
{
	return session->player_record_carrier;
}

static void
session_store_counterattack_player(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COUNTERATTACK_PLAYER_ADDRESS, raw);
}

static void
session_load_counterattack_player(struct yt_session *session,
    int *counterattack)
{
	if (counterattack != NULL)
		*counterattack = (int)yt_route_process_single(
		    &session->route_process, YT_COUNTERATTACK_PLAYER_ADDRESS);
}

static void
session_store_xannor_provoker(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_XANNOR_PROVOKER_ADDRESS, raw);
}

static void
session_load_xannor_provoker(struct yt_session *session, int *provoker)
{
	if (provoker != NULL)
		*provoker = (int)yt_route_process_single(&session->route_process,
		    YT_XANNOR_PROVOKER_ADDRESS);
}

static void
session_set_current_player_record(struct yt_session *session, int record)
{
	session->player_record_carrier = record;
}

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
    double *fighters, float *shields, bool bind_hostile_cells,
    struct yt_error *error);
static bool scanner_read_player(struct yt_session *session,
    float basic_record, struct yt_player *player, struct yt_error *error);

static void
session_current_warps(const struct yt_session *session, float warps[6])
{
	size_t slot;

	for (slot = 0U; slot < 6U; ++slot)
		warps[slot] = yt_route_process_single(&session->route_process,
		    (uint16_t)(YT_CURRENT_WARPS_ADDRESS + 4U * slot));
}

static void
session_market_bases(const struct yt_session *session, float bases[3])
{
	size_t index;

	for (index = 0U; index < 3U; ++index)
		bases[index] = yt_route_process_single(&session->route_process,
		    (uint16_t)(YT_MARKET_BASE_ADDRESS + 4U * index));
}

static float
session_disruption_sector(const struct yt_session *session, size_t index)
{
	return yt_route_process_single(&session->route_process,
	    (uint16_t)(YT_DISRUPTION_SECTOR_ADDRESS + 4U * index));
}

static void
session_disruption_sectors(const struct yt_session *session, float sectors[2])
{
	for (size_t index = 0U; index < 2U; ++index)
		sectors[index] = session_disruption_sector(session, index);
}

static bool
session_is_disruption_sector(const struct yt_session *session, float sector)
{
	return sector == session_disruption_sector(session, 0U)
	    || sector == session_disruption_sector(session, 1U);
}

static void
session_set_relationship(struct yt_session *session, float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) == QB_MBF_OK)
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_STATUS_ADDRESS, raw);
}

static void
session_set_self_mine_suppression(struct yt_session *session, bool enabled)
{
	static const uint8_t zero[4] = {0x00U, 0x00U, 0x00U, 0x00U};
	static const uint8_t one[4] = {0x00U, 0x00U, 0x00U, 0x81U};

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SELF_MINE_SUPPRESSION_ADDRESS, enabled ? one : zero);
}

static void
session_set_current_sector_record(struct yt_session *session, float value)
{
	session->current_sector_record = value;
}

static bool
session_anti_cloak_enabled(const struct yt_session *session)
{
	return yt_route_process_single(&session->route_process,
	    YT_ANTI_CLOAK_ADDRESS) != 0.0f;
}

static void
session_enable_anti_cloak(struct yt_session *session)
{
	static const uint8_t negative_one[4] = {0x00U, 0x00U, 0x80U, 0x81U};

	yt_route_process_set_raw_single(&session->route_process,
	    YT_ANTI_CLOAK_ADDRESS, negative_one);
}

static void
session_set_process_single(struct yt_session *session, uint16_t address,
    float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) == QB_MBF_OK)
		yt_route_process_set_raw_single(&session->route_process, address,
		    raw);
}

static void
session_set_process_double(struct yt_session *session, uint16_t address,
    double value)
{
	uint8_t raw[8];

	if (qb_mbf64_encode(value, raw) == QB_MBF_OK)
		yt_route_process_set_raw_double(&session->route_process, address,
		    raw);
}

static double
session_hostile_deployed_fighters(const struct yt_session *session)
{
	return yt_route_process_double(&session->route_process,
	    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS);
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
session_wait_raw(struct yt_session *session, const uint8_t raw[4],
    const char *operation, struct yt_error *error)
{
	float duration;

	duration = qb_mbf32_decode(raw);
	if (session_timed_wait(session, (double)duration))
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
session_ab36_repeat_emit(void *context, const uint8_t *prefix, size_t length)
{
	struct yt_session *session = context;

	session_set_pager_newline(session, 1.0f);
	return session_b05d(session, prefix, length);
}

static bool
session_ab36_submit_line(void *context)
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
session_ab36_printable_continue(void *context)
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

static bool
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

static int
sector_count(const struct yt_session *session)
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

static bool
session_hydration_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	return read_player_at_fault(session, player_record, player,
	    YT_BASIC_FAULT_CURRENT_PLAYER_A41C_GET, error);
}

static bool
reload_player(struct yt_session *session, struct yt_error *error)
{
	struct yt_current_player_hydration_state state = {
		.player = &session->player,
		.player_record = session_record(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.current_sector_record = &session->current_sector_record,
		.player_cache = &session->player_cache,
	};

	memcpy(state.sector_record_offset_raw,
	    session->door->game.config.record.bytes + YT_F53, 4U);
	yt_route_process_raw_single(&session->route_process,
	    YT_ANTI_CLOAK_ADDRESS, state.anti_cloak_raw);
	if (!yt_current_player_hydrate_run(&state,
	    session_hydration_read_player, session, error))
		return false;
	session->combat_ship_fighters = session->player.fighters;
	session->combat_ship_shields = session->player.shields;
	return true;
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
	bool result;

	memset(&state, 0, sizeof(state));
	state.hydration.player = &session->player;
	state.hydration.player_record = session_record(session);
	state.hydration.conversion_mode =
	    session->presentation.sound.conversion_mode;
	state.hydration.current_sector_record = &session->current_sector_record;
	state.hydration.player_cache = &session->player_cache;
	memcpy(state.hydration.sector_record_offset_raw,
	    session->door->game.config.record.bytes + YT_F53, 4U);
	yt_route_process_raw_single(&session->route_process,
	    YT_ANTI_CLOAK_ADDRESS, state.hydration.anti_cloak_raw);
	state.argument = argument;
	result = yt_credit_mutation_run(&state, &ops, session, error);
	if (state.hydrated) {
		session->combat_ship_fighters = session->player.fighters;
		session->combat_ship_shields = session->player.shields;
	}
	if (hydrated != NULL)
		*hydrated = state.hydrated;
	return result;
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

	if (player_record != (float)session_record(session)) {
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
	session->navigation_field_kind = NAVIGATION_FIELD_RETURN_PLAYER;
	session->navigation_field_record = session_record(session);
	session->navigation_field = session->player.record;
	session->navigation_field_active = false;
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
			if (!yt_input_ab36_queue_pop(session->queue,
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
			    session_ab36_printable_continue, session))
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
	if (!session_0317(session, (const uint8_t *)text, strlen(text),
	    "command notice", NULL))
		return false;
	return session_wait(session, 1.0, "command notice wait", NULL);
}

static void
session_input_process_store(void *context, uint16_t address,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process, address, raw);
}

static void
session_compat_upper_n(struct yt_session *session, uint8_t *text,
    size_t length)
{
	yt_input_compat_upper_n_observed(text, length,
	    session_input_process_store, session);
}

static bool
expand_repeat(struct yt_session *session, char *text, size_t size)
{
	struct yt_repeat_transform result;

	if (!yt_input_expand_repeat_observed(text, size,
	    session->saved_command, sizeof(session->saved_command),
	    session->output_source, sizeof(session->output_source), &result,
	    session_input_process_store, session)) {
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
	return yt_input_split_semicolon_observed(text, session->queue,
	    sizeof(session->queue), &session->queue_position,
	    &session->queue_length,
	    session_input_process_store, session);
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
	session_compat_upper_n(session, (uint8_t *)text, strlen(text));
	return session_store_output_source(session, (const uint8_t *)text,
	    strlen(text));
}

static bool
session_036f(struct yt_session *session, char *text, size_t size)
{
	if (!session_0357(session, text, size))
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
session_b05d(struct yt_session *session, const uint8_t *text, size_t length)
{
	static const struct yt_paged_row_ops ops = {
		session_paged_kernel,
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
	bool result;
	uint8_t foreground_raw[4];

	if (!session_store_output_source(session, text, length))
		return false;
	yt_pager_sync_process(&session->pager);
	yt_route_process_raw_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS, foreground_raw);
	session_set_foreground_raw(session, foreground_raw);
	result = yt_paged_row_run(&session->pager, &session->presentation,
	    &key_state, text, length, &ops, session);
	yt_route_process_raw_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS, foreground_raw);
	session_set_foreground_raw(session, foreground_raw);
	return result;
}

static void
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
			    || !append_news_bytes(session, event->data,
			    event->length, error))
				return false;
			break;
		case YT_SHARED_ERROR_NEWS:
			if (!append_news_bytes(session, event->data,
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

static bool
session_02fc(struct yt_session *session, const uint8_t *text, size_t length)
{
	session_set_pager_newline(session, 0.0f);
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
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	clear_queue(session);
	return session_02fc(session, text, length);
}

static bool
session_low_time(struct yt_session *session, const char *operation,
    struct yt_error *error)
{
	struct yt_present_result presentation;
	enum yt_present_status status;
	uint8_t remembered[4];
	uint8_t inherited[4];
	bool warned;

	yt_route_process_raw_single(&session->route_process,
	    YT_LOW_TIME_REMEMBERED_ADDRESS, remembered);
	memcpy(inherited, remembered, sizeof(inherited));
	status = yt_present_low_time_process(session->time.text,
	    session->time.text_length, remembered,
	    &session->presentation, &presentation, &warned);
	if (memcmp(remembered, inherited, sizeof(remembered)) != 0)
		yt_route_process_set_raw_single(&session->route_process,
		    YT_LOW_TIME_REMEMBERED_ADDRESS, remembered);
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
	session_set_pager_newline(session, 1.0f);
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

		if (session_mode(session) == 0.0f
		    && !yt_input_poll_source(&session->input, true, &selected))
			return false;
		reason = yt_input_drain_serial(&drain,
		    session_mode(session), &selected);
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

static float
session_file_viewer_save_foreground(struct yt_session *session)
{
	yt_route_process_copy_raw_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS, YT_FILE_VIEWER_SAVED_FOREGROUND_ADDRESS);
	return yt_route_process_single(&session->route_process,
	    YT_FILE_VIEWER_SAVED_FOREGROUND_ADDRESS);
}

static void
session_file_viewer_restore_foreground(struct yt_session *session)
{
	uint8_t raw[4];

	yt_route_process_raw_single(&session->route_process,
	    YT_FILE_VIEWER_SAVED_FOREGROUND_ADDRESS, raw);
	session_set_foreground_raw(session, raw);
}

struct session_file_viewer_context {
	struct yt_session *session;
	struct yt_text_input input;
	float *foreground;
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

	/* A5A9 commits the counter reset before the fallible OPEN. */
	session_set_pager_line_count(viewer->session,
	    viewer->session->pager.line_count);
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

	if (paged)
		session_set_foreground(viewer->session, *viewer->foreground);
	else
		session_file_viewer_restore_foreground(viewer->session);
	return session_file_viewer_present(viewer->session, text, length, paged,
	    error);
}

static void
session_file_viewer_set_bold(void *context, float value)
{
	struct session_file_viewer_context *viewer = context;

	yt_present_set_bold(&viewer->session->presentation, value);
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
	struct session_file_viewer_context context;
	struct yt_error local_error;
	struct yt_error *active_error = error == NULL ? &local_error : error;
	float saved_foreground = session_file_viewer_save_foreground(session);
	int saved_pager_foreground = session_pager_foreground(session);
	float foreground_carrier = saved_foreground;
	int pager_foreground_carrier = saved_pager_foreground;
	struct yt_file_viewer_stream_state state = {
		.play = {
			.foreground = &foreground_carrier,
			.pager_foreground = &pager_foreground_carrier,
			.bold = &session->presentation.bold,
			.set_bold = session_file_viewer_set_bold,
			.line_count = &session->pager.line_count,
			.pager_key = session->pager.key,
			.saved_foreground = saved_foreground,
			.saved_pager_foreground = saved_pager_foreground,
		},
		.path = path,
	};
	bool ok;

	memset(&context, 0, sizeof(context));
	context.session = session;
	context.foreground = &foreground_carrier;
	if (error == NULL)
		yt_error_clear(&local_error);
	if (!yt_file_viewer_entry(session->pager.key,
	    &session->pager.line_count, session_file_viewer_entry_present,
	    session, active_error))
		return false;
	yt_text_input_init(&context.input);
	ok = yt_file_viewer_stream_run(&state, &ops, &context, active_error);
	yt_text_input_destroy(&context.input);
	if (ok)
		session_set_pager_line_count(session, session->pager.line_count);
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
	uint8_t prompt_scratch[YT_COMMAND_SIZE];
	size_t prompt_scratch_length = prompt_length;

	if (answer == NULL || (prompt == NULL && prompt_length != 0U)
	    || prompt_length > sizeof(prompt_scratch))
		return false;
	if (prompt_length != 0U)
		memcpy(prompt_scratch, prompt, prompt_length);
	for (;;) {
		char response[YT_COMMAND_SIZE];
		struct yt_a8d2_transform transform;

		if (!session_present_text(session, prompt_scratch,
		    prompt_scratch_length,
		    SESSION_PRESENT_RAW, "yes/no prompt", error)
		    || !session_0357(session, response, sizeof(response))
		    || !yt_input_a8d2_staged(response, session->output_source,
		    sizeof(session->output_source), prompt_scratch,
		    sizeof(prompt_scratch), &prompt_scratch_length, session->queue,
		    sizeof(session->queue), &session->queue_position,
		    &session->queue_length, &session->presentation.bold,
		    YT_A8D2_FAULT_NONE, 0U, &transform)
		    || !transform.answer_valid)
			return false;
		*answer = transform.answer;
		if (transform.outcome == YT_A8D2_RETURNED)
			return true;
		if (transform.outcome != YT_A8D2_RETRY)
			return false;
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

static void
startup_configuration_store_disruption(void *context, size_t index,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	if (index >= 2U)
		return;
	yt_route_process_set_raw_single(&session->route_process,
	    (uint16_t)(YT_DISRUPTION_SECTOR_ADDRESS + 4U * index), raw);
}

static void
startup_configuration_store_local_screen(void *context,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_LOCAL_SCREEN_ADDRESS, raw);
}

static void
startup_configuration_store_uppercase(void *context,
    enum qb_compat_upper_store_kind kind, float value)
{
	struct yt_session *session = context;
	uint16_t address;

	switch (kind) {
	case QB_COMPAT_UPPER_STORE_NUMERIC_TEMP:
		address = YT_NUMERIC_TEMP_SINGLE_ADDRESS;
		break;
	case QB_COMPAT_UPPER_STORE_LENGTH:
		address = YT_UPPERCASE_LENGTH_ADDRESS;
		break;
	case QB_COMPAT_UPPER_STORE_INDEX:
		address = YT_UPPERCASE_INDEX_ADDRESS;
		break;
	default:
		return;
	}
	session_set_process_single(session, address, value);
}

static bool
load_configuration(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_startup_configuration_ops ops = {
		.close_data = startup_configuration_close,
		.open_data = startup_configuration_open,
		.load_config = startup_configuration_load,
		.store_config = startup_configuration_store,
		.read_player = startup_configuration_read_player,
		.write_player = startup_configuration_write_player,
		.random = startup_configuration_random,
		.store_disruption = startup_configuration_store_disruption,
		.store_local_screen = startup_configuration_store_local_screen,
		.store_uppercase = startup_configuration_store_uppercase,
	};
	struct yt_game *game = &session->door->game;
	struct yt_startup_configuration_state state;
	bool ok;

	memset(game, 0, sizeof(*game));
	yt_random_init(&game->random);
	memset(&state, 0, sizeof(state));
	state.config = &game->config;
	state.local_mode = session->door->identity.local ? -1.0f : 0.0f;
	state.player_cache = &session->player_cache;
	session_disruption_sectors(session, state.black_hole);
	ok = yt_startup_configuration_run(&state, &ops, session, error);
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
	completed = yt_registration_run(&state, &ops, &context, error);
	session->registered = state.registered;
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
	if (session_ansi(session) != 0.0f) {
		if (!yt_out_opening_file_observed("YTOPEN.ANS",
		    session_mode(session),
		    yt_sound_snoop(&session->presentation.sound),
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
	if (!session_0317(session, (const uint8_t *)"Initializing...",
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

		session_set_foreground(session, 2.0f);
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
			if (!session_02fc(session, (const uint8_t *)identity,
			    strlen(identity))
			    || !session_present_text(session, NULL, 0,
			    SESSION_PRESENT_LINE, "new alias confirmation blank", error)) {
				yt_names_free(&names);
				return false;
			}
		}
		session_set_foreground(session, 6.0f);
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
		row.real_first = first;
		row.real_last = last;
		row.alias_first = alias_first;
		row.alias_last = alias_last;
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
	uint8_t zero_raw[4];

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
	yt_route_process_set_raw_single(&session->route_process,
	    YT_NUMERIC_TEMP_SINGLE_ADDRESS, length_raw);
	(void)yt_record_set_raw_number(&identity, YT_F85, length_raw);
	yt_route_process_raw_single(&session->route_process,
	    YT_STATIC_SINGLE_ZERO_ADDRESS, zero_raw);
	(void)yt_record_set_raw_number(&identity, YT_F89, zero_raw);
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
		    || !session_0345(session, response, sizeof(response))
		    || !yt_input_yes_no_candidate(session->command_accumulator,
		    session->output_source, sizeof(session->output_source),
		    &answer))
			return false;
		if (answer == YT_YES_NO_EMPTY || answer == YT_YES_NO_NO)
			return true;
		if (answer == YT_YES_NO_YES)
			return display_game_file(session, "YTINSTR.DOC", error);
		yt_present_set_bold(&session->presentation, 1.0f);
		clear_queue(session);
	}
}

static bool
startup_retention_read_config(void *context, struct yt_record *record,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return yt_database_read(&session->door->game.database, 1U, record,
	    error);
}

static bool
startup_retention_present(void *context, const uint8_t *text, size_t length,
    enum yt_startup_retention_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_STARTUP_RETENTION_FIRST_ROW:
		return session_0317(session, text, length,
		    "new player retention first row", error);
	case YT_STARTUP_RETENTION_SECOND_ROW:
		return session_02fc(session, text, length);
	case YT_STARTUP_RETENTION_FINAL_BLANK:
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "new player retention final blank",
		    error);
	}
	return false;
}

static bool
returning_daily_same_day(void *context, struct yt_error *error)
{
	static const uint8_t row[] = "You have been on today.";
	struct yt_session *session = context;

	return session_present_text(session, row, sizeof(row) - 1U,
	    SESSION_PRESENT_LINE, "returning same-day row", error);
}

static void
returning_daily_store_scratch(void *context,
    enum yt_returning_daily_scratch_kind kind, const uint8_t raw[4])
{
	static const uint16_t addresses[] = {
		[YT_RETURNING_DAILY_OLD_DAY] = YT_RETURNING_OLD_DAY_ADDRESS,
		[YT_RETURNING_DAILY_KILLER] = YT_RETURNING_KILLER_ADDRESS,
		[YT_RETURNING_DAILY_TURNS] = YT_RETURNING_TURNS_ADDRESS,
	};
	struct yt_session *session = context;

	if ((size_t)kind < YT_ARRAY_LEN(addresses))
		yt_route_process_set_raw_single(&session->route_process,
		    addresses[kind], raw);
}

static bool
returning_denial_present(void *context, const uint8_t *text, size_t length,
    enum yt_returning_denial_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_present_text(session, text, length,
	    kind == YT_RETURNING_DENIAL_ROW ? SESSION_PRESENT_BOLD_LINE
	    : SESSION_PRESENT_LINE,
	    kind == YT_RETURNING_DENIAL_ROW ? "returning self-denial row"
	    : "returning self-denial blank", error);
}

static void
returning_denial_set_foreground(void *context, float foreground)
{
	session_set_foreground(context, foreground);
}

static void
returning_denial_set_blink(void *context, float blink)
{
	struct yt_session *session = context;

	yt_present_set_blink(&session->presentation, blink);
}

static bool
returning_denial_close_all(void *context, struct yt_error *error)
{
	(void)error;
	return session_editor_close_all(context);
}

static void
returning_denial_end(void *context)
{
	struct yt_session *session = context;

	session->running = false;
	session->terminated = true;
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

		session_set_process_single(session,
		    YT_SHARED_LOOP_SCRATCH_ADDRESS, (float)basic);
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
		session_set_process_single(session,
		    YT_SHARED_LOOP_SCRATCH_ADDRESS, (float)(basic + 1));
	}
	if (!returning) {
		int vacant = 0;
		float vacancy_bound;

		session_set_foreground(session, 5.0f);
		if (!session_0317(session,
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
			static const struct yt_startup_retention_ops ops = {
				startup_retention_read_config,
				startup_retention_present,
			};
			struct yt_startup_retention_state state;

			if (!yt_startup_retention_run(&state, &ops, session, error))
				return false;
		}
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
		static const struct yt_returning_daily_ops daily_ops = {
			returning_daily_same_day,
			returning_daily_store_scratch,
		};
		struct yt_returning_daily_state daily;
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
		memset(&daily, 0, sizeof(daily));
		daily.player_record = session_record(session);
		daily.today_raw = today_raw;
		daily.turns_per_day_raw = turns_raw;
		if (!yt_returning_daily_run(&session->door->game, &daily,
		    &daily_ops, session, error)) {
			if (!daily.player_hydrated)
				attach_database_get_fault(session, error,
				    YT_BASIC_FAULT_RETURNING_DAILY_GET);
			else if (daily.put_attempted)
				attach_database_put_fault(session, error,
				    YT_BASIC_FAULT_RETURNING_DAILY_PUT);
			if (error != NULL && error->basic_fault_valid)
				(void)session_route_basic_fault(session, error);
			return false;
		}
		session->player = daily.player;
		if (!yt_database_flush(&session->door->game.database, error))
			return false;
		previous_day = daily.previous_day;
		killer = daily.killer;
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
				static const struct yt_returning_denial_ops denial_ops = {
					returning_denial_present,
					returning_denial_set_foreground,
					returning_denial_set_blink,
					returning_denial_close_all,
					returning_denial_end,
				};
				struct yt_returning_denial_state denial;

				memset(&denial, 0, sizeof(denial));
				(void)yt_returning_self_denial_run(&denial,
				    &denial_ops, session, error);
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
    size_t capacity, size_t *length, bool sender,
    struct yt_player *loaded_player, bool *loaded_player_valid,
    struct yt_error *error)
{
	const uint8_t *literal;
	size_t literal_length;

	if (length == NULL)
		return false;
	*length = 0;
	if (loaded_player_valid != NULL)
		*loaded_player_valid = false;
	if (record > 0.0f) {
		struct yt_player player;
		uint8_t stored[YT_TEXT_FIELD_SIZE];
		size_t stored_length;

		if (!scanner_read_player(session, record, &player, error))
			return false;
		if (loaded_player != NULL)
			*loaded_player = player;
		if (loaded_player_valid != NULL)
			*loaded_player_valid = true;
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

struct radio_read_context {
	struct yt_session *session;
	struct yt_radio_file file;
	struct yt_radio_record radio_field;
	uint32_t radio_field_record;
	struct yt_player player_field;
	uint32_t player_field_record;
	enum yt_radio_read_name_role player_field_role;
	bool radio_field_valid;
	bool player_field_valid;
	float reader_mode;
};

static void
radio_read_attach_fault(struct yt_error *error, enum yt_basic_fault_site site,
    uint16_t basic_error)
{
	if (error == NULL)
		return;
	if (basic_error == 0U && error->basic_error_valid)
		basic_error = error->basic_error;
	if (basic_error == 0U
	    || !yt_error_attach_basic_fault_number(error, site, basic_error))
		(void)yt_error_attach_basic_fault(error, site);
}

static bool
radio_read_open(void *context, struct yt_error *error)
{
	struct radio_read_context *reader = context;

	yt_radio_file_init(&reader->file);
	if (yt_radio_file_open(&reader->file, "YTRMSG.DAT", error))
		return true;
	radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_OPEN,
	    reader->file.random.last_open.basic_error != 0U
	    ? reader->file.random.last_open.basic_error
	    : reader->file.random.last_close.basic_error);
	return false;
}

static bool
radio_read_size(void *context, uint64_t *length, struct yt_error *error)
{
	struct radio_read_context *reader = context;

	if (yt_radio_file_size(&reader->file, length, error))
		return true;
	radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_LOF,
	    reader->file.random.last_lof.basic_error);
	return false;
}

static bool
radio_read_get(void *context, uint32_t record,
    struct yt_radio_record *value, struct yt_error *error)
{
	struct radio_read_context *reader = context;
	bool result;

	result = yt_radio_file_get(&reader->file, record, value, NULL, error);
	if (!result)
		radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_RECORD_GET,
		    reader->file.random.last_get.basic_error);
	if (result) {
		reader->radio_field = *value;
		reader->radio_field_record = record;
		reader->radio_field_valid = true;
	}
	return result;
}

static bool
radio_read_name(void *context, float record, bool sender, uint8_t *dest,
    size_t capacity, size_t *length, struct yt_error *error)
{
	struct radio_read_context *reader = context;
	struct yt_player player;
	bool player_valid;
	bool result;

	result = radio_name_bytes(reader->session, record, dest, capacity,
	    length, sender, &player, &player_valid, error);
	if (!result && record > 0.0f)
		radio_read_attach_fault(error, sender
		    ? YT_BASIC_FAULT_RADIO_SENDER_GET
		    : YT_BASIC_FAULT_RADIO_RECIPIENT_GET,
		    reader->session->door->game.database.last_get.basic_error);
	if (player_valid) {
		reader->player_field = player;
		reader->player_field_record = qb_brun_random_record_number(record);
		reader->player_field_role = sender
		    ? YT_RADIO_READ_NAME_SENDER : YT_RADIO_READ_NAME_RECIPIENT;
		reader->player_field_valid = true;
	}
	return result;
}

static bool
radio_read_present(void *context, const uint8_t *text, size_t length,
    enum yt_radio_read_output_kind kind, struct yt_error *error)
{
	static const char *const operations[] = {
		"radio opening blank",
		"radio heading",
		"radio pair blank",
		"radio pair header",
		"radio body",
		"radio pause",
		"radio pause blank",
		"radio none found",
	};
	struct radio_read_context *reader = context;

	if ((size_t)kind >= YT_ARRAY_LEN(operations))
		return false;
	if (session_present_text(reader->session, text, length,
	    kind == YT_RADIO_READ_PAUSE ? SESSION_PRESENT_RAW
	    : SESSION_PRESENT_LINE, operations[kind], error))
		return true;
	if (kind == YT_RADIO_READ_OPENING_BLANK)
		radio_read_attach_fault(error,
		    YT_BASIC_FAULT_RADIO_OPENING_OUTPUT, 0U);
	else if (kind == YT_RADIO_READ_HEADING)
		radio_read_attach_fault(error, reader->reader_mode != 0.0f
		    ? YT_BASIC_FAULT_RADIO_LOG_HEADING_OUTPUT
		    : YT_BASIC_FAULT_RADIO_AUTO_HEADING_OUTPUT, 0U);
	else if (kind == YT_RADIO_READ_PAUSE)
		radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_PAUSE_OUTPUT,
		    0U);
	return false;
}

static bool
radio_read_wait(void *context, double seconds, struct yt_error *error)
{
	struct radio_read_context *reader = context;

	if (seconds == 99.0 && session_wait(reader->session, 99.0,
	    "radio private-pager wait", error))
		return true;
	radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_PRIVATE_WAIT, 0U);
	return false;
}

static bool
radio_read_put(void *context, uint32_t record,
    const struct yt_radio_record *value, struct yt_error *error)
{
	struct radio_read_context *reader = context;

	return yt_radio_file_put(&reader->file, record, value, error);
}

static bool
radio_read_close(void *context, struct yt_error *error)
{
	struct radio_read_context *reader = context;

	if (yt_radio_file_close(&reader->file, error))
		return true;
	radio_read_attach_fault(error, YT_BASIC_FAULT_RADIO_FINAL_CLOSE,
	    reader->file.random.last_close.basic_error);
	return false;
}

static bool
radio_read(struct yt_session *session, float reader_mode,
    struct yt_error *error)
{
	static const struct yt_radio_read_ops ops = {
		radio_read_open,
		radio_read_size,
		radio_read_get,
		radio_read_name,
		radio_read_present,
		radio_read_wait,
		radio_read_put,
		radio_read_close,
	};
	struct radio_read_context context = {
		.session = session,
		.reader_mode = reader_mode,
	};
	struct yt_radio_read_state state = {
		.reader_mode = reader_mode,
		.current_player = (float)session_record(session),
	};
	bool ok;

	ok = yt_radio_read_run(&state, &ops, &context, error);
	if (context.radio_field_valid) {
		session->radio_field_valid = true;
		session->radio_field_record = context.radio_field_record;
		session->radio_field = context.radio_field;
	}
	if (context.player_field_valid) {
		session->navigation_field_active = true;
		session->navigation_field_record =
		    (int)context.player_field_record;
		session->navigation_field = context.player_field.record;
		session->navigation_field_kind = context.player_field_role
		    == YT_RADIO_READ_NAME_SENDER
		    ? NAVIGATION_FIELD_RADIO_SENDER
		    : NAVIGATION_FIELD_RADIO_RECIPIENT;
	}
	if (!ok && error != NULL
	    && strcmp(error->operation, "radio scan bound") == 0)
		(void)snprintf(error->path, sizeof(error->path), "%s",
		    context.file.random.path);
	return ok;
}

static bool
post_login(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t prompt[] = "[ Press any Key ]";
	static const uint8_t radio_mode_zero[4] = {0x1f, 0x4e, 0x46, 0x00};

	{
		struct yt_record repaired;
		uint8_t one_raw[4];
		uint8_t zero_raw[4];
		uint8_t maximum_raw[4];

		if (!reload_player(session, error))
			return false;
		yt_route_process_raw_single(&session->route_process,
		    YT_STATIC_SINGLE_ONE_ADDRESS, one_raw);
		if (session->player.sector < qb_mbf32_decode(one_raw)) {
			repaired = session->player.record;
			(void)yt_record_set_raw_number(&repaired, YT_F57, one_raw);
			yt_player_decode(&session->player, &repaired);
			if (!write_database_record_at_fault(session,
			    (uint32_t)session_record(session), &repaired,
			    YT_BASIC_FAULT_POST_LOGIN_SECTOR_PUT, error))
				return false;
		}
		if (!reload_player(session, error))
			return false;
		yt_route_process_raw_single(&session->route_process,
		    YT_STATIC_SINGLE_ZERO_ADDRESS, zero_raw);
		memcpy(maximum_raw, session->door->game.config.record.bytes + YT_F121,
	    sizeof(maximum_raw));
		if ((double)session->player.holds
		    > (double)qb_mbf32_decode(maximum_raw)) {
			repaired = session->player.record;
			(void)yt_record_set_raw_number(&repaired, YT_F69, zero_raw);
			(void)yt_record_set_raw_number(&repaired, YT_F73, zero_raw);
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
	if (!session_031f(session, prompt, sizeof(prompt) - 1U,
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
	yt_route_process_set_raw_single(&session->route_process,
	    YT_POST_LOGIN_RADIO_MODE_ADDRESS, radio_mode_zero);
	if (!radio_read(session, yt_route_process_single(&session->route_process,
	    YT_POST_LOGIN_RADIO_MODE_ADDRESS), error))
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
	session_market_bases(session, state.base_price);
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
planet_updater_load_process(struct yt_session *session,
    struct yt_planet_updater_state *state)
{
	size_t index;

	for (index = 0U; index < 10U; ++index) {
		yt_route_process_raw_double(&session->route_process,
		    YT_PLANET_UPDATER_QUANTITY_ADDRESS + (uint16_t)(8U * index),
		    state->raw_cache.quantity[index]);
		yt_route_process_raw_single(&session->route_process,
		    YT_PLANET_UPDATER_PRODUCTION_ADDRESS + (uint16_t)(4U * index),
		    state->raw_cache.production[index]);
		yt_route_process_raw_single(&session->route_process,
		    YT_PLANET_UPDATER_CONTRIBUTION_ADDRESS + (uint16_t)(4U * index),
		    state->raw_cache.contribution[index]);
	}
	yt_route_process_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_DAY_ADDRESS, state->current_day_raw);
	yt_route_process_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_MINUTE_ADDRESS, state->raw_cache.current_minute);
	yt_route_process_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_ELAPSED_ADDRESS, state->raw_cache.elapsed);
}

static void
planet_updater_store_process(struct yt_session *session,
    const struct yt_planet_updater_state *state)
{
	size_t index;

	for (index = 0U; index < 10U; ++index) {
		yt_route_process_set_raw_double(&session->route_process,
		    YT_PLANET_UPDATER_QUANTITY_ADDRESS + (uint16_t)(8U * index),
		    state->raw_cache.quantity[index]);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_PLANET_UPDATER_PRODUCTION_ADDRESS + (uint16_t)(4U * index),
		    state->raw_cache.production[index]);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_PLANET_UPDATER_CONTRIBUTION_ADDRESS + (uint16_t)(4U * index),
		    state->raw_cache.contribution[index]);
	}
	yt_route_process_set_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_DAY_ADDRESS, state->current_day_raw);
	yt_route_process_set_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_MINUTE_ADDRESS, state->raw_cache.current_minute);
	yt_route_process_set_raw_single(&session->route_process,
	    YT_PLANET_UPDATER_ELAPSED_ADDRESS, state->raw_cache.elapsed);
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
	planet_updater_load_process(session, &state);
	if (!yt_planet_updater_raw_run(&state, &ops, session, error)) {
		planet_updater_store_process(session, &state);
		return false;
	}
	planet_updater_store_process(session, &state);
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
	uint8_t fighters_raw[8];

	yt_route_process_set_raw_single(&session->route_process,
	    YT_HOSTILE_PLANET_LINK_ADDRESS,
	    &sector->record.bytes[YT_F93]);
	(void)qb_mbf64_encode((double)qb_mbf32_decode(
	    &sector->record.bytes[YT_F81]), fighters_raw);
	yt_route_process_set_raw_double(&session->route_process,
	    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS, fighters_raw);
	yt_route_process_set_raw_single(&session->route_process,
	    YT_HOSTILE_ATTACK_OWNER_ADDRESS,
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
	return reload_player(context, error);
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

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, raw);
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
		.sector_count = (float)sector_count(session),
		.sector_offset = session_sector_offset(session),
		.current_player_record = (float)session_record(session),
		.disruption_sectors = {
			session_disruption_sector(session, 0U),
			session_disruption_sector(session, 1U),
		},
	};
	bool result;

	if (danger == NULL)
		return false;
	yt_route_process_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, state.relationship_raw);
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

static void
spy_store(void *context, enum yt_spy_scratch_kind kind,
    const uint8_t raw[4])
{
	struct yt_session *session = context;
	uint16_t address;

	switch (kind) {
	case YT_SPY_SCRATCH_DESTINATION:
		address = YT_SPY_DESTINATION_SCRATCH_ADDRESS;
		break;
	case YT_SPY_SCRATCH_FOUND:
		address = YT_SPY_FOUND_SCRATCH_ADDRESS;
		break;
	case YT_SPY_SCRATCH_DEAD_COUNTER:
		address = YT_SPY_DEAD_COUNTER_SCRATCH_ADDRESS;
		break;
	default:
		return;
	}
	yt_route_process_set_raw_single(&session->route_process, address, raw);
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
		spy_store,
	};
	int spy_sectors[3];
	int spy_markers[3];
	struct yt_spy_sweep_state state;
	size_t index;
	bool result;

	for (index = 0U; index < YT_ARRAY_LEN(spy_sectors); ++index) {
		spy_sectors[index] = yt_route_process_word(&session->route_process,
		    (uint16_t)(YT_SPY_SECTORS_ADDRESS + 2U * index));
		spy_markers[index] = yt_route_process_word(&session->route_process,
		    (uint16_t)(YT_SPY_MARKERS_ADDRESS + 2U * index));
	}
	state = (struct yt_spy_sweep_state){
		.active_spies = yt_route_process_single(&session->route_process,
		    YT_SPY_COUNT_ADDRESS),
		.spy_sectors = spy_sectors,
		.last_reported_sectors = spy_markers,
		.spy_capacity = YT_ARRAY_LEN(spy_sectors),
		.current_player_record = session_record(session),
		.last_player_record = session_sector_offset(session),
		.disruption_sectors = {
			session_disruption_sector(session, 0U),
			session_disruption_sector(session, 1U)
		},
		.player_cache = &session->player_cache,
		.found_scratch = yt_route_process_single(&session->route_process,
		    YT_SPY_FOUND_SCRATCH_ADDRESS),
		.dead_counter_scratch = yt_route_process_single(
		    &session->route_process,
		    YT_SPY_DEAD_COUNTER_SCRATCH_ADDRESS),
		.warp_destination_scratch = yt_route_process_single(
		    &session->route_process,
		    YT_SPY_DESTINATION_SCRATCH_ADDRESS),
		.foreground = session_foreground(session),
		.background = yt_present_background(&session->presentation),
		.bold = yt_present_bold(&session->presentation),
		.blink = yt_present_blink(&session->presentation),
	};
	result = yt_spy_sweep_run(&state, &ops, session, error);

	for (index = 0U; index < YT_ARRAY_LEN(spy_sectors); ++index) {
		yt_route_process_set_word(&session->route_process,
		    (uint16_t)(YT_SPY_SECTORS_ADDRESS + 2U * index),
		    (int16_t)spy_sectors[index]);
		yt_route_process_set_word(&session->route_process,
		    (uint16_t)(YT_SPY_MARKERS_ADDRESS + 2U * index),
		    (int16_t)spy_markers[index]);
	}
	spy_import_presentation(session, &state);
	return result;
}

static bool
fresh_no_turn_gate(struct yt_session *session, bool *denied,
    struct yt_error *error)
{
	static const uint8_t notice[] = "Sorry but you have no turns left.";
	uint8_t result_raw[4];

	if (!reload_player(session, error))
		return false;
	yt_no_turn_gate_result_raw(false, result_raw);
	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, result_raw);
	*denied = yt_no_turn_gate_denied(session->player.turns);
	if (*denied) {
		yt_no_turn_gate_result_raw(true, result_raw);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_STATUS_ADDRESS, result_raw);
		return session_02db(session, notice, sizeof(notice) - 1U,
		    "no-turn gate notice", error);
	}
	return true;
}

static bool
finalize_action(struct yt_session *session, float amount,
    struct yt_error *error)
{
	int xannor_provoker;
	float quotient;
	float draw;
	char number[64];
	char row[128];
	uint8_t turn_raw[4];
	uint8_t anti_cloak_raw[4];
	bool anti_cloak_allows;

	(void)amount;
	if (!spy_sweep(session, error) || !reload_player(session, error))
		return false;
	memcpy(turn_raw, session->player.record.bytes + YT_F49,
	    sizeof(turn_raw));
	if (!yt_action_finalizer_turn_raw(turn_raw, turn_raw))
		return false;
	session->player.turns = qb_mbf32_decode(turn_raw);
	if (!yt_record_set_raw_number(&session->player.record, YT_F49, turn_raw))
		return false;
	quotient = single_div(session->player.turns,
	    yt_route_process_single(&session->route_process,
	    YT_ACTION_TURN_DIVISOR_ADDRESS));
	yt_route_process_raw_single(&session->route_process,
	    YT_ANTI_CLOAK_ADDRESS, anti_cloak_raw);
	if (!yt_action_finalizer_anti_cloak_raw_allows(anti_cloak_raw,
	    session->presentation.sound.conversion_mode, &anti_cloak_allows)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			error->system_error = 0;
			(void)snprintf(error->operation, sizeof(error->operation), "%s",
			    "action-finalizer anti-cloak CINT");
		}
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_ACTION_FINALIZER_ANTI_CLOAK_CINT, 6U);
		return false;
	}
	if (quotient == floorf(quotient) && anti_cloak_allows) {
		float display;
		uint8_t cloak_arithmetic[4];
		uint8_t cloak_result[4];
		uint8_t foreground_raw[4];
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
		    yt_route_process_single(&session->route_process,
		    YT_ACTION_CLOAK_DISPLAY_SCALE_ADDRESS)));
		qb_str_single(number, sizeof(number), display);
		snprintf(row, sizeof(row), "Cloak at%s%%", number);
		yt_route_process_raw_single(&session->route_process,
		    YT_FOREGROUND_ADDRESS, foreground_raw);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_ACTION_FOREGROUND_SAVE_ADDRESS, foreground_raw);
		session_set_foreground(session, 7.0f);
		if (!session_031f(session, (const uint8_t *)row, strlen(row),
		    "action-finalizer cloak row", error))
			return false;
		yt_route_process_raw_single(&session->route_process,
		    YT_ACTION_FOREGROUND_SAVE_ADDRESS, foreground_raw);
		session_set_foreground_raw(session, foreground_raw);
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
	if (!session_02fc(session, (const uint8_t *)row, strlen(row)))
		return false;
	if (!random_value(session, &draw, error))
		return false;
	if (draw > yt_route_process_single(&session->route_process,
	    YT_ACTION_XANNOR_THRESHOLD_ADDRESS)) {
		session_load_xannor_provoker(session, &xannor_provoker);
		if (!launch_xannor_retaliation(session, &xannor_provoker,
		    error))
			return false;
		if (session_is_destroyed(session))
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
	if (!session_02fc(session, warning_one, sizeof(warning_one) - 1U))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	if (!session_02fc(session, warning_two, sizeof(warning_two) - 1U)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "emergency warp confirmation blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
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

	session_set_self_mine_suppression(session, false);
}

static bool
movement_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
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
		.read_player = death_team_read_player,
		.write_player = death_team_write_player,
		.read_record = session_read_physical_record,
		.write_record = death_team_write_record,
	};
	struct yt_death_team_remove_state state = {
		.victim_record = victim,
		.current_player_record = (float)session_record(session),
		.sector_record_offset = session_sector_offset(session),
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

static bool
player_death_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
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
		player_death_news,
		player_death_set_current,
		player_death_flush,
	};
	struct yt_player_death_state state = {
		.victim_record = victim_record,
		.current_player_record = session_record(session),
		.killer = killer,
		.sector_count = sector_count(session),
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
	return session_02db(context, text, length, "common fatal notice", error);
}

static bool
common_fatal_read_player(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;
	char cached_name[sizeof(session->player.name)];

	memcpy(cached_name, session->player.name, sizeof(cached_name));
	if (player_record != session_record(session)
	    || !reload_player(session, error))
		return false;
	memcpy(session->player.name, cached_name, sizeof(cached_name));
	*player = session->player;
	return true;
}

static bool
common_fatal_sound(void *context, const uint8_t selector_raw[4],
    struct yt_error *error)
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_FATAL_SOUND_SELECTOR_ADDRESS, selector_raw);
	return session_sound(session, yt_route_process_single(
	    &session->route_process, YT_FATAL_SOUND_SELECTOR_ADDRESS),
	    "fatal destruction sound", error);
}

static bool
common_fatal_death(void *context, int victim_record, float killer,
    struct yt_error *error)
{
	return kill_player_run(context, victim_record, killer, false, error);
}

static void
common_fatal_store_target(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SHARED_TARGET_RECORD_ADDRESS, raw);
}

static bool
common_fatal_wait(void *context, const uint8_t duration_raw[4],
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (!session_wait_raw(session, duration_raw,
	    "common fatal wait", error))
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
		common_fatal_store_target,
		common_fatal_sound,
		common_fatal_death,
		common_fatal_wait,
	};
	uint8_t current_record_raw[4];

	if (qb_mbf32_encode((float)session_record(session), current_record_raw)
	    != QB_MBF_OK)
		return false;
	struct yt_common_fatal_state state = {
		.current_player_record = session_record(session),
		.current_player_record_raw = current_record_raw,
		.foreground = session_foreground(session),
		.pager_foreground = session_pager_foreground(session),
	};

	return yt_common_fatal_run(&state, &ops, session, error);
}

static bool
salvage_load_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (player_record == session_record(session)) {
		if (!reload_player(session, error))
			return false;
		*player = session->player;
		return true;
	}
	return yt_game_read_player(&session->door->game, player_record, player,
	    error);
}

static bool
salvage_save_player(struct yt_session *session, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	if (!yt_game_write_player(&session->door->game, player_record, player,
	    error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;
	if (player_record == session_record(session))
		session->player = *player;
	return true;
}

static bool
salvage_player(struct yt_session *session, int victim_record,
    int killer_record, struct yt_error *error)
{
	static const uint8_t title[] =
	    "You destroyed the ship and salvaged the following:";
	static const uint8_t nothing[] = "  -  NOTHING!";
	static const enum yt_salvage_cargo_kind cargo_kind[4] = {
		YT_SALVAGE_EMPTY_HOLDS, YT_SALVAGE_ORE,
		YT_SALVAGE_ORGANICS, YT_SALVAGE_EQUIPMENT
	};
	static const size_t cargo_order[4] = {3U, 0U, 1U, 2U};
	struct yt_player victim;
	struct yt_player killer;
	float awards[6] = {0};
	float cargo_stock[3];
	float cargo_awards[4] = {0};
	float cargo_remaining;
	float requested_holds;
	float *simple_fields[5];
	uint8_t victim_name[YT_TEXT_FIELD_SIZE];
	uint8_t row[300];
	size_t victim_name_length;
	size_t row_length;
	size_t index;
	bool emitted = false;

	/*
	 * The victim GET precedes the killer-range gate.  Player record
	 * identities are integers because every caller and every persisted
	 * producer writes an integer player record.
	 */
	if (!yt_game_read_player(&session->door->game, victim_record, &victim,
	    error))
		return false;
	if (killer_record < YT_PLAYER_FIRST
	    || (float)killer_record > session->door->game.config.sector_offset)
		return true;
	if (!yt_player_stored_name(&victim, victim_name, &victim_name_length,
	    error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage result row", error)
	    || !session_present_text(session, title, sizeof(title) - 1U,
	    SESSION_PRESENT_BOLD_LINE, "salvage title", error)
	    || !yt_salvage_header_row((const uint8_t *)session->player.name,
	    strlen(session->player.name), victim_name, victim_name_length,
	    row, sizeof(row), &row_length)
	    || !append_news_bytes(session, row, row_length, error)
	    || !session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "salvage result row", error))
		return false;

	for (index = 0U; index < YT_ARRAY_LEN(awards); ++index) {
		float stock;
		float draw;

		if (!random_value(session, &draw, error))
			return false;
		switch (index) {
		case 0U: stock = victim.holds; break;
		case 1U: stock = victim.credits; break;
		case 2U: stock = victim.missiles; break;
		case 3U: stock = victim.plasma; break;
		case 4U: stock = victim.ground_forces; break;
		default: stock = victim.mines; break;
		}
		awards[index] = floorf(single_mul(draw, stock));
	}
	if (!session_wait(session, 1.0, "ship salvage wait", error)
	    || !salvage_load_player(session, killer_record, &killer, error))
		return false;

	simple_fields[0] = &killer.credits;
	simple_fields[1] = &killer.missiles;
	simple_fields[2] = &killer.plasma;
	simple_fields[3] = &killer.ground_forces;
	simple_fields[4] = &killer.mines;
	for (index = 1U; index < YT_ARRAY_LEN(awards); ++index) {
		if (awards[index] == 0.0f)
			continue;
		if (!session_wait(session, 0.5, "ship salvage wait", error))
			return false;
		emitted = true;
		if (!yt_salvage_simple_row(
		    (enum yt_salvage_simple_kind)(index - 1U), awards[index],
		    row, sizeof(row), &row_length)
		    || !append_news_bytes(session, row, row_length, error)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_LINE, "salvage result row", error))
			return false;
		*simple_fields[index - 1U] = single_add(
		    *simple_fields[index - 1U], awards[index]);
	}
	if (!salvage_save_player(session, killer_record, &killer, error))
		return false;

	requested_holds = awards[0];
	if (single_add(killer.holds, requested_holds)
	    > session->door->game.config.maximum_holds)
		requested_holds = single_sub(
		    session->door->game.config.maximum_holds, killer.holds);
	if (requested_holds > 0.0f) {
		float counter;

		emitted = true;
		if (!yt_game_read_player(&session->door->game, victim_record,
		    &victim, error))
			return false;
		cargo_stock[0] = victim.ore;
		cargo_stock[1] = victim.organics;
		cargo_stock[2] = victim.equipment;
		cargo_remaining = victim.holds;
		for (counter = 1.0f; counter <= requested_holds;
		    counter = single_add(counter, 1.0f)) {
			float one_based;
			float pick;
			float boundary;
			int selected;

			if (!yt_random_one_based_single(
			    &session->door->game.random, cargo_remaining,
			    &one_based, error))
				return false;
			pick = single_sub(one_based, 1.0f);
			if (pick < cargo_stock[0])
				selected = 0;
			else {
				boundary = single_add(cargo_stock[0],
				    cargo_stock[1]);
				if (pick < boundary)
					selected = 1;
				else {
					boundary = single_add(boundary,
					    cargo_stock[2]);
					selected = pick < boundary ? 2 : 3;
				}
			}
			cargo_awards[selected] = single_add(
			    cargo_awards[selected], 1.0f);
			if (selected < 3)
				cargo_stock[selected] = single_sub(
				    cargo_stock[selected], 1.0f);
			cargo_remaining = single_sub(cargo_remaining, 1.0f);
		}
		if (!salvage_load_player(session, killer_record, &killer, error))
			return false;
		for (index = 0U; index < YT_ARRAY_LEN(cargo_awards); ++index)
			killer.holds = single_add(killer.holds,
			    cargo_awards[index]);
		killer.ore = single_add(killer.ore, cargo_awards[0]);
		killer.organics = single_add(killer.organics, cargo_awards[1]);
		killer.equipment = single_add(killer.equipment,
		    cargo_awards[2]);
		if (!salvage_save_player(session, killer_record, &killer, error)
		    || !session_wait(session, 0.5, "ship salvage wait", error))
			return false;
		for (index = 0U; index < YT_ARRAY_LEN(cargo_order); ++index) {
			size_t award = cargo_order[index];

			if ((award == 3U && cargo_awards[award] <= 0.0f)
			    || (award != 3U && cargo_awards[award] == 0.0f))
				continue;
			if (!session_wait(session, 0.5, "ship salvage wait", error)
			    || !yt_salvage_cargo_row(cargo_kind[index],
			    cargo_awards[award], row, sizeof(row), &row_length)
			    || !append_news_bytes(session, row, row_length, error)
			    || !session_present_text(session, row, row_length,
			    SESSION_PRESENT_LINE, "salvage result row", error))
				return false;
		}
	}
	if (!emitted) {
		if (!session_wait(session, 0.5, "ship salvage wait", error)
		    || !append_news_bytes(session, nothing,
		    sizeof(nothing) - 1U, error)
		    || !session_present_text(session, nothing,
		    sizeof(nothing) - 1U, SESSION_PRESENT_LINE,
		    "salvage result row", error))
			return false;
	}
	return session_wait(session, 4.0, "ship salvage wait", error);
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
		xannor_victory_news,
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
	yt_route_process_raw_single(&session->route_process,
	    YT_DESTROYED_ADDRESS, destroyed_raw);
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

static void
direct_attack_store_target(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SHARED_TARGET_RECORD_ADDRESS, raw);
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

static bool
fighter_shield_spill_random(void *context, float *value,
    struct yt_error *error)
{
	return direct_attack_attrition_draw(context, value, error);
}

static void
fighter_shield_spill_store(void *context,
    enum yt_fighter_shield_spill_store_kind kind, double fighters,
    float shields)
{
	struct yt_session *session = context;

	if (kind == YT_FIGHTER_SHIELD_SPILL_STORE_FIGHTERS)
		session_set_process_double(session,
		    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS, fighters);
	else
		session->combat_ship_shields = shields;
}

static bool
fighter_shield_spill(struct yt_session *session, double *fighters,
    float *shields, bool bind_hostile_cells, struct yt_error *error)
{
	const struct yt_fighter_shield_spill_ops ops = {
		fighter_shield_spill_random,
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

static uint16_t
hostile_surrender_selector_address(enum yt_hostile_surrender_sound_kind kind)
{
	switch (kind) {
	case YT_HOSTILE_SURRENDER_RADIO_SOUND:
		return YT_HOSTILE_SURRENDER_RADIO_SELECTOR_ADDRESS;
	case YT_HOSTILE_SURRENDER_XANNOR_SOUND:
		return YT_HOSTILE_SURRENDER_XANNOR_SELECTOR_ADDRESS;
	case YT_HOSTILE_SURRENDER_MERCENARY_SOUND:
		return YT_HOSTILE_SURRENDER_MERCENARY_SELECTOR_ADDRESS;
	case YT_HOSTILE_SURRENDER_JOINED_SOUND:
		return YT_HOSTILE_SURRENDER_JOINED_SELECTOR_ADDRESS;
	default:
		return 0U;
	}
}

static void
hostile_surrender_sound_selector(void *context,
    enum yt_hostile_surrender_sound_kind kind, float selector)
{
	uint16_t address = hostile_surrender_selector_address(kind);

	if (address != 0U)
		session_set_process_single(context, address, selector);
}

static bool
hostile_surrender_sound(void *context,
    enum yt_hostile_surrender_sound_kind kind, float selector,
    struct yt_error *error)
{
	struct yt_session *session = context;
	uint16_t address = hostile_surrender_selector_address(kind);

	(void)selector;
	if (address == 0U)
		return false;
	return session_sound(session, yt_route_process_single(
	    &session->route_process, address), "hostile surrender sound", error);
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

static void
hostile_surrender_cache_forces(void *context, double ship_fighters,
    double deployed_fighters)
{
	struct yt_session *session = context;

	session->combat_ship_fighters = ship_fighters;
	session_set_process_double(session, YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS,
	    deployed_fighters);
}

static void
hostile_surrender_mark_checked(void *context)
{
	session_set_process_single(context, YT_COMPUTER_ROUTE_STATUS_ADDRESS,
	    1.0f);
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
		yt_present_set_bold(&session->presentation, 1.0f);
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

	return session_read_sector(combat->session, sector_number,
	    sector, error);
}

static void
hostile_attack_combat_store_owner(void *context, const uint8_t raw[4])
{
	struct hostile_attack_combat_context *combat = context;

	yt_route_process_set_raw_single(&combat->session->route_process,
	    YT_HOSTILE_ATTACK_OWNER_ADDRESS, raw);
}

static void
hostile_attack_combat_initialize(void *context)
{
	struct hostile_attack_combat_context *combat = context;
	struct yt_route_process *process = &combat->session->route_process;

	yt_route_process_copy_raw_double(process, YT_STATIC_DOUBLE_ZERO_ADDRESS,
	    YT_HOSTILE_ATTACKER_LOSSES_ADDRESS);
	yt_route_process_copy_raw_double(process, YT_STATIC_DOUBLE_ZERO_ADDRESS,
	    YT_HOSTILE_DEFENDER_LOSSES_ADDRESS);
	yt_route_process_copy_raw_single(process, YT_STATIC_SINGLE_ZERO_ADDRESS,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS);
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

static void
hostile_attack_combat_sound_selector(void *context, float selector)
{
	struct hostile_attack_combat_context *combat = context;

	session_set_process_single(combat->session,
	    YT_HOSTILE_ATTACK_SOUND_SELECTOR_ADDRESS, selector);
}

static bool
hostile_attack_combat_sound(void *context, float selector,
    struct yt_error *error)
{
	struct hostile_attack_combat_context *combat = context;

	(void)selector;
	return session_sound(combat->session, yt_route_process_single(
	    &combat->session->route_process,
	    YT_HOSTILE_ATTACK_SOUND_SELECTOR_ADDRESS),
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
hostile_attack_combat_store_quantum(void *context, float quantum)
{
	struct hostile_attack_combat_context *combat = context;

	session_set_process_single(combat->session,
	    YT_HOSTILE_ATTACK_QUANTUM_ADDRESS, quantum);
}

static void
hostile_attack_combat_store_loss(void *context,
    enum yt_hostile_attack_loss_kind kind, double loss)
{
	struct hostile_attack_combat_context *combat = context;
	uint16_t address = kind == YT_HOSTILE_ATTACK_ATTACKER_LOSS
	    ? YT_HOSTILE_ATTACKER_LOSSES_ADDRESS
	    : YT_HOSTILE_DEFENDER_LOSSES_ADDRESS;

	session_set_process_double(combat->session, address, loss);
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
		hostile_surrender_sound_selector,
		hostile_surrender_sound,
		hostile_surrender_prompt,
		hostile_surrender_news,
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
    const struct yt_sector *sector, double deployed_fighters)
{
	struct hostile_attack_combat_context *combat = context;

	*combat->sector = *sector;
	session_set_process_double(combat->session,
	    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS, deployed_fighters);
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
	if (state->mercenaries_hurt) {
		uint8_t raw[4];

		(void)qb_mbf32_encode(-1.0f, raw);
		yt_route_process_set_raw_single(&combat->session->route_process,
		    YT_MERCENARIES_HURT_ADDRESS, raw);
	}
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
		hostile_attack_combat_store_owner,
		hostile_attack_combat_initialize,
		hostile_attack_combat_read_player,
		hostile_attack_combat_sound_selector,
		hostile_attack_combat_sound,
		hostile_attack_combat_random,
		hostile_attack_combat_store_quantum,
		hostile_attack_combat_store_loss,
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

	if (!session_02fc(session, heading, sizeof(heading) - 1U))
		return false;
	cached_ship_fighters = session->combat_ship_fighters;
	admission = yt_hostile_attack_admit((float)cached_ship_fighters, 0.0f);
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
	yt_route_process_set_raw_single(&session->route_process,
	    YT_ATTACK_COMMITMENT_ADDRESS, commitment_raw);
	commitment = yt_route_process_single(&session->route_process,
	    YT_ATTACK_COMMITMENT_ADDRESS);
	admission = yt_hostile_attack_admit((float)cached_ship_fighters,
	    commitment);
	if (admission == YT_HOSTILE_ATTACK_TOO_MANY) {
		if (qb_str_double(available, sizeof(available),
		    cached_ship_fighters) < 0
		    || snprintf(row, sizeof(row), "You only have%s!", available) < 0)
			return false;
		return session_02db(session, (const uint8_t *)row, strlen(row),
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
	return session_02db(context, text, length,
	    "accepted Mercenary Bribe", error);
}

static void
hostile_bribe_accept_sound_selector(void *context, float selector)
{
	session_set_process_single(context, YT_HOSTILE_BRIBE_SOUND_SELECTOR_ADDRESS,
	    selector);
}

static bool
hostile_bribe_accept_sound(void *context, float selector,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)selector;
	return session_sound(session, yt_route_process_single(
	    &session->route_process, YT_HOSTILE_BRIBE_SOUND_SELECTOR_ADDRESS),
	    "accepted bribe sound", error);
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
		hostile_bribe_accept_sound_selector,
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

static void
hostile_bribe_store(void *context, enum yt_hostile_bribe_store_kind kind,
    const uint8_t raw[4])
{
	struct hostile_bribe_context *bribe = context;
	uint16_t address = kind == YT_HOSTILE_BRIBE_STORE_OFFER
	    ? YT_COMPUTER_PATH_MARKER_ADDRESS : YT_ATTACK_COMMITMENT_ADDRESS;

	yt_route_process_set_raw_single(&bribe->session->route_process, address,
	    raw);
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
		hostile_bribe_store,
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
		.owner = yt_route_process_single(&session->route_process,
		    YT_HOSTILE_ATTACK_OWNER_ADDRESS),
		.cached_defenders = session_hostile_deployed_fighters(session),
		.ship_fighters = session->combat_ship_fighters,
		.shields = session->combat_ship_shields,
		.credits = session->player.credits,
		.real_first_name =
		    (const uint8_t *)session->door->identity.real_first,
		.real_first_name_length =
		    strlen(session->door->identity.real_first),
	};
	yt_route_process_raw_single(&session->route_process,
	    YT_HOSTILE_PLANET_LINK_ADDRESS, state.planet_link_raw);
	yt_route_process_raw_single(&session->route_process,
	    YT_MERCENARIES_HURT_ADDRESS, state.mercenaries_hurt_raw);
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
		mine_news,
		mine_random,
		mine_shrink,
		mine_warp,
		mine_set_current,
		mine_style,
		session_store_destroyed,
	};
	bool destroyed = session_is_destroyed(session);
	struct yt_sector_mine_state state = {
		.current_player_record = session_record(session),
		.current_sector = session->player.sector,
		.conversion_mode = session->presentation.sound.conversion_mode,
		.foreground = session_foreground(session),
		.background = yt_present_background(&session->presentation),
		.blink = yt_present_blink(&session->presentation),
		.pager_foreground = session_pager_foreground(session),
		.destroyed = &destroyed,
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
	session_set_foreground(session, 7.0f);
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
		uint8_t scanner_mode_raw[4];

		yt_route_process_raw_single(&session->route_process,
		    YT_POST_LOGIN_SCANNER_MODE_ADDRESS, scanner_mode_raw);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_STATUS_ADDRESS, scanner_mode_raw);
		if (!display_sector(session,
		    qb_mbf32_truth(scanner_mode_raw), error)
		    || !reload_player(session, error))
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
		    yt_route_process_single(&session->route_process,
		    YT_SELF_MINE_SUPPRESSION_ADDRESS))) {
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
		session_set_foreground(session, 6.0f);
		return session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "sector mine success blank", error);
	case YT_DROP_MINES_SUCCESS_ROW:
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
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

static const uint8_t earth_report_seen_one[4] = {0x00, 0x00, 0x00, 0x81};
static const uint8_t earth_report_fallback_zero[4] = {
	0x00, 0x00, 0x01, 0x00
};

static void
session_set_earth_report_seen(struct yt_session *session,
    const uint8_t raw[4])
{
	yt_route_process_set_raw_single(&session->route_process,
	    YT_EARTH_REPORT_SEEN_ADDRESS, raw);
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

	if (physical_record == (uint32_t)session_record(session)) {
		if (!reload_player(session, error))
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
	session_market_bases(session, state.update.base_price);
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

	session_set_foreground(session, foreground);
}

static void
ordinary_commerce_loop_index(void *context, float index)
{
	struct yt_session *session = context;

	session_set_process_single(session, YT_SHARED_LOOP_SCRATCH_ADDRESS,
	    index);
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
	commerce.current_player_record = (uint32_t)session_record(session);
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

	if (!mutate_player_credits(session, -cost, error))
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
	if (session->player.holds >= session->door->game.config.maximum_holds)
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
	yt_present_set_bold(&session->presentation, 1.0f);
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
		int active_count = (int)yt_route_process_single(
		    &session->route_process, YT_SPY_COUNT_ADDRESS);
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
			if (!session_02fc(session, (const uint8_t *)active_row,
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
				    || sector > (float)sector_count(session))
					continue;
				selected = qb_cint(sector, &overflow);
				if (overflow)
					return port_report_failure(error,
					    "Earth Spy sector CINT");
				yt_route_process_set_word(&session->route_process,
				    (uint16_t)(YT_SPY_SECTORS_ADDRESS
				    + 2U * (size_t)(active_count + spy_index)),
				    (int16_t)selected);
				break;
			}
		}
		session_set_process_single(session, YT_SPY_COUNT_ADDRESS,
		    (float)(active_count + quantity));
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

	if (session_foreground(session) != foreground)
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
		.current_record = (float)session_record(session),
		.player_terminal = session_sector_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.player_cache = &session->player_cache,
		.foreground = session_foreground(session),
	};
	bool completed = yt_earth_anti_cloak_run(&state, &ops, session, error);

	if (session_foreground(session) != state.foreground)
		session_set_color(session, (int)state.foreground);
	if (state.field_record != 0.0f)
		session->player.record = state.field_player.record;
	if (state.credit_loaded)
		session->player.credits = state.field_player.credits;
	return completed;
}

static void
clearance_read(void *context, enum yt_clearance_store_kind kind, size_t item,
    uint8_t raw[4])
{
	static const uint16_t discount_address[4] = {
		YT_CLEARANCE_HOLDS_ADDRESS,
		YT_CLEARANCE_FIGHTERS_ADDRESS,
		YT_CLEARANCE_SHIELDS_ADDRESS,
		YT_CLEARANCE_GROUND_ADDRESS,
	};
	struct yt_session *session = context;
	uint16_t address;

	if (raw == NULL)
		return;
	switch (kind) {
	case YT_CLEARANCE_STORE_DISCOUNT:
		if (item >= YT_ARRAY_LEN(discount_address))
			return;
		address = discount_address[item];
		break;
	case YT_CLEARANCE_STORE_ANNOUNCED:
		address = YT_CLEARANCE_ANNOUNCED_ADDRESS;
		break;
	case YT_CLEARANCE_STORE_VALUE:
		address = YT_CLEARANCE_VALUE_ADDRESS;
		break;
	case YT_CLEARANCE_STORE_SOUND_SELECTOR:
		address = YT_CLEARANCE_SOUND_SELECTOR_ADDRESS;
		break;
	default:
		return;
	}
	yt_route_process_raw_single(&session->route_process, address, raw);
}

static void
clearance_store(void *context, enum yt_clearance_store_kind kind, size_t item,
    const uint8_t raw[4])
{
	static const uint16_t discount_address[4] = {
		YT_CLEARANCE_HOLDS_ADDRESS,
		YT_CLEARANCE_FIGHTERS_ADDRESS,
		YT_CLEARANCE_SHIELDS_ADDRESS,
		YT_CLEARANCE_GROUND_ADDRESS,
	};
	struct yt_session *session = context;
	uint16_t address;

	if (raw == NULL)
		return;
	switch (kind) {
	case YT_CLEARANCE_STORE_DISCOUNT:
		if (item >= YT_ARRAY_LEN(discount_address))
			return;
		address = discount_address[item];
		break;
	case YT_CLEARANCE_STORE_ANNOUNCED:
		address = YT_CLEARANCE_ANNOUNCED_ADDRESS;
		break;
	case YT_CLEARANCE_STORE_VALUE:
		address = YT_CLEARANCE_VALUE_ADDRESS;
		break;
	case YT_CLEARANCE_STORE_SOUND_SELECTOR:
		address = YT_CLEARANCE_SOUND_SELECTOR_ADDRESS;
		break;
	default:
		return;
	}
	yt_route_process_set_raw_single(&session->route_process, address, raw);
}

static bool
clearance_random(void *context, float *value, struct yt_error *error)
{
	return random_value(context, value, error);
}

static bool
clearance_present(void *context, const uint8_t *text, size_t length,
    enum yt_clearance_output_kind kind, struct yt_error *error)
{
	const char *operation;

	switch (kind) {
	case YT_CLEARANCE_LEADING_BLANK:
		operation = "clearance leading blank";
		break;
	case YT_CLEARANCE_ANNOUNCEMENT:
		operation = "clearance announcement";
		break;
	case YT_CLEARANCE_TRAILING_BLANK:
		operation = "clearance trailing blank";
		break;
	default:
		return false;
	}
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    operation, error);
}

static bool
clearance_sound(void *context, float selector, struct yt_error *error)
{
	return session_sound(context, selector, "clearance sale sound", error);
}

static bool
clearance(struct yt_session *session, bool create,
    struct yt_error *error)
{
	static const struct yt_clearance_ops ops = {
		clearance_read,
		clearance_store,
		clearance_random,
		clearance_present,
		clearance_sound,
	};
	struct yt_clearance_state state = {.create = create};

	return yt_clearance_run(&state, &ops, session, error);
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

		if (qb_str_single(limit, sizeof(limit), session->door->game.config.lottery_plays) < 0
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
		yt_present_set_bold(&session->presentation, 1.0f);
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
	session_set_pager_line_count(session, 0.0f);
	if (!read_port_at_fault(session, 1, earth,
	    YT_BASIC_FAULT_PORT_EARTH_GET, error))
		return false;
	computer_port_earth_field(earth_state,
	    YT_COMPUTER_PORT_EARTH_FIELD_PORT,
	    session_port_basic_record(session, 1.0f),
	    &earth->record);
	session_set_foreground(session, 3.0f);
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
	discount[0] = yt_route_process_single(&session->route_process,
	    YT_CLEARANCE_HOLDS_ADDRESS);
	discount[1] = yt_route_process_single(&session->route_process,
	    YT_CLEARANCE_FIGHTERS_ADDRESS);
	discount[2] = yt_route_process_single(&session->route_process,
	    YT_CLEARANCE_SHIELDS_ADDRESS);
	discount[3] = yt_route_process_single(&session->route_process,
	    YT_CLEARANCE_GROUND_ADDRESS);
	yt_earth_prices(discount, price);
	if (yt_route_process_single(&session->route_process,
	    YT_EARTH_REPORT_SEEN_ADDRESS) == 0.0f) {
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
	    (uint32_t)session_record(session), &session->player.record);
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
			session_enable_anti_cloak(session);
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

	if (!reload_player(session, error)
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

		if (!reload_player(session, error))
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

	if (!session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	old_garrison = planet.ground_forces;
	if (!reload_player(session, error))
		return false;
	session_set_foreground(session, 6.0f);
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
		if (!session_02fc(session, success, success_length))
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
	    || !reload_player(session, error))
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
		    session_planet_offset(session),
		    (float)logical_planet);
		if (yt_planet_rename_protected(current_record,
		    session_planet_offset(session),
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

			if (!session_02fc(session, success, sizeof(success) - 1U))
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
		    || !session_031f(session, (const uint8_t *)prompt,
		    strlen(prompt), "planet Transfer fighter prompt", error)
		    || !session_036f(session, response, sizeof(response)))
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
	    || !session_read_planet(session, logical_planet,
	    &planet, error))
		return false;
	yt_planet_productivity_planet_overlay(&planet, cache.rate,
	    cache.quantity, cache.contribution);
	if (!session_write_planet(session, logical_planet,
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
	saved_foreground = session_foreground(session);
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
	    (size_t)session_record(session), &session->player.record, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !yt_planet_assault_attack_news(player_name, player_name_length,
	    planet_name, planet_name_length, commitment, row, sizeof(row),
	    &row_length)
	    || !append_news_bytes(session, row, row_length, error))
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
		    || !append_news_bytes(session, defenses_news,
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
	yt_present_set_blink(&session->presentation, 1.0f);
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

	if (!read_sector_at_fault(session, logical_sector, &sector,
	    YT_BASIC_FAULT_ROUTE_SECTOR_GET, error))
		return false;
	session->navigation_field_kind = NAVIGATION_FIELD_ROUTE_SECTOR;
	session->navigation_field_record = (int)session_sector_basic_record(
	    session, (float)logical_sector);
	session->navigation_field = sector.record;
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

	success = yt_route_process_build(start, destination, &status,
	    session->presentation.sound.conversion_mode,
	    &session->route_process, route_sector_reader, session, &outcome,
	    error);
	if (next_hop != NULL)
		for (index = 0U; index < YT_ROUTE_CAPACITY; ++index)
			next_hop[index] = yt_route_process_second(
			    &session->route_process, (int16_t)index);
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

struct projectile_route_cells {
	uint16_t missiles;
	uint16_t destination;
	uint16_t origin;
};

static const struct projectile_route_cells projectile_main_cells = {
	0x4e32U,
	0x4e12U,
	0x1c44U,
};

static const struct projectile_route_cells projectile_xannor_cells = {
	0x5b62U,
	0x5b82U,
	0x4bd8U,
};

static const struct projectile_route_cells projectile_counterlaunch_cells = {
	0x5bc6U,
	0x5bc2U,
	0x5bbeU,
};

static void
route_process_store_single(struct yt_route_process *process, uint16_t address,
    float value)
{
	uint8_t raw[4];

	if (qb_mbf32_encode(value, raw) != QB_MBF_OVERFLOW)
		yt_route_process_set_raw_single(process, address, raw);
}

static void
projectile_route_cells_store(struct yt_session *session,
    const struct projectile_route_cells *cells, float origin,
    float destination, float missiles)
{
	route_process_store_single(&session->route_process, cells->origin,
	    origin);
	route_process_store_single(&session->route_process, cells->destination,
	    destination);
	route_process_store_single(&session->route_process, cells->missiles,
	    missiles);
}

static void
projectile_route_cells_store_raw(struct yt_session *session,
    const struct projectile_route_cells *cells, const uint8_t origin[4],
    const uint8_t destination[4], const uint8_t missiles[4])
{
	yt_route_process_set_raw_single(&session->route_process, cells->origin,
	    origin);
	yt_route_process_set_raw_single(&session->route_process,
	    cells->destination, destination);
	yt_route_process_set_raw_single(&session->route_process, cells->missiles,
	    missiles);
}

static void
projectile_route_cells_load(const struct yt_session *session,
    const struct projectile_route_cells *cells, float *origin,
    float *destination, float *missiles)
{
	*origin = yt_route_process_single(&session->route_process,
	    cells->origin);
	*destination = yt_route_process_single(&session->route_process,
	    cells->destination);
	*missiles = yt_route_process_single(&session->route_process,
	    cells->missiles);
}

static bool
build_route_cells(struct yt_session *session,
    const struct projectile_route_cells *cells, bool use_avoid, bool *found,
    enum yt_route_outcome *route_outcome, float *returned_status,
    struct yt_error *error)
{
	float status = use_avoid ? 1.0f : 0.0f;
	enum yt_route_outcome outcome;
	bool success;

	success = yt_route_process_build_cells(cells->origin,
	    cells->destination, &status,
	    session->presentation.sound.conversion_mode,
	    &session->route_process, route_sector_reader, session, &outcome,
	    error);
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
			    || !append_news_bytes(session, row, row_length, error))
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
	    || !reload_player(session, error))
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
	    || !session_02fc(session, row, row_length)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "planet Thrusters route leading blank", error)
	    || qb_str_single(number, sizeof(number), start) < 0
	    || !session_031f(session, (const uint8_t *)number, strlen(number),
	    "planet Thrusters route start", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = yt_route_process_second(&session->route_process,
		    (int16_t)cursor);
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
		if (!session_031f(session, (const uint8_t *)number,
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
	    || !session_0317(session, row, row_length,
	    "planet Thrusters distance summary", error)
	    || !computer_prompt_hydrate(session, error))
		return false;
	if (cost > session->player.turns) {
		return session_02db(session, insufficient,
		    sizeof(insufficient) - 1U,
		    "planet Thrusters insufficient turns", error);
	}
	if (!yt_planet_move_turns_row(session->player.turns, row, sizeof(row),
	    &row_length)
	    || !session_02fc(session, row, row_length)
	    || !session_a8d2(session, confirmation, sizeof(confirmation) - 1U,
	    &answer, error))
		return false;
	if (answer != YT_YES_NO_YES)
		return true;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_0317(session, engaged, sizeof(engaged) - 1U,
	    "planet Thrusters engaged", error))
		return false;
	session_set_pager_line_count(session, 0.0f);
	if (!session_031f(session, moving, sizeof(moving) - 1U,
	    "planet Thrusters moving prefix", error))
		return false;
	cursor = start_node;
	for (;;) {
		int next = yt_route_process_second(&session->route_process,
		    (int16_t)cursor);
		int number_length;

		if (next == 0)
			break;
		number_length = qb_str_single(number, sizeof(number),
		    (float)next);
		if (number_length < 0 || !session_031f(session,
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

	session_set_process_single(session, YT_PLANET_RECORD_SCRATCH_ADDRESS,
	    single_add(session_planet_offset(session),
	    (float)logical_planet));
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
	if (!session_read_sector(session,
	    (int)session->player.sector, &sector, error))
		return false;
	session_set_process_single(session, YT_SHARED_LOOP_SCRATCH_ADDRESS,
	    sector.planet);
	if (sector.planet == 0.0f) {
		bool created = create_planet(session, error);

		if (created && enter_sector != NULL)
			*enter_sector = true;
		return created;
	}
	session_set_foreground(session, 6.0f);
	if (!session_0317(session, landing, sizeof(landing) - 1U,
	    "planet landing progress", error))
		return false;
	planet_record_value = session_planet_offset(session)
	    + sector.planet;
	session_set_process_single(session, YT_PLANET_RECORD_SCRATCH_ADDRESS,
	    planet_record_value);
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
team_load_raw(struct yt_session *session, float id, struct yt_team *team,
    struct yt_error *error)
{
	struct yt_team_loader_state loader = {
		.team_id = id,
		.current_player_record = (float)session_record(session),
		.sector_record_offset = session_sector_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cache = &session->team_cache,
	};
	size_t index;

	if (team != NULL) {
		memset(team, 0, sizeof(*team));
		team->id = (int)id;
	}
	if (!yt_team_loader_run(&loader, session_read_physical_record, session,
	    error))
		return false;
	if (team == NULL)
		return true;
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
team_load(struct yt_session *session, int id, struct yt_team *team,
    struct yt_error *error)
{
	return team_load_raw(session, (float)id, team, error);
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
team_audit_clock_adapter(void *context, enum yt_team_audit_clock_kind kind,
    uint8_t *text, size_t capacity, size_t *length, struct yt_error *error)
{
	struct yt_clock_value now;
	char date[11];
	char time_text[9];
	const char *source;
	size_t source_length;

	(void)context;
	if (text == NULL || length == NULL || !yt_platform_clock(&now, error))
		return false;
	if (kind == YT_TEAM_AUDIT_DATE) {
		yt_format_date(&now, date);
		source = date;
		source_length = sizeof(date) - 1U;
	} else {
		yt_format_time(&now, time_text);
		source = time_text;
		source_length = sizeof(time_text) - 1U;
	}
	if (source_length > capacity)
		return false;
	memcpy(text, source, source_length);
	*length = source_length;
	return true;
}

static bool
team_audit_load_adapter(void *context, float team_id,
    struct yt_error *error)
{
	struct yt_session *session = context;

	return team_load_raw(session, team_id, NULL, error);
}

static bool
team_audit_write_adapter(void *context, const uint8_t *text, size_t length,
    const uint8_t sender_raw[4], const uint8_t recipient_raw[4],
    struct yt_error *error)
{
	(void)context;
	return radio_append_raw_bytes(text, length, sender_raw, recipient_raw,
	    error);
}

static void
team_audit_store_adapter(void *context, enum yt_team_audit_store_kind kind,
    const uint8_t raw[4])
{
	struct yt_session *session = context;
	uint16_t address = kind == YT_TEAM_AUDIT_STORE_LOOP_COUNTER
	    ? YT_TEAM_AUDIT_LOOP_ADDRESS : YT_TEAM_AUDIT_SENDER_ADDRESS;

	yt_route_process_set_raw_single(&session->route_process, address, raw);
}

static bool
team_audit(struct yt_session *session, float team_id, float event,
    const char *attempt, struct yt_error *error)
{
	static const struct yt_team_audit_ops ops = {
		.clock = team_audit_clock_adapter,
		.load_team = team_audit_load_adapter,
		.write_radio = team_audit_write_adapter,
		.store = team_audit_store_adapter,
	};
	struct yt_team_audit_state state = {
		.team_id = team_id,
		.event_type = event,
		.current_player_record = (float)session_record(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.current_player_name = (const uint8_t *)session->player.name,
		.current_player_name_length = strlen(session->player.name),
		.attempted_password = (const uint8_t *)attempt,
		.attempted_password_length = strlen(attempt),
		.message = session->team_audit_message,
		.message_capacity = sizeof(session->team_audit_message),
		.message_length = session->team_audit_message_length,
		.cache = &session->team_cache,
	};
	bool result;

	yt_route_process_raw_single(&session->route_process,
	    YT_TEAM_AUDIT_LOOP_ADDRESS, state.loop_counter_raw);
	yt_route_process_raw_single(&session->route_process,
	    YT_TEAM_AUDIT_SENDER_ADDRESS, state.sender_raw);
	result = yt_team_audit_run(&state, &ops, session, error);
	session->team_audit_message_length = state.message_length;
	return result;
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

static void
info_team_store_id(void *context, const uint8_t raw[4])
{
	static const uint8_t captain_flag_zero[4] = {
		0x00U, 0x00U, 0xA0U, 0x00U,
	};
	struct yt_session *session = context;

	(void)raw;
	memcpy(session->team_cache.captain_flag_raw, captain_flag_zero,
	    sizeof(session->team_cache.captain_flag_raw));
	session->team_cache.captain_flag = 0.0f;
	session->team_cache.raw_valid = true;
}

static void
info_team_store_captain(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SHARED_TARGET_RECORD_ADDRESS, raw);
}

static void
info_team_promote_cache(void *context, const uint8_t current_record_raw[4])
{
	static const uint8_t captain_flag_true[4] = {
		0x00U, 0x00U, 0x80U, 0x81U,
	};
	struct yt_session *session = context;

	memcpy(session->team_cache.captain_raw, current_record_raw,
	    sizeof(session->team_cache.captain_raw));
	session->team_cache.captain = qb_mbf32_decode(current_record_raw);
	memcpy(session->team_cache.captain_flag_raw, captain_flag_true,
	    sizeof(session->team_cache.captain_flag_raw));
	session->team_cache.captain_flag = -1.0f;
	session->team_cache.raw_valid = true;
}

static bool
info_team_load_team(void *context, float team_id, float current_record,
    float *captain_flag, struct yt_team *team, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_team_loader_state loader = {
		.team_id = team_id,
		.current_player_record = current_record,
		.sector_record_offset = session_sector_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
		.cache = &session->team_cache,
	};
	size_t index;

	memset(team, 0, sizeof(*team));
	team->id = (int)team_id;
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
	uint32_t physical = session_sector_basic_record(session, team_id);

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
	uint32_t physical = session_sector_basic_record(session, team_id);

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
		info_team_store_id,
		info_team_store_captain,
		info_team_promote_cache,
		info_team_load_team,
		info_team_read_overlay,
		info_team_write_overlay,
		info_team_present,
	};
	struct yt_info_team_state state = {
		.current_record = (float)session_record(session),
		.sector_offset = session_sector_offset(session),
		.conversion_mode = session->presentation.sound.conversion_mode,
	};

	if (qb_mbf32_encode((float)session_record(session),
	    state.current_record_raw) != QB_MBF_OK)
		return false;

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
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, selected);
	if (!write_player(session, error)
	    || !team_read_overlay(session, id, &team, error))
		return false;
	team.id = id;
	team.captain = (float)session_record(session);
	team.roster[0] = (float)session_record(session);
	team.roster[1] = 0.0f;
	team.roster[2] = 0.0f;
	team.roster[3] = 0.0f;
	yt_record_set_number_if_changed(&team.overlay.record, YT_F77,
	    team.captain);
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

		if (!team_read_overlay(session, selected, &ignored, error))
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
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	yt_team_membership_apply_player(&session->player, (float)selected);
	if (!write_player(session, error))
		return false;
	for (index = 0; index < 4; ++index) {
		if (team.roster[index] == 0.0f) {
			team.roster[index] = (float)session_record(session);
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
	if (!yt_game_read_player(&session->door->game, session_record(session),
	    &session->player, error))
		return false;
	old_team = session->player.team;
	yt_team_membership_apply_player(&session->player, 0.0f);
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
		if (team->roster[index] == (float)session_record(session))
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
		if (!team_store_inactive(session, team, error))
			return false;
	}
	session_set_foreground(session, 6.0f);
	if (!session_0317(session, success, sizeof(success) - 1U,
	    "team quit success row", error)
	    || !team_audit(session, old_team, 2.0f, "", error))
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

	if (!session_0317(session, locating, sizeof(locating) - 1U,
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
		for (logical_sector = 1; logical_sector <= sector_count(session);
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
	if (!session_read_sector(session, logical_sector,
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
		if (!reload_player(session, error))
			return false;
		yt_team_transfer_apply_player(&session->player, amount);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &session->player.record, error))
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
		    || team->roster[index] == (float)session_record(session))
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
		yt_team_loader_cache_sync_raw(&session->team_cache);
		if (!session_read_sector(session, team_id,
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

		session_set_foreground(session, 6.0f);
		if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
		    "team front leading blank", error)
		    || !info_team_lines(session, &team, &captain, error))
			return false;
		session_set_pager_line_count(session, 0.0f);
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
		.current_player_record = (float)session_record(session),
		.port_offset = session_port_offset(session),
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
		yt_present_set_bold(&session->presentation, 1.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
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

	if (player_record != session_record(session)
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
	uint8_t raw[4];
	uint16_t address;

	if (!yt_treasury_caller_binding(caller, &address, raw))
		return false;
	yt_route_process_set_raw_single(&session->route_process, address, raw);
	yt_route_process_raw_single(&session->route_process, address,
	    state.collecting_raw);
	return yt_treasury_run(&state, &ops, session, error);
}

static bool
genesis_hydrate(void *context, int player_record, struct yt_player *player,
    struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
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
		yt_present_set_bold(&session->presentation, 1.0f);
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

struct genesis_handoff_context {
	struct yt_session *session;
	struct yt_text_output output;
	uint8_t line[sizeof(((struct yt_door *)0)->command_line) + 2U];
	size_t line_length;
};

static bool
genesis_handoff_close_file5(void *context, struct yt_error *error)
{
	(void)context;
	return session_close_file5(error);
}

static bool
genesis_handoff_open_output(void *context, struct yt_error *error)
{
	struct genesis_handoff_context *handoff = context;
	bool opened;

	opened = yt_text_output_open(&handoff->output, "RMTINIT.TMP", error);
	if (!opened && handoff->output.last_output_open.basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_OPEN_OUTPUT,
		    handoff->output.last_output_open.basic_error);
	return opened;
}

static bool
genesis_handoff_print_command(void *context, struct yt_error *error)
{
	struct genesis_handoff_context *handoff = context;
	bool printed;

	printed = yt_text_output_write(&handoff->output, handoff->line,
	    handoff->line_length, error);
	if (!printed && handoff->output.last_write.basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_PRINT_VALUE,
		    handoff->output.last_write.basic_error);
	return printed;
}

static bool
genesis_handoff_close_all(void *context, struct yt_error *error)
{
	struct genesis_handoff_context *handoff = context;
	struct yt_session *session = handoff->session;
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
		yt_text_output_close_all_method, &handoff->output};
	closed = yt_close_all_run(controls, control_count, NULL, &close_all,
	    error);
	if (closed)
		return true;
	if (close_all.failed_index == output_index)
		basic_error = handoff->output.last_close.basic_error;
	else if (close_all.failed_index == game_index)
		basic_error = session->door->game.database.last_close.basic_error;
	if (basic_error != 0U)
		(void)yt_error_attach_basic_fault_number(error,
		    YT_BASIC_FAULT_GENESIS_CLOSE_ALL, basic_error);
	return false;
}

static bool
genesis_handoff_run_program(void *context, struct yt_error *error)
{
	struct genesis_handoff_context *handoff = context;
	struct yt_session *session = handoff->session;
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
	static const struct yt_genesis_handoff_ops ops = {
		genesis_handoff_close_file5,
		genesis_handoff_open_output,
		genesis_handoff_print_command,
		genesis_handoff_close_all,
		genesis_handoff_run_program,
	};
	struct yt_session *session = context;
	struct genesis_handoff_context handoff;
	struct yt_genesis_handoff_state state;
	bool result;

	memset(&handoff, 0, sizeof(handoff));
	handoff.session = session;
	handoff.line_length = strlen(session->door->command_line) + 2U;
	memcpy(handoff.line, session->door->command_line,
	    handoff.line_length - 2U);
	handoff.line[handoff.line_length - 2U] = '\r';
	handoff.line[handoff.line_length - 1U] = '\n';
	yt_text_output_init(&handoff.output);
	result = yt_genesis_handoff_run(&state, &ops, &handoff, error);
	yt_text_output_destroy(&handoff.output);
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
	struct yt_session *session = context;

	/* The fighter model assigns through its by-reference carrier first. */
	yt_present_set_bold(&session->presentation,
	    session->presentation.bold);
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

	yt_route_process_copy_raw_single(&session->route_process,
	    YT_FOREGROUND_ADDRESS, YT_PLASMA_PLAYER_SAVED_FOREGROUND_ADDRESS);
	if (saved_foreground != NULL)
		*saved_foreground = yt_route_process_single(&session->route_process,
		    YT_PLASMA_PLAYER_SAVED_FOREGROUND_ADDRESS);
}

static void
plasma_player_color(void *context, float foreground)
{
	session_set_foreground(context, foreground);
}

static void
plasma_player_sound_selector(void *context, float selector)
{
	session_set_process_single(context,
	    YT_PLASMA_PLAYER_SOUND_SELECTOR_ADDRESS, selector);
}

static bool
plasma_player_sound(void *context, float selector, struct yt_error *error)
{
	struct yt_session *session = context;

	(void)selector;
	return session_sound(session, yt_route_process_single(
	    &session->route_process, YT_PLASMA_PLAYER_SOUND_SELECTOR_ADDRESS),
	    "plasma player-attack sound", error);
}

static void
plasma_player_restore_foreground(void *context, float saved_foreground)
{
	struct yt_session *session = context;
	uint8_t raw[4];

	(void)saved_foreground;
	yt_route_process_raw_single(&session->route_process,
	    YT_PLASMA_PLAYER_SAVED_FOREGROUND_ADDRESS, raw);
	session_set_foreground_raw(session, raw);
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
	return salvage_player(context, victim, shooter, error);
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
		projectile_damage_draw,
		cruise_defense_damage_present,
		cruise_defense_news,
		cruise_defense_read_sector,
		cruise_defense_write_sector,
		cruise_defense_victory,
		session_store_xannor_provoker,
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
		    projectile_damage_draw, session, &damage, error))
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
				    || !salvage_player(session, basic,
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
		plasma_fighter_news,
		plasma_planet_sound,
		projectile_damage_draw,
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
		plasma_player_save_foreground,
		plasma_player_color,
		plasma_player_sound_selector,
		plasma_player_sound,
		projectile_damage_draw,
		plasma_fighter_news,
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
		session_store_destroyed,
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
			bool destroyed = session_is_destroyed(session);

			memset(&killed, 0, sizeof(killed));
			killed.victim = basic;
			killed.shooter = session_record(session);
			killed.sector = sector_number;
			killed.energy = energy;
			killed.blink = &session->presentation.blink;
			killed.destroyed = &destroyed;
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
	struct plasma_opening_context opening = {
		.session = session,
	};
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
	const struct projectile_route_cells *cells;
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
	success = build_route_cells(session, route_context->cells, false, &found,
	    &outcome, status, error);
	*origin = yt_route_process_single(&session->route_process,
	    route_context->cells->origin);
	*destination = yt_route_process_single(&session->route_process,
	    route_context->cells->destination);
	return success;
}

static int16_t
plasma_route_read(void *context, int16_t index)
{
	struct plasma_route_context *route_context = context;

	return yt_route_process_second(&route_context->session->route_process,
	    index);
}

static void
plasma_route_write(void *context, int16_t index, int16_t value)
{
	struct plasma_route_context *route_context = context;

	yt_route_process_set_second(&route_context->session->route_process,
	    index, value);
}

static void
plasma_route_arguments_changed(void *context, float origin,
    float destination, enum yt_projectile_plasma_argument_change change)
{
	static const uint8_t same_origin_zero[4] = {0, 0, 0x60, 0};
	struct plasma_route_context *route_context = context;
	struct yt_route_process *process =
	    &route_context->session->route_process;

	if (change == YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO)
		yt_route_process_set_raw_single(process,
		    route_context->cells->origin, same_origin_zero);
	else if (change == YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN)
		route_process_store_single(process, route_context->cells->origin,
		    origin);
	else if (change == YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION)
		route_process_store_single(process,
		    route_context->cells->destination, destination);
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
    bool plasma, const struct projectile_route_cells *cells,
    float *origin_alias, const uint8_t origin_raw[4],
    const uint8_t target_raw[4], const uint8_t amount_raw[4],
    int *pending_counterattack, int *pending_xannor, struct yt_error *error)
{
	static const uint8_t ordinary_plasma_attribution[4] = {2, 0, 0, 0};
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
	uint8_t destination_raw[4];
	int *counterattack = pending_counterattack != NULL
	    ? pending_counterattack : &local_counterattack;
	int *xannor_provoker = pending_xannor != NULL
	    ? pending_xannor : &local_xannor_provoker;
	float last_mine_news_sector;
	int start = (int)(origin_alias != NULL
	    ? *origin_alias : session->player.sector);

	if (origin_raw != NULL && target_raw != NULL && amount_raw != NULL)
		projectile_route_cells_store_raw(session, cells, origin_raw,
		    target_raw, amount_raw);
	else
		projectile_route_cells_store(session, cells, *origin_alias, *target,
		    *missiles);
	yt_route_process_raw_single(&session->route_process, cells->destination,
	    destination_raw);
	if (plasma)
		yt_route_process_set_raw_single(&session->route_process, 0x72a0U,
		    ordinary_plasma_attribution);
	(void)qb_cint_mbf32(destination_raw,
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
			cells,
			xannor_provoker,
			attacker,
			attacker_length,
		};

		struct yt_projectile_plasma_route_state state = {
			origin,
			target,
			&energy,
			hop_loss,
			{session_disruption_sector(session, 0U),
			 session_disruption_sector(session, 1U)},
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

		bool route_success = build_route_cells(session, cells,
		    yt_projectile_route_avoid_enabled(plasma, *counterattack,
		    session_record(session)), &found, &route_outcome, &route_status,
		    error);

		projectile_route_cells_load(session, cells, origin_alias, target,
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
			int next = yt_route_process_second(&session->route_process,
			    (int16_t)cursor);

			if (!yt_projectile_route_has_next((int16_t)next))
				break;
			if (session_is_disruption_sector(session, (float)next)) {
				float local_origin = (float)start;
				float local_destination = destination;
				static const struct yt_projectile_cruise_reroute_ops
				    cruise_ops = {
					cruise_reroute_line,
					cruise_reroute_attention,
					cruise_reroute_random,
				};
				struct yt_projectile_cruise_reroute_state state = {
					(float)next,
					session_sector_offset(session),
					session_port_offset(session),
					origin_alias != NULL ? origin_alias : &local_origin,
					&local_destination,
				};

				bool reroute_success =
				    yt_projectile_cruise_reroute_run(&state,
				    &cruise_ops, session, error);

				*origin_alias = *state.origin;
				*target = *state.destination;
				projectile_route_cells_store(session, cells,
				    *origin_alias, *target, *missiles);
				if (!reroute_success)
					return false;
				start = (int)*state.origin;
				destination = *state.destination;
				rerouted = true;
				break;
			}
			struct yt_projectile_union_police_state police = {
				(float)next,
				destination,
				*counterattack,
				*xannor_provoker,
				false,
			};

			if (!yt_projectile_union_police_run(&police,
			    cruise_union_police_present, session, error))
				return false;
			if (police.intercepted)
				return true;
			enum missile_sector_route sector_route;

			bool sector_success = missile_sector(session, next, missiles,
			    counterattack, xannor_provoker, &last_mine_news_sector,
			    &sector_route, error);

			route_process_store_single(&session->route_process,
			    cells->missiles, *missiles);
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
	const struct projectile_route_cells *cells = session_record(session) == -1
	    ? &projectile_xannor_cells : &projectile_main_cells;
	bool result;

	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	result = launch_projectile(session, target, amount, plasma, cells, origin,
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
	    &projectile_main_cells, origin, origin_raw, target_raw, amount_raw,
	    counterattack, xannor_provoker, error);

	yt_route_process_raw_single(&session->route_process,
	    projectile_main_cells.origin, origin_raw);
	yt_route_process_raw_single(&session->route_process,
	    projectile_main_cells.destination, target_raw);
	yt_route_process_raw_single(&session->route_process,
	    projectile_main_cells.missiles, amount_raw);
	*origin = qb_mbf32_decode(origin_raw);
	*target = qb_mbf32_decode(target_raw);
	*amount = qb_mbf32_decode(amount_raw);
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
session_xannor_wait(void *context, const uint8_t duration_raw[4],
    struct yt_error *error)
{
	return session_wait_raw(context, duration_raw,
	    "Xannor retaliation wait", error);
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
		session_store_destroyed,
		session_store_current_player_record,
		session_store_xannor_provoker,
	};
	bool destroyed = session_is_destroyed(session);
	uint8_t current_record_raw[4];

	session_load_xannor_provoker(session, provoking_player);
	session->player_record_carrier = session_record(session);
	if (qb_mbf32_encode((float)session_record(session), current_record_raw)
	    != QB_MBF_OK)
		return false;
	struct yt_xannor_retaliation_state state = {
		&session->player,
		&session->player_record_carrier,
		&session->player_cache,
		&destroyed,
		provoking_player,
		&session->door->game.config.headquarters,
		sector_count(session),
		current_record_raw,
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
session_counterlaunch_projectile(void *context, float *origin, float *target,
    float *amount, bool plasma, int *counterattack, int *xannor_provoker,
    struct yt_error *error)
{
	struct yt_session *session = context;
	bool result;

	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	result = launch_projectile(session, target, amount, plasma,
	    &projectile_counterlaunch_cells, origin, NULL, NULL, NULL,
	    counterattack, xannor_provoker, error);
	session_load_counterattack_player(session, counterattack);
	session_load_xannor_provoker(session, xannor_provoker);
	return result;
}

static bool
session_counterlaunch_wait(void *context, const uint8_t duration_raw[4],
    struct yt_error *error)
{
	return session_wait_raw(context, duration_raw,
	    "player counterattack wait", error);
}

static void
session_counterlaunch_store_count(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COUNTERLAUNCH_COUNT_ADDRESS, raw);
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
		session_counterlaunch_store_count,
		session_store_destroyed,
		session_store_current_player_record,
		session_store_counterattack_player,
	};
	bool destroyed = session_is_destroyed(session);
	float retained_count = yt_route_process_single(&session->route_process,
	    YT_COUNTERLAUNCH_COUNT_ADDRESS);
	uint8_t current_record_raw[4];

	session_load_counterattack_player(session, counterattacker);
	session->player_record_carrier = session_record(session);
	if (qb_mbf32_encode((float)session_record(session), current_record_raw)
	    != QB_MBF_OK)
		return false;
	struct yt_counterlaunch_state state = {
		&session->player,
		&session->player_record_carrier,
		&session->player_cache,
		&destroyed,
		&retained_count,
		counterattacker,
		xannor_provoker,
		(int)session_sector_offset(session),
		current_record_raw,
	};

	return yt_counterlaunch_run(&state, &ops, session, error);
}

static bool
projectile_command_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
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
projectile_command_destroyed_truth(void *context)
{
	return session_is_destroyed(context);
}

static bool
projectile_command_counterattack_truth(void *context)
{
	struct yt_session *session = context;
	uint8_t raw[4];

	yt_route_process_raw_single(&session->route_process,
	    YT_COUNTERATTACK_PLAYER_ADDRESS, raw);
	return qb_mbf32_truth(raw);
}

static bool
projectile_command_xannor_truth(void *context)
{
	struct yt_session *session = context;
	uint8_t raw[4];

	yt_route_process_raw_single(&session->route_process,
	    YT_XANNOR_PROVOKER_ADDRESS, raw);
	return qb_mbf32_truth(raw);
}

static void
projectile_command_store_turn_gate_result(void *context,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, raw);
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
		session_store_destroyed,
		projectile_command_destroyed_truth,
		projectile_command_counterattack_truth,
		projectile_command_xannor_truth,
		projectile_command_store_turn_gate_result,
	};
	bool destroyed = session_is_destroyed(session);
	struct yt_projectile_command_state state = {
		.current_player_record = session_record(session),
		.maximum_sector = (float)sector_count(session),
		.plasma = plasma,
		.displayed = plasma ? session->player.plasma
		    : session->player.missiles,
		.destroyed = &destroyed,
	};

	yt_route_process_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, state.turn_gate_result_raw);

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
radio_send_news(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return append_news_bytes(context, text, length, error);
}

static bool
radio_send_record(void *context, const uint8_t *text, size_t length,
    float sender, float recipient, struct yt_error *error)
{
	(void)context;
	return radio_append_bytes(text, length, sender, recipient, error);
}

static bool
radio_send_success(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	static const uint8_t success[] = "Transmission successful!";

	(void)error;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_02fc(session, success, sizeof(success) - 1U);
}

static bool
radio_compose(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_radio_send_ops send_ops = {
		radio_send_news,
		radio_send_record,
		radio_send_success,
	};
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
		struct yt_radio_team_target_state team_target;
		static const uint8_t teamless[] =
		    "You Don't belong to a team!";

		if (!reload_player(session, error))
			return false;
		team_target = (struct yt_radio_team_target_state){
			.raw_team_id = session->player.team,
			.current_player_record = (float)session_record(session),
			.sector_record_offset =
			    session_sector_offset(session),
			.conversion_mode =
			    session->presentation.sound.conversion_mode,
			.cache = &session->team_cache,
		};
		if (!yt_radio_team_target_run(&team_target,
		    session_read_physical_record, session, error))
			return false;
		if (team_target.teamless) {
			return session_02db(session, teamless,
			    sizeof(teamless) - 1U, "radio teamless row", error);
		}
		for (index = 0; index < 4; ++index)
			recipients[index] = team_target.recipients[index];
		recipient_count = (int)team_target.recipient_count;
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
		if (!session_02fc(session, row, row_length))
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
							if (!session_02db(session,
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
						if (!session_02db(session,
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
			    || !session_031f(session, menu_prompt,
			    sizeof(menu_prompt) - 1U, "radio menu prompt", error)
			    || !session_0357(session, choice, sizeof(choice)))
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
	{
		struct yt_radio_send_state state = {
			.recipient_count = (size_t)recipient_count,
			.sender = (float)session_record(session),
			.sender_name = session->cached_player_name,
			.sender_name_length = session->cached_player_name_length,
			.line_count = (size_t)line_count,
		};
		int body;

		for (index = 0; index < recipient_count; ++index)
			state.recipients[index] = recipients[index];
		for (body = 0; body < line_count; ++body) {
			state.lines[body].data = (const uint8_t *)lines[body];
			state.lines[body].length = strlen(lines[body]);
		}
		return yt_radio_send_run(&state, &send_ops, session, error);
	}
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
	static const uint8_t marker_entry_raw[4] = {0x00, 0x3c, 0x1c, 0x8e};
	static const uint8_t marker_success_raw[4] = {0x00, 0x00, 0x1c, 0x00};
	static const uint8_t status_one_raw[4] = {0x00, 0x00, 0x00, 0x81};
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
	bool stale_marker = autopilot
	    && yt_route_process_single(&session->route_process,
	    YT_COMPUTER_PATH_MARKER_ADDRESS) == 9999.0f;
	int start;
	int destination;
	int count = sector_count(session);
	bool conversion_overflow;
	bool found;
	int cursor;
	enum yt_route_outcome route_outcome;

	session->navigation_field_active = true;
	session->navigation_field_kind = NAVIGATION_FIELD_ENTRY_PLAYER;
	session->navigation_field_record = session_record(session);
	session->navigation_field = session->player.record;

	if (!autopilot) {
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_PATH_MARKER_ADDRESS, marker_entry_raw);
		if (!session_present_text(session, NULL, 0,
		    SESSION_PRESENT_LINE, "path start blank", error)
		    || !session_031f(session, start_prompt,
		    sizeof(start_prompt) - 1U, "path start prompt", error)
		    || !session_036f(session, response, sizeof(response)))
			return false;
		if (response[0] == '\0')
			return true;
		if (!yt_computer_path_parse(response, &start_value, parsed_raw,
		    error))
			return false;
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_START_ADDRESS, parsed_raw);
	}
	else if (!stale_marker) {
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_START_ADDRESS,
		    session->player.record.bytes + YT_F57);
	}
	start_value = yt_route_process_single(&session->route_process,
	    YT_COMPUTER_ROUTE_START_ADDRESS);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path destination blank", error)
	    || !session_031f(session, destination_prompt,
	    sizeof(destination_prompt) - 1U, "path destination prompt", error)
	    || !session_036f(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		return true;
	if (!yt_computer_path_parse(response, &destination_value, parsed_raw,
	    error))
		return false;
	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_DESTINATION_ADDRESS, parsed_raw);
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
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "path working blank", error)
	    || !session_031f(session, working, sizeof(working) - 1U,
	    "path working prompt", error))
		return false;
	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, status_one_raw);
	if (!yt_route_process_build_at(YT_COMPUTER_ROUTE_START_ADDRESS,
	    YT_COMPUTER_ROUTE_DESTINATION_ADDRESS,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS,
	    session->presentation.sound.conversion_mode,
	    &session->route_process, route_sector_reader, session,
	    &route_outcome, error))
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
		    || !session_02fc(session, (const uint8_t *)heading,
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
	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_PATH_HOPS_ADDRESS, hop_count_raw);
	{
		char number[64];

		if (qb_str_single(number, sizeof(number), (float)start) < 0
		    || !session_031f(session, (const uint8_t *)number,
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
		next = yt_route_process_second(&session->route_process,
		    (int16_t)display_index);
		if (next == 0)
			break;
		cursor = next;
		if (qb_str_single(number, sizeof(number), (float)cursor) < 0
		    || snprintf(token, sizeof(token), "%s%s", number,
		    cursor == destination ? "" : ",") < 0
		    || !session_031f(session, (const uint8_t *)token,
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
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_PATH_HOPS_ADDRESS, hop_count_raw);
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
		    || !session_0317(session, (const uint8_t *)course,
		    strlen(course), "path course row", error))
			return false;
	}
	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_PATH_MARKER_ADDRESS, marker_success_raw);
	if (!autopilot || stale_marker)
		return true;
	if (!reload_player(session, error))
		return false;
	session->navigation_field_kind = NAVIGATION_FIELD_INNER_PLAYER;
	session->navigation_field_record = session_record(session);
	session->navigation_field = session->player.record;
	if (hop_count > session->player.turns) {
		if (!session_02db(session, insufficient,
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
		    || !session_02fc(session, (const uint8_t *)row, strlen(row))
		    || !session_a8d2(session, confirmation,
		    sizeof(confirmation) - 1U, &answer, error))
			return false;
		if (answer == YT_YES_NO_YES) {
			if (!session_0317(session, engaged, sizeof(engaged) - 1U,
			    "autopilot engaged row", error)
			    || !session_0317(session, stop_notice,
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
		session->navigation_field_kind = NAVIGATION_FIELD_FINAL_SECTOR;
		session->navigation_field_record =
		    (int)session_sector_basic_record(session,
		    session->player.sector);
		session->navigation_field = current_sector.record;
		for (index = 0; index < 6U; ++index) {
			yt_route_process_set_raw_single(&session->route_process,
			    (uint16_t)(YT_CURRENT_WARPS_ADDRESS + index * 4U),
			    current_sector.record.bytes + YT_F41 + index * 4U);
		}
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
		if (!session_read_sector(session, (int)selected, &sector, error))
			return false;
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_PLANET_LINK_ADDRESS,
		    sector.record.bytes + YT_F93);
		link = yt_route_process_single(&session->route_process,
		    YT_COMPUTER_PLANET_LINK_ADDRESS);
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

			session_set_process_double(session,
			    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS,
			    (double)sector.fighters);
			yt_route_process_set_raw_single(&session->route_process,
			    YT_SHARED_TARGET_RECORD_ADDRESS,
			    sector.record.bytes + YT_F85);
			fighter_owner = yt_route_process_single(&session->route_process,
			    YT_SHARED_TARGET_RECORD_ADDRESS);
			if (!computer_port_friendship(session, fighter_owner,
			    &fighter_friendly, error))
				return false;
			sector_fighters = yt_route_process_double(
			    &session->route_process,
			    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS);
			last_relationship = yt_route_process_single(
			    &session->route_process,
			    YT_FRIENDSHIP_RELATION_ADDRESS);
			scratch = single_add(session_planet_offset(session), link);
			session_set_process_single(session,
			    YT_COMPUTER_PLANET_LINK_ADDRESS, scratch);
			session_set_process_single(session,
			    YT_PLANET_RECORD_SCRATCH_ADDRESS, scratch);
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
				last_relationship = yt_route_process_single(
				    &session->route_process,
				    YT_FRIENDSHIP_RELATION_ADDRESS);
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
					return session_0317(session, row, length,
					    "computer planet limited row", error);
				}
			}
		}
		else {
			sector_fighters = yt_route_process_double(
			    &session->route_process,
			    YT_HOSTILE_DEPLOYED_FIGHTERS_ADDRESS);
			fighter_owner = yt_route_process_single(&session->route_process,
			    YT_SHARED_TARGET_RECORD_ADDRESS);
			last_relationship = yt_route_process_single(
			    &session->route_process,
			    YT_FRIENDSHIP_RELATION_ADDRESS);
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
				return session_0317(session, unavailable,
				    sizeof(unavailable) - 1U,
				    "computer planet unavailable", error);
			}
		}
		if (!valid_link && yt_route_process_single(
		    &session->route_process,
		    YT_PLANET_RECORD_SCRATCH_ADDRESS) < 1.0f)
			return port_report_failure(error,
			    "computer planet stale current-planet record");
		return planet_inventory(session, (int)(valid_link
		    ? link : single_sub(yt_route_process_single(
		    &session->route_process, YT_PLANET_RECORD_SCRATCH_ADDRESS),
		    session_planet_offset(session))), error);
	}
}

static bool
owned_fighters_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	return session_read_sector(context, logical_sector, sector, error);
}

static bool
owned_fighters_direct_line(void *context, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	return session_present_text(context, text, length, SESSION_PRESENT_LINE,
	    operation, error);
}

static bool
owned_fighters_searching(void *context, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	return session_031f(context, text, length, operation, error);
}

static bool
owned_fighters_b05d(void *context, const uint8_t *text, size_t length,
    const char *operation, struct yt_error *error)
{
	(void)operation;
	(void)error;
	return session_02fc(context, text, length);
}

static bool
owned_fighters_fixed(void *context, const uint8_t *text, size_t length,
    float width, const char *operation, struct yt_error *error)
{
	return session_fixed_width_bytes(context, text, length, width, operation,
	    error);
}

static void
owned_fighters_store_scanner(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, raw);
}

static bool
owned_fighters_pager_quit(void *context)
{
	struct yt_session *session = context;

	return strcmp(session->pager.key, "Q") == 0;
}

static bool
computer_owned_fighters(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_owned_fighters_ops ops = {
		owned_fighters_read_sector,
		owned_fighters_direct_line,
		owned_fighters_searching,
		owned_fighters_b05d,
		owned_fighters_fixed,
		owned_fighters_store_scanner,
		owned_fighters_pager_quit,
	};
	struct yt_owned_fighters_state state = {
		.maximum_sector = sector_count(session),
		.current_player = (float)session_record(session),
	};

	yt_route_process_raw_single(&session->route_process,
	    YT_COMPUTER_ROUTE_STATUS_ADDRESS, state.scanner_scratch_raw);
	return yt_owned_fighters_run(&state, &ops, session, error);
}

static bool
owned_planets_read_sector(void *context, int logical_sector,
    struct yt_sector *sector, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_read_sector(session, logical_sector,
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

	yt_present_set_blink(&session->presentation, blink);
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
		.planet_record_base = session_planet_offset(session),
		.current_player = (float)session_record(session),
		.blink = yt_present_blink(&session->presentation),
	};

	return yt_owned_planets_run(&state, &ops, session, error);
}

static bool
computer_port_friendship(struct yt_session *session, float owner,
    bool *friendly, struct yt_error *error)
{
	static const uint8_t false_raw[4] = {0x00U, 0x00U, 0x80U, 0x00U};
	static const uint8_t true_raw[4] = {0x00U, 0x00U, 0x80U, 0x81U};
	struct yt_player current;
	struct yt_player other;

	if (friendly == NULL)
		return false;
	*friendly = false;
	yt_route_process_set_raw_single(&session->route_process,
	    YT_FRIENDSHIP_RELATION_ADDRESS, false_raw);
	if (owner < 2.0f
	    || owner > session_sector_offset(session)
	    || (float)session_record(session) < 2.0f
	    || (float)session_record(session)
	    > session_sector_offset(session))
		return true;
	if (owner == (float)session_record(session)) {
		*friendly = true;
		yt_route_process_set_raw_single(&session->route_process,
		    YT_FRIENDSHIP_RELATION_ADDRESS, true_raw);
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
		yt_route_process_set_raw_single(&session->route_process,
		    YT_FRIENDSHIP_RELATION_ADDRESS, true_raw);
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
		visibility.inherited_index = yt_route_process_single(
		    &session->route_process, YT_SHARED_LOOP_SCRATCH_ADDRESS);
		visibility.field_kind = YT_COMPUTER_PORT_FIELD_SECTOR;
		visibility.field_record =
		    qb_brun_random_record_number(sector_expression);
		visibility.field = sector.record;
		visibility.field_valid = true;
		visibility_ok = yt_computer_port_visibility_run(&visibility,
		    computer_port_visibility_read_player, &visibility_context,
		    error);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_PATH_MARKER_ADDRESS, visibility.marker_4d62_raw);
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_STATUS_ADDRESS, visibility.relation_raw);
		if (visibility.scratch_written)
			yt_route_process_set_raw_single(&session->route_process,
			    YT_PLANET_RECORD_SCRATCH_ADDRESS,
			    visibility.scratch_19c4_raw);
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
		yt_route_process_raw_single(&session->route_process,
		    YT_EARTH_REPORT_SEEN_ADDRESS, earth_state.report_seen_raw);
		if (!yt_computer_port_earth_run(&earth_state,
		    computer_port_earth_report, session, error))
			return false;
		session_set_earth_report_seen(session,
		    earth_state.report_seen_raw);
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
		    yt_route_process_avoid(&session->route_process, (size_t)row))
		    || !computer_avoid_cell(last, sizeof(last), row + 21,
		    yt_route_process_avoid(&session->route_process,
		    (size_t)row + 20U))
		    || !session_fixed_width(session, first, 20.0f,
		    "avoid first cell", error)
		    || !computer_avoid_cell(middle, sizeof(middle), row + 11,
		    yt_route_process_avoid(&session->route_process,
		    (size_t)row + 10U))
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
	old_value = yt_route_process_avoid(&session->route_process,
	    (size_t)(slot - 1));
	if (!yt_route_process_set_avoid_slot(&session->route_process,
	    (size_t)(slot - 1), new_value, error))
		return false;
	yt_computer_avoid_transition(old_value, new_value, &locked, &available);
	session_set_foreground(session, 2.0f);
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
	session_set_foreground(session, 1.0f);
	return true;
}

static bool
computer_spy_read_target(void *context, size_t index, int16_t *target,
    struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	if (index >= 3U)
		return false;
	*target = yt_route_process_word(&session->route_process,
	    (uint16_t)(YT_SPY_SECTORS_ADDRESS + 2U * index));
	return true;
}

static bool
computer_spy_present(void *context, const uint8_t *text, size_t length,
    enum yt_computer_spy_output_kind kind, struct yt_error *error)
{
	struct yt_session *session = context;

	if (kind == YT_COMPUTER_SPY_NONE)
		return session_02db(session, text, length,
		    "active-spy none notice", error);
	if (kind == YT_COMPUTER_SPY_LEADING_BLANK)
		return session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "active-spy leading blank", error);
	if (kind == YT_COMPUTER_SPY_ROW) {
		yt_present_set_bold(&session->presentation, 1.0f);
		return session_02fc(session, text, length);
	}
	if (error != NULL)
		error->status = YT_INVALID;
	return false;
}

static bool
computer_spies(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_computer_spy_ops ops = {
		computer_spy_read_target,
		computer_spy_present,
	};
	struct yt_computer_spy_state state = {
		.count = yt_route_process_single(&session->route_process,
		    YT_SPY_COUNT_ADDRESS),
	};

	return yt_computer_spy_run(&state, &ops, session, error);
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

static enum navigation_field_kind
nearest_navigation_field(enum yt_nearest_field_kind kind)
{
	switch (kind) {
	case YT_NEAREST_FIELD_PLAYER:
		return NAVIGATION_FIELD_NEAREST_PLAYER;
	case YT_NEAREST_FIELD_SECTOR:
		return NAVIGATION_FIELD_NEAREST_SECTOR;
	case YT_NEAREST_FIELD_PORT:
		return NAVIGATION_FIELD_NEAREST_PORT;
	case YT_NEAREST_FIELD_OWNER:
		return NAVIGATION_FIELD_NEAREST_OWNER;
	case YT_NEAREST_FIELD_NONE:
	default:
		return NAVIGATION_FIELD_NONE;
	}
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
	session_market_bases(session, state.base_price);
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
	if (state.field_valid) {
		session->navigation_field_active = true;
		session->navigation_field_kind =
		    nearest_navigation_field(state.field_kind);
		session->navigation_field_record = (int)state.field_record;
		session->navigation_field = state.field;
	}
	return ok;
}

static bool
nearest_front_hydrate(void *context, float *current_team,
    float *ports_owned, struct yt_error *error)
{
	struct yt_session *session = context;

	if (!reload_player(session, error))
		return false;
	*current_team = session->player.team;
	*ports_owned = session->player.ports_owned;
	return true;
}

static bool
nearest_front_present(void *context,
    enum yt_nearest_front_output_kind kind, const uint8_t *text,
    size_t length, struct yt_error *error)
{
	struct yt_session *session = context;

	switch (kind) {
	case YT_NEAREST_FRONT_FILTER_FIRST:
		return session_0317(session, text, length,
		    "nearest-port first filter row", error);
	case YT_NEAREST_FRONT_FILTER_SECOND:
		return session_02fc(session, text, length);
	case YT_NEAREST_FRONT_FILTER_PROMPT:
		return session_031f(session, text, length,
		    "nearest-port filter prompt", error);
	case YT_NEAREST_FRONT_NO_TEAM:
		return session_02db(session, text, length,
		    "nearest-port team rejection", error);
	case YT_NEAREST_FRONT_NO_PORTS:
		return session_02db(session, text, length,
		    "nearest-port ownership rejection", error);
	case YT_NEAREST_FRONT_DIRECTION_BLANK:
		return session_present_text(session, text, length,
		    SESSION_PRESENT_LINE, "nearest-port direction blank", error);
	case YT_NEAREST_FRONT_DIRECTION_PROMPT:
		return session_031f(session, text, length,
		    "nearest-port direction prompt", error);
	default:
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
}

static bool
nearest_front_input(void *context, uint8_t *text, size_t capacity,
    size_t *length, struct yt_error *error)
{
	struct yt_session *session = context;

	(void)error;
	if (capacity == 0U || !session_0357(session, (char *)text, capacity))
		return false;
	*length = strlen((const char *)text);
	return true;
}

static bool
computer_nearest_ports(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_nearest_front_ops ops = {
		nearest_front_hydrate,
		nearest_front_present,
		nearest_front_input,
	};
	struct yt_nearest_front_state state;

	memset(&state, 0, sizeof(state));
	if (!yt_nearest_front_run(&state, &ops, session, error))
		return false;
	if (state.result == YT_NEAREST_FRONT_REPROMPT)
		return true;
	return nearest_session_body(session, state.selector, state.direction,
	    error);
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

static enum navigation_field_kind
profit_navigation_field(enum yt_profit_field_kind kind)
{
	switch (kind) {
	case YT_PROFIT_FIELD_PLAYER:
		return NAVIGATION_FIELD_PROFIT_PLAYER;
	case YT_PROFIT_FIELD_SECTOR:
		return NAVIGATION_FIELD_PROFIT_SECTOR;
	case YT_PROFIT_FIELD_PORT:
		return NAVIGATION_FIELD_PROFIT_PORT;
	case YT_PROFIT_FIELD_NONE:
	default:
		return NAVIGATION_FIELD_NONE;
	}
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
	session_market_bases(session, state.base_price);
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
	if (state.field_valid) {
		session->navigation_field_active = true;
		session->navigation_field_kind =
		    profit_navigation_field(state.field_kind);
		session->navigation_field_record = (int)state.field_record;
		session->navigation_field = state.field;
	}
	return ok;
}

static bool
computer_activation_present(void *context, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	return session_0317(context, text, length,
	    "computer activation notice", error);
}

static void
computer_activation_effect(void *context,
    enum yt_computer_activation_effect effect)
{
	struct yt_session *session = context;

	(void)effect;
	session_set_foreground(session, 1.0f);
}

static void
computer_activation_store_selector(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_COMPUTER_ACTIVATION_SELECTOR_ADDRESS, raw);
}

static bool
computer_activation_sound(void *context, struct yt_error *error)
{
	struct yt_session *session = context;

	return session_sound(session, yt_route_process_single(
	    &session->route_process, YT_COMPUTER_ACTIVATION_SELECTOR_ADDRESS),
	    "computer activation sound", error);
}

static bool
computer_activate(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_computer_activation_ops ops = {
		computer_activation_effect,
		computer_activation_present,
		computer_activation_store_selector,
		computer_activation_sound,
	};
	struct yt_computer_activation_state state;

	return yt_computer_activation_run(&state, &ops, session, error);
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

		session_compat_upper_n(session,
		    (uint8_t *)session->output_source, *length);
	}
	return true;
}

static void
computer_scoreboard_reset_pager(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	session_set_pager_line_count_raw(session, raw);
}

static void
computer_scoreboard_store_defense_owner(void *context,
    const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SHARED_TARGET_RECORD_ADDRESS, raw);
}

static void
computer_scoreboard_store_team_id(void *context, const uint8_t raw[4])
{
	struct yt_session *session = context;

	yt_route_process_set_raw_single(&session->route_process,
	    YT_SCOREBOARD_TEAM_SCRATCH_ADDRESS, raw);
}

static bool
computer_scoreboard_generate(void *context, struct yt_error *error)
{
	struct yt_session *session = context;
	struct yt_score_field_observation field;
	bool generated;

	generated = yt_score_generate_progress_process_observed(
	    &session->door->game, session_sector_offset(session),
	    session_port_offset(session), computer_scoreboard_progress, session,
	    &field, computer_scoreboard_store_defense_owner, session,
	    computer_scoreboard_store_team_id, session, error);
	if (field.valid) {
		session->navigation_field_active = true;
		session->navigation_field_record = (int)field.physical_record;
		session->navigation_field = field.image;
		switch (field.kind) {
		case YT_SCORE_FIELD_PLAYER:
			session->navigation_field_kind =
			    NAVIGATION_FIELD_SCOREBOARD_PLAYER;
			break;
		case YT_SCORE_FIELD_SECTOR:
			session->navigation_field_kind =
			    NAVIGATION_FIELD_SCOREBOARD_SECTOR;
			break;
		case YT_SCORE_FIELD_TEAM:
			session->navigation_field_kind =
			    NAVIGATION_FIELD_SCOREBOARD_TEAM;
			break;
		case YT_SCORE_FIELD_NONE:
		default:
			break;
		}
	}
	return generated;
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
computer_newspaper_checkpoint(void *context, bool *resume,
    struct yt_error *error)
{
	(void)context;
	(void)error;
	if (resume == NULL)
		return false;
	/* Physical local ON KEY delivery is an explicitly deferred adapter. */
	*resume = true;
	return true;
}

static bool
computer_newspaper(struct yt_session *session, struct yt_error *error)
{
	static const struct yt_computer_newspaper_ops ops = {
		computer_newspaper_checkpoint,
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
		yt_route_process_set_raw_single(&session->route_process,
		    YT_COMPUTER_ROUTE_STATUS_ADDRESS, scanner_zero);
	}
	else if (effect == YT_COMPUTER_PROMPT_SET_FOREGROUND) {
		session_set_foreground(session, 1.0f);
	}
}

static bool
computer_menu_prompt_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
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
			.current_player_record = session_record(session),
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
			if (!computer_owned_fighters(session, error))
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
			session_set_process_single(session,
			    YT_COMPUTER_ROUTE_STATUS_ADDRESS, 1.0f);
			if (!radio_read(session, yt_route_process_single(
			    &session->route_process,
			    YT_COMPUTER_ROUTE_STATUS_ADDRESS), error))
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

	session_set_foreground(session, 6.0f);
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
	    || !session_031f(session, generating, sizeof(generating) - 1U,
	    "normal-exit generating row", error)
	    || !yt_score_generate_progress_process_observed(
	    &session->door->game, session_sector_offset(session),
	    session_port_offset(session), computer_scoreboard_progress, session,
	    NULL, NULL, NULL, NULL, NULL, error)
	    || !session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "normal-exit post-generator blank", error))
		return false;
	session_set_pager_nonstop(session, 1.0f);
	if (!display_game_file(session,
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
	return session_02fc(session, (const uint8_t *)returning,
	    (size_t)length);
}

static void
main_prompt_effect(void *context, enum yt_main_prompt_effect effect)
{
	struct yt_session *session = context;

	switch (effect) {
	case YT_MAIN_PROMPT_RESET_PAGER:
		session_set_pager_line_count(session, 0.0f);
		break;
	case YT_MAIN_PROMPT_SET_FOREGROUND:
		session_set_foreground(session, 2.0f);
		break;
	case YT_MAIN_PROMPT_RESET_SCANNER:
		session_set_relationship(session, 0.0f);
		break;
	}
}

static bool
main_prompt_hydrate(void *context, int player_record,
    struct yt_player *player, struct yt_error *error)
{
	struct yt_session *session = context;

	if (player_record != session_record(session)
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

	while (session->running && !session_is_destroyed(session)) {
		char command[YT_COMMAND_SIZE];
		struct yt_main_prompt_state prompt = {
			.current_player_record = session_record(session),
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
			enum yt_present_status status =
			    yt_present_sound_toggle_process(
			    &session->route_process.bytes[YT_SESSION_MODE_ADDRESS],
			    &session->route_process.bytes[YT_GAME_SOUND_ADDRESS],
			    &session->route_process.bytes[YT_LOCAL_SOUND_ADDRESS],
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
	float market_base[3];
	uint8_t mode_raw[4];
	static const uint8_t static_one[4] = {0x00, 0x00, 0x00, 0x81};
	static const uint8_t scanner_mode_zero[4] = {0x00, 0x00, 0x46, 0x00};
	static const uint8_t action_cloak_display_scale[4] = {
	    0x00, 0x00, 0x48, 0x86
	};
	static const uint8_t action_turn_divisor[4] = {
	    0x00, 0x00, 0x48, 0x85
	};
	static const uint8_t action_xannor_threshold[4] = {
	    0xa4, 0x70, 0x7d, 0x80
	};
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
	yt_present_bind_background_process(&session.presentation,
	    &session.route_process.bytes[YT_BACKGROUND_ADDRESS]);
	yt_present_bind_bold_process(&session.presentation,
	    &session.route_process.bytes[YT_BOLD_ADDRESS]);
	yt_present_bind_blink_process(&session.presentation,
	    &session.route_process.bytes[YT_BLINK_ADDRESS]);
	yt_sound_bind_ansi_process(&session.presentation.sound,
	    &session.route_process.bytes[YT_ANSI_ADDRESS]);
	yt_sound_bind_snoop_process(&session.presentation.sound,
	    &session.route_process.bytes[YT_LOCAL_SCREEN_ADDRESS]);
	yt_sound_bind_endpoint_process(&session.presentation.sound,
	    &session.route_process.bytes[YT_SESSION_MODE_ADDRESS],
	    &session.route_process.bytes[YT_GAME_SOUND_ADDRESS],
	    &session.route_process.bytes[YT_LOCAL_SOUND_ADDRESS]);
	yt_sound_bind_toggle_selector_process(&session.presentation.sound,
	    &session.route_process.bytes[YT_SOUND_TOGGLE_SELECTOR_ADDRESS]);
	yt_present_bind_color_process(&session.presentation,
	    session.route_process.bytes, YT_COLOR_INITIALIZED_ADDRESS,
	    YT_COLOR_TABLE_ADDRESS);
	yt_present_bind_cached_foreground_process(&session.presentation,
	    &session.route_process.bytes[YT_CACHED_FOREGROUND_ADDRESS]);
	yt_present_bind_cached_background_process(&session.presentation,
	    &session.route_process.bytes[YT_CACHED_BACKGROUND_ADDRESS]);
	session_bind_pager_process(&session);
	yt_present_bind_time_process_cells(&session.time,
	    &session.route_process.bytes[YT_TIME_SAVED_CURSOR_ROW_ADDRESS],
	    &session.route_process.bytes[YT_TIME_SAVED_CURSOR_COLUMN_ADDRESS],
	    &session.route_process.bytes[YT_TIME_REMAINING_MINUTES_ADDRESS]);
	session.door = door;
	session.executable_path = executable_path;
	session.running = true;
	yt_route_process_set_raw_single(&session.route_process,
	    YT_STATIC_SINGLE_ONE_ADDRESS, static_one);
	yt_route_process_set_raw_single(&session.route_process,
	    YT_POST_LOGIN_SCANNER_MODE_ADDRESS, scanner_mode_zero);
	yt_route_process_set_raw_single(&session.route_process,
	    YT_ACTION_CLOAK_DISPLAY_SCALE_ADDRESS,
	    action_cloak_display_scale);
	yt_route_process_set_raw_single(&session.route_process,
	    YT_ACTION_TURN_DIVISOR_ADDRESS, action_turn_divisor);
	yt_route_process_set_raw_single(&session.route_process,
	    YT_ACTION_XANNOR_THRESHOLD_ADDRESS,
	    action_xannor_threshold);
	/* YT:040A is the ordinary instruction after the handed-off checkpoint. */
	session_set_pager_nonstop(&session, 1.0f);
	if (door->identity.ansi
	    && !qb_mbf32_truth(door->identity.ansi_raw)) {
		uint8_t raw_one[4];

		if (qb_mbf32_encode(1.0f, raw_one) != QB_MBF_OK)
			return false;
		yt_route_process_set_raw_single(&session.route_process,
		    YT_ANSI_ADDRESS, raw_one);
	}
	else {
		yt_route_process_set_raw_single(&session.route_process,
		    YT_ANSI_ADDRESS, door->identity.ansi_raw);
	}
	if (!yt_startup_local_mode_raw(door->identity.local, mode_raw))
		return false;
	yt_route_process_set_raw_single(&session.route_process,
	    YT_SESSION_MODE_ADDRESS, mode_raw);
	session_set_process_single(&session, YT_GAME_SOUND_ADDRESS, -1.0f);
	session_set_process_single(&session, YT_LOCAL_SOUND_ADDRESS,
	    door->identity.local ? -1.0f : 0.0f);
	session_set_foreground(&session, 7.0f);
	yt_random_init(&launch_random);
	if (!yt_random_market_bases(&launch_random, market_base, error))
		return false;
	for (size_t index = 0U; index < 3U; ++index)
		session_set_process_single(&session,
		    (uint16_t)(YT_MARKET_BASE_ADDRESS + 4U * index),
		    market_base[index]);
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
