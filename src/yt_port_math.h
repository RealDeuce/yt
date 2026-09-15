#ifndef YT_PORT_MATH_H
#define YT_PORT_MATH_H

#include <stdint.h>

void yt_port_mbf64_promote_single(const uint8_t single[4], uint8_t raw[8]);
int yt_port_mbf64_compare(const uint8_t left[8], const uint8_t right[8]);
void yt_port_mbf64_negate(uint8_t raw[8]);
#endif
