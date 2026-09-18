#include "player_death_model.h"

void
test_death_team_roster_overlay(struct yt_record *record, float victim)
{
	static const size_t offsets[4] = {
		YT_F109, YT_F117, YT_F121, YT_F125
	};
	size_t index;

	if (record == NULL)
		return;
	for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); ++index)
		if (yt_record_get_number(record, offsets[index]) == victim)
			(void)yt_record_set_number(record, offsets[index], 0.0f);
}

bool
test_player_death_run(struct test_player_death_state *state,
    const struct test_player_death_ops *ops, void *context,
    struct yt_error *error)
{
	static const uint8_t active_cache_zero[4] = {
		0x00U, 0x00U, 0x7aU, 0x00U
	};
	struct yt_player player;
	uint8_t row[300];
	size_t row_length;
	int logical;
	bool self;
	bool valid_killer;

	if (state == NULL || ops == NULL || ops->clear_active_cache == NULL
	    || ops->read_player == NULL || ops->write_player == NULL
	    || ops->read_sector == NULL || ops->write_sector == NULL
	    || ops->remove_team == NULL || ops->read_port == NULL
	    || ops->write_port == NULL || ops->present == NULL
	    || ops->news == NULL || ops->set_current_player == NULL
	    || ops->flush == NULL
	    || (state->current_name == NULL && state->current_name_length != 0U))
		return false;
	state->complete = false;
	state->matched_ports = 0;
	ops->clear_active_cache(context, state->victim_record,
	    active_cache_zero);
	if (!ops->read_player(context, state->victim_record, &player, error))
		return false;
	state->victim_name_length = yt_player_stored_name(&player,
	    state->victim_name);
	state->old_ports_owned = player.ports_owned;
	yt_death_player_overlay(&player, state->killer);
	state->victim = player;
	if (!ops->write_player(context, state->victim_record, &player, error))
		return false;
	for (logical = 1; logical <= state->sector_count; ++logical) {
		struct yt_sector sector;

		if (!ops->read_sector(context, logical, &sector, error))
			return false;
		if (yt_death_sector_overlay(&sector, state->victim_record)
		    && !ops->write_sector(context, logical, &sector, error))
			return false;
	}
	if (!ops->remove_team(context, state->victim_record, error))
		return false;
	if (state->old_ports_owned != 0) {
		for (logical = 1; logical <= state->port_count; ++logical) {
			struct yt_port port;
			enum yt_death_port_route route;

			if (!ops->read_port(context, logical, &port, error))
				return false;
			route = yt_death_port_overlay(&port,
			    state->victim_record, state->killer,
			    state->last_player_record);
			if (route == YT_DEATH_PORT_UNMATCHED)
				continue;
			++state->matched_ports;
			if (!ops->write_port(context, logical, &port, error))
				return false;
		}
	}
	valid_killer = (state->killer != state->victim_record)
	    & (state->killer > 1)
	    & (state->killer <= state->last_player_record);
	if (valid_killer && state->matched_ports != 0) {
		if (!yt_death_title_row(state->victim_name,
		    state->victim_name_length, state->matched_ports,
		    row, sizeof(row),
		    &row_length)
		    || !ops->present(context, row, row_length, error)
		    || !ops->read_player(context, state->killer, &player,
		    error))
			return false;
		yt_death_killer_credit_overlay(&player, state->matched_ports);
		if (!ops->write_player(context, state->killer, &player,
		    error))
			return false;
	}
	self = state->killer == state->victim_record;
	if (!self
	    && !ops->read_player(context, state->victim_record, &player, error))
		return false;
	if (!yt_death_kill_news_row(state->current_name,
	    state->current_name_length, state->victim_name,
	    state->victim_name_length, self, row, sizeof(row), &row_length)
	    || !ops->news(context, row, row_length, error))
		return false;
	if (!self && state->matched_ports != 0) {
		if (!yt_death_port_news_row(state->victim_name,
		    state->victim_name_length, state->matched_ports,
		    row, sizeof(row),
		    &row_length)
		    || !ops->news(context, row, row_length, error))
			return false;
	}
	if (state->victim_record == state->current_player_record)
		ops->set_current_player(context, &state->victim);
	if (!ops->flush(context, error))
		return false;
	state->complete = true;
	return true;
}
