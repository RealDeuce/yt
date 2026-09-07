#include "yt_framebuffer.h"
#include "yt_brun_fatal.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

struct sha256 {
	uint32_t state[8];
	uint64_t bits;
	uint8_t block[64];
	size_t used;
};

static uint32_t
rotate_right(uint32_t value, unsigned count)
{
	return (value >> count) | (value << (32U - count));
}

static void
sha_transform(struct sha256 *hash, const uint8_t block[64])
{
	static const uint32_t k[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
	};
	uint32_t words[64];
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t e;
	uint32_t f;
	uint32_t g;
	uint32_t h;
	size_t index;

	for (index = 0U; index < 16U; ++index) {
		words[index] = ((uint32_t)block[index * 4U] << 24)
		    | ((uint32_t)block[index * 4U + 1U] << 16)
		    | ((uint32_t)block[index * 4U + 2U] << 8)
		    | block[index * 4U + 3U];
	}
	for (; index < 64U; ++index) {
		uint32_t x = words[index - 15U];
		uint32_t y = words[index - 2U];
		uint32_t s0 = rotate_right(x, 7U) ^ rotate_right(x, 18U)
		    ^ (x >> 3);
		uint32_t s1 = rotate_right(y, 17U) ^ rotate_right(y, 19U)
		    ^ (y >> 10);

		words[index] = words[index - 16U] + s0
		    + words[index - 7U] + s1;
	}
	a = hash->state[0];
	b = hash->state[1];
	c = hash->state[2];
	d = hash->state[3];
	e = hash->state[4];
	f = hash->state[5];
	g = hash->state[6];
	h = hash->state[7];
	for (index = 0U; index < 64U; ++index) {
		uint32_t s1 = rotate_right(e, 6U) ^ rotate_right(e, 11U)
		    ^ rotate_right(e, 25U);
		uint32_t choice = (e & f) ^ (~e & g);
		uint32_t temporary1 = h + s1 + choice + k[index]
		    + words[index];
		uint32_t s0 = rotate_right(a, 2U) ^ rotate_right(a, 13U)
		    ^ rotate_right(a, 22U);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temporary2 = s0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temporary1;
		d = c;
		c = b;
		b = a;
		a = temporary1 + temporary2;
	}
	hash->state[0] += a;
	hash->state[1] += b;
	hash->state[2] += c;
	hash->state[3] += d;
	hash->state[4] += e;
	hash->state[5] += f;
	hash->state[6] += g;
	hash->state[7] += h;
}

static void
sha_init(struct sha256 *hash)
{
	static const uint32_t initial[8] = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
	};

	memset(hash, 0, sizeof(*hash));
	memcpy(hash->state, initial, sizeof(initial));
}

static void
sha_update(struct sha256 *hash, const uint8_t *data, size_t length)
{
	size_t amount;

	hash->bits += (uint64_t)length * 8U;
	while (length != 0U) {
		amount = sizeof(hash->block) - hash->used;
		if (amount > length)
			amount = length;
		memcpy(hash->block + hash->used, data, amount);
		hash->used += amount;
		data += amount;
		length -= amount;
		if (hash->used == sizeof(hash->block)) {
			sha_transform(hash, hash->block);
			hash->used = 0U;
		}
	}
}

static void
sha_final(struct sha256 *hash, uint8_t output[32])
{
	size_t index;

	hash->block[hash->used++] = 0x80U;
	if (hash->used > 56U) {
		memset(hash->block + hash->used, 0,
		    sizeof(hash->block) - hash->used);
		sha_transform(hash, hash->block);
		hash->used = 0U;
	}
	memset(hash->block + hash->used, 0, 56U - hash->used);
	for (index = 0U; index < 8U; ++index)
		hash->block[63U - index] =
		    (uint8_t)(hash->bits >> (index * 8U));
	sha_transform(hash, hash->block);
	for (index = 0U; index < 8U; ++index) {
		output[index * 4U] = (uint8_t)(hash->state[index] >> 24);
		output[index * 4U + 1U] =
		    (uint8_t)(hash->state[index] >> 16);
		output[index * 4U + 2U] =
		    (uint8_t)(hash->state[index] >> 8);
		output[index * 4U + 3U] = (uint8_t)hash->state[index];
	}
}

static void
digest_hex(const uint8_t *data, size_t length, char output[65])
{
	static const char digits[] = "0123456789abcdef";
	struct sha256 hash;
	uint8_t digest[32];
	size_t index;

	sha_init(&hash);
	sha_update(&hash, data, length);
	sha_final(&hash, digest);
	for (index = 0U; index < sizeof(digest); ++index) {
		output[index * 2U] = digits[digest[index] >> 4];
		output[index * 2U + 1U] = digits[digest[index] & 0x0fU];
	}
	output[64] = '\0';
}

static struct yt_framebuffer_event
simple_event(enum yt_framebuffer_operation operation)
{
	struct yt_framebuffer_event event;

	memset(&event, 0, sizeof(event));
	event.operation = operation;
	return event;
}

static struct yt_framebuffer_event
data_event(enum yt_framebuffer_operation operation, const void *data,
    size_t length)
{
	struct yt_framebuffer_event event = simple_event(operation);

	event.data = data;
	event.length = length;
	return event;
}

static struct yt_framebuffer_event
color_event(uint8_t foreground, uint8_t background)
{
	struct yt_framebuffer_event event = simple_event(YT_FRAMEBUFFER_COLOR);

	event.has_foreground = true;
	event.foreground = foreground;
	event.has_background = true;
	event.background = background;
	return event;
}

static struct yt_framebuffer_event
locate_event(uint16_t row, uint16_t column)
{
	struct yt_framebuffer_event event = simple_event(YT_FRAMEBUFFER_LOCATE);

	event.has_row = true;
	event.row = row;
	event.has_column = true;
	event.column = column;
	return event;
}

static void
test_initial_serialization(void)
{
	struct yt_framebuffer_state state;
	struct yt_framebuffer_state decoded;
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	size_t written = 0U;
	char digest[65];

	yt_framebuffer_init(&state, true, 0x0607U);
	state.function_bar = true;
	CHECK(yt_framebuffer_validate(&state));
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), &written)
	    == YT_FRAMEBUFFER_OK);
	CHECK(written == sizeof(raw));
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "410c272a48fc8cb97094f9581c2632955771a253b8fffe807249c5e00e1b94ed")
	    == 0);
	memset(&decoded, 0xa5, sizeof(decoded));
	CHECK(yt_framebuffer_deserialize(&decoded, raw, sizeof(raw))
	    == YT_FRAMEBUFFER_OK);
	CHECK(memcmp(&decoded, &state, sizeof(state)) == 0);
	raw[4044U + 15U] = 2U;
	CHECK(yt_framebuffer_deserialize(&decoded, raw, sizeof(raw))
	    == YT_FRAMEBUFFER_INVALID_ARGUMENT);
}

static void
test_witness_and_prefix_law(void)
{
	struct yt_framebuffer_state joined;
	struct yt_framebuffer_state sliced;
	struct yt_framebuffer_state startup;
	struct yt_framebuffer_event events[6];
	uint8_t function_row[YT_FRAMEBUFFER_WIDTH];
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	uint8_t trace[6U * YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];
	size_t index;

	memset(function_row, 'F', sizeof(function_row));
	events[0] = color_event(14U, 3U);
	events[1] = locate_event(25U, 1U);
	events[2] = data_event(YT_FRAMEBUFFER_FUNCTION_BAR_SET,
	    function_row, sizeof(function_row));
	events[2].has_function_bar = true;
	events[2].function_bar = true;
	events[3] = simple_event(YT_FRAMEBUFFER_CLS);
	events[4] = data_event(YT_FRAMEBUFFER_PRINT_NL,
	    "canonical", 9U);
	events[5] = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[2;3H\x1b[31mX", sizeof("\x1b[2;3H\x1b[31mX") - 1U);

	yt_framebuffer_init(&joined, true, 0x0607U);
	for (index = 0U; index < 6U; ++index) {
		CHECK(yt_framebuffer_apply(&joined, &events[index])
		    == YT_FRAMEBUFFER_OK);
		CHECK(yt_framebuffer_serialize(&joined,
		    trace + index * YT_FRAMEBUFFER_STATE_BYTES,
		    YT_FRAMEBUFFER_STATE_BYTES, NULL) == YT_FRAMEBUFFER_OK);
	}
	CHECK(yt_framebuffer_serialize(&joined, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "475451f2a844fc7d346eed3956803e1aa107a0c32dd85140f679cca2490dd8c5")
	    == 0);
	digest_hex(trace, sizeof(trace), digest);
	CHECK(strcmp(digest,
	    "967a6c0db76c3acbe4b7a37290534e4893864e0e258cb7405726444d34f603b2")
	    == 0);

	yt_framebuffer_init(&sliced, true, 0x0607U);
	CHECK(yt_framebuffer_apply_all(&sliced, events, 3U)
	    == YT_FRAMEBUFFER_OK);
	CHECK(yt_framebuffer_apply_all(&sliced, events + 3U, 3U)
	    == YT_FRAMEBUFFER_OK);
	CHECK(memcmp(&sliced, &joined, sizeof(joined)) == 0);

	startup = joined;
	events[0] = simple_event(YT_FRAMEBUFFER_PROCESS_STARTUP);
	CHECK(yt_framebuffer_apply(&startup, &events[0]) == YT_FRAMEBUFFER_OK);
	CHECK(yt_framebuffer_serialize(&startup, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "3f3f9c97b08e23875bfcc29106ef449e4bb0ffb4c5355deca62c1f54799505f9")
	    == 0);
	CHECK(!startup.function_bar
	    && startup.process_entry_cursor_shape_present
	    && startup.process_entry_cursor_shape == 0x0607U
	    && startup.ansi_attribute == joined.ansi_attribute
	    && startup.ansi_escape_pending == joined.ansi_escape_pending
	    && startup.ansi_csi_pending == joined.ansi_csi_pending);
}

static void
test_qb_edges_and_function_bar(void)
{
	struct yt_framebuffer_state state;
	struct yt_framebuffer_state before;
	struct yt_framebuffer_event event;
	uint8_t row[YT_FRAMEBUFFER_WIDTH];
	uint8_t exact[YT_FRAMEBUFFER_WIDTH];
	size_t index;

	memset(row, 'F', sizeof(row));
	memset(exact, 'X', sizeof(exact));
	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_FUNCTION_BAR_SET, row, sizeof(row));
	event.has_function_bar = true;
	event.function_bar = true;
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(yt_framebuffer_apply(&state, &(struct yt_framebuffer_event){
	    .operation = YT_FRAMEBUFFER_COLOR,
	    .has_foreground = true, .foreground = 14U,
	    .has_background = true, .background = 1U
	}) == YT_FRAMEBUFFER_OK);
	event = simple_event(YT_FRAMEBUFFER_CLS);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	for (index = 0U; index < YT_FRAMEBUFFER_WIDTH; ++index) {
		unsigned number = (unsigned)(index / 8U) + 1U;
		size_t offset = index % 8U;
		uint8_t expected = ' ';

		if (offset == 0U && number == 10U)
			expected = '1';
		else if (offset == 1U)
			expected = number == 10U ? '0'
			    : (uint8_t)('0' + number);
		CHECK(state.characters[1920U + index] == expected);
		CHECK(state.attributes[1920U + index] == 0x1eU);
	}

	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, exact, sizeof(exact));
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_column == 81U && state.bios_column == 81U);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "Z", 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 2U && state.qb_column == 2U
	    && state.characters[80] == 'Z');

	before = state;
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "\x14", 1U);
	CHECK(yt_framebuffer_apply(&state, &event)
	    == YT_FRAMEBUFFER_UNSUPPORTED_CONTROL);
	CHECK(memcmp(&state, &before, sizeof(state)) == 0);
}

static void
test_con_parser_and_observations(void)
{
	struct yt_framebuffer_state split;
	struct yt_framebuffer_state joined;
	struct yt_framebuffer_state before;
	struct yt_framebuffer_event event;
	const uint8_t requested[] = "\x1b[31mFATAL";
	size_t length;
	unsigned error;

	yt_framebuffer_init(&split, false, 0U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM, "\x1b[12;", 5U);
	CHECK(yt_framebuffer_apply(&split, &event) == YT_FRAMEBUFFER_OK);
	CHECK(split.ansi_escape_pending && split.ansi_csi_pending
	    && split.ansi_arguments[0] == 12U
	    && split.ansi_argument_index == 1U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "34H\x1b[31mX", 10U);
	CHECK(yt_framebuffer_apply(&split, &event) == YT_FRAMEBUFFER_OK);
	yt_framebuffer_init(&joined, false, 0U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[12;34H\x1b[31mX", 15U);
	CHECK(yt_framebuffer_apply(&joined, &event) == YT_FRAMEBUFFER_OK);
	CHECK(memcmp(&split, &joined, sizeof(joined)) == 0);
	CHECK(joined.characters[(12U - 1U) * 80U + 33U] == 'X');

	for (error = 1U; error <= 255U; ++error) {
		for (length = 0U; length <= sizeof(requested) - 1U; ++length) {
			yt_framebuffer_init(&joined, false, 0U);
			CHECK(yt_framebuffer_apply_con_observation(&joined,
			    requested, sizeof(requested) - 1U, requested,
			    length, false, (uint8_t)error)
			    == YT_FRAMEBUFFER_OK);
		}
	}
	before = joined;
	CHECK(yt_framebuffer_apply_con_observation(&joined,
	    requested, sizeof(requested) - 1U, requested, 1U, true, 0U)
	    == YT_FRAMEBUFFER_INVALID_ARGUMENT);
	CHECK(memcmp(&joined, &before, sizeof(joined)) == 0);

	yt_framebuffer_init(&joined, false, 0U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[1;2;3;4;5;6;7;8;9;10;", 24U);
	before = joined;
	CHECK(yt_framebuffer_apply(&joined, &event)
	    == YT_FRAMEBUFFER_ANSI_OVERFLOW);
	CHECK(memcmp(&joined, &before, sizeof(joined)) == 0);
}

static void
test_presentation_projection(void)
{
	struct yt_framebuffer_state state;
	struct yt_present_result result;
	struct yt_framebuffer_state before;

	yt_framebuffer_init(&state, false, 0U);
	memset(&result, 0, sizeof(result));
	result.event_count = 5U;
	result.events[0].operation = YT_PRESENT_REMOTE_LINE;
	result.events[1].operation = YT_PRESENT_LOCAL_COLOR;
	result.events[1].foreground = 14;
	result.events[1].background = 3;
	result.events[2].operation = YT_PRESENT_LOCAL_LOCATE;
	result.events[2].row = 12;
	result.events[2].column = 34;
	result.events[2].cursor_visible = 1;
	result.events[2].cursor_start = 1;
	result.events[2].cursor_stop = 16;
	result.events[3].operation = YT_PRESENT_LOCAL_SEMI;
	memcpy(result.events[3].data, "status", 6U);
	result.events[3].length = 6U;
	result.events[4].operation = YT_PRESENT_LOCAL_BEEP;
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 12U && state.qb_column == 40U
	    && state.cursor_shape == 0x0110U && state.cursor_visible
	    && state.qb_attribute == 0x3eU
	    && memcmp(state.characters + 913U, "status", 6U) == 0);

	before = state;
	memset(&result, 0, sizeof(result));
	result.event_count = 1U;
	result.events[0].operation = YT_PRESENT_LOCAL_LOCATE;
	result.events[0].row = -1;
	result.events[0].column = -1;
	result.events[0].cursor_visible = 0;
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(!state.cursor_visible && state.cursor_shape == 0x2000U);
	CHECK(memcmp(state.characters, before.characters,
	    sizeof(state.characters)) == 0);
}

static void
test_documented_cursor_and_scroll_edges(void)
{
	struct yt_framebuffer_state state;
	struct yt_framebuffer_event event;
	uint8_t full[YT_FRAMEBUFFER_WIDTH];
	size_t index;

	yt_framebuffer_init(&state, false, 0U);
	event = color_event(3U, 0U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = locate_event(25U, 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "status", 6U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_NL, NULL, 0U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 24U && state.qb_column == 1U
	    && state.bios_row == 24U && state.bios_column == 1U
	    && state.qb_scrolls == 1U);
	CHECK(memcmp(state.characters + 1920U, "status", 6U) == 0);
	for (index = 1840U; index < 1920U; ++index)
		CHECK(state.attributes[index] == 0x03U);

	yt_framebuffer_init(&state, false, 0U);
	memset(full, 'X', sizeof(full));
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, full, 76U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, full, 4U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_column == 81U && state.bios_column == 80U
	    && state.brun_bios_cache_column == 80U);

	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "AB\x08", 3U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_column == 2U && state.bios_column == 2U
	    && state.characters[1] == ' ');
	event = locate_event(2U, 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW,
	    "\x1d\x1c\x1f\x1e\x0b", 5U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 1U && state.qb_column == 1U
	    && state.bios_row == 2U && state.bios_column == 1U);
	event = color_event(3U, 0U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "dirty\x0c", 6U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 1U && state.qb_column == 1U
	    && state.bios_row == 1U && state.bios_column == 6U);
	for (index = 0U; index < YT_FRAMEBUFFER_CELLS; ++index) {
		CHECK(state.characters[index] == ' ');
		CHECK(state.attributes[index] == 0x03U);
	}
}

static void
test_documented_con_commands_and_stale_cache(void)
{
	struct yt_framebuffer_state state;
	struct yt_framebuffer_event event;
	size_t index;

	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_PRINT_NL, "QB", 2U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[2J\x1b[5;6H\x1b[31;44mRQ\x1b[s",
	    sizeof("\x1b[2J\x1b[5;6H\x1b[31;44mRQ\x1b[s") - 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	index = 4U * 80U + 5U;
	CHECK(state.bios_row == 5U && state.bios_column == 8U
	    && state.qb_row == 2U && state.qb_column == 1U
	    && state.ansi_saved_row == 5U && state.ansi_saved_column == 8U
	    && state.characters[index] == 'R'
	    && state.attributes[index] == 0x14U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[1;1H\x1b[u\x1b[D\x1b[0mZ",
	    sizeof("\x1b[1;1H\x1b[u\x1b[D\x1b[0mZ") - 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.bios_row == 5U && state.bios_column == 8U
	    && !state.ansi_enabled && state.attributes[index + 1U] == 0x14U);

	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "AB", 2U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[10;10H", 8U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_PRINT_RAW, "C", 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.characters[9U * 80U + 9U] == 'C'
	    && state.characters[2] == ' '
	    && state.bios_row == 1U && state.bios_column == 4U
	    && state.brun_bios_cache_row == 1U
	    && state.brun_bios_cache_column == 4U);

	yt_framebuffer_init(&state, false, 0U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM,
	    "\x1b[25;80H\x1b[41mX",
	    sizeof("\x1b[25;80H\x1b[41mX") - 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.bios_row == 25U && state.bios_column == 1U);
	for (index = 1920U; index < 2000U; ++index)
		CHECK(state.attributes[index] == 0x07U);
	event = data_event(YT_FRAMEBUFFER_CON_DEVICE_STREAM, "A\nB", 3U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(state.characters[1920U] == 'B'
	    && state.bios_row == 25U && state.bios_column == 2U);
}

static void
test_end_cleanup(void)
{
	struct yt_framebuffer_state state;
	struct yt_framebuffer_event event;
	size_t index;

	yt_framebuffer_init(&state, true, 0x0607U);
	event = color_event(14U, 3U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = locate_event(24U, 7U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	memset(state.characters + 1920U, 'F', 80U);
	event = simple_event(YT_FRAMEBUFFER_END_CLEANUP);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	for (index = 1920U; index < 2000U; ++index) {
		CHECK(state.characters[index] == ' ');
		CHECK(state.attributes[index] == 0x3eU);
	}
	CHECK(state.bios_row == 25U && state.bios_column == 1U
	    && state.brun_bios_cache_row == 25U
	    && state.brun_bios_cache_column == 1U
	    && state.qb_row == 24U && state.qb_column == 7U
	    && state.cursor_shape == 0x2000U
	    && state.process_entry_cursor_shape == 0x0607U);
}

static void
test_status_row_component_join(void)
{
	struct yt_present_state presentation;
	struct yt_present_result result;
	struct yt_framebuffer_state state;
	struct yt_framebuffer_state initial;
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];

	memset(&presentation, 0, sizeof(presentation));
	presentation.sound.snoop = -1.0f;
	yt_framebuffer_init(&state, false, 0U);
	initial = state;
	CHECK(yt_present_status_row((const uint8_t *)"John Doe", 8U,
	    (const uint8_t *)"Pilot", 5U, &presentation, &result)
	    == YT_PRESENT_OK);
	CHECK(result.event_count == 10U);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 25U && state.qb_column == 36U
	    && state.qb_attribute == 0x07U);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "2fdb1984445b3192a6ea207c830d5100c4256ed35f66baa6d1f6ea4ae425ba7b")
	    == 0);

	presentation.sound.snoop = 0.0f;
	CHECK(yt_present_status_row((const uint8_t *)"John Doe", 8U,
	    (const uint8_t *)"Pilot", 5U, &presentation, &result)
	    == YT_PRESENT_OK);
	CHECK(result.event_count == 0U);
	CHECK(yt_framebuffer_apply_present_result(&initial, &result)
	    == YT_FRAMEBUFFER_OK);
	yt_framebuffer_init(&state, false, 0U);
	CHECK(memcmp(&initial, &state, sizeof(state)) == 0);
}

static void
test_time_refresh_component_join(void)
{
	static const float reads[] = { 100.0f, 100.0f, 100.0f, 100.0f };
	struct yt_present_state presentation;
	struct yt_present_time_state time;
	struct yt_present_result result;
	struct yt_framebuffer_state state;
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];
	size_t used = 0U;
	bool updated = false;

	memset(&presentation, 0, sizeof(presentation));
	presentation.sound.ansi = -1.0f;
	presentation.sound.snoop = -1.0f;
	presentation.foreground = 2.0f;
	memset(&time, 0, sizeof(time));
	time.deadline = 460.0f;
	memcpy(time.text, " 6:00  ", 7U);
	time.text_length = 7U;
	CHECK(yt_present_refresh_time(&time, reads,
	    sizeof(reads) / sizeof(reads[0]), &used, 4, 9,
	    &presentation, &result, &updated) == YT_PRESENT_OK);
	CHECK(updated && used == 4U && result.event_count == 5U);
	yt_framebuffer_init(&state, false, 0U);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 4U && state.qb_column == 9U
	    && state.bios_row == 4U && state.bios_column == 9U
	    && state.brun_bios_cache_row == 4U
	    && state.brun_bios_cache_column == 9U
	    && state.cursor_visible && state.cursor_shape == 0x0110U
	    && state.qb_attribute == 0x07U);
	CHECK(memcmp(state.characters + 1990U, " 6:00  ", 7U) == 0);
	CHECK(memcmp(state.attributes + 1990U,
	    "\x1b\x1b\x1b\x1b\x1b\x1b\x1b", 7U) == 0);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "51715929f07368ee9a37b964834dfb7161a1dc55b786b9b0d355bb7bd17b5fb0")
	    == 0);
}

static void
test_opening_row_component_join(void)
{
	static const uint8_t row[] = { 'A', 0U, 'B' };
	struct yt_present_result result;
	struct yt_framebuffer_state state;
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];

	yt_framebuffer_init(&state, false, 0U);
	CHECK(yt_present_opening_row(row, sizeof(row), 0.0f, -1.0f,
	    &result) == YT_PRESENT_OK);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(yt_present_opening_cleanup(0.0f, -1.0f, &result)
	    == YT_PRESENT_OK);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.characters[0] == 'A' && state.characters[1] == 'B'
	    && memcmp(state.characters + 80U, "[0m", 3U) == 0
	    && state.qb_row == 2U && state.qb_column == 4U);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "e28c9315e62d69409aea1923ad663292a1d0f5f95904b30d918f0dca5a23961d")
	    == 0);
}

static void
test_press_any_key_component_join(void)
{
	struct yt_present_state presentation;
	struct yt_present_result result;
	struct yt_framebuffer_state state;
	struct yt_framebuffer_event locate = locate_event(7U, 9U);
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];
	float saved = 0.0f;

	memset(&presentation, 0, sizeof(presentation));
	presentation.sound.ansi = -1.0f;
	presentation.sound.snoop = -1.0f;
	presentation.sound.user_sound = -1.0f;
	presentation.sound.local_sound = -1.0f;
	presentation.foreground = 7.0f;
	yt_framebuffer_init(&state, false, 0U);
	CHECK(yt_framebuffer_apply(&state, &locate) == YT_FRAMEBUFFER_OK);
	CHECK(yt_present_press_prompt(&presentation, &result, &saved)
	    == YT_PRESENT_OK);
	CHECK(saved == 7.0f);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(yt_present_press_cleanup(saved, &presentation, &result)
	    == YT_PRESENT_OK);
	CHECK(yt_framebuffer_apply_present_result(&state, &result)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 7U && state.qb_column == 1U
	    && state.bios_row == 7U && state.bios_column == 1U
	    && state.qb_attribute == 0x06U
	    && presentation.foreground == 7.0f);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "3086cc0720d4c0ccf2ebaf60b00c4e5d8bf28e31007f90760b277a100661992c")
	    == 0);
}

static void
fatal_local(void *context, const uint8_t *data, size_t length)
{
	(void)context;
	(void)data;
	(void)length;
}

static void
fatal_close_all(void *context)
{
	(void)context;
}

static size_t
fatal_drain(void *context, uint16_t *words, size_t capacity)
{
	(void)context;
	(void)words;
	(void)capacity;
	return 0U;
}

static void
fatal_clear_function_bar(void *context)
{
	(void)context;
}

static void
fatal_restore(void *context, bool known, uint16_t shape)
{
	(void)context;
	(void)known;
	(void)shape;
}

static void
fatal_end(void *context, unsigned status)
{
	(void)context;
	(void)status;
}

static void
test_brun_fatal_component_join(void)
{
	static const struct yt_brun_internal_fatal_ops ops = {
		fatal_local,
		fatal_close_all,
		fatal_drain,
		fatal_clear_function_bar,
		fatal_restore,
		fatal_end
	};
	struct yt_brun_runtime_fatal_state fatal;
	struct yt_framebuffer_state state;
	struct yt_framebuffer_state before;
	struct yt_framebuffer_event event;
	uint8_t function_row[YT_FRAMEBUFFER_WIDTH];
	uint8_t raw[YT_FRAMEBUFFER_STATE_BYTES];
	char digest[65];
	size_t index;

	for (index = 0U; index < 10U; ++index) {
		size_t base = index * 8U;
		unsigned number = (unsigned)index + 1U;

		function_row[base] = number == 10U ? '1' : ' ';
		function_row[base + 1U] = number == 10U
		    ? '0' : (uint8_t)('0' + number);
		memset(function_row + base + 2U, ' ', 6U);
	}
	yt_framebuffer_init(&state, true, 0x0708U);
	event = color_event(14U, 3U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = locate_event(25U, 1U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = data_event(YT_FRAMEBUFFER_FUNCTION_BAR_SET,
	    function_row, sizeof(function_row));
	event.has_function_bar = true;
	event.function_bar = true;
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	event = locate_event(10U, 20U);
	CHECK(yt_framebuffer_apply(&state, &event) == YT_FRAMEBUFFER_OK);
	CHECK(yt_brun_runtime_error_fatal_run(53U, "YT-SUB  ", true, 610,
	    0x1f42U, 0x1abbU, false, true, true, 0x0708U,
	    &ops, NULL, &fatal));
	CHECK(yt_framebuffer_apply_brun_fatal(&state, &fatal.terminal)
	    == YT_FRAMEBUFFER_OK);
	CHECK(!state.function_bar && state.qb_row == 13U
	    && state.qb_column == 32U && state.bios_row == 14U
	    && state.bios_column == 1U
	    && state.brun_bios_cache_row == 14U
	    && state.brun_bios_cache_column == 1U);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "e6146405a4b5e6f5425588bf63e8863db415e37248439ac344d16e6ee98f1a4e")
	    == 0);

	before = state;
	fatal.terminal.function_bar_before = true;
	CHECK(yt_framebuffer_apply_brun_fatal(&state, &fatal.terminal)
	    == YT_FRAMEBUFFER_INVALID_ARGUMENT);
	CHECK(memcmp(&state, &before, sizeof(state)) == 0);

	yt_framebuffer_init(&state, false, 0U);
	CHECK(yt_brun_runtime_error_fatal_run(75U, "YT-INIT ", false, 0,
	    0x4444U, 0x23dfU, true, false, false, 0U,
	    &ops, NULL, &fatal));
	CHECK(yt_framebuffer_apply_brun_fatal(&state, &fatal.terminal)
	    == YT_FRAMEBUFFER_OK);
	CHECK(state.qb_row == 5U && state.qb_column == 1U
	    && state.bios_row == 6U && state.bios_column == 1U);
	CHECK(yt_framebuffer_serialize(&state, raw, sizeof(raw), NULL)
	    == YT_FRAMEBUFFER_OK);
	digest_hex(raw, sizeof(raw), digest);
	CHECK(strcmp(digest,
	    "ab4ea34a4f2f4b03361d35af3f695a3dd63aced54777142ef78ef2562468f2c8")
	    == 0);
}






int
main(void)
{
	test_initial_serialization();
	test_witness_and_prefix_law();
	test_qb_edges_and_function_bar();
	test_con_parser_and_observations();
	test_presentation_projection();
	test_documented_cursor_and_scroll_edges();
	test_documented_con_commands_and_stale_cache();
	test_end_cleanup();
	test_status_row_component_join();
	test_time_refresh_component_join();
	test_opening_row_component_join();
	test_press_any_key_component_join();
	test_brun_fatal_component_join();
	if (failures != 0U)
		fprintf(stderr, "%u framebuffer test(s) failed\n", failures);
	return failures == 0U ? 0 : 1;
}
