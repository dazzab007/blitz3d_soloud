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

## Next
- Push commit (c) + Windows CI green (gh run watch --exit-status)
- 3D commit (d): handle_table blitz3d tests (tests/test_bbblitz3d.cpp extension)

## Blockers
- None
