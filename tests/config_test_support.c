#include "config_test_support.h"

#include <string.h>

void
test_config_encode(struct yt_config *config)
{
	size_t length = strlen(config->scoreboard);

	if (length > 41U)
		length = 41U;
	yt_record_set_text_if_changed(&config->record,
	    (const uint8_t *)config->scoreboard, length);
	yt_record_set_number_if_changed(&config->record, YT_F41, (float)length);
	yt_record_set_number_if_changed(&config->record, YT_F45,
	    config->epoch_year);
	yt_record_set_number_if_changed(&config->record, YT_F49,
	    config->turns_per_day);
	yt_record_set_number_if_changed(&config->record, YT_F53,
	    (float)config->sector_offset);
	yt_record_set_number_if_changed(&config->record, YT_F57,
	    (float)config->port_offset);
	yt_record_set_number_if_changed(&config->record, YT_F61,
	    (float)config->planet_offset);
	yt_record_set_number_if_changed(&config->record, YT_F65,
	    config->initial_fighters);
	yt_record_set_number_if_changed(&config->record, YT_F69,
	    config->initial_credits);
	yt_record_set_number_if_changed(&config->record, YT_F73,
	    config->initial_holds);
	yt_record_set_number_if_changed(&config->record, YT_F77,
	    config->retention_days);
	yt_record_set_number_if_changed(&config->record, YT_F81,
	    config->last_maintenance);
	yt_record_set_number_if_changed(&config->record, YT_F85,
	    config->local_screen ? -1.0f : 0.0f);
	yt_record_set_number_if_changed(&config->record, YT_F93,
	    (float)config->total_records);
	yt_record_set_number_if_changed(&config->record, YT_F101,
	    config->lottery_plays);
	yt_record_set_number_if_changed(&config->record, YT_F105,
	    config->genesis_ports);
	yt_record_set_number_if_changed(&config->record, YT_F117,
	    config->headquarters);
	yt_record_set_number_if_changed(&config->record, YT_F121,
	    config->maximum_holds);
	yt_record_set_number_if_changed(&config->record, YT_F125,
	    (float)config->marker);
	yt_record_set_number_if_changed(&config->record, YT_F129,
	    (float)config->maximum_planets);
}

bool
test_config_store(struct yt_database *database,
    const struct yt_config *config, struct yt_error *error)
{
	struct yt_config encoded = *config;

	test_config_encode(&encoded);
	return yt_database_write(database, 1U, &encoded.record, error);
}
