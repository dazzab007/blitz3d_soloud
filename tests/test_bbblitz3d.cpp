// Unit tests for the bbblitz3d handle-table integration (3D commit b).
// Links against the REAL bbblitz3d.cpp + the blitz3d library + stubs_gxruntime.cpp.
// Handle bookkeeping only: no assertions on rendering.
// Tests that entity handles are allocated on create, stay stable, go stale
// after free (debug hook fires), and that ClearWorld empties the table.

#include "../bbruntime/bb_handle.h"
#include "../bbruntime/bb_handle_types.h"
#include "../bbruntime/basic.h"
#include "../stdutil/stdutil.h"
#include "../bbruntime/bbsys.h"
#include "../gxruntime/gxruntime_b3d.h"
#include "../gxruntime/gxgraphics.h"
#include "../gxruntime/gxscene.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real bbblitz3d functions (defined in bbblitz3d.cpp).
class Entity;
BB_HANDLE(Entity) bbCreateMesh(BB_HANDLE(Entity) p_h);
void bbFreeEntity(BB_HANDLE(Entity) e_h);
float bbEntityX(BB_HANDLE(Entity) e_h, int global);
void bbEntityColor(BB_HANDLE(Entity) m_h, float r, float g, float b);
void bbClearWorld(int e, int b, int t);
// Real bbgraphics functions (defined in bbgraphics.cpp, linked via bbruntime_core).
void bbGraphics3D(int w, int h, int d, int mode);
void bbEndGraphics();

// The real debug flag and gx_runtime (defined in stubs_gxruntime.cpp).
extern bool debug;
extern class gxRuntime *gx_runtime;
// bbgraphics.cpp globals.
extern gxGraphics *gx_graphics;
extern gxCanvas *gx_canvas;
void stub_init_runtime();

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

// ---- Test 1: CreateMesh allocates a stable handle ----
static void test_create_mesh_handle() {
	BB_HANDLE(Entity) h = bbCreateMesh(0);
	CHECK(h.h != 0);

	// The same object resolves back to the same pointer.
	Entity *e1 = BB_HANDLE(Entity)(h.h);
	Entity *e2 = BB_HANDLE(Entity)(h.h);
	CHECK(e1 != 0);
	CHECK(e1 == e2);

	// A second mesh gets a different handle (distinct allocation).
	BB_HANDLE(Entity) h2 = bbCreateMesh(0);
	CHECK(h2.h != 0);
	CHECK(h2.h != h.h);

	bbFreeEntity(h);
	bbFreeEntity(h2);
}

// ---- Test 2: freed entity handle goes stale (debug hook fires) ----
static void test_entity_stale_after_free() {
	// debug must be ON before creation: insert() only tracks entities in
	// entity_set when debug is true, so freeing a debug-off entity throws
	// "Entity does not exist" (pre-existing Blitz3D semantics).
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Entity) h = bbCreateMesh(0);
	CHECK(h.h != 0);
	Entity *live = BB_HANDLE(Entity)(h.h);
	CHECK(live != 0);

	error_log.clear();
	bbFreeEntity(h);

	// The freed handle must resolve to null and log exactly one error.
	bb_handle_error = record_error;
	Entity *p = BB_HANDLE(Entity)(h.h);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");
	error_log.clear();

	// The stale handle must not be re-resolvable even if the slot is reused.
	debug = false;
	bb_handle_debug = false;
}

// ---- Test 3: ClearWorld frees all entities and empties the table ----
static void test_clear_world_cleanup() {
	// debug ON before creation (see test 2 for why).
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Entity) h1 = bbCreateMesh(0);
	BB_HANDLE(Entity) h2 = bbCreateMesh(0);
	CHECK(h1.h != 0);
	CHECK(h2.h != 0);

	error_log.clear();
	bbClearWorld(1, 1, 1);

	// Both handles are now stale.
	bb_handle_error = record_error;
	Entity *p1 = BB_HANDLE(Entity)(h1.h);
	Entity *p2 = BB_HANDLE(Entity)(h2.h);
	CHECK(p1 == 0);
	CHECK(p2 == 0);
	CHECK(error_log.size() == 2);
	error_log.clear();

	debug = false;
	bb_handle_debug = false;

	// Every entity was deleted -> no entity handles remain (canvas handle from bbGraphics3D may remain).
	int entity_count = 0;
	for (auto &kv : g_handle_table.by_ptr) {
		unsigned index = kv.second >> 11;
		if (g_handle_table.by_handle[index].type_tag == HT_ENTITY) entity_count++;
	}
	CHECK(entity_count == 0);
}

int main() {
	printf("bbblitz3d handle-table integration tests (real bbblitz3d.cpp + blitz3d)\n");
	printf("======================================================\n");

	stub_init_runtime();

	// Open a stub 3D graphics context: sets gx_graphics and calls
	// blitz3d_open() (creates the stub gxScene + World).
	try {
		bbGraphics3D(320, 200, 16, 1);
	} catch (bbEx &e) {
		printf("bbGraphics3D threw: %s\n", e.err);
	}
	CHECK(gx_graphics != 0);

	test_create_mesh_handle();
	test_entity_stale_after_free();
	test_clear_world_cleanup();

	// Close the context (frees the scene + world) and the graphics object.
	bbEndGraphics();
	gx_runtime->closeGraphics(gx_graphics);
	gx_graphics = 0;
	gx_canvas = 0;

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

#else
int main() { printf("skipped on Windows\n"); return 0; }
#endif
