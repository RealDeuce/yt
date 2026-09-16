#ifndef YT_INIT_INTERNAL_H
#define YT_INIT_INTERNAL_H

#include "yt_init.h"

bool yt_present(const struct yt_initializer_options *options, uint16_t site,
    enum yt_init_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error);
bool yt_present_text(const struct yt_initializer_options *options,
    uint16_t site, enum yt_init_output_entry entry, const char *text,
    struct yt_error *error);
bool yt_present_number(const struct yt_initializer_options *options,
    uint16_t site, float value, enum yt_init_output_entry entry,
    struct yt_error *error);
bool yt_present_str_number_line(const struct yt_initializer_options *options,
    uint16_t site, const char *label, float value, struct yt_error *error);
bool rmt_present(const struct yt_initializer_options *options, uint16_t site,
    enum yt_rmt_output_entry entry, const uint8_t *payload,
    size_t payload_length, struct yt_error *error);
bool rmt_present_text(const struct yt_initializer_options *options,
    uint16_t site, enum yt_rmt_output_entry entry, const char *text,
    struct yt_error *error);
bool rmt_present_number_line(const struct yt_initializer_options *options,
    uint16_t site, const char *label, float value, struct yt_error *error);

#endif
