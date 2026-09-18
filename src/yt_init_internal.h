#ifndef YT_INIT_INTERNAL_H
#define YT_INIT_INTERNAL_H

#include "yt_init.h"

struct yt_init_world {
	int sectors;
	int ports;
	int (*warps)[6];
	int *port_sectors;
	int *sector_ports;
};

bool yt_present(const struct yt_initializer_options *options,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error);
bool yt_present_text(const struct yt_initializer_options *options,
    enum yt_init_output_entry entry, const char *text,
    struct yt_error *error);
bool yt_present_number(const struct yt_initializer_options *options,
	uint16_t value, enum yt_init_output_entry entry,
	struct yt_error *error);
bool yt_present_str_number_line(const struct yt_initializer_options *options,
	const char *label, uint16_t value, struct yt_error *error);
bool rmt_present(const struct yt_initializer_options *options,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error);
bool rmt_present_text(const struct yt_initializer_options *options,
    enum yt_rmt_output_entry entry, const char *text,
    struct yt_error *error);
bool rmt_present_number_line(const struct yt_initializer_options *options,
    const char *label, float value, struct yt_error *error);
bool yt_init_write_yt_auxiliary(struct yt_database *database,
    const struct yt_initializer_options *options, struct yt_error *error);
bool yt_init_write_rmt_auxiliary(struct yt_database *database,
    const char *credited_name, const struct yt_initializer_options *options,
    struct yt_error *error);
bool yt_init_write_sequential_file(const char *path, const uint8_t *data,
    size_t length, struct yt_error *error);
bool yt_init_world_allocate(struct yt_init_world *world,
    struct yt_error *error);
void yt_init_world_free(struct yt_init_world *world);
bool yt_init_world_build_graph(struct yt_init_world *world,
    enum yt_initializer_family family, struct yt_random *random,
    const struct yt_initializer_options *options, struct yt_error *error);
bool yt_init_world_assign_ports(struct yt_init_world *world,
    struct yt_random *random, struct yt_error *error);

#endif
