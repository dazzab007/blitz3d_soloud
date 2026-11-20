//Stand-alone executables for macOS (Prompt 4 step 4).
//
//Layout: a copy of the launcher binary (blitzrun) followed by
//  [serialized X64Module][u32 flags][u64 module size][u64 FNV-1a of the module][8-byte magic "BBX64EXE"].
//The launcher finds the trailer at the end of its own file, deserializes the
//module, links it against the statically linked runtime and runs it. Appending
//to a Mach-O file leaves it loadable on x86-64; a code-signed (arm64) binary
//would need re-signing afterwards.

#ifndef X64EXE_H
#define X64EXE_H

#include "x64module.h"
#include <string>

enum{ X64EXE_DEBUG=1 };

//Copy launcher to outFile, append the module, mark it executable.
bool x64WriteExe( const std::string &launcher,const std::string &outFile,
	const X64Module &mod,unsigned flags,std::string &err );

//Read the module appended to exeFile. False (with err) if there is none.
bool x64ReadExe( const std::string &exeFile,X64Module &mod,unsigned &flags,std::string &err );

//Absolute path of the running executable ("" if unknown).
std::string x64SelfPath();

#endif
