#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct packed_asset {
	const char *name;
	size_t original_size;
	size_t packed_size;
	uint64_t checksum;
	const char *const *chunks;
	size_t chunk_count;
};

#include "yt_assets.inc"

static int
base64_value(unsigned char byte)
{
	if (byte >= 'A' && byte <= 'Z')
		return byte - 'A';
	if (byte >= 'a' && byte <= 'z')
		return byte - 'a' + 26;
	if (byte >= '0' && byte <= '9')
		return byte - '0' + 52;
	if (byte == '+')
		return 62;
	if (byte == '/')
		return 63;
	return -1;
}

static int
decode_base64(const struct packed_asset *asset, uint8_t *packed)
{
	uint32_t accumulator = 0;
	unsigned bits = 0;
	size_t used = 0;
	size_t chunk;

	for (chunk = 0; chunk < asset->chunk_count; ++chunk) {
		const unsigned char *cursor =
		    (const unsigned char *)asset->chunks[chunk];

		while (*cursor != '\0' && *cursor != '=') {
			int value = base64_value(*cursor++);

			if (value < 0)
				return 0;
			accumulator = (accumulator << 6) | (uint32_t)value;
			bits += 6;
			if (bits >= 8) {
				bits -= 8;
				if (used >= asset->packed_size)
					return 0;
				packed[used++] =
				    (uint8_t)(accumulator >> bits);
				if (bits == 0)
					accumulator = 0;
				else
					accumulator &=
					    (UINT32_C(1) << bits) - 1U;
			}
		}
	}
	return used == asset->packed_size;
}

static int
unpack_lzss(const struct packed_asset *asset, const uint8_t *packed,
    uint8_t *output)
{
	size_t source = 0;
	size_t dest = 0;

	while (source < asset->packed_size
	    && dest < asset->original_size) {
		unsigned control = packed[source++];
		unsigned bit;

		for (bit = 0; bit < 8 && dest < asset->original_size; ++bit) {
			if ((control & (1U << bit)) == 0) {
				if (source >= asset->packed_size)
					return 0;
				output[dest++] = packed[source++];
			}
			else {
				unsigned encoded;
				size_t offset;
				size_t length;
				size_t count;

				if (source + 1U >= asset->packed_size)
					return 0;
				encoded = (unsigned)packed[source]
				    | ((unsigned)packed[source + 1U] << 8);
				source += 2;
				offset = (encoded & 0x0fffU) + 1U;
				length = (encoded >> 12) + 3U;
				if (offset > dest
				    || length > asset->original_size - dest)
					return 0;
				for (count = 0; count < length; ++count) {
					output[dest] = output[dest - offset];
					++dest;
				}
			}
		}
	}
	return source == asset->packed_size
	    && dest == asset->original_size;
}

static uint64_t
fnv1a(const uint8_t *data, size_t length)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	size_t index;

	for (index = 0; index < length; ++index) {
		hash ^= data[index];
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static int
write_asset(const char *directory, const struct packed_asset *asset)
{
	uint8_t *packed;
	uint8_t *output;
	char path[1024];
	FILE *file;
	int written;
	int ok = 0;

	packed = malloc(asset->packed_size);
	output = malloc(asset->original_size);
	if (packed == NULL || output == NULL) {
		fprintf(stderr, "assetgen: out of memory\n");
		goto done;
	}
	if (!decode_base64(asset, packed)
	    || !unpack_lzss(asset, packed, output)
	    || fnv1a(output, asset->original_size) != asset->checksum) {
		fprintf(stderr, "assetgen: corrupt embedded asset %s\n",
		    asset->name);
		goto done;
	}
	written = snprintf(path, sizeof(path), "%s/%s", directory,
	    asset->name);
	if (written < 0 || (size_t)written >= sizeof(path)) {
		fprintf(stderr, "assetgen: output path is too long\n");
		goto done;
	}
	file = fopen(path, "wb");
	if (file == NULL) {
		fprintf(stderr, "assetgen: cannot create %s: %s\n", path,
		    strerror(errno));
		goto done;
	}
	if (fwrite(output, 1, asset->original_size, file)
	    != asset->original_size || fclose(file) != 0) {
		fprintf(stderr, "assetgen: cannot write %s\n", path);
		goto done;
	}
	ok = 1;

done:
	free(output);
	free(packed);
	return ok;
}

int
main(int argc, char **argv)
{
	const char *directory = argc > 1 ? argv[1] : "data";
	size_t index;

	for (index = 0;
	    index < sizeof(packed_assets) / sizeof(packed_assets[0]);
	    ++index) {
		if (!write_asset(directory, &packed_assets[index]))
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
