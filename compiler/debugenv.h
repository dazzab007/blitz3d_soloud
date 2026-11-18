/*

  Debug env id: maps an Environ* to a small int that fits in a 32-bit BASIC
  int, for the __bbDebugEnter call emitted in debug builds.

  On _WIN32 (32-bit) the pointer itself is the id: behaviour unchanged.
  On non-Windows (64-bit) each Environ* is registered in a growable table
  and an index+1 is returned; 0 = null. The future Mac debugger resolves
  ids with debugEnvLookup.

  */

#ifndef DEBUGENV_H
#define DEBUGENV_H

#include "std.h"
#include "environ.h"

#ifdef _WIN32

inline int debugEnvId( Environ *e ){ return (int)e; }

#else

Environ *debugEnvLookup( int id );

#endif // _WIN32

int debugEnvId( Environ *e );

#endif // DEBUGENV_H
