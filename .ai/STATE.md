# State — Blitz3D macOS port (macos-port branch)

## Done (b1)
- 3D commit (b1) `da3089a` + `d4352d8` "handle_table: blitz3d entities (batch 1)" — Windows CI green.
  - 146 entity-family function signatures wrapped with BB_HANDLE(Entity)
  - eH<T>() template for handle→pointer conversion (both _WIN32 and macOS)
  - RTSYM_CAST on all 234 rtSym calls in blitz3d_link()
  - Special cases: bbEmitSound, bbClearWorld
  - Zero -fsyntax-only errors. All Mac test suites pass.

## Done (a2)
- 3D commit (a2) `babf964` "3D: blitz3d library compiles on macOS; fix loader dispatch" — Windows CI green.
  - loadEntity dangling else-if fixed (#ifdef _WIN32 if/else chain).
  - blitz3d static lib target in portable block; loader_x.cpp excluded non-Windows.
  - Compile fixes: gxruntime_b3d.h fwd decls (gxScene/gxMesh/gxTexture/gxCanvas); 12 TUs include full gx header;
    texture.h/brush.h getMatrix/getRenderState -> const void* (+casts at brush.cpp/model.cpp/q3bsprep.cpp);
    q3bsprep itoa -> snprintf %x.
  - All Mac suites pass: 407+26+28+11+27+19.
  - bbblitz3d.cpp -fsyntax-only report in .ai/blitz3d-plan.md under commit (b): 2 header errors +
    198 rtSym sites, 110 unique signatures.

## Done
- Prompt 1: stdutil + compiler compile on 64-bit macOS/Clang (commit 6211180)
- Handle-table design v2 written to `.ai/handle-table-plan.md` with all corrections applied
- Commit 1: `bb_handle.h/.cpp`, `bb_handle_types.h`, CMake, `tests/test_handle_table.cpp` (commit 5928261)
  - 407 checks, 0 failed.
- Commit 2: bbbank.cpp pilot (commit 0aa7ec4)
- Commit 3: fix commit 0aa7ec4 (commit c8208ca)
- Commit 4: bbstream + bbfilesystem (commit 984cdbd)
- Commit 5: review fixes on 984cdbd (commit 55c827e)
- Commit 6: handle table for timers (bbtimer.cpp)

## Done (b2)
- 3D commit (b2): eH<T> static_casts, all 30 return types → BB_HANDLE(Entity), bbEmitSound param wrapped.
  - Zero entity-family errors remain (syntax check with -DBB_BLITZ3D_ENABLED=1 = 0).
  - Test infrastructure: full gxScene/gxLight/gxMesh/gxGraphics-scene-method stubs; blitz3d+stdutil linked into bbruntime_core.
  - test_bbblitz3d: 17 checks, 0 failed. All 7 Mac suites pass.

## Done (c)
- 3D commit (c): Texture/Brush/Surface families → BB_HANDLE; bbPickedSurface/bbCollisionSurface void*→BB_HANDLE(Surface);
  bbTextureBuffer→BB_HANDLE(gxCanvas); tH<T>() template added.
  - forget(this) in Texture::~Texture, Brush::~Brush, Surface::~Surface; forget(frames[k]) in CachedTexture::Rep::~Rep.
  - Syntax check (-DBB_BLITZ3D_ENABLED=1): 0 errors. Build: 0 errors.
  - All 7 Mac suites pass: 407+26+28+11+27+19+17.

## Done (d)
- 3D commit (d) `69dc27d` "test_bbblitz3d: lifecycle and stale-handle coverage" — Windows CI green.
  - 5 new tests: parent/child pivots, texture+canvas stale after free, brush lifecycle, surface stale after mesh free, ClearWorld empties table.
  - Test accessors: EntityListTest (orphanCount, childCount), MeshSurfaceTest (first, count) — both use __attribute__((used)) to prevent dead-code elimination.
  - Stub additions: gx_scene definition (for non-test_bbblitz3d targets that link blitz3d lib); removed duplicate stats3d/active_texs/bbSet3dListenerConfig stubs.
  - Header changes: entity.h friend EntityListTest (non-Windows). All other blitz3d headers reverted to original.
  - All 7 Mac suites pass: 407+26+28+11+27+19+61.

## Done (e)
- sem_env commit: `compiler/debugenv.h/.cpp` — non-Windows: `debugEnvId(Environ*)` registers in a 1024-slot table, returns index+1; `debugEnvLookup(id)` resolves. `_WIN32`: inline `(int)e` (unchanged).
- `declnode.cpp:137`, `prognode.cpp:59`: `iconst(debugEnvId(sem_env))` replaces `(int)(intptr_t)sem_env`; TODO comments removed.
- `bbruntime/bbruntime.cpp`: non-Windows `_bbDebugEnter(frame, int env_id, func)` passes id through to `gx_runtime->debugEnter`. Windows signature unchanged.
- `tests/test_debugenv.cpp`: 11 checks (ids 1/2/3, round-trip, out-of-range → 0). All 8 Mac suites pass.
- Future Mac debugger must map env ids via `debugEnvLookup` (compiler/debugenv.h).

## Done (f)
- debugenv: fixed 1024-slot table → growable `vector<Environ*>` (compiler/debugenv.cpp); ids stay index+1.
- test_debugenv: 5000-env block (id assignment + round-trip for all). 10011 checks, 0 failed. All 8 Mac suites pass.

## Done (Prompt 3, commit 1)
- ABI constants `6bba566` — Windows CI green.
  - New compiler/bb_abi.h: BB_SLOT (4 Win / 8 macOS), BB_ARRAY_DATA_OFF/DIMS_OFF/SCALE_OFF(k), BB_FIELD_STRIDE.
  - Front end uses them (identical values on Windows): node.cpp enumVars + call/fcall arg offsets/sizes;
    declnode.cpp leave pop size + Type field offsets; exprnode.cpp cfunc args + CALL/FCALL sizes;
    varnode.cpp/stmtnode.cpp BBArray header offsets + element stride.
  - bbruntime/basic.cpp static_asserts offsetof(BBArray) fields and sizeof(BBField) against the constants.
  - Mac: build OK, all 8 suites pass (407+26+28+11+27+19+10011+61).
  - PC identical-exe check (old vs new blitzcc on a sample .bb) NOT done: blitzcc is not in the portable
    Mac build (top-level CMakeLists returns before add_subdirectory(blitz)). Needs a Windows machine or
    the PC CI step from the plan.

## Done (Prompt 3, commit 2)
- Frame hooks: Codegen::argOffset(k)/localOffset(k) pure virtual; Codegen_x86 returns 20+k*BB_SLOT / -BB_SLOT*(k+1) (today's values).
  - node.cpp enumVars(Environ*,Codegen*) uses the hooks; declnode.cpp + prognode.cpp pass g.
  - Windows output unchanged by construction: same int values reach the same TNodes → identical asm (CI baseline compares).
  - Mac: build OK, all 11 suites pass. PC identical-exe check: CI asm-baseline comparison is the equivalent proof.

## Done (Prompt 3, commit 3a)
- IR dump tool: tests/x64/dump_codegen.h/.cpp (DumpCodegen prints TNode trees as indented text),
  tests/x64/bbdump.cpp (Mac CLI: parse+semant+translate a .bb through the real front end into DumpCodegen).
  - Runtime symbols declared in one table in bbdump.cpp (Print, Len + all __bb* internals).
  - Added to portable CMake as executable `bbdump` (links compiler lib).
  - Ran on tests/asm_baseline/abitest.bb → tests/x64/abitest.dump; trees match the x86 baseline asm
    (same ops/order/offsets: addup params at LOCAL +20/+24/+28, local t at -4, For i at -8).
  - `typeof` renamed to `bb_typeof` in bbdump.cpp: macOS Clang reserves `typeof` even in gnu++14.
  - Mac: build OK, all 11 suites pass.

## Done (Prompt 3, commit 3b)
- TNode `wide` tagging: x64-plan.md D4 implemented.
  - `compiler/codegen.h`: added `bool wide` to TNode (default false).
  - Front end sets `wide=true` on pointer-width values:
    - IR_MEM loads/stores of string/object/vector slots
    - Internal pointer loads: BBObj fields, BBArray data pointer
    - IR_CALL results from pointer-returning helpers (__bbObjNew, __bbStrLoad, etc.)
  - Helper functions in `node.cpp`: `isWideType()`, `isPointerHelper()` (one explicit list).
  - DumpCodegen: prints " W" on wide nodes; CONST 0 printed explicitly; idata/sdata/pdata labels.
  - Regenerated `tests/x64/abitest.dump`: 16 MEM W, 2 MOVE W, 4 CALL W nodes (all pointer-width).
  - Mac: build OK, all 11 suites pass (407+19+61+27+11+10011+26+28 = 11,590 checks, 0 failed).

- x64 commits 4/4b/5/6 (asmjit, harness relocs, integer tiles, calls) — written and tested in the claude.ai session, applied as patches:
  - test_codegen_x64 (10), test_tiles_x64 (167), test_calls_x64 (50).
- x64 commit 9 (front-end integration): done. test_frontend_x64 (49 checks: arithmetic, control flow, functions, Gosub, floats, strings, Types, arrays/Dim, Data, bigger programs, leak checks), data 62, floats 114, calls 50, tiles 167, codegen 10, all 0 failed. Fixed helper-call pointer-arg width, wide Null, jcc to runtime routines.
- x64 commit 8 (data section): done. test_data_x64 (62 checks), floats 114, calls 50, tiles 167, codegen 10, all 0 failed. Packed runtime structs, null_data/field_data hooks.
- x64 commit 7 (floats, SSE): done. test_floats_x64 (114 checks), test_calls_x64 (50), test_tiles_x64 (167), test_codegen_x64 (10), all 0 failed.
  - Commit 6 front end: IR_CALL/IR_FCALL sconst = call class ("" Blitz, "C"+i/f per arg C call); FuncType::blitzFunc; Node::floatArgs. Reference dumps regenerated; new tests/x64/calltest.bb/.dump.

## Next
- Prompt 3 commit 10: debug mode (__bbDebugStmt/__bbDebugEnter/__bbDebugLeave emission with env ids)

## Blockers
- None
