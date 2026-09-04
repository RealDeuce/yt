#include "yt_route.h"
#include "yt_main_error.h"
#include "qb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

static unsigned failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
		    __FILE__, __LINE__, #expr); \
		++failures; \
	} \
} while (0)

struct graph {
	float rows[8][6];
	int maximum;
	int expanded[32];
	size_t expanded_count;
	int fail_sector;
};

static bool
read_sector(void *context, int sector, float warps[6],
    struct yt_error *error)
{
	struct graph *graph = context;

	if (graph->expanded_count < YT_ARRAY_LEN(graph->expanded))
		graph->expanded[graph->expanded_count++] = sector;
	if (sector == graph->fail_sector || sector < 0
	    || sector > graph->maximum) {
		if (error != NULL) {
			error->status = YT_IO_ERROR;
			(void)snprintf(error->operation, sizeof(error->operation),
			    "%s", "route sector GET");
		}
		return false;
	}
	memcpy(warps, graph->rows[sector], sizeof(graph->rows[sector]));
	return true;
}

static bool
run_route(struct graph *graph, float start, float destination, float *status,
    const float avoid[YT_ROUTE_AVOID_COUNT], uint8_t conversion_mode,
    int16_t predecessor[YT_ROUTE_CAPACITY],
    int16_t second[YT_ROUTE_CAPACITY], enum yt_route_outcome *outcome,
    struct yt_error *error)
{
	graph->expanded_count = 0U;
	return yt_route_build(start, destination, status, avoid,
	    conversion_mode, predecessor, second, read_sector, graph, outcome,
	    error);
}

static void
test_fifo_and_failure_residue(void)
{
	struct graph graph = {.maximum = 4, .fail_sector = -1};
	float avoid[YT_ROUTE_AVOID_COUNT] = {0};
	int16_t predecessor[YT_ROUTE_CAPACITY];
	int16_t second[YT_ROUTE_CAPACITY];
	enum yt_route_outcome outcome;
	float status = 0.0f;

	graph.rows[1][0] = 3.0f;
	graph.rows[1][1] = 2.0f;
	graph.rows[2][0] = 4.0f;
	graph.rows[3][0] = 4.0f;
	CHECK(run_route(&graph, 1.0f, 4.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_FOUND && status == 0.0f);
	CHECK(second[1] == 3 && second[3] == 4 && second[4] == 0);
	CHECK(graph.expanded_count == 2U
	    && graph.expanded[0] == 1 && graph.expanded[1] == 3);

	memset(&graph, 0, sizeof(graph));
	graph.maximum = 3;
	graph.fail_sector = -1;
	graph.rows[1][0] = 2.0f;
	status = 0.0f;
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_NOT_FOUND && status == 1.0f);
	CHECK(second[1] == 0 && second[2] == 2);
}

static void
test_avoid_semantics(void)
{
	struct graph graph = {.maximum = 3, .fail_sector = -1};
	float avoid[YT_ROUTE_AVOID_COUNT] = {0};
	int16_t predecessor[YT_ROUTE_CAPACITY];
	int16_t second[YT_ROUTE_CAPACITY];
	enum yt_route_outcome outcome;
	struct yt_error error;
	float status;

	graph.rows[1][0] = 2.0f;
	graph.rows[2][0] = 3.0f;
	status = 1.0f;
	avoid[0] = 1.0f;
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_NOT_FOUND && status == 1.0f
	    && graph.expanded_count == 0U && second[1] == 0);

	memset(avoid, 0, sizeof(avoid));
	status = 1.0f;
	avoid[0] = 1.5f;
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_NOT_FOUND && predecessor[2] == 2);

	memset(avoid, 0, sizeof(avoid));
	status = 1.0f;
	avoid[0] = 1.4f;
	yt_error_clear(&error);
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, &error));
	CHECK(outcome == YT_ROUTE_BACK_EDGE && error.status == YT_OK);

	memset(avoid, 0, sizeof(avoid));
	status = 1.0f;
	avoid[0] = 2.6f;
	yt_error_clear(&error);
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, &error));
	CHECK(outcome == YT_ROUTE_BACK_EDGE && error.status == YT_OK);

	memset(avoid, 0, sizeof(avoid));
	status = 0.0f;
	avoid[0] = 1000000.0f;
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_FOUND && status == 0.0f);

	memset(avoid, 0, sizeof(avoid));
	status = 1.0f;
	avoid[0] = 3000.4f;
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_FOUND && predecessor[3000] == 3000);

	avoid[0] = 3000.6f;
	status = 1.0f;
	yt_error_clear(&error);
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, &error));
	CHECK(outcome == YT_ROUTE_FOUND && error.status == YT_OK
	    && second[0] == 3001);

	memset(avoid, 0, sizeof(avoid));
	status = 1.0f;
	avoid[0] = 1.5f;
	yt_error_clear(&error);
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 4,
	    predecessor, second, &outcome, &error));
	CHECK(outcome == YT_ROUTE_BACK_EDGE && error.status == YT_OK);
}

static void
test_wrapped_process_writes(void)
{
	struct graph graph = {.maximum = 3, .fail_sector = -1};
	struct yt_route_process process;
	float avoid[YT_ROUTE_AVOID_COUNT] = {0};
	enum yt_route_outcome outcome;
	struct yt_error error;
	float status;

	graph.rows[1][0] = 2.0f;
	graph.rows[2][0] = 3.0f;
	memset(&process, 0xa5, sizeof(process));
	memset(process.bytes + YT_ROUTE_WORKSPACE_ADDRESS, 0x5a,
	    YT_ROUTE_WORKSPACE_BYTES);
	avoid[0] = -1.0f;
	yt_error_clear(&error);
	CHECK(yt_route_process_set_avoid(&process, avoid, &error));
	CHECK(yt_route_process_avoid(&process, 0U) == -1.0f
	    && yt_route_process_set_avoid_slot(&process, 1U, 1.75f, &error)
	    && yt_route_process_avoid(&process, 1U) == 1.75f
	    && !yt_route_process_set_avoid_slot(&process,
	    YT_ROUTE_AVOID_COUNT, 1.0f, &error)
	    && yt_route_process_avoid(&process, YT_ROUTE_AVOID_COUNT) == 0.0f);
	yt_error_clear(&error);
	CHECK(yt_route_process_set_avoid_slot(&process, 1U, 0.0f, &error));
	status = 1.0f;
	CHECK(yt_route_process_build(1.0f, 3.0f, &status, 0, &process,
	    read_sector, &graph, &outcome, &error));
	CHECK(outcome == YT_ROUTE_FOUND && status == 0.0f
	    && yt_route_process_predecessor(&process, -1) == -1
	    && process.bytes[YT_ROUTE_WORKSPACE_ADDRESS - 2U] == 0xffU
	    && process.bytes[YT_ROUTE_WORKSPACE_ADDRESS - 1U] == 0xffU);

	memset(&process, 0, sizeof(process));
	memset(avoid, 0, sizeof(avoid));
	avoid[0] = 29130.0f;
	CHECK(yt_route_process_set_avoid(&process, avoid, &error));
	status = 1.0f;
	CHECK(yt_route_process_build(1.0f, 3.0f, &status, 0, &process,
	    read_sector, &graph, &outcome, &error));
	CHECK(outcome == YT_ROUTE_FOUND && process.bytes[0] == 0xcaU
	    && process.bytes[1] == 0x71U);

	memset(&process, 0, sizeof(process));
	memset(avoid, 0, sizeof(avoid));
	avoid[0] = 6952.0f;
	CHECK(yt_route_process_set_avoid(&process, avoid, &error));
	status = 1.0f;
	graph.expanded_count = 0U;
	CHECK(yt_route_process_build(1.0f, 3.0f, &status, 0, &process,
	    read_sector, &graph, &outcome, &error));
	CHECK(outcome == YT_ROUTE_NOT_FOUND && status == 1.0f
	    && graph.expanded_count == 0U && process.bytes[0x52bc] == 0x28U
	    && process.bytes[0x52bd] == 0x1bU);

	memset(&process, 0, sizeof(process));
	memset(avoid, 0, sizeof(avoid));
	CHECK(yt_route_process_set_avoid(&process, avoid, &error));
	memset(&graph, 0, sizeof(graph));
	graph.maximum = 3;
	graph.fail_sector = -1;
	graph.rows[1][0] = -1.0f;
	graph.rows[1][1] = 3.0f;
	status = 0.0f;
	CHECK(yt_route_process_build(1.0f, 3.0f, &status, 0, &process,
	    read_sector, &graph, &outcome, &error));
	CHECK(outcome == YT_ROUTE_FOUND
	    && yt_route_process_predecessor(&process, -1) == 1);
}

static void
test_same_zero_and_conversion_order(void)
{
	struct graph graph = {.maximum = 3, .fail_sector = -1};
	float avoid[YT_ROUTE_AVOID_COUNT] = {0};
	int16_t predecessor[YT_ROUTE_CAPACITY];
	int16_t second[YT_ROUTE_CAPACITY];
	enum yt_route_outcome outcome;
	struct yt_error error;
	float status = 7.0f;

	avoid[0] = 1000000.0f;
	CHECK(run_route(&graph, 1.0f, 1.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_SAME && status == 7.0f
	    && predecessor[0] == 1 && second[1] == 0
	    && graph.expanded_count == 0U);

	memset(&graph, 0, sizeof(graph));
	graph.maximum = 3;
	graph.fail_sector = -1;
	graph.rows[0][0] = 3.0f;
	status = 0.0f;
	memset(avoid, 0, sizeof(avoid));
	CHECK(run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, NULL));
	CHECK(outcome == YT_ROUTE_NOT_FOUND && graph.expanded_count == 3U);
	CHECK(graph.expanded[0] == 1 && graph.expanded[1] == 0
	    && graph.expanded[2] == 3);

	memset(&graph, 0, sizeof(graph));
	graph.maximum = 3;
	graph.fail_sector = -1;
	graph.rows[1][0] = 2.0f;
	graph.rows[1][5] = 40000.0f;
	status = 0.0f;
	yt_error_clear(&error);
	CHECK(!run_route(&graph, 1.0f, 3.0f, &status, avoid, 0,
	    predecessor, second, &outcome, &error));
	CHECK(error.status == YT_RANGE
	    && strcmp(error.operation, "route warp CINT") == 0
	    && !error.basic_fault_valid && predecessor[2] == 0);
}

static void
test_route_cint_fault_sites(void)
{
	struct graph graph = {.maximum = 3, .fail_sector = -1};
	struct yt_route_process process;
	float avoid[YT_ROUTE_AVOID_COUNT] = {0};
	enum yt_route_outcome outcome;
	struct yt_error error;
	float status;

	memset(&process, 0xa5, sizeof(process));
	status = 0.0f;
	yt_error_clear(&error);
	CHECK(!yt_route_process_build(40000.0f, 3.0f, &status, 0,
	    &process, read_sector, &graph, &outcome, &error));
	CHECK(error.status == YT_RANGE && error.basic_fault_valid
	    && error.basic_fault_site == YT_BASIC_FAULT_ROUTE_START_FIFO_CINT
	    && error.basic_error_valid && error.basic_error == 6U
	    && process.bytes[YT_ROUTE_WORKSPACE_ADDRESS] == 0U
	    && process.bytes[YT_ROUTE_WORKSPACE_ADDRESS
	    + YT_ROUTE_WORKSPACE_BYTES - 1U] == 0U);

	memset(&process, 0, sizeof(process));
	status = 0.0f;
	yt_error_clear(&error);
	CHECK(!yt_route_process_build(1.0f, 40000.0f, &status, 0,
	    &process, read_sector, &graph, &outcome, &error));
	CHECK(error.status == YT_RANGE && error.basic_fault_valid
	    && error.basic_fault_site
	    == YT_BASIC_FAULT_ROUTE_DESTINATION_PREDECESSOR_CINT
	    && error.basic_error_valid && error.basic_error == 6U
	    && yt_route_process_second(&process, 1) == 1
	    && yt_route_process_predecessor(&process, 1) == -1);

	memset(&process, 0, sizeof(process));
	avoid[0] = 40000.0f;
	yt_error_clear(&error);
	CHECK(yt_route_process_set_avoid(&process, avoid, &error));
	status = 1.0f;
	yt_error_clear(&error);
	CHECK(!yt_route_process_build(1.0f, 3.0f, &status, 0,
	    &process, read_sector, &graph, &outcome, &error));
	CHECK(error.status == YT_RANGE && error.basic_fault_valid
	    && error.basic_fault_site
	    == YT_BASIC_FAULT_ROUTE_AVOID_PREDECESSOR_CINT
	    && error.basic_error_valid && error.basic_error == 6U);
}

static void
test_addressed_route_arguments(void)
{
	static const uint16_t status_address = 0x4cf2U;
	static const uint16_t destination_address = 0x4e12U;
	static const uint16_t start_address = 0x4e1aU;
	struct graph graph = {.maximum = 3, .fail_sector = -1};
	struct yt_route_process process;
	enum yt_route_outcome outcome;
	struct yt_error error;
	float status;
	uint8_t raw[4];
	uint8_t returned[4];
	uint8_t raw_double[8];
	uint8_t returned_double[8];

	graph.rows[1][0] = 2.0f;
	graph.rows[2][0] = 3.0f;
	memset(&process, 0, sizeof(process));
	CHECK(qb_mbf32_encode(1.0f, raw) == QB_MBF_OK);
	yt_route_process_set_raw_single(&process, start_address, raw);
	CHECK(qb_mbf32_encode(3.0f, raw) == QB_MBF_OK);
	yt_route_process_set_raw_single(&process, destination_address, raw);
	CHECK(qb_mbf32_encode(1.0f, raw) == QB_MBF_OK);
	yt_route_process_set_raw_single(&process, status_address, raw);
	CHECK(yt_route_process_set_avoid_slot(&process, 0U, 6355.0f,
	    &error));
	yt_error_clear(&error);
	CHECK(yt_route_process_build_at(start_address, destination_address,
	    status_address, 0, &process, read_sector, &graph, &outcome,
	    &error));
	CHECK(outcome == YT_ROUTE_FOUND
	    && yt_route_process_single(&process, status_address) == 0.0f
	    && yt_route_process_second(&process, 1) == 2
	    && yt_route_process_second(&process, 2) == 3
	    && process.bytes[destination_address] == 0xd3U
	    && process.bytes[destination_address + 1U] == 0x18U);

	memset(&process, 0, sizeof(process));
	CHECK(qb_mbf32_encode(1.0f, raw) == QB_MBF_OK);
	yt_route_process_set_raw_single(&process, start_address, raw);
	CHECK(qb_mbf32_encode(3.0f, raw) == QB_MBF_OK);
	yt_route_process_set_raw_single(&process, destination_address, raw);
	CHECK(yt_route_process_set_avoid_slot(&process, 0U, 6355.0f,
	    &error));
	status = 1.0f;
	graph.expanded_count = 0U;
	yt_error_clear(&error);
	CHECK(yt_route_process_build_cells(start_address, destination_address,
	    &status, 0, &process, read_sector, &graph, &outcome, &error));
	CHECK(outcome == YT_ROUTE_FOUND && status == 0.0f
	    && yt_route_process_second(&process, 1) == 2
	    && yt_route_process_second(&process, 2) == 3
	    && process.bytes[destination_address] == 0xd3U
	    && process.bytes[destination_address + 1U] == 0x18U);

	memset(&process, 0, sizeof(process));
	raw[0] = 0x11U;
	raw[1] = 0x22U;
	raw[2] = 0x33U;
	raw[3] = 0x44U;
	yt_route_process_set_raw_single(&process, 0xfffeU, raw);
	memset(returned, 0, sizeof(returned));
	yt_route_process_raw_single(&process, 0xfffeU, returned);
	CHECK(memcmp(raw, returned, sizeof(raw)) == 0
	    && process.bytes[0xfffeU] == 0x11U
	    && process.bytes[0xffffU] == 0x22U
	    && process.bytes[0] == 0x33U
	    && process.bytes[1] == 0x44U);
	memset(returned, 0xa5, sizeof(returned));
	yt_route_process_set_raw_single(&process, 0x4ccaU, returned);
	yt_route_process_copy_raw_single(&process, 0xfffeU, 0x4ccaU);
	yt_route_process_raw_single(&process, 0x4ccaU, returned);
	CHECK(memcmp(raw, returned, sizeof(raw)) == 0);
	raw[0] = 0x55U;
	raw[1] = 0x66U;
	raw[2] = 0x77U;
	raw[3] = 0x88U;
	yt_route_process_set_raw_single(&process, 0x1234U, raw);
	yt_route_process_copy_raw_single(&process, 0x1234U, 0xffffU);
	yt_route_process_raw_single(&process, 0xffffU, returned);
	CHECK(memcmp(raw, returned, sizeof(raw)) == 0);
	yt_route_process_set_second(&process, -1, 0x1234);
	CHECK(yt_route_process_second(&process, -1) == 0x1234);
	yt_route_process_set_word(&process, 0xffffU, -2);
	CHECK(yt_route_process_word(&process, 0xffffU) == -2
	    && process.bytes[0xffffU] == 0xfeU
	    && process.bytes[0] == 0xffU);
	CHECK(qb_mbf64_encode(16777215.5, raw_double) == QB_MBF_OK);
	yt_route_process_set_raw_double(&process, 0xfffcU, raw_double);
	memset(returned_double, 0, sizeof(returned_double));
	yt_route_process_raw_double(&process, 0xfffcU, returned_double);
	CHECK(memcmp(raw_double, returned_double, sizeof(raw_double)) == 0
	    && yt_route_process_double(&process, 0xfffcU) == 16777215.5
	    && process.bytes[0xfffcU] == raw_double[0]
	    && process.bytes[3] == raw_double[7]);
	memset(returned_double, 0xa5, sizeof(returned_double));
	yt_route_process_set_raw_double(&process, 0x2000U, returned_double);
	yt_route_process_copy_raw_double(&process, 0xfffcU, 0x2000U);
	yt_route_process_raw_double(&process, 0x2000U, returned_double);
	CHECK(memcmp(raw_double, returned_double, sizeof(raw_double)) == 0);

	yt_error_clear(&error);
	CHECK(!yt_route_process_build_at(YT_ROUTE_WORKSPACE_ADDRESS,
	    destination_address, status_address, 0, &process, read_sector,
	    &graph, &outcome, &error));
	CHECK(error.status == YT_RANGE
	    && strcmp(error.operation, "route argument workspace alias") == 0);
}

static bool
reconstruction_back_edge_does_not_return(void)
{
#ifdef _WIN32
	SECURITY_ATTRIBUTES security = {
		sizeof(security), NULL, TRUE
	};
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	HANDLE ready_read;
	HANDLE ready_write;
	char executable[MAX_PATH];
	char command[MAX_PATH + 64U];
	char ready = '\0';
	DWORD transferred = 0U;
	DWORD waited;
	bool result;

	if (!CreatePipe(&ready_read, &ready_write, &security, 0)
	    || !SetHandleInformation(ready_read, HANDLE_FLAG_INHERIT, 0))
		return false;
	if (GetModuleFileNameA(NULL, executable, sizeof(executable)) == 0U
	    || snprintf(command, sizeof(command),
	    "\"%s\" --route-back-edge", executable) <= 0) {
		(void)CloseHandle(ready_read);
		(void)CloseHandle(ready_write);
		return false;
	}
	memset(&startup, 0, sizeof(startup));
	memset(&process, 0, sizeof(process));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = ready_write;
	startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL,
	    &startup, &process)) {
		(void)CloseHandle(ready_read);
		(void)CloseHandle(ready_write);
		return false;
	}
	(void)CloseHandle(ready_write);
	result = ReadFile(ready_read, &ready, 1U, &transferred, NULL)
	    && transferred == 1U && ready == 'R';
	waited = WaitForSingleObject(process.hProcess, 20U);
	result = result && waited == WAIT_TIMEOUT;
	if (waited == WAIT_TIMEOUT)
		(void)TerminateProcess(process.hProcess, 0U);
	(void)WaitForSingleObject(process.hProcess, INFINITE);
	(void)CloseHandle(process.hThread);
	(void)CloseHandle(process.hProcess);
	(void)CloseHandle(ready_read);
	return result;
#else
	int ready_pipe[2];
	pid_t child;
	pid_t waited;
	int status;
	char ready = '\0';
	struct timespec pause = {0, 20000000L};
	bool result;

	if (pipe(ready_pipe) != 0)
		return false;
	child = fork();
	if (child == 0) {
		(void)close(ready_pipe[0]);
		ready = 'R';
		(void)write(ready_pipe[1], &ready, 1U);
		(void)close(ready_pipe[1]);
		yt_route_reconstruction_back_edge();
	}
	if (child < 0) {
		(void)close(ready_pipe[0]);
		(void)close(ready_pipe[1]);
		return false;
	}
	(void)close(ready_pipe[1]);
	result = read(ready_pipe[0], &ready, 1U) == 1 && ready == 'R';
	(void)close(ready_pipe[0]);
	(void)nanosleep(&pause, NULL);
	waited = waitpid(child, &status, WNOHANG);
	result = result && waited == 0;
	if (waited == 0) {
		(void)kill(child, SIGTERM);
		result = result && waitpid(child, &status, 0) == child
		    && WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
	}
	return result;
#endif
}

int
main(int argc, char **argv)
{
#ifdef _WIN32
	if (argc == 2 && strcmp(argv[1], "--route-back-edge") == 0) {
		(void)fputc('R', stdout);
		(void)fflush(stdout);
		yt_route_reconstruction_back_edge();
	}
#else
	(void)argc;
	(void)argv;
#endif
	test_fifo_and_failure_residue();
	test_avoid_semantics();
	test_wrapped_process_writes();
	test_same_zero_and_conversion_order();
	test_route_cint_fault_sites();
	test_addressed_route_arguments();
	CHECK(reconstruction_back_edge_does_not_return());
	if (failures != 0U)
		return EXIT_FAILURE;
	puts("test_route: ok");
	return EXIT_SUCCESS;
}
