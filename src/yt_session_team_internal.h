#ifndef YT_SESSION_TEAM_INTERNAL_H
#define YT_SESSION_TEAM_INTERNAL_H

#include "yt_session_internal.h"

bool session_team_store_inactive(struct yt_session *session,
    struct yt_team *team, struct yt_error *error);
bool session_team_read_overlay(struct yt_session *session, int id,
    struct yt_team *team, struct yt_error *error);
bool session_team_store_roster(struct yt_session *session,
    struct yt_team *team, struct yt_error *error);
bool session_team_audit(struct yt_session *session, int team_id,
    enum yt_team_audit_event event, const char *attempt,
    struct yt_error *error);
bool session_team_pick_name(struct yt_session *session, int team_id,
    char name[42], bool *accepted, struct yt_error *error);
bool session_team_create_password(struct yt_session *session, int team_id,
    char password[5], struct yt_error *error);
bool session_team_create(struct yt_session *session, struct yt_error *error);
bool session_team_join(struct yt_session *session, struct yt_error *error);
bool session_team_quit(struct yt_session *session, struct yt_team *team,
    struct yt_error *error);
bool session_team_search(struct yt_session *session, struct yt_error *error);
bool session_team_transfer(struct yt_session *session, struct yt_error *error);
bool session_team_banish(struct yt_session *session, struct yt_team *team,
    struct yt_error *error);

#endif
