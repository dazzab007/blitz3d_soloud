//Access to the SDL runtime's OpenGL context for the GL 3D classes
//(gxscene_gl.cpp, gxmesh_gl.cpp). macOS port, Prompt 5 step 3.
#ifndef GXGL_SDL_H
#define GXGL_SDL_H

class gxCanvas;

//Make the window's GL context current. False when there is none (SDL dummy
//video driver: graphics run in memory only), in which case 3D is unavailable.
bool gxSdlMakeGLCurrent();

//Called when a canvas is destroyed so GL textures made from it can be dropped.
extern void (*gxCanvasFreedHook)( gxCanvas *c );

#endif
