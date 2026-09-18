#include "yt_cli.h"

#include "yt_text.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

static const char *
status_name(enum yt_status status)
{
	switch (status) {
	case YT_OK:
		return "no error";
	case YT_IO_ERROR:
		return "I/O error";
	case YT_NOT_FOUND:
		return "file not found";
	case YT_INVALID:
		return "invalid data";
	case YT_RANGE:
		return "value out of range";
	case YT_NO_MEMORY:
		return "out of memory";
	case YT_RANDOM_ERROR:
		return "random source failure";
	case YT_CHILD_ERROR:
		return "child process failure";
	case YT_EOF:
		return "unexpected end of file";
	default:
		return "unknown error";
	}
}

void
yt_cli_error(const char *program, const struct yt_error *error)
{
	fprintf(stderr, "%s: %s", program, status_name(error->status));
	if (error->operation[0] != '\0')
		fprintf(stderr, " while attempting to %s", error->operation);
	if (error->path[0] != '\0')
		fprintf(stderr, " (%s)", error->path);
	if (error->system_error != 0)
		fprintf(stderr, ": system error %d", error->system_error);
	fputc('\n', stderr);
}

bool
yt_cli_line(char *text, size_t size)
{
	size_t length;

	if (size == 0)
		return false;
	if (fgets(text, (int)size, stdin) == NULL)
		return false;
	length = strlen(text);
	while (length > 0 && (text[length - 1] == '\n'
	    || text[length - 1] == '\r'))
		text[--length] = '\0';
	return true;
}

int
yt_cli_key(void)
{
#ifdef _WIN32
	return _getch();
#else
	struct termios saved;
	struct termios raw;
	int key;

	if (!isatty(STDIN_FILENO))
		return getchar();
	if (tcgetattr(STDIN_FILENO, &saved) != 0)
		return getchar();
	raw = saved;
	raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
		return getchar();
	key = getchar();
	(void)tcsetattr(STDIN_FILENO, TCSANOW, &saved);
	return key;
#endif
}

bool
yt_cli_write_dorinfo(const char *first, const char *last,
    struct yt_error *error)
{
	char data[512];
	int length = snprintf(data, sizeof(data),
	    "Yankee Trader Local Logon Program\r\n"
	    "Alan\r\n"
	    "Davenport\r\n"
	    "COM0\r\n"
	    "0 BAUD,N,8,1\r\n"
	    " 0 \r\n"
	    "%s\r\n"
	    "%s\r\n"
	    "Anytown, USA\r\n"
	    " 1 \r\n"
	    " 100 \r\n"
	    " 180 \r\n"
	    " 0 \r\n",
	    first, last);

	if (length < 0 || (size_t)length >= sizeof(data)) {
		if (error != NULL) {
			error->status = YT_RANGE;
			snprintf(error->operation, sizeof(error->operation),
			    "format DORINFO");
			snprintf(error->path, sizeof(error->path), "DORINFO1.DEF");
		}
		return false;
	}
	return yt_text_write("DORINFO1.DEF", (const uint8_t *)data,
	    (size_t)length, true, error);
}
