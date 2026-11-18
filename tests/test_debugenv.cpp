// Unit tests for the debug env id table (compiler/debugenv.h).
// Self-contained: no graphics, audio, or runtime dependencies.

#include "../compiler/debugenv.h"

#include <cstdio>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
	++tests_run; \
	if (!(cond)) { \
		++tests_failed; \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while (0)

#ifdef _WIN32

// On Windows debugEnvId is an inline pointer cast — nothing to test.
int main() {
	printf("Windows build: debug env id is a pointer cast (nothing to test)\n");
	printf("RESULT: PASS (nothing to test)\n");
	return 0;
}

#else

int main() {
	// Type globals are static initializers in type.cpp; no explicit init needed.
	Environ *a = new Environ("", Type::int_type, 0, 0);
	Environ *b = new Environ("", Type::int_type, 0, 0);
	Environ *c = new Environ("", Type::int_type, 0, 0);

	int id_a = debugEnvId(a);
	int id_b = debugEnvId(b);
	int id_c = debugEnvId(c);

	CHECK(id_a == 1);
	CHECK(id_b == 2);
	CHECK(id_c == 3);

	// Re-registering the same pointer returns the same id.
	CHECK(debugEnvId(a) == id_a);
	CHECK(debugEnvId(b) == id_b);

	// Round-trip lookup.
	CHECK(debugEnvLookup(id_a) == a);
	CHECK(debugEnvLookup(id_b) == b);
	CHECK(debugEnvLookup(id_c) == c);

	// Out-of-range / null ids return 0.
	CHECK(debugEnvLookup(0) == 0);
	CHECK(debugEnvLookup(-1) == 0);
	CHECK(debugEnvLookup(99) == 0);

	delete a; delete b; delete c;

	// Growable table: register 5000 envs, ids stay index+1, all resolve.
	Environ *envs[5000];
	for (int k = 0; k < 5000; ++k)
		envs[k] = new Environ("", Type::int_type, 0, 0);
	int first_id = debugEnvId(envs[0]);
	for (int k = 0; k < 5000; ++k)
		CHECK(debugEnvId(envs[k]) == first_id + k);
	for (int k = 0; k < 5000; ++k)
		CHECK(debugEnvLookup(first_id + k) == envs[k]);
	for (int k = 4999; k >= 0; --k)
		delete envs[k];

	printf("=======================\n");
	printf("%d checks, %d failed\n", tests_run, tests_failed);
	if (tests_failed) {
		printf("RESULT: FAIL\n");
		return 1;
	}
	printf("RESULT: PASS\n");
	return 0;
}

#endif // _WIN32
