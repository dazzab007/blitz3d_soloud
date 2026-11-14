
#include "bb_handle.h"

#ifndef _WIN32

// Error hook: null by default. bbruntime init sets it to call RTEX.
// Tests set it to a recorder function.
void (*bb_handle_error)(const char *msg) = 0;

// Debug flag: mirrors bbruntime's `debug` global (bbsys.h).
// Set by bbruntime init or directly in tests.
bool bb_handle_debug = false;

HandleTable g_handle_table;

// Index 0 is reserved (handle 0 = null), so the vector starts with one dummy entry.
// This ensures the first real allocation gets index 1.
static struct HandleTableInit {
	HandleTableInit() { g_handle_table.by_handle.push_back(HandleTable::Entry()); }
} handle_table_init;

int HandleTable::lookup_or_alloc(void *base_ptr, int type_tag) {
	// Check if we already have a handle for this pointer.
	auto it = by_ptr.find(base_ptr);
	if (it != by_ptr.end()) return (int)it->second;

	// Find a free slot from the free list, or grow the vector.
	unsigned index;
	if (free_list_head != 0) {
		index = (unsigned)free_list_head;
		free_list_head = by_handle[index].next_free;
	} else {
		index = by_handle.size();
		by_handle.push_back(Entry());
	}

	// Capacity check: index must fit in 20 bits (1..0xFFFFF).
	if (index > 0xFFFFF) {
		if (bb_handle_error) bb_handle_error("Handle table capacity exceeded");
		return 0;
	}

	// Allocate: increment generation (wrap at 2047 -> 1, skipping 0).
	int gen = by_handle[index].generation + 1;
	if (gen == 0 || gen > 2047) gen = 1;

	by_handle[index].base_ptr   = base_ptr;
	by_handle[index].type_tag   = type_tag;
	by_handle[index].generation = gen;
	by_handle[index].next_free  = 0;

	unsigned handle = (index << 11) | gen;
	by_ptr[base_ptr] = handle;
	return (int)handle;
}

void *HandleTable::lookup(int handle, int expected_type) {
	if (!handle) return 0;
	unsigned u = (unsigned)handle;
	unsigned index = u >> 11;       // bits 30..11 (bit 31 is always 0)
	unsigned gen   = u & 0x7FF;     // bits 10..0

	if (index >= by_handle.size() || by_handle[index].base_ptr == 0 ||
	    by_handle[index].generation != gen) {
		if (bb_handle_debug && bb_handle_error)
			bb_handle_error("Stale or invalid handle");
		return 0;
	}
	if (by_handle[index].type_tag != expected_type) {
		if (bb_handle_debug && bb_handle_error)
			bb_handle_error("Handle type mismatch");
		return 0;
	}
	return by_handle[index].base_ptr;
}

void HandleTable::forget(void *base_ptr) {
	auto it = by_ptr.find(base_ptr);
	if (it == by_ptr.end()) return;
	unsigned handle = it->second;
	unsigned index = handle >> 11;
	by_handle[index].base_ptr   = 0;  // mark slot free (generation persists for wrap)
	by_handle[index].type_tag   = 0;
	// Add to front of free list.
	by_handle[index].next_free  = free_list_head;
	free_list_head = (int)index;
	by_ptr.erase(it);
}

#endif // !_WIN32
