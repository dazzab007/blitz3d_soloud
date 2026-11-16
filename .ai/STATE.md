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
- Commit 3: fix commit 0aa7ec4 (commit c8208ca)
  - Fix Windows cast bug: `BB_HANDLE(bbBank)(b)` → `return b;`, plain call in bank_destroy
  - gxruntime_b3d.h: guard all Windows-only includes/members with `#ifdef _WIN32`, forward-declare on macOS
  - tests/stubs_gxruntime.cpp: stubs for debug, gx_runtime, debugError, callDll, debugStream, BBStr
  - bbruntime_core CMake target (bbbank.cpp + bb_handle.cpp), test_bbbank links against it
  - test_bbbank.cpp rewritten to use real bbbank functions via forward declarations
  - handle-table-plan.md: userlib `*` param note under Prompt 3
  - test_handle_table: 407 checks, 0 failed. test_bbbank: 26 checks, 0 failed.
- Commit 4: bbstream + bbfilesystem (commit 984cdbd)
  - bbstream.cpp: 15 rtSym signatures wrapped with BB_HANDLE(bbStream), forget() in ~bbStream
  - bbfilesystem.cpp: OpenFile/ReadFile/WriteFile return BB_HANDLE(bbFile), CloseFile takes BB_HANDLE(bbStream),
    ReadDir/NextFile/CloseDir use BB_HANDLE(gxDir) with wrapper-side forget
  - bb_handle.h: RTSYM_CAST macro moved here from bbbank.cpp (identity on _WIN32, void* cast on macOS)
  - gxdir.h: guard Windows-only includes/members with #ifdef _WIN32
  - tests/stubs_gxruntime.cpp: add gxFileSystem/gxDir stubs for test binaries
  - tests/test_bbstream.cpp: 30 checks (file round-trip, same-object same-handle, stale handle debug hook,
    bank+file via WriteBytes/ReadBytes, filesystem_destroy cleanup)
  - bbruntime_core now includes bbstream.cpp + bbfilesystem.cpp
  - test_handle_table: 407, test_bbbank: 26, test_bbstream: 30 — all pass
- Commit 5: review fixes on 984cdbd (commit 55c827e)
  - test_bbstream.cpp: removed ODR-violating struct bbBank redefinition; uses real bbPokeByte/bbPeekByte
  - bbfilesystem.cpp: filename ownership confirmed unchanged (no delete f) — original leak noted in Notes
  - test_handle_table: 407, test_bbbank: 26, test_bbstream: 28 — all pass

- Commit 6: handle table for timers (bbtimer.cpp)
  - bbtimer.cpp: new file with bbCreateTimer/bbWaitTimer/bbFreeTimer using BB_HANDLE(gxTimer)
  - bbruntime.cpp: removed timer functions, added forward decls + RTSYM_CAST in link calls
  - gxtimer.h: guarded Win32 members (HANDLE/MMRESULT/timerCallback) with #ifdef _WIN32
  - tests/stubs_gxruntime.cpp: gxTimer constructor/destructor/wait + gxRuntime createTimer/freeTimer/idle stubs
  - tests/test_bbtimer.cpp: 11 checks (lifecycle, same-handle, stale handle debug hook)
  - bbruntime_core now includes bbtimer.cpp
  - test_handle_table: 407, test_bbbank: 26, test_bbstream: 28, test_bbtimer: 11 — all pass

- Graphics prep survey (no source edits): `.ai/graphics-plan.md` written
  - Header guards for gxgraphics/gxcanvas/gxfont/gxmovie/ddutil mapped with line numbers
  - 112 rtSyms surveyed; ~40 cross the handle boundary (canvas/font/movie/image)
  - Free paths: forget in bbFreeImage, freeGraphics, bbInput resize, graphics()/bbEndGraphics
    close sites, bbFreeFont, bbCloseMovie. Texture canvases deferred to bbblitz3d commit.
  - Commit split: (A) headers only, (B) handles + test_bbgraphics with stubs_blitz3d.cpp

## Next
- Prompt 2 commit 7+: remaining modules per `.ai/handle-table-plan.md` §7 sequence.
  - bbsockets deferred (needs Winsock → BSD port first)
  - Next: implement graphics-plan.md commit A (header guards), then commit B (handles + tests)

## Blockers for full bbruntime on macOS (not fixed in pilot)
- gxruntime.h → other Windows headers still needed by non-bank files
- userlibs.cpp: LoadLibrary/GetProcAddress are Windows-only

## Notes
- Userlib `'*'` params: compiler/exprnode.cpp:94-100 emits IR_MEM (raw pointer load).
  On macOS this loads a 4-byte handle, not a pointer. **Needs a handle lookup** — not changed yet.
- bbsgd.cpp (313 rtSyms) is out of scope for the handle-table work.
- bbReadFile/bbWriteFile/bbOpenFile never free the BBStr filename argument — original leak,
  left as-is (out of scope for the handle-table work).
