//Host hooks for the SDL gxRuntime (macOS port, Prompt 5).
#ifndef GXRUNTIME_SDL_H
#define GXRUNTIME_SDL_H

#include <string>

//SDL scancode -> DirectInput-style scan code (0 if none); used by tests and
//anything that injects SDL key events.
int gxSdlScancodeToBlitz( int sdlScancode );

//Command line returned by CommandLine() (set by blitzcc/blitzrun).
void gxSdlSetCommandLine( const std::string &cmd );

#endif
