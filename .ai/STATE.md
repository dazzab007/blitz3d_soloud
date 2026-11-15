# State — Blitz3D macOS port (macos-port branch)

## Done
- Prompt 1: stdutil + compiler compile on 64-bit macOS/Clang (commit 6211180)
- Handle-table design v2 written to `.ai/handle-table-plan.md` with all corrections applied
- Commit 1: `bb_handle.h/.cpp`, `bb_handle_types.h`, CMake, `tests/test_handle_table.cpp` (commit 5928261)
  - 407 checks, 0 failed.
- Commit 2: bbbank.cpp pilot (commit 0aa7ec4)
  - std.h: guard `<windows.h>` with `#ifdef _WIN32`
  - bbbank.cpp: 16 rtSym signatures wrapped with BB_HANDLE, forget() in ~bbBank
  - bbruntime.cpp: wire bb_handle_debug + bb_handle_error (RTEX) in bbruntime_run
  - tests/test_bbbank.cpp: 28 checks, 0 failed.
- Commit 3: fix commit 0aa7ec4 (this commit)
  - Fix Windows cast bug: `BB_HANDLE(bbBank)(b)` → `return b;`, plain call in bank_destroy
  - gxruntime_b3d.h: guard all Windows-only includes/members with `#ifdef _WIN32`, forward-declare on macOS
  - tests/stubs_gxruntime.cpp: stubs for debug, gx_runtime, debugError, callDll, debugStream, BBStr
  - bbruntime_core CMake target (bbbank.cpp + bb_handle.cpp), test_bbbank links against it
  - test_bbbank.cpp rewritten to use real bbbank functions via forward declarations
  - handle-table-plan.md: userlib `*` param note under Prompt 3
  - test_handle_table: 407 checks, 0 failed. test_bbbank: 26 checks, 0 failed.

## Next
- Prompt 2 commit 4+: remaining modules per `.ai/handle-table-plan.md` §7 sequence.

## Blockers for full bbruntime on macOS (not fixed in pilot)
- gxruntime.h → other Windows headers still needed by non-bank files
- userlibs.cpp: LoadLibrary/GetProcAddress are Windows-only

## Notes
- Userlib `'*'` params: compiler/exprnode.cpp:94-100 emits IR_MEM (raw pointer load).
  On macOS this loads a 4-byte handle, not a pointer. **Needs a handle lookup** — not changed yet.
- bbsgd.cpp (313 rtSyms) is out of scope for the handle-table work.
