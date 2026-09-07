#ifndef YT_INIT_H
#define YT_INIT_H

#include "yt_game.h"
#include "yt_startup_model.h"

struct yt_name_file;

enum yt_init_output_entry {
	YT_INIT_OUTPUT_LINE,
	YT_INIT_OUTPUT_INLINE,
	YT_INIT_OUTPUT_COMMA,
	YT_INIT_OUTPUT_LOCATE_COLUMN_ONE,
	YT_INIT_OUTPUT_LOCATE_ROW_25,
	YT_INIT_OUTPUT_PLAY
};

typedef bool (*yt_init_present_write)(void *context, uint16_t site,
	enum yt_init_output_entry entry, const uint8_t *payload,
	size_t payload_length, struct yt_error *error);

struct yt_init_presenter {
	void *context;
	yt_init_present_write write;
};

struct yt_initializer_preparation {
	struct yt_config config;
	int today;
};

enum yt_initializer_family {
	YT_INITIALIZER_YT,
	YT_INITIALIZER_RMT
};

enum yt_rmt_output_entry {
	YT_RMT_OUTPUT_LINE,
	YT_RMT_OUTPUT_BLANK,
	YT_RMT_OUTPUT_INLINE,
	YT_RMT_OUTPUT_COMMA_SERIAL_FIRST,
	YT_RMT_OUTPUT_SERIAL_LINE
};

struct yt_rmt_output_state {
	size_t local_column;
	size_t serial_column;
};

struct yt_rmt_output_result {
	size_t local_length;
	size_t serial_length;
	bool serial_first;
};

enum yt_rmt_output_endpoint {
	YT_RMT_OUTPUT_ENDPOINT_LOCAL,
	YT_RMT_OUTPUT_ENDPOINT_SERIAL
};

enum yt_rmt_output_apply_outcome {
	YT_RMT_OUTPUT_APPLY_SUCCESS,
	YT_RMT_OUTPUT_APPLY_LOCAL_FAILURE,
	YT_RMT_OUTPUT_APPLY_SERIAL_FAILURE
};

/* A writer reports its exact accepted prefix and true only for completion. */
typedef bool (*yt_rmt_output_write)(void *context, const uint8_t *data,
    size_t length, size_t *accepted);

struct yt_rmt_output_sink {
	void *context;
	yt_rmt_output_write local;
	yt_rmt_output_write serial;
};

struct yt_rmt_output_attempt {
	enum yt_rmt_output_endpoint endpoint;
	size_t requested;
	size_t accepted;
};

struct yt_rmt_output_apply_result {
	enum yt_rmt_output_apply_outcome outcome;
	struct yt_rmt_output_attempt attempts[2];
	size_t attempt_count;
};

#define YT_RMT_COMPLETION_LINES 6U
#define YT_RMT_COMPLETION_PAYLOAD 160U

struct yt_rmt_completion_line {
	uint8_t bytes[YT_RMT_COMPLETION_PAYLOAD];
	size_t length;
};

struct yt_rmt_completion_result {
	struct yt_rmt_completion_line lines[YT_RMT_COMPLETION_LINES];
	size_t line_count;
	bool returns_to_bbs;
};

struct yt_rmt_delay_result {
	size_t admitted_values;
	float final_value;
};

typedef bool (*yt_rmt_present_write)(void *context, uint16_t site,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error);

struct yt_rmt_presenter {
	void *context;
	yt_rmt_present_write write;
};

struct yt_rmt_standalone_output {
	uint8_t bytes[192];
	size_t length;
	bool proceed;
};

struct yt_rmt_handoff_result {
	bool standalone;
	uint8_t path[512];
	size_t path_length;
};

enum yt_rmt_handoff_read_operation {
	YT_RMT_HANDOFF_READ_NONE,
	YT_RMT_HANDOFF_READ_OPEN_RANDOM,
	YT_RMT_HANDOFF_READ_LOF,
	YT_RMT_HANDOFF_READ_CLOSE_RANDOM,
	YT_RMT_HANDOFF_READ_OPEN_SEQUENTIAL,
	YT_RMT_HANDOFF_READ_LINE,
	YT_RMT_HANDOFF_READ_PARSE_LINE,
	YT_RMT_HANDOFF_READ_CLOSE_SEQUENTIAL,
};

struct yt_rmt_handoff_read_state {
	enum yt_rmt_handoff_read_operation failed_operation;
	uint32_t size;
	struct yt_rmt_handoff_result result;
	bool random_opened;
	bool lof_read;
	bool random_closed;
	bool sequential_opened;
	bool line_read;
	bool line_available;
	bool empty_line_substituted;
	bool line_parsed;
	bool sequential_closed;
	bool complete;
};

typedef bool (*yt_rmt_handoff_step_fn)(void *context,
	struct yt_error *error);
typedef bool (*yt_rmt_handoff_lof_fn)(void *context, uint32_t *size,
	struct yt_error *error);
typedef bool (*yt_rmt_handoff_line_fn)(void *context,
	const uint8_t **line, size_t *length, bool *available,
	struct yt_error *error);

struct yt_rmt_handoff_read_ops {
	yt_rmt_handoff_step_fn open_random;
	yt_rmt_handoff_lof_fn lof;
	yt_rmt_handoff_step_fn close_random;
	yt_rmt_handoff_step_fn open_sequential;
	yt_rmt_handoff_line_fn read_line;
	yt_rmt_handoff_step_fn close_sequential;
};

#define YT_RMT_DORINFO_FIELDS 8U

enum yt_rmt_dorinfo_outcome {
	YT_RMT_DORINFO_SUCCESS,
	YT_RMT_DORINFO_INPUT_PAST_END
};

struct yt_rmt_dorinfo_field {
	size_t offset;
	size_t length;
};

struct yt_rmt_dorinfo_result {
	enum yt_rmt_dorinfo_outcome outcome;
	struct yt_rmt_dorinfo_field fields[YT_RMT_DORINFO_FIELDS];
	size_t fields_assigned;
	size_t failed_field;
	size_t cursor;
	int error_number;
};

enum yt_rmt_serial_outcome {
	YT_RMT_SERIAL_LOCAL,
	YT_RMT_SERIAL_REMOTE,
	YT_RMT_SERIAL_ZERO_DIVISOR
};

struct yt_rmt_serial_state {
	enum yt_rmt_serial_outcome outcome;
	int requested_port;
	int brun_device;
	uint16_t uart_base;
	uint16_t modem_status_port;
	uint16_t bios_address;
	uint16_t bios_value;
	struct yt_startup_framing opening_framing;
	float detected_baud;
	uint8_t sampled_dll;
	uint8_t sampled_dlm;
	uint8_t restored_dll;
	uint8_t restored_dlm;
	uint8_t open_spec[64];
	size_t open_spec_length;
};

enum yt_rmt_serial_event_operation {
	YT_RMT_SERIAL_EVENT_DEF_SEG,
	YT_RMT_SERIAL_EVENT_POKE,
	YT_RMT_SERIAL_EVENT_IN,
	YT_RMT_SERIAL_EVENT_OUT,
	YT_RMT_SERIAL_EVENT_OPEN,
	YT_RMT_SERIAL_EVENT_RUNTIME_ERROR
};

enum yt_rmt_serial_event_outcome {
	YT_RMT_SERIAL_EVENTS_LOCAL,
	YT_RMT_SERIAL_EVENTS_ZERO_DIVISOR,
	YT_RMT_SERIAL_EVENTS_OPEN_ERROR,
	YT_RMT_SERIAL_EVENTS_REMOTE
};

#define YT_RMT_SERIAL_EVENTS 24U

struct yt_rmt_serial_event {
	enum yt_rmt_serial_event_operation operation;
	uint16_t address;
	uint16_t value;
	int error_number;
	bool complete;
};

struct yt_rmt_serial_event_result {
	enum yt_rmt_serial_event_outcome outcome;
	struct yt_rmt_serial_event events[YT_RMT_SERIAL_EVENTS];
	size_t event_count;
};

struct yt_initializer_options {
	enum yt_initializer_family family;
	const char *scoreboard;
	struct yt_config config;
	bool use_existing_config;
	bool database_already_truncated;
	const char *credited_name;
	const struct yt_rmt_presenter *rmt_presenter;
	const struct yt_init_presenter *yt_presenter;
	bool prepared_yt;
	struct yt_database *bound_database;
};

struct yt_init_binding {
	struct yt_config loaded;
	struct yt_record second_record;
	size_t first_accepted;
	size_t second_accepted;
};

bool yt_generate_port_name(struct yt_random *random, char name[42],
    struct yt_error *error);
bool yt_initializer_confirm_response(const char *response);
void yt_initializer_layout_yt(
	struct yt_initializer_preparation *preparation);
bool yt_init_present_confirmation_prefix(
	const struct yt_init_presenter *presenter, struct yt_error *error);
bool yt_init_present_opening(const struct yt_init_presenter *presenter,
	struct yt_error *error);
bool yt_initializer_prepare_yt(struct yt_random *random,
	struct yt_initializer_preparation *preparation,
	struct yt_error *error);
bool yt_init_present_prepared_configuration(
	const struct yt_initializer_preparation *preparation,
	const struct yt_init_presenter *presenter, struct yt_error *error);
bool yt_initializer_bounded(struct yt_random *random, int bound, int *value,
    struct yt_error *error);
bool yt_rmt_output_compose(enum yt_rmt_output_entry entry,
    const uint8_t *payload, size_t payload_length, bool local_mode,
    uint8_t *local, size_t local_capacity, uint8_t *serial,
    size_t serial_capacity, struct yt_rmt_output_result *result);
bool yt_rmt_output_compose_state(enum yt_rmt_output_entry entry,
    const uint8_t *payload, size_t payload_length, bool local_mode,
    const struct yt_rmt_output_state *state, uint8_t *local,
    size_t local_capacity, uint8_t *serial, size_t serial_capacity,
    struct yt_rmt_output_result *result,
    struct yt_rmt_output_state *final_state);
bool yt_rmt_output_apply(const uint8_t *local, const uint8_t *serial,
    const struct yt_rmt_output_result *output,
    const struct yt_rmt_output_sink *sink,
    struct yt_rmt_output_apply_result *result);
bool yt_rmt_completion_compose(bool local_mode, const char *credited_name,
    struct yt_rmt_completion_result *result);
bool yt_rmt_completion_delay(struct yt_rmt_delay_result *result);
bool yt_rmt_standalone_prompt_compose(
    struct yt_rmt_standalone_output *output);
bool yt_rmt_standalone_response_compose(const uint8_t *response,
    size_t response_length, struct yt_rmt_standalone_output *output);
bool yt_rmt_handoff_parse(const uint8_t *data, size_t length,
    struct yt_rmt_handoff_result *result);
bool yt_rmt_handoff_read_run(struct yt_rmt_handoff_read_state *state,
	const struct yt_rmt_handoff_read_ops *ops, void *context,
	struct yt_error *error);
bool yt_rmt_dorinfo_parse(const uint8_t *raw, size_t raw_length,
    uint8_t *storage, size_t storage_capacity,
    struct yt_rmt_dorinfo_result *result);
const uint8_t *yt_rmt_dorinfo_field(
    const struct yt_rmt_dorinfo_result *result, const uint8_t *storage,
    size_t field, size_t *length);
bool yt_rmt_serial_state_compose(const uint8_t *identifier,
    size_t identifier_length, const uint8_t *description,
    size_t description_length, uint8_t dll, uint8_t dlm,
    struct yt_rmt_serial_state *result);
bool yt_rmt_serial_events_compose(const struct yt_rmt_serial_state *state,
    uint8_t pre_open_lcr, uint8_t pre_open_ier, int serial_open_error,
    uint8_t post_open_lcr, uint8_t post_open_ier,
    struct yt_rmt_serial_event_result *result);
bool yt_rmt_remote_status_compose(bool serial_open, float com_port,
    float baud, struct yt_rmt_standalone_output *output);
bool yt_rmt_credited_name(const char *first, const char *last,
    const struct yt_name_file *names, char *credited, size_t credited_size);
void yt_rmt_normalize_config(struct yt_config *config, bool local_mode);
bool yt_rmt_preprocess_old_database(struct yt_database *database,
    struct yt_config *config, struct yt_error *error);
bool yt_init_sector_prepass(struct yt_database *database,
    float sector_offset, int sector_count, float *port_offset,
    struct yt_error *error);
bool yt_initialize_begin_yt(struct yt_error *error);
bool yt_initialize_bind_yt(struct yt_database *database,
    struct yt_init_binding *binding, struct yt_error *error);
bool yt_initialize_world(const struct yt_initializer_options *options,
    struct yt_random *random, struct yt_error *error);
bool yt_initialize_yt(const char *scoreboard, struct yt_random *random,
    struct yt_error *error);
bool yt_initialize_yt_prepared(
	const struct yt_initializer_preparation *preparation,
	const char *scoreboard, struct yt_random *random,
	const struct yt_init_presenter *presenter, struct yt_error *error);
/* Consumes and closes the successfully bound database. */
bool yt_initialize_yt_prepared_bound(struct yt_database *database,
	const struct yt_initializer_preparation *preparation,
	const char *scoreboard, struct yt_random *random,
	const struct yt_init_presenter *presenter, struct yt_error *error);
bool yt_initialize_rmt(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    struct yt_error *error);
bool yt_initialize_rmt_presented(const struct yt_config *config,
    const char *credited_name, struct yt_random *random,
    const struct yt_rmt_presenter *presenter, struct yt_error *error);

#endif
