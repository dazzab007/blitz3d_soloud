//gxMovie on macOS: movies are not supported (the Windows version uses DirectShow).
//gxGraphics::openMovie always fails, so no gxMovie is ever created; this only
//provides the members the runtime links against.
#include "../gxmovie.h"
#include "../gxgraphics.h"

gxMovie::gxMovie( gxGraphics *g ):playing( false ),gfx( g ),canvas( 0 ){
	src_rect.left=src_rect.top=src_rect.right=src_rect.bottom=0;
}

gxMovie::~gxMovie(){
}

bool gxMovie::draw( gxCanvas *dest,int x,int y,int w,int h ){
	return false;
}
