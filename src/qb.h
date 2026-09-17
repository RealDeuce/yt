#ifndef YT_QB_H
#define YT_QB_H

#include "yt_common.h"

#include <fenv.h>

struct qb_val_result {
	double value;
	uint8_t mbf[8];
	size_t consumed;
	bool valid;
	bool overflow;
};

enum qb_mbf_status {
	QB_MBF_OK,
	QB_MBF_UNDERFLOW,
	QB_MBF_OVERFLOW,
	QB_MBF_DOMAIN
};

float qb_mbf32_decode(const uint8_t raw[4]);
enum qb_mbf_status qb_mbf32_encode(float value, uint8_t raw[4]);
double qb_mbf64_decode(const uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_encode(double value, uint8_t raw[8]);
enum qb_mbf_status qb_mbf32_from_mbf64_raw(const uint8_t source[8],
    uint8_t raw[4]);
enum qb_mbf_status qb_mbf64_from_u64(uint64_t value, uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_add_raw(const uint8_t left[8],
    const uint8_t right[8], uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_mul_raw(const uint8_t left[8],
    const uint8_t right[8], uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_div_raw(const uint8_t numerator[8],
    const uint8_t denominator[8], uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_sqrt_raw(const uint8_t operand[8],
    uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_floor_positive_raw(const uint8_t operand[8],
    uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_floor_raw(const uint8_t operand[8],
    uint8_t raw[8]);
enum qb_mbf_status qb_mbf64_int_positive_raw(const uint8_t operand[8],
    uint8_t raw[8]);

float qb_single_add(float left, float right);
float qb_single_subtract(float left, float right);
float qb_single_multiply(float left, float right);
float qb_single_divide(float left, float right);
double qb_double_add(double left, double right);
double qb_double_subtract(double left, double right);
double qb_double_multiply(double left, double right);
double qb_double_divide(double left, double right);

double qb_int(double value);
int32_t qb_cint(double value, bool *overflow);
int32_t qb_cint_mode(double value, uint8_t mode, bool *overflow);
struct qb_val_result qb_val(const char *text);
struct qb_val_result qb_val_n(const uint8_t *text, size_t length);

size_t qb_ltrim(char *text);
size_t qb_rtrim(char *text);
size_t qb_trim(char *text);
void qb_collapse_spaces(char *text);
size_t qb_ltrim_n(uint8_t *text, size_t length);
size_t qb_rtrim_n(uint8_t *text, size_t length);
size_t qb_trim_n(uint8_t *text, size_t length);
size_t qb_collapse_spaces_n(uint8_t *text, size_t length);
void qb_ascii_upper_n(uint8_t *text, size_t length);
void qb_compat_upper_n(uint8_t *text, size_t length);
size_t qb_title_case_n(uint8_t *text, size_t length);
void qb_ascii_upper(char *text);
void qb_compat_upper(char *text);
void qb_title_case(char *text);
int qb_ascii_casecmp(const char *left, const char *right);
bool qb_ascii_equal_nocase(const char *left, const char *right);

int qb_str_integer(char *dest, size_t size, int16_t value);
int qb_str_single(char *dest, size_t size, float value);
int qb_str_double(char *dest, size_t size, double value);
int qb_str_mbf32(char *dest, size_t size, const uint8_t raw[4]);
int qb_str_mbf64(char *dest, size_t size, const uint8_t raw[8]);
int qb_print_integer(char *dest, size_t size, int16_t value);
int qb_print_single(char *dest, size_t size, float value);
int qb_print_double(char *dest, size_t size, double value);

#endif
