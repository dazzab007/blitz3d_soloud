
// Unit tests for the bbbank handle-table integration.
// Calls bbCreateBank/bbPokeInt/bbPeekInt/bbResizeBank/bbFreeBank via handles.
// Tests that using a freed bank handle with debug=true calls the error hook.
//
// This test defines its own stubs for the Windows-only dependencies
// (gx_runtime, debug) so it can link without the full bbruntime library.

#include "../bbruntime/bb_handle.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>

#ifndef _WIN32

// ---- Stubs for bbsys.h dependencies ----
bool debug = false;

struct gxRuntimeStub {
	void debugError(const char *t) {}
	int callDll(const std::string &dll, const std::string &func,
		const void *in, int in_sz, void *out, int out_sz) { return 0; }
};
static gxRuntimeStub gx_runtime_stub;
gxRuntimeStub *gx_runtime = &gx_runtime_stub;

struct bbEx {
	const char *err;
	bbEx(const char *e) : err(e) {}
};
#define RTEX(_X_) throw bbEx(_X_)

// ---- bbBank definition (mirrors bbbank.cpp) ----
struct bbBank {
	char *data;
	int size, capacity;

	bbBank(int sz) : size(sz) {
		capacity = (size + 15) & ~15;
		data = new char[capacity];
		memset(data, 0, size);
	}
	virtual ~bbBank() {
#ifndef _WIN32
		g_handle_table.forget(this);
#endif
		delete[] data;
	}
	void resize(int n) {
		if (n > size) {
			if (n > capacity) {
				capacity = capacity * 3 / 2;
				if (n > capacity) capacity = n;
				capacity = (capacity + 15) & ~15;
				char *p = new char[capacity];
				memcpy(p, data, size);
				delete[] data;
				data = p;
			} else memset(data + size, 0, n - size);
		}
		size = n;
	}
};

// ---- handle_type_tag for bbBank (from bb_handle_types.h) ----
template <> struct handle_type_tag<bbBank> { static const int value = HT_BANK; };

// ---- Bank functions (mirrors bbbank.cpp, using BB_HANDLE) ----
static std::set<bbBank *> bank_set;

static inline void debugBank(bbBank *b) {
	if (debug) {
		if (!bank_set.count(b)) RTEX("bbBank does not exist");
	}
}

static inline void debugBank(bbBank *b, int offset) {
	if (debug) {
		debugBank(b);
		if (offset >= b->size) RTEX("Offset out of range");
	}
}

BB_HANDLE(bbBank) bbCreateBank(int size) {
	bbBank *b = new bbBank(size);
	bank_set.insert(b);
	return BB_HANDLE(bbBank)(b);
}

void bbFreeBank(BB_HANDLE(bbBank) h) {
	bbBank *b = h;
	if (bank_set.erase(b)) delete b;
}

int bbBankSize(BB_HANDLE(bbBank) h) {
	bbBank *b = h;
	debugBank(b);
	return b->size;
}

void bbResizeBank(BB_HANDLE(bbBank) h, int size) {
	bbBank *b = h;
	debugBank(b);
	b->resize(size);
}

int bbPeekInt(BB_HANDLE(bbBank) h, int offset) {
	bbBank *b = h;
	debugBank(b, offset + 3);
	return *(int *)(b->data + offset);
}

void bbPokeInt(BB_HANDLE(bbBank) h, int offset, int value) {
	bbBank *b = h;
	debugBank(b, offset);
	*(int *)(b->data + offset) = value;
}

// ---- Test infrastructure ----
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

// ---- Test 1: create, poke, peek, resize, free ----
static void test_basic_lifecycle() {
	reset_table();
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(bbBank) h = bbCreateBank(64);
	CHECK(h.h != 0);
	CHECK(bbBankSize(h) == 64);

	// Poke and peek an int.
	bbPokeInt(h, 0, 42);
	CHECK(bbPeekInt(h, 0) == 42);

	bbPokeInt(h, 4, -123);
	CHECK(bbPeekInt(h, 4) == -123);

	// Resize larger.
	bbResizeBank(h, 128);
	CHECK(bbBankSize(h) == 128);

	// Data preserved after resize.
	CHECK(bbPeekInt(h, 0) == 42);
	CHECK(bbPeekInt(h, 4) == -123);

	// Resize smaller.
	bbResizeBank(h, 16);
	CHECK(bbBankSize(h) == 16);
	CHECK(bbPeekInt(h, 0) == 42);

	// Free.
	bbFreeBank(h);
	CHECK(bank_set.empty());
}

// ---- Test 2: freed bank handle with debug=true calls the hook ----
static void test_freed_handle_debug_hook() {
	reset_table();
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(bbBank) h = bbCreateBank(32);
	int old_handle = h.h;
	CHECK(old_handle != 0);

	// Free the bank.
	bbFreeBank(h);
	CHECK(bank_set.empty());

	// Use the old handle: should call the hook (stale handle).
	error_log.clear();
	BB_HANDLE(bbBank) stale(old_handle);
	bbBank *p = stale;  // triggers lookup
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug=false: no hook call.
	bb_handle_debug = false;
	error_log.clear();
	p = stale;
	CHECK(p == 0);
	CHECK(error_log.empty());
}

// ---- Test 3: multiple banks, independent handles ----
static void test_multiple_banks() {
	reset_table();
	bb_handle_debug = false;
	bb_handle_error = record_error;

	BB_HANDLE(bbBank) h1 = bbCreateBank(16);
	BB_HANDLE(bbBank) h2 = bbCreateBank(32);

	CHECK(h1.h != 0);
	CHECK(h2.h != 0);
	CHECK(h1.h != h2.h);

	bbPokeInt(h1, 0, 111);
	bbPokeInt(h2, 0, 222);

	CHECK(bbPeekInt(h1, 0) == 111);
	CHECK(bbPeekInt(h2, 0) == 222);

	// Free one, the other still works.
	bbFreeBank(h1);
	CHECK(bbPeekInt(h2, 0) == 222);
	bbFreeBank(h2);
}

// ---- Test 4: null handle ----
static void test_null_handle() {
	reset_table();

	BB_HANDLE(bbBank) h;
	CHECK(h.h == 0);
	bbBank *p = h;
	CHECK(p == 0);
}

// ---- Test 5: type mismatch — bank handle used as wrong type ----
static void test_type_mismatch() {
	reset_table();
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(bbBank) h = bbCreateBank(16);
	int bank_handle = h.h;

	// Try to look up with wrong type tag.
	void *p = g_handle_table.lookup(bank_handle, HT_ENTITY);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Handle type mismatch");

	bbFreeBank(h);
}

int main() {
	printf("bbBank handle-table integration tests\n");
	printf("=====================================\n");

	test_basic_lifecycle();
	printf("  create/poke/peek/resize/free: OK\n");

	test_freed_handle_debug_hook();
	printf("  freed handle + debug hook: OK\n");

	test_multiple_banks();
	printf("  multiple independent banks: OK\n");

	test_null_handle();
	printf("  null handle: OK\n");

	test_type_mismatch();
	printf("  type mismatch (debug on): OK\n");

	printf("=====================================\n");
	printf("%d checks, %d failed\n", tests_run, tests_failed);
	if (tests_failed) {
		printf("RESULT: FAIL\n");
		return 1;
	}
	printf("RESULT: PASS\n");
	return 0;
}

#else // _WIN32

int main() {
	printf("Windows build: bbbank handle tests not compiled (BB_HANDLE(T) = T*)\n");
	printf("RESULT: PASS (nothing to test)\n");
	return 0;
}

#endif // _WIN32
