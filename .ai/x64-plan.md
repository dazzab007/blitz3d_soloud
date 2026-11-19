# Prompt 3 plan: x86-64 (macOS, System V) backend for the Blitz3D compiler

Branch `macos-port`. Windows keeps the existing x86-32 backend, byte-identical.

## 1. Key finding: the IR has no pointer width

`compiler/codegen.h` TNodes carry no types. Everything is a 4-byte value:
pointers (BBStr*, BBObj*, arrays, label addresses) and ints alike. 4-byte
assumptions are hard-coded in the front end:

| Site | Assumption |
|---|---|
| node.cpp:56,59 | frame layout: args at `p_size+20`, locals at `-4-l_size` |
| declnode.cpp:151,167 | params `size()*4`; Type fields `k*4` |
| exprnode.cpp:101,106,179,181 | arg offsets `k*4`; userlib `*` deref `+4` |
| varnode.cpp:90,94,98,142 | BBArray header `k*4+8`, `k*4+12`; element stride `*4`; `sz=4` |
| stmtnode.cpp:111 | BBArray header `k*4+12` |

On x86-64 a 32-bit slot can't hold a pointer, and `IR_CALL` can't tell whether
the callee returns an int (eax, upper bits undefined) or a pointer (rax).

## 2. Design decisions

**D1. Separate backend, shared front end.** New `compiler/codegen_x64/`
implementing the existing `Codegen` interface. Built only on non-Windows. The
compiler picks the backend with `#ifdef _WIN32`. The x86-32 backend and
`assem_x86` are untouched.

**D2. Encode with asmjit, not a hand-extended assembler.** The x86-32 path emits
text and re-parses it with `assem_x86` (a 585-line opcode table). Extending that
table for REX/64-bit is error-prone (an earlier local-model edit broke it
silently). `codegen_x64` emits machine code directly via asmjit (zlib licence,
FetchContent like SoLoud), into the existing `Module` interface. Named Blitz
labels (`_f…`, `_v…`, `_a…`, jump targets) stay `Module` symbols and relocations,
so there's one symbol system.

**D3. 8-byte slots on x86-64.** Every BASIC variable, array element and Type
field is one 8-byte slot (BBField is already 8 bytes on macOS). One shared header,
`compiler/bb_abi.h`, defines `BB_SLOT` (4 on Windows, 8 on macOS) and the
BBArray/BBObj header offsets. The runtime `static_assert`s its struct
`offsetof`s against these constants, so the compiler and runtime can't drift.

**D4. Tag pointer-width only at the leaves.** Add `bool wide` to TNode
(default false, so Windows is unchanged). The front end sets `wide` only where a
value's Type is a pointer (string, struct/object, array, vector) on: `IR_MEM`
loads, `IR_CALL` returns, `IR_ARG`, `IR_MOVE` stores. The backend infers the
rest: `IR_LOCAL`/`IR_GLOBAL` are addresses (always wide); `IR_ADD`/`IR_SUB` with
one wide operand is pointer arithmetic (sign-extend the int side, 64-bit add).
Everything else uses 32-bit ops; int results from calls are sign-extended
(`movsxd`).

**D5. Calls.** System V: RDI, RSI, RDX, RCX, R8, R9, then stack; floats in
XMM0-7 by position class; 16-byte stack alignment at every call; int returns in
eax (sign-extend), wide returns in rax, float returns in xmm0. Runtime functions
are called through absolute 64-bit addresses (new `ABS64` reloc), never rel32
(the runtime dylib can be more than 2 GB away).
*Refined in commit 6:* System V is used for every **C** call (runtime commands,
`__bb*` helpers, userlibs). Calls between **Blitz** functions keep the IR's
stack-argument model: the caller's outgoing area stays at `[rsp]` and the
callee reads params at `[rbp+16+8*k]`, so no register homing or param-type
plumbing is needed. The IR's float values are bit patterns with no type, so the
front end tags each call: `IR_CALL`/`IR_FCALL` `sconst` is empty for a Blitz
function, or `"C"` plus one char per arg (`i` GP register, `f` XMM register).

**D6. Frame layout via the backend.** `Codegen` gains
`argOffset(k)`/`localOffset(k)` (or equivalent). The x86-32 backend returns
today's values, so Windows output is identical. The front end stops hard-coding
`+20` and `-4`.

**D7. Module relocs.** `Module::addReloc` grows a kind: `REL32` (existing
`pcrel=true`), `ABS32` (existing `false`) and new `ABS64`. The Windows linker
only ever sees the first two.

**Rejected:** squeezing every pointer under 4 GB with `-pagezero_size` plus a low
arena (no IR changes). It's fragile on current macOS: Wine developers found
macOS refuses to run executables with a page-zero smaller than 4 GB in some
configurations. A C-emitting backend would need clang on users' Macs at runtime
and still needs the same type tagging.

## 3. Test harness (no real loader needed)

`tests/x64/`: a small in-memory `Module` (byte buffer, symbol table, reloc list)
plus a mini loader: `mmap` RW, copy, apply `REL32`/`ABS64` relocs against a table
of stub runtime functions, `mprotect` RX, call the entry point. Stage 1 tests feed
hand-built TNode trees. Stage 2 runs real `.bb` snippets through the parser with a
fake runtime declaration list (e.g. a stub `Print` that records its argument).
This harness is the seed of Prompt 4's loader.

## 4. Files

New: `compiler/bb_abi.h`, `compiler/codegen_x64/{codegen_x64.h,.cpp,tile64.cpp}`,
`tests/x64/{harness.h,.cpp,test_codegen_x64.cpp}`.
Changed: `codegen.h` (TNode `wide`, frame offset hooks), `node.cpp`,
`declnode.cpp`, `exprnode.cpp`, `varnode.cpp`, `stmtnode.cpp` (use `BB_SLOT` and
the hooks, set `wide`), `linker/linker.h` (reloc kind), `bbruntime/basic.cpp`
(static_asserts), CMake (asmjit portable-only, new targets), the compiler
backend selection.

## 5. Commit sequence (one local-model session each, `/clear` between)

1. **ABI constants.** Add `bb_abi.h`; replace the hard-coded 4/8/12/`*4` sites
   with `BB_SLOT`/offset constants (values identical on Windows); runtime
   static_asserts. Test: Windows CI green; on the PC, compile a sample `.bb` with
   the old and new `blitzcc` and compare the `.exe` files (must be identical).
2. **Frame hooks.** `argOffset`/`localOffset` on `Codegen`; x86-32 returns
   today's values; front end uses them. Same PC identical-exe check.
3. **TNode `wide` tagging.** Add the field (default false) and set it in the
   front end from expression Types. Windows ignores it. Mac test: a debug dump
   of the TNode trees for a small program shows `wide` exactly on
   string/object/array values.
4. **asmjit + skeleton.** FetchContent asmjit (portable only); `Codegen_x64`
   stub; harness `Module` + mini loader; test: emit `mov eax,42; ret`, run it,
   get 42.
5. **Integer tiles.** CONST, LOCAL/GLOBAL, MEM/MOVE (32 and 64), ADD/SUB/MUL/DIV,
   AND/OR/XOR/shifts, NEG, SETxx, JUMP/JUMPT/JUMPF/JUMPGE. Tests: one TNode tree
   per op, including wide pointer + int index.
6. **Calls and frames.** enter/leave prologue/epilogue, IR_CALL/IR_RETURN,
   System V args (more than 6 args too), alignment, JSR/RET (Gosub),
   `ABS64` relocs. Tests: call stub functions with 1, 6 and 8 args; nested calls;
   int vs wide returns.
7. **Floats.** FCONST via data, FADD..FDIV, FNEG, FSETxx, CAST/FCAST, FCALL/
   FRETURN, mixed int/float/wide args. Tests per op.
8. **Data.** i_data/s_data/p_data (8-byte pointers)/align_data; Type
   descriptor tables. Test: a Type with fields, read back through the harness.
9. **Front-end integration.** Real `.bb` snippets: arithmetic, If/For/While,
   functions, Gosub, strings (link the real `basic.cpp`/`bbstring.cpp` if they
   compile on macOS), Types (New/First/Each/Delete), arrays and Dim.
10. **Debug mode.** `__bbDebugStmt`/`__bbDebugEnter` emission with env ids.

Userlib `*` params (exprnode.cpp:94-100) need a handle-to-pointer lookup. That
waits for Prompt 4, since userlibs need `dlopen` anyway.

## 6. Rules for every commit

- Windows output must not change: commits 1-3 need the PC identical-exe check.
- Never edit `compiler/assem_x86/asm_insts.cpp`.
- No `.bb` program runs end to end until Prompt 4. Harness tests are the proof.
- If a step needs a design decision not covered here, stop and ask.

## 7. Wide-tag rules (settled in commits 3b/3c)

- Store width = the destination `IR_MEM`'s `wide` flag. `MOVE W` is informational only.
- Call arguments (`MEM ARG k`): wide args are stored and passed as 64 bits; int args are stored as 32 bits, so the upper half of the register/stack slot is undefined and callees read only the low 32 bits (as System V allows for `int`). (Updated in commit 6.)
- `RETURN` width is inferred from the returned expression; a `CONST 0` return in a pointer-returning function is fine (32-bit mov zero-extends).
- `__bbObjEachFirst/Next`, `__bbObjToHandle`, `__bbObjDelete` are int/void: not wide. `__bbObjFromHandle`, `__bbObjNew`, `__bbStr*` producers, runtime `$` functions and user functions returning string/object are `CALL W`.
- Commit 8: type descriptors (e.g. `_tthing`) currently emit null pointer fields as `idata 0`. On x86-64 every pointer field must be an 8-byte `pdata`, with padding matching the runtime `BBObjType`/`BBObj` layout (static_assert the offsets).
- Coverage proof: `tests/x64/widetest.bb` / `widetest.dump` (objects, strings, arrays of both, Each, Handle/Object, Delete, user and runtime functions returning pointers).
