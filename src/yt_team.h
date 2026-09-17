#ifndef YT_TEAM_H
#define YT_TEAM_H

#include "yt_game.h"

enum yt_team_audit_event {
	YT_TEAM_AUDIT_INVALID_PASSWORD,
	YT_TEAM_AUDIT_JOIN,
	YT_TEAM_AUDIT_QUIT,
};

enum yt_info_team_row_kind {
	YT_INFO_TEAM_SUMMARY,
	YT_INFO_TEAM_SELF_CAPTAIN,
	YT_INFO_TEAM_OTHER_CAPTAIN,
};

struct yt_team_cache {
	int roster[4];
	int captain;
	bool current_player_is_captain;
	char name[YT_TEXT_FIELD_SIZE + 1U];
	size_t name_length;
	char password[5];
};

bool yt_team_audit_message(enum yt_team_audit_event event,
    const char *player_name, const char *attempt, const char *date,
    const char *time_text, uint8_t *message, size_t capacity,
    size_t *length);
bool yt_info_team_row(enum yt_info_team_row_kind kind, int team_id,
    const uint8_t *name, size_t name_length, uint8_t *row,
    size_t capacity, size_t *length);
void yt_team_cache_load(struct yt_team_cache *cache,
    const struct yt_record *record, int current_player_record, bool *live);
bool yt_team_choice_rejected(float choice, int team,
    int32_t captain_cint);
void yt_team_transfer_apply_sector(struct yt_sector *sector,
    double initial_fighters, float amount);
void yt_team_transfer_apply_player(struct yt_player *player, float amount);
void yt_team_membership_apply_player(struct yt_player *player, int team);
void yt_team_banish_apply_player(struct yt_player *player);
void yt_team_roster_overlay(struct yt_record *record, const int roster[4]);
void yt_team_name_overlay(struct yt_record *record, const uint8_t *name,
    size_t length);
bool yt_team_prepare_name(char *name, size_t *length);
void yt_team_password_overlay(struct yt_record *record,
    const uint8_t password[4]);
void yt_team_inactive_overlay(struct yt_record *record);

#endif
