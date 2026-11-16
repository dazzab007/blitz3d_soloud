// Unit tests for the bbgraphics handle-table integration.
// Links against the REAL bbgraphics.cpp (via bbruntime_core) + stubs_gxruntime.cpp.
// Handle bookkeeping only: no assertions on stub pixels.
// Tests that freed image/font/canvas handles go stale (debug hook fires),
// that live objects return stable handles, and that cleanup empties the table.

#include "../bbruntime/bb_handle.h"
#include "../bbruntime/bb_handle_types.h"
#include "../bbruntime/basic.h"
#include "../stdutil/stdutil.h"
#include "../gxruntime/gxruntime_b3d.h"
#include "../gxruntime/gxgraphics.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real bbgraphics functions (defined in bbgraphics.cpp).
class bbImage;
BB_HANDLE(bbImage) bbCreateImage(int w, int h, int n);
void bbFreeImage(BB_HANDLE(bbImage) i_h);
BB_HANDLE(gxCanvas) bbImageBuffer(BB_HANDLE(bbImage) i_h, int n);
BB_HANDLE(gxFont) bbLoadFont(BBStr *name, int height, int bold, int italic, int underline);
void bbFreeFont(BB_HANDLE(gxFont) f_h);
void bbEndGraphics();

// The real debug flag and gx_runtime (defined in stubs_gxruntime.cpp).
extern bool debug;
extern class gxRuntime *gx_runtime;
// bbgraphics.cpp globals (defined in stubs_gxruntime.cpp).
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

static void reset_table() {
	g_handle_table.by_handle.clear();
	g_handle_table.by_handle.push_back(HandleTable::Entry());
	g_handle_table.by_ptr.clear();
	g_handle_table.free_list_head = 0;
}

// ---- Test 1: CreateImage/FreeImage -> image handle is stale ----
static void test_image_stale_after_free() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(bbImage) h = bbCreateImage(4, 4, 1);
	int image_handle = h.h;
	CHECK(image_handle != 0);

	bbFreeImage(h);

	// With debug=true: using the old handle fires the stale-handle hook.
	debug = true;
	bb_handle_debug = true;
	error_log.clear();
	bbImage *p = BB_HANDLE(bbImage)(image_handle);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug=false: no hook call.
	debug = false;
	bb_handle_debug = false;
	error_log.clear();
	p = BB_HANDLE(bbImage)(image_handle);
	CHECK(p == 0);
	CHECK(error_log.empty());
}

// ---- Test 2: ImageBuffer returns the same handle twice, stale after FreeImage ----
static void test_image_buffer_stable_then_stale() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(bbImage) img = bbCreateImage(4, 4, 1);
	int image_handle = img.h;
	CHECK(image_handle != 0);

	BB_HANDLE(gxCanvas) c1 = bbImageBuffer(img, 0);
	BB_HANDLE(gxCanvas) c2 = bbImageBuffer(img, 0);
	CHECK(c1.h != 0);
	CHECK(c1.h == c2.h); // same canvas -> same handle (find-or-alloc)

	bbFreeImage(img);

	// The frame canvas was freed with the image: its handle is stale.
	debug = true;
	bb_handle_debug = true;
	error_log.clear();
	gxCanvas *c = BB_HANDLE(gxCanvas)(c1.h);
	CHECK(c == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");
}

// ---- Test 3: LoadFont/FreeFont -> font handle is stale after free ----
static void test_font_stale_after_free() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	BB_HANDLE(gxFont) f = bbLoadFont(d_new BBStr("Arial"), 12, 0, 0, 0);
	int font_handle = f.h;
	CHECK(font_handle != 0);

	bbFreeFont(f);

	debug = true;
	bb_handle_debug = true;
	error_log.clear();
	gxFont *p = BB_HANDLE(gxFont)(font_handle);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");
}

// ---- Test 4: EndGraphics forgets front/back handles; table empty after cleanup ----
static void test_end_graphics_cleanup() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	// Open a stub graphics context (sets gx_graphics/gx_canvas via bbEndGraphics).
	bbEndGraphics();

	CHECK(gx_graphics != 0);
	CHECK(gx_canvas != 0);

	BB_HANDLE(gxCanvas) front_h = BB_HANDLE(gxCanvas)(gx_graphics->getFrontCanvas());
	BB_HANDLE(gxCanvas) back_h = BB_HANDLE(gxCanvas)(gx_graphics->getBackCanvas());
	CHECK(front_h.h != 0);
	CHECK(back_h.h != 0);

	bbEndGraphics(); // closes the old context, opens a fresh one

	// The old front/back canvases were freed: their handles are stale.
	debug = true;
	bb_handle_debug = true;
	error_log.clear();
	gxCanvas *p = BB_HANDLE(gxCanvas)(front_h.h);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");
	error_log.clear();
	p = BB_HANDLE(gxCanvas)(back_h.h);
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// Full cleanup: close the final context (forgets its canvases + default font).
	debug = false;
	bb_handle_debug = false;
	error_log.clear();
	gx_runtime->closeGraphics(gx_graphics);
	gx_graphics = 0;
	gx_canvas = 0;

	// Every registered object has been freed -> the table is empty.
	CHECK(g_handle_table.by_ptr.empty());
}

int main() {
	printf("bbgraphics handle-table integration tests (real bbgraphics.cpp)\n");
	printf("======================================================\n");

	stub_init_runtime();

	test_image_stale_after_free();
	test_image_buffer_stable_then_stale();
	test_font_stale_after_free();
	test_end_graphics_cleanup();

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

#else
int main() { printf("skipped on Windows\n"); return 0; }
#endif
