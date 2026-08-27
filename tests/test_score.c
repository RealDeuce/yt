#include "yt_game.h"
#include "yt_platform.h"
#include "yt_score.h"
#include "yt_score_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define yt_chdir _chdir
#define yt_rmdir _rmdir
#else
#include <unistd.h>
#define yt_chdir chdir
#define yt_rmdir rmdir
#endif

static int
fail(const char *message)
{
	fprintf(stderr, "test_score: %s\n", message);
	return EXIT_FAILURE;
}

struct score_clock_script {
	struct yt_clock_value values[6];
	size_t position;
};

static bool
score_clock_read(void *context, struct yt_clock_value *value,
    struct yt_error *error)
{
	struct score_clock_script *script = context;

	if (script->position >= YT_ARRAY_LEN(script->values)) {
		if (error != NULL)
			error->status = YT_IO_ERROR;
		return false;
	}
	*value = script->values[script->position++];
	return true;
}

static bool
check_date_serial(void)
{
	struct yt_clock_value date = {2028, 3, 1, 0, 0, 0, 0};
	struct score_clock_script script = {{{0}}, 0};
	struct yt_error error;
	int adjusted;
	int serial;

	if (yt_date_serial(&date, 28, &adjusted) != 61 || adjusted != 28)
		return false;
	if (yt_date_serial(&date, 27, &adjusted) != 426 || adjusted != 28)
		return false;
	if (yt_date_serial(&date, 26, &adjusted) != 426 || adjusted != 28)
		return false;
	date.year = 2100;
	if (yt_date_serial(&date, 99, &adjusted) != 426 || adjusted != 100)
		return false;
	date.year = 2028;
	script.values[0] = date;
	yt_error_clear(&error);
	yt_platform_set_clock_provider(score_clock_read, &script);
	if (!yt_current_date_serial(27.5f, &serial, &adjusted, &error)
	    || serial != 61 || adjusted != 28 || script.position != 1U) {
		yt_platform_set_clock_provider(NULL, NULL);
		return false;
	}
	yt_platform_set_clock_provider(NULL, NULL);
	return true;
}

static unsigned
hex_digit(char digit)
{
	if (digit >= '0' && digit <= '9')
		return (unsigned)(digit - '0');
	if (digit >= 'a' && digit <= 'f')
		return (unsigned)(digit - 'a') + 10U;
	return (unsigned)(digit - 'A') + 10U;
}

static uint64_t
hash_text(uint64_t hash, const char *text)
{
	do {
		hash = (hash ^ (uint8_t)*text) * UINT64_C(1099511628211);
	} while (*text++ != '\0');
	return hash;
}

static bool
check_format(const char *hex, enum yt_score_field field,
    const char *expected)
{
	uint8_t raw[8];
	char actual[160];
	size_t hex_length = strlen(hex);
	size_t raw_length = hex_length / 2U;
	size_t index;

	if ((hex_length != 8U && hex_length != 16U)
	    || raw_length > sizeof(raw))
		return false;
	for (index = 0; index < raw_length; ++index)
		raw[index] = (uint8_t)((hex_digit(hex[index * 2U]) << 4)
		    | hex_digit(hex[index * 2U + 1U]));
	return yt_score_format_mbf(actual, sizeof(actual), raw, raw_length,
	    field) && strcmp(actual, expected) == 0;
}

static bool
check_formatter_boundaries(void)
{
	static const struct {
		const char *hex;
		enum yt_score_field field;
		const char *expected;
	} vectors[] = {
		{"ffff4687", YT_SCORE_FIELD_RANK, " 99"},
		{"00004787", YT_SCORE_FIELD_RANK, "%100"},
		{"f7237494", YT_SCORE_FIELD_RANK, "%999999"},
		{"f8237494", YT_SCORE_FIELD_RANK, "%10E+05"},
		{"ffdf798a", YT_SCORE_FIELD_PORTS, " 999"},
		{"00e0798a", YT_SCORE_FIELD_PORTS, "%1000"},
		{"7f961898", YT_SCORE_FIELD_PORTS, "%9999999"},
		{"80961898", YT_SCORE_FIELD_PORTS, "%100E+05"},
		{"3d0ad7a370fd4787", YT_SCORE_FIELD_PERCENT, " 99.99"},
		{"3e0ad7a370fd4787", YT_SCORE_FIELD_PERCENT, "%100.00"},
		{"b81e85ebff237494", YT_SCORE_FIELD_PERCENT, "%999999.99"},
		{"b91e85ebff237494", YT_SCORE_FIELD_PERCENT, "%10.00D+05"},
		{"47e17a14aeff798a", YT_SCORE_FIELD_XANNOR_PERCENT,
		    " 999.99"},
		{"48e17a14aeff798a", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%1000.00"},
		{"eb51b8fe7f961898", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%9999999.99"},
		{"ec51b8fe7f961898", YT_SCORE_FIELD_XANNOR_PERCENT,
		    "%100.00D+05"},
		{"fffffb3fb7433aa5", YT_SCORE_FIELD_SCORE,
		    " 99,999,999,999"},
		{"0000fc3fb7433aa5", YT_SCORE_FIELD_SCORE,
		    "%100,000,000,000"},
		{"fffe7ff420e635af", YT_SCORE_FIELD_SCORE,
		    "%99,999,999,999,999"},
		{"00ff7ff420e635af", YT_SCORE_FIELD_SCORE,
		    "%10000000000000D+01"},
		{"b7433aa5", YT_SCORE_FIELD_SCORE, " 99,999,998,000"},
		{"b8433aa5", YT_SCORE_FIELD_SCORE, "%100,000,006,000"},
		{"20e635af", YT_SCORE_FIELD_SCORE,
		    "%99,999,992,000,000"},
		{"21e635af", YT_SCORE_FIELD_SCORE,
		    "%10000000000000E+01"},
		{"82000099", YT_SCORE_FIELD_SCORE, "     16,777,476"},
		{"820000a6", YT_SCORE_FIELD_SCORE, "%137,441,083,000"},
		{"820000b0", YT_SCORE_FIELD_SCORE,
		    "%14073966900000E+01"},
		{"0000c787", YT_SCORE_FIELD_RANK, "%-100"},
		{"00e0f98a", YT_SCORE_FIELD_PORTS, "%-1000"},
		{"3e0ad7a370fdc787", YT_SCORE_FIELD_PERCENT, "%-100.00"},
		{"0000fc3fb743baa5", YT_SCORE_FIELD_SCORE,
		    "%-100,000,000,000"},
		{"f823f494", YT_SCORE_FIELD_RANK, "%-10E+05"},
		{"b91e85ebff23f494", YT_SCORE_FIELD_PERCENT,
		    "%-10.00D+05"},
		{"00ff7ff420e6b5af", YT_SCORE_FIELD_SCORE,
		    "%-10000000000000D+01"},
		{"21e6b5af", YT_SCORE_FIELD_SCORE,
		    "%-10000000000000E+01"}
	};
	size_t index;
	char actual[40];

	for (index = 0; index < sizeof(vectors) / sizeof(vectors[0]);
	    ++index) {
		if (!check_format(vectors[index].hex, vectors[index].field,
		    vectors[index].expected))
			return false;
	}
	return yt_score_format_double(actual, sizeof(actual), -0.0049,
	    YT_SCORE_FIELD_PERCENT)
	    && strcmp(actual, " -0.00") == 0;
}

static bool
check_formatter_sweep(void)
{
	uint64_t hash32 = UINT64_C(14695981039346656037);
	uint64_t hash64 = UINT64_C(14695981039346656037);
	uint32_t state32 = UINT32_C(0x31415926);
	uint64_t state64 = UINT64_C(0x2718281828459045);
	char rendered[160];
	unsigned iteration;

	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[4];
		unsigned field;
		unsigned index;

		state32 ^= state32 << 13;
		state32 ^= state32 >> 17;
		state32 ^= state32 << 5;
		for (index = 0; index < 4U; ++index)
			raw[index] = (uint8_t)(state32 >> (index * 8U));
		for (field = 0; field <= (unsigned)YT_SCORE_FIELD_SCORE; ++field) {
			if (!yt_score_format_mbf(rendered, sizeof(rendered), raw,
			    sizeof(raw), (enum yt_score_field)field))
				return false;
			hash32 = hash_text(hash32, rendered);
		}
	}
	for (iteration = 0; iteration < 4096U; ++iteration) {
		uint8_t raw[8];
		unsigned field;
		unsigned index;

		state64 ^= state64 << 13;
		state64 ^= state64 >> 7;
		state64 ^= state64 << 17;
		for (index = 0; index < 8U; ++index)
			raw[index] = (uint8_t)(state64 >> (index * 8U));
		for (field = 0; field <= (unsigned)YT_SCORE_FIELD_SCORE; ++field) {
			if (!yt_score_format_mbf(rendered, sizeof(rendered), raw,
			    sizeof(raw), (enum yt_score_field)field))
				return false;
			hash64 = hash_text(hash64, rendered);
		}
	}
	return hash32 == UINT64_C(0xe7a35ef075efc336)
	    && hash64 == UINT64_C(0xd994fd9ba5bf8346);
}

static bool
check_datetime_format(void)
{
	static const int month_days[] =
	    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	char rendered[11];
	unsigned dates = 0;
	int year;
	int hour;

	for (year = 1980; year <= 2099; ++year) {
		int month;

		for (month = 1; month <= 12; ++month) {
			int days = month_days[month]
			    + (month == 2 && year % 4 == 0
			    && (year % 100 != 0 || year % 400 == 0));
			int day;

			for (day = 1; day <= days; ++day) {
				struct yt_clock_value value =
				    {year, month, day, 0, 0, 0, 0};

				yt_format_date(&value, rendered);
				if (rendered[0] != '0' + month / 10
				    || rendered[1] != '0' + month % 10
				    || rendered[2] != '-'
				    || rendered[3] != '0' + day / 10
				    || rendered[4] != '0' + day % 10
				    || rendered[5] != '-'
				    || rendered[6] != '0' + year / 1000
				    || rendered[7] != '0' + year / 100 % 10
				    || rendered[8] != '0' + year / 10 % 10
				    || rendered[9] != '0' + year % 10
				    || rendered[10] != '\0')
					return false;
				++dates;
			}
		}
	}
	if (dates != 43830U)
		return false;
	for (hour = 0; hour < 24; ++hour) {
		int minute;

		for (minute = 0; minute < 60; ++minute) {
			int second;

			for (second = 0; second < 60; ++second) {
				int hundredth;

				for (hundredth = 0; hundredth < 100; ++hundredth) {
					struct yt_clock_value value =
					    {2000, 1, 1, hour, minute, second, hundredth};
					int rounded = hour * 3600 + minute * 60 + second
					    + (hundredth >= 50 ? 1 : 0);

					yt_format_time(&value, rendered);
					if (rendered[0] != '0' + rounded / 36000
					    || rendered[1] != '0' + rounded / 3600 % 10
					    || rendered[2] != ':'
					    || rendered[3] != '0' + rounded / 600 % 6
					    || rendered[4] != '0' + rounded / 60 % 10
					    || rendered[5] != ':'
					    || rendered[6] != '0' + rounded / 10 % 6
					    || rendered[7] != '0' + rounded % 10
					    || rendered[8] != '\0')
						return false;
				}
			}
		}
	}
	return true;
}

int
main(void)
{
#ifdef _WIN32
	char directory[] = "yt-score-test";
	if (_mkdir(directory) != 0)
		return fail("cannot create temporary directory");
#else
	char directory[] = "/tmp/yt-score-test.XXXXXX";
	if (mkdtemp(directory) == NULL)
		return fail("cannot create temporary directory");
#endif
	struct yt_game game;
	struct score_clock_script clock_script = {{
		{2026, 12, 31, 23, 59, 59, 49},
		{2027, 1, 1, 0, 0, 0, 0},
		{2027, 1, 1, 0, 0, 1, 0},
		{2027, 1, 1, 0, 0, 2, 0},
		{2027, 1, 1, 0, 0, 3, 0},
		{2027, 1, 1, 0, 0, 4, 0}
	}, 0};
	struct yt_error error;
	struct yt_record blank;
	struct yt_sector sector;
	struct yt_player player;
	FILE *score;
	unsigned char bytes[1024];
	size_t length;
	size_t lines = 0;
	size_t index;
	int result = EXIT_FAILURE;

	if (!check_formatter_boundaries())
		return fail("PRINT USING boundary formatting differs");
	if (!check_formatter_sweep())
		return fail("PRINT USING raw-MBF oracle sweep differs");
	if (!check_datetime_format())
		return fail("DATE$/TIME$ formatting differs");
	if (!check_date_serial())
		return fail("DATE$ serial helper differs");
	if (yt_chdir(directory) != 0)
		return fail("cannot enter temporary directory");
	yt_platform_set_clock_provider(score_clock_read, &clock_script);
	memset(&game, 0, sizeof(game));
	yt_error_clear(&error);
	if (!yt_database_open(&game.database, "YTDATA.DAT", YT_OPEN_CREATE,
	    &error))
		goto done;
	strcpy(game.config.scoreboard, "YTSCORE.ASC");
	game.config.scoreboard_length = 11.0f;
	game.config.epoch_year = 0.0f;
	game.config.sector_offset = 3.0f;
	game.config.port_offset = 5.0f;
	game.config.planet_offset = 5.0f;
	game.config.total_records = 5.0f;
	if (!yt_config_store(&game.database, &game.config, &error))
		goto close;
	yt_record_blank(&blank);
	if (!yt_database_write(&game.database, 2, &blank, &error)
	    || !yt_database_write(&game.database, 3, &blank, &error))
		goto close;
	memset(&sector, 0, sizeof(sector));
	yt_record_blank(&sector.record);
	sector.fighters = 10.0f;
	sector.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	sector.fighters = 0.0f;
	sector.fighter_owner = 0.0f;
	if (!yt_game_write_sector(&game, 2, &sector, &error)
	    || !yt_score_generate(&game, &error))
		goto close;
	score = fopen("YTSCORE.ASC", "rb");
	if (score == NULL)
		goto close;
	length = fread(bytes, 1, sizeof(bytes), score);
	if (ferror(score) || fclose(score) != 0)
		goto close;
	if (length >= sizeof(bytes))
		goto close;
	bytes[length] = '\0';
	for (index = 0; index + 1U < length; ++index) {
		if (bytes[index] == '\r' && bytes[index + 1U] == '\n')
			++lines;
	}
	if (length != 603U || lines != 19U || bytes[length - 1U] != 0x1a)
		goto close;
	if (strstr((const char *)bytes,
	    "Last updated at: 12-31-2026 00:00:00\r\n") == NULL)
		goto close;
	if (strstr((const char *)bytes,
	    "            1,000   100.00%                  0    0.00%\r\n")
	    == NULL)
		goto close;
	if (!yt_game_read_player(&game, 2, &player, &error)
	    || player.score != -1.0f
	    || !yt_game_read_player(&game, 3, &player, &error)
	    || player.score != -1.0f)
		goto close;
	sector.fighters = 0.0f;
	sector.fighter_owner = 0.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	strcpy(game.config.scoreboard, "ZERO.ASC");
	yt_error_clear(&error);
	if (yt_score_generate(&game, &error)
	    || error.status != YT_RANGE)
		goto close;
	score = fopen("ZERO.ASC", "rb");
	if (score == NULL)
		goto close;
	length = fread(bytes, 1, sizeof(bytes), score);
	if (ferror(score) || fclose(score) != 0 || length >= sizeof(bytes))
		goto close;
	bytes[length] = '\0';
	if (length == 0 || bytes[length - 1U] == 0x1a
	    || strstr((const char *)bytes,
	    "================== =========  ================= =======\r\n")
	    == NULL)
		goto close;
	sector.fighters = 10.0f;
	sector.fighter_owner = -1.0f;
	if (!yt_game_write_sector(&game, 1, &sector, &error))
		goto close;
	strcpy(game.config.scoreboard, "NUL");
	yt_error_clear(&error);
	if (!yt_score_generate(&game, &error))
		goto close;
	score = fopen("yttemp", "rb");
	if (score == NULL)
		goto close;
	if (fseek(score, 0, SEEK_END) != 0 || ftell(score) != 603L) {
		(void)fclose(score);
		goto close;
	}
	if (fclose(score) != 0)
		goto close;
	score = fopen("NUL", "rb");
	if (score != NULL) {
		(void)fclose(score);
		goto close;
	}
	if (clock_script.position != YT_ARRAY_LEN(clock_script.values))
		goto close;
	result = EXIT_SUCCESS;

close:
	yt_database_close(&game.database);
done:
	yt_platform_set_clock_provider(NULL, NULL);
	remove("YTSCORE.ASC");
	remove("ZERO.ASC");
	remove("yttemp");
	remove("NUL");
	remove("YTDATA.DAT");
	if (yt_chdir("..") == 0)
		(void)yt_rmdir(directory);
	if (result == EXIT_SUCCESS)
		puts("test_score: ok");
	return result;
}
