//Hand a finished Codegen_x64 over to an X64Module (Prompt 4 step 3).
//Shared by the test programs and the macOS blitzcc: copies the text section
//with every bound label as a module symbol, adds the data section on its own
//pages (writable), and turns the backend's fixups into module relocations.

#ifndef X64EXPORT_H
#define X64EXPORT_H

#include "x64module.h"
#include "../compiler/codegen_x64/codegen_x64.h"

//False if the backend reported an error (cg.ok() is false).
bool x64Export( asmjit::CodeHolder &holder,Codegen_x64 &cg,X64Module &mod );

#endif
