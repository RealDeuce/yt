#ifndef YT_INPUT_MODEL_H
#define YT_INPUT_MODEL_H

#include "yt_common.h"
#include "yt_brun_fatal.h"
#include "yt_main_error.h"

#define YT_INPUT_PENDING 4096U

enum yt_input_fault_family {
	YT_INPUT_FAULT_B05D,
	YT_INPUT_FAULT_B1F3,
	YT_INPUT_FAULT_AB36,
};

enum yt_input_fault_module {
	YT_INPUT_FAULT_MODULE_YT,
	YT_INPUT_FAULT_MODULE_YT_SUB,
};

struct yt_input_fault_site {
	enum yt_input_fault_module module;
	uint16_t address;
	uint16_t saved_ip;
	uint16_t statement;
	int32_t source_line;
	uint8_t error_number;
	bool live;
};

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

struct yt_b05d_key_state {
	char *accumulator;
	size_t accumulator_capacity;
	char *queue;
	size_t queue_capacity;
	size_t *queue_position;
	size_t *queue_length;
	char *pager_key;
	size_t pager_key_capacity;
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

struct yt_upper_transform {
	size_t length;
	size_t index;
	uint8_t extracted;
	uint8_t mapped;
	enum yt_basic_fault_site fault_site;
	bool scratch_initialized;
	bool extracted_valid;
	bool mapped_valid;
	bool fault_valid;
};

struct yt_command_save_transform {
	enum yt_basic_fault_site fault_site;
	bool save_requested;
	bool notice_ready;
	bool fault_valid;
};

struct yt_repeat_prefix_transform {
	struct yt_upper_transform upper;
	uint8_t work_raw[4];
	size_t repeat_position;
	enum yt_basic_fault_site fault_site;
	bool fault_valid;
};

enum yt_repeat_pending_role {
	YT_REPEAT_PENDING_NONE,
	YT_REPEAT_PENDING_PREFIX,
	YT_REPEAT_PENDING_SUFFIX,
	YT_REPEAT_PENDING_INTEGER,
	YT_REPEAT_PENDING_EXPANDED,
	YT_REPEAT_PENDING_COUNT_TEXT,
	YT_REPEAT_PENDING_NOTICE_PREFIX,
};

struct yt_repeat_parse_transform {
	char pending_string[YT_INPUT_PENDING];
	size_t pending_length;
	uint8_t pending_double_raw[8];
	uint8_t count_raw[4];
	float count;
	enum yt_repeat_pending_role pending_role;
	enum yt_repeat_failure failure;
	enum yt_basic_fault_site fault_site;
	bool repeat_reached;
	bool pending_double_valid;
	bool fault_valid;
};

struct yt_repeat_build_transform {
	char pending_string[YT_INPUT_PENDING];
	size_t pending_length;
	size_t completed_iterations;
	enum yt_repeat_pending_role pending_role;
	enum yt_basic_fault_site fault_site;
	bool bold_committed;
	bool notice_ready;
	bool fault_valid;
};

enum yt_semicolon_pending_role {
	YT_SEMICOLON_PENDING_NONE,
	YT_SEMICOLON_PENDING_TAIL,
	YT_SEMICOLON_PENDING_FINAL_CR,
};

struct yt_semicolon_transform {
	char pending_string[YT_INPUT_PENDING];
	size_t pending_length;
	size_t semicolon_position;
	size_t replacements;
	enum yt_semicolon_pending_role pending_role;
	enum yt_basic_fault_site fault_site;
	bool fault_valid;
};

enum yt_yes_no_answer {
	YT_YES_NO_EMPTY,
	YT_YES_NO_YES,
	YT_YES_NO_NO,
	YT_YES_NO_INVALID,
};

enum yt_a8d2_fault_site {
	YT_A8D2_FAULT_NONE,
	YT_A8D2_FAULT_LEFT_ONE,
	YT_A8D2_FAULT_FIRST_COPY,
	YT_A8D2_FAULT_INVALID_QUEUE_CLEAR,
	YT_A8D2_FAULT_PROMPT_CLEAR,
	YT_A8D2_FAULT_SITE_COUNT,
};

enum yt_a8d2_outcome {
	YT_A8D2_RETURNED,
	YT_A8D2_RETRY,
	YT_A8D2_BASIC_ERROR,
	YT_A8D2_INTERNAL_FATAL,
};

#define YT_A8D2_FAULT_ERRORS 3U

struct yt_a8d2_fault_identity {
	const char *name;
	uint16_t instruction;
	uint16_t saved_ip;
	uint16_t statement;
	int32_t source_line;
	uint16_t destination;
	uint16_t errors[YT_A8D2_FAULT_ERRORS];
	size_t error_count;
};

struct yt_a8d2_transform {
	enum yt_a8d2_outcome outcome;
	enum yt_a8d2_fault_site fault_site;
	enum yt_yes_no_answer answer;
	uint16_t error_number;
	bool answer_valid;
	bool uppercase_complete;
	bool left_complete;
	bool first_copy_complete;
	bool bold_committed;
	bool queue_cleared;
	bool prompt_cleared;
};

enum yt_ab36_terminal_kind {
	YT_AB36_TERMINAL_INACTIVITY,
	YT_AB36_TERMINAL_SESSION_LIMIT,
};

typedef bool (*yt_ab36_terminal_notice_fn)(void *context,
    const uint8_t *notice, size_t length);
typedef bool (*yt_ab36_terminal_close_fn)(void *context);
typedef bool (*yt_ab36_repeat_emit_fn)(void *context,
    const uint8_t *prefix, size_t length);
typedef bool (*yt_ab36_submit_line_fn)(void *context);
typedef bool (*yt_ab36_echo_fn)(void *context,
    const uint8_t *local, size_t local_length, const uint8_t *remote,
    size_t remote_length);
typedef bool (*yt_ab36_carrier_fn)(void *context);
typedef void (*yt_input_process_store_fn)(void *context, uint16_t address,
    const uint8_t raw[4]);

enum yt_input_drain_reason {
	YT_INPUT_DRAIN_CONTINUE,
	YT_INPUT_DRAIN_LOCAL_COMPLETE,
	YT_INPUT_DRAIN_COMPLETE,
	YT_INPUT_DRAIN_ERROR,
};

enum yt_opening_row_route {
	YT_OPENING_ROW_CONTINUE,
	YT_OPENING_ROW_STOP_LOCAL,
	YT_OPENING_ROW_STOP_REMOTE,
};

struct yt_input_drain_state {
	uint8_t residue[2];
	size_t residue_length;
	size_t local_reads;
	size_t loc_reads;
	size_t serial_reads;
	bool expect_paired_local;
};

size_t yt_input_fault_site_count(enum yt_input_fault_family family);
bool yt_input_fault_site(enum yt_input_fault_family family, size_t index,
    struct yt_input_fault_site *site);
enum yt_radio_body_key_action yt_input_radio_body_key(uint8_t key,
    size_t current_length);
bool yt_input_ab36_queue_pop(char *queue, size_t capacity,
    size_t *position, size_t *length, struct yt_input_value *selected);
bool yt_input_queue_clear(char *queue, size_t capacity,
    size_t *position, size_t *length);
bool yt_input_queue_prepend_program(char *queue, size_t capacity,
	size_t *position, size_t *length, const char *program,
	size_t program_length);
bool yt_input_ab36_repeat_requested(bool queued,
    const struct yt_input_value *selected);
bool yt_input_ab36_repeat_run(char *accumulator,
    size_t accumulator_capacity, const char *saved_command,
    size_t saved_capacity, char *paged_text, size_t paged_text_capacity,
    float *newline_flag, uint8_t *selected_key,
    yt_ab36_repeat_emit_fn emit, void *context);
bool yt_input_ab36_submit_requested(uint8_t selected_key);
bool yt_input_ab36_submit_run(float *newline_flag,
    yt_ab36_submit_line_fn line, void *context);
bool yt_input_ab36_backspace_run(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, bool *handled,
    yt_ab36_echo_fn echo, void *context);
bool yt_input_ab36_printable_run(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, size_t response_capacity,
    char *paged_text, size_t paged_text_capacity, float *newline_flag,
    bool *handled, yt_ab36_echo_fn echo, yt_ab36_carrier_fn carrier,
    void *context);
bool yt_input_command_save_requested(const char *text, size_t capacity,
	bool *requested);
bool yt_input_command_save_staged(char *text, size_t text_capacity,
	char *queue, size_t queue_capacity, size_t *queue_position,
	size_t *queue_length, char *saved_command, size_t saved_capacity,
	char *output_source, size_t output_capacity,
	enum yt_basic_fault_site target,
	struct yt_command_save_transform *result);
bool yt_input_ab36_inactivity_expired(float timer, float deadline,
    float mode);
bool yt_input_ab36_session_expired(float timer, float deadline);
bool yt_input_carrier_returns(float mode, bool carrier_detected);
enum yt_opening_row_route yt_input_opening_row_route(bool local_key,
    bool remote_pending);
bool yt_input_ab36_terminal_run(enum yt_ab36_terminal_kind kind,
    bool *running, bool *terminated, yt_ab36_terminal_notice_fn notice,
    yt_ab36_terminal_close_fn close_all, void *context);
bool yt_b05d_process_key(const struct yt_input_value *value,
    struct yt_b05d_key_state *state);
bool yt_input_expand_repeat(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    struct yt_repeat_transform *result);
void yt_input_compat_upper_n_observed(uint8_t *text, size_t length,
	yt_input_process_store_fn store, void *context);
bool yt_input_compat_upper_n_staged(uint8_t *text, size_t length,
	enum yt_basic_fault_site target, size_t occurrence,
	struct yt_upper_transform *result, yt_input_process_store_fn store,
	void *context);
bool yt_input_repeat_prefix_staged(const uint8_t *text, size_t length,
	uint8_t *upper_scratch, size_t scratch_capacity,
	enum yt_basic_fault_site target, size_t occurrence,
	struct yt_repeat_prefix_transform *result,
	yt_input_process_store_fn store, void *context);
bool yt_input_repeat_parse_staged(char *text, size_t text_capacity,
	uint8_t *upper_scratch, size_t scratch_capacity,
	size_t repeat_position, enum yt_basic_fault_site target,
	struct yt_repeat_parse_transform *result,
	yt_input_process_store_fn store, void *context);
bool yt_input_repeat_build_staged(char *text, size_t text_capacity,
	uint8_t *build_scratch, size_t scratch_capacity,
	char *saved_command, size_t saved_capacity,
	char *output_source, size_t output_capacity, float count,
	enum yt_basic_fault_site target, size_t occurrence,
	struct yt_repeat_build_transform *result,
	yt_input_process_store_fn store, void *context);
bool yt_input_expand_repeat_observed(char *text, size_t text_capacity,
	char *saved_command, size_t saved_capacity,
	char *output_source, size_t output_capacity,
    struct yt_repeat_transform *result, yt_input_process_store_fn store,
    void *context);
bool yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length);
bool yt_input_split_semicolon_observed(char *text, char *queue,
    size_t queue_capacity, size_t *queue_position, size_t *queue_length,
    yt_input_process_store_fn store, void *context);
bool yt_input_split_semicolon_staged(char *text, size_t text_capacity,
	char *queue, size_t queue_capacity, size_t *queue_position,
	size_t *queue_length, enum yt_basic_fault_site target,
	size_t occurrence, struct yt_semicolon_transform *result,
	yt_input_process_store_fn store, void *context);
bool yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer);
const struct yt_a8d2_fault_identity *yt_input_a8d2_fault_identity(
	enum yt_a8d2_fault_site site);
bool yt_input_a8d2_staged(const char *command_accumulator,
	char *output_source, size_t output_source_capacity,
	uint8_t *prompt, size_t prompt_capacity, size_t *prompt_length,
	char *queue, size_t queue_capacity, size_t *queue_position,
	size_t *queue_length, float *bold,
	enum yt_a8d2_fault_site target, uint16_t error_number,
	struct yt_a8d2_transform *result);
bool yt_input_a8d2_internal_fatal_run(
	const struct yt_a8d2_transform *transform, uint16_t module_segment,
	bool redirected_stdin, bool function_bar, bool cursor_shape_known,
	uint16_t process_entry_cursor_shape,
	const struct yt_brun_internal_fatal_ops *ops, void *context,
	struct yt_brun_internal_fatal_state *state);
void yt_input_numeric_response(char *text);
bool yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue);
enum yt_input_drain_reason yt_input_drain_local(
    struct yt_input_drain_state *state,
    const struct yt_input_value *selected);
enum yt_input_drain_reason yt_input_drain_serial(
    struct yt_input_drain_state *state, float mode,
    const struct yt_input_value *selected);
#endif
