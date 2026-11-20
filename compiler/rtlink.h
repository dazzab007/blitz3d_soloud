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
#include "parser.h"
#include "prognode.h"	//UserFunc
#include <set>

//Declare one runtime symbol to the front end (adding the command to
//runtimeEnviron->funcDecls) and return the module symbol its address is
//linked under: "_"+sym for internals, "_f"+lowercase name for commands.
//keywords, when given, receives the decorated name of each command.
string rtlinkDeclare( const string &sym,Environ *runtimeEnviron,vector<string> *keywords=0 );

//Userlibs (.decls files). Same format and errors as blitz/libs.cpp loadUserLib:
//  .lib "name"
//  Func%( a%, b#, c$, d* ) [: "alias"]
//Declares each function to the front end as a userlib C function, adds a
//UserFunc (ident, real proc name, lib) and the name to keywords. `seen` carries
//the lowercase identifiers already declared across files (duplicate check).
//Returns 0 or an error message (valid until the next call).
const char *rtlinkLoadUserLib( const string &declsFile,Environ *runtimeEnviron,
	vector<string> *keywords,vector<UserFunc> &userFuncs,set<string> &seen );

//Load every *.decls in dir (a missing directory is not an error). Returns 0 or
//"Error in userlib '<file>' - <reason>".
const char *rtlinkLoadUserLibs( const string &dir,Environ *runtimeEnviron,
	vector<string> *keywords,vector<UserFunc> &userFuncs );

#endif
