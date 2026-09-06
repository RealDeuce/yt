#ifndef YT_INPUT_MODEL_H
#define YT_INPUT_MODEL_H

#include "yt_common.h"
#include "yt_main_error.h"

#define YT_INPUT_PENDING 4096U
#define YT_WAIT_SCRATCH_SIZE 80U
#define YT_SYSOP_CHAT_EVENTS 4U
#define YT_SYSOP_F5_PHASES 7U
#define YT_SYSOP_KEY_COUNT 5U
#define YT_SYSOP_KEY_FIFO_BYTES (YT_SYSOP_KEY_COUNT * 2U)
#define YT_SYSOP_KEY_PROCESS_SIZE 0x10000U
#define YT_SYSOP_EVENT_STACK_SIZE 0x10000U

enum yt_input_phase {
	YT_INPUT_PHASE_B05D,
	YT_INPUT_PHASE_AB36,
	YT_INPUT_PHASE_RADIO_BODY,
	YT_INPUT_PHASE_WAIT,
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

enum yt_repeat_failure {
	YT_REPEAT_FAILURE_NONE,
	YT_REPEAT_FAILURE_VAL_OVERFLOW,
	YT_REPEAT_FAILURE_SINGLE_OVERFLOW,
};

struct yt_repeat_transform {
	bool emit_notice;
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

enum yt_command_notice_kind {
	YT_COMMAND_NOTICE_SAVE,
	YT_COMMAND_NOTICE_REPEAT,
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

struct yt_sysop_chat_process_cells {
	uint8_t *mode;
	uint8_t *snoop;
	uint8_t *foreground;
	uint8_t *deadline;
	uint8_t *inactivity_deadline;
	uint8_t *saved_remaining;
	uint8_t *newline_flag;
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
	struct yt_sysop_chat_process_cells process;
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

enum yt_sysop_key {
	YT_SYSOP_KEY_F4 = 4,
	YT_SYSOP_KEY_F5 = 5,
	YT_SYSOP_KEY_F8 = 8,
	YT_SYSOP_KEY_F9 = 9,
	YT_SYSOP_KEY_F10 = 10,
};

struct yt_sysop_key_record {
	enum yt_sysop_key key;
	uint16_t address;
	uint16_t target;
	uint16_t target_segment;
	uint8_t keyboard_state;
	uint8_t event_state;
};

struct yt_sysop_key_scheduler {
	struct yt_sysop_key_record records[YT_SYSOP_KEY_COUNT];
	size_t fifo[YT_SYSOP_KEY_COUNT];
	size_t fifo_position;
	size_t fifo_length;
	size_t frames[YT_SYSOP_KEY_COUNT];
	size_t frame_depth;
	size_t abandoned_depth;
	uint8_t *process;
};

struct yt_sysop_key_delivery {
	bool delivered;
	enum yt_sysop_key key;
	uint16_t record_address;
	uint16_t target;
};

enum yt_sysop_event_stack_outcome {
	YT_SYSOP_EVENT_STACK_OK,
	YT_SYSOP_EVENT_STACK_ERROR_7,
	YT_SYSOP_EVENT_STACK_INVALID,
};

struct yt_sysop_event_stack {
	uint16_t sp;
	uint16_t bp;
	uint16_t cs;
	uint16_t ip;
	uint16_t ds;
	uint16_t ss;
	uint8_t *bytes;
	size_t size;
};

struct yt_sysop_event_registers {
	uint16_t ax;
	uint16_t bx;
	uint16_t cx;
	uint16_t dx;
	uint16_t si;
	uint16_t di;
	uint16_t es;
	uint16_t flags;
};

void yt_input_splitter_init(struct yt_input_splitter *splitter);
bool yt_input_splitter_can_push(const struct yt_input_splitter *splitter,
    bool remote);
bool yt_input_splitter_push(struct yt_input_splitter *splitter, bool remote,
    const struct yt_input_value *value);
struct yt_input_value yt_input_splitter_select(
    struct yt_input_splitter *splitter, float mode,
    enum yt_input_phase phase);
enum yt_radio_body_key_action yt_input_radio_body_key(uint8_t key,
    size_t current_length);
struct yt_input_value yt_input_splitter_select_merged(
    struct yt_input_splitter *splitter);
struct yt_input_value yt_input_splitter_select_source(
    struct yt_input_splitter *splitter, bool remote);
bool yt_input_ab36_remote_replace(float mode,
    const struct yt_input_value *remote, struct yt_input_value *selected);
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
bool yt_input_ab36_inactivity_begin_process(float timer,
    uint8_t deadline[4]);
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
bool yt_input_expand_repeat_observed(char *text, size_t text_capacity,
    char *saved_command, size_t saved_capacity,
    struct yt_repeat_transform *result, yt_input_process_store_fn store,
    void *context);
bool yt_input_split_semicolon(char *text, char *queue, size_t queue_capacity,
    size_t *queue_position, size_t *queue_length);
bool yt_input_split_semicolon_observed(char *text, char *queue,
    size_t queue_capacity, size_t *queue_position, size_t *queue_length,
    yt_input_process_store_fn store, void *context);
bool yt_input_yes_no_candidate(const char *command_accumulator,
    char *output_source, size_t output_source_capacity,
    enum yt_yes_no_answer *answer);
void yt_input_numeric_response(char *text);
bool yt_input_command_notice_wait(enum yt_command_notice_kind kind,
    uint16_t *address, uint8_t duration_raw[4]);
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
bool yt_sysop_chat_begin_process(struct yt_sysop_chat_state *state,
	const struct yt_sysop_chat_process_cells *process, float entry_timer,
	const uint8_t *command_accumulator, size_t command_accumulator_length,
	const uint8_t *queue, size_t queue_length);
void yt_sysop_chat_sync_process(struct yt_sysop_chat_state *state);
enum yt_sysop_chat_step_result yt_sysop_chat_step(
    struct yt_sysop_chat_state *state,
    const struct yt_sysop_chat_poll *poll,
    struct yt_sysop_chat_output *output);
bool yt_sysop_chat_finish(struct yt_sysop_chat_state *state,
    float deadline_timer, float inactivity_timer);
bool yt_sysop_f5_compose(bool same_f5_make,
    struct yt_sysop_f5_result *result);
void yt_sysop_key_scheduler_init(struct yt_sysop_key_scheduler *scheduler);
bool yt_sysop_key_scheduler_bind_process(
	struct yt_sysop_key_scheduler *scheduler, uint8_t *process,
	size_t process_size, uint16_t target_segment);
bool yt_sysop_key_latch(struct yt_sysop_key_scheduler *scheduler,
	enum yt_sysop_key key);
bool yt_sysop_key_checkpoint(struct yt_sysop_key_scheduler *scheduler,
	bool error_active, struct yt_sysop_key_delivery *delivery);
bool yt_sysop_key_return(struct yt_sysop_key_scheduler *scheduler,
	enum yt_sysop_key *returned);
bool yt_sysop_key_resume_abandon(struct yt_sysop_key_scheduler *scheduler);
bool yt_sysop_event_stack_init(struct yt_sysop_key_scheduler *scheduler,
	struct yt_sysop_event_stack *stack, uint16_t floor);
enum yt_sysop_event_stack_outcome yt_sysop_event_deliver_raw(
	struct yt_sysop_key_scheduler *scheduler,
	const struct yt_sysop_key_delivery *delivery,
	struct yt_sysop_event_stack *stack, uint16_t brun_segment,
	struct yt_sysop_event_registers *registers);
bool yt_sysop_event_checkpoint_quiet_raw(
	struct yt_sysop_key_scheduler *scheduler,
	struct yt_sysop_event_stack *stack,
	struct yt_sysop_event_registers *registers);
bool yt_sysop_event_return_raw(struct yt_sysop_key_scheduler *scheduler,
	struct yt_sysop_event_stack *stack, uint16_t opcode_flags,
	enum yt_sysop_key *returned);
const struct yt_sysop_key_record *yt_sysop_key_record(
	const struct yt_sysop_key_scheduler *scheduler, enum yt_sysop_key key);
size_t yt_sysop_key_fifo_bytes(const struct yt_sysop_key_scheduler *scheduler,
	uint8_t *bytes, size_t capacity);

#endif
