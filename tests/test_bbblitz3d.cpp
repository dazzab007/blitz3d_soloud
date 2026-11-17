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
// blitz3d/std.h first: it provides `using namespace std;` and d_new, which
// the other blitz3d headers rely on.
#include "../blitz3d/std.h"
#include "../blitz3d/surface.h"
#include "../blitz3d/pivot.h"
#include "../blitz3d/meshmodel.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real bbblitz3d functions (defined in bbblitz3d.cpp).
class Entity;
BB_HANDLE(Entity) bbCreateMesh(BB_HANDLE(Entity) p_h);
BB_HANDLE(Entity) bbCreatePivot(BB_HANDLE(Entity) p_h);
void bbFreeEntity(BB_HANDLE(Entity) e_h);
float bbEntityX(BB_HANDLE(Entity) e_h, int global);
void bbEntityColor(BB_HANDLE(Entity) m_h, float r, float g, float b);
BB_HANDLE(Entity) bbGetParent(BB_HANDLE(Entity) e_h);
BB_HANDLE(Texture) bbCreateTexture(int w, int h, int flags, int frames);
void bbFreeTexture(BB_HANDLE(Texture) t_h);
BB_HANDLE(gxCanvas) bbTextureBuffer(BB_HANDLE(Texture) t_h, int frame);
BB_HANDLE(Brush) bbCreateBrush(float r, float g, float b);
void bbFreeBrush(BB_HANDLE(Brush) b_h);
BB_HANDLE(Brush) bbGetEntityBrush(BB_HANDLE(Entity) m_h);
BB_HANDLE(Surface) bbCreateSurface(BB_HANDLE(Entity) m_h, BB_HANDLE(Brush) b_h);
void bbClearWorld(int e, int b, int t);

// Handle -> pointer conversion (same logic as bbblitz3d.cpp's tH/eH, which
// are file-local there). On macOS these go through the handle table.
template <typename T> static inline T *tH(BB_HANDLE(T) h) { return static_cast<T*>(h); }
#ifndef _WIN32
template <typename T> static inline T *eH(BB_HANDLE(Entity) h) { return static_cast<T*>(static_cast<Entity*>(h)); }
#else
template <typename T> static inline T *eH(BB_HANDLE(Entity) h) { return static_cast<T*>(h); }
#endif

// Test-only accessors (friend-declared in the blitz3d headers; defined in
// stubs_gxruntime.cpp).
class EntityListTest {
public:
	static int orphanCount();
	static int childCount(Pivot *p);
};
class MeshSurfaceTest {
public:
	static Surface *first(MeshModel *m);
	static int count(MeshModel *m);
};
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

// ---- Test 4: parent/child pivots share one handle per object; freeing the
// ---- parent frees the child too (Entity::~Entity deletes its children) ----
static void test_parent_child_handles() {
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Entity) p = bbCreatePivot(0);
	BB_HANDLE(Entity) c = bbCreatePivot(p);
	CHECK(p.h != 0);
	CHECK(c.h != 0);
	CHECK(c.h != p.h);

	// One handle per object: GetParent resolves the child's parent to the
	// SAME handle as the one CreatePivot returned.
	BB_HANDLE(Entity) par = bbGetParent(c);
	CHECK(par.h == p.h);

	// The scene graph really has the parent/child relation (orphan + child).
	CHECK(EntityListTest::orphanCount() == 1);
	CHECK(EntityListTest::childCount((Pivot*)eH<Pivot>(p)) == 1);

	error_log.clear();
	bbFreeEntity(p);

	// Freeing the parent deleted the child too: BOTH handles are stale.
	bb_handle_error = record_error;
	Entity *pp = eH<Entity>(p);
	Entity *cc = eH<Entity>(c);
	CHECK(pp == 0);
	CHECK(cc == 0);
	CHECK(error_log.size() == 2);
	error_log.clear();

	debug = false;
	bb_handle_debug = false;
}

// ---- Test 5: texture + its frame canvas go stale after FreeTexture ----
static void test_texture_stale_after_free() {
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Texture) t = bbCreateTexture(16, 16, 0, 1);
	CHECK(t.h != 0);
	Texture *live = tH<Texture>(t);
	CHECK(live != 0);

	// TextureBuffer hands out the frame canvas as its own handle.
	BB_HANDLE(gxCanvas) buf = bbTextureBuffer(t, 0);
	CHECK(buf.h != 0);

	error_log.clear();
	bbFreeTexture(t);

	// Both the texture and its canvas are stale after the free.
	bb_handle_error = record_error;
	Texture *tt = tH<Texture>(t);
	gxCanvas *bb = tH<gxCanvas>(buf);
	CHECK(tt == 0);
	CHECK(bb == 0);
	CHECK(error_log.size() == 2);
	error_log.clear();

	debug = false;
	bb_handle_debug = false;
}

// ---- Test 6: brush handles; GetEntityBrush returns a freeable handle ----
static void test_brush_lifecycle() {
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Brush) b1 = bbCreateBrush(128, 0, 0);
	CHECK(b1.h != 0);
	Brush *live = tH<Brush>(b1);
	CHECK(live != 0);
	// The brush really carries the color we asked for (public accessor).
	const Vector &col = live->getColor();
	CHECK((int)(col.x * 255 + 0.5f) == 128);
	CHECK(col.y == 0 && col.z == 0);

	error_log.clear();
	bbFreeBrush(b1);

	bb_handle_error = record_error;
	Brush *bb = tH<Brush>(b1);
	CHECK(bb == 0);
	CHECK(error_log.size() == 1);
	error_log.clear();

	// GetEntityBrush copies the model's brush into a NEW brush object with its
	// own handle, which FreeBrush accepts.
	BB_HANDLE(Entity) m = bbCreateMesh(0);
	BB_HANDLE(Brush) b2 = bbGetEntityBrush(m);
	CHECK(b2.h != 0);
	CHECK(b2.h != b1.h);

	error_log.clear();
	bbFreeBrush(b2);
	bb_handle_error = record_error;
	Brush *bb2 = tH<Brush>(b2);
	CHECK(bb2 == 0);
	CHECK(error_log.size() == 1);
	error_log.clear();

	debug = false;
	bb_handle_debug = false;
}

// ---- Test 7: mesh surfaces go stale when the mesh entity is freed ----
static void test_surface_stale_after_mesh_free() {
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Entity) m1 = bbCreateMesh(0);
	BB_HANDLE(Brush) b1 = bbCreateBrush(255, 0, 0);
	BB_HANDLE(Surface) s1 = bbCreateSurface(m1, b1);
	CHECK(s1.h != 0);

	// A second mesh: its surface must be a distinct object (and handle).
	BB_HANDLE(Entity) m2 = bbCreateMesh(0);
	BB_HANDLE(Brush) b2 = bbCreateBrush(0, 255, 0);
	BB_HANDLE(Surface) s2 = bbCreateSurface(m2, b2);
	CHECK(s2.h != 0);
	CHECK(s2.h != s1.h);

	// The surfaces really belong to their meshes (MeshModel holds the list).
	CHECK(MeshSurfaceTest::first(eH<MeshModel>(m1)) == tH<Surface>(s1));
	CHECK(MeshSurfaceTest::count(eH<MeshModel>(m1)) == 1);

	error_log.clear();
	bbFreeEntity(m1);

	// Freeing the mesh deleted its surface: s1 is stale, s2 still resolves.
	bb_handle_error = record_error;
	Surface *ss1 = tH<Surface>(s1);
	Surface *ss2 = tH<Surface>(s2);
	CHECK(ss1 == 0);
	CHECK(ss2 != 0);
	CHECK(error_log.size() == 1);
	error_log.clear();

	// The mesh's own handle is stale too.
	bb_handle_error = record_error;
	Entity *mm = eH<Entity>(m1);
	CHECK(mm == 0);
	CHECK(error_log.size() == 1);
	error_log.clear();

	debug = false;
	bb_handle_debug = false;
}

// ---- Test 8: ClearWorld empties the whole handle table ----
static void test_clear_world_empties_table() {
	debug = true;
	bb_handle_debug = true;

	BB_HANDLE(Entity) p = bbCreatePivot(0);
	BB_HANDLE(Entity) c = bbCreatePivot(p);
	BB_HANDLE(Texture) t = bbCreateTexture(8, 8, 0, 1);
	BB_HANDLE(Brush) b = bbCreateBrush(255, 255, 255);
	BB_HANDLE(Entity) m = bbCreateMesh(0);
	BB_HANDLE(Surface) s = bbCreateSurface(m, b);
	CHECK(p.h && c.h && t.h && b.h && m.h && s.h);

	bbClearWorld(1, 1, 1);

	// Every handle above is stale.
	bb_handle_error = record_error;
	CHECK(eH<Entity>(p) == 0);
	CHECK(eH<Entity>(c) == 0);
	CHECK(tH<Texture>(t) == 0);
	CHECK(tH<Brush>(b) == 0);
	CHECK(eH<Entity>(m) == 0);
	CHECK(tH<Surface>(s) == 0);
	CHECK(error_log.size() == 6);
	error_log.clear();

	// The table should be empty except for the graphics context canvases
	// (front/back), which are only freed by bbEndGraphics, not bbClearWorld.
	int non_canvas_count = 0;
	for (auto &kv : g_handle_table.by_ptr) {
		unsigned index = kv.second >> 11;
		if (g_handle_table.by_handle[index].type_tag != HT_CANVAS) {
			++non_canvas_count;
		}
	}
	CHECK(non_canvas_count == 0);

	debug = false;
	bb_handle_debug = false;
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
	test_parent_child_handles();
	test_texture_stale_after_free();
	test_brush_lifecycle();
	test_surface_stale_after_mesh_free();
	test_clear_world_cleanup();
	test_clear_world_empties_table();

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
