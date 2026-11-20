//gxGraphics on SDL (macOS port, Prompt 5): canvases are software ARGB32 surfaces,
//fonts are rasterised from system TrueType files with stb_truetype into the same
//white-on-black atlas canvases the Windows version builds with GDI, images load
//with stb_image. The 3D parts (scene, mesh) arrive with the 3D step.

#include "../gxgraphics.h"
#include "../gxruntime.h"
#include "gxsurface_sdl.h"
#include "gxruntime_sdl.h"
#include "gxgl_sdl.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_BUT_FILE_OK
#include "../../third_party/stb/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

gxGraphics::gxGraphics( gxRuntime *rt,ddSurf *fs,ddSurf *bs,bool d3d ):
runtime(rt),front_canvas(0),back_canvas(0),def_font(0),gfx_lost(false),dummy_mesh(0){

	front_canvas=new gxCanvas( this,fs,0 );
	back_canvas=new gxCanvas( this,bs,0 );

	front_canvas->cls();
	back_canvas->cls();

	def_font=loadFont( "courier",12,0 );

	front_canvas->setFont( def_font );
	back_canvas->setFont( def_font );
}

gxGraphics::~gxGraphics(){
	while( scene_set.size() ) freeScene( *scene_set.begin() );
	while( movie_set.size() ) closeMovie( *movie_set.begin() );
	while( font_set.size() ) freeFont( *font_set.begin() );
	while( canvas_set.size() ) freeCanvas( *canvas_set.begin() );
	delete back_canvas;
	delete front_canvas;
}

//no gamma ramp: the window is composited by the OS
void gxGraphics::setGamma( int r,int g,int b,float dr,float dg,float db ){}
void gxGraphics::updateGamma( bool calibrate ){}
void gxGraphics::getGamma( int r,int g,int b,float *dr,float *dg,float *db ){
	*dr=(float)r;*dg=(float)g;*db=(float)b;
}

void gxGraphics::backup(){}
bool gxGraphics::restore(){ return true; }

gxCanvas *gxGraphics::getFrontCanvas()const{ return front_canvas; }
gxCanvas *gxGraphics::getBackCanvas()const{ return back_canvas; }
gxFont *gxGraphics::getDefaultFont()const{ return def_font; }

void gxGraphics::vwait(){}

void gxGraphics::flip( bool v ){
	runtime->flip( v );
}

//Copy (and stretch, nearest neighbour) a rectangle between canvases.
void gxGraphics::copy( gxCanvas *dest,int dx,int dy,int dw,int dh,gxCanvas *src,int sx,int sy,int sw,int sh ){
	GxNativeSurface *d=dest->getSurface(),*s=src->getSurface();
	if( dw<=0 || dh<=0 || sw<=0 || sh<=0 ) return;
	std::vector<uint32_t> tmp;	//handles overlapping source and destination
	const uint32_t *sp=s->pixels.data();
	int swidth=s->width;
	if( d==s ){
		tmp=s->pixels;
		sp=tmp.data();
	}
	for( int y=0;y<dh;++y ){
		int ty=dy+y;
		if( ty<0 || ty>=d->height ) continue;
		int syy=sy+(int)((long long)y*sh/dh);
		if( syy<0 || syy>=s->height ) continue;
		for( int x=0;x<dw;++x ){
			int tx=dx+x;
			if( tx<0 || tx>=d->width ) continue;
			int sxx=sx+(int)((long long)x*sw/dw);
			if( sxx<0 || sxx>=s->width ) continue;
			d->pixels[(size_t)ty*d->width+tx]=sp[(size_t)syy*swidth+sxx];
		}
	}
	RECT r={ dx,dy,dx+dw,dy+dh };
	dest->damage( r );
}

int gxGraphics::getScanLine()const{ return 0; }
int gxGraphics::getTotalVidmem()const{ return 256*1024*1024; }
int gxGraphics::getAvailVidmem()const{ return 256*1024*1024; }

//movies (DirectShow on Windows) are not supported
gxMovie *gxGraphics::openMovie( const std::string &file,int flags ){ return 0; }
gxMovie *gxGraphics::verifyMovie( gxMovie *m ){ return movie_set.count( m ) ? m : 0; }
void gxGraphics::closeMovie( gxMovie *m ){ if( movie_set.erase( m ) ) delete m; }

//Textures get power-of-two sizes, as the D3D version always makes them
static int pow2( int n ){
	int p=1;
	while( p<n ) p<<=1;
	return p;
}

ddSurf *gxGraphics::createSurface( int w,int h,int flags ){
	if( w<=0 || h<=0 || (long long)w*h>(1LL<<28) ) return 0;
	if( flags & gxCanvas::CANVAS_TEXTURE ){
		w=pow2( w );h=pow2( h );
		if( w>8192 ) w=8192;
		if( h>8192 ) h=8192;
	}
	return new GxNativeSurface( w,h );
}

//Load an image into an ARGB surface. Plain 2D images get alpha 255 (as the
//Windows loader does); textures keep the file's alpha.
ddSurf *gxGraphics::loadSurface( const std::string &f,int flags ){
	int w,h,n;
	unsigned char *data=stbi_load( f.c_str(),&w,&h,&n,4 );
	if( !data ) return 0;
	GxNativeSurface *s=new GxNativeSurface( w,h );
	bool hasAlpha=(n==4);	//the file carries an alpha channel
	bool texture=(flags & gxCanvas::CANVAS_TEXTURE)!=0;
	for( size_t k=0;k<(size_t)w*h;++k ){
		unsigned r=data[k*4],g=data[k*4+1],b=data[k*4+2],a=0xff;
		if( texture && (flags & gxCanvas::CANVAS_TEX_ALPHA) ){
			if( flags & gxCanvas::CANVAS_TEX_MASK ){
				a=(r|g|b) ? 0xff : 0;							//black is the mask colour: transparent
			}else if( hasAlpha ){
				a=data[k*4+3];									//the file's own alpha
			}else{
				a=(r+g+b)/3;									//alpha from intensity ...
				if( !(flags & gxCanvas::CANVAS_TEX_RGB) ) r=g=b=255;	//... with white colour for alpha-only textures
			}
		}
		s->pixels[k]=(a<<24)|(r<<16)|(g<<8)|b;
	}
	stbi_image_free( data );
	if( texture ){
		//stretch to the power-of-two texture size (bilinear)
		int tw=pow2( w ),th=pow2( h );
		if( tw>8192 ) tw=8192;
		if( th>8192 ) th=8192;
		if( tw!=w || th!=h ){
			GxNativeSurface *t=new GxNativeSurface( tw,th );
			for( int y=0;y<th;++y ){
				float fy=(y+.5f)*h/th-.5f;
				int y0=(int)floorf( fy );float wy=fy-y0;
				int y1=y0+1;if( y0<0 ) y0=0;if( y1>=h ) y1=h-1;if( y0>=h ) y0=h-1;
				for( int x=0;x<tw;++x ){
					float fx=(x+.5f)*w/tw-.5f;
					int x0=(int)floorf( fx );float wx=fx-x0;
					int x1=x0+1;if( x0<0 ) x0=0;if( x1>=w ) x1=w-1;if( x0>=w ) x0=w-1;
					unsigned out=0;
					for( int sh=0;sh<32;sh+=8 ){
						float c00=(s->pixels[(size_t)y0*w+x0]>>sh)&0xff,c10=(s->pixels[(size_t)y0*w+x1]>>sh)&0xff;
						float c01=(s->pixels[(size_t)y1*w+x0]>>sh)&0xff,c11=(s->pixels[(size_t)y1*w+x1]>>sh)&0xff;
						float c=(c00*(1-wx)+c10*wx)*(1-wy)+(c01*(1-wx)+c11*wx)*wy;
						out|=((unsigned)(c+.5f)&0xff)<<sh;
					}
					t->pixels[(size_t)y*tw+x]=out;
				}
			}
			delete s;
			s=t;
		}
	}
	return s;
}

gxCanvas *gxGraphics::createCanvas( int w,int h,int flags ){
	ddSurf *s=createSurface( w,h,flags );
	if( !s ) return 0;
	gxCanvas *c=new gxCanvas( this,s,flags );
	canvas_set.insert( c );
	c->cls();
	return c;
}

gxCanvas *gxGraphics::loadCanvas( const std::string &f,int flags ){
	ddSurf *s=loadSurface( f,flags );
	if( !s ) return 0;
	gxCanvas *c=new gxCanvas( this,s,flags );
	canvas_set.insert( c );
	return c;
}

gxCanvas *gxGraphics::verifyCanvas( gxCanvas *c ){
	return canvas_set.count( c ) || c==front_canvas || c==back_canvas ? c : 0;
}

void gxGraphics::freeCanvas( gxCanvas *c ){
	if( canvas_set.erase( c ) ) delete c;
}

int gxGraphics::getWidth()const{ return front_canvas->getWidth(); }
int gxGraphics::getHeight()const{ return front_canvas->getHeight(); }
int gxGraphics::getDepth()const{ return front_canvas->getDepth(); }

/////////////
// Fonts   //
/////////////

static bool readFile( const std::string &path,std::vector<unsigned char> &out ){
	FILE *f=fopen( path.c_str(),"rb" );
	if( !f ) return false;
	unsigned char buf[65536];
	size_t n;
	out.clear();
	while( (n=fread( buf,1,sizeof(buf),f ))>0 ) out.insert( out.end(),buf,buf+n );
	fclose( f );
	return out.size()>0;
}

static bool fileExists( const std::string &p ){
	FILE *f=fopen( p.c_str(),"rb" );
	if( !f ) return false;
	fclose( f );
	return true;
}

//Find a TrueType file for a Windows-style font name. Falls back to a monospaced
//system font so a program never ends up without text.
static std::string findFontFile( const std::string &name,int flags ){
	std::string lname=name;
	for( size_t k=0;k<lname.size();++k ) lname[k]=(char)tolower( (unsigned char)lname[k] );

	std::vector<std::string> families;
	if( lname=="courier" || lname=="courier new" ){
		families.push_back( "Courier New" );
		families.push_back( "Courier" );
	}else if( lname=="arial" || lname=="helvetica" || lname=="sans" || lname=="sans serif" ){
		families.push_back( "Arial" );
		families.push_back( "Helvetica" );
	}else if( lname=="times" || lname=="times new roman" ){
		families.push_back( "Times New Roman" );
	}else if( lname=="verdana" ){
		families.push_back( "Verdana" );
	}else if( lname=="tahoma" ){
		families.push_back( "Tahoma" );
	}else if( lname=="georgia" ){
		families.push_back( "Georgia" );
	}else if( lname=="lucida console" || lname=="terminal" || lname=="fixedsys" || lname=="system" ){
		families.push_back( "Monaco" );
		families.push_back( "Menlo" );
	}else{
		families.push_back( name );
	}

	const char *dirs[]={
		"/System/Library/Fonts/Supplemental/","/System/Library/Fonts/","/Library/Fonts/",0 };
	std::string home;
	if( const char *h=getenv( "HOME" ) ) home=std::string( h )+"/Library/Fonts/";

	bool bold=(flags & gxFont::FONT_BOLD)!=0,italic=(flags & gxFont::FONT_ITALIC)!=0;
	const char *exts[]={ ".ttf",".otf",".ttc",0 };
	for( size_t f=0;f<families.size();++f ){
		std::vector<std::string> variants;
		if( bold && italic ) variants.push_back( families[f]+" Bold Italic" );
		if( bold ) variants.push_back( families[f]+" Bold" );
		if( italic ) variants.push_back( families[f]+" Italic" );
		variants.push_back( families[f] );
		for( size_t v=0;v<variants.size();++v ){
			for( int d=0;dirs[d] || (d==3 && home.size());++d ){
				std::string dir=dirs[d] ? dirs[d] : home;
				for( int e=0;exts[e];++e ){
					std::string p=dir+variants[v]+exts[e];
					if( fileExists( p ) ) return p;
				}
				if( !dirs[d] ) break;
			}
		}
	}
	const char *fallback[]={
		"/System/Library/Fonts/Monaco.ttf","/System/Library/Fonts/Menlo.ttc","/System/Library/Fonts/SFNSMono.ttf",
		"/System/Library/Fonts/Supplemental/Courier New.ttf",0 };
	for( int k=0;fallback[k];++k ) if( fileExists( fallback[k] ) ) return fallback[k];
	return "";
}

//Coverage a rasterised pixel needs to count as set. Windows draws these fonts without
//antialiasing but hinted, so thin strokes still get a full pixel; stb_truetype
//is unhinted, and at small sizes a half-coverage cut-off drops thin strokes.
static const int GLYPH_THRESHOLD=40;

gxFont *gxGraphics::loadFont( const std::string &f,int height,int flags ){

	std::string path;
	if( f.find( '.' )!=std::string::npos ) path=f;	//a font file, as on Windows
	else path=findFontFile( f,flags );
	if( path.empty() ) return 0;

	std::vector<unsigned char> data;
	if( !readFile( path,data ) ) return 0;

	stbtt_fontinfo info;
	int off=stbtt_GetFontOffsetForIndex( &data[0],0 );
	if( off<0 || !stbtt_InitFont( &info,&data[0],off ) ) return 0;

	if( height<4 ) height=4;
	float scale=stbtt_ScaleForPixelHeight( &info,(float)height );
	int asc,desc,gap;
	stbtt_GetFontVMetrics( &info,&asc,&desc,&gap );
	int ascent=(int)ceil( asc*scale );
	int cell=ascent+(int)ceil( -desc*scale );	//cell height: ascent + descent
	if( cell<1 ) cell=1;

	const int first=32,last=255;
	int sz=last-first+1;
	int *offs=new int[sz];
	int *widths=new int[sz];

	//advance widths and atlas layout (rows no wider than the screen, as on Windows)
	int maxw=0,x=0,y=0,max_x=0,screenw=getWidth()>64 ? getWidth() : 64;
	for( int k=0;k<sz;++k ){
		int adv,lsb;
		stbtt_GetCodepointHMetrics( &info,first+k,&adv,&lsb );
		int w=(int)floor( adv*scale+.5f );
		if( w<1 ) w=1;
		widths[k]=w;
		if( w>maxw ) maxw=w;
		if( x && x+w>screenw ){ x=0;y+=cell; }
		offs[k]=(x<<16)|y;
		x+=w;if( x>max_x ) max_x=x;
	}
	int cw=max_x,ch=y+cell;

	gxCanvas *c=createCanvas( cw,ch,0 );
	if( !c ){
		delete[] offs;delete[] widths;
		return 0;
	}

	//white glyphs on black, not antialiased (the Windows font smoothing is switched off for this)
	c->lock();
	std::vector<unsigned char> bmp;
	for( int k=0;k<sz;++k ){
		int gx=(offs[k]>>16)&0xffff,gy=offs[k]&0xffff;
		int x0,y0,x1,y1;
		stbtt_GetCodepointBitmapBox( &info,first+k,scale,scale,&x0,&y0,&x1,&y1 );
		int bw=x1-x0,bh=y1-y0;
		if( bw>0 && bh>0 ){
			bmp.assign( (size_t)bw*bh,0 );
			stbtt_MakeCodepointBitmap( &info,&bmp[0],bw,bh,bw,scale,scale,first+k );
			for( int j=0;j<bh;++j ){
				for( int i=0;i<bw;++i ){
					if( bmp[(size_t)j*bw+i]<GLYPH_THRESHOLD ) continue;
					int px=gx+x0+i,py=gy+ascent+y0+j;
					if( px<gx || px>=gx+widths[k] || py<gy || py>=gy+cell ) continue;	//stay inside the cell
					c->setPixelFast( px,py,0xffffffff );
				}
			}
		}
		if( flags & gxFont::FONT_UNDERLINE ){
			int py=gy+ascent+1;
			if( py<gy+cell ) for( int i=0;i<widths[k];++i ) c->setPixelFast( gx+i,py,0xffffffff );
		}
	}
	c->unlock();
	c->damage( c->getSurface() ? RECT{ 0,0,cw,ch } : RECT{ 0,0,0,0 } );

	gxFont *font=new gxFont( this,c,maxw,cell,first,last+1,'?',offs,widths );
	font_set.insert( font );
	return font;
}

gxFont *gxGraphics::verifyFont( gxFont *f ){
	return font_set.count( f ) ? f : 0;
}

void gxGraphics::freeFont( gxFont *f ){
	if( font_set.erase( f ) ) delete f;
}

/////////////////////////////////
// 3D: scenes and meshes (later) //
/////////////////////////////////
//One scene at a time, rendering into the back buffer. Needs an OpenGL context:
//without one (headless) 3D is unavailable and this returns 0.
gxScene *gxGraphics::createScene( int flags ){
	if( scene_set.size() ) return 0;
	if( !gxSdlMakeGLCurrent() ) return 0;
	gxScene *scene=new gxScene( this,back_canvas );
	scene_set.insert( scene );
	dummy_mesh=createMesh( 8,12,0 );
	return scene;
}

gxScene *gxGraphics::verifyScene( gxScene *s ){
	return scene_set.count( s ) ? s : 0;
}

void gxGraphics::freeScene( gxScene *scene ){
	if( !scene_set.erase( scene ) ) return;
	dummy_mesh=0;
	while( mesh_set.size() ) freeMesh( *mesh_set.begin() );
	delete scene;
}

gxMesh *gxGraphics::createMesh( int max_verts,int max_tris,int flags ){
	if( max_verts<1 || max_tris<0 ) return 0;
	unsigned short *indices=new unsigned short[max_tris*3+1];
	gxMesh *mesh=new gxMesh( this,0,indices,max_verts,max_tris );
	mesh_set.insert( mesh );
	return mesh;
}

gxMesh *gxGraphics::verifyMesh( gxMesh *m ){
	return mesh_set.count( m ) ? m : 0;
}

void gxGraphics::freeMesh( gxMesh *mesh ){
	if( mesh_set.erase( mesh ) ) delete mesh;
}
