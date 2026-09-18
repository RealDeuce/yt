#ifndef TESTS_INFO_PANEL_MODEL_H
#define TESTS_INFO_PANEL_MODEL_H

#include "yt_game.h"

enum yt_info_panel_output_kind {
	YT_INFO_PANEL_LINE,
	YT_INFO_PANEL_FIXED,
};

struct yt_info_panel_state {
	const uint8_t *cached_name;
	size_t cached_name_length;
	float anti_cloak;
	int foreground;
	int background;
	bool bold;
	uint8_t time_text[64];
	size_t time_text_length;
	struct yt_player player;
};

typedef bool (*yt_info_panel_refresh_fn)(void *context, uint8_t *text,
	size_t capacity, size_t *length, struct yt_error *error);
typedef bool (*yt_info_panel_team_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_info_panel_read_player_fn)(void *context,
	struct yt_player *player, struct yt_error *error);
typedef bool (*yt_info_panel_present_fn)(void *context,
	const uint8_t *text, size_t length, enum yt_info_panel_output_kind kind,
	uint8_t width, struct yt_info_panel_state *state,
	struct yt_error *error);

struct yt_info_panel_ops {
	yt_info_panel_refresh_fn refresh_time;
	yt_info_panel_team_fn team;
	yt_info_panel_read_player_fn read_player;
	yt_info_panel_present_fn present;
};

bool yt_info_panel_run(struct yt_info_panel_state *state,
	const struct yt_info_panel_ops *ops, void *context,
	struct yt_error *error);

#endif
