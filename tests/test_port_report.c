#include "yt_game.h"

#include "qb.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
		    __FILE__, __LINE__, #expression); \
		++failures; \
	} \
} while (0)

static void
set_number(struct yt_record *record, size_t offset, float value)
{
	CHECK(yt_record_set_number(record, offset, value));
}

static void
market_fixture(struct yt_port_market_state *market)
{
	static const float stock[3] = {100.0f, 200.0f, 300.0f};
	static const float production[3] = {10.0f, 20.0f, 30.0f};
	static const float factor[3] = {-60.0f, 74.0f, -66.0f};
	struct yt_record record;
	size_t index;

	memset(market, 0, sizeof(*market));
	yt_record_blank(&record);
	memcpy(record.bytes, "Argus", 5U);
	set_number(&record, YT_F45, 1000.0f);
	for (index = 0U; index < 3U; ++index) {
		set_number(&record, YT_F49 + index * 4U, stock[index]);
		set_number(&record, YT_F61 + index * 4U, production[index]);
		set_number(&record, YT_F73 + index * 4U, factor[index]);
	}
	set_number(&record, YT_F85, 5.0f);
	set_number(&record, YT_F97, 2.0f);
	set_number(&record, YT_F101, 600.0f);
	yt_port_decode(&market->port, &record);
	market->current_day = 1000.0f;
	market->timer_seconds = 36000.0f;
	market->base_price[0] = 20.0f;
	market->base_price[1] = 30.0f;
	market->base_price[2] = 40.0f;
	CHECK(yt_port_market_update(market, NULL));
}

static void
player_fixture(struct yt_player *player)
{
	struct yt_record record;

	yt_record_blank(&record);
	memcpy(record.bytes, "Current", 7U);
	set_number(&record, YT_F69, 5.5f);
	set_number(&record, YT_F73, 6.0f);
	set_number(&record, YT_F77, 7.25f);
	set_number(&record, YT_F85, 7.0f);
	yt_player_decode(player, &record);
}

static void
report_port_fixture(const struct yt_port_market_state *market,
    struct yt_port *port)
{
	struct yt_record record = market->port.record;

	memset(record.bytes, ' ', YT_TEXT_FIELD_SIZE);
	memcpy(record.bytes, "Fresh", 5U);
	set_number(&record, YT_F85, 5.0f);
	yt_port_decode(port, &record);
}

static void
test_report_text(void)
{
	static const uint8_t title[] =
	    "Commerce report for Fresh: 07-25-2026 12:34:56";
	static const uint8_t names[3][23] = {
		"Ore..........  Buying ",
		"Organics.....  Selling",
		"Equipment....  Buying ",
	};
	static const uint8_t capacities[3][12] = {
		"         100", "         200", "         300",
	};
	static const uint8_t holds[3][11] = {
		"        5.5", "          6", "       7.25",
	};
	static const char *const prices[3] = {" 32    ", " 8    ", " 66    "};
	static const size_t price_lengths[3] = {7U, 6U, 7U};
	struct yt_port_market_state market;
	struct yt_port_report_text report;
	struct yt_player player;
	struct yt_port port;
	struct yt_error error;
	size_t index;

	market_fixture(&market);
	player_fixture(&player);
	report_port_fixture(&market, &port);
	yt_error_clear(&error);
	CHECK(yt_port_report_compose(&market, &player, &port, 0U,
	    (const uint8_t *)"07-25-2026", (const uint8_t *)"12:34:56",
	    &report, &error));
	CHECK(report.title_length == sizeof(title) - 1U);
	CHECK(memcmp(report.title, title, sizeof(title) - 1U) == 0);
	for (index = 0U; index < 3U; ++index) {
		CHECK(memcmp(report.item[index].name_status, names[index],
		    sizeof(names[index])) == 0);
		CHECK(memcmp(report.item[index].capacity, capacities[index],
		    sizeof(capacities[index])) == 0);
		CHECK(memcmp(report.item[index].hold, holds[index],
		    sizeof(holds[index])) == 0);
		CHECK(report.item[index].price_length == price_lengths[index]);
		CHECK(memcmp(report.item[index].price, prices[index],
		    price_lengths[index]) == 0);
	}
	CHECK(report.item[0].foreground == 3.0f);
	CHECK(report.item[1].foreground == 2.0f);
	CHECK(report.item[2].foreground == 3.0f);
}

static void
test_exact_capacity(void)
{
	struct yt_port_market_state market;
	struct yt_port_report_text report;
	struct yt_player player;
	struct yt_port port;
	struct yt_error error;

	market_fixture(&market);
	player_fixture(&player);
	report_port_fixture(&market, &port);
	CHECK(qb_mbf64_encode(16777217.0, market.capacity_raw[0]) == QB_MBF_OK);
	yt_error_clear(&error);
	CHECK(yt_port_report_compose(&market, &player, &port, 0U,
	    (const uint8_t *)"07-25-2026", (const uint8_t *)"12:34:56",
	    &report, &error));
	CHECK(memcmp(report.item[0].capacity, "    16777217", 12U) == 0);
}

int
main(void)
{
	test_report_text();
	test_exact_capacity();
	return failures == 0 ? 0 : 1;
}
