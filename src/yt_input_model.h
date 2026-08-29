#ifndef YT_INPUT_MODEL_H
#define YT_INPUT_MODEL_H

#include "yt_common.h"

#define YT_INPUT_PENDING 4096U
#define YT_WAIT_SCRATCH_SIZE 80U
#define YT_SYSOP_CHAT_EVENTS 4U
#define YT_SYSOP_F5_PHASES 7U

enum yt_input_phase {
	YT_INPUT_PHASE_B05D,
	YT_INPUT_PHASE_AB36,
	YT_INPUT_PHASE_WAIT,
};

struct yt_input_value {
	uint8_t bytes[2];
	size_t length;
	uint64_t sequence;
	bool remote;
};

struct yt_input_value_queue {
	struct yt_input_value values[YT_INPUT_PENDING];
	size_t position;
	size_t length;
};

struct yt_input_splitter {
	struct yt_input_value_queue local;
	struct yt_input_value_queue remote;
	uint64_t next_sequence;
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

struct yt_repeat_transform {
	bool emit_notice;
	float count;
};

enum yt_yes_no_answer {
	YT_YES_NO_EMPTY,
	YT_YES_NO_YES,
	YT_YES_NO_NO,
	YT_YES_NO_INVALID,
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
typedef bool (*yt_ab36_backspace_echo_fn)(void *context,
    const uint8_t *local, size_t local_length, const uint8_t *remote,
    size_t remote_length);

enum yt_timed_wait_reason {
	YT_TIMED_WAIT_CONTINUE,
	YT_TIMED_WAIT_TIMER,
	YT_TIMED_WAIT_LOCAL,
	YT_TIMED_WAIT_SERIAL,
	YT_TIMED_WAIT_ERROR,
};

struct yt_timed_wait_state {
	float duration_cell;
	uint8_t serial_scratch[YT_WAIT_SCRATCH_SIZE];
	size_t serial_scratch_length;
	size_t timer_reads;
	size_t local_reads;
	size_t loc_reads;
	size_t serial_reads;
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
	size_t local_reads;
	size_t loc_reads;
	size_t serial_reads;
	bool expect_paired_local;
};

enum yt_sysop_chat_step_result {
	YT_SYSOP_CHAT_CONTINUE,
	YT_SYSOP_CHAT_EXIT,
	YT_SYSOP_CHAT_CARRIER_END,
	YT_SYSOP_CHAT_INVALID,
};

enum yt_sysop_chat_destination {
	YT_SYSOP_CHAT_LOCAL_RAW,
	YT_SYSOP_CHAT_LOCAL_GATED,
	YT_SYSOP_CHAT_SERIAL,
};

struct yt_sysop_chat_event {
	enum yt_sysop_chat_destination destination;
	bool line;
	uint8_t data[3];
	size_t length;
};

struct yt_sysop_chat_output {
	struct yt_sysop_chat_event events[YT_SYSOP_CHAT_EVENTS];
	size_t count;
};

struct yt_sysop_chat_state {
	float mode;
	float snoop;
	float foreground;
	float bold;
	float deadline;
	float inactivity_deadline;
	float saved_remaining;
	float newline_flag;
	uint8_t key[2];
	size_t key_length;
	uint8_t command_accumulator[YT_INPUT_PENDING];
	size_t command_accumulator_length;
	uint8_t queue[YT_INPUT_PENDING];
	size_t queue_length;
	size_t polls;
	size_t carrier_checks;
	size_t timer_reads;
	bool exited;
	bool terminated;
};

struct yt_sysop_chat_poll {
	struct yt_input_value local;
	struct yt_input_value remote;
	uint8_t modem_status;
	int position_after_output;
};

enum yt_sysop_f5_phase {
	YT_SYSOP_F5_CHECKPOINT,
	YT_SYSOP_F5_BASIC_END,
	YT_SYSOP_F5_REGISTERED_CLEANUP,
	YT_SYSOP_F5_CALLBACK_CLEANUP,
	YT_SYSOP_F5_RUNTIME_CLEANUP,
	YT_SYSOP_F5_COMMON_CLEANUP,
	YT_SYSOP_F5_DOS_EXIT,
};

struct yt_sysop_f5_event {
	enum yt_sysop_f5_phase phase;
	uint16_t address;
};

struct yt_sysop_f5_result {
	struct yt_sysop_f5_event events[YT_SYSOP_F5_PHASES];
	size_t event_count;
	bool same_f5_pending;
	bool local_end_cleanup;
	bool event_returned;
	bool terminated;
	int exit_status;
};

void yt_input_splitter_init(struct yt_input_splitter *splitter);
bool yt_input_splitter_can_push(const struct yt_input_splitter *splitter,
    bool remote);
bool yt_input_splitter_push(struct yt_input_splitter *splitter, bool remote,
    const struct yt_input_value *value);
struct yt_input_value yt_input_splitter_select(
    struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase);
struct yt_input_value yt_input_splitter_select_merged(
    struct yt_input_splitter *splitter);
struct yt_input_value yt_input_splitter_select_source(
    struct yt_input_splitter *splitter, bool remote);
bool yt_input_ab36_remote_replace(float mode,
    const struct yt_input_value *remote, struct yt_input_value *selected);
bool yt_input_ab36_queue_pop(char *queue, size_t capacity,
    size_t *position, size_t *length, struct yt_input_value *selected);
bool yt_input_ab36_repeat_requested(bool queued,
    const struct yt_input_value *selected);
bool yt_input_ab36_repeat_run(char *accumulator,
    size_t accumulator_capacity, const char *saved_command,
    size_t saved_capacity, float *newline_flag, uint8_t *selected_key,
    yt_ab36_repeat_emit_fn emit, void *context);
bool yt_input_ab36_submit_requested(uint8_t selected_key);
bool yt_input_ab36_submit_run(float *newline_flag,
    yt_ab36_submit_line_fn line, void *context);
bool yt_input_ab36_backspace_run(uint8_t selected_key, char *accumulator,
    size_t accumulator_capacity, bool *handled,
    yt_ab36_backspace_echo_fn echo, void *context);
bool yt_input_ab36_inactivity_expired(float timer, float deadline,
    float mode);
bool yt_input_ab36_session_expired(float timer, float deadline);
bool yt_input_carrier_returns(float mode, bool carrier_detected);
bool yt_input_ab36_terminal_run(enum yt_ab36_terminal_kind kind,
    bool *running, bool *terminated, yt_ab36_terminal_notice_fn notice,
    yt_ab36_terminal_close_fn close_all, void *context);
bool yt_b05d_process_key(const struct yt_input_value *value,
    struct yt_b05d_key_state *state);
bool yt_input_expand_repeat(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    struct yt_repeat_transform *result);
bool yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length);
bool yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer);
void yt_input_numeric_response(char *text);
bool yt_timed_wait_begin(struct yt_timed_wait_state *state, float duration,
    float initial_timer);
enum yt_timed_wait_reason yt_timed_wait_timer(
    struct yt_timed_wait_state *state, float current_timer);
enum yt_timed_wait_reason yt_timed_wait_input(
    struct yt_timed_wait_state *state, float mode,
    const struct yt_input_value *selected);
bool yt_input_drain_begin(struct yt_input_drain_state *state,
    const struct yt_input_value *initial_residue);
enum yt_input_drain_reason yt_input_drain_local(
    struct yt_input_drain_state *state,
    const struct yt_input_value *selected);
enum yt_input_drain_reason yt_input_drain_serial(
    struct yt_input_drain_state *state, float mode,
    const struct yt_input_value *selected);
bool yt_sysop_chat_begin(struct yt_sysop_chat_state *state, float mode,
    float snoop, float deadline, float entry_timer,
    float inactivity_deadline, const uint8_t *command_accumulator,
    size_t command_accumulator_length, const uint8_t *queue,
    size_t queue_length);
enum yt_sysop_chat_step_result yt_sysop_chat_step(
    struct yt_sysop_chat_state *state,
    const struct yt_sysop_chat_poll *poll,
    struct yt_sysop_chat_output *output);
bool yt_sysop_chat_finish(struct yt_sysop_chat_state *state,
    float deadline_timer, float inactivity_timer);
bool yt_sysop_f5_compose(bool same_f5_make,
    struct yt_sysop_f5_result *result);

#endif
