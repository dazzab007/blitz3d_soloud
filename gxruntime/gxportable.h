
#ifndef GXPORTABLE_H
#define GXPORTABLE_H

// Portable stand-ins for the Windows types used by the gx* interfaces.
// On _WIN32 the real Win32 types are used and this header adds nothing.

#ifndef _WIN32

class GxNativeSurface;	// native surface handle (DirectDraw on Windows)
typedef GxNativeSurface ddSurf;

struct RECT{
	long left,top,right,bottom;
};

#endif

#endif
