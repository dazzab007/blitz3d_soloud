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

## Done (cont.)
- Graphics commit A (headers): 68dbf12 + fix ea564e8 (ddSurf typedef restored on Windows). CI green.
- Graphics commit B b1 (code): b33e561 — BB_HANDLE in bbgraphics.cpp/.h, forget() sites,
  bbgraphics.cpp in BB_CORE_SRCS, stubs extended. Mac build green, 4 suites pass
  (407+26+28+11). Windows CI run 43912636011 GREEN.
- Graphics commit B b2: two bug fixes from b33e561 + tests/test_bbgraphics.cpp.
  - ~bbImage now forgets the image itself (frame canvases were forgotten, image was not).
  - saveCanvas BMP structs: uint16_t/uint32_t/int32_t + #pragma pack(2)/pack(1),
    static_assert sizeof==14 / ==40.
  - test_bbgraphics.cpp: 27 checks (image stale after free, ImageBuffer stable-then-stale,
    font stale after free, EndGraphics forgets front/back, table empty after cleanup).
  - stubs_gxruntime.cpp: gxRuntime ctor/dtor + StubGraphics populates base private members
    (front_canvas/back_canvas/def_font made public on non-Windows), base-class method
    definitions for gxGraphics/gxCanvas/gxFont/gxMovie, gxGraphics dtor forgets canvases/font.
  - Mac build green; 5 suites pass (407+26+28+11+27).

- Commit 8: handle table for audio (bbaudio_soloud) — commit 4ac6a0d
  - BB_HANDLE(Sound) on all Sound* rtSym signatures, RTSYM_CAST at link sites, channels stay uint32_t
  - ~Sound forgets handle under #ifndef _WIN32
  - bbaudio_soloud.cpp in bbruntime_core (portable block only, not BB_CORE_SRCS); links soloud
  - soloud: null backend + WITH_NULL=1 on APPLE only; Windows config unchanged
  - tests/test_bbaudio.cpp: 19 checks (null driver, tiny WAV in /tmp, lifecycle, distinct handles,
    stale-handle debug hook, table empty after cleanup). Note: "empty" = by_ptr.empty() + all slots
    base_ptr==0; forget() keeps slots in the vector for reuse.
  - Mac build green; 6 suites pass (407+26+28+11+27+19). Windows CI run 43954074793 GREEN.

- 3D prep survey (no source edits): `.ai/blitz3d-plan.md` written
  - Headers: all blitz3d/*.h clean; gxscene/gxmesh/gxlight already guarded in 68dbf12.
    bbruntime/std.h <windows.h> already guarded (bbbank pilot); loader_x.cpp minidx9
    guarded in bbblitz3d.cpp in commit (a).
  - blitz3d/std.h and bbruntime/std.h shared include guard STD_H — fixed in (a) by
    renaming blitz3d's to BLITZ3D_STD_H.
  - 235 rtSyms, 170 non-index handle-typed: entity ~120 (incl. Object* returns at :1052/:1788),
    texture 14, brush 13, surface 25 (void* sites :1056/:1793), TextureBuffer 1, EmitSound 1.
  - Free paths: ~Entity (entity.cpp:57, children+orphans), ~Texture (:102), ~Brush (:82),
    ~Surface (:17), CachedTexture::Rep::~Rep canvases (cachedtexture.cpp:90 + :52/:75/:87).
  - Commits: (a) done — std.h guard + loader_x #ifdef; (a2) blitz3d CMake target
    (loader_x.cpp excluded); (b) entity sigs + ~Entity forget; (c) tex/brush/surf + forgets;
    (d) tests.

- 3D commit (a): blitz3d/std.h guard → BLITZ3D_STD_H; bbblitz3d.cpp loader_x include,
  static instance and dispatch line under #ifdef _WIN32. Mac build green, 6 suites pass
  (407+26+28+11+27+19). Windows CI pending (touches _WIN32 branch — needs the check).

## Next
- Prompt 2 commit 7+: remaining modules per `.ai/handle-table-plan.md` §7 sequence.
  - bbsockets deferred (needs Winsock → BSD port first)

## Windows CI investigation (RESOLVED)
- Commit A (68dbf12 "graphics headers: compile on macOS") broke the Windows build.
  Root cause: 68dbf12 removed `typedef IDirectDrawSurface7 ddSurf;` from the _WIN32 branch of
  ddutil.h while gxcanvas.cpp/gxgraphics.cpp still declared `ddSurf *` params — MSVC errors, not a
  runner/cache issue. The "old vs new file" paradox was a misread: error sites referenced the
  _WIN32 branch which had lost the typedef.
- Fix: ea564e8 restored the typedef on Windows and dropped the probe (dc32b03). CI run
  43865694244 @ ea564e8 is GREEN.
- Lesson recorded in DECISIONS.md: Mac compiling proves nothing about the _WIN32 branch — every
  #ifdef _WIN32 edit needs a Windows build check (already a CLAUDE.md rule; now evidenced).

## Blockers for full bbruntime on macOS (not fixed in pilot)
- gxruntime.h → other Windows headers still needed by non-bank files
- userlibs.cpp: LoadLibrary/GetProcAddress are Windows-only

## Notes
- Userlib `'*'` params: compiler/exprnode.cpp:94-100 emits IR_MEM (raw pointer load).
  On macOS this loads a 4-byte handle, not a pointer. **Needs a handle lookup** — not changed yet.
- bbsgd.cpp (313 rtSyms) is out of scope for the handle-table work.
- bbReadFile/bbWriteFile/bbOpenFile never free the BBStr filename argument — original leak,
  left as-is (out of scope for the handle-table work).
