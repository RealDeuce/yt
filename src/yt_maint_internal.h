#ifndef YT_MAINT_INTERNAL_H
#define YT_MAINT_INTERNAL_H

#include "yt_maint.h"

bool maintenance_copy_part(uint8_t *dest, size_t capacity, size_t *length,
    const uint8_t *data, size_t data_length);

#endif
