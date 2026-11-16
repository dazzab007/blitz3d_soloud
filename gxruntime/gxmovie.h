
#ifndef GXMOVIE_H
#define GXMOVIE_H

#ifdef _WIN32
#include "mmstream.h"    // multimedia stream interfaces
#include "amstream.h"    // DirectShow multimedia stream interfaces
#include "ddstream.h"    // DirectDraw multimedia stream interfaces
#endif

#include "gxcanvas.h"

class gxGraphics;

class gxMovie{

public:
#ifdef _WIN32
	gxMovie( gxGraphics *gfx,IMultiMediaStream *mm_stream );
#else
	gxMovie( gxGraphics *gfx );
#endif
	~gxMovie();

private:
	bool playing;
	RECT src_rect;
	gxGraphics *gfx;
	gxCanvas *canvas;
#ifdef _WIN32
	IDirectDrawSurface *dd_surf;
	IMediaStream *vid_stream;
	IDirectDrawMediaStream *dd_stream;
	IDirectDrawStreamSample *dd_sample;
	IMultiMediaStream *mm_stream;
#endif

	/***** GX INTERFACE *****/
public:

	bool draw( gxCanvas *dest,int x,int y,int w,int h );

	bool isPlaying()const{ return playing; }
	int getWidth()const{ return src_rect.right; }
	int getHeight()const{ return src_rect.bottom; }
};

#endif