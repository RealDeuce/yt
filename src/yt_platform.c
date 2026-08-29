#include "yt_platform.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif

#ifdef _WIN32
struct rmt_serial_private {
	HANDLE handle;
	DCB original;
	DCB opening;
};
#else
struct rmt_serial_private {
	struct termios original;
	struct termios opening;
	speed_t observed_input;
	speed_t observed_output;
};
#endif

_Static_assert(sizeof(struct rmt_serial_private)
    <= YT_PLATFORM_RMT_SERIAL_PRIVATE,
    "RMT serial private state exceeds its public storage");
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <stdlib.h>
void arc4random_buf(void *, size_t);
#endif
#endif

static yt_clock_provider installed_clock_provider;
static void *installed_clock_context;

static void
set_error(struct yt_error *error, enum yt_status status, const char *operation,
    const char *path)
{
	if (error == NULL)
		return;
	error->status = status;
	error->system_error = errno;
	snprintf(error->operation, sizeof(error->operation), "%s", operation);
	snprintf(error->path, sizeof(error->path), "%s", path != NULL ? path : "");
}

bool
yt_platform_entropy(void *buffer, size_t length, struct yt_error *error)
{
#ifdef _WIN32
	NTSTATUS result = BCryptGenRandom(NULL, buffer, (ULONG)length,
	    BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (result != 0) {
		if (error != NULL) {
			error->status = YT_RANDOM_ERROR;
			error->system_error = (int)result;
			snprintf(error->operation, sizeof(error->operation),
			    "BCryptGenRandom");
			error->path[0] = '\0';
		}
		return false;
	}
	return true;
#elif defined(__FreeBSD__) || defined(__APPLE__)
	arc4random_buf(buffer, length);
	(void)error;
	return true;
#elif defined(__linux__)
	uint8_t *current = buffer;
	size_t remaining = length;

	while (remaining > 0) {
		ssize_t count = getrandom(current, remaining, 0);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			set_error(error, YT_RANDOM_ERROR, "getrandom", NULL);
			return false;
		}
		if (count == 0) {
			errno = EIO;
			set_error(error, YT_RANDOM_ERROR, "getrandom", NULL);
			return false;
		}
		current += (size_t)count;
		remaining -= (size_t)count;
	}
	return true;
#else
#error "No system CSPRNG implementation is defined for this platform"
#endif
}

void
yt_platform_set_clock_provider(yt_clock_provider provider, void *context)
{
	installed_clock_provider = provider;
	installed_clock_context = context;
}

bool
yt_platform_clock(struct yt_clock_value *value, struct yt_error *error)
{
	if (installed_clock_provider != NULL)
		return installed_clock_provider(installed_clock_context, value, error);
#ifdef _WIN32
	SYSTEMTIME system_time;

	GetLocalTime(&system_time);
	value->year = system_time.wYear;
	value->month = system_time.wMonth;
	value->day = system_time.wDay;
	value->hour = system_time.wHour;
	value->minute = system_time.wMinute;
	value->second = system_time.wSecond;
	value->hundredth = system_time.wMilliseconds / 10;
	(void)error;
	return true;
#else
	struct timespec now;
	struct tm local;

	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		set_error(error, YT_IO_ERROR, "clock_gettime", NULL);
		return false;
	}
	if (localtime_r(&now.tv_sec, &local) == NULL) {
		set_error(error, YT_IO_ERROR, "localtime_r", NULL);
		return false;
	}
	value->year = local.tm_year + 1900;
	value->month = local.tm_mon + 1;
	value->day = local.tm_mday;
	value->hour = local.tm_hour;
	value->minute = local.tm_min;
	value->second = local.tm_sec;
	value->hundredth = (int)(now.tv_nsec / 10000000L);
	return true;
#endif
}

double
yt_platform_timer(void)
{
	struct yt_clock_value value;

	if (!yt_platform_clock(&value, NULL))
		return 0.0;
	return (double)(value.hour * 3600 + value.minute * 60 + value.second)
	    + (double)value.hundredth / 100.0;
}

bool
yt_platform_executable_path(char *dest, size_t size, const char *argv0,
    struct yt_error *error)
{
#ifdef _WIN32
	DWORD count;

	if (size > (size_t)UINT_MAX)
		size = UINT_MAX;
	count = GetModuleFileNameA(NULL, dest, (DWORD)size);
	if (count == 0 || (size_t)count >= size) {
		set_error(error, YT_IO_ERROR, "GetModuleFileName", NULL);
		return false;
	}
	return true;
#else
	char resolved[PATH_MAX];

#if defined(__linux__)
	ssize_t count = readlink("/proc/self/exe", resolved, sizeof(resolved) - 1);
	if (count >= 0) {
		resolved[count] = '\0';
		if (strlen(resolved) >= size) {
			set_error(error, YT_RANGE, "executable path", resolved);
			return false;
		}
		strcpy(dest, resolved);
		return true;
	}
#endif
	if (argv0 == NULL || *argv0 == '\0') {
		set_error(error, YT_INVALID, "executable path", NULL);
		return false;
	}
	if (realpath(argv0, resolved) == NULL) {
		if (strlen(argv0) >= size) {
			set_error(error, YT_RANGE, "executable path", argv0);
			return false;
		}
		strcpy(dest, argv0);
		return true;
	}
	if (strlen(resolved) >= size) {
		set_error(error, YT_RANGE, "executable path", resolved);
		return false;
	}
	strcpy(dest, resolved);
	return true;
#endif
}

bool
yt_platform_sibling_program(char *dest, size_t size,
    const char *executable_path, const char *program, struct yt_error *error)
{
	const char *slash;
	size_t directory_length;
	const char *suffix;
	const char *dot;
	int written;

	slash = strrchr(executable_path, '/');
#ifdef _WIN32
	{
		const char *backslash = strrchr(executable_path, '\\');
		if (backslash != NULL && (slash == NULL || backslash > slash))
			slash = backslash;
	}
#endif
	directory_length = slash == NULL ? 0 : (size_t)(slash - executable_path + 1);
	suffix = slash == NULL ? executable_path : slash + 1;
	dot = strchr(suffix, '.');
	suffix = dot != NULL ? dot : "";
	written = snprintf(dest, size, "%.*s%s%s", (int)directory_length,
	    executable_path, program, suffix);
	if (written < 0 || (size_t)written >= size) {
		set_error(error, YT_RANGE, "sibling program", program);
		return false;
	}
	return true;
}

bool
yt_platform_spawn(const char *program, char *const argv[],
    enum yt_spawn_mode mode, int *exit_code, struct yt_error *error)
{
#ifdef _WIN32
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	char command[4096] = {0};
	size_t used = 0;
	size_t index;
	DWORD status;

	for (index = 0; argv[index] != NULL; ++index) {
		const char *argument = argv[index];
		size_t backslashes = 0;

#define APPEND_COMMAND_CHARACTER(character) do { \
	if (used + 1U >= sizeof(command)) { \
		set_error(error, YT_RANGE, "spawn command", program); \
		return false; \
	} \
	command[used++] = (character); \
} while (0)

		if (index != 0)
			APPEND_COMMAND_CHARACTER(' ');
		APPEND_COMMAND_CHARACTER('"');
		for (; *argument != '\0'; ++argument) {
			if (*argument == '\\') {
				++backslashes;
				continue;
			}
			if (*argument == '"') {
				size_t slash;

				for (slash = 0; slash < backslashes * 2U + 1U;
				    ++slash)
					APPEND_COMMAND_CHARACTER('\\');
				APPEND_COMMAND_CHARACTER('"');
				backslashes = 0;
				continue;
			}
			while (backslashes != 0) {
				APPEND_COMMAND_CHARACTER('\\');
				--backslashes;
			}
			APPEND_COMMAND_CHARACTER(*argument);
		}
		while (backslashes != 0) {
			APPEND_COMMAND_CHARACTER('\\');
			APPEND_COMMAND_CHARACTER('\\');
			--backslashes;
		}
		APPEND_COMMAND_CHARACTER('"');
#undef APPEND_COMMAND_CHARACTER
	}
	command[used] = '\0';
	memset(&startup, 0, sizeof(startup));
	memset(&process, 0, sizeof(process));
	startup.cb = sizeof(startup);
	if (!CreateProcessA(program, command, NULL, NULL, TRUE, 0, NULL, NULL,
	    &startup, &process)) {
		set_error(error, YT_CHILD_ERROR, "CreateProcess", program);
		return false;
	}
	if (mode == YT_SPAWN_REPLACE)
		ExitProcess(0);
	WaitForSingleObject(process.hProcess, INFINITE);
	GetExitCodeProcess(process.hProcess, &status);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	if (exit_code != NULL)
		*exit_code = (int)status;
	return true;
#else
	pid_t child;
	pid_t waited;
	int status;

	if (mode == YT_SPAWN_REPLACE) {
		execv(program, argv);
		set_error(error, YT_CHILD_ERROR, "execv", program);
		return false;
	}
	child = fork();
	if (child < 0) {
		set_error(error, YT_CHILD_ERROR, "fork", program);
		return false;
	}
	if (child == 0) {
		execv(program, argv);
		_exit(127);
	}
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited < 0) {
		set_error(error, YT_CHILD_ERROR, "waitpid", program);
		return false;
	}
	if (!WIFEXITED(status)) {
		set_error(error, YT_CHILD_ERROR, "waitpid", program);
		return false;
	}
	if (exit_code != NULL)
		*exit_code = WEXITSTATUS(status);
	return true;
#endif
}

void
yt_platform_delay(unsigned milliseconds)
{
#ifdef _WIN32
	Sleep(milliseconds);
#else
	struct timespec duration;

	duration.tv_sec = (time_t)(milliseconds / 1000U);
	duration.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
	while (nanosleep(&duration, &duration) != 0 && errno == EINTR)
		;
#endif
}

#ifndef _WIN32
static uint32_t
rmt_termios_baud(speed_t speed)
{
	switch (speed) {
#ifdef B50
	case B50: return 50U;
#endif
#ifdef B75
	case B75: return 75U;
#endif
#ifdef B110
	case B110: return 110U;
#endif
#ifdef B134
	case B134: return 134U;
#endif
#ifdef B150
	case B150: return 150U;
#endif
#ifdef B200
	case B200: return 200U;
#endif
#ifdef B300
	case B300: return 300U;
#endif
#ifdef B600
	case B600: return 600U;
#endif
	case B1200: return 1200U;
#ifdef B1800
	case B1800: return 1800U;
#endif
	case B2400: return 2400U;
	case B4800: return 4800U;
	case B9600: return 9600U;
	case B19200: return 19200U;
	case B38400: return 38400U;
#ifdef B57600
	case B57600: return 57600U;
#endif
#ifdef B115200
	case B115200: return 115200U;
#endif
	default: return 0U;
	}
}

static bool
rmt_termios_framing(struct termios *terminal,
    const struct yt_startup_framing *framing)
{
	if (terminal == NULL || framing == NULL || framing->opening_baud != 1200U
	    || (framing->data_bits != 7U && framing->data_bits != 8U)
	    || framing->stop_bits != 1U
	    || (framing->parity != YT_STARTUP_PARITY_NONE
	    && framing->parity != YT_STARTUP_PARITY_EVEN))
		return false;
	terminal->c_cflag &= (tcflag_t)~(CSIZE | PARENB | PARODD | CSTOPB);
	terminal->c_cflag |= framing->data_bits == 7U ? CS7 : CS8;
	if (framing->parity == YT_STARTUP_PARITY_EVEN)
		terminal->c_cflag |= PARENB;
	/* BASIC's COM device writes bytes without tty LF/CR translation. */
	terminal->c_oflag &= (tcflag_t)~OPOST;
	return true;
}
#endif

bool
yt_platform_rmt_serial_prepare(int port,
    const struct yt_startup_framing *framing,
    struct yt_platform_rmt_serial *serial, struct yt_error *error)
{
	struct rmt_serial_private state;

	if (serial == NULL || framing == NULL || port < 1 || port > 4
	    || framing->opening_baud != 1200U
	    || (framing->data_bits != 7U && framing->data_bits != 8U)
	    || framing->stop_bits != 1U
	    || (framing->parity != YT_STARTUP_PARITY_NONE
	    && framing->parity != YT_STARTUP_PARITY_EVEN)) {
		set_error(error, YT_INVALID, "prepare RMT serial", NULL);
		return false;
	}
	memset(serial, 0, sizeof(*serial));
	memset(&state, 0, sizeof(state));
#ifdef _WIN32
	{
		char device[16];
		int written = snprintf(device, sizeof(device), "\\\\.\\COM%d", port);

		if (written < 0 || (size_t)written >= sizeof(device)) {
			set_error(error, YT_RANGE, "name RMT serial", NULL);
			return false;
		}
		state.handle = CreateFileA(device, GENERIC_READ | GENERIC_WRITE,
		    0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (state.handle == INVALID_HANDLE_VALUE) {
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				error->system_error = (int)GetLastError();
				snprintf(error->operation, sizeof(error->operation),
				    "open RMT serial");
				snprintf(error->path, sizeof(error->path), "%s", device);
			}
			return false;
		}
		state.original.DCBlength = sizeof(state.original);
		if (!GetCommState(state.handle, &state.original)) {
			DWORD system_error = GetLastError();

			CloseHandle(state.handle);
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				error->system_error = (int)system_error;
				snprintf(error->operation, sizeof(error->operation),
				    "observe RMT serial");
				snprintf(error->path, sizeof(error->path), "%s", device);
			}
			return false;
		}
		state.opening = state.original;
		state.opening.BaudRate = framing->opening_baud;
		state.opening.ByteSize = framing->data_bits;
		state.opening.Parity = framing->parity == YT_STARTUP_PARITY_EVEN
		    ? EVENPARITY : NOPARITY;
		state.opening.fParity = framing->parity == YT_STARTUP_PARITY_EVEN;
		state.opening.StopBits = ONESTOPBIT;
		if (!SetCommState(state.handle, &state.opening)) {
			DWORD system_error = GetLastError();

			CloseHandle(state.handle);
			if (error != NULL) {
				error->status = YT_IO_ERROR;
				error->system_error = (int)system_error;
				snprintf(error->operation, sizeof(error->operation),
				    "open RMT serial at 1200");
				snprintf(error->path, sizeof(error->path), "%s", device);
			}
			return false;
		}
		serial->native_handle = (uintptr_t)state.handle;
		serial->observed_baud = state.original.BaudRate;
		serial->owns_handle = true;
	}
#else
	(void)port;
	if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &state.original) != 0) {
		set_error(error, YT_IO_ERROR, "observe RMT serial", "stdin");
		return false;
	}
	state.observed_input = cfgetispeed(&state.original);
	state.observed_output = cfgetospeed(&state.original);
	if (state.observed_input != state.observed_output
	    || rmt_termios_baud(state.observed_input) == 0U) {
		set_error(error, YT_RANGE, "decode RMT serial speed", "stdin");
		return false;
	}
	state.opening = state.original;
	if (!rmt_termios_framing(&state.opening, framing)
	    || cfsetispeed(&state.opening, B1200) != 0
	    || cfsetospeed(&state.opening, B1200) != 0
	    || tcsetattr(STDIN_FILENO, TCSANOW, &state.opening) != 0) {
		set_error(error, YT_IO_ERROR, "open RMT serial at 1200", "stdin");
		return false;
	}
	serial->observed_baud = rmt_termios_baud(state.observed_input);
#endif
	memcpy(serial->private_state, &state, sizeof(state));
	serial->prepared = true;
	return true;
}

bool
yt_platform_rmt_serial_restore(struct yt_platform_rmt_serial *serial,
    struct yt_error *error)
{
	struct rmt_serial_private state;

	if (serial == NULL || !serial->prepared || serial->restored) {
		set_error(error, YT_INVALID, "restore RMT serial", NULL);
		return false;
	}
	memcpy(&state, serial->private_state, sizeof(state));
#ifdef _WIN32
	state.opening.BaudRate = state.original.BaudRate;
	if (!SetCommState(state.handle, &state.opening)) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			error->system_error = (int)GetLastError();
			snprintf(error->operation, sizeof(error->operation),
			    "restore RMT serial speed");
			error->path[0] = '\0';
		}
		return false;
	}
#else
	if (cfsetispeed(&state.opening, state.observed_input) != 0
	    || cfsetospeed(&state.opening, state.observed_output) != 0
	    || tcsetattr(STDIN_FILENO, TCSANOW, &state.opening) != 0) {
		set_error(error, YT_IO_ERROR, "restore RMT serial speed", "stdin");
		return false;
	}
#endif
	serial->restored = true;
	memcpy(serial->private_state, &state, sizeof(state));
	return true;
}

void
yt_platform_rmt_serial_close(struct yt_platform_rmt_serial *serial)
{
	struct rmt_serial_private state;

	if (serial == NULL || !serial->prepared)
		return;
	memcpy(&state, serial->private_state, sizeof(state));
#ifdef _WIN32
	(void)SetCommState(state.handle,
	    serial->restored ? &state.opening : &state.original);
	if (serial->owns_handle)
		(void)CloseHandle(state.handle);
#else
	(void)tcsetattr(STDIN_FILENO, TCSANOW,
	    serial->restored ? &state.opening : &state.original);
#endif
	memset(serial, 0, sizeof(*serial));
}
