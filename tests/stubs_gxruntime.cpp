
// Stubs for symbols that bbbank.cpp needs at link time but which are
// provided by the full gxruntime/bbruntime libraries on Windows.
// Used ONLY by test targets (test_bbbank) in the portable build.

#include "../gxruntime/gxruntime.h"
#include "../gxruntime/gxtimer.h"
#include "../gxruntime/gxfilesystem.h"
#include "../gxruntime/gxgraphics.h"
#include "../gxruntime/gxscene.h"
#include "../gxruntime/gxinput.h"
#include "../bbruntime/basic.h"
#include "../bbruntime/bbsys.h"
#include "../bbruntime/bbstream.h"
#ifndef _WIN32
#include "../bbruntime/bb_handle.h"
#endif
#include <set>
#include <string>

// The global debug flag (normally defined in bbruntime.cpp).
bool debug = false;

// The global gx_runtime pointer (normally set in bbruntime_run).
// The stub runtime is a real object: bbEx (bbsys.h) calls
// gx_runtime->debugError() when RTEX throws. It leaks for the lifetime of
// the test process (the ctor/dtor are private and closeRuntime needs the
// full Windows runtime), so it is created once and never destroyed.
gxRuntime *gx_runtime = 0;

// Stub ctor/dtor (normally in gxruntime/gxruntime_b3d.cpp, Windows-only).
// The dtor is private, so the object leaks for the lifetime of the test process.
gxRuntime::gxRuntime() {
	input = 0; graphics = 0; fileSystem = 0;
	enum_all = false; drivers.clear(); curr_driver = 0; use_di = 0;
	pointer_visible = true; app_close = "";
}
gxRuntime::~gxRuntime() {}

// Test helper: create the stub runtime (no-op if already created).
void stub_init_runtime() {
	if (!gx_runtime) gx_runtime = new gxRuntime();
}

// Stub implementation of gxRuntime methods that bbbank.cpp calls.
// These are only linked into test binaries, never into the real runtime.
void gxRuntime::debugError(const char *t) {
	(void)t;
}

// gx_runtime->debugLog is called by basic.cpp and the blitz3d loaders.
void gxRuntime::debugLog(const char *t) {
	(void)t;
}

int gxRuntime::callDll(const std::string &dll, const std::string &func,
	const void *in, int in_sz, void *out, int out_sz) {
	(void)dll; (void)func; (void)in; (void)in_sz; (void)out; (void)out_sz;
	return 0;
}

// NOTE: debugStream is now in bbruntime_core (bbstream.cpp), so no stub needed.

// gxFileSystem stubs (normally in gxruntime/gxfilesystem.cpp, which is
// Windows-only via <windows.h>). Used only by test binaries.
gxFileSystem::gxFileSystem() {}
gxFileSystem::~gxFileSystem() {}
bool gxFileSystem::createDir(const std::string &dir) { (void)dir; return false; }
bool gxFileSystem::deleteDir(const std::string &dir) { (void)dir; return false; }
bool gxFileSystem::createFile(const std::string &file) { (void)file; return false; }
bool gxFileSystem::deleteFile(const std::string &file) { (void)file; return false; }
bool gxFileSystem::copyFile(const std::string &src, const std::string &dest) { (void)src; (void)dest; return false; }
bool gxFileSystem::renameFile(const std::string &src, const std::string &dest) { (void)src; (void)dest; return false; }
bool gxFileSystem::setCurrentDir(const std::string &dir) { (void)dir; return false; }
std::string gxFileSystem::getCurrentDir()const { return "."; }
int gxFileSystem::getFileSize(const std::string &name)const { (void)name; return 0; }
int gxFileSystem::getFileType(const std::string &name)const { (void)name; return FILE_TYPE_NONE; }
gxDir *gxFileSystem::openDir(const std::string &name, int flags) { (void)name; (void)flags; return 0; }
gxDir *gxFileSystem::verifyDir(gxDir *d) { return d; }
void gxFileSystem::closeDir(gxDir *dir) { (void)dir; }

// gxDir stub (normally in gxruntime/gxdir.cpp, Windows-only).
gxDir::~gxDir() {}
std::string gxDir::getNextFile() { return ""; }

// gxRuntime filesystem stubs (normally in gxruntime.cpp, Windows-only).
gxFileSystem *gxRuntime::openFileSystem(int flags) {
	(void)flags;
	static gxFileSystem fs;
	return &fs;
}
void gxRuntime::closeFileSystem(gxFileSystem *filesys) { (void)filesys; }

// gxTimer stubs (normally in gxruntime/gxtimer.cpp, Windows-only via Win32 timers).
// Used only by test binaries.
static int g_stub_timer_count = 0;

gxTimer::gxTimer(gxRuntime *rt, int hertz) : runtime(rt), ticks_put(0), ticks_get(0) {
	(void)hertz;
	++g_stub_timer_count;
}
gxTimer::~gxTimer() {}
int gxTimer::wait() {
	// Simulate one tick per call.
	++ticks_put;
	int n = ticks_put - ticks_get;
	ticks_get += n;
	return n;
}

// gxRuntime timer stubs (normally in gxruntime_b3d.cpp, Windows-only).
gxTimer *gxRuntime::createTimer(int hertz) {
	return new gxTimer(this, hertz);
}
void gxRuntime::freeTimer(gxTimer *timer) {
	delete timer;
}
bool gxRuntime::idle() {
	return true; // program should continue
}

// BBStr stubs (normally in basic.cpp).
BBStr::BBStr() : std::string(), next(0), prev(0) {}
BBStr::BBStr(const char *s) : std::string(s), next(0), prev(0) {}
BBStr::BBStr(const char *s, int n) : std::string(s, n), next(0), prev(0) {}
BBStr::BBStr(const BBStr &s) : std::string(s), next(0), prev(0) {}
BBStr::BBStr(const std::string &s) : std::string(s), next(0), prev(0) {}
BBStr &BBStr::operator=(const char *s) { std::string::operator=(s); return *this; }
BBStr &BBStr::operator=(const BBStr &s) { std::string::operator=(s); return *this; }
BBStr &BBStr::operator=(const std::string &s) { std::string::operator=(s); return *this; }
BBStr::~BBStr() {}

void *BBStr::operator new(size_t size) {
	return ::operator new(size);
}
void BBStr::operator delete(void *q) {
	::operator delete(q);
}

// ==========================================================================
// Graphics stubs (normally in gxruntime/gxgraphics.cpp etc., Windows-only).
// Needed only so bbgraphics.cpp links into the test binaries. The stub
// canvases keep a real CPU pixel store so future graphics tests can
// round-trip pixels through Lock/WritePixel/CopyRect.

// blitz3d_open/close are defined in bbblitz3d.cpp. When that file is linked
// (test_bbblitz3d) these stubs would duplicate it; otherwise they provide the
// symbols bbgraphics.cpp needs.
#ifndef STUBS_NO_BLITZ3D_OPEN_CLOSE
void blitz3d_open() {}
void blitz3d_close() {}
#endif

// bbinput.cpp globals referenced by bbInput() (bbinput.cpp is Windows-only).
gxInput *gx_input = 0;
gxDevice *gx_mouse = 0;
gxDevice *gx_keyboard = 0;
std::vector<gxDevice*> gx_joysticks;


// Base ctor/dtor definitions (normally in gxruntime/gxcanvas.cpp etc.,
// Windows-only). Member initializers are zeroed; the stubs below never
// touch the native-surface members.

// PixelFormat::~PixelFormat (normally in gxruntime/ddutil.cpp, which uses
// VirtualFree and is not compiled on macOS). Stub canvases never call
// setFormat, so plot_code stays null.
PixelFormat::~PixelFormat() {}
gxCanvas::gxCanvas(gxGraphics *graphics, ddSurf *surface, int flags)
	: graphics(graphics), main_surf(surface), surf(0), z_surf(0), flags(flags), cube_mode(0),
	  mod_cnt(0), t_surf(0), locked_pitch(0), locked_cnt(0), lock_mod_cnt(0), remip_cnt(0),
	  locked_surf(0), cm_pitch(0), cm_mask(0), font(0), origin_x(0), origin_y(0),
	  handle_x(0), handle_y(0), color_argb(0xffffffff), clsColor_surf(0) {
	for (int i = 0; i < 6; ++i) cube_surfs[i] = 0;
}
gxCanvas::~gxCanvas() {}

gxGraphics::gxGraphics(gxRuntime *runtime, ddSurf *front, ddSurf *back, bool d3d)
	: runtime(runtime), front_canvas(0), back_canvas(0), def_font(0), gfx_lost(false), dummy_mesh(0) {
	(void)front; (void)back; (void)d3d;
}
gxGraphics::~gxGraphics() {
#ifndef _WIN32
	// Forget the canvases and default font (the real dtor is Windows-only).
	g_handle_table.forget( front_canvas );
	g_handle_table.forget( back_canvas );
	g_handle_table.forget( def_font );
#endif
}

gxFont::gxFont(gxGraphics *graphics, gxCanvas *canvas, int width, int height,
	int begin_char, int end_char, int def_char, int *offs, int *widths)
	: graphics(graphics), canvas(canvas), t_canvas(0), width(width), height(height),
	  begin_char(begin_char), end_char(end_char), def_char(def_char), offs(offs), widths(widths) {}
gxFont::~gxFont() {}

gxMovie::gxMovie(gxGraphics *gfx) : playing(false), gfx(gfx), canvas(0) {
	src_rect.left = src_rect.top = src_rect.right = src_rect.bottom = 0;
}
gxMovie::~gxMovie() {}

// ---- gxCanvas stub: real CPU pixel store, ARGB packed in unsigned ----
// gxCanvas has no virtual destructor, so the registry below is used to
// recover the StubCanvas from a base pointer instead of dynamic_cast.
class StubCanvas;
static std::set<gxCanvas*> g_stub_canvases;

class StubCanvas : public gxCanvas {
	unsigned int *pixels;
	int w, h;
public:
	StubCanvas(int width, int height)
		: gxCanvas(0, 0, 0), pixels(new unsigned int[width * height]()), w(width), h(height) {
		g_stub_canvases.insert(this);
	}
	~StubCanvas() { g_stub_canvases.erase(this); delete[] pixels; }

	static StubCanvas *asStub(gxCanvas *c) {
		return g_stub_canvases.count(c) ? (StubCanvas*)c : 0;
	}

	int getWidth()const { return w; }
	int getHeight()const { return h; }
	int getDepth()const { return 32; }

	void setOrigin(int x, int y) { (void)x; (void)y; }
	void setHandle(int x, int y) { (void)x; (void)y; }
	void setViewport(int x, int y, int ww, int hh) { (void)x; (void)y; (void)ww; (void)hh; }
	void getOrigin(int *x, int *y)const { *x = 0; *y = 0; }
	void getHandle(int *x, int *y)const { *x = 0; *y = 0; }
	void getViewport(int *x, int *y, int *ww, int *hh)const { *x = 0; *y = 0; *ww = w; *hh = h; }

	unsigned getMask()const { return 0; }
	void setMask(unsigned argb) { (void)argb; }
	unsigned getColor()const { return 0xffffffff; }
	void setColor(unsigned argb) { (void)argb; }
	unsigned getClsColor()const { return 0; }
	void setClsColor(unsigned argb) { (void)argb; }
	void setFont(gxFont *font) { (void)font; }

	bool lock()const { return true; }
	void unlock()const {}
	void setPixel(int x, int y, unsigned argb) { if (x >= 0 && x < w && y >= 0 && y < h) pixels[y * w + x] = argb; }
	unsigned getPixel(int x, int y)const { return (x >= 0 && x < w && y >= 0 && y < h) ? pixels[y * w + x] : 0; }
	void copyPixel(int x, int y, gxCanvas *src, int sx, int sy) {
		if (StubCanvas *s = asStub(src)) setPixel(x, y, s->getPixel(sx, sy));
	}
	void blit(int x, int y, gxCanvas *src, int sx, int sy, int sw, int sh, bool solid) {
		if (StubCanvas *s = asStub(src)) {
			for (int j = 0; j < sh; ++j)
				for (int i = 0; i < sw; ++i) {
					unsigned rgb = s->getPixel(sx + i, sy + j);
					if (!solid && !(rgb >> 24)) continue;
					setPixel(x + i, y + j, rgb);
				}
		}
	}
	bool collide(int x, int y, const gxCanvas *src, int sx, int sy, bool solid)const {
		(void)x; (void)y; (void)src; (void)sx; (void)sy; (void)solid; return false;
	}
	bool rect_collide(int x, int y, int rx, int ry, int rw, int rh, bool solid)const {
		(void)x; (void)y; (void)rx; (void)ry; (void)rw; (void)rh; (void)solid; return false;
	}
	void cls() {}
	void plot(int x, int y) { setPixel(x, y, 0xffffffff); }
	void line(int x, int y, int x2, int y2) { (void)x; (void)y; (void)x2; (void)y2; }
	void rect(int x, int y, int ww, int hh, bool solid) { (void)x; (void)y; (void)ww; (void)hh; (void)solid; }
	void oval(int x, int y, int ww, int hh, bool solid) { (void)x; (void)y; (void)ww; (void)hh; (void)solid; }
	void text(int x, int y, const std::string &t) { (void)x; (void)y; (void)t; }
	void backup()const {}
};

// ---- gxFont stub ----
class StubFont : public gxFont {
	int hgt;
public:
	StubFont(int height) : gxFont(0, 0, 8, height, 32, 126, 63, 0, 0), hgt(height) {}
	int getWidth()const { return 8; }
	int getHeight()const { return hgt; }
	int getWidth(const std::string &text)const { return 8 * (int)text.size(); }
	bool isPrintable(int chr)const { return chr >= 32 && chr < 127; }
};

// ---- gxMovie stub ----
class StubMovie : public gxMovie {
public:
	StubMovie() : gxMovie(0) {}
	bool draw(gxCanvas *dest, int x, int y, int w, int h) { (void)dest; (void)x; (void)y; (void)w; (void)h; return false; }
};

// ---- gxGraphics stub: real canvas/font/movie objects so the handle table
// ---- can track them (forget() must find what was allocated). ----
class StubGraphics : public gxGraphics {
	gxCanvas *front, *back;
	gxFont *def_font;
public:
	StubGraphics(int w, int h)
		: gxGraphics(0, 0, 0, false), front(new StubCanvas(w, h)), back(new StubCanvas(w, h)), def_font(new StubFont(12)) {
			// Populate the PRIVATE base members: the base accessors (which
			// bbgraphics.cpp calls through a gxGraphics*) read these.
			front_canvas = front;
			back_canvas = back;
			gxGraphics::def_font = def_font;
		}
	~StubGraphics() { delete front; delete back; delete def_font; }

	int getWidth()const { return front->getWidth(); }
	int getHeight()const { return front->getHeight(); }
	int getDepth()const { return 32; }
	int getScanLine()const { return front->getWidth() * 4; }
	int getAvailVidmem()const { return 1024 * 1024; }
	int getTotalVidmem()const { return 16 * 1024 * 1024; }

	gxCanvas *getFrontCanvas()const { return front; }
	gxCanvas *getBackCanvas()const { return back; }
	gxFont *getDefaultFont()const { return def_font; }

	gxCanvas *createCanvas(int width, int height, int flags) { (void)flags; return new StubCanvas(width, height); }
	gxCanvas *loadCanvas(const std::string &file, int flags) { (void)file; (void)flags; return 0; }
	gxCanvas *verifyCanvas(gxCanvas *canvas) { return canvas; }
	void freeCanvas(gxCanvas *canvas) { delete canvas; }

	gxFont *loadFont(const std::string &font, int height, int flags) { (void)font; (void)flags; return new StubFont(height); }
	gxFont *verifyFont(gxFont *font) { return font; }
	void freeFont(gxFont *font) { delete font; }

	gxMovie *openMovie(const std::string &file, int flags) { (void)file; (void)flags; return new StubMovie(); }
	gxMovie *verifyMovie(gxMovie *movie) { return movie; }
	void closeMovie(gxMovie *movie) { delete movie; }

	void vwait() {}
	void flip(bool vwait) { (void)vwait; }
	void setGamma(int r, int g, int b, float dr, float dg, float db) { (void)r; (void)g; (void)b; (void)dr; (void)dg; (void)db; }
	void getGamma(int r, int g, int b, float *dr, float *dg, float *db) {
		(void)r; (void)g; (void)b; *dr = *dg = *db = 1.0f;
	}
	void updateGamma(bool calibrate) { (void)calibrate; }
};

// ---- Base-class method definitions for the non-Windows build ----
// The gx* classes have no vtable, so calls through a base pointer bind to
// the base methods at compile time. bbgraphics.cpp calls through
// gxGraphics*/gxCanvas*/gxFont*/gxMovie*, so the base definitions are what
// the test binaries need. (The Stub* definitions above are only reachable
// through Stub* pointers; they exist for future pixel-level tests.)

int gxGraphics::getWidth()const { return front_canvas->getWidth(); }
int gxGraphics::getHeight()const { return front_canvas->getHeight(); }
int gxGraphics::getDepth()const { return front_canvas->getDepth(); }
int gxGraphics::getScanLine()const { return front_canvas->getWidth() * 4; }
int gxGraphics::getAvailVidmem()const { return 1024 * 1024; }
int gxGraphics::getTotalVidmem()const { return 16 * 1024 * 1024; }
gxCanvas *gxGraphics::getFrontCanvas()const { return front_canvas; }
gxCanvas *gxGraphics::getBackCanvas()const { return back_canvas; }
gxFont *gxGraphics::getDefaultFont()const { return def_font; }

gxCanvas *gxGraphics::createCanvas(int width, int height, int flags) { (void)flags; return new StubCanvas(width, height); }
gxCanvas *gxGraphics::loadCanvas(const std::string &file, int flags) { (void)file; (void)flags; return 0; }
gxCanvas *gxGraphics::verifyCanvas(gxCanvas *canvas) { return canvas; }
void gxGraphics::freeCanvas(gxCanvas *canvas) { delete canvas; }

gxFont *gxGraphics::loadFont(const std::string &font, int height, int flags) { (void)font; (void)flags; return new StubFont(height); }
gxFont *gxGraphics::verifyFont(gxFont *font) { return font; }
void gxGraphics::freeFont(gxFont *font) { delete font; }

gxMovie *gxGraphics::openMovie(const std::string &file, int flags) { (void)file; (void)flags; return new StubMovie(); }
gxMovie *gxGraphics::verifyMovie(gxMovie *movie) { return movie; }
void gxGraphics::closeMovie(gxMovie *movie) { delete movie; }

void gxGraphics::vwait() {}
void gxGraphics::flip(bool vwait) { (void)vwait; }
void gxGraphics::setGamma(int r, int g, int b, float dr, float dg, float db) { (void)r; (void)g; (void)b; (void)dr; (void)dg; (void)db; }
void gxGraphics::getGamma(int r, int g, int b, float *dr, float *dg, float *db) {
	(void)r; (void)g; (void)b; *dr = *dg = *db = 1.0f;
}
void gxGraphics::updateGamma(bool calibrate) { (void)calibrate; }

// gxCanvas base methods used by bbgraphics.cpp.
bool gxCanvas::lock()const { return true; }
void gxCanvas::unlock()const {}
void gxCanvas::setOrigin(int x, int y) { (void)x; (void)y; }
void gxCanvas::setHandle(int x, int y) { (void)x; (void)y; }
void gxCanvas::setViewport(int x, int y, int ww, int hh) { (void)x; (void)y; (void)ww; (void)hh; }
void gxCanvas::getOrigin(int *x, int *y)const { *x = 0; *y = 0; }
void gxCanvas::getHandle(int *x, int *y)const { *x = 0; *y = 0; }
void gxCanvas::getViewport(int *x, int *y, int *ww, int *hh)const { (void)x; (void)y; (void)ww; (void)hh; }
unsigned gxCanvas::getMask()const { return 0; }
void gxCanvas::setMask(unsigned argb) { (void)argb; }
void gxCanvas::setCubeMode(int mode) { (void)mode; }
void gxCanvas::setCubeFace(int face) { (void)face; }
unsigned gxCanvas::getColor()const { return 0xffffffff; }
void gxCanvas::setColor(unsigned argb) { (void)argb; }
unsigned gxCanvas::getClsColor()const { return 0; }
void gxCanvas::setClsColor(unsigned argb) { (void)argb; }
void gxCanvas::setFont(gxFont *font) { (void)font; }
void gxCanvas::backup()const {}
int gxCanvas::getWidth()const { return 16; }
int gxCanvas::getHeight()const { return 16; }
int gxCanvas::getDepth()const { return 32; }
void gxCanvas::setPixel(int x, int y, unsigned argb) { (void)x; (void)y; (void)argb; }
unsigned gxCanvas::getPixel(int x, int y)const { (void)x; (void)y; return 0; }
void gxCanvas::copyPixel(int x, int y, gxCanvas *src, int sx, int sy) { (void)x; (void)y; (void)src; (void)sx; (void)sy; }
void gxCanvas::copyPixelFast(int x, int y, gxCanvas *src, int sx, int sy) { (void)x; (void)y; (void)src; (void)sx; (void)sy; }
void gxCanvas::blit(int x, int y, gxCanvas *src, int sx, int sy, int sw, int sh, bool solid) {
	(void)x; (void)y; (void)src; (void)sx; (void)sy; (void)sw; (void)sh; (void)solid;
}
bool gxCanvas::collide(int x, int y, const gxCanvas *src, int sx, int sy, bool solid)const {
	(void)x; (void)y; (void)src; (void)sx; (void)sy; (void)solid; return false;
}
bool gxCanvas::rect_collide(int x, int y, int rx, int ry, int rw, int rh, bool solid)const {
	(void)x; (void)y; (void)rx; (void)ry; (void)rw; (void)rh; (void)solid; return false;
}
void gxCanvas::cls() {}
void gxCanvas::plot(int x, int y) { (void)x; (void)y; }
void gxCanvas::line(int x, int y, int x2, int y2) { (void)x; (void)y; (void)x2; (void)y2; }
void gxCanvas::rect(int x, int y, int ww, int hh, bool solid) { (void)x; (void)y; (void)ww; (void)hh; (void)solid; }
void gxCanvas::oval(int x, int y, int ww, int hh, bool solid) { (void)x; (void)y; (void)ww; (void)hh; (void)solid; }
void gxCanvas::text(int x, int y, const std::string &t) { (void)x; (void)y; (void)t; }

// gxFont base methods used by bbgraphics.cpp.
int gxFont::getWidth()const { return 8; }
int gxFont::getHeight()const { return height; }
int gxFont::getWidth(const std::string &text)const { return 8 * (int)text.size(); }
bool gxFont::isPrintable(int chr)const { return chr >= 32 && chr < 127; }

// gxMovie base methods used by bbgraphics.cpp.
bool gxMovie::draw(gxCanvas *dest, int x, int y, int w, int h) { (void)dest; (void)x; (void)y; (void)w; (void)h; return false; }

// ---- gxRuntime graphics stubs (normally in gxruntime_b3d.cpp, Windows-only) ----
gxGraphics *gxRuntime::openGraphics(int w, int h, int d, int driver, int flags) {
	(void)d; (void)driver; (void)flags;
	return new StubGraphics(w, h);
}
void gxRuntime::closeGraphics(gxGraphics *graphics) { delete graphics; }
bool gxRuntime::graphicsLost() { return false; }
int gxRuntime::numGraphicsDrivers() { return 1; }
void gxRuntime::graphicsDriverInfo(int driver, std::string *name, int *caps) {
	(void)driver; *name = "Stub"; *caps = gxRuntime::GFXMODECAPS_3D;
}
int gxRuntime::numGraphicsModes(int driver) { (void)driver; return 1; }
void gxRuntime::graphicsModeInfo(int driver, int mode, int *w, int *h, int *d, int *caps) {
	(void)driver; (void)mode; *w = 640; *h = 480; *d = 32; *caps = gxRuntime::GFXMODECAPS_3D;
}
void gxRuntime::windowedModeInfo(int *caps) { *caps = gxRuntime::GFXMODECAPS_3D; }
bool gxRuntime::delay(int ms) { (void)ms; return true; }
int gxRuntime::getMilliSecs() { return 0; }
void gxRuntime::setPointerVisible(bool vis) { (void)vis; }

// ---- gxInput / gxDevice stubs (normally in gxruntime/gxinput.cpp, Windows-only) ----
gxInput::gxInput(gxRuntime *rt) : runtime(rt) {}
gxInput::~gxInput() {}
int gxInput::toAscii(int key)const { (void)key; return 0; }

gxDevice::gxDevice() {
	for (int i = 0; i < 32; ++i) axis_states[i] = 0;
	for (int i = 0; i < 256; ++i) hit_count[i] = 0, down_state[i] = false;
	put = get = 0;
}
gxDevice::~gxDevice() {}
int gxDevice::getKey() { return 0; }
bool gxDevice::keyDown(int key) { (void)key; return false; }

// ---- gxScene stubs for the blitz3d test library (commit b) ----
// bbblitz3d.cpp and the blitz3d TUs call through gxScene*, so the base
// definitions below are what the test binaries link against. All state is
// no-op: tests exercise the handle table, not rendering.

gxScene::gxScene(gxGraphics *g, gxCanvas *t)
	: graphics(g), target(t), wbuffer(false), dither(false), antialias(false),
	  wireframe(false), flipped(false), ambient(0), ambient2(0), fogcolor(0),
	  caps_level(1), fogmode(FOG_NONE), zmode(ZMODE_NORMAL),
	  fogrange_nr(0), fogrange_fr(1), ortho_proj(false),
	  frustum_nr(0.1f), frustum_fr(1000.f), frustum_w(1), frustum_h(1),
	  shininess(0), blend(BLEND_REPLACE), fx(FX_VERTEXCOLOR), n_texs(0), tris_drawn(0) {
	for (int k = 0; k < MAX_TEXTURES; ++k) {
		texstate[k].canvas = 0;
		texstate[k].blend = BLEND_REPLACE;
		texstate[k].flags = 0;
		texstate[k].mat_valid = false;
	}
}
gxScene::~gxScene() {}

int gxScene::hwTexUnits() { return 1; }
int gxScene::gfxDriverCaps3D() { return 1; }
void gxScene::setWBuffer(bool e) { (void)e; }
void gxScene::setHWMultiTex(bool e) { (void)e; }
void gxScene::setDither(bool e) { (void)e; }
void gxScene::setAntialias(bool e) { (void)e; }
void gxScene::setWireframe(bool e) { (void)e; }
void gxScene::setFlippedTris(bool e) { (void)e; }
void gxScene::setAmbient(const float rgb[3]) { (void)rgb; }
void gxScene::setAmbient2(const float rgb[3]) { (void)rgb; }
void gxScene::setFogColor(const float rgb[3]) { (void)rgb; }
void gxScene::setFogRange(float nr, float fr) { (void)nr; (void)fr; }
void gxScene::setFogMode(int mode) { (void)mode; }
void gxScene::setZMode(int mode) { (void)mode; }
void gxScene::setViewport(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void gxScene::setOrthoProj(float nr, float fr, float nr_w, float nr_h) { (void)nr; (void)fr; (void)nr_w; (void)nr_h; }
void gxScene::setPerspProj(float nr, float fr, float nr_w, float nr_h) { (void)nr; (void)fr; (void)nr_w; (void)nr_h; }
void gxScene::setViewMatrix(const Matrix *m) { (void)m; }
void gxScene::setWorldMatrix(const Matrix *m) { (void)m; }
void gxScene::setRenderState(const RenderState &s) { (void)s; }
bool gxScene::begin(const std::vector<gxLight*> &lights) { (void)lights; return true; }
void gxScene::clear(const float rgb[3], float alpha, float z, bool clear_argb, bool clear_z) {
	(void)rgb; (void)alpha; (void)z; (void)clear_argb; (void)clear_z;
}
void gxScene::render(gxMesh *mesh, int first_vert, int vert_cnt, int first_tri, int tri_cnt) {
	(void)mesh; (void)first_vert; (void)vert_cnt; (void)first_tri; (void)tri_cnt;
}
void gxScene::end() {}
// gxLight base methods (blitz3d's Light ctor/dtor call through gxLight*).
gxLight::gxLight(gxScene *s, int type) : scene(s) { (void)type; }
gxLight::~gxLight() {}
void gxLight::setRange(float range) { (void)range; }
void gxLight::setColor(const float rgb[3]) { (void)rgb; }
void gxLight::getColor(float rgb[3]) { rgb[0] = rgb[1] = rgb[2] = 1; }
void gxLight::setPosition(const float pos[3]) { (void)pos; }
void gxLight::setDirection(const float dir[3]) { (void)dir; }
void gxLight::setConeAngles(float inner, float outer) { (void)inner; (void)outer; }

gxLight *gxScene::createLight(int flags) { (void)flags; return 0; }
void gxScene::freeLight(gxLight *l) { (void)l; }
int gxScene::getTrianglesDrawn()const { return tris_drawn; }
void gxScene::setEnabled(gxLight *light, bool enabled) { (void)light; (void)enabled; }

// gxGraphics scene factory methods.
gxScene *gxGraphics::createScene(int flags) {
	(void)flags;
	return new gxScene(this, front_canvas);
}
gxScene *gxGraphics::verifyScene(gxScene *scene) { return scene; }
void gxGraphics::freeScene(gxScene *scene) { delete scene; }

// gxGraphics canvas copy (blitz3d's cachedtexture.cpp calls through gxGraphics*).
void gxGraphics::copy(gxCanvas *dest, int dx, int dy, int dw, int dh,
                      gxCanvas *src, int sx, int sy, int sw, int sh) {
	(void)dest; (void)dx; (void)dy; (void)dw; (void)dh;
	(void)src; (void)sx; (void)sy; (void)sw; (void)sh;
}

// gxMesh: blitz3d allocates meshes via gxGraphics::createMesh and fills them
// through lock/setVertex/setTriangle/unlock. The stub keeps a plain CPU
// buffer so the whole path works without any graphics API.
gxMesh::gxMesh(gxGraphics *g, void *verts, unsigned short *indicies, int max_verts, int max_tris)
	: graphics(g), vertex_buff(0), tri_indices(0),
	  max_verts(max_verts), max_tris(max_tris), mesh_dirty(false), locked_verts(0) {
	(void)verts; (void)indicies;
}
gxMesh::~gxMesh() {
	if (vertex_buff) free(vertex_buff);
	if (tri_indices) free(tri_indices);
}
bool gxMesh::lock(bool all) {
	(void)all;
	if (!vertex_buff) vertex_buff = calloc(max_verts, 32);
	if (!tri_indices) tri_indices = (unsigned short*)calloc(max_tris * 3, sizeof(unsigned short));
	locked_verts = (gxMesh::dxVertex*)vertex_buff;
	return true;
}
void gxMesh::unlock() { locked_verts = 0; }
void gxMesh::render(int first_vert, int vert_cnt, int first_tri, int tri_cnt) {
	(void)first_vert; (void)vert_cnt; (void)first_tri; (void)tri_cnt;
}
void gxMesh::backup() {}
void gxMesh::restore() {}

gxMesh *gxGraphics::createMesh(int max_verts, int max_tris, int flags) {
	(void)flags;
	return new gxMesh(this, 0, 0, max_verts, max_tris);
}
void gxGraphics::freeMesh(gxMesh *mesh) { delete mesh; }
