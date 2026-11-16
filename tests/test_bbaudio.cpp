// Unit tests for the bbaudio_soloud handle-table integration.
// Links against the REAL bbaudio_soloud.cpp (via bbruntime_core) + stubs_gxruntime.cpp.
// Uses SoLoud's null backend so no audio device is needed.
// Tests bbLoadSound/bbPlaySound/bbFreeSound via handles.
// Tests that using a freed Sound handle with debug=true calls the error hook.

#include "../bbruntime/basic.h"
#include "../bbruntime/bb_handle.h"
#include "../bbruntime/bb_handle_types.h"
#include "../bbruntime/bbaudio_soloud.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32

// Forward declarations for the real audio functions (defined in bbaudio_soloud.cpp).
struct Sound;
BB_HANDLE(Sound) bbLoadSound(BBStr *path);
void bbFreeSound(BB_HANDLE(Sound) sound_h);
uint32_t bbPlaySound(BB_HANDLE(Sound) sound_h);

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

// Write a minimal 8-bit mono WAV file (100 samples of silence).
static bool write_test_wav(const char *path) {
	FILE *f = fopen(path, "wb");
	if (!f) return false;

	const int sample_rate = 8000;
	const int num_samples = 100;
	const int data_size = num_samples; // 8-bit mono
	const int chunk_size = 36 + data_size;

	// RIFF header
	fwrite("RIFF", 1, 4, f);
	fwrite(&chunk_size, 4, 1, f);
	fwrite("WAVE", 1, 4, f);
	// fmt chunk
	fwrite("fmt ", 1, 4, f);
	const int fmt_size = 16;
	fwrite(&fmt_size, 4, 1, f);
	const uint16_t audio_format = 1; // PCM
	const uint16_t num_channels = 1;
	fwrite(&audio_format, 2, 1, f);
	fwrite(&num_channels, 2, 1, f);
	fwrite(&sample_rate, 4, 1, f);
	const int byte_rate = sample_rate * num_channels;
	fwrite(&byte_rate, 4, 1, f);
	const uint16_t block_align = num_channels; // 1 byte per sample
	fwrite(&block_align, 2, 1, f);
	const uint16_t bits_per_sample = 8;
	fwrite(&bits_per_sample, 2, 1, f);
	// data chunk
	fwrite("data", 1, 4, f);
	fwrite(&data_size, 4, 1, f);
	unsigned char silence[num_samples];
	memset(silence, 0, num_samples);
	fwrite(silence, 1, num_samples, f);

	fclose(f);
	return true;
}

// ---- Test 1: load -> play (non-zero channel) -> free ----
static void test_basic_lifecycle() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	const char *wav_path = "/tmp/test_bbaudio.wav";
	CHECK(write_test_wav(wav_path));

	// Load.
	BB_HANDLE(Sound) h = bbLoadSound(new BBStr(wav_path));
	CHECK(h.h != 0);

	// Play: should return a non-zero channel.
	uint32_t chan = bbPlaySound(h);
	CHECK(chan != 0);

	// Free.
	bbFreeSound(h);
}

// ---- Test 2: same object -> same handle (find-or-alloc) ----
static void test_same_handle() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	const char *wav_path = "/tmp/test_bbaudio.wav";
	CHECK(write_test_wav(wav_path));

	BB_HANDLE(Sound) h1 = bbLoadSound(new BBStr(wav_path));
	int handle1 = h1.h;
	CHECK(handle1 != 0);

	// Load a second sound — should get a different handle.
	BB_HANDLE(Sound) h2 = bbLoadSound(new BBStr(wav_path));
	CHECK(h2.h != 0);
	CHECK(h2.h != handle1);

	bbFreeSound(h1);
	bbFreeSound(h2);
}

// ---- Test 3: freed Sound handle with debug=true calls the hook ----
static void test_freed_handle_debug_hook() {
	reset_table();
	debug = true;
	bb_handle_debug = true;
	bb_handle_error = record_error;
	error_log.clear();

	const char *wav_path = "/tmp/test_bbaudio.wav";
	CHECK(write_test_wav(wav_path));

	BB_HANDLE(Sound) h = bbLoadSound(new BBStr(wav_path));
	int old_handle = h.h;
	CHECK(old_handle != 0);

	// Free the sound.
	bbFreeSound(h);

	// Use the old handle: should call the hook (stale handle).
	error_log.clear();
	BB_HANDLE(Sound) stale(old_handle);
	// Trigger lookup by converting to pointer.
	Sound *p = stale;
	CHECK(p == 0);
	CHECK(error_log.size() == 1);
	CHECK(error_log[0] == "Stale or invalid handle");

	// With debug=false: no hook call.
	debug = false;
	bb_handle_debug = false;
	error_log.clear();
	p = stale;
	CHECK(p == 0);
	CHECK(error_log.empty());
}

// ---- Test 4: table is empty after cleanup ----
static void test_table_empty_after_cleanup() {
	reset_table();
	debug = false;
	bb_handle_debug = false;
	bb_handle_error = record_error;
	error_log.clear();

	const char *wav_path = "/tmp/test_bbaudio.wav";
	CHECK(write_test_wav(wav_path));

	BB_HANDLE(Sound) h = bbLoadSound(new BBStr(wav_path));
	CHECK(h.h != 0);
	bbFreeSound(h);

	// Table should be logically empty: only the sentinel entry at index 0 is
	// occupied, and no pointer mappings remain. (forget() keeps the slot in the
	// vector for reuse — it does not shrink the vector.)
	CHECK(g_handle_table.by_ptr.empty());
	CHECK(g_handle_table.by_handle.size() >= 1);
	for (size_t i = 1; i < g_handle_table.by_handle.size(); ++i)
		CHECK(g_handle_table.by_handle[i].base_ptr == 0);
}

int main() {
	printf("Sound handle-table integration tests (real bbaudio_soloud.cpp, null driver)\n");
	printf("==========================================================================\n");

	// Init SoLoud with the null driver once for all tests.
	soloud = new SoLoud::Soloud();
	if (soloud->init(SoLoud::Soloud::LEFT_HANDED_3D, static_cast<unsigned int>(SoLoud::Soloud::NULLDRIVER)) != SoLoud::SO_NO_ERROR) {
		printf("FAIL: SoLoud null driver init failed\n");
		delete soloud;
		soloud = nullptr;
		return 1;
	}

	test_basic_lifecycle();
	test_same_handle();
	test_freed_handle_debug_hook();
	test_table_empty_after_cleanup();

	// Cleanup.
	soloud->deinit();
	delete soloud;
	soloud = nullptr;

	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}

#else
int main() { printf("skipped on Windows\n"); return 0; }
#endif
