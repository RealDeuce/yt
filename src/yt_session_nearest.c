#include "yt_session_internal.h"

#include "qb.h"
#include "yt_platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum nearest_present_mode {
	YT_NEAREST_PRESENT_LINE,
	YT_NEAREST_PRESENT_RAW,
	YT_NEAREST_PRESENT_BOLD_LINE,
	YT_NEAREST_PRESENT_BOLD_RAW,
};

enum nearest_output_kind {
	YT_NEAREST_ENTRY_BLANK,
	YT_NEAREST_SCANNING,
	YT_NEAREST_SCAN_BLANK,
	YT_NEAREST_OWNER_INSTRUCTION,
	YT_NEAREST_OWNER_BLANK,
	YT_NEAREST_DISTANCE,
	YT_NEAREST_SECTOR,
	YT_NEAREST_ORE,
	YT_NEAREST_ORGANICS,
	YT_NEAREST_EQUIPMENT,
	YT_NEAREST_STOCK,
	YT_NEAREST_NAME,
	YT_NEAREST_PAGER_PROMPT,
	YT_NEAREST_PAGER_ECHO,
	YT_NEAREST_FINAL_BLANK,
};

struct nearest_scan {
	int selector;
	uint8_t direction;
	uint8_t conversion_mode;
	float actor_number;
	float sector_record_offset;
	float port_record_offset;
	float base_price[3];
	float cached_roster[4];
	struct yt_player player;
	struct yt_sector sector;
	struct yt_port port;
	struct yt_player owner;
	struct yt_nearest_market market;
	float current_team;
	float start_sector_raw;
	float display_sector;
	float current_day;
	float timer_seconds;
	float page_count;
	int current_sector;
	int distance;
	bool continuous;
};

static bool
nearest_session_error(struct yt_error *error, const char *operation)
{
	if (error != NULL) {
		error->status = YT_RANGE;
		error->system_error = 0;
		(void)snprintf(error->operation, sizeof(error->operation), "%s",
		    operation);
		error->path[0] = '\0';
	}
	return false;
}

static bool
nearest_single(float value, float *result, struct yt_error *error,
    const char *operation)
{
	uint8_t raw[4];
	enum qb_mbf_status status = qb_mbf32_encode(value, raw);

	if (!isfinite(value) || status == QB_MBF_OVERFLOW
	    || status == QB_MBF_DOMAIN)
		return nearest_session_error(error, operation);
	*result = qb_mbf32_decode(raw);
	return true;
}

static bool
nearest_add(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value = left + right;

	return nearest_single(value, result, error, operation);
}

static bool
nearest_div(float left, float right, float *result, struct yt_error *error,
    const char *operation)
{
	volatile float value;

	if (right == 0.0f)
		return nearest_session_error(error, operation);
	value = left / right;
	return nearest_single(value, result, error, operation);
}

static bool
nearest_cint(const struct nearest_scan *scan, float value, int *result,
    struct yt_error *error, const char *operation)
{
	bool overflow;
	int32_t converted = qb_cint_mode((double)value, scan->conversion_mode,
	    &overflow);

	if (overflow)
		return nearest_session_error(error, operation);
	*result = (int)converted;
	return true;
}

static bool
nearest_read(struct yt_session *session, float expression,
    struct yt_record *record, struct yt_error *error)
{
	float rounded;

	if (!nearest_single(expression, &rounded, error,
	    "nearest record expression"))
		return false;
	return yt_database_read(&session->door->game.database,
	    (size_t)qb_brun_random_record_number(rounded), record, error);
}

static const char *
nearest_session_output_operation(enum nearest_output_kind kind)
{
	static const char *const operations[] = {
		[YT_NEAREST_ENTRY_BLANK] = "nearest-port opening blank",
		[YT_NEAREST_SCANNING] = "nearest-port scanning row",
		[YT_NEAREST_SCAN_BLANK] = "nearest-port scanning blank",
		[YT_NEAREST_OWNER_INSTRUCTION] = "nearest-port ownership row",
		[YT_NEAREST_OWNER_BLANK] = "nearest-port ownership blank",
		[YT_NEAREST_DISTANCE] = "nearest-port distance heading",
		[YT_NEAREST_SECTOR] = "nearest-port sector prefix",
		[YT_NEAREST_ORE] = "nearest-port ore cell",
		[YT_NEAREST_ORGANICS] = "nearest-port organics cell",
		[YT_NEAREST_EQUIPMENT] = "nearest-port equipment cell",
		[YT_NEAREST_STOCK] = "nearest-port aggregate cell",
		[YT_NEAREST_NAME] = "nearest-port name row",
		[YT_NEAREST_PAGER_PROMPT] = "nearest-port pager prompt",
		[YT_NEAREST_PAGER_ECHO] = "nearest-port pager echo",
		[YT_NEAREST_FINAL_BLANK] = "nearest-port final blank",
	};

	if ((size_t)kind >= YT_ARRAY_LEN(operations)
	    || operations[kind] == NULL)
		return "nearest-port presentation";
	return operations[kind];
}

static bool
nearest_present(struct yt_session *session, enum nearest_output_kind kind,
    enum nearest_present_mode mode, const uint8_t *text, size_t length,
    struct yt_error *error)
{
	enum session_present_text_kind present_kind;

	switch (mode) {
	case YT_NEAREST_PRESENT_LINE:
		present_kind = SESSION_PRESENT_LINE;
		break;
	case YT_NEAREST_PRESENT_RAW:
		present_kind = SESSION_PRESENT_RAW;
		break;
	case YT_NEAREST_PRESENT_BOLD_LINE:
		present_kind = SESSION_PRESENT_BOLD_LINE;
		break;
	case YT_NEAREST_PRESENT_BOLD_RAW:
		present_kind = SESSION_PRESENT_BOLD_RAW;
		break;
	default:
		if (error != NULL)
			error->status = YT_INVALID;
		return false;
	}
	if (mode == YT_NEAREST_PRESENT_BOLD_LINE
	    || mode == YT_NEAREST_PRESENT_BOLD_RAW)
		yt_present_set_bold(&session->presentation, 1.0f);
	return session_present_text(session, text, length, present_kind,
	    nearest_session_output_operation(kind), error);
}

static int
nearest_descending_compare(const void *left, const void *right)
{
	int a = *(const int *)left;
	int b = *(const int *)right;

	return a < b ? 1 : a > b ? -1 : 0;
}

static bool
nearest_filter(const struct nearest_scan *scan, bool member)
{
	float klass = scan->port.commodity_class;
	float owner = scan->port.owner;
	bool accepted = false;

	if (scan->selector >= 1 && scan->selector <= 3) {
		accepted = scan->direction == 'S'
		    ? klass == (float)scan->selector
		    : scan->direction == 'B'
		    && klass != (float)scan->selector;
	} else if (scan->selector == 4) {
		accepted = true;
	} else if (scan->selector == 5) {
		accepted = owner > 0.0f && owner != scan->actor_number && member;
	} else if (scan->selector == 6) {
		accepted = owner == scan->actor_number;
	} else if (scan->selector == 7) {
		accepted = owner > 0.0f && owner != scan->actor_number
		    && (scan->current_team == 0.0f
		    || (scan->current_team > 0.0f && !member));
	} else if (scan->selector == 8) {
		accepted = owner == 0.0f;
	}
	return accepted && klass != 0.0f;
}

static bool
nearest_price_cell(float price, char result[4])
{
	char number[64];
	char source[65];
	size_t length;
	size_t amount;
	size_t padding;
	int rendered = qb_str_single(number, sizeof(number), price);

	if (rendered < 0)
		return false;
	source[0] = ' ';
	memcpy(source + 1, number, (size_t)rendered);
	length = (size_t)rendered + 1U;
	amount = length < 3U ? length : 3U;
	padding = 3U - amount;
	memset(result, ' ', padding);
	memcpy(result + padding, source + length - amount, amount);
	result[3] = '\0';
	return true;
}

static bool
nearest_stock_cell(const struct yt_nearest_market *market, uint8_t result[7],
    struct yt_error *error)
{
	char number[64];
	char source[96];
	float total;
	float scaled;
	float rounded;
	int length;
	size_t source_length;

	if (!nearest_add(market->stock[0], market->stock[1], &total, error,
	    "nearest stock total one")
	    || !nearest_add(total, market->stock[2], &total, error,
	    "nearest stock total two")
	    || !nearest_div(total, 10000.0f, &scaled, error,
	    "nearest stock scale")
	    || !nearest_add(scaled, 0.5f, &rounded, error,
	    "nearest stock rounding"))
		return false;
	rounded = floorf(rounded);
	if (!nearest_single(rounded, &rounded, error, "nearest stock INT"))
		return false;
	length = qb_str_double(number, sizeof(number), (double)rounded);
	if (length < 0 || snprintf(source, sizeof(source), "      %sK  ",
	    number) < 0)
		return nearest_session_error(error, "nearest stock formatting");
	source_length = strlen(source);
	if (source_length < 7U)
		return nearest_session_error(error, "nearest stock width");
	memcpy(result, source + source_length - 7U, 7U);
	return true;
}

static bool
nearest_page(struct yt_session *session, struct nearest_scan *scan,
    bool *stop, struct yt_error *error)
{
	static const uint8_t prompt[] =
	    "More? [Y]es [N]o [+] Continuous [Y] ";
	struct yt_input_value selected;
	uint8_t key;

	*stop = false;
	scan->page_count = 0.0f;
	session_set_foreground(session, 3.0f);
	if (!nearest_present(session, YT_NEAREST_PAGER_PROMPT,
	    YT_NEAREST_PRESENT_BOLD_RAW, prompt, sizeof(prompt) - 1U, error))
		return false;
	for (;;) {
		if (!yt_input_wait(&session->io.input, &selected))
			return false;
		if (selected.length != 1U)
			continue;
		key = selected.bytes[0];
		if (key == '\r')
			key = 'Y';
		yt_input_compat_upper_n(&key, 1U);
		if (key != 'Y' && key != 'N' && key != '+')
			continue;
		if (!nearest_present(session, YT_NEAREST_PAGER_ECHO,
		    YT_NEAREST_PRESENT_LINE, &key, 1U, error))
			return false;
		if (key == '+')
			scan->continuous = true;
		*stop = key == 'N';
		return true;
	}
}

static bool
nearest_scan_run(struct yt_session *session, int selector,
    uint8_t direction, struct yt_error *error)
{
	static const uint8_t scanning[] = "Scanning Starmap Database...";
	static const uint8_t instruction[] =
	    "Owned ports show the name of the owner preceeded by a \">\".";
	static const uint8_t earth[] = "** Earth **";
	struct nearest_scan scan;
	bool *visited;
	int *current_layer;
	int *next_layer;
	size_t current_count = 0U;
	struct yt_record raw;
	bool first_sector = true;
	int maximum_sector = session_sector_count(session);
	size_t index;
	bool success = false;

	memset(&scan, 0, sizeof(scan));
	scan.selector = selector;
	scan.direction = direction;
	scan.conversion_mode =
	    session->presentation.sound.conversion_mode;
	scan.actor_number = (float)session_record(session);
	scan.sector_record_offset = session_sector_offset(session);
	scan.port_record_offset = session_port_offset(session);
	memcpy(scan.base_price, session->market_bases, sizeof(scan.base_price));
	for (index = 0U; index < YT_ARRAY_LEN(scan.cached_roster); ++index)
		scan.cached_roster[index] =
		    (float)session->team_cache.roster[index];
	scan.page_count = 4.0f;
	visited = calloc((size_t)maximum_sector + 1U, sizeof(*visited));
	current_layer = malloc(((size_t)maximum_sector + 1U)
	    * sizeof(*current_layer));
	next_layer = malloc(((size_t)maximum_sector + 1U)
	    * sizeof(*next_layer));
	if (visited == NULL || current_layer == NULL || next_layer == NULL) {
		nearest_session_error(error, "nearest workspace");
		goto done;
	}

	if (!nearest_single(scan.actor_number, &scan.actor_number, error,
	    "nearest actor number")
	    || !nearest_present(session, YT_NEAREST_ENTRY_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		goto done;
	session_set_foreground(session, 3.0f);
	if (!nearest_present(session, YT_NEAREST_SCANNING,
	    YT_NEAREST_PRESENT_BOLD_LINE, scanning, sizeof(scanning) - 1U,
	    error)
	    || !nearest_present(session, YT_NEAREST_SCAN_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error))
		goto done;
	session_set_foreground(session, 7.0f);
	if (!nearest_present(session, YT_NEAREST_OWNER_INSTRUCTION,
	    YT_NEAREST_PRESENT_BOLD_LINE, instruction,
	    sizeof(instruction) - 1U, error)
	    || !nearest_present(session, YT_NEAREST_OWNER_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error)
	    || !nearest_read(session, scan.actor_number, &raw, error))
		goto done;
	yt_player_decode(&scan.player, &raw);
	session->player = scan.player;
	scan.current_team = scan.player.team;
	scan.start_sector_raw = scan.player.sector;
	if (!nearest_cint(&scan, scan.start_sector_raw, &scan.current_sector,
	    error, "nearest start-sector CINT"))
		goto done;
	if (scan.start_sector_raw != 0.0f) {
		visited[scan.current_sector] = true;
		current_layer[current_count++] = scan.current_sector;
	}

	while (current_count != 0U) {
		size_t layer_index;
		size_t next_count = 0U;
		bool heading_emitted = false;

		for (layer_index = 0U; layer_index < current_count;
		    ++layer_index) {
			int sector_number = current_layer[layer_index];
			float sector_operand = first_sector
			    ? scan.start_sector_raw : (float)sector_number;
			float expression;
			float raw_port;
			size_t slot;

			first_sector = false;
			scan.current_sector = sector_number;
			scan.display_sector = sector_operand;
			if (!nearest_add(scan.sector_record_offset, sector_operand,
			    &expression, error, "nearest sector record expression")
			    || !nearest_read(session, expression, &raw, error))
				goto done;
			yt_sector_decode(&scan.sector, &raw);
			for (slot = 0U; slot < 6U; ++slot) {
				int target;
				float warp = scan.sector.warps[slot];

				if (warp == 0.0f)
					continue;
				if (!nearest_cint(&scan, warp, &target, error,
				    "nearest warp CINT"))
					goto done;
				if (visited[target])
					continue;
				visited[target] = true;
				next_layer[next_count++] = target;
			}
			raw_port = scan.sector.port;
			if (raw_port == 0.0f)
				continue;
			{
				int today;
				int adjusted_year;

				if (!yt_current_date_serial(
				    session->door->game.config.epoch_year, &today,
				    &adjusted_year, error))
					goto done;
				session->door->game.today = today;
				session->door->game.adjusted_year = adjusted_year;
				scan.current_day = (float)today;
			}
			if (!nearest_single(scan.current_day, &scan.current_day,
			    error, "nearest current day")
			    || !nearest_add(scan.port_record_offset, raw_port,
			    &expression, error, "nearest port record expression")
			    || !nearest_read(session, expression, &raw, error))
				goto done;
			yt_port_decode(&scan.port, &raw);
			{
				bool member = false;

				if (scan.current_team != 0.0f) {
					for (slot = 0U; slot < 4U; ++slot) {
						if (scan.cached_roster[slot]
						    == scan.port.owner)
							member = true;
					}
				}
				if (!nearest_filter(&scan, member))
					continue;
			}
			scan.timer_seconds = (float)yt_platform_timer();
			if (!nearest_single(scan.timer_seconds, &scan.timer_seconds,
			    error, "nearest TIMER")
			    || !yt_nearest_market_project(&scan.market, &scan.port,
			    scan.base_price, scan.current_day, scan.timer_seconds,
			    error))
				goto done;

			if (!heading_emitted) {
				char number[64];
				uint8_t heading[96];
				int length = qb_str_single(number, sizeof(number),
				    (float)scan.distance);

				if (length < 0
				    || 9U + (size_t)length > sizeof(heading)) {
					nearest_session_error(error,
					    "nearest distance formatting");
					goto done;
				}
				memcpy(heading, "Distance:", 9U);
				memcpy(heading + 9U, number, (size_t)length);
				session_set_foreground(session, 1.0f);
				if (!nearest_present(session, YT_NEAREST_DISTANCE,
				    YT_NEAREST_PRESENT_BOLD_LINE, heading,
				    9U + (size_t)length, error)
				    || !nearest_add(scan.page_count, 1.0f,
				    &scan.page_count, error, "nearest pager count"))
					goto done;
				heading_emitted = true;
			}
			{
				uint8_t name[43];
				uint8_t sector_cell[13];
				uint8_t ore[13];
				uint8_t organics[13];
				uint8_t equipment[11];
				uint8_t stock[7];
				char number[64];
				char price[4];
				size_t name_length;
				int rendered;
				int owner_record;
				bool stop;

				rendered = qb_str_single(number, sizeof(number),
				    scan.display_sector);
				if (rendered < 0) {
					nearest_session_error(error,
					    "nearest sector formatting");
					goto done;
				}
				memcpy(sector_cell, "Sector:", 7U);
				memset(sector_cell + 7U, ' ', 6U);
				memcpy(sector_cell + 7U, number,
				    (size_t)rendered < 6U ? (size_t)rendered : 6U);
				if (!nearest_price_cell(scan.market.price[0], price)) {
					nearest_session_error(error,
					    "nearest ore formatting");
					goto done;
				}
				(void)snprintf((char *)ore, sizeof(ore),
				    " Ore @%c%s  ", scan.port.commodity_class
				    == 3.0f ? 'S' : 'B', price);
				if (!nearest_price_cell(scan.market.price[1], price)) {
					nearest_session_error(error,
					    "nearest organics formatting");
					goto done;
				}
				(void)snprintf((char *)organics, sizeof(organics),
				    " Org @%c%s  ", scan.port.commodity_class
				    == 2.0f ? 'S' : 'B', price);
				if (!nearest_price_cell(scan.market.price[2], price)) {
					nearest_session_error(error,
					    "nearest equipment formatting");
					goto done;
				}
				(void)snprintf((char *)equipment, sizeof(equipment),
				    " Equ @%c%s", scan.port.commodity_class
				    == 1.0f ? 'S' : 'B', price);
				if (!nearest_stock_cell(&scan.market, stock, error)
				    || !nearest_cint(&scan, scan.port.name_length,
				    &rendered, error, "nearest name-length CINT"))
					goto done;
				name_length = (size_t)rendered;
				if (name_length > YT_TEXT_FIELD_SIZE)
					name_length = YT_TEXT_FIELD_SIZE;
				memcpy(name, scan.port.record.bytes, name_length);
				if (scan.display_sector == 1.0f) {
					name_length = sizeof(earth) - 1U;
					memcpy(name, earth, name_length);
					memset(ore, 0, sizeof(ore));
					memset(organics, 0, sizeof(organics));
					memset(equipment, 0, sizeof(equipment));
					memset(stock, 0, sizeof(stock));
				}
				if (scan.port.owner != 0.0f)
					yt_present_set_bold(&session->presentation, 1.0f);
				session_set_foreground(session, 2.0f);
				if (!nearest_present(session, YT_NEAREST_SECTOR,
				    YT_NEAREST_PRESENT_RAW, sector_cell,
				    sizeof(sector_cell), error))
					goto done;
				session_set_foreground(session,
				    scan.port.commodity_class == 3.0f ? 7.0f : 6.0f);
				if (!nearest_present(session, YT_NEAREST_ORE,
				    YT_NEAREST_PRESENT_BOLD_RAW, ore,
				    scan.display_sector == 1.0f ? 0U : sizeof(ore) - 1U,
				    error))
					goto done;
				session_set_foreground(session,
				    scan.port.commodity_class == 2.0f ? 7.0f : 6.0f);
				if (!nearest_present(session, YT_NEAREST_ORGANICS,
				    YT_NEAREST_PRESENT_BOLD_RAW, organics,
				    scan.display_sector == 1.0f
				    ? 0U : sizeof(organics) - 1U, error))
					goto done;
				session_set_foreground(session,
				    scan.port.commodity_class == 1.0f ? 7.0f : 6.0f);
				if (!nearest_present(session, YT_NEAREST_EQUIPMENT,
				    YT_NEAREST_PRESENT_BOLD_RAW, equipment,
				    scan.display_sector == 1.0f
				    ? 0U : sizeof(equipment) - 1U, error))
					goto done;
				session_set_foreground(session, 2.0f);
				if (!nearest_present(session, YT_NEAREST_STOCK,
				    YT_NEAREST_PRESENT_RAW, stock,
				    scan.display_sector == 1.0f ? 0U : sizeof(stock),
				    error))
					goto done;
				session_set_foreground(session, 3.0f);
				if (!nearest_cint(&scan, scan.port.owner, &owner_record,
				    error, "nearest owner CINT"))
					goto done;
				if (scan.display_sector != 1.0f && owner_record != 0) {
					if (!nearest_read(session, scan.port.owner, &raw,
					    error))
						goto done;
					yt_player_decode(&scan.owner, &raw);
					memmove(name + 2U, scan.owner.record.bytes,
					    YT_TEXT_FIELD_SIZE);
					memcpy(name, "> ", 2U);
					name_length = qb_title_case_n(name,
					    YT_TEXT_FIELD_SIZE + 2U);
					if (name_length > 26U)
						name_length = 26U;
					if (scan.port.owner == scan.actor_number)
						session_set_foreground(session, 5.0f);
				}
				if (scan.display_sector == 1.0f) {
					session_set_foreground(session, 3.0f);
					yt_present_set_blink(&session->presentation, 1.0f);
				}
				if (!nearest_present(session, YT_NEAREST_NAME,
				    YT_NEAREST_PRESENT_BOLD_LINE, name, name_length,
				    error)
				    || !nearest_add(scan.page_count, 1.0f,
				    &scan.page_count, error, "nearest pager count"))
					goto done;
				if (scan.continuous)
					scan.page_count = 0.0f;
				if (scan.page_count > 22.0f) {
					if (!nearest_page(session, &scan, &stop, error))
						goto done;
					if (stop) {
						success = nearest_present(session,
						    YT_NEAREST_FINAL_BLANK,
						    YT_NEAREST_PRESENT_LINE, NULL, 0U,
						    error);
						goto done;
					}
				}
			}
		}
		qsort(next_layer, next_count, sizeof(next_layer[0]),
		    nearest_descending_compare);
		memcpy(current_layer, next_layer,
		    next_count * sizeof(current_layer[0]));
		current_count = next_count;
		++scan.distance;
	}
	success = nearest_present(session, YT_NEAREST_FINAL_BLANK,
	    YT_NEAREST_PRESENT_LINE, NULL, 0U, error);

done:
	free(next_layer);
	free(current_layer);
	free(visited);
	return success;
}

bool
yt_session_computer_nearest_ports(struct yt_session *session,
    struct yt_error *error)
{
	static const uint8_t first_line[] =
	    "Show buying/selling [1] Equ, [2] Org, [3] Ore,";
	static const uint8_t second_line[] =
	    "[Y] Your Ports, [T] Team's Ports, [E] Enemy Ports";
	static const uint8_t filter_prompt[] =
	    "[U] Un-owned Ports OR [A] All Ports ? -=> [A] ";
	static const uint8_t no_team[] = "You dont belong to a team!";
	static const uint8_t no_ports[] = "You dont own any!";
	uint8_t direction_prompt[80];
	char response[80];
	size_t direction_prompt_length;
	size_t response_length;
	uint8_t direction = 0U;
	int selector;

	if (!session_reload_player(session, error)
	    || !session_present_paged_line(session, first_line,
	    sizeof(first_line) - 1U, "nearest-port first filter row", error)
	    || !session_present_paged_fragment(session, second_line,
	    sizeof(second_line) - 1U)
	    || !session_present_timed_paged_row(session, filter_prompt,
	    sizeof(filter_prompt) - 1U, "nearest-port filter prompt", error)
	    || !session_read_upper_command(session, response, sizeof(response)))
		return false;
	response_length = strlen(response);
	selector = yt_nearest_filter_selector((const uint8_t *)response,
	    response_length);
	if (selector == 0)
		return true;
	if (selector == 5 && session->player.team == 0.0f)
		return session_present_alert(session, no_team,
		    sizeof(no_team) - 1U, "nearest-port team rejection", error);
	if (selector == 6 && session->player.ports_owned == 0.0f)
		return session_present_alert(session, no_ports,
		    sizeof(no_ports) - 1U,
		    "nearest-port ownership rejection", error);
	if (selector >= 1 && selector <= 3) {
		if (!session_present_text(session, NULL, 0U,
		    SESSION_PRESENT_LINE, "nearest-port direction blank", error))
			return false;
		if (!yt_nearest_direction_prompt(selector, direction_prompt,
		    sizeof(direction_prompt), &direction_prompt_length)) {
			if (error != NULL) {
				error->status = YT_RANGE;
				(void)snprintf(error->operation,
				    sizeof(error->operation), "%s",
				    "nearest direction prompt");
			}
			return false;
		}
		if (!session_present_timed_paged_row(session, direction_prompt,
		    direction_prompt_length, "nearest-port direction prompt", error)
		    || !session_read_upper_command(session, response,
		    sizeof(response)))
			return false;
		response_length = strlen(response);
		if (response_length != 1U
		    || (response[0] != 'B' && response[0] != 'S'))
			return true;
		direction = (uint8_t)response[0];
	}
	return nearest_scan_run(session, selector, direction, error);
}
