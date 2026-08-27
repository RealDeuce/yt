#include "yt_data.h"

#include <string.h>

static bool
valid_numeric_offset(size_t offset)
{
	return offset >= YT_F41 && offset <= YT_F129
	    && (offset - YT_F41) % 4U == 0;
}

void
yt_record_clear(struct yt_record *record)
{
	memset(record->bytes, 0, sizeof(record->bytes));
}

void
yt_record_blank(struct yt_record *record)
{
	yt_record_clear(record);
	memset(record->bytes, ' ', YT_TEXT_FIELD_SIZE);
}

float
yt_record_get_number(const struct yt_record *record, size_t offset)
{
	if (!valid_numeric_offset(offset))
		return 0.0f;
	return qb_mbf32_decode(record->bytes + offset);
}

bool
yt_record_set_number(struct yt_record *record, size_t offset, float value)
{
	if (!valid_numeric_offset(offset))
		return false;
	return qb_mbf32_encode(value, record->bytes + offset)
	    != QB_MBF_OVERFLOW;
}

bool
yt_record_set_number_if_changed(struct yt_record *record, size_t offset,
    float value)
{
	if (!valid_numeric_offset(offset))
		return false;
	if (yt_record_get_number(record, offset) != value
	    && qb_mbf32_encode(value, record->bytes + offset)
	    == QB_MBF_OVERFLOW)
		return false;
	return true;
}

bool
yt_record_set_raw_number(struct yt_record *record, size_t offset,
    const uint8_t raw[4])
{
	if (!valid_numeric_offset(offset))
		return false;
	memcpy(record->bytes + offset, raw, 4);
	return true;
}

size_t
yt_record_get_text(const struct yt_record *record, char *dest, size_t size)
{
	size_t length = YT_TEXT_FIELD_SIZE;
	size_t copied;

	while (length > 0 && record->bytes[length - 1] == ' ')
		--length;
	if (size == 0)
		return length;
	copied = length < size - 1 ? length : size - 1;
	memcpy(dest, record->bytes, copied);
	dest[copied] = '\0';
	return length;
}

void
yt_record_set_text(struct yt_record *record, const uint8_t *text,
    size_t length)
{
	size_t copied = length < YT_TEXT_FIELD_SIZE ? length : YT_TEXT_FIELD_SIZE;

	memset(record->bytes, ' ', YT_TEXT_FIELD_SIZE);
	if (copied > 0)
		memcpy(record->bytes, text, copied);
}

void
yt_record_set_text_if_changed(struct yt_record *record, const uint8_t *text,
    size_t length)
{
	uint8_t field[YT_TEXT_FIELD_SIZE];
	size_t copied = length < sizeof(field) ? length : sizeof(field);

	memset(field, ' ', sizeof(field));
	if (copied > 0)
		memcpy(field, text, copied);
	if (memcmp(record->bytes, field, sizeof(field)) != 0)
		memcpy(record->bytes, field, sizeof(field));
}

float
yt_radio_get_number(const struct yt_radio_record *record, size_t offset)
{
	if (offset > 8 || offset % 4U != 0)
		return 0.0f;
	return qb_mbf32_decode(record->bytes + offset);
}

bool
yt_radio_set_number(struct yt_radio_record *record, size_t offset,
    float value)
{
	if (offset > 8 || offset % 4U != 0)
		return false;
	return qb_mbf32_encode(value, record->bytes + offset)
	    != QB_MBF_OVERFLOW;
}

void
yt_radio_set_text(struct yt_radio_record *record, const uint8_t *text,
    size_t length, size_t field_width)
{
	size_t copied;

	if (field_width > 74)
		field_width = 74;
	copied = length < field_width ? length : field_width;
	memset(record->bytes + 12, ' ', field_width);
	if (copied > 0)
		memcpy(record->bytes + 12, text, copied);
	if (field_width == 72)
		memset(record->bytes + 84, 0, 2);
}
