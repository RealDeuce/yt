#include "yt_score.h"
#include "qb.h"
#include "yt_score_format.h"
#include "yt_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct score_player {
	int record;
	struct yt_player player;
	double score;
	bool occupied;
};

struct score_team {
	int id;
	double score;
};

static bool
write_bytes(struct yt_text_output *output, const char *text,
    struct yt_error *error)
{
	return yt_text_output_write(output, (const uint8_t *)text,
	    strlen(text), error);
}

static bool
open_scoreboard(const struct yt_game *game, struct yt_text_output *output,
    char path[512],
    struct yt_error *error)
{
	const char *requested = strcmp(game->config.scoreboard, "NUL") == 0
	    ? "YTTEMP" : game->config.scoreboard;

	if (!yt_resolve_case_path(requested, true, path, 512, error))
		return false;
	return yt_text_output_open(output, path, error);
}

static bool
scoreboard_division_error(struct yt_text_output *output, const char *path,
    struct yt_error *error)
{
	/* Fatal END cleanup closes the still-registered sequential file. */
	(void)yt_text_output_close_all_method(output, 0, NULL);
	yt_text_output_destroy(output);
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		snprintf(error->operation, sizeof(error->operation),
		    "scoreboard division by zero");
		snprintf(error->path, sizeof(error->path), "%s", path);
	}
	return false;
}

static bool
fixed_string(char dest[7], const char *source)
{
	size_t length = strlen(source);

	if (length > 6U)
		length = 6U;
	memcpy(dest, source, length);
	memset(dest + length, ' ', 6U - length);
	dest[6] = '\0';
	return true;
}

static bool
format_player_row(char *dest, size_t size, int rank, double percentage,
    double score, const char *team, float ports, const char *name)
{
	char rank_text[32];
	char percentage_text[64];
	char score_text[160];
	char team_text[7];
	char ports_text[32];
	int written;

	if (!yt_score_format_single(rank_text, sizeof(rank_text), (float)rank,
	    YT_SCORE_FIELD_RANK)
	    || !yt_score_format_double(percentage_text,
	    sizeof(percentage_text), percentage, YT_SCORE_FIELD_PERCENT)
	    || !yt_score_format_double(score_text, sizeof(score_text), score,
	    YT_SCORE_FIELD_SCORE)
	    || !fixed_string(team_text, team)
	    || !yt_score_format_single(ports_text, sizeof(ports_text), ports,
	    YT_SCORE_FIELD_PORTS))
		return false;
	written = snprintf(dest, size, "%s  %s%%  %s   %s %s    %.30s\r\n",
	    rank_text, percentage_text, score_text, team_text, ports_text,
	    name);
	return written >= 0 && (size_t)written < size;
}

static bool
format_team_row(char *dest, size_t size, int rank, double percentage,
    double score, int team, const char *name)
{
	char rank_text[32];
	char percentage_text[64];
	char score_text[160];
	char team_text[32];
	int written;

	if (!yt_score_format_single(rank_text, sizeof(rank_text), (float)rank,
	    YT_SCORE_FIELD_RANK)
	    || !yt_score_format_double(percentage_text,
	    sizeof(percentage_text), percentage, YT_SCORE_FIELD_PERCENT)
	    || !yt_score_format_double(score_text, sizeof(score_text), score,
	    YT_SCORE_FIELD_SCORE)
	    || !yt_score_format_single(team_text, sizeof(team_text),
	    (float)team, YT_SCORE_FIELD_RANK))
		return false;
	written = snprintf(dest, size, "%s  %s%%  %s   %s    %.36s\r\n",
	    rank_text, percentage_text, score_text, team_text, name);
	return written >= 0 && (size_t)written < size;
}

static bool
format_nonhuman_row(char *dest, size_t size, double xannor,
    double xannor_percentage, double mercenaries,
    double mercenary_percentage)
{
	char xannor_text[160];
	char xannor_percentage_text[64];
	char mercenary_text[160];
	char mercenary_percentage_text[64];
	int written;

	if (!yt_score_format_single(xannor_text, sizeof(xannor_text),
	    (float)xannor, YT_SCORE_FIELD_SCORE)
	    || !yt_score_format_double(xannor_percentage_text,
	    sizeof(xannor_percentage_text), xannor_percentage,
	    YT_SCORE_FIELD_XANNOR_PERCENT)
	    || !yt_score_format_single(mercenary_text,
	    sizeof(mercenary_text), (float)mercenaries,
	    YT_SCORE_FIELD_SCORE)
	    || !yt_score_format_double(mercenary_percentage_text,
	    sizeof(mercenary_percentage_text), mercenary_percentage,
	    YT_SCORE_FIELD_PERCENT))
		return false;
	written = snprintf(dest, size, "  %s  %s%%    %s  %s%%\r\n\r\n",
	    xannor_text, xannor_percentage_text, mercenary_text,
	    mercenary_percentage_text);
	return written >= 0 && (size_t)written < size;
}

static float
single_add(float left, float right)
{
	volatile float result = left + right;
	return result;
}

static float
single_mul(float left, float right)
{
	volatile float result = left * right;
	return result;
}

static float
single_sub(float left, float right)
{
	volatile float result = left - right;
	return result;
}

static bool
score_read_sector(struct yt_game *game, float sector_record_offset,
    int logical_sector, struct yt_sector *sector, uint32_t *physical_record,
    struct yt_error *error)
{
	struct yt_record record;
	uint32_t physical = qb_brun_random_record_number(single_add(
	    sector_record_offset, (float)logical_sector));

	if (!yt_database_read(&game->database, (size_t)physical, &record, error))
		return false;
	yt_sector_decode(sector, &record);
	if (physical_record != NULL)
		*physical_record = physical;
	return true;
}

static double
base_score(const struct yt_player *player)
{
	float score = 0;

	if (player->killed_by != 0)
		return 0;
	score = single_add(score, single_mul(player->shields, 50.0f));
	score = single_add(score, single_mul(player->fighters, 100.0f));
	score = single_add(score, single_mul(player->holds, 2500.0f));
	score = single_add(score, single_mul(player->ore, 20.0f));
	score = single_add(score, single_mul(player->organics, 30.0f));
	score = single_add(score, single_mul(player->equipment, 40.0f));
	score = single_add(score, single_mul(player->ports_owned, 50000.0f));
	score = single_add(score, single_mul(player->missiles, 1000.0f));
	score = single_add(score, single_mul(player->ground_forces, 750.0f));
	score = single_add(score, single_mul(player->mines, 2500.0f));
	score = single_add(score, player->credits);
	return (double)score
	    + (double)single_mul(player->plasma, 16000000.0f)
	    + (player->danger_scanner != 0 ? 250000.0 : 0.0);
}

static void
sort_players(struct score_player *players, size_t count)
{
	bool changed;
	size_t index;

	do {
		changed = false;
		for (index = 1; index < count; ++index) {
			if (players[index].score > players[index - 1].score) {
				struct score_player swap = players[index];
				players[index] = players[index - 1];
				players[index - 1] = swap;
				changed = true;
			}
		}
	} while (changed);
}

static void
sort_teams(struct score_team *teams, size_t count)
{
	bool changed;
	size_t index;

	do {
		changed = false;
		for (index = 1; index < count; ++index) {
			if (teams[index].score > teams[index - 1].score) {
				struct score_team swap = teams[index];
				teams[index] = teams[index - 1];
				teams[index - 1] = swap;
				changed = true;
			}
		}
	} while (changed);
}

static void
score_field_observe(struct yt_score_field_observation *field,
    enum yt_score_field_kind kind, uint32_t physical_record,
    const struct yt_record *image)
{
	if (field == NULL || image == NULL)
		return;
	field->kind = kind;
	field->physical_record = physical_record;
	field->image = *image;
	field->valid = true;
}

bool
yt_score_generate_progress_process_observed(struct yt_game *game,
    float sector_record_offset, float port_record_offset,
    yt_score_progress_fn progress, void *context,
    struct yt_score_field_observation *field,
    yt_score_process_store_fn store_defense_owner, void *process_context,
    yt_score_process_store_fn store_team_id, void *team_context,
    struct yt_error *error)
{
	struct score_player players[YT_DEFAULT_PLAYER_COUNT];
	struct score_team teams[YT_DEFAULT_PLAYER_COUNT];
	double xannor = 0;
	double mercenaries = 0;
	double denominator;
	double team_denominator;
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];
	char line[256];
	char path[512];
	struct yt_text_output output;
	int player_count = (int)sector_record_offset - 1;
	int sector_count = (int)single_sub(port_record_offset,
	    sector_record_offset);
	int index;

	if (field != NULL) {
		field->kind = YT_SCORE_FIELD_NONE;
		field->physical_record = 0U;
		field->valid = false;
	}

	if (player_count < 0 || player_count > YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	memset(players, 0, sizeof(players));
	memset(teams, 0, sizeof(teams));
	for (index = 0; index < YT_DEFAULT_PLAYER_COUNT; ++index) {
		players[index].record = index + 2;
		teams[index].id = index + 1;
	}
	if (progress != NULL && !progress(context, 1U, error))
		return false;
	for (index = 0; index < player_count; ++index) {
		if (!yt_game_read_player(game, index + 2, &players[index].player,
		    error))
			return false;
		score_field_observe(field, YT_SCORE_FIELD_PLAYER,
		    (uint32_t)(index + 2), &players[index].player.record);
		players[index].occupied = players[index].player.name_length != 0;
		if (players[index].occupied)
			players[index].score = base_score(&players[index].player);
	}
	if (progress != NULL && !progress(context, 2U, error))
		return false;
	for (index = 1; index <= sector_count; ++index) {
		struct yt_sector sector;
		double contribution;
		uint32_t physical_record;
		int owner;

		if (!score_read_sector(game, sector_record_offset, index, &sector,
		    &physical_record, error))
			return false;
		score_field_observe(field, YT_SCORE_FIELD_SECTOR,
		    physical_record, &sector.record);
		if (store_defense_owner != NULL)
			store_defense_owner(process_context,
			    sector.record.bytes + YT_F85);
		contribution = (double)single_mul(sector.fighters, 100.0f);
		owner = (int)sector.fighter_owner;
		if (owner == -1)
			xannor += contribution;
		else if (owner == -2)
			mercenaries += contribution;
		else if (owner >= 2 && owner <= player_count + 1)
			players[owner - 2].score += contribution;
	}
	for (index = 0; index < player_count; ++index) {
		struct yt_player cached;
		uint8_t team_raw[4];

		if (!yt_game_read_player(game, players[index].record, &cached,
		    error))
			return false;
		score_field_observe(field, YT_SCORE_FIELD_PLAYER,
		    (uint32_t)players[index].record, &cached.record);
		cached.score = players[index].occupied
		    ? (float)players[index].score : -1.0f;
		memcpy(team_raw, cached.record.bytes + YT_F89, 4U);
		yt_player_encode(&cached);
		if (store_team_id != NULL)
			store_team_id(team_context, team_raw);
		score_field_observe(field, YT_SCORE_FIELD_PLAYER,
		    (uint32_t)players[index].record, &cached.record);
		if (!yt_game_write_player(game, players[index].record, &cached,
		    error))
			return false;
		players[index].player.score = cached.score;
		if (!players[index].occupied)
			continue;
		if (players[index].player.team >= 1.0f
		    && players[index].player.team <= 50.0f)
			teams[(int)players[index].player.team - 1].score
			    += players[index].score;
	}
	if (progress != NULL && !progress(context, 3U, error))
		return false;

	sort_players(players, (size_t)player_count);
	if (progress != NULL && !progress(context, 4U, error))
		return false;
	sort_teams(teams, YT_ARRAY_LEN(teams));
	denominator = player_count > 0 ? players[0].score : 0;
	if (denominator == 0)
		denominator = xannor > mercenaries ? xannor : mercenaries;
	team_denominator = teams[0].score;
	yt_text_output_init(&output);
	if (!open_scoreboard(game, &output, path, error))
		return false;
	if (!write_bytes(&output, "\r\n"
	    "Y a n k e e   T r a d e r   S c o r e b o a r d\r\n\r\n",
	    error))
		goto failure;
	if (!yt_platform_clock(&date_now, error)
	    || !yt_platform_clock(&time_now, error))
		goto failure;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	snprintf(line, sizeof(line), "Last updated at: %s %s\r\n\r\n", date,
	    time_text);
	if (!write_bytes(&output, line, error)
	    || !write_bytes(&output,
	    "Rank  Rank%        Score        Team   Ports   Player\r\n"
	    "==== ======= ================= ====== ======= "
	    "================================\r\n", error))
		goto failure;
	{
		int rank = 0;
		for (index = 0; index < player_count; ++index) {
			struct yt_player row_player;
			char team_text[32];

			if (!players[index].occupied)
				continue;
			if (!yt_game_read_player(game, players[index].record,
			    &row_player, error))
				goto failure;
			score_field_observe(field, YT_SCORE_FIELD_PLAYER,
			    (uint32_t)players[index].record, &row_player.record);
			++rank;
			if (denominator == 0)
				return scoreboard_division_error(&output, path, error);
			if (row_player.team == 0.0f)
				strcpy(team_text, "None");
			else {
				size_t length;

				qb_str_single(team_text, sizeof(team_text),
				    row_player.team);
				length = strlen(team_text);
				if (length + 1U < sizeof(team_text)) {
					team_text[length] = ' ';
					team_text[length + 1U] = '\0';
				}
			}
			if (!format_player_row(line, sizeof(line), rank,
			    players[index].score / denominator * 100.0,
			    players[index].score, team_text,
			    row_player.ports_owned, row_player.name)) {
				if (error != NULL)
					error->status = YT_RANGE;
				goto failure;
			}
			if (!write_bytes(&output, line, error))
				goto failure;
		}
	}
	if (!write_bytes(&output, "\r\nT e a m   R a n k i n g s\r\n\r\n"
	    "Rank  Rank%        Score        Team   Team Name\r\n"
	    "==== ======= ================= ====== "
	    "========================================\r\n", error))
		goto failure;
	if (team_denominator > 0) {
		int rank = 0;
		for (index = 0; index < YT_DEFAULT_PLAYER_COUNT; ++index) {
			struct yt_sector overlay;
			char team_name[42];
			uint32_t physical_record;

			if (teams[index].score <= 0)
				continue;
			++rank;
			if (!score_read_sector(game, sector_record_offset,
			    teams[index].id, &overlay, &physical_record, error)) {
				goto failure;
			}
			score_field_observe(field, YT_SCORE_FIELD_TEAM,
			    physical_record, &overlay.record);
			yt_record_get_text(&overlay.record, team_name, sizeof(team_name));
			if (!format_team_row(line, sizeof(line), rank,
			    teams[index].score / team_denominator * 100.0,
			    teams[index].score, teams[index].id, team_name)) {
				if (error != NULL)
					error->status = YT_RANGE;
				goto failure;
			}
			if (!write_bytes(&output, line, error))
				goto failure;
		}
	}
	if (!write_bytes(&output,
	    "\r\nN o n  -  H u m a n   P l a y e r s\r\n\r\n"
	    "    The Xannor       Rank%     The Mercenaries   Rank%\r\n"
	    "================== =========  ================= =======\r\n",
	    error))
		goto failure;
	if (denominator == 0)
		return scoreboard_division_error(&output, path, error);
	if (!format_nonhuman_row(line, sizeof(line), xannor,
	    xannor / denominator * 100.0, mercenaries,
	    mercenaries / denominator * 100.0)) {
		if (error != NULL)
			error->status = YT_RANGE;
		goto failure;
	}
	if (!write_bytes(&output, line, error))
		goto failure;
	if (!yt_text_output_close(&output, error)) {
		yt_text_output_destroy(&output);
		return false;
	}
	yt_text_output_destroy(&output);
	return true;

failure:
	(void)yt_text_output_close_all_method(&output, 0, NULL);
	yt_text_output_destroy(&output);
	return false;
}

bool
yt_score_generate_progress_observed(struct yt_game *game,
    yt_score_progress_fn progress, void *context,
    struct yt_score_field_observation *field, struct yt_error *error)
{
	return yt_score_generate_progress_process_observed(game,
	    game->config.sector_offset, game->config.port_offset, progress,
	    context, field, NULL, NULL, NULL, NULL, error);
}

bool
yt_score_generate_progress(struct yt_game *game,
    yt_score_progress_fn progress, void *context, struct yt_error *error)
{
	return yt_score_generate_progress_observed(game, progress, context,
	    NULL, error);
}

bool
yt_score_generate(struct yt_game *game, struct yt_error *error)
{
	return yt_score_generate_progress(game, NULL, NULL, error);
}
