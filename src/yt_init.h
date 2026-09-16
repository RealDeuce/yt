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
	size_t column;
};

struct yt_rmt_output_result {
	size_t length;
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
bool yt_rmt_output_compose_state(enum yt_rmt_output_entry entry,
    const uint8_t *payload, size_t payload_length, bool local_mode,
    const struct yt_rmt_output_state *state, uint8_t *dest,
    size_t capacity,
    struct yt_rmt_output_result *result,
    struct yt_rmt_output_state *final_state);
bool yt_rmt_completion_compose(bool local_mode, const char *credited_name,
    struct yt_rmt_completion_result *result);
void yt_rmt_completion_delay(void);
bool yt_rmt_standalone_prompt_compose(
    struct yt_rmt_standalone_output *output);
bool yt_rmt_standalone_response_compose(const uint8_t *response,
    size_t response_length, struct yt_rmt_standalone_output *output);
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
