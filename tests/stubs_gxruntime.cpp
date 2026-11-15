
// Stubs for symbols that bbbank.cpp needs at link time but which are
// provided by the full gxruntime/bbruntime libraries on Windows.
// Used ONLY by test targets (test_bbbank) in the portable build.

#include "../gxruntime/gxruntime.h"
#include "../bbruntime/basic.h"
#include "../bbruntime/bbsys.h"
#include "../bbruntime/bbstream.h"
#include <string>

// The global debug flag (normally defined in bbruntime.cpp).
bool debug = false;

// The global gx_runtime pointer (normally set in bbruntime_run).
gxRuntime *gx_runtime = 0;

// Stub implementation of gxRuntime methods that bbbank.cpp calls.
// These are only linked into test binaries, never into the real runtime.
void gxRuntime::debugError(const char *t) {
	(void)t;
}

int gxRuntime::callDll(const std::string &dll, const std::string &func,
	const void *in, int in_sz, void *out, int out_sz) {
	(void)dll; (void)func; (void)in; (void)in_sz; (void)out; (void)out_sz;
	return 0;
}

// Stub for debugStream (normally in bbstream.cpp).
void debugStream(bbStream *s) {
	(void)s;
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
