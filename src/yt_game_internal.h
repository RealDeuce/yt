#ifndef YT_GAME_INTERNAL_H
#define YT_GAME_INTERNAL_H

#include "yt_game.h"

struct yt_game_row_builder {
	uint8_t *row;
	size_t capacity;
	size_t length;
};

bool yt_game_error(struct yt_error *error, enum yt_status status,
    const char *operation);
bool yt_game_join_parts(const uint8_t *first, size_t first_length,
    const uint8_t *second, size_t second_length,
    const uint8_t *third, size_t third_length,
    const uint8_t *fourth, size_t fourth_length,
    const uint8_t *fifth, size_t fifth_length,
    uint8_t *row, size_t capacity, size_t *length);
bool yt_game_row_append(struct yt_game_row_builder *builder,
    const void *data, size_t length);
bool yt_game_row_number(struct yt_game_row_builder *builder, float value,
    bool promoted);

#endif
