# bbblitz3d.cpp macOS prep — header guards + handle-table plan

Branch: `macos-port`. No source edits in this pass. Surveys below; commands at bottom.

## 1. Header survey (what bbblitz3d.cpp pulls in)

bbblitz3d.cpp includes (L2-23): std.h, bbblitz3d.h, bbgraphics.h, then
world/texture/brush/camera/sprite/meshmodel/loader_x/loader_3ds/loader_b3d/md2model/
q3bspmodel/meshutil/pivot/planemodel/terrain/listener/cachedtexture.
bbblitz3d.h:6 includes gxruntime/gxscene.h; bbgraphics.h:7 includes gxgraphics.h (guarded).

### Already guarded in 68dbf12 (+ ea564e8) — nothing to do
- **gxscene.h**: L9-11 `<d3d.h>`; L25-27 `dir3dDev` (IDirect3DDevice7); L127-129
  `viewport` (D3DVIEWPORT7); L132-135 proj/view/inv_view matrices + material (D3DMATRIX/D3DMATERIAL7);
  L142-144 `TexState::matrix`; L153-155 `d3d_rs`/`d3d_tss`. Portable: all GX state methods,
  enums (FX_*/BLEND_*/ZMODE_*), light sets.
- **gxmesh.h**: L7-9 `<d3d.h>`; L16-20 ctor (Windows takes IDirect3DVertexBuffer7/WORD*,
  non-Windows takes void*/unsigned short*); L42-45 `vertex_buff`/`tri_indices`.
- **gxlight.h**: L7-9 `<d3d.h>`; L18-20 `d3d_light` (D3DLIGHT7); L31/L40 setColor/getColor
  inline-on-Windows / out-of-line non-Windows.
- **gxgraphics.h, gxcanvas.h, gxfont.h, gxmovie.h, ddutil.h**: done in graphics commit A.

### Already guarded — nothing to do
- **bbruntime/std.h L9** `#include <windows.h>`: already wrapped in `#ifdef _WIN32`
  (done during the bbbank pilot, commit 0aa7ec4). Not a blocker.

### Shared include guard (fixed in commit (a))
- **blitz3d/std.h and bbruntime/std.h both use the include guard `STD_H`**. A TU that
  includes both sees only the first; bbblitz3d.cpp includes bbruntime/std.h at L2, so
  blitz3d/std.h's body (gxruntime.h, std:: containers) is silently skipped. Fixed in
  commit (a): blitz3d/std.h guard renamed to `BLITZ3D_STD_H`.

### Still Windows-only — must guard for commit (a)
- **blitz3d/loader_x.cpp L8-10** minidx9 (dxfile.h/rmxfguid.h/rmxftmpl.h) — Windows-only
  DXFile. Commit (a) guards the loader_x include in bbblitz3d.cpp with `#ifdef _WIN32`
  (include, static instance, dispatch line); .x loading on macOS comes later
  (Prompt 5 portable parser).

### No Windows types (verified by grep)
All blitz3d/*.h headers: entity/object/model/meshmodel/surface/brush/texture/cachedtexture/
camera/light/mirror/listener/pivot/sprite/md2model/q3bspmodel/terrain/planemodel/geom/
frustum/rendercontext/animator/collision/meshloader/loader_3ds/loader_b3d/meshutil.
gxruntime/gxscene.h, gxmesh.h, gxlight.h: fully guarded (above).

## 2. rtSym signatures by family (bbblitz3d.cpp, 235 total)

Param-token counts (`grep -oE '%[a-z_0-9]+'` on the 235 rtSym lines):
entity 68, surface 25, parent 22, mesh 17, texture 14, brush 13, camera 12, terrain 8,
sprite 4, md2 4, bsp 2, light 3, sound 1, source_entity 4, dest_entity 6, src_entity 3,
destination_entity 1, obscurer 1, cls_color 1. Non-index handle-typed total: **170**.

### Entity family (HT_ENTITY) — ~120 signatures
`Entity*` params: %entity (68), %parent (22), %mesh→MeshModel* (17), %camera (12),
%terrain (8), %sprite (4), %md2 (4), %bsp (2), %light (3), %source/%dest/%src/
%destination_entity (14), %obscurer (1).
`Entity*` returns: bbCreateMesh :543, bbCreateCamera :857, bbCreateLight :1067,
bbCreatePivot :1097, bbCreateSprite :1106, bbLoadSprite :1113, bbCreateMirror :1151,
bbCreatePlane :1160, bbLoadMD2 :1172, bbLoadBSP :1202, bbCreateTerrain :1244,
bbLoadTerrain :1253, bbCreateListener :1320, bbCopyEntity :1345, bbGetChild :1409,
bbFindChild :1416, bbGetParent :1714.

**Object* returns (wrap as BB_HANDLE(Entity), family base is Entity):**
- `Object * bbPickedEntity()` — bbblitz3d.cpp:1052
- `Object * bbCollisionEntity(Object *o,int index)` — :1788 (%entity param)

### Texture (HT_TEXTURE) — 14
Params %texture; returns: bbLoadTexture :354, bbLoadAnimTexture :362, bbCreateTexture :374,
bbGetBrushTexture :523.

### Brush (HT_BRUSH) — 13
Params %brush; returns: bbCreateBrush :477, bbLoadBrush :485, bbGetSurfaceBrush :731,
bbGetEntityBrush :737 (param `Model *m` → HT_ENTITY).

### Surface (HT_SURFACE) — 25
Params %surface. Returns: bbGetSurface :699 (`MeshModel *m` param), bbFindSurface :720
(`MeshModel*` + `Brush*` params), bbCreateSurface :725 (`MeshModel*` param).

### gxCanvas (HT_CANVAS) — 1
bbTextureBuffer :447 returns `gxCanvas *` (param Texture*).

### Sound (HT_SOUND) — 1
bbEmitSound :1330/1332: `#if BB_FMOD_ENABLED gxChannel* bbEmitSound(gxSound*,Object*)`
else `uint32_t bbEmitSound(Sound*,Object*)`. Soloud path: wrap Sound param, return stays
uint32_t channel. FMOD path (Windows): unchanged — BB_HANDLE is T* on _WIN32.

### Not handle-typed
%index/%collision_index/%global etc. are ints; BBStr* stays raw per CLAUDE.md.

## 3. Free paths / forget() placement

| Object | Where it dies | forget() goes |
|---|---|---|
| Entity + all subclasses | `~Entity` (blitz3d/entity.cpp:57) deletes children (`while(children()) delete children()`), then `remove()` unlinks from parent/orphans list | `g_handle_table.forget(this)` in `~Entity` (we own entity.cpp). Covers: bbFreeEntity (bbblitz3d.cpp:1361 `delete e`), ClearWorld orphans loop (:1917), collapseMesh (:176), bbLoadAnimSeq temp `delete t` (:1435), blitz3d_close→`delete world` (:1954 — World has no dtor; orphaned root entities are freed by the ClearWorld call at :1951 first) |
| Texture | `~Texture` (blitz3d/texture.cpp:102) drops rep ref, deletes Rep at 0 | `forget(this)` in `~Texture`. Covers bbFreeTexture (:389), ClearWorld (:1923), load-failure deletes (:357/:367/:471). Note: copy-ctor/operator= share reps — forget is per-Texture object, correct since each Texture* is a distinct BASIC-visible object |
| Brush | `~Brush` (blitz3d/brush.cpp:82) drops rep ref | `forget(this)` in `~Brush`. Covers bbFreeBrush (:499), ClearWorld (:1920) |
| Surface | `~Surface` (blitz3d/surface.cpp:17) frees its gxMesh | `forget(this)` in `~Surface`. Covers MeshModel::Rep::~Rep deleting surfaces (blitz3d/meshmodel.cpp:23-26) and entity-deletion cascade (~Entity→~Object→~Model→~MeshModel :181→~Rep→~Surface) |
| CachedTexture canvases | `CachedTexture::Rep::~Rep` (blitz3d/cachedtexture.cpp:90-92) frees `frames[k]` via freeCanvas; also release paths :52/:75/:87 | `forget(frames[k])` before each freeCanvas in `~Rep` and at the three single-frame release sites. These canvases are BASIC-visible via bbTextureBuffer (:447), so they must be registered+forgotten |
| ClearWorld | bbblitz3d.cpp:1915-1925 | Fully covered by the four destructors above (orphans→~Entity, brush_set→~Brush, texture_set→~Texture) |
| blitz3d_close | bbblitz3d.cpp:1949-1957 | `bbClearWorld(1,1,1)` at :1951 covers entities/brushes/textures; `delete world` at :1954 — World holds no Entity* members (only collision vectors of Object*, all already deleted by ClearWorld); no extra forget needed. Verify at implementation that no entity is parented into `world` after ClearWorld |

## 4. Proposed commits

### (a) `3D prep: unique blitz3d std.h guard; guard loader_x on macOS` — done
- blitz3d/std.h: include guard renamed `STD_H` → `BLITZ3D_STD_H` (was colliding with
  bbruntime/std.h and gxruntime/std.h, which keep `STD_H`).
- bbblitz3d.cpp: `#ifdef _WIN32` around the loader_x include, the static `Loader_X`
  instance and its dispatch line in loadEntity. Windows unchanged; .x loading on
  macOS comes later (Prompt 5 portable parser).
- bbruntime/std.h `<windows.h>` was already guarded — no change.

### (a2) `3D: blitz3d library compiles on macOS; fix loader dispatch` — see done section below


### (a2) `3D: blitz3d library compiles on macOS; fix loader dispatch` — done
- bbruntime/bbblitz3d.cpp loadEntity: `#ifdef _WIN32 if( ext=="x" ) ... else #endif` so the
  `else if( ext=="3ds" )` chain still attaches on macOS (was a dangling else-if).
- blitz3d/CMakeLists.txt: sources in BLITZ3D_SRCS; loader_x.cpp filtered out on non-Windows;
  BB_PORTABLE_BUILD returns before Windows lib linking (static lib, not linked yet).
- Top-level CMakeLists.txt: add_subdirectory(blitz3D) in the portable block.
- Compile fixes (Windows paths untouched except noted):
  - gxruntime/gxruntime_b3d.h: forward-declare gxScene/gxMesh/gxTexture/gxCanvas (non-Windows).
  - 11 TUs include the full gx header they use members of (gxgraphics.h or gxscene.h):
    cachedtexture, md2rep, surface, planemodel, meshmodel, world, model, terrainrep,
    q3bsprep, sprite, loader_b3d; camera.cpp already had it.
  - texture.h/brush.h: getMatrix()/getRenderState() return const void* (nested gxScene
    types can't be named against an incomplete class); casts at the two call sites in
    brush.cpp + model.cpp/q3bsprep.cpp. Windows ABI identical (pointer-sized).
  - q3bsprep.cpp: MSVC itoa(int,char*,base) -> snprintf %x (Windows behaviour identical).

#### bbblitz3d.cpp -fsyntax-only error report (for commit (b))
Command: `clang++ -fsyntax-only -std=c++14 -I. -Wno-deprecated-declarations -Wno-unknown-pragmas bbruntime/bbblitz3d.cpp`

- 2 header errors (missing forward decls in bbsys.h / bbblitz3d.cpp, or include gxruntime_b3d.h):
  - bbruntime/bbsys.h:9 `unknown type name 'gxRuntime'`
  - bbruntime/bbblitz3d.cpp:28 `unknown type name 'gxFileSystem'`
- 198 rtSym sites: `cannot initialize a parameter of type 'void *' with an lvalue of type <fn>` —
  all 113 unique signatures below; these are the handle-table wrap sites for commit (b)+(c).

Unique failing rtSym signatures (113):
  - `BBStr *(Texture *)`
  - `Brush *(BBStr *, int, float, float)`
  - `Brush *(float, float, float)`
  - `Brush *(Model *)`
  - `Brush *(Surface *)`
  - `Entity *(BBStr *, Entity *)`
  - `Entity *(BBStr *, float, Entity *)`
  - `Entity *(BBStr *, int, Entity *)`
  - `Entity *(Camera *, float, float)`
  - `Entity *(Entity *, Entity *)`
  - `Entity *(Entity *, float, float, float)`
  - `Entity *(Entity *)`
  - `Entity *(float, float, float, float, float, float, float)`
  - `Entity *(int, Entity *)`
  - `Entity *(int, int, Entity *)`
  - `Entity *(MeshModel *, Entity *)`
  - `Entity *(Object *, float)`
  - `float ()`
  - `float (Entity *, Entity *)`
  - `float (Entity *, int, int)`
  - `float (Entity *, int)`
  - `float (float, float, float)`
  - `float (int)`
  - `float (MD2Model *)`
  - `float (MeshModel *)`
  - `float (Object *, int)`
  - `float (Surface *, int, int)`
  - `float (Surface *, int)`
  - `float (Terrain *, float, float, float)`
  - `float (Terrain *, int, int)`
  - `gxCanvas *(Texture *, int)`
  - `int ()`
  - `int (Camera *, float, float, float)`
  - `int (Entity *, Camera *)`
  - `int (MD2Model *)`
  - `int (MeshModel *, MeshModel *)`
  - `int (MeshModel *)`
  - `int (Object *, BBStr *)`
  - `int (Object *, int)`
  - `int (Object *, Object *)`
  - `int (Object *)`
  - `int (Surface *, float, float, float, float, float, float)`
  - `int (Surface *, int, int, int)`
  - `int (Surface *, int, int)`
  - `int (Surface *)`
  - `int (Terrain *)`
  - `int (Texture *)`
  - `Object *()`
  - `Object *(Object *, int)`
  - `Surface *(MeshModel *, Brush *)`
  - `Surface *(MeshModel *, int)`
  - `Texture *(BBStr *, int, int, int, int, int)`
  - `Texture *(BBStr *, int)`
  - `Texture *(Brush *, int)`
  - `Texture *(int, int, int, int)`
  - `uint32_t (Sound *, Object *)' (aka 'unsigned int (Sound *, Object *)')`
  - `void ()`
  - `void (BBStr *, float, float, float, float, float, float, float, float, float)`
  - `void (BBStr *, int)`
  - `void (Brush *, float, float, float)`
  - `void (Brush *, float)`
  - `void (Brush *, int)`
  - `void (Brush *, Texture *, int, int)`
  - `void (Brush *)`
  - `void (Camera *, float, float, float)`
  - `void (Camera *, float, float)`
  - `void (Camera *, float)`
  - `void (Camera *, int, int, int, int)`
  - `void (Camera *, int, int)`
  - `void (Camera *, int)`
  - `void (float, float, float, Entity *, Entity *)`
  - `void (float, float, float)`
  - `void (float)`
  - `void (int, int, int, int)`
  - `void (int, int, int)`
  - `void (int)`
  - `void (Light *, float, float, float)`
  - `void (Light *, float, float)`
  - `void (Light *, float)`
  - `void (MD2Model *, int, float, int, int, float)`
  - `void (MeshModel *, Brush *)`
  - `void (MeshModel *, float, float, float, float, float, float, float)`
  - `void (MeshModel *, float, float, float, float, float, float, int)`
  - `void (MeshModel *, float, float, float, float, float, float)`
  - `void (MeshModel *, float, float, float)`
  - `void (MeshModel *, MeshModel *)`
  - `void (MeshModel *)`
  - `void (Object *, float, float, float, float, float, float)`
  - `void (Object *, float, float)`
  - `void (Object *, int, int)`
  - `void (Object *)`
  - `void (Q3BSPModel *, float, float, float)`
  - `void (Q3BSPModel *, int)`
  - `void (Sprite *, float, float)`
  - `void (Sprite *, float)`
  - `void (Sprite *, int)`
  - `void (Surface *, Brush *)`
  - `void (Surface *, int, float, float, float, float)`
  - `void (Surface *, int, float, float, float, int)`
  - `void (Surface *, int, float, float, float)`
  - `void (Surface *, int, int)`
  - `void (Terrain *, int, int, float, int)`
  - `void (Terrain *, int, int)`
  - `void (Terrain *, int)`
  - `void (Texture *, float, float)`
  - `void (Texture *, float)`
  - `void (Texture *, int)`
  - `void (Texture *)`
  - `void *()`
  - `void *(Object *, int)`

### (b) `handle_table: blitz3d entity family`
- bbblitz3d.cpp/.h: wrap ~120 entity-family signatures (params + 17 Entity* returns +
  the two Object* returns) with BB_HANDLE(Entity); RTSYM_CAST at all link sites.
- blitz3d/entity.cpp: `forget(this)` in `~Entity` under `#ifndef _WIN32`.
- Stubs needed in tests/stubs_gxruntime.cpp: StubScene (gxScene ctor/dtor + the state
  setters bbblitz3d calls: setWBuffer/setDither/setAntialias/setWireframe/setAmbient/
  hwTexUnits etc. — only what the test path touches), StubGraphics::createScene/
  verifyScene/freeScene, plus whatever blitz3d/*.cpp TUs get added to bbruntime_core
  (entity.cpp, object.cpp, model.cpp, meshmodel.cpp, surface.cpp, world.cpp, geom.cpp,
  animator.cpp, collision.cpp, frustum.cpp, camera.cpp, light.cpp, listener.cpp,
  mirror.cpp, pivot.cpp, planemodel.cpp, terrain.cpp, sprite.cpp, md2model.cpp,
  q3bspmodel.cpp, meshutil.cpp — add incrementally as the compiler demands; keep
  loaders OUT: loader_x/3ds/b3d need files + Windows deps).

### (c) `handle_table: blitz3d texture/brush/surface`
- bbblitz3d.cpp: wrap texture (14), brush (13), surface (25), TextureBuffer (1),
  EmitSound (1) signatures; fix bbPickedSurface/bbCollisionSurface void* → BB_HANDLE(Surface).
- blitz3d/texture.cpp, brush.cpp, surface.cpp: `forget(this)` in each dtor.
- blitz3d/cachedtexture.cpp: `forget(frames[k])` at ~Rep (:90) + release sites :52/:75/:87.
- Stubs: StubGraphics::createCanvas already exists; CachedTexture needs a stub canvas
  path (loadCanvas returning a StubCanvas for the test texture file).

### (d) `handle_table: blitz3d tests` — tests/test_bbblitz3d.cpp
- Null-driver-style init: gx_graphics = new StubGraphics, blitz3d_open() (needs
  createScene stub), then: CreateMesh→FreeEntity (stale handle hook with debug=true),
  parent/child (GetChild/GetParent return same handles as the originals),
  CreateTexture→FreeTexture, CreateBrush→FreeBrush, CreateSurface via a MeshModel stub,
  PickedSurface/CollisionSurface return valid handles, ClearWorld → table empty
  (by_ptr.empty() + all slots base_ptr==0, per the audio-test lesson).
- Stubs: same set as (b)+(c); test links bbruntime_core.

**Verification after each commit:** `cmake --build build -k 0` zero errors; all existing
Mac suites still pass (407+26+28+11+27+19); Windows CI green after push.

## Commands used

```sh
grep -c 'rtSym' bbruntime/bbblitz3d.cpp                      # 235
grep -n 'rtSym' bbruntime/bbblitz3d.cpp | grep -oE '%[a-z_0-9]+' | sort | uniq -c | sort -rn
grep -n 'rtSym' bbruntime/bbblitz3d.cpp | grep -E '%entity|%parent|...' | wc -l   # 170 (non-index)
grep -rln 'IDirect\|RECT\b\|ddSurf\|HBITMAP\|_WIN32' blitz3d/*.h    # (empty)
git show --stat 68dbf12                                          # 10 gxruntime headers guarded
```
