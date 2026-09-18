#include "yt_data.h"

#include <stdio.h>
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
	if (yt_record_get_number(record, offset) == value)
		return true;
	if (qb_mbf32_encode(value, record->bytes + offset) == QB_MBF_OVERFLOW)
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

bool
yt_radio_set_raw_number(struct yt_radio_record *record, size_t offset,
    const uint8_t raw[4])
{
	if (offset > 8 || offset % 4U != 0)
		return false;
	memcpy(record->bytes + offset, raw, 4);
	return true;
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

bool
yt_radio_reader_decide(float counter, float recipient, float sender,
    float current_player, float reader_mode,
    struct yt_radio_reader_decision *decision, struct yt_error *error)
{
	bool overflow;
	int32_t mode;
	int32_t greater;
	int32_t equal_one;
	int32_t recipient_equal;
	int32_t sender_equal;

	if (decision == NULL)
		return false;
	memset(decision, 0, sizeof(*decision));
	mode = qb_cint((double)reader_mode, &overflow);
	if (overflow) {
		if (error != NULL) {
			error->status = YT_RANGE;
			(void)snprintf(error->operation,
			    sizeof(error->operation), "%s",
			    "radio reader mode CINT");
		}
		return false;
	}
	greater = counter > 1.0f ? -1 : 0;
	equal_one = counter == 1.0f ? -1 : 0;
	recipient_equal = recipient == current_player ? -1 : 0;
	sender_equal = sender == current_player ? -1 : 0;
	decision->log_heading = reader_mode != 0.0f;
	decision->visible = (greater | ((equal_one | mode)
	    & (recipient_equal | (mode & sender_equal)))) != 0;
	decision->automatic_write = reader_mode == 0.0f
	    && decision->visible;
	return true;
}

bool
yt_radio_reader_mutate(struct yt_radio_record *record, float counter)
{
	static const uint8_t dirty_zero[4] = {0x00, 0x00, 0x80, 0x00};
	volatile float updated;

	if (record == NULL)
		return false;
	if (!(counter > 1.0f))
		return yt_radio_set_raw_number(record, 0, dirty_zero);
	updated = counter - 2.0f;
	return yt_radio_set_number(record, 0, updated);
}

bool
yt_radio_reader_header(const uint8_t *recipient, size_t recipient_length,
    const uint8_t *sender, size_t sender_length, uint8_t *header,
    size_t capacity, size_t *length)
{
	static const uint8_t prefix[] = "Message to: ";
	static const uint8_t infix[] = " * From: ";
	size_t needed;
	size_t position = 0;

	if (length == NULL)
		return false;
	*length = 0;
	if (recipient_length > SIZE_MAX - (sizeof(prefix) - 1U)
	    || sender_length > SIZE_MAX - (sizeof(infix) - 1U)
	    || recipient_length + sizeof(prefix) - 1U
	    > SIZE_MAX - sender_length - (sizeof(infix) - 1U))
		return false;
	needed = sizeof(prefix) - 1U + recipient_length
	    + sizeof(infix) - 1U + sender_length;
	if (needed > capacity || (needed != 0 && header == NULL)
	    || (recipient_length != 0 && recipient == NULL)
	    || (sender_length != 0 && sender == NULL))
		return false;
	memcpy(header + position, prefix, sizeof(prefix) - 1U);
	position += sizeof(prefix) - 1U;
	if (recipient_length != 0) {
		memcpy(header + position, recipient, recipient_length);
		position += recipient_length;
	}
	memcpy(header + position, infix, sizeof(infix) - 1U);
	position += sizeof(infix) - 1U;
	if (sender_length != 0) {
		memcpy(header + position, sender, sender_length);
		position += sender_length;
	}
	*length = position;
	return true;
}
