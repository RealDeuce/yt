#ifndef YT_STARTUP_MODEL_H
#define YT_STARTUP_MODEL_H

#include "yt_common.h"

#define YT_STARTUP_COMMAND_SIZE 1024U
#define YT_STARTUP_OPEN_SPEC_SIZE 64U
#define YT_STARTUP_DORINFO_FIELDS 12U
struct yt_startup_command_split {
	uint8_t path[YT_STARTUP_COMMAND_SIZE];
	size_t path_length;
	uint8_t remainder[YT_STARTUP_COMMAND_SIZE];
	size_t remainder_length;
};

enum yt_startup_entry_outcome {
	YT_STARTUP_ENTRY_CONTINUE,
	YT_STARTUP_ENTRY_MISSING_COMMAND_END,
};

struct yt_startup_entry_result {
	enum yt_startup_entry_outcome outcome;
	struct yt_startup_command_split command;
	uint16_t installed_handler;
	bool handler_installed;
	bool process_end;
	int exit_status;
};

struct yt_startup_serial_layout {
	int port;
	int brun_device;
	uint16_t uart_base;
	uint16_t modem_status_port;
	uint16_t bios_address;
	uint16_t bios_value;
};

enum yt_startup_parity {
	YT_STARTUP_PARITY_NONE,
	YT_STARTUP_PARITY_EVEN
};

struct yt_startup_framing {
	uint32_t opening_baud;
	enum yt_startup_parity parity;
	uint8_t data_bits;
	uint8_t stop_bits;
};

enum yt_startup_dorinfo_outcome {
	YT_STARTUP_DORINFO_SUCCESS,
	YT_STARTUP_DORINFO_INPUT_PAST_END,
};

struct yt_startup_dorinfo_field {
	size_t offset;
	size_t length;
};

struct yt_startup_dorinfo_result {
	enum yt_startup_dorinfo_outcome outcome;
	struct yt_startup_dorinfo_field fields[YT_STARTUP_DORINFO_FIELDS];
	size_t fields_assigned;
	size_t failed_field;
	size_t cursor;
	int error_number;
};

enum yt_startup_state_outcome {
	YT_STARTUP_STATE_LOCAL_READY,
	YT_STARTUP_STATE_REMOTE_READY,
	YT_STARTUP_STATE_ZERO_DIVISOR,
};

struct yt_startup_state_result {
	enum yt_startup_state_outcome outcome;
	int requested_port;
	float detected_baud;
	uint8_t open_spec[YT_STARTUP_OPEN_SPEC_SIZE];
	size_t open_spec_length;
	size_t canonical_name_length;
	float ansi_flag;
	float deadline;
	bool deadline_set;
	float local_mode;
	float carrier_local_screen;
	float local_sound;
	float game_sound;
	uint8_t sampled_dll;
	uint8_t sampled_dlm;
	uint8_t restored_dll;
	uint8_t restored_dlm;
	bool cleared_fields[YT_STARTUP_DORINFO_FIELDS];
};

enum yt_startup_wait_exit {
	YT_STARTUP_WAIT_TIMER,
	YT_STARTUP_WAIT_LOCAL_KEY,
	YT_STARTUP_WAIT_SERIAL_KEY,
};

enum yt_startup_event_operation {
	YT_STARTUP_EVENT_DEF_SEG,
	YT_STARTUP_EVENT_POKE,
	YT_STARTUP_EVENT_IN,
	YT_STARTUP_EVENT_OUT,
	YT_STARTUP_EVENT_UPPERCASE,
	YT_STARTUP_EVENT_OPEN,
	YT_STARTUP_EVENT_SET_LOCAL_SOUND,
	YT_STARTUP_EVENT_WAIT,
	YT_STARTUP_EVENT_CARRIER_CHECK,
	YT_STARTUP_EVENT_CARRIER_NOTICE,
	YT_STARTUP_EVENT_CLOSE,
	YT_STARTUP_EVENT_PROCESS_END,
	YT_STARTUP_EVENT_RUNTIME_ERROR,
};

enum yt_startup_event_outcome {
	YT_STARTUP_EVENTS_LOCAL_READY,
	YT_STARTUP_EVENTS_ZERO_DIVISOR,
	YT_STARTUP_EVENTS_OPEN_ERROR,
	YT_STARTUP_EVENTS_CARRIER_DROP,
	YT_STARTUP_EVENTS_REMOTE_READY,
};

#define YT_STARTUP_EVENTS 32U

struct yt_startup_event {
	enum yt_startup_event_operation operation;
	uint16_t address;
	uint16_t value;
	int error_number;
	bool complete;
};

struct yt_startup_event_result {
	enum yt_startup_event_outcome outcome;
	struct yt_startup_event events[YT_STARTUP_EVENTS];
	size_t event_count;
	enum yt_startup_wait_exit wait_exit;
	bool carrier_checked;
	bool carrier_detected;
};

bool yt_startup_split_command(const uint8_t *command, size_t length,
    struct yt_startup_command_split *result);
bool yt_startup_compose_entry(const uint8_t *command, size_t length,
    struct yt_startup_entry_result *result);
int yt_startup_parse_port(const uint8_t *identifier, size_t length);
bool yt_startup_serial_layout(int port,
    struct yt_startup_serial_layout *layout);
bool yt_startup_detect_baud(uint8_t dll, uint8_t dlm, float *baud);
bool yt_startup_divisor_from_observed_baud(uint32_t baud, uint8_t *dll,
    uint8_t *dlm);
bool yt_startup_framing_compose(const uint8_t *description,
    size_t description_length, struct yt_startup_framing *framing);
bool yt_startup_open_spec(int port, const uint8_t *description,
    size_t description_length, uint8_t *spec, size_t capacity,
    size_t *spec_length);
bool yt_startup_restored_divisor(float baud, uint8_t *dll, uint8_t *dlm);
float yt_startup_session_deadline(float timer, double minutes,
    float cap_timer);
bool yt_startup_local_mode_raw(bool local, uint8_t raw[4]);
bool yt_startup_ansi_raw(const uint8_t *text, size_t length, uint8_t raw[4]);
bool yt_startup_canonical_name(const uint8_t *first, size_t first_length,
    const uint8_t *last, size_t last_length, uint8_t *name,
    size_t capacity, size_t *name_length);
bool yt_startup_parse_dorinfo(const uint8_t *raw, size_t raw_length,
    uint8_t *storage, size_t storage_capacity,
    struct yt_startup_dorinfo_result *result);
const uint8_t *yt_startup_dorinfo_field(
    const struct yt_startup_dorinfo_result *result,
    const uint8_t *storage, size_t field, size_t *length);
bool yt_startup_compose_state(
    const struct yt_startup_dorinfo_result *dorinfo,
    uint8_t *storage, uint8_t dll, uint8_t dlm, float timer,
    float cap_timer, uint8_t *canonical_name, size_t canonical_capacity,
    struct yt_startup_state_result *result);
bool yt_startup_compose_events(const struct yt_startup_state_result *state,
    uint8_t pre_open_lcr, uint8_t pre_open_ier, int serial_open_error,
    enum yt_startup_wait_exit wait_exit, uint8_t modem_status,
    uint8_t post_open_lcr, uint8_t post_open_ier,
    struct yt_startup_event_result *result);
#endif
