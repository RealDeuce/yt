#ifndef YT_PAGER_H
#define YT_PAGER_H

#include "yt_presentation.h"

#define YT_PAGER_KEY_SIZE 80U

struct yt_pager_state {
	float line_count;
	float nonstop;
	float newline_flag;
	char key[YT_PAGER_KEY_SIZE];
	int foreground;
};

bool yt_pager_advance(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int *saved_foreground);
void yt_pager_editor_enter(struct yt_pager_state *pager,
    char *accumulator, size_t accumulator_capacity);
bool yt_pager_accept_response(struct yt_pager_state *pager, char *response,
    size_t response_capacity);
void yt_pager_complete(struct yt_pager_state *pager,
    struct yt_present_state *presentation, int saved_foreground);

#endif
