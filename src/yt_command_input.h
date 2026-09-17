#ifndef YT_COMMAND_INPUT_H
#define YT_COMMAND_INPUT_H

#include "yt_common.h"
#include "yt_main_error.h"

#define YT_INPUT_PENDING 4096U

enum yt_radio_body_key_action {
	YT_RADIO_BODY_KEY_IGNORE,
	YT_RADIO_BODY_KEY_COMMIT,
	YT_RADIO_BODY_KEY_BACKSPACE,
	YT_RADIO_BODY_KEY_PRINTABLE,
};

struct yt_input_value {
	uint8_t bytes[2];
	size_t length;
	bool remote;
};

enum yt_repeat_failure {
	YT_REPEAT_FAILURE_NONE,
	YT_REPEAT_FAILURE_VAL_OVERFLOW,
	YT_REPEAT_FAILURE_SINGLE_OVERFLOW,
};

struct yt_repeat_transform {
	bool emit_notice;
	bool bold_committed;
	float count;
	enum yt_repeat_failure failure;
	enum yt_basic_fault_site fault_site;
	bool fault_valid;
};

enum yt_yes_no_answer {
	YT_YES_NO_EMPTY,
	YT_YES_NO_YES,
	YT_YES_NO_NO,
	YT_YES_NO_INVALID,
};

enum yt_confirmation_outcome {
	YT_CONFIRMATION_RETURNED,
	YT_CONFIRMATION_RETRY,
};

enum yt_input_drain_reason {
	YT_INPUT_DRAIN_CONTINUE,
	YT_INPUT_DRAIN_LOCAL_COMPLETE,
	YT_INPUT_DRAIN_COMPLETE,
	YT_INPUT_DRAIN_ERROR,
};

struct yt_input_drain_state {
	uint8_t residue[2];
	size_t residue_length;
	bool expect_paired_local;
};

enum yt_radio_body_key_action yt_input_radio_body_key(uint8_t key,
    size_t current_length);
bool yt_input_queue_pop(char *queue, size_t capacity,
    size_t *position, size_t *length, struct yt_input_value *selected);
bool yt_input_queue_clear(char *queue, size_t capacity,
    size_t *position, size_t *length);
bool yt_input_queue_prepend_program(char *queue, size_t capacity,
	size_t *position, size_t *length, const char *program,
	size_t program_length);
bool yt_input_repeat_requested(bool queued,
    const struct yt_input_value *selected);
bool yt_input_submit_requested(uint8_t selected_key);
bool yt_input_save_command(char *text, size_t text_capacity,
	char *queue, size_t queue_capacity, size_t *queue_position,
	size_t *queue_length, char *saved_command, size_t saved_capacity,
	char *output_source, size_t output_capacity, bool *notice_ready);
void yt_input_compat_upper_n(uint8_t *text, size_t length);
bool yt_input_expand_repeat_with_notice(char *text, size_t text_capacity,
	char *saved_command, size_t saved_capacity,
	char *output_source, size_t output_capacity,
	struct yt_repeat_transform *result);
bool yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length);
bool yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer);
bool yt_input_confirmation(const char *command_accumulator,
	char *output_source, size_t output_source_capacity,
	uint8_t *prompt, size_t prompt_capacity, size_t *prompt_length,
	char *queue, size_t queue_capacity, size_t *queue_position,
	size_t *queue_length, bool *bold, enum yt_yes_no_answer *answer,
	enum yt_confirmation_outcome *outcome);
bool yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue);
enum yt_input_drain_reason yt_input_drain_local(
    struct yt_input_drain_state *state,
    const struct yt_input_value *selected);
enum yt_input_drain_reason yt_input_drain_serial(
    struct yt_input_drain_state *state, bool local_mode,
    const struct yt_input_value *selected);
#endif
