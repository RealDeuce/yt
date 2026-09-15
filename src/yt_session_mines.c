#include "yt_session_internal.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static bool
mine_loss_row(struct yt_session *session, enum yt_sector_mine_loss_kind kind,
    float loss, uint8_t *row, size_t capacity, struct yt_error *error)
{
	size_t length;

	return yt_sector_mine_loss_row(kind, loss, row, capacity, &length)
	    && session_present_text(session, row, length, SESSION_PRESENT_LINE,
	    "sector mine output", error);
}

static bool
mine_shrink(struct yt_session *session, float range, float *result,
    struct yt_error *error)
{
	return yt_random_nested_single(&session->door->game.random, 3.0f,
	    &range, result, error);
}

static bool
mine_stock_loss(struct yt_session *session, float batch, float *stock,
    float *loss, struct yt_error *error)
{
	float sampled;

	if (!mine_shrink(session, qb_single_multiply(batch, *stock), &sampled, error))
		return false;
	if (sampled > *stock)
		sampled = *stock;
	*stock = qb_single_subtract(*stock, sampled);
	*loss = sampled;
	return true;
}

static bool
mine_damage_shields(struct yt_session *session, struct yt_player *player,
    float batch, float saved_foreground, unsigned *touched,
    struct yt_error *error)
{
	static const uint8_t shields_destroyed[] = "Shields disintegrated!";
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";
	uint8_t row[300];
	size_t row_length;
	float draw;

	if (!yt_random_next(&session->door->game.random, &draw, error))
		return false;
	player->shields = yt_sector_mine_shield_result(player->shields, batch,
	    draw);
	*touched |= YT_SECTOR_MINE_DAMAGE_SHIELDS;
	if (player->shields == 0.0f) {
		session_set_foreground(session, 7.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_text(session, shields_destroyed,
		    sizeof(shields_destroyed) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "sector mine output", error))
			return false;
		session_set_foreground(session, saved_foreground);
		return true;
	}
	if (!yt_sector_mine_shields_row(player->shields, row, sizeof(row),
	    &row_length)
	    || !session_present_text(session, row, row_length,
	    SESSION_PRESENT_BOLD_LINE, "sector mine output", error)
	    || !yt_random_next(&session->door->game.random, &draw, error))
		return false;
	if (player->danger_scanner == 0.0f
	    || draw <= 0.949999988079071f)
		return true;
	player->danger_scanner = 0.0f;
	*touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
	session_set_foreground(session, 7.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, scanner_destroyed,
	    sizeof(scanner_destroyed) - 1U, SESSION_PRESENT_BOLD_LINE,
	    "sector mine output", error))
		return false;
	session_set_foreground(session, saved_foreground);
	return true;
}

static bool
mine_damage_unshielded(struct yt_session *session, struct yt_player *player,
    float batch, float saved_foreground, unsigned *touched,
    struct yt_error *error)
{
	static const uint8_t scanner_destroyed[] =
	    "Danger scanner destroyed!";
	uint8_t row[300];
	float draw;
	float loss;
	float empty;

	if (player->fighters != 0.0f) {
		if (!mine_shrink(session, qb_single_multiply(40000.0f, batch), &loss,
		    error))
			return false;
		if (loss > player->fighters)
			loss = player->fighters;
		player->fighters = qb_single_subtract(player->fighters, loss);
		*touched |= YT_SECTOR_MINE_DAMAGE_FIGHTERS;
		if (!mine_loss_row(session, YT_SECTOR_MINE_LOSS_FIGHTERS,
		    loss, row, sizeof(row), error))
			return false;
	}
	if (player->cloak != 0.0f) {
		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		loss = yt_sector_mine_cloak_loss(player->cloak, batch, draw);
		player->cloak = qb_single_subtract(player->cloak, loss);
		*touched |= YT_SECTOR_MINE_DAMAGE_CLOAK;
		if (!mine_loss_row(session, YT_SECTOR_MINE_LOSS_CLOAK, loss,
		    row, sizeof(row), error))
			return false;
	}
	if (player->missiles != 0.0f) {
		if (!yt_random_next(&session->door->game.random, &draw, error))
			return false;
		loss = yt_sector_mine_missile_loss(player->missiles, batch, draw);
		player->missiles = qb_single_subtract(player->missiles, loss);
		*touched |= YT_SECTOR_MINE_DAMAGE_MISSILES;
		if (!mine_loss_row(session, YT_SECTOR_MINE_LOSS_MISSILES, loss,
		    row, sizeof(row), error))
			return false;
	}
	if (player->danger_scanner != 0.0f) {
		player->danger_scanner = 0.0f;
		*touched |= YT_SECTOR_MINE_DAMAGE_SCANNER;
		session_set_foreground(session, 7.0f);
		yt_present_set_blink(&session->presentation, 1.0f);
		if (!session_present_text(session, scanner_destroyed,
		    sizeof(scanner_destroyed) - 1U, SESSION_PRESENT_BOLD_LINE,
		    "sector mine output", error))
			return false;
		session_set_foreground(session, saved_foreground);
	}
#define MINE_STOCK(member, flag, kind) do { \
	if (player->member != 0.0f) { \
		if (!mine_stock_loss(session, batch, &player->member, &loss, \
		    error)) \
			return false; \
		*touched |= (flag); \
		if (!mine_loss_row(session, (kind), loss, row, sizeof(row), \
		    error)) \
			return false; \
	} \
} while (0)
	MINE_STOCK(mines, YT_SECTOR_MINE_DAMAGE_CARRIED_MINES,
	    YT_SECTOR_MINE_LOSS_MINES);
	MINE_STOCK(ore, YT_SECTOR_MINE_DAMAGE_ORE,
	    YT_SECTOR_MINE_LOSS_ORE);
	MINE_STOCK(organics, YT_SECTOR_MINE_DAMAGE_ORGANICS,
	    YT_SECTOR_MINE_LOSS_ORGANICS);
	MINE_STOCK(equipment, YT_SECTOR_MINE_DAMAGE_EQUIPMENT,
	    YT_SECTOR_MINE_LOSS_EQUIPMENT);
#undef MINE_STOCK
	empty = yt_sector_mine_empty_holds(player);
	if (!(empty > 0.0f))
		return true;
	if (!mine_shrink(session, empty, &loss, error))
		return false;
	loss = qb_single_multiply(loss, batch);
	if (loss > empty)
		loss = empty;
	player->holds = qb_single_subtract(player->holds, loss);
	if (player->holds < 1.0f) {
		player->holds = 0.0f;
		session->destroyed = true;
	}
	*touched |= YT_SECTOR_MINE_DAMAGE_HOLDS;
	return mine_loss_row(session, YT_SECTOR_MINE_LOSS_EMPTY_HOLDS, loss,
	    row, sizeof(row), error);
}

static bool
drop_mines_failure(struct yt_error *error, enum yt_status status,
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

bool
yt_session_command_mines(struct yt_session *session, struct yt_error *error)
{
	static const uint8_t no_mines[] = "You don't HAVE any!";
	static const uint8_t union_refusal[] =
	    "The Union doesnt like the home 7 sectors mined!";
	static const char prompt_suffix[] =
	    " mines. Drop how many? [0] -=>";
	static const char success_suffix[] = " is now mined!";
	struct qb_val_result parsed;
	struct yt_player player;
	struct yt_sector sector;
	enum qb_mbf_status conversion;
	char response[4096] = {0};
	char number[64];
	uint8_t amount_raw[4];
	uint8_t row[192];
	float amount;
	float carried;
	float remaining;
	int current_sector;
	int number_length;
	size_t row_length;

	if (!session_reload_player(session, error))
		return false;
	player = session->player;
	carried = player.mines;
	current_sector = (int)player.sector;
	if (carried < 0.0f) {
		player.mines = 0.0f;
		if (!yt_record_set_number(&player.record, YT_F129, 0.0f)
		    || !yt_game_write_player(&session->door->game,
		    session_record(session), &player, error)
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
	}
	if (carried < 1.0f)
		return session_present_alert(session, no_mines,
		    sizeof(no_mines) - 1U, "no sector mines", error);
	if (player.sector < 8.0f)
		return session_present_alert(session, union_refusal,
		    sizeof(union_refusal) - 1U, "Union sector mine refusal",
		    error);

	number_length = qb_str_single(number, sizeof(number), carried);
	if (number_length < 0
	    || sizeof("You have") - 1U + (size_t)number_length
	    + sizeof(prompt_suffix) - 1U > sizeof(row))
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines prompt");
	memcpy(row, "You have", sizeof("You have") - 1U);
	row_length = sizeof("You have") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, prompt_suffix, sizeof(prompt_suffix) - 1U);
	row_length += sizeof(prompt_suffix) - 1U;
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine prompt blank", error)
	    || !session_present_timed_paged_row(session, row, row_length,
	    "sector mine prompt", error)
	    || !session_read_number_command(session, response, sizeof(response)))
		return false;
	if (response[0] == '\0')
		amount = 0.0f;
	else {
		parsed = qb_val(response);
		if (!parsed.valid || parsed.overflow)
			return drop_mines_failure(error, YT_RANGE,
			    "drop-mines:VAL");
		amount = (float)parsed.value;
	}
	conversion = qb_mbf32_encode(amount, amount_raw);
	if (conversion == QB_MBF_OVERFLOW)
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines:amount-csng");
	amount = qb_mbf32_decode(amount_raw);
	if (yt_sector_mine_admit(carried, amount) != YT_SECTOR_MINE_ACCEPTED)
		return true;

	session->self_mine_suppressed = true;
	remaining = qb_single_subtract(carried, amount);
	player.mines = remaining;
	if (!yt_record_set_number(&player.record, YT_F129, remaining)
	    || !yt_game_write_player(&session->door->game,
	    session_record(session), &player, error)
	    || !yt_database_flush(&session->door->game.database, error)
	    || !session_read_sector(session, current_sector, &sector, error))
		return false;
	sector.mines = qb_single_add(sector.mines, amount);
	if (!yt_record_set_number(&sector.record, YT_F129, sector.mines)
	    || !session_write_sector(session, current_sector, &sector, error)
	    || !yt_database_flush(&session->door->game.database, error))
		return false;

	number_length = qb_str_single(number, sizeof(number), player.sector);
	if (number_length < 0
	    || sizeof("Sector") - 1U + (size_t)number_length
	    + sizeof(success_suffix) - 1U > sizeof(row))
		return drop_mines_failure(error, YT_RANGE,
		    "drop-mines success row");
	memcpy(row, "Sector", sizeof("Sector") - 1U);
	row_length = sizeof("Sector") - 1U;
	memcpy(row + row_length, number, (size_t)number_length);
	row_length += (size_t)number_length;
	memcpy(row + row_length, success_suffix, sizeof(success_suffix) - 1U);
	row_length += sizeof(success_suffix) - 1U;
	session_set_foreground(session, 6.0f);
	if (!session_present_text(session, NULL, 0, SESSION_PRESENT_LINE,
	    "sector mine success blank", error))
		return false;
	yt_present_set_bold(&session->presentation, 1.0f);
	yt_present_set_blink(&session->presentation, 1.0f);
	return session_present_paged_fragment(session, row, row_length)
	    && session_sound(session, 4.0f, "sector mine sound", error);
}

bool
yt_session_mine_encounter(struct yt_session *session, bool *terminal,
    struct yt_error *error)
{
	static const uint8_t warning[] = "** Sector is Mined!! **";
	struct yt_player player;
	struct yt_sector sector;
	uint8_t row[300];
	size_t row_length;
	float current_sector;
	int current;

	if (terminal == NULL)
		return false;
	*terminal = false;
	current_sector = session->player.sector;
	current = (int)current_sector;
	if (!session_present_text(session, NULL, 0U, SESSION_PRESENT_LINE,
	    "sector mine output", error))
		return false;
	yt_present_set_blink(&session->presentation, 1.0f);
	if (!session_present_text(session, warning, sizeof(warning) - 1U,
	    SESSION_PRESENT_LINE, "sector mine output", error)
	    || !session_sound(session, 5.0f, "sector mine sound", error)
	    || !session_reload_player(session, error))
		return false;
	player = session->player;
	if (!yt_sector_mine_entry_news(player.record.bytes,
	    (size_t)player.name_length, current_sector, row, sizeof(row),
	    &row_length)
	    || !yt_news_append_bytes(row, row_length, error))
		return false;

	for (;;) {
		struct yt_player working;
		struct yt_player persisted;
		float saved_foreground;
		float mines_before;
		float batch;
		float draw;
		unsigned touched = 0U;

		if (!session_read_sector(session, current, &sector, error))
			return false;
		mines_before = sector.mines;
		batch = yt_sector_mine_batch(mines_before);
		yt_sector_mine_sector_overlay(&sector,
		    qb_single_subtract(mines_before, batch));
		if (!yt_database_write(&session->door->game.database,
		    (size_t)yt_sector_basic_record(&session->door->game.config,
		    current), &sector.record, error))
			return false;

		saved_foreground = session->foreground;
		session_set_foreground(session, 3.0f);
		yt_present_set_background(&session->presentation, 0.0f);
		yt_present_set_blink(&session->presentation, 0.0f);
		if (!yt_sector_mine_explosion_row(mines_before, batch, row,
		    sizeof(row), &row_length)
		    || !session_present_text(session, row, row_length,
		    SESSION_PRESENT_BOLD_RAW, "sector mine output", error))
			return false;
		yt_present_set_background(&session->presentation, 1.0f);
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "sector mine output", error)
		    || !session_reload_player(session, error))
			return false;
		working = session->player;

		if (working.shields > 0.0f) {
			if (!mine_damage_shields(session, &working, batch,
			    saved_foreground, &touched, error))
				return false;
		}
		else if (!mine_damage_unshielded(session, &working, batch,
		    saved_foreground, &touched, error))
			return false;

		if (!yt_game_read_player(&session->door->game,
		    session_record(session), &persisted, error))
			return false;
		yt_sector_mine_player_overlay(&persisted, &working, touched);
		if (!yt_database_write(&session->door->game.database,
		    (size_t)session_record(session), &persisted.record, error)
		    || !yt_database_flush(&session->door->game.database, error))
			return false;
		working.record = persisted.record;
		player = working;
		session->player = working;
		if (!session_sound(session, 2.0f, "sector mine sound", error)
		    || !yt_random_next(&session->door->game.random, &draw, error))
			return false;
		if (draw > 0.800000011920929f && working.holds < 10.0f) {
			if (!yt_session_emergency_warp(session, error))
				return false;
			*terminal = true;
			return true;
		}
		if (sector.mines > 0.0f && !session->destroyed)
			continue;
		break;
	}
	if (!yt_sector_mine_final_news(player.shields, row, sizeof(row),
	    &row_length)
	    || !yt_news_append_bytes(row, row_length, error)
	    || !session_read_sector(session, current, &sector, error))
		return false;
	return true;
}
