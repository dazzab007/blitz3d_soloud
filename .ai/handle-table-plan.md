# Handle-Table Design (bbruntime) — Revised v2

Branch: `macos-port`. No source edits in this step.

## 1. Survey — exact grep commands and counts

All commands run from the repository root.

### 1a. Total rtSym registrations per file

```sh
grep -c 'rtSym' bbruntime/*.cpp | grep -v ':0'
```

| File | Count |
|---|---|
| bbsgd.cpp | 313 |
| bbblitz3d.cpp | 235 |
| bbgraphics.cpp | 112 |
| basic.cpp | 53 |
| bbruntime.cpp | 45 |
| bbinput.cpp | 42 |
| bbsockets.cpp | 21 |
| bbstring.cpp | 19 |
| bbaudio.cpp | 18 |
| bbaudio_soloud.cpp | 17 |
| bbbank.cpp | 17 |
| bbstream.cpp | 16 |
| bbmath.cpp | 18 |
| userlibs.cpp | 4 |
| multiplay.cpp | 15 |
| **Total** | **963** |

### 1b. Handle-typed rtSym signatures (params AND returns)

The pattern `%typename` in the rtSym string marks a handle parameter. Return types are
identified by reading the function definition (the rtSym string does not encode them).

```sh
# blitz3d: entity-family handles (Entity*, Camera*, Light*, Model*/MeshModel*,
#          Sprite*, MD2Model*, BSPModel*, Terrain*, Pivot*, Mirror*, Listener*)
grep -n 'rtSym' bbruntime/bbblitz3d.cpp | grep -E '%entity|%camera|%light|%mesh|%surface|%sprite|%md2|%bsp|%terrain|%parent|%source_entity|%dest_entity|%src_entity|%destination_entity|%obscurer|%cls_color'
# → 169 signatures

# blitz3d: texture + brush handles
grep -n 'rtSym' bbruntime/bbblitz3d.cpp | grep -E '%texture|%brush'
# → 30 signatures

# blitz3d: sound handle (only one)
grep -n 'rtSym' bbruntime/bbblitz3d.cpp | grep -E '%sound'
# → 1 signature (%EmitSound%sound%entity)

# Total bbblitzd handle-typed signatures: 200 of 235
```

```sh
# graphics: canvas (buffer), font, movie, image handles
grep -n 'rtSym' bbruntime/bbgraphics.cpp | grep -E '%buffer|%font|%movie|%image'
# → 43 signatures
```

```sh
# filesystem: file_stream and dir handles
grep -n 'rtSym' bbruntime/bbfilesystem.cpp | grep -E '%file_stream|%dir'
# → 5 signatures
```

```sh
# sockets: udp_stream, tcp_stream, tcp_server handles
grep -n 'rtSym' bbruntime/bbsockets.cpp | grep -E '%udp_stream|%tcp_stream|%tcp_server'
# → 12 signatures
```

```sh
# stream (bbStream base): stream, src_stream, dest_stream handles
grep -n 'rtSym' bbruntime/bbstream.cpp | grep -E '%stream|%src_stream|%dest_stream'
# → 15 signatures
```

```sh
# bank: bank, src_bank, dest_bank handles
grep -n 'rtSym' bbruntime/bbbank.cpp | grep -E '%bank|%src_bank|%dest_bank'
# → 14 signatures
```

```sh
# audio (both backends): sound and channel handles
grep -n 'rtSym' bbruntime/bbaudio.cpp bbruntime/bbaudio_soloud.cpp | grep -E '%sound|%channel'
# → 26 signatures
```

```sh
# runtime: timer handle
grep -n 'rtSym' bbruntime/bbruntime.cpp | grep -E '%timer'
# → 1 signature (%WaitTimer%timer) + bbCreateTimer returns gxTimer*
```

### 1c. Return types that cross the rtSym boundary (found by reading definitions)

| Function | File:line | Returns | Family |
|---|---|---|---|
| `bbCreateBank` | bbbank.cpp:49 | `bbBank *` | HT_BANK |
| `bbLoadTexture` | bbblitz3d.cpp:355 | `Texture *` | HT_TEXTURE |
| `bbLoadAnimTexture` | bbblitz3d.cpp:362 | `Texture *` | HT_TEXTURE |
| `bbCreateTexture` | bbblitz3d.cpp:374 | `Texture *` | HT_TEXTURE |
| `bbGetBrushTexture` | bbblitz3d.cpp:523 | `Texture *` | HT_TEXTURE |
| `bbTextureBuffer` | bbblitz3d.cpp:447 | `gxCanvas *` | HT_CANVAS |
| `bbCreateBrush` | bbblitz3d.cpp:477 | `Brush *` | HT_BRUSH |
| `bbLoadBrush` | bbblitz3d.cpp:485 | `Brush *` | HT_BRUSH |
| `bbGetSurface` | bbblitz3d.cpp:699 | `Surface *` | HT_SURFACE |
| `bbFindSurface` | bbblitz3d.cpp:720 | `Surface *` | HT_SURFACE |
| `bbCreateSurface` | bbblitz3d.cpp:725 | `Surface *` | HT_SURFACE |
| `bbGetSurfaceBrush` | bbblitz3d.cpp:731 | `Brush *` | HT_BRUSH |
| `bbGetEntityBrush` | bbblitz3d.cpp:737 | `Brush *` | HT_BRUSH |
| `bbPickedSurface` | bbblitz3d.cpp:1056 | `void *` (Surface*) | HT_SURFACE |
| `bbCollisionSurface` | bbblitz3d.cpp:1793 | `void *` (Surface*) | HT_SURFACE |
| `bbCreateMesh` | bbblitz3d.cpp:543 | `Entity *` | HT_ENTITY |
| `bbCreateCamera` | bbblitz3d.cpp:857 | `Entity *` | HT_ENTITY |
| `bbCreateLight` | bbblitz3d.cpp:1067 | `Entity *` | HT_ENTITY |
| `bbCreatePivot` | bbblitz3d.cpp:1097 | `Entity *` | HT_ENTITY |
| `bbCreateSprite` | bbblitz3d.cpp:1106 | `Entity *` | HT_ENTITY |
| `bbLoadSprite` | bbblitz3d.cpp:1113 | `Entity *` | HT_ENTITY |
| `bbLoadMD2` | bbblitz3d.cpp:1172 | `Entity *` | HT_ENTITY |
| `bbCreateMirror` | bbblitz3d.cpp:1151 | `Entity *` | HT_ENTITY |
| `bbCreatePlane` | bbblitz3d.cpp:1160 | `Entity *` | HT_ENTITY |
| `bbLoadBSP` | bbblitz3d.cpp:1202 | `Entity *` | HT_ENTITY |
| `bbCreateTerrain` | bbblitz3d.cpp:1244 | `Entity *` | HT_ENTITY |
| `bbLoadTerrain` | bbblitz3d.cpp:1253 | `Entity *` | HT_ENTITY |
| `bbCreateListener` | bbblitz3d.cpp:1320 | `Entity *` | HT_ENTITY |
| `bbCopyEntity` | bbblitz3d.cpp:1345 | `Entity *` | HT_ENTITY |
| `bbGetChild` | bbblitz3d.cpp:1409 | `Entity *` | HT_ENTITY |
| `bbGetParent` | bbblitz3d.cpp:1714 | `Entity *` | HT_ENTITY |
| `bbFindChild` | bbblitz3d.cpp:~460 | `Entity *` | HT_ENTITY |
| `bbPickedEntity` | bbblitz3d.cpp:1052 | `Object *` | HT_ENTITY |
| `bbCollisionEntity` | bbblitz3d.cpp:1788 | `Object *` | HT_ENTITY |
| `bbLoadFont` | bbgraphics.cpp:605 | `gxFont *` | HT_FONT |
| `bbOpenMovie` | bbgraphics.cpp:639 | `gxMovie *` | HT_MOVIE |
| `bbLoadImage` | bbgraphics.cpp:668 | `bbImage *` | HT_IMAGE |
| `bbLoadAnimImage` | bbgraphics.cpp:681 | `bbImage *` | HT_IMAGE |
| `bbCopyImage` | bbgraphics.cpp:721 | `bbImage *` | HT_IMAGE |
| `bbCreateImage` | bbgraphics.cpp:747 | `bbImage *` | HT_IMAGE |
| `bbFrontBuffer` | bbgraphics.cpp:463 | `gxCanvas *` | HT_CANVAS |
| `bbBackBuffer` | bbgraphics.cpp:467 | `gxCanvas *` | HT_CANVAS |
| `bbGraphicsBuffer` | bbgraphics.cpp:333 | `gxCanvas *` | HT_CANVAS |
| `bbImageBuffer` | bbgraphics.cpp:795 | `gxCanvas *` | HT_CANVAS |
| `bbOpenFile`/`bbReadFile`/`bbWriteFile` | bbfilesystem.cpp:56-64 | `bbFile *` | HT_STREAM |
| `bbReadDir` | bbfilesystem.cpp:82 | `gxDir *` | HT_DIR |
| `bbCreateUDPStream` | bbsockets.cpp:312 | `UDPStream *` | HT_STREAM |
| `bbOpenTCPStream` | bbsockets.cpp:385 | `TCPStream *` | HT_STREAM |
| `bbCreateTCPServer` | bbsockets.cpp:419 | `TCPServer *` | HT_TCP_SERVER |
| `bbAcceptTCPStream` | bbsockets.cpp:441 | `TCPStream *` | HT_STREAM |
| `bbLoadSound` (soloud) | bbaudio_soloud.cpp:44 | `Sound *` | HT_SOUND |
| `bbCreateTimer` | bbruntime.cpp:83 | `gxTimer *` | HT_TIMER |

**Excluded from handle table** (stay raw pointers per CLAUDE.md):
- `BBStr *` — all string params/returns
- Array/vector handles (`_bbVecAlloc`, `_bbDimArray`, etc.)
- BASIC Type instances (`_bbIntType`, `_bbFltType`, `_bbStrType`, `_bbCStrType`)
- `DPID` (multiplay) — already an integer, not a pointer

### 1d. Total handle-typed signatures to wrap

| Module | Count |
|---|---|
| bbblitz3d.cpp | 200 |
| bbgraphics.cpp | 43 |
| bbaudio*.cpp | 26 |
| bbstream.cpp | 15 |
| bbbank.cpp | 14 |
| bbsockets.cpp | 12 |
| bbfilesystem.cpp | 5 |
| bbruntime.cpp (timer) | 2 (create + wait) |
| **Total** | **~317** |

---

## 2. HandleTable: generation-tagged handles, family base pointers

### 2a. Handle format

A handle is a 32-bit unsigned int: **bit 31 always 0**, 20-bit index, 11-bit generation. Value never 0.

```
bit 31 | bits 30..11 | bits 10..0
+------+-------------+-----------+
|  0   |   index     |   gen     |
+------+-------------+-----------+
```

- Bit 31 is always 0 (handles are positive ints). Index 0 is reserved (handle 0 = null). Valid indices: 1..0xFFFFF (≈1M slots).
- Generation starts at 1 for a fresh slot, increments on each reuse (wraps at 2047→1, skipping 0). Generation persists across forget/realloc — only `base_ptr == 0` marks a slot as free.
- A stale handle (old generation) fails the lookup even if the slot was reused.
- Encode/decode use `unsigned` arithmetic: `handle = (index << 11) | gen`, `index = handle >> 11`, `gen = handle & 0x7FF`.

### 2b. Storage

```cpp
struct HandleTable {
    struct Entry {
        void *base_ptr;   // family BASE pointer (Entity*, bbStream*, ...). 0 = slot free.
        int   type_tag;   // HT_* family tag
        int   generation; // 1..2047, persists across forget/realloc for wrap
    };

    std::vector<Entry>       by_handle;  // index → Entry (index is the vector offset)
    std::unordered_map<void*, unsigned> by_ptr;  // base_ptr → (index << 11 | gen)

    int      lookup_or_alloc(void *base_ptr, int type_tag);
    void   * lookup(int handle, int expected_type);  // returns base_ptr or 0
    void     forget(void *base_ptr);
};
```

- `by_handle` is a `vector` (cache-friendly, index is the offset).
- `by_ptr` is an `unordered_map` keyed on the **family base pointer**.
- `lookup_or_alloc` returns the existing handle if `by_ptr` already has `base_ptr`.

### 2c. Family base pointer normalisation

Every subclass pointer is converted to its family base before entering the table:

| Family | Base type | Subclasses that map to it |
|---|---|---|
| HT_ENTITY | `Entity *` | Entity, Object, Model, MeshModel, Camera, Light, Sprite, MD2Model, Q3BSPModel, Terrain, Pivot, Mirror, Listener |
| HT_STREAM | `bbStream *` | bbFile, TCPStream, UDPStream |
| HT_TCP_SERVER | `TCPServer *` | TCPServer (no subclasses) |
| HT_TEXTURE | `Texture *` | Texture |
| HT_BRUSH | `Brush *` | Brush |
| HT_SURFACE | `Surface *` | Surface |
| HT_IMAGE | `bbImage *` | bbImage |
| HT_CANVAS | `gxCanvas *` | gxCanvas |
| HT_FONT | `gxFont *` | gxFont |
| HT_MOVIE | `gxMovie *` | gxMovie |
| HT_DIR | `gxDir *` | gxDir |
| HT_BANK | `bbBank *` | bbBank |
| HT_SOUND | `Sound *` (soloud) / `gxSound *` (FMOD) | Sound, gxSound |
| HT_TIMER | `gxTimer *` | gxTimer |

`handle_family<T>` is a trait that maps each type to its family base:

```cpp
// Primary template: T is its own base (singleton families).
template <typename T> struct handle_family { typedef T base; };

// Entity family: all subclasses → Entity
template <> struct handle_family<Entity>     { typedef Entity base; };
template <> struct handle_family<Object>     { typedef Entity base; };
template <> struct handle_family<Model>      { typedef Entity base; };
template <> struct handle_family<MeshModel>  { typedef Entity base; };
template <> struct handle_family<Camera>     { typedef Entity base; };
template <> struct handle_family<Light>      { typedef Entity base; };
template <> struct handle_family<Sprite>     { typedef Entity base; };
template <> struct handle_family<MD2Model>   { typedef Entity base; };
template <> struct handle_family<Q3BSPModel> { typedef Entity base; };
template <> struct handle_family<Terrain>    { typedef Entity base; };
template <> struct handle_family<Pivot>      { typedef Entity base; };
template <> struct handle_family<Mirror>     { typedef Entity base; };
template <> struct handle_family<Listener>   { typedef Entity base; };

// Stream family: all subclasses → bbStream
template <> struct handle_family<bbStream>  { typedef bbStream base; };
template <> struct handle_family<bbFile>    { typedef bbStream base; };
template <> struct handle_family<TCPStream> { typedef bbStream base; };
template <> struct handle_family<UDPStream> { typedef bbStream base; };

// Sound family (both backends)
template <> struct handle_family<Sound>   { typedef Sound base; };   // soloud
template <> struct handle_family<gxSound> { typedef gxSound base; }; // FMOD
```

Upcast: `typename handle_family<T>::base *bp = p;` (implicit, well-defined).

Downcast (on the way out): `static_cast<T*>(static_cast<typename handle_family<T>::base *>(vp))`.
**Never** `static_cast<T*>(void*)` directly — always go through the family base type first.

---

## 3. Release on real deletion — forget() in family base destructors

**Principle**: `forget()` lives in the **family base destructor we own**, so that
all internal deletes (TCPServer deleting accepted streams, ClearWorld, child entity
deletion, MeshModel::Rep deleting surfaces) are covered automatically. Wrapper-side
`forget` is only needed for **gx-owned objects** whose destructors are in gxruntime
(font, movie, dir, timer, canvases).

### 3a. Destructors where forget() goes (we own these)

| Family | Destructor | File:line | What it covers |
|---|---|---|---|
| HT_ENTITY | `Entity::~Entity` | blitz3d/entity.cpp:57 | All entities: bbFreeEntity, ClearWorld, collapseMesh, child deletion, blitz3d_close→delete world, listener |
| HT_STREAM | `bbStream::~bbStream` | bbruntime/bbstream.cpp:16 | bbFile (bbCloseFile, file_destroy), TCPStream (bbCloseTCPStream, TCPServer::~TCPServer deleting accepted streams), UDPStream (bbCloseUDPStream, sockets_destroy) |
| HT_SURFACE | `Surface::~Surface` | blitz3d/surface.cpp:17 | All surfaces: MeshModel::Rep::~Rep (blitz3d/meshmodel.cpp:26), entity deletion |
| HT_TEXTURE | `Texture::~Texture` | blitz3d/texture.cpp:102 | bbFreeTexture, ClearWorld, load-failure deletes. Also forgets the texture's canvases (see 3b) |
| HT_BRUSH | `Brush::~Brush` | blitz3d/brush.cpp:82 | bbFreeBrush, ClearWorld |
| HT_IMAGE | `bbImage::~bbImage` | bbruntime/bbgraphics.cpp:17 | bbFreeImage, freeGraphics. Also forgets frame canvases (see 3b) |
| HT_BANK | `bbBank::~bbBank` | bbruntime/bbbank.cpp:15 | bbFreeBank, bank_destroy |
| HT_SOUND | `Sound::~Sound` (soloud) | bbruntime/bbaudio_soloud.cpp:39 | bbFreeSound, audio_destroy |

**Note on Texture/Brush reference counting**: both use a `Rep*` with `ref_cnt`.
The `Rep` is deleted when the last `Texture`/`Brush` handle goes away. The
`forget()` call goes in `~Texture`/`~Brush` (the outer object), not in `~Rep`,
because the BASIC-visible object is the `Texture`/`Brush`, not the `Rep`.

**Note on MeshModel**: `MeshModel::~MeshModel` (blitz3d/meshmodel.cpp:181) decrements
`rep->ref_cnt` and deletes `Rep` when it hits 0. `Rep::~Rep` (line 26) deletes all
surfaces. Since `Surface::~Surface` calls `forget`, surfaces are forgotten
automatically. No separate `forget` in `~MeshModel` is needed.

### 3b. Canvas forgetting (HT_CANVAS)

Canvases are gx-owned (`gx_graphics->freeCanvas()`). They are **not** forgotten
in a destructor we own. Instead:

| Canvas owner | Where forget happens | Notes |
|---|---|---|
| `bbImage` frames | `bbImage::~bbImage` (bbgraphics.cpp:17-20) | Before calling `gx_graphics->freeCanvas(frames[k])`, call `forget(frames[k])` for each frame |
| `Texture` canvases | `Texture::~Texture` → `CachedTexture::~CachedTexture` (cachedtexture.cpp:135) | The `Rep` holds `tex_frames` (vector<gxCanvas*>). Forget each canvas in `~Rep` or in `CachedTexture::~CachedTexture`. Since we own cachedtexture.cpp, add forget there. |
| `p_canvas` (private) | `freeGraphics` (bbgraphics.cpp:93) | `forget(p_canvas)` before `gx_graphics->freeCanvas(p_canvas)` |
| Front/back buffers | `forget(front_canvas)` + `forget(back_canvas)` in `bbEndGraphics`/`bbGraphics` (bbgraphics.cpp:412) | Explicitly forget both canvas pointers before gx frees them. No bulk clear_type. |

### 3c. Wrapper-side forget (gx-owned objects we don't own the destructor for)

| Family | Wrapper function | File:line | Notes |
|---|---|---|---|
| HT_FONT | `bbFreeFont` | bbgraphics.cpp:615 | `forget(f)` before `gx_graphics->freeFont(f)` |
| HT_MOVIE | `bbCloseMovie` | bbgraphics.cpp:664 | `forget(movie)` before `gx_graphics->closeMovie(movie)` |
| HT_DIR | `bbCloseDir` | bbfilesystem.cpp:87 | `forget(d)` before `gx_filesys->closeDir(d)` |
| HT_TIMER | `bbFreeTimer` | bbruntime.cpp:97 | `forget(t)` before `gx_runtime->freeTimer(t)` |
| HT_TCP_SERVER | `bbCloseTCPServer` | bbsockets.cpp:435 | `TCPServer` is not a `bbStream`; its destructor (line 251) deletes accepted streams. `forget(p)` in the wrapper. Accepted streams are forgotten via `~bbStream`. |

### 3d. Complete free-path coverage (audit)

| Free path | File:line | Covered by |
|---|---|---|
| `bbFreeBank` → `delete b` | bbbank.cpp:57 | `~bbBank` |
| `bank_destroy` cleanup loop | bbbank.cpp:~140 | `~bbBank` (via bbFreeBank) |
| `bbFreeEntity` → `delete e` | bbblitz3d.cpp:1361 | `~Entity` |
| `bbClearWorld` entities | bbblitz3d.cpp:1917 | `~Entity` (via bbFreeEntity) |
| `collapseMesh` → `delete e` | bbblitz3d.cpp:176 | `~Entity` |
| `bbLoadAnimSeq` temp entity `delete t` | bbblitz3d.cpp:1435 | `~Entity` |
| `blitz3d_close` → `delete world` | bbblitz3d.cpp:1954 | `~Entity` (children deleted by World) |
| `bbFreeTexture` → `delete t` | bbblitz3d.cpp:389 | `~Texture` |
| `bbClearWorld` textures | bbblitz3d.cpp:1923 | `~Texture` (via bbFreeTexture) |
| `bbLoadTexture` failure `delete t` | bbblitz3d.cpp:357 | `~Texture` |
| `bbLoadAnimTexture` failure `delete t` | bbblitz3d.cpp:367 | `~Texture` |
| `bbCreateTexture` failure `delete t` | bbblitz3d.cpp:471 | `~Texture` |
| `bbFreeBrush` → `delete b` | bbblitz3d.cpp:499 | `~Brush` |
| `bbClearWorld` brushes | bbblitz3d.cpp:1920 | `~Brush` (via bbFreeBrush) |
| Surface deletion via `Rep::~Rep` | blitz3d/meshmodel.cpp:26 | `~Surface` |
| Entity deletion cascading to surfaces | blitz3d/entity.cpp:57-58 | `~Entity` → `~Model` → `~MeshModel` → `~Rep` → `~Surface` |
| `bbFreeImage` → `delete i` | bbgraphics.cpp:764 | `~bbImage` |
| `freeGraphics` cleanup loop | bbgraphics.cpp:91 | `~bbImage` (via bbFreeImage) |
| `bbEndGraphics` → `freeGraphics` | bbgraphics.cpp:412 | `~bbImage` + explicit `forget(front/back canvas)` |
| `bbCloseFile` → `delete f` | bbfilesystem.cpp:70 | `~bbStream` (bbFile inherits bbStream) |
| `file_destroy` cleanup loop | bbfilesystem.cpp:145 | `~bbStream` (via bbCloseFile) |
| `bbCloseUDPStream` → `delete p` | bbsockets.cpp:329 | `~bbStream` (UDPStream inherits bbStream) |
| `bbCloseTCPStream` → `delete p` | bbsockets.cpp:415 | `~bbStream` (TCPStream inherits bbStream) |
| `TCPServer::~TCPServer` deletes accepted | bbsockets.cpp:252 | `~bbStream` (each TCPStream) |
| `sockets_destroy` cleanup loops | bbsockets.cpp:475-477 | `~bbStream` + wrapper `forget` for TCPServer |
| `bbFreeSound` → `delete sound` | bbaudio_soloud.cpp:60 | `~Sound` |
| `audio_destroy` cleanup | bbaudio_soloud.cpp:~196-205 | `~Sound` (via bbFreeSound) |
| `bbFreeFont` → `gx_graphics->freeFont` | bbgraphics.cpp:618 | wrapper `forget` (3c) |
| `bbCloseMovie` → `gx_graphics->closeMovie` | bbgraphics.cpp:665 | wrapper `forget` (3c) |
| `bbCloseDir` → `gx_filesys->closeDir` | bbfilesystem.cpp:88 | wrapper `forget` (3c) |
| `bbFreeTimer` → `gx_runtime->freeTimer` | bbruntime.cpp:98 | wrapper `forget` (3c) |

---

## 4. Type tags per family; validation via runtime `debug` flag

### 4a. Family tags

```cpp
enum HandleType {
    HT_ENTITY = 1,   // Entity and all subclasses
    HT_STREAM,       // bbStream: bbFile, TCPStream, UDPStream
    HT_TCP_SERVER,   // TCPServer (not a bbStream)
    HT_TEXTURE,      // Texture
    HT_BRUSH,        // Brush
    HT_SURFACE,      // Surface
    HT_IMAGE,        // bbImage
    HT_CANVAS,       // gxCanvas
    HT_FONT,         // gxFont
    HT_MOVIE,        // gxMovie
    HT_DIR,          // gxDir
    HT_BANK,         // bbBank
    HT_SOUND,        // Sound (soloud) / gxSound (FMOD)
    HT_TIMER         // gxTimer
};
```

### 4b. handle_type_tag<T> — maps each subclass to its family

```cpp
template <typename T> struct handle_type_tag { static const int value; };

// Entity family
template <> struct handle_type_tag<Entity>          { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Object>          { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Model>           { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MeshModel>       { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Camera>          { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Light>           { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Sprite>          { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MD2Model>        { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Q3BSPModel>      { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Terrain>         { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Pivot>           { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Mirror>          { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Listener>        { static const int value = HT_ENTITY; };

// Stream family
template <> struct handle_type_tag<bbStream>        { static const int value = HT_STREAM; };
template <> struct handle_type_tag<bbFile>          { static const int value = HT_STREAM; };
template <> struct handle_type_tag<TCPStream>       { static const int value = HT_STREAM; };
template <> struct handle_type_tag<UDPStream>       { static const int value = HT_STREAM; };

// Singletons / standalone
template <> struct handle_type_tag<TCPServer>       { static const int value = HT_TCP_SERVER; };
template <> struct handle_type_tag<Texture>         { static const int value = HT_TEXTURE; };
template <> struct handle_type_tag<Brush>           { static const int value = HT_BRUSH; };
template <> struct handle_type_tag<Surface>         { static const int value = HT_SURFACE; };
template <> struct handle_type_tag<bbImage>         { static const int value = HT_IMAGE; };
template <> struct handle_type_tag<gxCanvas>        { static const int value = HT_CANVAS; };
template <> struct handle_type_tag<gxFont>          { static const int value = HT_FONT; };
template <> struct handle_type_tag<gxMovie>         { static const int value = HT_MOVIE; };
template <> struct handle_type_tag<gxDir>           { static const int value = HT_DIR; };
template <> struct handle_type_tag<bbBank>          { static const int value = HT_BANK; };
// Sound: soloud backend uses file-local struct Sound; FMOD uses gxSound
template <> struct handle_type_tag<Sound>           { static const int value = HT_SOUND; };
template <> struct handle_type_tag<gxSound>         { static const int value = HT_SOUND; };
template <> struct handle_type_tag<gxTimer>         { static const int value = HT_TIMER; };
```

### 4c. Validation — runtime `debug` flag, not #ifdef

Uses the existing global `bool debug` (same flag as `debugEntity`, `debugTexture`,
etc. in bbblitz3d.cpp:57-70). No preprocessor conditionals.

```cpp
void *HandleTable::lookup(int handle, int expected_type) {
    if (!handle) return 0;
    unsigned u = (unsigned)handle;
    unsigned index = u >> 11;       // bits 30..11 (bit 31 is always 0)
    unsigned gen   = u & 0x7FF;     // bits 10..0
    if (index >= by_handle.size() || by_handle[index].generation != gen) {
        if (debug) RTEX("Stale or invalid handle");
        return 0;  // release: return null, caller's null-check handles it
    }
    if (by_handle[index].type_tag != expected_type) {
        if (debug) RTEX("Handle type mismatch");
        return 0;
    }
    return by_handle[index].base_ptr;
}
```

- **Debug** (`debug == true`): stale handle or type mismatch → `RTEX` (runtime error, same as existing debug checks).
- **Release** (`debug == false`): invalid handle → returns null. The existing null-checks in bb* functions (e.g. `if (!sound) return 0;`) handle this gracefully.

---

## 5. `bbH<T>` template wrapper

```cpp
// bbruntime/bb_handle.h

#ifdef _WIN32
// Windows 32-bit: handle IS the pointer. No table, no lookup, no wrapper.
#define BB_HANDLE(T) T*
#else
// macOS 64-bit: handle is a 4-byte unsigned int (bit 31 = 0, 20-bit index, 11-bit gen).

template <typename T>
struct bbH {
    int h;  // 4 bytes, trivially copyable. 0 = null.
    static_assert(sizeof(bbH<T>) == 4, "bbH must be 4 bytes on macOS");

    bbH() : h(0) {}
    explicit bbH(int handle) : h(handle) {}

    // From pointer: upcast to family base, find-or-allocate.
    bbH(T *p) {
        if (!p) { h = 0; return; }
        typename handle_family<T>::base *bp = p;  // implicit upcast
        h = g_handle_table.lookup_or_alloc(bp, handle_type_tag<T>::value);
    }

    // To pointer: validated lookup, downcast via family base (never from void*).
    operator T*() const {
        if (!h) return 0;
        void *vp = g_handle_table.lookup(h, handle_type_tag<T>::value);
        return vp ? static_cast<T*>(static_cast<typename handle_family<T>::base *>(vp)) : 0;
    }

    T *operator->() const { return static_cast<T*>(static_cast<typename handle_family<T>::base *>(g_handle_table.lookup(h, handle_type_tag<T>::value))); }
};

#define BB_HANDLE(T) bbH<T>
#endif
```

**Key design points:**

- **Windows**: `BB_HANDLE(T)` is plain `T*`. No struct, no table, no lookup.
  Zero behavioural change. The Windows struct version from v1 is deleted entirely.
- **macOS**: `sizeof(bbH<T>) == 4` so the x86-64 calling convention passes it
  as a 32-bit int, matching the BASIC int ABI.
- **Only rtSym-registered signatures change.** Function bodies keep using `T*`.
  The implicit conversion happens at the boundary:
  - Entry: generated code passes a 32-bit int → `bbH<T>(int)` constructor.
  - Body: `bbH<T>` → `T*` via `operator T*()` (validated lookup).
  - Return: `T*` → `bbH<T>` via the `bbH(T*)` constructor (find-or-allocate).
- **Pointer normalisation**: `by_ptr` is keyed on the family base pointer.
  `Camera* cam` and `Entity* e = cam` produce the same handle because both
  upcast to the same `Entity*` (via `handle_family<T>::base`) before `lookup_or_alloc`.
- **Downcast safety**: the stored pointer is cast through the family base type:
  `static_cast<T*>(static_cast<typename handle_family<T>::base *>(vp))`.
  Never `static_cast<T*>(void*)` directly. The downcast is well-defined because
  we know the dynamic type matches (validated by the type_tag check in debug mode).

**How the rtSym boundary works on macOS:**

```cpp
// Before (current, broken on 64-bit):
Entity *bbCreateMesh(Entity *parent);

// After (macOS only, via BB_HANDLE macro):
BB_HANDLE(Entity) bbCreateMesh(BB_HANDLE(Entity) parent);
// Expands to: bbH<Entity> bbCreateMesh(bbH<Entity> parent);
// Body unchanged: uses `parent` as Entity* (implicit conversion), returns Entity*.

// On _WIN32, BB_HANDLE(Entity) is just Entity*, so the signature is unchanged.
```

---

## 6. `sem_env` in declnode.cpp / prognode.cpp

Current state (from Prompt 1): `sem_env` is a pointer cast to `int` via
`(int)(intptr_t)sem_env` with a `TODO(handle-table)` comment at:
- `compiler/declnode.cpp:136-137`
- `compiler/prognode.cpp:58-59`

Both call `__bbDebugEnter(local(0), iconst((int)(intptr_t)sem_env), global(t))`.

**Minimal fix — `DebugEnvTable`:**

```cpp
// bbruntime/debug_env.h (new file, or added to bbruntime.h)
struct DebugEnvTable {
    std::map<int, void*> envs;  // id → sem_env pointer
    int  alloc(void *env);      // returns new id
    void *lookup(int id);       // returns env or 0
    void free(int id);          // remove
};
extern DebugEnvTable g_debug_envs;
```

- At compile time (codegen), `sem_env` is assigned an integer id via a
  counter. The id is emitted as a 32-bit `iconst`.
- At runtime, `_bbDebugEnter(frame, env_id, func)` receives the id and
  looks up the real pointer in `g_debug_envs`.
- The table is populated at program start (the linker/loader knows all
  `sem_env` pointers) and is only used in debug builds.

This avoids the 64-bit pointer-in-32-bit-int problem without changing the
x86-64 calling convention for debug calls.

---

## 7. Commit sequence

Each commit ≤ ~5 files, one concern.

| # | Commit | Files | Description |
|---|---|---|---|
| 1 | `handle_table: core + unit tests` | `bbruntime/bb_handle.h` (new), `bbruntime/bb_handle.cpp` (new), `bbruntime/CMakeLists.txt`, `tests/test_handle_table.cpp` (new) | `HandleTable` with generation bits, `bbH<T>`, `handle_type_tag`, `handle_base`. Unit tests: alloc/lookup/forget/reuse/stale-handle (generation mismatch)/type-mismatch/base-pointer normalisation. No rtSym signatures changed yet. |
| 2 | `handle_table: pilot — bbbank.cpp` | `bbruntime/bbbank.cpp`, `bbruntime/bbbank.h`, `bbruntime/CMakeLists.txt` (if needed) | Wrap all 14 bank rtSym signatures with `BB_HANDLE(bbBank)`. Add `forget` in `~bbBank`. C++ unit test calling bbCreateBank/bbFreeBank directly. |
| 3 | `handle_table: bbstream + bbsockets` | `bbruntime/bbstream.cpp`, `bbruntime/bbsockets.cpp`, `bbruntime/bbsockets.h` | Wrap stream (15), udp/tcp/server (12) signatures. Add `forget` in `~bbStream`. Wrapper `forget` for TCPServer. C++ unit tests for file/stream open/close. |
| 4 | `handle_table: bbfilesystem + bbruntime timer` | `bbruntime/bbfilesystem.cpp`, `bbruntime/bbruntime.cpp` | Wrap file/dir (5) and timer (2) signatures. Wrapper `forget` in `bbCloseDir`, `bbFreeTimer`. C++ unit tests. |
| 5 | `handle_table: bbgraphics` | `bbruntime/bbgraphics.cpp`, `bbruntime/bbgraphics.h` | Wrap image/canvas/font/movie (43) signatures. Add `forget` in `~bbImage` (frame canvases), wrapper `forget` for font/movie, explicit `forget(front/back canvas)` in bbEndGraphics. C++ unit tests where possible without graphics init. |
| 6 | `handle_table: bbaudio (soloud)` | `bbruntime/bbaudio_soloud.cpp`, `bbruntime/bbaudio_soloud.h`, `bbruntime/bbaudio.cpp` (if shared) | Wrap sound/channel (26) signatures. Add `forget` in `~Sound`. C++ unit tests (soloud init required). |
| 7 | `handle_table: bbblitz3d — entity family` | `bbruntime/bbblitz3d.cpp`, `bbruntime/bbblitz3d.h`, `blitz3d/entity.cpp` | Wrap entity/camera/light/model/sprite/md2/bsp/terrain/pivot/mirror/listener signatures (~169). Add `forget` in `~Entity`. C++ unit tests: create/free entity, parent/child, ClearWorld. |
| 8 | `handle_table: bbblitz3d — texture/brush/surface + canvas` | `bbruntime/bbblitz3d.cpp` (continued), `blitz3d/surface.cpp`, `blitz3d/texture.cpp`, `blitz3d/brush.cpp`, `blitz3d/cachedtexture.cpp` | Wrap texture/brush/surface signatures (~30). Add `forget` in `~Surface`, `~Texture` (incl. canvas forget), `~Brush`. Canvas forget in `CachedTexture::~CachedTexture`. C++ unit tests. |
| 9 | `handle_table: sem_env DebugEnvTable` | `bbruntime/bbruntime.cpp`, `compiler/declnode.cpp`, `compiler/prognode.cpp`, `bbruntime/debug_env.h` (new) | Replace `(int)(intptr_t)sem_env` casts with `DebugEnvTable` id. Remove TODO comments. |

**Verification after each commit:**
- `cmake --build build -- -k 0` — zero errors.
- Windows MSVC build unchanged (`BB_HANDLE(T)` is `T*` on `_WIN32`).
- C++ unit tests (run directly, no .bb interpreter needed): call bb* functions
  that don't require graphics/audio init. For modules that need init
  (graphics, audio, blitz3d), unit tests cover the handle-table mechanics
  (alloc/lookup/forget/stale) with mock objects; full integration testing
  happens once the x86-64 codegen and loader exist (Prompts 3-4).

**Note on .bb program testing**: .bb programs cannot run until the x86-64
codegen (Prompt 3) and macOS loader (Prompt 4) are complete. Per-commit
verification relies on C++ unit tests that call bb* functions directly.

**Note on userlib `*` params (Prompt 3 — codegen)**: When a userlib function
declares a parameter with `*` (e.g. `Function Foo(Bank *b)`), the compiler
passes the handle value directly as an integer argument. The x86-64 codegen
must translate that handle to a real pointer before calling the userlib C
function. This happens in `compiler/exprnode.cpp:94-100` (the `*` param
type handling). The generated call must do a handle→pointer lookup via
`g_handle_table.lookup(handle, HT_BANK)` (or the appropriate family tag) and
pass the resulting pointer to the userlib function. On Windows 32-bit this
is a no-op (handle IS the pointer).

---

## Open questions / risks

1. **`gxCanvas*` lifetime**: front/back buffers are created/destroyed by
   `gxGraphics` internally. We call `forget(front_canvas)` and `forget(back_canvas)`
   explicitly in `bbEndGraphics`/`bbGraphics` before gx frees them. Texture and
   image canvases are forgotten in the owner's destructor (we own those).

2. **`gxFont*` / `gxMovie*` / `gxDir*` / `gxTimer*`**: actual deletion happens
   inside gxruntime. We call `forget` in the bbruntime wrapper *before* calling
   into gxruntime, so the table is consistent even if gxruntime's delete path
   changes.

3. **`Surface*`**: owned by `MeshModel::Rep`, no independent free function.
   `forget` in `~Surface` (blitz3d/surface.cpp:17) covers all paths:
   `Rep::~Rep` deleting surfaces, entity deletion cascading down.

4. **`Sound*` in soloud backend**: the `Sound` struct is file-local to
   `bbaudio_soloud.cpp`. `forget` in `~Sound` (line 39) is in the same file —
   straightforward. The `handle_type_tag<Sound>` specialisation must be
   visible where `bbH<Sound>` is used; since both are in bbaudio_soloud.cpp,
   this works.

5. **`gxChannel*` (FMOD path)**: on the soloud backend, channels are `uint32_t`
   (SoLoud channel IDs), not pointers. No handle wrapping needed. The
   `BB_HANDLE` macro is applied conditionally: `#if BB_FMOD_ENABLED` → wrap
   with `bbH<gxChannel>`; `#else` → pass through as plain `int`/`uint32_t`.

6. **bbsgd.cpp (313 rtSyms)**: not in scope for this prompt. SGD is a separate
   runtime with its own object model. It will need its own survey and
   handle-table integration in a later prompt.

7. **`TCPServer` is not a `bbStream`**: it has its own destructor (bbsockets.cpp:251)
   that deletes accepted streams. The accepted streams are `TCPStream` (a
   `bbStream`), so they are forgotten via `~bbStream`. The `TCPServer` itself
   needs a wrapper-side `forget` in `bbCloseTCPServer` since we don't add
   `forget` to its destructor (it's not a family base — it's a singleton type).
