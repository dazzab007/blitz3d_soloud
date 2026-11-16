# bbgraphics.cpp macOS prep — header guards + handle-table plan

No source edits in this pass. Surveys below; commands at bottom.

## 1. Header guards (Windows-only members, proposed `#ifdef _WIN32`)

### gxruntime/gxgraphics.h
- L7 `#include <d3d.h>`, L9 `#include "ddutil.h"` — Windows includes.
- L21-29 members: `dirDraw`, `ds_dirDraw`, `dir3d`, `dir3dDev` (IDirectDraw7/IDirect3D7),
  `dir3dDevDesc` (D3DDEVICEDESC7), `primFmt,zbuffFmt` + 4 tex fmt arrays (DDPIXELFORMAT).
- L31 ctor: takes `IDirectDrawSurface7 *front,*back`.
- L46 `DDSURFACEDESC2 initDesc(...)`; L47-48 `ddSurf *createSurface/loadSurface(...)`.
- L57-58 `DDGAMMARAMP _gammaRamp; IDirectDrawGammaControl *_gamma;`.
- Proposed: guard the two includes, L21-29, the ctor decl (L31) and L46-48, L57-58.
  Everything else (vwait/flip/copy/setGamma/getGamma/updateGamma, getWidth/Height/Depth/
  ScanLine/Vidmem, getFrontCanvas/getBackCanvas/getDefaultFont, createCanvas/loadCanvas/
  verifyCanvas/freeCanvas, openMovie/verifyMovie/closeMovie, loadFont/verifyFont/freeFont,
  createScene/verifyScene/freeScene, createMesh/verifyMesh/freeMesh) is pure C++ and stays
  unguarded — the SDL/GL backend (Prompt 5) implements exactly this interface.

### gxruntime/gxcanvas.h
- L5 `#include "ddutil.h"`.
- L10 `typedef IDirectDrawSurface7 ddSurf;` — keep on all platforms as an opaque alias:
  Windows = IDirectDrawSurface7; macOS = forward-declared placeholder class (e.g. `class
  GxNativeSurface; typedef GxNativeSurface ddSurf;`). Keeps `getSurface()`/`getTexSurface()`
  decls (L19-20) and ctor (L14) unguarded.
- L27-29, L55: `RECT` in clip/damage/updateBitMask signatures → replace with a small
  cross-platform `gxRect {int x,y,w,h;}` in ddutil.h, `#define RECT gxRect` on _WIN32
  (or guard the four decls and give the backend its own). DECIDE AT IMPLEMENTATION.
- L46 `RECT clip_rect;`, L51 `RECT viewport;`.
- L48 `PixelFormat format;` — ddutil.h's PixelFormat uses DDPIXELFORMAT + `_fastcall`
  (L23-27, L40, L43) → guard the ctor/setFormat in ddutil.h and add an ARGB constructor.
- All GX-interface methods (L59-134) are portable C++ — unguarded.

### gxruntime/gxfont.h
- L5 `#include <ddraw.h>`, L10 `typedef IDirectDrawSurface7 ddSurf;` (unused in the class —
  remove the include, keep alias via gxcanvas.h/ddutil.h).
- No Windows-only members: ctor/render/accessors are pure C++.

### gxruntime/gxmovie.h
- L5-7 includes mmstream.h / amstream.h / ddstream.h (DirectShow + DirectDraw media).
- L16 ctor param `IMultiMediaStream *mm_stream`; L21 `RECT src_rect;`; L24-28 `dd_surf`
  (IDirectDrawSurface), `vid_stream` (IMediaStream), `dd_stream`, `dd_sample`, `mm_stream`.
- Proposed: guard includes + ctor decl + those members. `draw/isPlaying/getWidth/getHeight`
  (L33-37) stay — but `getWidth/getHeight` read `src_rect` inline (L36-37): move them to
  out-of-line `.cpp` or add a portable `int w,h;` member. DECIDE AT IMPLEMENTATION.

### gxruntime/ddutil.h (pulled in by all four)
- L5 `#include <ddraw.h>`; L10 ddSurf typedef; L12-20 ddUtil uses ddSurf only (guard or
  forward-declare); L23-27 `_fastcall` function-pointer members; L40/43 DDPIXELFORMAT
  ctor/setFormat.

Windows behaviour: every guarded block compiles exactly as today on _WIN32; zero changes
outside the guards.

## 2. rtSym functions crossing the handle boundary (bbgraphics.cpp)

Link function `graphics_link` at bbgraphics.cpp:1202-1340 (112 rtSyms). All calls need
`RTSYM_CAST(...)` on macOS (today none do).

**gxCanvas* — params:** SetBuffer L1247, LoadBuffer L1249, SaveBuffer L1250, BufferDirty
L1251, LockBuffer L1254, UnlockBuffer L1255, ReadPixel L1256, WritePixel L1257,
ReadPixelFast L1258, WritePixelFast L1259, CopyPixel L1260 (src+dest), CopyPixelFast L1261
(src+dest), CopyRect L1279 (src+dest).
**gxCanvas* — returns:** FrontBuffer L1237, BackBuffer L1238, GraphicsBuffer L1248,
ImageBuffer L1305.

**gxFont*:** SetFont L1272 (param); LoadFont L1282 (return), FreeFont L1283 (param).

**gxMovie*:** OpenMovie L1290 (return); DrawMovie L1291, MovieWidth L1292, MovieHeight
L1293, MoviePlaying L1294, CloseMovie L1295 (params).

**bbImage* — params:** CopyImage L1299, FreeImage L1301, SaveImage L1302, GrabImage L1304,
ImageBuffer L1305, DrawImage L1306, DrawBlock L1307, TileImage L1308, TileBlock L1309,
DrawImageRect L1310, DrawBlockRect L1311, MaskImage L1312, HandleImage L1313, MidHandle
L1314, ImageWidth L1316, ImageHeight L1317, ImageXHandle L1318, ImageYHandle L1319,
ImagesOverlap L1327 (×2), ImagesCollide L1328 (×2), ScaleImage L1321, ResizeImage L1322,
RotateImage L1323, TFormImage L1324, ImageRectOverlap L1330, ImageRectCollide L1331.
**bbImage* — returns:** LoadImage L1297, LoadAnimImage L1298, CopyImage L1299, CreateImage
L1300.

Tags HT_IMAGE/HT_CANVAS/HT_FONT/HT_MOVIE already exist (bb_handle.h:9-12) and the
`handle_type_tag` specialisations are in bb_handle_types.h:91-94. `handle_family` needs no
specialisation (primary template = self).

## 3. Free paths / forget() placement

| Object | Where it dies | forget() goes |
|---|---|---|
| bbImage + its frame canvases | `~bbImage` frees frames via `gx_graphics->freeCanvas` (bbgraphics.cpp:17-20); `bbFreeImage` deletes the image (L764-773) | `g_handle_table.forget(i)` in `bbFreeImage` (wrapper-side, before `delete i` — bbImage has no ctor we should touch; it is defined locally in bbgraphics.cpp:13). Frame canvases are internal: forget each raw canvas in `~bbImage` after freeCanvas, or never register frames that BASIC can't reach… **it can**: `ImageBuffer()` returns them. So forget(frames[k]) in `~bbImage`. |
| p_canvas (input temp) | `freeGraphics` L92-95; `bbInput` resize L1043-1044 | wrapper-side `forget(p_canvas)` at both sites |
| front/back buffers | created by `gx_runtime->openGraphics`, destroyed by `closeGraphics` in `graphics()` L376 and `bbEndGraphics` L441 — canvases are **not** freed individually | explicit `g_handle_table.forget(front); forget(back);` at both close sites (wrapper-side; the gx objects die inside gxruntime) |
| gxFont | `gx_graphics->freeFont(f)` in `bbFreeFont` L615-619 | wrapper-side `forget(f)` before freeFont |
| gxMovie | `gx_graphics->closeMovie(movie)` in `bbCloseMovie` L664-666 | wrapper-side `forget(movie)` before closeMovie |
| texture canvases | `CachedTexture::Rep::~Rep` (blitz3d/cachedtexture.cpp:92) + rep release paths L52/75/87 | deferred — belongs to the bbblitz3d commit; note that `bbTextureBuffer` (bbblitz3d.cpp:447) returns these canvases to BASIC, so they must be forgotten when the Rep frees them |

Every `gx_graphics->freeCanvas(...)` call site in bbgraphics.cpp (L19, 26, 347, 351, 695,
706-707, 715, 729, 743, 752) frees either a frame canvas (covered by `~bbImage`) or a
transient canvas never handed to BASIC (`tformCanvas`/`loadCanvas` temps at L347/351,
`pic` at L695-715) — no extra forget needed there. Verify each at implementation.

## 4. gx methods the tests must stub

Tests link `bbgraphics.cpp` + `stubs_gxruntime.cpp` (pattern: test_bbtimer). Needed:

- **gxGraphics**: getWidth/getHeight/getDepth/getScanLine, getFrontCanvas/getBackCanvas/
  getDefaultFont, createCanvas/loadCanvas/freeCanvas, loadFont/freeFont, openMovie/
  closeMovie. (vwait/flip/setGamma only if those rtSyms are exercised — cheap to stub all.)
- **gxCanvas**: getWidth/getHeight, setOrigin/setViewport/setColor/setClsColor/setFont/
  setHandle/setMask/getOrigin/getHandle/getMask, cls/plot/line/rect/oval/text/blit,
  lock/unlock/setPixel/getPixel/setPixelFast/getPixelFast/copyPixel/copyPixelFast,
  backup.
- **gxFont**: getWidth()/getWidth(text), getHeight, render (not called directly by bb*).
- **gxMovie**: draw/isPlaying/getWidth/getHeight.
- **gxRuntime** (existing stub): openGraphics/closeGraphics must return a stub gxGraphics;
  idle() already returns true.

Stub canvases need a real pixel store for ReadPixel/WritePixel/CopyRect tests (small
ARGB buffer + pitch), so `bbLoadBuffer`/`tformCanvas` paths can be exercised end to end.

## 5. Commit split

**Commit A — headers only** (no .cpp changes, no behaviour change):
1. gxruntime/gxgraphics.h — guards per §1
2. gxruntime/gxcanvas.h — guards + ddSurf alias
3. gxruntime/gxfont.h — drop ddraw include
4. gxruntime/gxmovie.h — guards
5. gxruntime/ddutil.h — guards + portable RECT/PixelFormat bits

Verification: macOS `cmake --build build -k 0` (config/soloud/stdutil/compiler still build;
bbruntime not yet in the portable set) AND Windows build check required (touching _WIN32
paths — flag it).

**Commit B — handles + tests:**
1. bbruntime/bbgraphics.cpp — BB_HANDLE on ~40 signatures, forget() sites per §3,
   RTSYM_CAST in graphics_link
2. bbruntime/bb_handle_types.h — (nothing needed; tags already exist) → skip if empty
3. bbruntime/CMakeLists.txt — add bbgraphics.cpp to `bbruntime_core`? **No** — it pulls in
   blitz3d (`blitz3d_open/close`, gxScene). Instead: new test target `test_bbgraphics`
   linking bbgraphics.cpp + stubs, with a tiny `tests/stubs_blitz3d.cpp` (blitz3d_open/
   close no-ops) — keeps bbruntime_core lean. DECIDE AT IMPLEMENTATION; alternative is a
   `#ifndef _WIN32` extern stub.
4. tests/stubs_gxruntime.cpp — gxGraphics/gxCanvas/gxFont/gxMovie stubs (§4)
5. tests/test_bbgraphics.cpp — lifecycle: CreateImage/FreeImage, same-handle identity,
   stale-handle debug hook (font + image + canvas), FrontBuffer/ImageBuffer handle round-
   trip, pixel round-trip through stub canvas, LoadFont/FreeFont, OpenMovie/CloseMovie.

Windows: BB_HANDLE is `T*` and RTSYM_CAST is identity → byte-identical behaviour; but the
commit touches _WIN32 paths → needs a Windows build check before merge.

## Survey commands (reproducible)

```sh
grep -n 'rtSym' bbruntime/bbgraphics.cpp            # 112 hits, L1205-1339
grep -nE 'gxCanvas|gxFont|gxMovie|bbImage' bbruntime/bbgraphics.cpp | grep -v '//'
grep -rn 'IMultiMediaStream' bbruntime/             # 0 — movie stream types stay in gxruntime
grep -n 'forget\|g_handle_table' bbruntime/bbblitz3d.cpp   # 0 — blitz3d handles not done yet
```
