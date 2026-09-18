#include "yt_maint.h"

#include "yt_maint_internal.h"

#include "qb.h"

#include <errno.h>
#include <string.h>

static void
set_error(struct yt_error *error, enum yt_status status,
    const char *operation, const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s",
	    path != NULL ? path : "");
}

static bool
current_day_minute(struct yt_game *game, float *day, float *minute,
    struct yt_error *error)
{
	int serial;

	if (!yt_current_date_serial(&game->clock,
	    (float)game->config.epoch_year, &serial,
	    NULL, error))
		return false;
	*day = (float)serial;
	*minute = (float)(yt_clock_timer(&game->clock) / 60.0);
	return true;
}

static float
elapsed_days(float day, float minute, float old_day, float old_minute)
{
	float elapsed = qb_single_add(qb_single_subtract(day, old_day),
	    qb_single_divide(qb_single_subtract(minute, old_minute), 1440.0f));

	if (elapsed > 10.0f || elapsed < 0.0f)
		elapsed = 10.0f;
	return elapsed;
}

bool
yt_maintenance_update_port(struct yt_random *random, struct yt_port *port,
    float current_day, float current_minute,
    bool *plagued, struct yt_error *error)
{
	double stock[3];
	float elapsed;
	int commodity;

	if (random == NULL || port == NULL || plagued == NULL) {
		set_error(error, YT_INVALID, "maintain port", "YTDATA.DAT");
		return false;
	}
	elapsed = elapsed_days(current_day, current_minute, port->last_day,
	    port->last_minute);
	for (commodity = 0; commodity < 3; ++commodity) {
		stock[commodity] = (double)port->stock[commodity]
		    + (double)qb_single_multiply(port->production[commodity], elapsed);
		if (stock[commodity] / 10.0
		    > (double)port->production[commodity])
			port->production[commodity] =
			    (float)(stock[commodity] / 10.0);
	}
	*plagued = qb_single_add(qb_single_add(port->production[0],
	    port->production[1]), port->production[2]) > 16000000.0f;
	if (*plagued) {
		float maximum = 0.0f;
		int selected = 0;

		for (commodity = 0; commodity < 3; ++commodity) {
			if (port->production[commodity] > 500.0f) {
				float sample;

				if (!yt_random_next(random, &sample, error))
					return false;
				port->production[commodity] = qb_single_add(
				    qb_single_multiply(sample, port->production[commodity]),
				    500.0f);
			}
		}
		for (commodity = 0; commodity < 3; ++commodity) {
			float cap = qb_single_multiply(port->production[commodity], 10.0f);

			if (stock[commodity] > (double)cap)
				stock[commodity] = (double)cap;
			if (stock[commodity] > (double)maximum) {
				maximum = (float)stock[commodity];
				selected = commodity + 1;
			}
			port->factor[commodity] =
			    -fabsf(port->factor[commodity]);
		}
		port->commodity_class = 4 - selected;
		if (selected > 0)
			port->factor[selected - 1] =
			    fabsf(port->factor[selected - 1]);
	}
	for (commodity = 0; commodity < 3; ++commodity)
		port->stock[commodity] = (float)stock[commodity];
	port->last_day = current_day;
	port->last_minute = current_minute;
	return true;
}

static bool
maintenance_write_port(struct yt_game *game, int logical,
    struct yt_port *port, struct yt_error *error)
{
	int commodity;

	if (!yt_record_set_number(&port->record, YT_F41,
	    (float)port->commodity_class)) {
		set_error(error, YT_RANGE, "encode maintained port", "YTDATA.DAT");
		return false;
	}
	if (!yt_record_set_number(&port->record, YT_F45, port->last_day)) {
		set_error(error, YT_RANGE, "encode maintained port", "YTDATA.DAT");
		return false;
	}
	for (commodity = 0; commodity < 3; ++commodity) {
		if (!yt_record_set_number(&port->record,
		    YT_F49 + (size_t)commodity * 4U, port->stock[commodity])) {
			set_error(error, YT_RANGE, "encode maintained port",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_record_set_number(&port->record,
		    YT_F61 + (size_t)commodity * 4U,
		    port->production[commodity])) {
			set_error(error, YT_RANGE, "encode maintained port",
			    "YTDATA.DAT");
			return false;
		}
		if (!yt_record_set_number(&port->record,
		    YT_F73 + (size_t)commodity * 4U,
		    port->factor[commodity])) {
			set_error(error, YT_RANGE, "encode maintained port",
			    "YTDATA.DAT");
			return false;
		}
	}
	if (!yt_record_set_number(&port->record, YT_F101,
	    port->last_minute)) {
		set_error(error, YT_RANGE, "encode maintained port", "YTDATA.DAT");
		return false;
	}
	return yt_database_write(&game->database,
	    (size_t)yt_port_basic_record(&game->config, logical),
	    &port->record, error);
}

bool
yt_maintenance_maintain_ports(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	int port_count;
	int plagued = 0;
	int logical;

	if (game == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "maintain ports", "YTDATA.DAT");
		return false;
	}
	port_count = (int)game->config.planet_offset
	    - (int)game->config.port_offset;
	if (port_count < 1 || port_count > 1000) {
		set_error(error, YT_RANGE, "maintain ports", "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_compose_port_phase(blank,
	    blank_length, 0, &output)) {
		set_error(error, YT_RANGE, "maintain ports", "YTDATA.DAT");
		return false;
	}
	for (logical = 0; logical < 2; ++logical) {
		if (!line_output(line_context, output.rows[logical].data,
		    output.rows[logical].length, error))
			return false;
	}
	for (logical = 1; logical <= port_count; ++logical) {
		struct yt_port port;
		float day;
		float minute;
		bool port_plagued;

		if (!yt_game_read_port(game, logical, &port, error))
			return false;
		if (!current_day_minute(game, &day, &minute, error))
			return false;
		if (!yt_maintenance_update_port(&game->random, &port, day,
		    minute, &port_plagued, error))
			return false;
		if (!maintenance_write_port(game, logical, &port, error))
			return false;
		if (port_plagued)
			++plagued;
	}
	if (!yt_maintenance_compose_port_phase(blank,
	    blank_length, plagued, &output)) {
		set_error(error, YT_RANGE, "compose port output", "");
		return false;
	}
	if (plagued != 0) {
		if (!line_output(line_context, output.rows[2].data,
		    output.rows[2].length, error))
			return false;
		if (!line_output(line_context, output.rows[3].data,
		    output.rows[3].length, error))
			return false;
		if (!yt_news_append_bytes(output.rows[3].data,
		    output.rows[3].length, error))
			return false;
	}
	return true;
}

bool
yt_maintenance_update_planet(struct yt_random *random,
    struct yt_planet *planet, float day, float minute,
    struct yt_maintenance_planet_result *result, struct yt_error *error)
{
	const float one_percent = 0.009999999776482582f;
	const float missile_multiplier = 0.000009999999747378752f;
	const float mine_multiplier = 0.000003999999989900971f;
	float elapsed;
	float production[9];
	float quantity[9];
	float contribution[9];
	float sum;
	float old_total;
	float old_ground;
	float old_bank;
	float first;
	float second;
	float third;
	enum yt_maintenance_planet_event event =
	    YT_MAINTENANCE_PLANET_NO_EVENT;
	int index;

	if (random == NULL || planet == NULL || result == NULL) {
		set_error(error, YT_INVALID, "maintain planet", "YTDATA.DAT");
		return false;
	}
	memset(result, 0, sizeof(*result));
	for (index = 0; index < 3; ++index) {
		production[index] = planet->production[index];
		quantity[index] = planet->stock[index];
	}
	quantity[3] = planet->fighters;
	quantity[4] = planet->missiles;
	quantity[5] = planet->mines;
	quantity[6] = planet->bank;
	quantity[7] = planet->ground_forces;
	quantity[8] = planet->plasma;
	sum = qb_single_add(qb_single_add(production[0], production[1]), production[2]);
	production[3] = yt_maintenance_sint(sum);
	production[4] = yt_maintenance_sint(qb_single_divide(sum, 2500.0f));
	production[5] = yt_maintenance_sint(qb_single_divide(sum, 25000.0f));
	production[6] = 0.0f;
	production[7] = 0.0f;
	production[8] = yt_maintenance_sint(qb_single_multiply(sum, missile_multiplier));
	elapsed = elapsed_days(day, minute, planet->last_day,
	    planet->last_minute);
	old_bank = quantity[6];
	contribution[0] = qb_single_divide(old_bank, 10000.0f);
	contribution[1] = qb_single_divide(old_bank, 20000.0f);
	contribution[2] = qb_single_divide(old_bank, 30000.0f);
	contribution[3] = qb_single_divide(old_bank, 500.0f);
	contribution[4] = qb_single_multiply(old_bank, missile_multiplier);
	contribution[5] = qb_single_multiply(old_bank, mine_multiplier);
	contribution[6] = 0.0f;
	contribution[7] = qb_single_divide(old_bank, 10000.0f);
	contribution[8] = (float)((double)old_bank * 0.00000004);

	quantity[6] = yt_maintenance_sint(qb_single_add(quantity[6],
	    qb_single_multiply(qb_single_multiply(quantity[6], elapsed), one_percent)));
	quantity[7] = yt_maintenance_sint(qb_single_add(qb_single_add(quantity[7],
	    qb_single_multiply(qb_single_multiply(quantity[7], elapsed), one_percent)),
	    qb_single_multiply(contribution[7], elapsed)));
	for (index = 0; index < 3; ++index)
		production[index] = qb_single_add(production[index],
		    qb_single_multiply(qb_single_multiply(production[index], elapsed), one_percent));
	for (index = 0; index < 6; ++index) {
		quantity[index] = qb_single_add(quantity[index],
		    qb_single_multiply(qb_single_add(production[index], contribution[index]), elapsed));
		if (index < 3
		    && quantity[index] > qb_single_multiply(production[index], 10.0f))
			production[index] = qb_single_divide(quantity[index], 10.0f);
	}
	quantity[8] = qb_single_add(quantity[8],
	    qb_single_multiply(qb_single_add(production[8], contribution[8]), elapsed));
	for (index = 0; index < 3; ++index) {
		if (production[index] < 1.0f)
			production[index] = 1.0f;
	}

	old_total = qb_single_add(qb_single_add(production[0], production[1]), production[2]);
	old_ground = quantity[7];
	if (!yt_random_next(random, &first, error))
		return false;
	if (!yt_random_next(random, &second, error))
		return false;
	if (qb_single_multiply(first, old_total)
	    > qb_single_add(qb_single_multiply(second, 16000000.0f), 100000.0f))
		event = YT_MAINTENANCE_PLANET_PLAGUE;
	if (!yt_random_next(random, &third, error))
		return false;
	if (qb_single_multiply(third, old_ground) > 16000000.0f)
		event = YT_MAINTENANCE_PLANET_CIVIL_WAR;
	if (event != YT_MAINTENANCE_PLANET_NO_EVENT) {
		float expense = 0.0f;

		for (index = 0; index < 3; ++index) {
			float sample;

			if (!yt_random_next(random, &sample, error))
				return false;
			production[index] = qb_single_multiply(sample, production[index]);
		}
		if (quantity[7] > 0.0f) {
			float a;
			float b;

			if (!yt_random_next(random, &a, error))
				return false;
			if (!yt_random_next(random, &b, error))
				return false;
			quantity[7] = qb_single_subtract(quantity[7],
			    qb_single_multiply(qb_single_multiply(quantity[7], a), b));
		}
		for (index = 0; index < 3; ++index) {
			float cap = qb_single_multiply(production[index], 10.0f);

			if (quantity[index] > cap)
				quantity[index] = cap;
		}
		if (event == YT_MAINTENANCE_PLANET_CIVIL_WAR) {
			float sample;

			if (!yt_random_next(random, &sample, error))
				return false;
			expense = yt_maintenance_sint(qb_single_multiply(sample, quantity[6]));
			quantity[6] = (float)((double)quantity[6]
			    - (double)expense);
		}
		result->civil_war_expense = expense;
	}

	for (index = 0; index < 3; ++index) {
		planet->production[index] = production[index];
		planet->stock[index] = quantity[index];
	}
	planet->fighters = quantity[3];
	planet->missiles = quantity[4];
	planet->mines = quantity[5];
	planet->bank = quantity[6];
	planet->ground_forces = quantity[7];
	planet->plasma = quantity[8];
	planet->last_day = day;
	planet->last_minute = minute;
	result->event = event;
	result->old_event_total = old_total;
	result->new_event_total = qb_single_add(qb_single_add(production[0], production[1]),
	    production[2]);
	result->old_event_ground = old_ground;
	result->new_event_ground = quantity[7];
	result->emit_ground_line = floorf(quantity[7]) != floorf(old_ground)
	    && floorf(quantity[7]) > 0.0f;
	return true;
}

static bool
maintenance_write_planet(struct yt_game *game, int logical,
    struct yt_planet *planet, struct yt_error *error)
{
	static const size_t production_offsets[] = {YT_F45, YT_F49, YT_F53};
	static const size_t stock_offsets[] = {YT_F57, YT_F61, YT_F65};
	int index;

	if (!yt_record_set_number(&planet->record, YT_F41, planet->last_day))
		goto range;
	for (index = 0; index < 3; ++index) {
		if (!yt_record_set_number(&planet->record,
		    production_offsets[index], planet->production[index]))
			goto range;
		if (!yt_record_set_number(&planet->record,
		    stock_offsets[index], planet->stock[index]))
			goto range;
	}
	if (!yt_record_set_number(&planet->record, YT_F69, planet->missiles))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F77,
	    planet->ground_forces))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F89,
	    planet->last_minute))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F113, planet->plasma))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F117, planet->bank))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F125, planet->mines))
		goto range;
	if (!yt_record_set_number(&planet->record, YT_F129,
	    planet->fighters))
		goto range;
	return yt_database_write(&game->database,
	    (size_t)yt_planet_basic_record(&game->config, logical),
	    &planet->record, error);

range:
	set_error(error, YT_RANGE, "encode maintained planet", "YTDATA.DAT");
	return false;
}

bool
yt_maintenance_maintain_planets(struct yt_game *game,
    const uint8_t *blank, size_t blank_length,
    yt_maintenance_score_line_fn line_output, void *line_context,
    struct yt_error *error)
{
	struct yt_maintenance_output_result output;
	int planet_count;
	int logical;

	if (game == NULL || line_output == NULL
	    || (blank == NULL && blank_length != 0U)) {
		set_error(error, YT_INVALID, "maintain planets", "YTDATA.DAT");
		return false;
	}
	planet_count = (int)game->config.total_records
	    - (int)game->config.planet_offset;
	if (planet_count < 1 || planet_count > 100) {
		set_error(error, YT_RANGE, "maintain planets", "YTDATA.DAT");
		return false;
	}
	if (!yt_maintenance_compose_planet_phase(blank,
	    blank_length, NULL, NULL, &output)) {
		set_error(error, YT_RANGE, "maintain planets", "YTDATA.DAT");
		return false;
	}
	for (logical = 0; logical < 2; ++logical) {
		if (!line_output(line_context, output.rows[logical].data,
		    output.rows[logical].length, error))
			return false;
	}
	for (logical = 1; logical <= planet_count; ++logical) {
		struct yt_maintenance_planet_result mutation;
		struct yt_maintenance_text name;
		struct yt_planet planet;
		float day;
		float minute;
		size_t row;

		if (!yt_game_read_planet(game, logical, &planet, error))
			return false;
		if (planet.name_length == 0U)
			continue;
		name.data = planet.record.bytes;
		name.length = planet.name_length < YT_TEXT_FIELD_SIZE
		    ? planet.name_length : YT_TEXT_FIELD_SIZE;
		if (!current_day_minute(game, &day, &minute, error))
			return false;
		if (!yt_maintenance_update_planet(&game->random, &planet, day,
		    minute, &mutation, error))
			return false;
		if (!yt_maintenance_compose_planet_phase(blank,
		    blank_length, &name, &mutation, &output))
			return false;
		for (row = 2U; row < output.row_count; ++row) {
			if (!line_output(line_context, output.rows[row].data,
			    output.rows[row].length, error))
				return false;
			if (row > 2U) {
				if (!yt_news_append_bytes(output.rows[row].data,
				    output.rows[row].length, error))
					return false;
			}
		}
		if (!maintenance_write_planet(game, logical, &planet, error))
			return false;
	}
	return true;
}
