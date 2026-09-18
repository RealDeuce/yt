#include "yt_score.h"
#include "qb.h"
#include "yt_score_format.h"
#include "yt_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
	(void)yt_text_output_close_all(output, NULL);
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
    double score, const char *team, uint16_t ports, const char *name)
{
	char rank_text[32];
	char percentage_text[64];
	char score_text[160];
	char team_text[7];
	char ports_text[32];
	int written;

	if (!yt_score_format_single(rank_text, sizeof(rank_text), (float)rank,
	    YT_SCORE_FIELD_RANK))
		return false;
	if (!yt_score_format_double(percentage_text,
	    sizeof(percentage_text), percentage, YT_SCORE_FIELD_PERCENT))
		return false;
	if (!yt_score_format_double(score_text, sizeof(score_text), score,
	    YT_SCORE_FIELD_SCORE))
		return false;
	if (!fixed_string(team_text, team))
		return false;
	if (!yt_score_format_single(ports_text, sizeof(ports_text), (float)ports,
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
	    YT_SCORE_FIELD_RANK))
		return false;
	if (!yt_score_format_double(percentage_text,
	    sizeof(percentage_text), percentage, YT_SCORE_FIELD_PERCENT))
		return false;
	if (!yt_score_format_double(score_text, sizeof(score_text), score,
	    YT_SCORE_FIELD_SCORE))
		return false;
	if (!yt_score_format_single(team_text, sizeof(team_text),
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
	    (float)xannor, YT_SCORE_FIELD_SCORE))
		return false;
	if (!yt_score_format_double(xannor_percentage_text,
	    sizeof(xannor_percentage_text), xannor_percentage,
	    YT_SCORE_FIELD_XANNOR_PERCENT))
		return false;
	if (!yt_score_format_single(mercenary_text,
	    sizeof(mercenary_text), (float)mercenaries,
	    YT_SCORE_FIELD_SCORE))
		return false;
	if (!yt_score_format_double(mercenary_percentage_text,
	    sizeof(mercenary_percentage_text), mercenary_percentage,
	    YT_SCORE_FIELD_PERCENT))
		return false;
	written = snprintf(dest, size, "  %s  %s%%    %s  %s%%\r\n\r\n",
	    xannor_text, xannor_percentage_text, mercenary_text,
	    mercenary_percentage_text);
	return written >= 0 && (size_t)written < size;
}

static bool
score_read_sector(struct yt_game *game, int sector_record_offset,
    int logical_sector, struct yt_sector *sector, struct yt_error *error)
{
	struct yt_record record;
	int physical = sector_record_offset + logical_sector;

	if (!yt_database_read(&game->database, (size_t)physical, &record, error))
		return false;
	yt_sector_decode(sector, &record);
	return true;
}

static double
base_score(const struct yt_player *player)
{
	float score = 0;

	if (player->killed_by != 0)
		return 0;
	score = (score + (player->shields * 50.0f));
	score = (score + (player->fighters * 100.0f));
	score = (score + (player->holds * 2500.0f));
	score = (score + (player->ore * 20.0f));
	score = (score + (player->organics * 30.0f));
	score = (score + (player->equipment * 40.0f));
	score = (score + (
	    (float)player->ports_owned * 50000.0f));
	score = (score + (player->missiles * 1000.0f));
	score = (score + (player->ground_forces * 750.0f));
	score = (score + (player->mines * 2500.0f));
	score = (score + player->credits);
	return (double)score
	    + (double)(player->plasma * 16000000.0f)
	    + (player->danger_scanner != 0 ? 250000.0 : 0.0);
}

static void
sort_players(struct yt_score_player *players, size_t count)
{
	bool changed;
	size_t index;

	do {
		changed = false;
		for (index = 1; index < count; ++index) {
			if (players[index].score > players[index - 1].score) {
				struct yt_score_player swap = players[index];
				players[index] = players[index - 1];
				players[index - 1] = swap;
				changed = true;
			}
		}
	} while (changed);
}

static void
sort_teams(struct yt_score_team *teams, size_t count)
{
	bool changed;
	size_t index;

	do {
		changed = false;
		for (index = 1; index < count; ++index) {
			if (teams[index].score > teams[index - 1].score) {
				struct yt_score_team swap = teams[index];
				teams[index] = teams[index - 1];
				teams[index - 1] = swap;
				changed = true;
			}
		}
	} while (changed);
}

bool
yt_scoreboard_prepare(struct yt_scoreboard *scoreboard, struct yt_game *game,
    int sector_record_offset, int port_record_offset,
    struct yt_error *error)
{
	int index;

	if (scoreboard == NULL || game == NULL)
		return false;
	memset(scoreboard, 0, sizeof(*scoreboard));
	scoreboard->game = game;
	scoreboard->sector_record_offset = sector_record_offset;
	scoreboard->player_count = sector_record_offset - 1;
	scoreboard->sector_count = port_record_offset - sector_record_offset;
	if (scoreboard->player_count < 0
	    || scoreboard->player_count > YT_DEFAULT_PLAYER_COUNT) {
		if (error != NULL)
			error->status = YT_RANGE;
		return false;
	}
	for (index = 0; index < YT_DEFAULT_PLAYER_COUNT; ++index) {
		scoreboard->players[index].record = index + 2;
		scoreboard->teams[index].id = index + 1;
	}
	return true;
}

bool
yt_scoreboard_load_players(struct yt_scoreboard *scoreboard,
    struct yt_error *error)
{
	int index;

	if (scoreboard == NULL || scoreboard->game == NULL)
		return false;
	for (index = 0; index < scoreboard->player_count; ++index) {
		struct yt_score_player *player = &scoreboard->players[index];

		if (!yt_game_read_player(scoreboard->game, index + 2,
		    &player->player, error))
			return false;
		player->occupied = player->player.name_length != 0;
		if (player->occupied)
			player->score = base_score(&player->player);
	}
	return true;
}

bool
yt_scoreboard_score_sectors(struct yt_scoreboard *scoreboard,
    struct yt_error *error)
{
	int index;

	if (scoreboard == NULL || scoreboard->game == NULL)
		return false;
	for (index = 1; index <= scoreboard->sector_count; ++index) {
		struct yt_sector sector;
		double contribution;
		int owner;

		if (!score_read_sector(scoreboard->game,
		    scoreboard->sector_record_offset, index, &sector, error))
			return false;
		contribution = (double)(sector.fighters * 100.0f);
		owner = sector.fighter_owner;
		if (owner == -1)
			scoreboard->xannor += contribution;
		else if (owner == -2)
			scoreboard->mercenaries += contribution;
		else if (owner >= 2 && owner <= scoreboard->player_count + 1)
			scoreboard->players[owner - 2].score += contribution;
	}
	for (index = 0; index < scoreboard->player_count; ++index) {
		struct yt_score_player *player = &scoreboard->players[index];
		struct yt_player cached;

		if (!yt_game_read_player(scoreboard->game, player->record, &cached,
		    error))
			return false;
		cached.score = player->occupied ? (float)player->score : -1.0f;
		yt_player_encode(&cached);
		if (!yt_game_write_player(scoreboard->game, player->record, &cached,
		    error))
			return false;
		player->player.score = cached.score;
		if (!player->occupied)
			continue;
		if (player->player.team >= 1 && player->player.team <= 50)
			scoreboard->teams[player->player.team - 1].score
			    += player->score;
	}
	return true;
}

void
yt_scoreboard_rank_players(struct yt_scoreboard *scoreboard)
{
	if (scoreboard != NULL)
		sort_players(scoreboard->players, (size_t)scoreboard->player_count);
}

bool
yt_scoreboard_write(struct yt_scoreboard *scoreboard, struct yt_error *error)
{
	struct yt_game *game;
	double denominator;
	double team_denominator;
	struct yt_clock_value date_now;
	struct yt_clock_value time_now;
	char date[11];
	char time_text[9];
	char line[256];
	char path[512];
	struct yt_text_output output;
	int index;

	if (scoreboard == NULL || scoreboard->game == NULL)
		return false;
	game = scoreboard->game;
	sort_teams(scoreboard->teams, YT_ARRAY_LEN(scoreboard->teams));
	denominator = scoreboard->player_count > 0
	    ? scoreboard->players[0].score : 0;
	if (denominator == 0)
		denominator = scoreboard->xannor > scoreboard->mercenaries
		    ? scoreboard->xannor : scoreboard->mercenaries;
	team_denominator = scoreboard->teams[0].score;
	yt_text_output_init(&output);
	if (!open_scoreboard(game, &output, path, error))
		return false;
	if (!write_bytes(&output, "\r\n"
	    "Y a n k e e   T r a d e r   S c o r e b o a r d\r\n\r\n",
	    error))
		goto failure;
	if (!yt_clock_read(&game->clock, &date_now, error))
		goto failure;
	if (!yt_clock_read(&game->clock, &time_now, error))
		goto failure;
	yt_format_date(&date_now, date);
	yt_format_time(&time_now, time_text);
	snprintf(line, sizeof(line), "Last updated at: %s %s\r\n\r\n", date,
	    time_text);
	if (!write_bytes(&output, line, error))
		goto failure;
	if (!write_bytes(&output,
	    "Rank  Rank%        Score        Team   Ports   Player\r\n"
	    "==== ======= ================= ====== ======= "
	    "================================\r\n", error))
		goto failure;
	{
		int rank = 0;

		for (index = 0; index < scoreboard->player_count; ++index) {
			struct yt_score_player *player = &scoreboard->players[index];
			struct yt_player row_player;
			char team_text[32];

			if (!player->occupied)
				continue;
			if (!yt_game_read_player(game, player->record, &row_player,
			    error))
				goto failure;
			++rank;
			if (denominator == 0)
				return scoreboard_division_error(&output, path, error);
			if (row_player.team == 0)
				strcpy(team_text, "None");
			else {
				size_t length;

				qb_str_single(team_text, sizeof(team_text),
				    (float)row_player.team);
				length = strlen(team_text);
				if (length + 1U < sizeof(team_text)) {
					team_text[length] = ' ';
					team_text[length + 1U] = '\0';
				}
			}
			if (!format_player_row(line, sizeof(line), rank,
			    player->score / denominator * 100.0, player->score,
			    team_text, row_player.ports_owned,
			    row_player.name)) {
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
			struct yt_score_team *team = &scoreboard->teams[index];
			struct yt_sector overlay;
			char team_name[42];

			if (team->score <= 0)
				continue;
			++rank;
			if (!score_read_sector(game, scoreboard->sector_record_offset,
			    team->id, &overlay, error))
				goto failure;
			yt_record_get_text(&overlay.record, team_name,
			    sizeof(team_name));
			if (!format_team_row(line, sizeof(line), rank,
			    team->score / team_denominator * 100.0, team->score,
			    team->id, team_name)) {
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
	if (!format_nonhuman_row(line, sizeof(line), scoreboard->xannor,
	    scoreboard->xannor / denominator * 100.0, scoreboard->mercenaries,
	    scoreboard->mercenaries / denominator * 100.0)) {
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
	(void)yt_text_output_close_all(&output, NULL);
	yt_text_output_destroy(&output);
	return false;
}

bool
yt_score_generate(struct yt_game *game, struct yt_error *error)
{
	struct yt_scoreboard scoreboard;

	if (!yt_scoreboard_prepare(&scoreboard, game,
	    (int)game->config.sector_offset, (int)game->config.port_offset, error))
		return false;
	if (!yt_scoreboard_load_players(&scoreboard, error))
		return false;
	if (!yt_scoreboard_score_sectors(&scoreboard, error))
		return false;
	yt_scoreboard_rank_players(&scoreboard);
	return yt_scoreboard_write(&scoreboard, error);
}
