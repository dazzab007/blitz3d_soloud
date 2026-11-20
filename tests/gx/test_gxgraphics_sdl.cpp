//SDL gxGraphics step 2 (Prompt 5): the software canvas (primitives, clipping,
//blits, collision masks), fonts, image loading, flip, and - when a GL context can
//be made - the presented window image. Runs headless with SDL's dummy driver for
//everything except the presentation check.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <SDL.h>
#include "../../gxruntime/gxruntime.h"
#include "../../gxruntime/gxgraphics.h"
#include "../../gxruntime/sdl/gxruntime_sdl.h"

static int tests_run=0,tests_failed=0;
#define CHECK(c) do{ ++tests_run; if( !(c) ){ ++tests_failed; printf( "FAIL %s:%d: %s\n",__FILE__,__LINE__,#c ); } }while(0)
#define CHECK_EQ(a,b) do{ ++tests_run; unsigned _a=(unsigned)(a),_b=(unsigned)(b); if( _a!=_b ){ ++tests_failed; printf( "FAIL %s:%d: %s == %s (got %08x, want %08x)\n",__FILE__,__LINE__,#a,#b,_a,_b ); } }while(0)

static const unsigned RED=0xffff0000,GREEN=0xff00ff00,BLUE=0xff0000ff,BLACK=0xff000000,WHITE=0xffffffff;

static int countColor( gxCanvas *c,unsigned argb ){
	int n=0;
	for( int y=0;y<c->getHeight();++y ) for( int x=0;x<c->getWidth();++x ) if( c->getPixel( x,y )==argb ) ++n;
	return n;
}

static void test_primitives( gxGraphics *g ){
	gxCanvas *c=g->createCanvas( 32,24,0 );
	CHECK( c!=0 );
	CHECK( c->getWidth()==32 && c->getHeight()==24 && c->getDepth()==32 );
	CHECK_EQ( c->getPixel( 5,5 ),BLACK );			//created canvases are cleared

	c->setClsColor( 0x102030 );c->cls();
	CHECK_EQ( c->getPixel( 0,0 ),0xff102030 );
	CHECK_EQ( c->getPixel( 31,23 ),0xff102030 );
	CHECK_EQ( c->getClsColor(),0xff102030 );

	c->setColor( 0xff0000 );c->plot( 3,4 );
	CHECK_EQ( c->getPixel( 3,4 ),RED );
	CHECK_EQ( c->getColor(),RED );
	c->plot( -1,0 );c->plot( 32,0 );c->plot( 0,24 );			//out of range: ignored
	CHECK_EQ( countColor( c,RED ),1 );

	//solid rect, clipped at the edge
	c->setClsColor( 0 );c->cls();
	c->setColor( 0x00ff00 );c->rect( 28,20,10,10,true );
	CHECK_EQ( countColor( c,GREEN ),4*4 );
	c->cls();
	c->rect( 2,2,5,4,false );								//outline: 5*4 box
	CHECK_EQ( countColor( c,GREEN ),2*5+2*2 );
	CHECK_EQ( c->getPixel( 2,2 ),GREEN );
	CHECK_EQ( c->getPixel( 6,5 ),GREEN );
	CHECK_EQ( c->getPixel( 4,3 ),BLACK );					//hollow

	//lines: horizontal, vertical, diagonal, clipped
	c->cls();c->setColor( 0x0000ff );
	c->line( 0,0,31,0 );CHECK_EQ( countColor( c,BLUE ),32 );
	c->cls();c->line( 5,0,5,23 );CHECK_EQ( countColor( c,BLUE ),24 );
	c->cls();c->line( 0,0,10,10 );CHECK_EQ( countColor( c,BLUE ),11 );
	CHECK_EQ( c->getPixel( 7,7 ),BLUE );
	c->cls();c->line( -50,12,100,12 );CHECK_EQ( countColor( c,BLUE ),32 );
	c->cls();c->line( -10,-10,-5,-5 );CHECK_EQ( countColor( c,BLUE ),0 );
	c->cls();c->line( 9,9,9,9 );CHECK_EQ( countColor( c,BLUE ),1 );

	//oval: filled is symmetric and inside its box
	c->cls();c->setColor( 0xff0000 );c->oval( 4,4,16,12,true );
	int filled=countColor( c,RED );
	CHECK( filled>100 && filled<16*12 );
	CHECK_EQ( c->getPixel( 12,10 ),RED );					//centre
	CHECK_EQ( c->getPixel( 4,4 ),BLACK );					//box corner
	for( int y=0;y<24;++y ) for( int x=0;x<32;++x ){
		if( c->getPixel( x,y )==RED ) CHECK( x>=4 && x<20 && y>=4 && y<16 );
	}
	c->cls();c->oval( 4,4,16,12,false );
	CHECK( countColor( c,RED )>20 && countColor( c,RED )<filled );
	CHECK_EQ( c->getPixel( 12,10 ),BLACK );					//hollow

	//origin and viewport
	c->cls();c->setColor( 0xffffff );
	c->setOrigin( 10,10 );c->plot( 0,0 );
	CHECK_EQ( c->getPixel( 0,0 ),WHITE );					//getPixel also uses the origin
	int ox,oy;c->getOrigin( &ox,&oy );CHECK( ox==10 && oy==10 );
	c->setOrigin( 0,0 );c->cls();
	c->setViewport( 8,8,8,8 );
	c->rect( 0,0,32,24,true );
	CHECK_EQ( countColor( c,WHITE ),64 );					//only the 8x8 viewport got painted
	c->setViewport( 0,0,32,24 );
	CHECK_EQ( countColor( c,WHITE ),64 );					//and nothing outside it
	int vx,vy,vw,vh;c->setViewport( 8,8,100,100 );c->getViewport( &vx,&vy,&vw,&vh );
	CHECK( vx==8 && vy==8 && vw==24 && vh==16 );			//clipped to the canvas
	c->setViewport( 0,0,32,24 );

	g->freeCanvas( c );
}

static void test_blit_mask_collide( gxGraphics *g ){
	gxCanvas *src=g->createCanvas( 8,8,0 );
	src->setColor( 0xff00ff );src->rect( 0,0,8,8,true );		//magenta everywhere
	src->setColor( 0xffffff );src->rect( 2,2,4,4,true );		//white square inside
	src->setMask( 0xff00ff );								//magenta is transparent

	gxCanvas *dst=g->createCanvas( 16,16,0 );
	dst->setClsColor( 0x0000ff );dst->cls();
	dst->blit( 4,4,src,0,0,8,8,false );
	CHECK_EQ( dst->getPixel( 4,4 ),BLUE );					//masked out
	CHECK_EQ( dst->getPixel( 7,7 ),WHITE );
	CHECK_EQ( countColor( dst,WHITE ),16 );

	dst->cls();dst->blit( 4,4,src,0,0,8,8,true );			//solid ignores the mask
	CHECK_EQ( dst->getPixel( 4,4 ),0xffff00ff );
	CHECK_EQ( countColor( dst,WHITE ),16 );

	//handle shifts the drawing, clipping at the edges
	src->setHandle( 4,4 );
	dst->cls();dst->blit( 0,0,src,0,0,8,8,true );
	CHECK_EQ( dst->getPixel( 3,3 ),0xffff00ff );
	CHECK_EQ( dst->getPixel( 4,4 ),BLUE );
	int hx,hy;src->getHandle( &hx,&hy );CHECK( hx==4 && hy==4 );
	src->setHandle( 0,0 );
	dst->cls();dst->blit( 12,12,src,0,0,8,8,true );
	CHECK_EQ( countColor( dst,0xffff00ff )+countColor( dst,WHITE ),16 );	//only the 4x4 corner shows
	dst->cls();dst->blit( -100,-100,src,0,0,8,8,true );
	CHECK_EQ( countColor( dst,BLUE ),256 );

	//overlapping blit within one canvas
	dst->setClsColor( 0 );dst->cls();
	dst->setColor( 0xff0000 );dst->rect( 0,0,4,4,true );
	dst->blit( 2,2,dst,0,0,4,4,true );
	CHECK_EQ( dst->getPixel( 5,5 ),RED );
	CHECK_EQ( dst->getPixel( 2,2 ),RED );

	//pixel-exact collision uses the mask bitmap
	gxCanvas *a=g->createCanvas( 8,8,0 ),*b=g->createCanvas( 8,8,0 );
	a->setMask( 0 );b->setMask( 0 );							//black is transparent
	a->setColor( 0xffffff );a->rect( 0,0,4,4,true );			//opaque top-left quarter
	b->setColor( 0xffffff );b->rect( 4,4,4,4,true );			//opaque bottom-right quarter
	CHECK( a->collide( 0,0,b,0,0,true ) );					//boxes overlap
	CHECK( !a->collide( 0,0,b,0,0,false ) );				//but the opaque parts do not
	CHECK( a->collide( 0,0,b,-4,-4,false ) );				//shift b so its quarter meets a's
	CHECK( !a->collide( 0,0,b,20,0,false ) );
	CHECK( a->rect_collide( 0,0,1,1,2,2,false ) );
	CHECK( !a->rect_collide( 0,0,5,5,2,2,false ) );
	CHECK( !a->rect_collide( 0,0,50,50,2,2,false ) );
	a->setColor( 0xffffff );a->plot( 7,7 );					//modifying updates the cached mask
	CHECK( a->rect_collide( 0,0,6,6,2,2,false ) );
	g->freeCanvas( a );g->freeCanvas( b );

	g->freeCanvas( src );g->freeCanvas( dst );
}

static void test_pixels_copy( gxGraphics *g ){
	gxCanvas *c=g->createCanvas( 8,8,0 );
	c->setPixel( 1,1,0x123456 );
	CHECK_EQ( c->getPixel( 1,1 ),0x123456 );	//alpha is kept as given
	c->lock();
	c->setPixelFast( 2,2,0xff654321 );
	CHECK_EQ( c->getPixelFast( 2,2 ),0xff654321 );
	gxCanvas *d=g->createCanvas( 8,8,0 );
	d->lock();
	d->copyPixelFast( 3,3,c,2,2 );
	d->unlock();
	CHECK_EQ( d->getPixel( 3,3 ),0xff654321 );
	c->unlock();
	d->copyPixel( 4,4,c,1,1 );
	CHECK_EQ( d->getPixel( 4,4 ),0x123456 );
	d->copyPixel( 99,99,c,1,1 );				//out of range: ignored

	//gxGraphics::copy: straight and stretched
	c->setColor( 0xff0000 );c->rect( 0,0,4,4,true );
	gxCanvas *e=g->createCanvas( 16,16,0 );
	g->copy( e,0,0,16,16,c,0,0,8,8 );			//2x stretch
	CHECK_EQ( e->getPixel( 7,7 ),RED );
	CHECK_EQ( e->getPixel( 8,8 ),BLACK );
	g->copy( e,0,0,4,4,c,0,0,4,4 );
	CHECK_EQ( e->getPixel( 3,3 ),RED );
	g->freeCanvas( c );g->freeCanvas( d );g->freeCanvas( e );
}

static void test_fonts( gxGraphics *g ){
	gxFont *f=g->getDefaultFont();
	CHECK( f!=0 );
	if( !f ) return;
	CHECK( f->getHeight()>=8 && f->getHeight()<=20 );
	CHECK( f->getWidth( "HHHH" )==4*f->getWidth( "H" ) );		//monospaced default font
	CHECK( f->getWidth( "" )==0 );
	CHECK( f->isPrintable( 'A' ) && !f->isPrintable( 5 ) );

	gxCanvas *c=g->createCanvas( 64,32,0 );
	c->setFont( f );
	c->setColor( 0x00ff00 );
	c->text( 2,2,"Hi" );
	int green=countColor( c,GREEN );
	CHECK( green>10 );
	for( int y=0;y<32;++y ) for( int x=0;x<64;++x ){
		unsigned p=c->getPixel( x,y );
		CHECK( p==GREEN || p==BLACK );							//no antialiasing, no stray colours
	}
	//text stays inside its box
	for( int y=0;y<32;++y ) for( int x=0;x<64;++x ){
		if( c->getPixel( x,y )==GREEN ) CHECK( x>=2 && x<2+f->getWidth( "Hi" ) && y>=2 && y<2+f->getHeight() );
	}
	//clipped text does not crash and draws less
	c->setClsColor( 0 );c->cls();c->text( -3,2,"Hi" );
	CHECK( countColor( c,GREEN )>0 && countColor( c,GREEN )<=green );
	c->cls();c->text( 100,2,"Hi" );CHECK_EQ( countColor( c,GREEN ),0 );
	c->setColor( 0 );c->cls();c->text( 2,2,"Hi" );					//black text: still drawn (0x10 fix)
	g->freeCanvas( c );

	//other fonts
	gxFont *a=g->loadFont( "arial",20,0 );
	CHECK( a!=0 );
	if( a ){
		CHECK( a->getHeight()>f->getHeight() );
		CHECK( a->getWidth( "iiii" )<a->getWidth( "WWWW" ) );		//proportional
		CHECK( g->verifyFont( a )==a );
		gxFont *b=g->loadFont( "arial",20,gxFont::FONT_BOLD|gxFont::FONT_UNDERLINE );
		CHECK( b!=0 );
		if( b ) g->freeFont( b );
		g->freeFont( a );
		CHECK( g->verifyFont( a )==0 );
	}
	gxFont *u=g->loadFont( "NoSuchFontName",14,0 );		//unknown names fall back to a system font
	CHECK( u!=0 );
	if( u ) g->freeFont( u );
	CHECK( g->loadFont( "/no/such/file.ttf",14,0 )==0 );
}

static void test_images( gxGraphics *g ){
	//a 2x2 PPM (P6): red, green / blue, white
	char tmpl[]="/tmp/gximgXXXXXX";
	std::string dir=mkdtemp( tmpl );
	std::string path=dir+"/img.ppm";
	FILE *f=fopen( path.c_str(),"wb" );
	fprintf( f,"P6\n2 2\n255\n" );
	unsigned char px[]={ 255,0,0, 0,255,0, 0,0,255, 255,255,255 };
	fwrite( px,1,sizeof(px),f );fclose( f );

	gxCanvas *c=g->loadCanvas( path,0 );
	CHECK( c!=0 );
	if( c ){
		CHECK( c->getWidth()==2 && c->getHeight()==2 );
		CHECK_EQ( c->getPixel( 0,0 ),RED );
		CHECK_EQ( c->getPixel( 1,0 ),GREEN );
		CHECK_EQ( c->getPixel( 0,1 ),BLUE );
		CHECK_EQ( c->getPixel( 1,1 ),WHITE );
		CHECK( g->verifyCanvas( c )==c );
		g->freeCanvas( c );
		CHECK( g->verifyCanvas( c )==0 );
	}
	CHECK( g->loadCanvas( dir+"/missing.png",0 )==0 );
	f=fopen( (dir+"/bad.png").c_str(),"wb" );fwrite( "not an image",1,12,f );fclose( f );
	CHECK( g->loadCanvas( dir+"/bad.png",0 )==0 );
	unlink( path.c_str() );unlink( (dir+"/bad.png").c_str() );rmdir( dir.c_str() );

	CHECK( g->createCanvas( 0,10,0 )==0 );
	CHECK( g->createCanvas( 10,-1,0 )==0 );
}

static void test_flip_and_lifecycle( gxRuntime *rt ){
	gxGraphics *g=rt->openGraphics( 40,30,32,0,gxGraphics::GRAPHICS_WINDOWED );
	CHECK( g!=0 );
	if( !g ) return;
	CHECK( rt->openGraphics( 10,10,32,0,0 )==0 );		//one at a time
	CHECK( g->getWidth()==40 && g->getHeight()==30 && g->getDepth()==32 );
	gxCanvas *front=g->getFrontCanvas(),*back=g->getBackCanvas();
	CHECK( front!=back );
	CHECK( g->verifyCanvas( front )==front && g->verifyCanvas( back )==back );

	back->setColor( 0xff0000 );back->rect( 0,0,10,10,true );
	CHECK_EQ( front->getPixel( 1,1 ),BLACK );			//nothing shown until the flip
	g->flip( false );
	CHECK_EQ( front->getPixel( 1,1 ),RED );
	CHECK_EQ( back->getPixel( 1,1 ),RED );				//flip copies, the back buffer keeps its image
	CHECK( rt->idle() );

	rt->closeGraphics( g );
	CHECK( rt->openGraphics( -1,10,32,0,0 )==0 );
	gxGraphics *g2=rt->openGraphics( 20,20,32,0,gxGraphics::GRAPHICS_WINDOWED );
	CHECK( g2!=0 );						//can be reopened with a different size
	CHECK( g2 && g2->getWidth()==20 );
	rt->closeGraphics( g2 );
}

//The image really reaches the window (needs an OpenGL context: skipped otherwise)
static void test_present(){
	unsetenv( "SDL_VIDEODRIVER" );
	setenv( "BB_HIDDEN","1",1 );
	gxRuntime *rt=gxRuntime::openRuntime();
	gxGraphics *g=rt ? rt->openGraphics( 64,48,32,0,gxGraphics::GRAPHICS_WINDOWED ) : 0;
	if( !g ){ printf( "present: no window available, skipped\n" );if( rt ) gxRuntime::closeRuntime( rt );return; }
	gxCanvas *b=g->getBackCanvas();
	b->setColor( 0xff0000 );b->rect( 0,0,32,24,true );
	b->setColor( 0x00ff00 );b->rect( 32,0,32,24,true );
	b->setColor( 0x0000ff );b->rect( 0,24,32,24,true );
	gxSdlRequestScreenshot();
	g->flip( false );
	int w,h;std::vector<unsigned> px;
	if( !gxSdlScreenshot( &w,&h,&px ) ){
		printf( "present: no OpenGL context, skipped\n" );
	}else{
		CHECK( w>=64 && h>=48 && w*3==h*4 );				//image area keeps the 4:3 shape
		CHECK_EQ( px[(h/4)*w+w/4],RED );						//quadrants
		CHECK_EQ( px[(h/4)*w+3*w/4],GREEN );
		CHECK_EQ( px[(3*h/4)*w+w/4],BLUE );
		CHECK_EQ( px[(3*h/4)*w+3*w/4],BLACK );
	}
	rt->closeGraphics( g );
	gxRuntime::closeRuntime( rt );
}

int main(){
	setenv( "SDL_VIDEODRIVER","dummy",1 );
	gxRuntime *rt=gxRuntime::openRuntime();
	CHECK( rt!=0 );
	if( !rt ){ printf( "SDL_Init failed: %s\n",SDL_GetError() );return 1; }
	gxGraphics *g=rt->openGraphics( 64,48,32,0,gxGraphics::GRAPHICS_WINDOWED );
	CHECK( g!=0 );
	if( !g ){ printf( "openGraphics failed\n" );return 1; }
	test_primitives( g );
	test_blit_mask_collide( g );
	test_pixels_copy( g );
	test_fonts( g );
	test_images( g );
	rt->closeGraphics( g );
	test_flip_and_lifecycle( rt );
	gxRuntime::closeRuntime( rt );
	test_present();
	printf( "test_gxgraphics_sdl: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed ? 1 : 0;
}
