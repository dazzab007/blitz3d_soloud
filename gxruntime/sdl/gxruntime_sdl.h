//Host hooks for the SDL gxRuntime (macOS port, Prompt 5).
#ifndef GXRUNTIME_SDL_H
#define GXRUNTIME_SDL_H

#include <string>
#include <vector>

//SDL scancode -> DirectInput-style scan code (0 if none); used by tests and
//anything that injects SDL key events.
int gxSdlScancodeToBlitz( int sdlScancode );

//Command line returned by CommandLine() (set by blitzcc/blitzrun).
void gxSdlSetCommandLine( const std::string &cmd );

//Test hook: read back what the next present() draws (the image area of the window
//framebuffer, BGRA, top row first). Returns false from gxSdlScreenshot() when
//there is no GL context (headless) or nothing was presented yet.
void gxSdlRequestScreenshot();
bool gxSdlScreenshot( int *w,int *h,std::vector<unsigned> *argb );

#endif
