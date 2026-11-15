
#ifndef BB_HANDLE_H
#define BB_HANDLE_H

#include <cstddef>

// Family tags — one per object family, not per class.
enum HandleType {
	HT_ENTITY = 1,   // Entity and all subclasses
	HT_STREAM,       // bbStream: bbFile, TCPStream, UDPStream
	HT_TCP_SERVER,   // TCPServer (not a bbStream)
	HT_TEXTURE,      // Texture
	HT_BRUSH,        // Brush
	HT_SURFACE,      // Surface
	HT_IMAGE,        // bbImage
	HT_CANVAS,       // gxCanvas
	HT_FONT,         // gxFont
	HT_MOVIE,        // gxMovie
	HT_DIR,          // gxDir
	HT_BANK,         // bbBank
	HT_SOUND,        // Sound (soloud) / gxSound (FMOD)
	HT_TIMER         // gxTimer
};

// handle_family<T>: maps each type to its family base.
// Primary template: T is its own base (singleton families).
template <typename T> struct handle_family { typedef T base; };

// handle_type_tag<T>: maps each type to its HT_* family tag.
template <typename T> struct handle_type_tag { static const int value; };

#ifdef _WIN32
// Windows 32-bit: handle IS the pointer. No table, no lookup, no wrapper.
#define BB_HANDLE(T) T*
// rtSym stores function pointers as void*; plain T* signatures convert implicitly.
#define RTSYM_CAST(f) f
#else
// macOS 64-bit: handle is a 4-byte unsigned int.
// Layout: bit 31 = 0, bits 30..11 = index (20 bits), bits 10..0 = generation (11 bits).
// Value never 0. Generation wraps at 2047 -> 1 (skipping 0).

#include <vector>
#include <unordered_map>

// Error hook: set by bbruntime init to call RTEX, or by tests to a recorder.
// Called when debug=true and a handle error is detected (stale handle, type mismatch,
// capacity exceeded). If null, the error is silently ignored (release behaviour).
extern void (*bb_handle_error)(const char *msg);

// Debug flag: mirrors bbruntime's `debug` global (bbsys.h).
// Set by bbruntime init (bbruntime_run) or directly in tests.
extern bool bb_handle_debug;

struct HandleTable {
	struct Entry {
		void *base_ptr;   // family BASE pointer (Entity*, bbStream*, ...). 0 = slot free.
		int   type_tag;   // HT_* family tag
		int   generation; // 1..2047, persists across forget/realloc for wrap
		int   next_free;  // index of next free entry (for free list), 0 if not on free list
	};

	std::vector<Entry>          by_handle;  // index -> Entry (index is the vector offset)
	std::unordered_map<void*, unsigned> by_ptr;  // base_ptr -> (index << 11 | gen)
	int free_list_head;  // index of first free entry, 0 if none

	int      lookup_or_alloc(void *base_ptr, int type_tag);
	void   * lookup(int handle, int expected_type);
	void     forget(void *base_ptr);
};

extern HandleTable g_handle_table;

template <typename T>
struct bbH {
	int h;  // 4 bytes, trivially copyable. 0 = null.

	bbH() : h(0) {}
	explicit bbH(int handle) : h(handle) {}

	// From pointer: upcast to family base, find-or-allocate.
	bbH(T *p) {
		static_assert(sizeof(bbH<T>) == 4, "bbH must be 4 bytes on macOS");
		if (!p) { h = 0; return; }
		typename handle_family<T>::base *bp = p;  // implicit upcast
		h = g_handle_table.lookup_or_alloc(bp, handle_type_tag<T>::value);
	}

	// To pointer: validated lookup, downcast via family base (never from void*).
	operator T*() const {
		if (!h) return 0;
		void *vp = g_handle_table.lookup(h, handle_type_tag<T>::value);
		return vp ? static_cast<T*>(static_cast<typename handle_family<T>::base *>(vp)) : 0;
	}

	T *operator->() const {
		void *vp = g_handle_table.lookup(h, handle_type_tag<T>::value);
		return vp ? static_cast<T*>(static_cast<typename handle_family<T>::base *>(vp)) : 0;
	}
};

// Namespace-scope check: bbH<int> must be exactly 4 bytes.
static_assert(sizeof(bbH<int>) == 4, "bbH<int> must be 4 bytes on macOS");

#define BB_HANDLE(T) bbH<T>
// On macOS, BB_HANDLE(T) is bbH<T> (a struct), so function pointers with
// BB_HANDLE params can't implicitly convert to void*. Cast through reinterpret_cast.
#define RTSYM_CAST(f) (void*)(f)
#endif

#endif // BB_HANDLE_H
