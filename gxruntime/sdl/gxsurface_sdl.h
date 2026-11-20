//Software surface for the SDL gxRuntime (macOS port, Prompt 5). On Windows a
//ddSurf is a DirectDraw surface; here GxNativeSurface (declared in
//gxportable.h) is a plain ARGB32 pixel buffer, row 0 at the top.
#ifndef GXSURFACE_SDL_H
#define GXSURFACE_SDL_H

#include "../gxportable.h"
#include <vector>
#include <cstdint>

class GxNativeSurface{
public:
	int width,height;
	std::vector<uint32_t> pixels;	//0xAARRGGBB
	GxNativeSurface( int w,int h ):width( w ),height( h ),pixels( (size_t)w*h,0xff000000u ){}
	int pitch()const{ return width*4; }
	unsigned char *bits(){ return (unsigned char*)pixels.data(); }
};

#endif
