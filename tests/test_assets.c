#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef YT_DATA_DIR
#define YT_DATA_DIR "data"
#endif

struct expected_asset {
	const char *name;
	size_t size;
	uint64_t checksum;
};

static uint64_t
fnv1a(FILE *file, size_t *size)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	size_t length = 0;
	int byte;

	while ((byte = fgetc(file)) != EOF) {
		hash ^= (uint8_t)byte;
		hash *= UINT64_C(1099511628211);
		++length;
	}
	*size = length;
	return hash;
}

int
main(void)
{
	static const struct expected_asset expected[] = {
		{"YTOPEN.ANS", 2629U, UINT64_C(0xaf58a719e3ae805e)},
		{"YTOPEN.ASC", 505U, UINT64_C(0x204289c490c71819)},
		{"YTINSTR.DOC", 24736U, UINT64_C(0x8a7d8d1e8a02653e)},
		{"YTSYSOP.DOC", 21005U, UINT64_C(0x060746de6459df39)},
		{"XANNORHQ.TXT", 230U, UINT64_C(0xed2a08a3c74c3305)},
		{"YTECHO.TXT", 715U, UINT64_C(0x27042307973877de)},
		{"LOCKOUT.DAT", 14U, UINT64_C(0xb0cf20ea24271bac)},
		{"YT.REG", 33U, UINT64_C(0x4166ec80b52f4a97)}
	};
	size_t index;

	for (index = 0; index < sizeof(expected) / sizeof(expected[0]);
	    ++index) {
		char path[1024];
		FILE *file;
		size_t size;
		uint64_t checksum;

		if (snprintf(path, sizeof(path), "%s/%s", YT_DATA_DIR,
		    expected[index].name) < 0) {
			return EXIT_FAILURE;
		}
		file = fopen(path, "rb");
		if (file == NULL) {
			fprintf(stderr, "test_assets: cannot open %s\n", path);
			return EXIT_FAILURE;
		}
		checksum = fnv1a(file, &size);
		if (ferror(file) || fclose(file) != 0
		    || size != expected[index].size
		    || checksum != expected[index].checksum) {
			fprintf(stderr, "test_assets: mismatch for %s\n", path);
			return EXIT_FAILURE;
		}
	}
	puts("test_assets: ok");
	return EXIT_SUCCESS;
}
