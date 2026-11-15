// Unit tests for the bbtimer handle-table integration.
// Links against the REAL bbtimer.cpp (via bbruntime_core) + stubs_gxruntime.cpp.
// Tests bbCreateTimer/bbWaitTimer/bbFreeTimer via handles.
// Tests that using a freed timer handle with debug=true calls the error hook.

#include "../bbruntime/bb_handle.h"
#include "../bbruntime/bb_handle_types.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real bbtimer functions (defined in bbtimer.cpp).
class gxTimer;
BB_HANDLE(gxTimer) bbCreateTimer(int hertz);
int bbWaitTimer(BB_HANDLE(gxTimer) t_h);
void bbFreeTimer(BB_HANDLE(gxTimer) t_h);

// The real debug flag and gx_runtime (defined in stubs_gxruntime.cpp).
extern bool debug;
extern class gxRuntime *gx_runtime;

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
	++tests_run; \
	if (!(cond)) { \
		++tests_failed; \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while (0)

static std::vector<std::string> error_log;
static void record_error(const char *msg) {
	error_log.push_back(msg ? msg : "(null)");
}

static void reset_table() {
	g_handle_table.by_handle.clear();
	g_handle_table.by_handle.push_back(HandleTable::Entry());
	g_handle_table.by_ptr.clear();
	g_handle_table.free_list_head = 0;
}

// ---- Test 1: create -> wait -> free, same handle for same object ----
static void test_basic_lifecycle() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(gxTimer) h = bbCreateTimer(10);
	CHECK(h.h != 0);

	// Wait should return a tick count (stub returns 1 per call).
	int ticks = bbWaitTimer(h);
	CHECK(ticks == 1);

	// Free.
	bbFreeTimer(h);
}

// ---- Test 2: same object -> same handle (find-or-alloc) ----
static void test_same_handle() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(gxTimer) h1 = bbCreateTimer(10);
	int handle1 = h1.h;
	CHECK(handle1 != 0);

	// Create a second timer — should get a different handle.
	BB_HANDLE(gxTimer) h2 = bbCreateTimer(20);
	CHECK(h2.h != 0);
	CHECK(h2.h != handle1);

	bbFreeTimer(h1);
	bbFreeTimer(h2);
}

// ---- Test 3: freed timer handle with debug=true calls the hook ----
static void test_freed_handle_debug_hook() {
	reset_table();
	debug = true;
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(gxTimer) h = bbCreateTimer(10);
	int old_handle = h.h;
	CHECK(old_handle != 0);

	// Free the timer.
	bbFreeTimer(h);

	// Use the old handle: should call the hook (stale handle).
	error_log.clear();
	BB_HANDLE(gxTimer) stale(old_handle);
	// Trigger lookup by converting to pointer.
	gxTimer *p = stale;
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug=false: no hook call.
	debug = false;
	bb_handle_debug = false;
	error_log.clear();
	p = stale;
	CHECK(p == 0);
	CHECK(error_log.empty());
}

int main() {
	printf("gxTimer handle-table integration tests (real bbtimer.cpp)\n");
	printf("=======================================================\n");

	test_basic_lifecycle();
	test_same_handle();
	test_freed_handle_debug_hook();

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

#else
int main() { printf("skipped on Windows\n"); return 0; }
#endif
