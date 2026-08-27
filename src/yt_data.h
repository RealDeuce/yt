#ifndef YT_DATA_H
#define YT_DATA_H

#include "qb.h"

#define YT_RECORD_SIZE 137U
#define YT_BOUND_SIZE 133U
#define YT_RECORD_TAIL_OFFSET 133U
#define YT_RECORD_TAIL_SIZE 4U
#define YT_TEXT_FIELD_SIZE 41U
#define YT_NUMERIC_FIELDS 23U
#define YT_RADIO_RECORD_SIZE 86U

#define YT_DEFAULT_PLAYER_COUNT 50
#define YT_DEFAULT_SECTOR_COUNT 2004
#define YT_DEFAULT_PORT_COUNT 1000
#define YT_DEFAULT_PLANET_COUNT 100
#define YT_DEFAULT_RECORD_COUNT 3155

struct yt_record {
	uint8_t bytes[YT_RECORD_SIZE];
};

struct yt_radio_record {
	uint8_t bytes[YT_RADIO_RECORD_SIZE];
};

enum yt_record_field {
	YT_F41 = 41,
	YT_F45 = 45,
	YT_F49 = 49,
	YT_F53 = 53,
	YT_F57 = 57,
	YT_F61 = 61,
	YT_F65 = 65,
	YT_F69 = 69,
	YT_F73 = 73,
	YT_F77 = 77,
	YT_F81 = 81,
	YT_F85 = 85,
	YT_F89 = 89,
	YT_F93 = 93,
	YT_F97 = 97,
	YT_F101 = 101,
	YT_F105 = 105,
	YT_F109 = 109,
	YT_F113 = 113,
	YT_F117 = 117,
	YT_F121 = 121,
	YT_F125 = 125,
	YT_F129 = 129
};

void yt_record_clear(struct yt_record *record);
void yt_record_blank(struct yt_record *record);
float yt_record_get_number(const struct yt_record *record, size_t offset);
bool yt_record_set_number(struct yt_record *record, size_t offset, float value);
bool yt_record_set_number_if_changed(struct yt_record *record, size_t offset,
    float value);
bool yt_record_set_raw_number(struct yt_record *record, size_t offset,
    const uint8_t raw[4]);
size_t yt_record_get_text(const struct yt_record *record, char *dest,
    size_t size);
void yt_record_set_text(struct yt_record *record, const uint8_t *text,
    size_t length);
void yt_record_set_text_if_changed(struct yt_record *record,
    const uint8_t *text, size_t length);

float yt_radio_get_number(const struct yt_radio_record *record, size_t offset);
bool yt_radio_set_number(struct yt_radio_record *record, size_t offset,
    float value);
void yt_radio_set_text(struct yt_radio_record *record, const uint8_t *text,
    size_t length, size_t field_width);

#endif
