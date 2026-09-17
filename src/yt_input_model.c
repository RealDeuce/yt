#include "yt_input_model.h"

#include "qb.h"

#include <math.h>
#include <string.h>


static bool
bounded_string_length(const char *text, size_t capacity, size_t *length)
{
	size_t index;

	if (text == NULL || capacity == 0U || length == NULL)
		return false;
	for (index = 0U; index < capacity; ++index) {
		if (text[index] == '\0') {
			*length = index;
			return true;
		}
	}
	return false;
}

enum yt_radio_body_key_action
yt_input_radio_body_key(uint8_t key, size_t current_length)
{
	if (key == '\r')
		return YT_RADIO_BODY_KEY_COMMIT;
	if (key == '\b' || key == 0x7fU)
		return current_length != 0U
		    ? YT_RADIO_BODY_KEY_BACKSPACE : YT_RADIO_BODY_KEY_IGNORE;
	if (key >= 0x20U && key < 0x7fU)
		return YT_RADIO_BODY_KEY_PRINTABLE;
	return YT_RADIO_BODY_KEY_IGNORE;
}

bool
yt_input_queue_pop(char *queue, size_t capacity, size_t *position,
    size_t *length, struct yt_input_value *selected)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || selected == NULL || *position > *length
	    || *length >= capacity)
		return false;
	memset(selected, 0, sizeof(*selected));
	if (*position == *length)
		return true;
	selected->bytes[0] = (uint8_t)queue[(*position)++];
	selected->length = 1U;
	if (*position == *length) {
		*position = 0U;
		*length = 0U;
		queue[0] = '\0';
	}
	return true;
}

bool
yt_input_queue_clear(char *queue, size_t capacity, size_t *position,
    size_t *length)
{
	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || *position > *length || *length >= capacity)
		return false;
	queue[0] = '\0';
	*position = 0U;
	*length = 0U;
	return true;
}

bool
yt_input_queue_prepend_program(char *queue, size_t capacity,
    size_t *position, size_t *length, const char *program,
    size_t program_length)
{
	size_t pending;

	if (queue == NULL || capacity == 0U || position == NULL
	    || length == NULL || (program == NULL && program_length != 0U)
	    || *position > *length || *length >= capacity)
		return false;
	pending = *length - *position;
	if (program_length >= capacity
	    || program_length + 1U >= capacity - pending)
		return false;
	memmove(queue + program_length + 1U, queue + *position, pending);
	if (program_length != 0U)
		memcpy(queue, program, program_length);
	queue[program_length] = '\r';
	queue[program_length + 1U + pending] = '\0';
	*position = 0U;
	*length = program_length + 1U + pending;
	return true;
}

bool
yt_input_repeat_requested(bool queued,
    const struct yt_input_value *selected)
{
	return !queued && selected != NULL && selected->length == 1U
	    && selected->bytes[0] == 0x12U;
}

bool
yt_input_submit_requested(uint8_t selected_key)
{
	return selected_key == '\r';
}

static bool
command_save_fault_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK;
}

static bool
command_save_fail(struct yt_command_save_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_command_save_staged(char *text, size_t text_capacity,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity,
    enum yt_basic_fault_site target,
    struct yt_command_save_transform *result)
{
	static const char notice[] =
	    "Command Saved -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	size_t length;
	size_t stripped_length;

	if (text == NULL || text_capacity == 0U || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL || *queue_position > *queue_length
	    || *queue_length >= queue_capacity || saved_command == NULL
	    || saved_capacity == 0U || output_source == NULL
	    || output_capacity == 0U || result == NULL
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !command_save_fault_target(target))
	    || !bounded_string_length(text, text_capacity, &length))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target != YT_BASIC_FAULT_SITE_COUNT) {
		if (length == 0U
		    || (target != YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE
		    && text[length - 1U] != '/'))
			return false;
		stripped_length = length - 1U;
		if ((target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE
		    || target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE)
		    && stripped_length == 0U)
			return false;
	}
	if (target == YT_BASIC_FAULT_ADE0_SLASH_TEST_RIGHT_SPACE)
		return command_save_fail(result, target);
	if (length == 0U || text[length - 1U] != '/')
		return target == YT_BASIC_FAULT_SITE_COUNT;
	result->save_requested = true;
	stripped_length = length - 1U;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_STRIP_LEFT_SPACE)
		return command_save_fail(result, target);
	text[stripped_length] = '\0';
	queue[0] = '\0';
	*queue_position = 0U;
	*queue_length = 0U;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_COMMAND_CLONE_SPACE)
		return command_save_fail(result, target);
	if (stripped_length >= saved_capacity)
		return false;
	memcpy(saved_command, text, stripped_length + 1U);
	if (target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_CLONE_SPACE)
		return command_save_fail(result, target);
	if (sizeof(notice) > output_capacity)
		return false;
	memcpy(output_source, notice, sizeof(notice));
	result->notice_ready = true;
	if (target == YT_BASIC_FAULT_ADE0_SAVE_NOTICE_GOSUB_STACK)
		return command_save_fail(result, target);
	return true;
}

static bool
upper_fault_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_UPPER_FRAME_STACK
	    || target == YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE
	    || target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
	    || target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE;
}

static bool
upper_fail(struct yt_upper_transform *result,
    enum yt_basic_fault_site site, size_t length, size_t index,
    bool initialized, bool extracted, uint8_t byte, bool mapped,
    uint8_t code)
{
	result->length = length;
	result->index = index;
	result->extracted = byte;
	result->mapped = code;
	result->fault_site = site;
	result->scratch_initialized = initialized;
	result->extracted_valid = extracted;
	result->mapped_valid = mapped;
	result->fault_valid = true;
	return false;
}

bool
yt_input_compat_upper_n_staged(uint8_t *text, size_t length,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_upper_transform *result)
{
	size_t index;

	if (text == NULL || result == NULL || occurrence == 0U
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !upper_fault_target(target)))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target == YT_BASIC_FAULT_UPPER_FRAME_STACK) {
		if (occurrence != 1U)
			return false;
		return upper_fail(result, target, 0U, 0U, false, false, 0U,
		    false, 0U);
	}
	if (target != YT_BASIC_FAULT_SITE_COUNT) {
		if (occurrence > length)
			return false;
		if ((target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
		    || target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE)
		    && text[occurrence - 1U] <= (uint8_t)'@')
			return false;
	}
	result->length = length;
	result->index = 1U;
	result->scratch_initialized = true;
	for (index = 0U; index < length; ++index) {
		uint8_t current;
		uint8_t mapped;

		result->index = index + 1U;
		if (target == YT_BASIC_FAULT_UPPER_MID_COMPARE_STRING_SPACE
		    && occurrence == index + 1U)
			return upper_fail(result, target, length, index + 1U, true,
			    false, 0U, false, 0U);
		current = text[index];
		if (current > (uint8_t)'@') {
			if (target == YT_BASIC_FAULT_UPPER_MID_VALUE_STRING_SPACE
			    && occurrence == index + 1U)
				return upper_fail(result, target, length, index + 1U,
				    true, false, 0U, false, 0U);
			mapped = current & 0xdfU;
			if (target == YT_BASIC_FAULT_UPPER_CHR_STRING_SPACE
			    && occurrence == index + 1U)
				return upper_fail(result, target, length, index + 1U,
				    true, true, current, true, mapped);
			text[index] = mapped;
		}
		result->index = index + 2U;
	}
	return true;
}

void
yt_input_compat_upper_n(uint8_t *text, size_t length)
{
	struct yt_upper_transform result;

	(void)yt_input_compat_upper_n_staged(text, length,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result);
}

bool
yt_input_repeat_prefix_staged(const uint8_t *text, size_t length,
    uint8_t *upper_scratch, size_t scratch_capacity,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_repeat_prefix_transform *result)
{
	struct yt_upper_transform upper;
	size_t index;
	size_t repeat_position = 0U;

	if (text == NULL || upper_scratch == NULL || scratch_capacity == 0U
	    || result == NULL || occurrence == 0U || length >= scratch_capacity
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE
	    && !upper_fault_target(target)))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	result->upper.fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (target == YT_BASIC_FAULT_ADE0_UPPER_SCRATCH_CLONE_SPACE) {
		if (occurrence != 1U || length == 0U)
			return false;
		result->fault_site = target;
		result->fault_valid = true;
		return false;
	}
	memcpy(upper_scratch, text, length);
	upper_scratch[length] = '\0';
	if (!yt_input_compat_upper_n_staged(upper_scratch, length,
	    upper_fault_target(target) ? target : YT_BASIC_FAULT_SITE_COUNT,
	    occurrence, &upper)) {
		result->upper = upper;
		if (upper.fault_valid) {
			result->fault_site = upper.fault_site;
			result->fault_valid = true;
		}
		return false;
	}
	result->upper = upper;
	for (index = 0U; index + 1U < length; ++index) {
		if (upper_scratch[index] == (uint8_t)'/'
		    && upper_scratch[index + 1U] == (uint8_t)'R') {
			repeat_position = index + 1U;
			break;
		}
	}
	if (qb_mbf32_encode((float)repeat_position, result->work_raw)
	    != QB_MBF_OK)
		return false;
	result->repeat_position = repeat_position;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static bool
repeat_parse_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE
	    || target == YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW
	    || target == YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW;
}

static bool
repeat_parse_fault(struct yt_repeat_parse_transform *result,
    enum yt_basic_fault_site site, enum yt_repeat_failure failure)
{
	result->fault_site = site;
	result->failure = failure;
	result->fault_valid = true;
	return false;
}

bool
yt_input_repeat_parse_staged(char *text, size_t text_capacity,
    uint8_t *upper_scratch, size_t scratch_capacity,
    size_t repeat_position, enum yt_basic_fault_site target,
    struct yt_repeat_parse_transform *result)
{
	struct qb_val_result parsed;
	double integer;
	size_t text_length;
	size_t upper_length;
	size_t prefix_length;
	size_t suffix_offset;
	size_t suffix_length;
	float count;

	if (text == NULL || text_capacity == 0U || upper_scratch == NULL
	    || scratch_capacity == 0U || result == NULL
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !repeat_parse_target(target))
	    || !bounded_string_length(text, text_capacity, &text_length)
	    || !bounded_string_length((const char *)upper_scratch,
	    scratch_capacity, &upper_length))
		return false;
	memset(result, 0, sizeof(*result));
	result->failure = YT_REPEAT_FAILURE_NONE;
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	if (repeat_position == 0U)
		return target == YT_BASIC_FAULT_SITE_COUNT;
	if (repeat_position >= upper_length
	    || upper_scratch[repeat_position - 1U] != (uint8_t)'/'
	    || upper_scratch[repeat_position] != (uint8_t)'R')
		return false;
	result->repeat_reached = true;
	prefix_length = repeat_position - 1U;
	suffix_offset = repeat_position + 1U;
	suffix_length = upper_length - suffix_offset;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_LEFT_SPACE) {
		if (prefix_length == 0U)
			return false;
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	}
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE
	    && suffix_length == 0U)
		return false;
	if (prefix_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, text, prefix_length);
	result->pending_string[prefix_length] = '\0';
	result->pending_length = prefix_length;
	result->pending_role = YT_REPEAT_PENDING_PREFIX;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SEMICOLON_CONCAT_SPACE)
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	if (prefix_length + 2U > text_capacity)
		return false;
	memcpy(text, result->pending_string, prefix_length);
	text[prefix_length] = ';';
	text[prefix_length + 1U] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SUFFIX_RIGHT_SPACE) {
		return repeat_parse_fault(result, target,
		    YT_REPEAT_FAILURE_NONE);
	}
	if (suffix_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, upper_scratch + suffix_offset,
	    suffix_length);
	result->pending_string[suffix_length] = '\0';
	result->pending_length = suffix_length;
	result->pending_role = YT_REPEAT_PENDING_SUFFIX;
	parsed = qb_val(result->pending_string);
	if (parsed.overflow) {
		if (target != YT_BASIC_FAULT_SITE_COUNT
		    && target != YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW)
			return false;
		return repeat_parse_fault(result,
		    YT_BASIC_FAULT_REPEAT_VAL_OVERFLOW,
		    YT_REPEAT_FAILURE_VAL_OVERFLOW);
	}
	integer = qb_int(parsed.valid ? parsed.value : 0.0);
	if (qb_mbf64_encode(integer, result->pending_double_raw) != QB_MBF_OK)
		return false;
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_INTEGER;
	result->pending_double_valid = true;
	count = (float)integer;
	if (qb_mbf32_encode(count, result->count_raw) == QB_MBF_OVERFLOW) {
		if (target != YT_BASIC_FAULT_SITE_COUNT
		    && target != YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW)
			return false;
		return repeat_parse_fault(result,
		    YT_BASIC_FAULT_REPEAT_SINGLE_OVERFLOW,
		    YT_REPEAT_FAILURE_SINGLE_OVERFLOW);
	}
	upper_scratch[0] = '\0';
	result->pending_role = YT_REPEAT_PENDING_NONE;
	result->pending_double_valid = false;
	if (count > 10.0f) {
		count = 20.0f;
		if (qb_mbf32_encode(count, result->count_raw) != QB_MBF_OK)
			return false;
	}
	result->count = count;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static bool
repeat_build_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK;
}

static bool
repeat_build_fault(struct yt_repeat_build_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_repeat_build_staged(char *text, size_t text_capacity,
    uint8_t *build_scratch, size_t scratch_capacity,
    char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity, float count,
    enum yt_basic_fault_site target, size_t occurrence,
    struct yt_repeat_build_transform *result)
{
	static const char notice_prefix[] = "Command Repeated";
	static const char notice_suffix[] =
	    " times -+- Ctrl-R to Re-use -+- Ctrl-X to cancel.";
	char count_text[64];
	size_t text_length;
	size_t scratch_length;
	size_t used = 0U;
	size_t expanded_length;
	size_t count_length;
	size_t prefix_length;
	size_t notice_length;
	int copies;
	int index;

	if (text == NULL || text_capacity == 0U || build_scratch == NULL
	    || scratch_capacity == 0U || saved_command == NULL
	    || saved_capacity == 0U || output_source == NULL
	    || output_capacity == 0U || result == NULL || occurrence == 0U
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && !repeat_build_target(target))
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
	    && occurrence != 1U)
	    || !bounded_string_length(text, text_capacity, &text_length)
	    || !bounded_string_length((const char *)build_scratch,
	    scratch_capacity, &scratch_length)
	    || scratch_length != 0U || text_length == 0U
	    || text[text_length - 1U] != ';' || !isfinite(count)
	    || count <= 0.0f || floorf(count) != count
	    || (count > 10.0f && count != 20.0f))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	copies = (int)count;
	for (index = 0; index < copies; ++index) {
		if (used > 500U)
			break;
		if (target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE
		    && occurrence == (size_t)index + 1U)
			return repeat_build_fault(result, target);
		if (used + text_length >= scratch_capacity)
			return false;
		memcpy(build_scratch + used, text, text_length);
		used += text_length;
		build_scratch[used] = '\0';
		result->completed_iterations = (size_t)index + 1U;
	}
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_BUILD_CONCAT_SPACE)
		return false;
	if (used == 0U)
		return false;
	expanded_length = used - 1U;
	if (expanded_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, build_scratch, expanded_length);
	result->pending_string[expanded_length] = '\0';
	result->pending_length = expanded_length;
	result->pending_role = YT_REPEAT_PENDING_EXPANDED;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_FINAL_LEFT_SPACE) {
		if (expanded_length == 0U)
			return false;
		return repeat_build_fault(result, target);
	}
	if (expanded_length >= text_capacity)
		return false;
	memcpy(text, result->pending_string, expanded_length + 1U);
	build_scratch[0] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_SAVE_CLONE_SPACE) {
		if (expanded_length == 0U)
			return false;
		return repeat_build_fault(result, target);
	}
	if (expanded_length >= saved_capacity)
		return false;
	memcpy(saved_command, text, expanded_length + 1U);
	result->bold_committed = true;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_COUNT_STR_SPACE)
		return repeat_build_fault(result, target);
	if (qb_str_double(count_text, sizeof(count_text), (double)count) < 0)
		return false;
	count_length = strlen(count_text);
	if (count_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, count_text, count_length + 1U);
	result->pending_length = count_length;
	result->pending_role = YT_REPEAT_PENDING_COUNT_TEXT;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_PREFIX_CONCAT_SPACE)
		return repeat_build_fault(result, target);
	prefix_length = sizeof(notice_prefix) - 1U + count_length;
	if (prefix_length >= sizeof(result->pending_string))
		return false;
	memmove(result->pending_string + sizeof(notice_prefix) - 1U,
	    result->pending_string, count_length + 1U);
	memcpy(result->pending_string, notice_prefix,
	    sizeof(notice_prefix) - 1U);
	result->pending_length = prefix_length;
	result->pending_role = YT_REPEAT_PENDING_NOTICE_PREFIX;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_CONCAT_SPACE)
		return repeat_build_fault(result, target);
	notice_length = prefix_length + sizeof(notice_suffix) - 1U;
	if (notice_length >= output_capacity)
		return false;
	memcpy(output_source, result->pending_string, prefix_length);
	memcpy(output_source + prefix_length, notice_suffix,
	    sizeof(notice_suffix));
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_REPEAT_PENDING_NONE;
	result->notice_ready = true;
	if (target == YT_BASIC_FAULT_ADE0_REPEAT_NOTICE_GOSUB_STACK)
		return repeat_build_fault(result, target);
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

bool
yt_input_expand_repeat_with_notice(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    char *output_source, size_t output_capacity,
    struct yt_repeat_transform *result)
{
	uint8_t upper[YT_INPUT_PENDING];
	struct yt_repeat_prefix_transform repeat_prefix;
	struct yt_repeat_parse_transform repeat_parse;
	struct yt_repeat_build_transform repeat_build;
	size_t text_length;
	float count;

	if (text == NULL || saved_command == NULL || output_source == NULL
	    || result == NULL || text_capacity == 0 || saved_capacity == 0
	    || output_capacity == 0)
		return false;
	result->emit_notice = false;
	result->bold_committed = false;
	result->count = 0.0f;
	result->failure = YT_REPEAT_FAILURE_NONE;
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	result->fault_valid = false;
	text_length = strlen(text);
	if (text_length >= sizeof(upper))
		return false;
	if (!yt_input_repeat_prefix_staged((const uint8_t *)text, text_length,
	    upper, sizeof(upper), YT_BASIC_FAULT_SITE_COUNT, 1U,
	    &repeat_prefix))
		return false;
	if (repeat_prefix.repeat_position == 0U)
		return true;
	if (!yt_input_repeat_parse_staged(text, text_capacity, upper,
	    sizeof(upper), repeat_prefix.repeat_position,
	    YT_BASIC_FAULT_SITE_COUNT, &repeat_parse)) {
		result->failure = repeat_parse.failure;
		result->fault_site = repeat_parse.fault_site;
		result->fault_valid = repeat_parse.fault_valid;
		return false;
	}
	count = repeat_parse.count;
	result->count = count;
	if (count <= 0.0f)
		return true;
	if (!yt_input_repeat_build_staged(text, text_capacity, upper,
	    sizeof(upper), saved_command, saved_capacity, output_source,
	    output_capacity, count, YT_BASIC_FAULT_SITE_COUNT, 1U,
	    &repeat_build)) {
		result->fault_site = repeat_build.fault_site;
		result->fault_valid = repeat_build.fault_valid;
		return false;
	}
	result->bold_committed = repeat_build.bold_committed;
	result->emit_notice = repeat_build.notice_ready;
	return true;
}

bool
yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length)
{
	struct yt_semicolon_transform result;
	size_t text_length;

	if (text == NULL)
		return false;
	text_length = strlen(text);
	return yt_input_split_semicolon_staged(text, text_length + 1U,
	    queue, queue_capacity, queue_position, queue_length,
	    YT_BASIC_FAULT_SITE_COUNT, 1U, &result);
}

static bool
semicolon_target(enum yt_basic_fault_site target)
{
	return target == YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE
	    || target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE;
}

static bool
semicolon_fault(struct yt_semicolon_transform *result,
    enum yt_basic_fault_site site)
{
	result->fault_site = site;
	result->fault_valid = true;
	return false;
}

bool
yt_input_split_semicolon_staged(char *text, size_t text_capacity,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, enum yt_basic_fault_site target,
    size_t occurrence, struct yt_semicolon_transform *result)
{
	char old_queue[YT_INPUT_PENDING];
	char *semicolon;
	char *replacement;
	size_t text_length;
	size_t tail_length;
	size_t old_length;
	size_t combined_length;
	size_t prefix_length;

	if (text == NULL || text_capacity == 0U || queue == NULL
	    || queue_capacity == 0U || queue_position == NULL
	    || queue_length == NULL || result == NULL || occurrence == 0U
	    || *queue_length < *queue_position
	    || *queue_length >= queue_capacity
	    || (target != YT_BASIC_FAULT_SITE_COUNT && !semicolon_target(target))
	    || (target != YT_BASIC_FAULT_SITE_COUNT
	    && target != YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
	    && occurrence != 1U)
	    || !bounded_string_length(text, text_capacity, &text_length))
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_BASIC_FAULT_SITE_COUNT;
	semicolon = memchr(text, ';', text_length);
	result->semicolon_position = semicolon == NULL ? 0U
	    : (size_t)(semicolon - text) + 1U;
	if (semicolon == NULL)
		return target == YT_BASIC_FAULT_SITE_COUNT;
	tail_length = text_length - result->semicolon_position;
	old_length = *queue_length - *queue_position;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_TAIL_MID_SPACE) {
		if (tail_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	if (tail_length >= sizeof(result->pending_string))
		return false;
	memcpy(result->pending_string, semicolon + 1U, tail_length);
	result->pending_string[tail_length] = '\0';
	result->pending_length = tail_length;
	result->pending_role = YT_SEMICOLON_PENDING_TAIL;
	combined_length = tail_length + old_length;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_QUEUE_CONCAT_SPACE) {
		if (combined_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	if (old_length > sizeof(old_queue) || combined_length >= queue_capacity)
		return false;
	if (old_length != 0U)
		memcpy(old_queue, queue + *queue_position, old_length);
	if (tail_length != 0U)
		memcpy(queue, result->pending_string, tail_length);
	if (old_length != 0U)
		memcpy(queue + tail_length, old_queue, old_length);
	queue[combined_length] = '\0';
	*queue_position = 0U;
	*queue_length = combined_length;
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_SEMICOLON_PENDING_NONE;
	prefix_length = result->semicolon_position - 1U;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_PREFIX_LEFT_SPACE) {
		if (prefix_length == 0U)
			return false;
		return semicolon_fault(result, target);
	}
	text[prefix_length] = '\0';
	for (;;) {
		replacement = memchr(queue, ';', *queue_length);
		if (replacement == NULL)
			break;
		if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE
		    && occurrence == result->replacements + 1U)
			return semicolon_fault(result, target);
		*replacement = '\r';
		++result->replacements;
	}
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_REPLACEMENT_CHR_SPACE)
		return false;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CR_SPACE)
		return semicolon_fault(result, target);
	result->pending_string[0] = '\r';
	result->pending_string[1] = '\0';
	result->pending_length = 1U;
	result->pending_role = YT_SEMICOLON_PENDING_FINAL_CR;
	if (target == YT_BASIC_FAULT_ADE0_SEMICOLON_FINAL_CONCAT_SPACE)
		return semicolon_fault(result, target);
	if (*queue_length + 1U >= queue_capacity)
		return false;
	queue[*queue_length] = '\r';
	++*queue_length;
	queue[*queue_length] = '\0';
	result->pending_string[0] = '\0';
	result->pending_length = 0U;
	result->pending_role = YT_SEMICOLON_PENDING_NONE;
	return target == YT_BASIC_FAULT_SITE_COUNT;
}

static const struct yt_confirmation_fault_identity confirmation_faults[] = {
	{"none", 0U, 0U, 0U, 0, 0U, {0U}, 0U},
	{"LEFT$ first byte", 0xA8EFU, 0xA8F2U, 0xA8E7U, 40001,
	    0x4C9AU, {14U, 16U, 0x0AC9U}, 3U},
	{"first-byte COPY", 0xA8F4U, 0xA8F7U, 0xA8E7U, 40001,
	    0x4C9AU, {0x0ACCU}, 1U},
	{"invalid queue clear", 0xA938U, 0xA93BU, 0xA932U, 40001,
	    0x4BE0U, {0x0ACCU}, 1U},
	{"prompt clear", 0xA943U, 0xA946U, 0xA93DU, 40001,
	    0x4D3AU, {0x0ACCU}, 1U},
};

_Static_assert(YT_ARRAY_LEN(confirmation_faults) == YT_CONFIRMATION_FAULT_SITE_COUNT,
    "A8D2 fault identity table is incomplete");

const struct yt_confirmation_fault_identity *
yt_input_confirmation_fault_identity(enum yt_confirmation_fault_site site)
{
	if (site <= YT_CONFIRMATION_FAULT_NONE
	    || (unsigned)site >= YT_ARRAY_LEN(confirmation_faults))
		return NULL;
	return &confirmation_faults[(size_t)site];
}

static bool
confirmation_fault_admits(enum yt_confirmation_fault_site site, uint16_t error_number)
{
	const struct yt_confirmation_fault_identity *identity =
	    yt_input_confirmation_fault_identity(site);
	size_t index;

	if (identity == NULL)
		return false;
	for (index = 0U; index < identity->error_count; ++index)
		if (identity->errors[index] == error_number)
			return true;
	return false;
}

static bool
confirmation_fail(struct yt_confirmation_transform *result,
	    enum yt_confirmation_fault_site site, uint16_t error_number)
{
	result->fault_site = site;
	result->error_number = error_number;
	result->outcome = error_number == 14U || error_number == 16U
	    ? YT_CONFIRMATION_BASIC_ERROR : YT_CONFIRMATION_INTERNAL_FATAL;
	return true;
}

bool
yt_input_confirmation_staged(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    uint8_t *prompt, size_t prompt_capacity, size_t *prompt_length,
    char *queue, size_t queue_capacity, size_t *queue_position,
    size_t *queue_length, float *bold,
    enum yt_confirmation_fault_site target, uint16_t error_number,
    struct yt_confirmation_transform *result)
{
	size_t length;
	char first;

	if (command_accumulator == NULL || output_source == NULL
	    || output_source_capacity < 2U || prompt == NULL
	    || prompt_capacity == 0U || prompt_length == NULL
	    || *prompt_length > prompt_capacity
	    || queue == NULL || queue_capacity == 0U
	    || queue_position == NULL || queue_length == NULL || bold == NULL
	    || result == NULL || *queue_position > *queue_length
	    || *queue_length >= queue_capacity
	    || (target != YT_CONFIRMATION_FAULT_NONE
	    && !confirmation_fault_admits(target, error_number))
	    || (target == YT_CONFIRMATION_FAULT_NONE && error_number != 0U))
		return false;
	length = strlen(command_accumulator);
	if (length >= output_source_capacity)
		return false;
	memset(result, 0, sizeof(*result));
	result->fault_site = YT_CONFIRMATION_FAULT_NONE;
	memcpy(output_source, command_accumulator, length + 1U);
	qb_compat_upper(output_source);
	result->uppercase_complete = true;
	if (target == YT_CONFIRMATION_FAULT_LEFT_ONE) {
		if (length == 0U)
			return false;
		return confirmation_fail(result, target, error_number);
	}
	first = output_source[0];
	result->left_complete = true;
	if (target == YT_CONFIRMATION_FAULT_FIRST_COPY)
		return confirmation_fail(result, target, error_number);
	if (first != '\0')
		output_source[1] = '\0';
	result->first_copy_complete = true;
	if (first == '\0')
		result->answer = YT_YES_NO_EMPTY;
	else if (first == 'Y')
		result->answer = YT_YES_NO_YES;
	else if (first == 'N')
		result->answer = YT_YES_NO_NO;
	else
		result->answer = YT_YES_NO_INVALID;
	result->answer_valid = true;
	if (result->answer == YT_YES_NO_INVALID) {
		*bold = 1.0f;
		result->bold_committed = true;
		if (target == YT_CONFIRMATION_FAULT_INVALID_QUEUE_CLEAR)
			return confirmation_fail(result, target, error_number);
		if (target == YT_CONFIRMATION_FAULT_PROMPT_CLEAR)
			return false;
		if (!yt_input_queue_clear(queue, queue_capacity,
		    queue_position, queue_length))
			return false;
		result->queue_cleared = true;
		result->outcome = YT_CONFIRMATION_RETRY;
		return true;
	}
	if (target == YT_CONFIRMATION_FAULT_INVALID_QUEUE_CLEAR)
		return false;
	if (target == YT_CONFIRMATION_FAULT_PROMPT_CLEAR)
		return confirmation_fail(result, target, error_number);
	*prompt_length = 0U;
	prompt[0] = 0U;
	result->prompt_cleared = true;
	result->outcome = YT_CONFIRMATION_RETURNED;
	return true;
}

bool
yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer)
{
	struct yt_confirmation_transform result;
	uint8_t prompt[1] = {0U};
	size_t prompt_length = 0U;
	char queue[1] = "";
	size_t queue_position = 0U;
	size_t queue_length = 0U;
	float bold = 0.0f;

	if (answer == NULL || !yt_input_confirmation_staged(command_accumulator,
	    output_source, output_source_capacity, prompt, sizeof(prompt),
	    &prompt_length, queue, sizeof(queue), &queue_position, &queue_length,
	    &bold,
	    YT_CONFIRMATION_FAULT_NONE, 0U, &result) || !result.answer_valid)
		return false;
	*answer = result.answer;
	return true;
}

bool
yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue)
{
	if (state == NULL || initial_residue == NULL
	    || initial_residue->length > sizeof(state->residue))
		return false;
	memset(state, 0, sizeof(*state));
	if (initial_residue->length != 0)
		memcpy(state->residue, initial_residue->bytes,
		    initial_residue->length);
	state->residue_length = initial_residue->length;
	return true;
}

enum yt_input_drain_reason
yt_input_drain_local(struct yt_input_drain_state *state,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || (selected->remote && selected->length != 0))
		return YT_INPUT_DRAIN_ERROR;
	++state->local_reads;
	if (state->expect_paired_local) {
		if (selected->length != 0)
			memcpy(state->residue, selected->bytes,
			    selected->length);
		state->residue_length = selected->length;
		state->expect_paired_local = false;
		return YT_INPUT_DRAIN_CONTINUE;
	}
	if (selected->length == 0)
		return YT_INPUT_DRAIN_LOCAL_COMPLETE;
	state->expect_paired_local = true;
	return YT_INPUT_DRAIN_CONTINUE;
}

enum yt_input_drain_reason
yt_input_drain_serial(struct yt_input_drain_state *state, float mode,
    const struct yt_input_value *selected)
{
	if (state == NULL || selected == NULL || selected->length > 2U
	    || state->expect_paired_local)
		return YT_INPUT_DRAIN_ERROR;
	if (mode != 0.0f)
		return YT_INPUT_DRAIN_COMPLETE;
	++state->loc_reads;
	if (selected->length == 0)
		return YT_INPUT_DRAIN_COMPLETE;
	if (!selected->remote || selected->length != 1U)
		return YT_INPUT_DRAIN_ERROR;
	state->residue[0] = selected->bytes[0];
	state->residue_length = 1;
	++state->serial_reads;
	return YT_INPUT_DRAIN_CONTINUE;
}
