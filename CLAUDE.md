# Blitz3D → macOS (Intel) port: handoff

## 0. Apply the patch

```sh
git clone https://github.com/dazzab007/blitz3d_soloud && cd blitz3d_soloud
git checkout -b macos-port
git am /path/to/0001-Add-portable-macOS-Clang-64-bit-CMake-build-path.patch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -k 0
```

State: `config` and `soloud` build. `stdutil` and `compiler` still fail (list in prompt 1).
The Windows build is unchanged.

---

## Prompt A: context (paste first, or save as CLAUDE.md)

```
Project: porting Blitz3D (blitz3d_soloud) to Intel macOS, 64-bit, Clang. Branch: macos-port.

Architecture:
- compiler/: BASIC parser + semantics (portable) and codegen_x86/assem_x86 (emits 32-bit x86 machine code)
- linker/, bbruntime_dll/: load generated code via VirtualAlloc/LoadLibrary; dlltoexe patches PE files
- bbruntime/: Blitz command implementations; object pointers are returned as 32-bit BASIC ints
- gxruntime/: DirectDraw7, D3D7, DirectInput8, GDI, Win32. Only module that talks to graphics APIs
- blitz3d/: scene graph, talks only to gx* interfaces (portable apart from loader_x/minidx9)
- blitzide/, debugger/: MFC (Windows only, not being ported)

Rules:
- Windows MSVC 32-bit build must keep working. Guard platform code with #ifdef _WIN32, don't delete it.
- Non-Windows build is gated by BB_PORTABLE_BUILD in the top-level CMakeLists.txt. Add modules
  to that block only when they compile.
- Use bb_target_options(target) in CMake, never raw /Gz.
- BASIC ints stay 32-bit.
- Small, reviewable commits. Explain each change before making it.
- Never bulk-edit generated files (e.g. compiler/assem_x86/asm_insts.cpp). Fix headers or the generator instead.
- "It compiles" is not verification for compiler/runtime changes: explain the runtime effect of each change.
```

## Prompt 1: get stdutil + compiler compiling

```
Make stdutil and compiler compile with Clang on 64-bit macOS, keeping Windows behaviour identical.
Remaining errors:
- stdutil/stdutil.cpp: includes windows.h
- compiler/assem.h, assem_x86/asm_insts.cpp: backslash include paths ('..\linker\linker.h') -> use '/'
- codegen_x86/tile.cpp: itoa overload ambiguity
- decl.cpp: wchar_t/char mismatch (Win32 W-vs-A API?) and missing <cstring>
- declnode.cpp, prognode.cpp: pointer cast to int. Flag each for the handle-table work
  (intptr_t for now, with a TODO)
- exprnode.cpp: _controlfp/_asm FPU setup for constant folding -> <cfenv> on non-Windows
- parser.cpp: MAX_PATH / _fullpath -> PATH_MAX / realpath
Build with: cmake --build build -k 0. Done when the compiler library links with zero errors.
```

## Prompt 2: handle table

```
In bbruntime, BASIC ints are 32-bit but object pointers (Entity*, Texture*, Brush*, Surface*, Image*,
Sound*, bank*, stream* etc.) are 64-bit on macOS. Design and implement a handle table:
- int handle <-> pointer, 0 = null, freed slots reused, debug-mode validation of stale handles
- Apply it at the runtime boundary only: the functions registered via rtSym(...) in bb*.cpp
  (e.g. blitz3d_link in bbblitz3d.cpp). Internal C++ code keeps using real pointers.
- On Windows 32-bit the handle can just be the pointer (compile-time switch) so behaviour is unchanged.
First, survey every rtSym signature that passes or returns an object, and show me the list and plan
before you edit anything.
```

## Prompt 3: x86-64 backend

```
Adapt compiler/codegen_x86 + assem_x86 to also emit x86-64 for macOS (keep x86-32 for Windows):
- REX prefixes, 64-bit registers for pointers, BASIC ints stay 32-bit
- System V AMD64 calling convention (RDI,RSI,RDX,RCX,R8,R9 / XMM0-7), 16-byte stack alignment at calls
- RIP-relative addressing for globals and string constants
- Callee-saved: RBX, RBP, R12-R15
Start by reading tile.cpp and codegen_x86.cpp, then propose the design (separate codegen_x64 dir vs
mode flag) before writing code. Add a tiny test harness that compiles a .bb snippet, mmaps the code
PROT_EXEC and runs it against stub runtime functions.
```

## Prompt 4: loader/linker for macOS

```
Port the code loading path (linker/linker.cpp, bbruntime_dll.cpp, blitz/libs.cpp) to macOS:
- LoadLibrary/GetProcAddress -> dlopen/dlsym (runtime.dylib), or link the runtime statically
- VirtualAlloc RWX -> mmap(PROT_READ|PROT_WRITE), then mprotect(PROT_READ|PROT_EXEC)
- _set_se_translator SEH -> sigaction for SIGSEGV/SIGFPE/SIGILL/SIGBUS mapped to Blitz runtime errors
- "Create executable" (dlltoexe): propose an approach (e.g. launcher binary + appended code blob)
Get `blitzcc` running a console-only Print "Hello" program end to end.
```

## Prompt 5: gxruntime on SDL2 + OpenGL

```
Create a non-Windows gxruntime backend using SDL2 + OpenGL 3.3 core, keeping the existing gx* class
interfaces so blitz3d/ is untouched. Order:
1. Window, mode list, input (keyboard, mouse, joystick) and timer via SDL2
2. 2D: gxCanvas as GL texture + CPU shadow buffer (Lock/WritePixel/CopyRect), fonts via stb_truetype
3. 3D: gxMesh -> VBO/IBO, gxScene -> fixed-function-emulating shader (8 lights, fog, 8 texture
   stages, the blend modes and FX_ flags in gxscene.h)
Use the gxscene.cpp D3D7 state as the spec. Replace loader_x's minidx9 dependency with a small portable
.x text/binary parser. Test with the BLITZ3D_INSTALL samples.
```

## Blitz3D-specific rules
- Generated: compiler/assem_x86/asm_insts.cpp — never bulk edit.
- Windows ABI is frozen: no signature/struct layout changes on _WIN32 paths.
- Object pointers crossing rtSym go through bbH<T>; BBStr*, arrays, Type instances stay raw pointers.

## Editing discipline
- Never rewrite a whole existing file with Write; use targeted edits. After editing, `git diff` must show only intended lines.
- Code generator changes: show the emitted assembly before/after for one example.
- Before a commit that touches _WIN32 paths, say so explicitly: it needs a Windows build check.
