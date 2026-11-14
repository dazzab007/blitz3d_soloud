
// Unit tests for the handle table (commit 1).
// Self-contained: defines mock types and specialises the traits.
// No graphics, audio, blitz3d, or gxruntime dependencies.

#include "../bbruntime/bb_handle.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// ---- Mock types mirroring the Entity family structure ----
struct MockEntity { int id; };
struct MockCamera : MockEntity {};
struct MockLight  : MockEntity {};
struct MockBank   { int id; };

// Specialise handle_family: MockCamera/MockLight -> MockEntity (like Camera/Light -> Entity)
template <> struct handle_family<MockEntity> { typedef MockEntity base; };
template <> struct handle_family<MockCamera> { typedef MockEntity base; };
template <> struct handle_family<MockLight>  { typedef MockEntity base; };

// Specialise handle_type_tag: all entity-family mocks -> HT_ENTITY
template <> struct handle_type_tag<MockEntity> { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MockCamera> { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MockLight>  { static const int value = HT_ENTITY; };

// Bank family (for type-mismatch tests) — uses primary template (base = MockBank).
template <> struct handle_type_tag<MockBank> { static const int value = HT_BANK; };

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
	++tests_run; \
	if (!(cond)) { \
		++tests_failed; \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while (0)

// ---- Error hook recorder ----
static std::vector<std::string> error_log;

static void record_error(const char *msg) {
	error_log.push_back(msg ? msg : "(null)");
}

// Reset the handle table to a clean state.
// Index 0 is reserved (handle 0 = null), so we keep one dummy entry at index 0.
static void reset_table() {
	g_handle_table.by_handle.clear();
	g_handle_table.by_handle.push_back(HandleTable::Entry()); // index 0 = reserved
	g_handle_table.by_ptr.clear();
	g_handle_table.free_list_head = 0;
}

// Helper: lookup and cast to MockEntity* in one step.
static MockEntity *lookup_ent(int h, int tag) {
	return static_cast<MockEntity *>(g_handle_table.lookup(h, tag));
}

// ---- Test 1: null pointer <-> handle 0 ----
static void test_null_pointer() {
	reset_table();

	bbH<MockEntity> h_null;
	CHECK(h_null.h == 0);
	MockEntity *p = h_null;
	CHECK(p == 0);

	bbH<MockEntity> h_from_zero(0);
	CHECK(h_from_zero.h == 0);

	MockEntity *null_ptr = 0;
	bbH<MockEntity> h2(null_ptr);
	CHECK(h2.h == 0);
}

// ---- Test 2: same object via Entity* and subclass* -> same handle ----
static void test_same_object_different_pointers() {
	reset_table();

	MockCamera cam;
	cam.id = 42;

	// Get handle via MockEntity* (base)
	MockEntity *base_ptr = &cam;
	bbH<MockEntity> h_base(base_ptr);

	// Get handle via MockCamera* (subclass)
	MockCamera *sub_ptr = &cam;
	bbH<MockCamera> h_sub(sub_ptr);

	// Both should produce the same handle value.
	CHECK(h_base.h != 0);
	CHECK(h_base.h == h_sub.h);

	// And both should resolve to the same object.
	MockEntity *p1 = h_base;
	MockCamera *p2 = h_sub;
	CHECK(p1 == &cam);
	CHECK(p2 == &cam);
	CHECK(p1->id == 42);
}

// ---- Test 3: forget then reuse slot -> old handle fails (generation) ----
static void test_stale_handle_after_reuse() {
	reset_table();

	MockEntity a, b;
	a.id = 1; b.id = 2;

	// Allocate handle for 'a' (gets index 1, gen 1)
	bbH<MockEntity> h_a(&a);
	int old_handle = h_a.h;
	CHECK(old_handle != 0);
	unsigned old_idx = (unsigned)old_handle >> 11;
	unsigned old_gen = (unsigned)old_handle & 0x7FF;
	CHECK(old_idx == 1);
	CHECK(old_gen == 1);

	// Forget 'a' (frees the slot, adds to free list)
	g_handle_table.forget(&a);

	// Allocate handle for 'b' — reuses index 1 from free list, gen increments to 2
	bbH<MockEntity> h_b(&b);
	int new_handle = h_b.h;
	CHECK(new_handle != 0);

	unsigned new_idx = (unsigned)new_handle >> 11;
	unsigned new_gen = (unsigned)new_handle & 0x7FF;

	// Same index, different generation.
	CHECK(old_idx == new_idx);
	CHECK(old_gen != new_gen);
	CHECK(new_gen == 2);

	// Old handle should fail lookup (stale generation: slot now has gen 2).
	CHECK(lookup_ent(old_handle, HT_ENTITY) == 0);

	// New handle should work.
	MockEntity *p_new = lookup_ent(new_handle, HT_ENTITY);
	CHECK(p_new == &b);
	CHECK(p_new->id == 2);
}

// ---- Test 4: generation wrap (2047 -> 1) ----
static void test_generation_wrap() {
	reset_table();

	// Set up a FREE slot at index 1 with generation 2047 (simulating 2047 prior uses).
	// base_ptr == 0 marks it as free; generation persists for the wrap.
	g_handle_table.by_handle.resize(2);
	g_handle_table.by_handle[1].generation = 2047;
	g_handle_table.by_handle[1].base_ptr   = 0;  // free
	g_handle_table.by_handle[1].type_tag   = 0;
	g_handle_table.by_handle[1].next_free  = 0;
	// Add to free list.
	g_handle_table.free_list_head = 1;

	MockEntity obj;
	obj.id = 99;

	// Allocate: should find index 1 from free list, increment gen 2047 -> wrap to 1.
	int h = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);

	unsigned idx = (unsigned)h >> 11;
	unsigned gen = (unsigned)h & 0x7FF;
	CHECK(idx == 1);
	CHECK(gen == 1);  // wrapped from 2047 to 1

	// Handle should be positive (bit 31 is 0).
	CHECK(h > 0);

	// Lookup should work.
	CHECK(lookup_ent(h, HT_ENTITY) == &obj);
}

// ---- Test 4b: handles stay positive at high index ----
static void test_high_index_positive() {
	reset_table();

	const int N = 100;
	MockEntity objs[N];
	int handles[N];

	for (int i = 0; i < N; ++i) {
		objs[i].id = i;
		handles[i] = g_handle_table.lookup_or_alloc(&objs[i], HT_ENTITY);
		CHECK(handles[i] > 0);  // all handles positive
	}

	// The last handle should have a high index.
	unsigned last_idx = (unsigned)handles[N-1] >> 11;
	CHECK(last_idx == N);  // indices are 1..N

	// All lookups should work.
	for (int i = 0; i < N; ++i) {
		MockEntity *p = lookup_ent(handles[i], HT_ENTITY);
		CHECK(p == &objs[i]);
		CHECK(p->id == i);
	}
}

// ---- Test 5: family type mismatch -> null with debug=false, hook called with debug=true ----
static void test_type_mismatch() {
	reset_table();

	MockEntity ent;
	ent.id = 7;

	int h = g_handle_table.lookup_or_alloc(&ent, HT_ENTITY);
	CHECK(h != 0);

	// With debug=false: should return null, no hook call.
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();
	CHECK(lookup_ent(h, HT_BANK) == 0);
	CHECK(error_log.empty());  // no error recorded

	// With debug=true: should return null AND call the hook.
	bb_handle_debug = true;
	error_log.clear();
	CHECK(lookup_ent(h, HT_BANK) == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Handle type mismatch");

	// Correct type tag still works.
	bb_handle_debug = false;
	MockEntity *p_ok = lookup_ent(h, HT_ENTITY);
	CHECK(p_ok == &ent);
	CHECK(p_ok->id == 7);
}

// ---- Test 5b: stale handle with debug=true calls the hook ----
static void test_stale_handle_hook() {
	reset_table();

	MockEntity a, b;
	a.id = 1; b.id = 2;

	bbH<MockEntity> h_a(&a);
	int old_handle = h_a.h;

	g_handle_table.forget(&a);
	bbH<MockEntity> h_b(&b);  // reuses slot, new generation

	// With debug=true: stale handle should call the hook.
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();
	CHECK(lookup_ent(old_handle, HT_ENTITY) == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug=false: no hook call.
	bb_handle_debug = false;
	error_log.clear();
	CHECK(lookup_ent(old_handle, HT_ENTITY) == 0);
	CHECK(error_log.empty());
}

// ---- Test 6: one handle per object (reverse map) ----
static void test_one_handle_per_object() {
	reset_table();

	MockEntity obj;
	obj.id = 5;

	int h1 = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);
	int h2 = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);
	int h3 = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);

	CHECK(h1 == h2);
	CHECK(h2 == h3);

	// Only one entry in by_ptr.
	CHECK(g_handle_table.by_ptr.size() == 1);
}

// ---- Test 7: forget removes from both maps ----
static void test_forget_removes_entry() {
	reset_table();

	MockEntity obj;
	obj.id = 3;

	int h = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);
	CHECK(g_handle_table.by_ptr.size() == 1);

	g_handle_table.forget(&obj);
	CHECK(g_handle_table.by_ptr.size() == 0);

	// Slot is free (base_ptr == 0). Generation persists for wrap.
	unsigned idx = (unsigned)h >> 11;
	CHECK(g_handle_table.by_handle[idx].base_ptr == 0);
	CHECK(g_handle_table.by_handle[idx].type_tag == 0);

	// Handle no longer resolves.
	CHECK(lookup_ent(h, HT_ENTITY) == 0);
}

// ---- Test 8: forget of unknown pointer is a no-op ----
static void test_forget_unknown() {
	reset_table();

	MockEntity obj;
	obj.id = 1;
	g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);

	MockEntity other;
	other.id = 2;
	g_handle_table.forget(&other);  // should not crash or affect 'obj'

	CHECK(g_handle_table.by_ptr.size() == 1);
	int h = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);
	CHECK(lookup_ent(h, HT_ENTITY) == &obj);
}

// ---- Test 9: handle layout — bit 31 is always 0 ----
static void test_handle_layout() {
	reset_table();

	MockEntity obj;
	obj.id = 1;
	int h = g_handle_table.lookup_or_alloc(&obj, HT_ENTITY);

	// Bit 31 must be 0 (handle is positive).
	CHECK((h & 0x80000000) == 0);
	CHECK(h > 0);

	// Index is in bits 30..11.
	unsigned idx = (unsigned)h >> 11;
	CHECK(idx >= 1);  // index 0 is reserved

	// Generation is in bits 10..0, must be 1..2047.
	unsigned gen = (unsigned)h & 0x7FF;
	CHECK(gen >= 1 && gen <= 2047);
}

// ---- Test 10: free list — allocate/free/allocate reuses slots in order ----
static void test_free_list() {
	reset_table();

	MockEntity a, b, c;
	a.id = 1; b.id = 2; c.id = 3;

	// Allocate 3 objects: indices 1, 2, 3.
	int ha = g_handle_table.lookup_or_alloc(&a, HT_ENTITY);
	int hb = g_handle_table.lookup_or_alloc(&b, HT_ENTITY);
	int hc = g_handle_table.lookup_or_alloc(&c, HT_ENTITY);

	CHECK((unsigned)ha >> 11 == 1);
	CHECK((unsigned)hb >> 11 == 2);
	CHECK((unsigned)hc >> 11 == 3);

	// Free middle and first: free list should be [1, 2] (LIFO: 1 pushed after 2).
	g_handle_table.forget(&b);
	g_handle_table.forget(&a);

	// Allocate 2 more: should reuse indices 1 and 2 (from free list, LIFO order).
	MockEntity d, e;
	d.id = 4; e.id = 5;
	int hd = g_handle_table.lookup_or_alloc(&d, HT_ENTITY);
	int he = g_handle_table.lookup_or_alloc(&e, HT_ENTITY);

	// Free list was [1, 2] (head=1, 1->2). So d gets index 1, e gets index 2.
	CHECK((unsigned)hd >> 11 == 1);
	CHECK((unsigned)he >> 11 == 2);

	// All lookups work.
	CHECK(lookup_ent(hd, HT_ENTITY) == &d);
	CHECK(lookup_ent(he, HT_ENTITY) == &e);
	CHECK(lookup_ent(hc, HT_ENTITY) == &c);  // c was never freed
}

// ---- Test 11: capacity check — out-of-range index triggers error hook ----
static void test_capacity_check() {
	reset_table();

	// Verify that lookup with an out-of-range index returns null and calls the hook.
	int fake_handle = (0xFFFFF << 11) | 1;  // max valid index, gen 1
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();
	CHECK(g_handle_table.lookup(fake_handle, HT_ENTITY) == 0);
	// Index 0xFFFFF is out of range (vector has only 1 entry), so hook should fire.
	CHECK(error_log.size() == 1);

	bb_handle_debug = false;
}

// ---- Test 12: allocate/free 10,000 objects ----
static void test_10k_alloc_free() {
	reset_table();

	const int N = 10000;
	MockEntity *objs = new MockEntity[N];
	int *handles = new int[N];

	// Allocate all.
	for (int i = 0; i < N; ++i) {
		objs[i].id = i;
		handles[i] = g_handle_table.lookup_or_alloc(&objs[i], HT_ENTITY);
		if (i % 1000 == 0) CHECK(handles[i] > 0);
	}
	CHECK(g_handle_table.by_ptr.size() == N);

	// Free all.
	for (int i = 0; i < N; ++i) {
		g_handle_table.forget(&objs[i]);
	}
	CHECK(g_handle_table.by_ptr.size() == 0);

	// Re-allocate all: should reuse the same slots from the free list.
	for (int i = 0; i < N; ++i) {
		handles[i] = g_handle_table.lookup_or_alloc(&objs[i], HT_ENTITY);
		if (i % 1000 == 0) CHECK(handles[i] > 0);
	}
	CHECK(g_handle_table.by_ptr.size() == N);

	// Verify all lookups work.
	int spot_checks = 0;
	for (int i = 0; i < N; i += 997) {
		MockEntity *p = lookup_ent(handles[i], HT_ENTITY);
		CHECK(p == &objs[i]);
		CHECK(p->id == i);
		++spot_checks;
	}
	CHECK(spot_checks > 10);  // at least ~10 spot checks

	delete[] objs;
	delete[] handles;
}

int main() {
	printf("Handle table unit tests\n");
	printf("=======================\n");

	test_null_pointer();
	printf("  null pointer <-> handle 0: OK\n");

	test_same_object_different_pointers();
	printf("  same object via base* and subclass*: OK\n");

	test_stale_handle_after_reuse();
	printf("  stale handle after slot reuse: OK\n");

	test_generation_wrap();
	printf("  generation wrap 2047->1: OK\n");

	test_high_index_positive();
	printf("  handles positive at high index: OK\n");

	test_type_mismatch();
	printf("  family type mismatch (debug on/off): OK\n");

	test_stale_handle_hook();
	printf("  stale handle hook (debug on/off): OK\n");

	test_one_handle_per_object();
	printf("  one handle per object: OK\n");

	test_forget_removes_entry();
	printf("  forget removes entry: OK\n");

	test_forget_unknown();
	printf("  forget unknown pointer (no-op): OK\n");

	test_handle_layout();
	printf("  handle layout (bit 31 = 0): OK\n");

	test_free_list();
	printf("  free list LIFO reuse: OK\n");

	test_capacity_check();
	printf("  capacity check / out-of-range index: OK\n");

	test_10k_alloc_free();
	printf("  10,000 alloc/free/realloc: OK\n");

	printf("=======================\n");
	printf("%d checks, %d failed\n", tests_run, tests_failed);
	if (tests_failed) {
		printf("RESULT: FAIL\n");
		return 1;
	}
	printf("RESULT: PASS\n");
	return 0;
}

#else // _WIN32

// On Windows, BB_HANDLE(T) is T* — no table to test.
int main() {
	printf("Windows build: handle table not compiled (BB_HANDLE(T) = T*)\n");
	printf("RESULT: PASS (nothing to test)\n");
	return 0;
}

#endif // _WIN32
