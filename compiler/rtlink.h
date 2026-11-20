//Portable runtime symbol declaration (macOS port, Prompt 4).
//
//The runtime registers every symbol with rtSym( "decorated name",address ).
//Names starting with '_' are internals the compiler calls directly
//("_bbStrLoad" is linked as "__bbStrLoad"); anything else is a BASIC command
//whose decoration gives its signature: "%Len$string", "$Left$string%count",
//"#Sin#degrees", "ExecFile$command%x=0". This is the parsing blitz/libs.cpp
//does on Windows (linkRuntime), without the Windows loader around it.

#ifndef RTLINK_H
#define RTLINK_H

#include "std.h"
#include "environ.h"

//Declare one runtime symbol to the front end (adding the command to
//runtimeEnviron->funcDecls) and return the module symbol its address is
//linked under: "_"+sym for internals, "_f"+lowercase name for commands.
//keywords, when given, receives the decorated name of each command.
string rtlinkDeclare( const string &sym,Environ *runtimeEnviron,vector<string> *keywords=0 );

#endif
