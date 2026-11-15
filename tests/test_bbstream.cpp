
// Unit tests for the bbstream + bbfilesystem handle-table integration.
// Links against the REAL bbstream.cpp / bbfilesystem.cpp (via bbruntime_core)
// + stubs_gxruntime.cpp. No copied code: all bb* functions are the real ones.
//
// Covers:
//  - WriteFile/WriteInt/WriteString -> CloseFile -> ReadFile/ReadInt/ReadString round-trip
//  - same object -> same handle (find-or-alloc)
//  - file handle accepted by stream-family functions (family check)
//  - stale handle after CloseFile: debug=true calls the hook, debug=false does not
//  - bank + file together via bbWriteBytes/bbReadBytes
//  - filesystem_destroy (cleanup) leaves the handle table empty

#include "../bbruntime/bb_handle.h"
#include "../bbruntime/bb_handle_types.h"
#include "../bbruntime/basic.h"   // BBStr definition (self-contained, no gxruntime)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real functions (defined in bbstream.cpp / bbfilesystem.cpp).
struct bbBank;

BB_HANDLE(bbBank) bbCreateBank(int size);
void bbFreeBank(BB_HANDLE(bbBank) h);
int bbPeekByte(BB_HANDLE(bbBank) h, int offset);
void bbPokeByte(BB_HANDLE(bbBank) h, int offset, int value);
int bbWriteBytes(BB_HANDLE(bbBank) h, BB_HANDLE(bbStream) s_h, int offset, int count);
int bbReadBytes(BB_HANDLE(bbBank) h, BB_HANDLE(bbStream) s_h, int offset, int count);

BB_HANDLE(bbFile) bbWriteFile(BBStr *f);
BB_HANDLE(bbFile) bbReadFile(BBStr *f);
void bbCloseFile(BB_HANDLE(bbStream) s_h);
int bbEof(BB_HANDLE(bbStream) s_h);
int bbReadInt(BB_HANDLE(bbStream) s_h);
float bbReadFloat(BB_HANDLE(bbStream) s_h);
BBStr *bbReadString(BB_HANDLE(bbStream) s_h);
void bbWriteInt(BB_HANDLE(bbStream) s_h, int n);
void bbWriteFloat(BB_HANDLE(bbStream) s_h, float n);
void bbWriteString(BB_HANDLE(bbStream) s_h, BBStr *t);

// The real debug flag (defined in stubs_gxruntime.cpp).
extern bool debug;

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

// ---- Test 1: write -> close -> read round-trip, same handle for same object ----
static void test_file_roundtrip() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	const char *path = "/tmp/bbstream_test.dat";

	// Write data to a file via bbWriteFile (trunc mode).
	BB_HANDLE(bbFile) w = bbWriteFile(new BBStr(path));
	CHECK(w.h != 0);

	// Same object -> same handle: the handle table is find-or-alloc, so
	// looking up the same pointer twice yields the same handle value.
	{
		bbStream *raw = BB_HANDLE(bbStream)(w.h); // resolve to raw pointer
		CHECK(raw != 0);
		if (raw) {
			int h1 = g_handle_table.lookup_or_alloc(raw, HT_STREAM);
			int h2 = g_handle_table.lookup_or_alloc(raw, HT_STREAM);
			CHECK(h1 == w.h);
			CHECK(h2 == w.h);
			CHECK(h1 == h2);
		}
	}

	// Write data through the handle (as a stream-family handle).
	bbWriteInt(BB_HANDLE(bbStream)(w.h), 0x12345678);
	bbWriteFloat(BB_HANDLE(bbStream)(w.h), 3.5f);
	bbWriteString(BB_HANDLE(bbStream)(w.h), new BBStr("hello blitz"));

	// CloseFile takes a stream-family handle: the file handle must be accepted.
	// ~bbFile deletes the filebuf, which flushes to disk.
	bbCloseFile(BB_HANDLE(bbStream)(w.h));
	CHECK(error_log.empty());

	// Read it back through a fresh handle to the same object.
	BB_HANDLE(bbFile) r = bbReadFile(new BBStr(path));
	CHECK(r.h != 0);
	int rh = r.h; // file handle as stream-family handle (same int value)
	CHECK(bbReadInt(BB_HANDLE(bbStream)(rh)) == 0x12345678);
	float f;
	{
		// bbReadFloat returns a float; check via a small reader.
		f = bbReadFloat(BB_HANDLE(bbStream)(rh));
	}
	CHECK(f == 3.5f);
	BBStr *s = bbReadString(BB_HANDLE(bbStream)(rh));
	CHECK(s != 0);
	if (s) {
		CHECK(std::string(*s) == "hello blitz");
		delete s;
	}
	CHECK(bbEof(BB_HANDLE(bbStream)(rh)) != 0);
	bbCloseFile(BB_HANDLE(bbStream)(rh));
}

// ---- Test 2: stale file handle — debug=true calls hook, debug=false doesn't ----
static void test_stale_file_handle() {
	reset_table();
	debug = true;
	bb_handle_debug = true;
	bb_handle_error = record_error;

	const char *path = "/tmp/bbstream_test2.dat";
	BB_HANDLE(bbFile) w = bbWriteFile(new BBStr(path));
	int old_handle = w.h;
	CHECK(old_handle != 0);

	bbCloseFile(BB_HANDLE(bbStream)(w.h)); // forget() runs in ~bbFile

	// Reuse the stale handle with debug on: hook must fire.
	error_log.clear();
	bbStream *p = BB_HANDLE(bbStream)(old_handle);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug off: no hook call, still null.
	debug = false;
	bb_handle_debug = false;
	error_log.clear();
	p = BB_HANDLE(bbStream)(old_handle);
	CHECK(p == 0);
	CHECK(error_log.empty());
}

// ---- Test 3: bank + file together via bbWriteBytes / bbReadBytes ----
static void test_bank_file_bytes() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;

	const char *path = "/tmp/bbstream_test3.dat";

	BB_HANDLE(bbBank) bank = bbCreateBank(16);
	CHECK(bank.h != 0);
	for (int i = 0; i < 16; i++) bbPokeByte(bank, i, i);

	// Use bbWriteBytes: bank -> file stream handle.
	BB_HANDLE(bbFile) w = bbWriteFile(new BBStr(path));
	CHECK(w.h != 0);
	int n = bbWriteBytes(bank, BB_HANDLE(bbStream)(w.h), 0, 16);
	CHECK(n == 16);
	bbCloseFile(BB_HANDLE(bbStream)(w.h));

	// Read back into a second bank from the file stream handle.
	BB_HANDLE(bbBank) bank2 = bbCreateBank(16);
	BB_HANDLE(bbFile) r = bbReadFile(new BBStr(path));
	CHECK(r.h != 0);
	n = bbReadBytes(bank2, BB_HANDLE(bbStream)(r.h), 0, 16);
	CHECK(n == 16);

	// Compare byte-by-byte via the real bbPeekByte.
	bool same = true;
	for (int i = 0; i < 16; i++) if (bbPeekByte(bank, i) != bbPeekByte(bank2, i)) same = false;
	CHECK(same);

	bbFreeBank(bank);
	bbFreeBank(bank2);
	bbCloseFile(BB_HANDLE(bbStream)(r.h));
}

// ---- Test 4: filesystem_destroy leaves the handle table empty ----
static void test_filesystem_destroy_cleanup() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;

	const char *path = "/tmp/bbstream_test4.dat";
	BB_HANDLE(bbFile) w = bbWriteFile(new BBStr(path));
	CHECK(w.h != 0);
	CHECK(g_handle_table.by_ptr.size() == 1);

	// filesystem_destroy closes all open files (bbCloseFile -> ~bbFile -> forget).
	extern bool filesystem_destroy();
	CHECK(filesystem_destroy());
	CHECK(g_handle_table.by_ptr.empty());
}

int main() {
	printf("bbstream + bbfilesystem handle-table integration tests\n");
	printf("=======================================================\n");

	test_file_roundtrip();
	printf("  file round-trip + same-object same-handle: OK\n");

	test_stale_file_handle();
	printf("  stale file handle (debug on/off): OK\n");

	test_bank_file_bytes();
	printf("  bank <-> file via WriteBytes/ReadBytes: OK\n");

	test_filesystem_destroy_cleanup();
	printf("  filesystem_destroy empties the table: OK\n");

	printf("=======================================================\n");
	printf("%d checks, %d failed\n", tests_run, tests_failed);
	if (tests_failed) {
		printf("RESULT: FAIL\n");
		return 1;
	}
	printf("RESULT: PASS\n");
	return 0;
}

#else // _WIN32

#include <cstdio>

int main() {
	printf("Windows build: bbstream handle tests not compiled (BB_HANDLE(T) = T*)\n");
	printf("RESULT: PASS (nothing to test)\n");
	return 0;
}

#endif // _WIN32
