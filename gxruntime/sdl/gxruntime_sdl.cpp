//gxRuntime on SDL2 (macOS port, Prompt 5). Replaces gxruntime_b3d.cpp, which is
//the Win32 window/DirectX runtime. Step 1: events, time, timers, input,
//file system, display enumeration. Graphics (window + GL canvas) comes next;
//until then openGraphics fails.

#include "../gxruntime.h"
#include "../gxgraphics.h"
#include "gxruntime_sdl.h"
#include "gxsurface_sdl.h"
#include <SDL.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <set>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
#include <sys/sysctl.h>
#include <mach-o/dyld.h>

struct gxRuntime::GfxMode{
	int w,h,d;
};
struct gxRuntime::GfxDriver{
	std::string name;
	std::vector<GfxMode*> modes;
};

static bool s_runFlag=true;
static bool s_suspended=false;
static bool s_sdlOwned=false;		//we called SDL_Init, so we SDL_Quit
static bool s_videoOwned=false;
static std::string s_cmdLine;
static std::set<gxTimer*> s_timers;

//user events posted by asyncStop/Run/End
enum{ EV_STOP=1,EV_RUN=2,EV_END=3 };

void gxSdlSetCommandLine( const std::string &cmd ){ s_cmdLine=cmd; }

gxRuntime::gxRuntime():
	input(0),graphics(0),fileSystem(0),
	pointer_visible(true),enum_all(false),curr_driver(0),use_di(0){
}

gxRuntime::~gxRuntime(){
	while( s_timers.size() ){
		gxTimer *t=*s_timers.begin();
		s_timers.erase( s_timers.begin() );
		delete t;
	}
	if( input ){ delete input;input=0; }
	if( fileSystem ){ delete fileSystem;fileSystem=0; }
	denumGfx();
}

gxRuntime *gxRuntime::openRuntime(){
	if( !SDL_WasInit( 0 ) ){
		if( SDL_Init( SDL_INIT_EVENTS|SDL_INIT_TIMER )!=0 ) return 0;
		s_sdlOwned=true;
	}
	//joysticks are optional: a failure just means none are found
	SDL_InitSubSystem( SDL_INIT_JOYSTICK );
	s_runFlag=true;
	s_suspended=false;
	return new gxRuntime();
}

void gxRuntime::closeRuntime( gxRuntime *r ){
	if( !r ) return;
	delete r;
	SDL_QuitSubSystem( SDL_INIT_JOYSTICK );
	if( s_videoOwned ){
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		s_videoOwned=false;
	}
	if( s_sdlOwned ){
		SDL_Quit();
		s_sdlOwned=false;
	}
}

//---- window and screen ----
//One window shows the front buffer: its pixels are uploaded to a GL texture and
//drawn as a letterboxed quad. If no GL context can be made (SDL's dummy video
//driver, tests) the graphics still work in memory and presenting is skipped.
static struct Screen{
	SDL_Window *win;
	SDL_GLContext gl;
	bool headless;
	GLuint prog,vao,tex;
	int gw,gh;				//graphics size
	int vx,vy,vw,vh;		//where the image sits in the window (window points)
	unsigned lastMod;
	Uint64 lastPresent;
	int swapInterval;
	bool glReady;
} s_screen={0};

static const char *VERT=
	"#version 330 core\n"
	"out vec2 uv;\n"
	"void main(){\n"
	"	vec2 p=vec2( float( gl_VertexID&1 ),float( (gl_VertexID>>1)&1 ) );\n"
	"	uv=vec2( p.x,1.0-p.y );\n"				//image row 0 is the top
	"	gl_Position=vec4( p*2.0-1.0,0.0,1.0 );\n"
	"}\n";
static const char *FRAG=
	"#version 330 core\n"
	"in vec2 uv;\n"
	"out vec4 color;\n"
	"uniform sampler2D img;\n"
	"void main(){ color=vec4( texture( img,uv ).rgb,1.0 ); }\n";

static GLuint compile( GLenum type,const char *src ){
	GLuint s=glCreateShader( type );
	glShaderSource( s,1,&src,0 );
	glCompileShader( s );
	GLint ok=0;
	glGetShaderiv( s,GL_COMPILE_STATUS,&ok );
	if( !ok ){
		char log[1024];glGetShaderInfoLog( s,sizeof(log),0,log );
		fprintf( stderr,"gxruntime: shader error: %s\n",log );
		glDeleteShader( s );
		return 0;
	}
	return s;
}

static bool initGL(){
	GLuint vs=compile( GL_VERTEX_SHADER,VERT ),fs=compile( GL_FRAGMENT_SHADER,FRAG );
	if( !vs || !fs ) return false;
	s_screen.prog=glCreateProgram();
	glAttachShader( s_screen.prog,vs );
	glAttachShader( s_screen.prog,fs );
	glLinkProgram( s_screen.prog );
	glDeleteShader( vs );glDeleteShader( fs );
	GLint ok=0;
	glGetProgramiv( s_screen.prog,GL_LINK_STATUS,&ok );
	if( !ok ) return false;
	glGenVertexArrays( 1,&s_screen.vao );
	glGenTextures( 1,&s_screen.tex );
	glBindTexture( GL_TEXTURE_2D,s_screen.tex );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D,0,GL_RGBA8,s_screen.gw,s_screen.gh,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,0 );
	return true;
}

static void layoutImage(){
	int ww,wh;
	SDL_GetWindowSize( s_screen.win,&ww,&wh );
	float sc=std::min( (float)ww/s_screen.gw,(float)wh/s_screen.gh );
	s_screen.vw=(int)(s_screen.gw*sc+.5f);
	s_screen.vh=(int)(s_screen.gh*sc+.5f);
	s_screen.vx=(ww-s_screen.vw)/2;
	s_screen.vy=(wh-s_screen.vh)/2;
}

//window coordinates -> graphics coordinates
static void mapMouse( int *x,int *y ){
	if( !s_screen.win || !s_screen.vw || !s_screen.vh ) return;
	int mx=(int)((long long)(*x-s_screen.vx)*s_screen.gw/s_screen.vw);
	int my=(int)((long long)(*y-s_screen.vy)*s_screen.gh/s_screen.vh);
	if( mx<0 ) mx=0; else if( mx>=s_screen.gw ) mx=s_screen.gw-1;
	if( my<0 ) my=0; else if( my>=s_screen.gh ) my=s_screen.gh-1;
	*x=mx;*y=my;
}

static bool s_shotWanted=false,s_shotValid=false;
static int s_shotW=0,s_shotH=0;
static std::vector<unsigned> s_shot;

void gxSdlRequestScreenshot(){ s_shotWanted=true;s_shotValid=false; }
bool gxSdlScreenshot( int *w,int *h,std::vector<unsigned> *argb ){
	if( !s_shotValid ) return false;
	*w=s_shotW;*h=s_shotH;*argb=s_shot;
	return true;
}

static void present( gxGraphics *g,bool vsync ){
	if( s_screen.headless || !s_screen.glReady || !g ) return;
	SDL_GL_MakeCurrent( s_screen.win,s_screen.gl );
	int si=vsync ? 1 : 0;
	if( si!=s_screen.swapInterval ){
		SDL_GL_SetSwapInterval( si );
		s_screen.swapInterval=si;
	}
	gxCanvas *front=g->getFrontCanvas();
	GxNativeSurface *surf=front->getSurface();
	layoutImage();
	int dw,dh,ww,wh;
	SDL_GL_GetDrawableSize( s_screen.win,&dw,&dh );
	SDL_GetWindowSize( s_screen.win,&ww,&wh );
	float px=ww ? (float)dw/ww : 1,py=wh ? (float)dh/wh : 1;	//drawable pixels per window point

	glViewport( 0,0,dw,dh );
	glClearColor( 0,0,0,1 );
	glClear( GL_COLOR_BUFFER_BIT );
	glViewport( (int)(s_screen.vx*px),(int)((wh-s_screen.vy-s_screen.vh)*py),(int)(s_screen.vw*px),(int)(s_screen.vh*py) );
	glUseProgram( s_screen.prog );
	glBindVertexArray( s_screen.vao );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D,s_screen.tex );
	glPixelStorei( GL_UNPACK_ALIGNMENT,4 );
	glTexSubImage2D( GL_TEXTURE_2D,0,0,0,surf->width,surf->height,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,surf->pixels.data() );
	glUniform1i( glGetUniformLocation( s_screen.prog,"img" ),0 );
	glDrawArrays( GL_TRIANGLE_STRIP,0,4 );
	if( s_shotWanted ){
		//read the image area back before the swap makes the buffer undefined
		s_shotWanted=false;
		int rw=(int)(s_screen.vw*px),rh=(int)(s_screen.vh*py);
		int rx=(int)(s_screen.vx*px),ry=(int)((wh-s_screen.vy-s_screen.vh)*py);
		std::vector<unsigned> raw( (size_t)rw*rh );
		glPixelStorei( GL_PACK_ALIGNMENT,4 );
		glReadPixels( rx,ry,rw,rh,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,raw.data() );
		s_shot.assign( raw.size(),0 );
		for( int y=0;y<rh;++y ){	//GL rows run bottom to top
			std::copy( raw.begin()+(size_t)(rh-1-y)*rw,raw.begin()+(size_t)(rh-y)*rw,s_shot.begin()+(size_t)y*rw );
		}
		s_shotW=rw;s_shotH=rh;s_shotValid=true;
	}
	SDL_GL_SwapWindow( s_screen.win );
	s_screen.lastMod=(unsigned)front->getModify();
	s_screen.lastPresent=SDL_GetTicks64();
}

//---- window/graphics hooks ----
void gxRuntime::paint(){}
void gxRuntime::suspend(){ s_suspended=true; }
void gxRuntime::forceSuspend(){ s_suspended=true; }
void gxRuntime::resume(){ s_suspended=false; }
void gxRuntime::forceResume(){ s_suspended=false; }
void gxRuntime::backupWindowState(){}
void gxRuntime::restoreWindowState(){}
void gxRuntime::backupGraphics(){}
void gxRuntime::restoreGraphics(){}
void gxRuntime::resetInput(){ if( input ) input->reset(); }
void gxRuntime::pauseAudio(){}
void gxRuntime::resumeAudio(){}
void gxRuntime::acquireInput(){ if( input ) input->acquire(); }
void gxRuntime::unacquireInput(){ if( input ) input->unacquire(); }

//Show the back buffer: copy it to the front buffer and present.
void gxRuntime::flip( bool vwait ){
	if( !graphics ) return;
	gxCanvas *front=graphics->getFrontCanvas(),*back=graphics->getBackCanvas();
	GxNativeSurface *fs=front->getSurface(),*bs=back->getSurface();
	if( fs!=bs ) fs->pixels=bs->pixels;
	front->setModify( front->getModify()+1 );
	present( graphics,vwait );
}

void gxRuntime::moveMouse( int x,int y ){
	if( !s_screen.win || !s_screen.gw ) return;
	SDL_WarpMouseInWindow( s_screen.win,s_screen.vx+x*s_screen.vw/s_screen.gw,s_screen.vy+y*s_screen.vh/s_screen.gh );
}

gxGraphics *gxRuntime::openGraphics( int w,int h,int d,int driver,int flags ){
	if( graphics || w<1 || h<1 ) return 0;
	if( !SDL_WasInit( SDL_INIT_VIDEO ) ){
		if( SDL_InitSubSystem( SDL_INIT_VIDEO )!=0 ) return 0;
		s_videoOwned=true;
	}
	bool windowed=(flags & gxGraphics::GRAPHICS_WINDOWED)!=0;
	Uint32 wf=SDL_WINDOW_ALLOW_HIGHDPI;
	if( !windowed ) wf|=SDL_WINDOW_FULLSCREEN_DESKTOP;
	if( getenv( "BB_HIDDEN" ) ) wf|=SDL_WINDOW_HIDDEN;	//tests and automation
	const char *title=app_title.size() ? app_title.c_str() : "Blitz3D";

	memset( &s_screen,0,sizeof(s_screen) );
	s_screen.swapInterval=-1;
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION,3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION,3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS,SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,1 );
	s_screen.win=SDL_CreateWindow( title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,wf|SDL_WINDOW_OPENGL );
	if( s_screen.win ) s_screen.gl=SDL_GL_CreateContext( s_screen.win );
	if( !s_screen.gl ){
		//no OpenGL here: run without presenting
		if( s_screen.win ){ SDL_DestroyWindow( s_screen.win );s_screen.win=0; }
		s_screen.win=SDL_CreateWindow( title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,wf );
		if( !s_screen.win ) return 0;
		s_screen.headless=true;
	}
	s_screen.gw=w;s_screen.gh=h;
	if( !s_screen.headless ){
		SDL_GL_MakeCurrent( s_screen.win,s_screen.gl );
		s_screen.glReady=initGL();
	}
	layoutImage();

	GxNativeSurface *front=new GxNativeSurface( w,h ),*back=new GxNativeSurface( w,h );
	graphics=new gxGraphics( this,front,back,(flags & gxGraphics::GRAPHICS_3D)!=0 );
	s_screen.lastMod=(unsigned)graphics->getFrontCanvas()->getModify();
	s_screen.lastPresent=0;
	if( !pointer_visible ) SDL_ShowCursor( SDL_DISABLE );
	return graphics;
}

void gxRuntime::closeGraphics( gxGraphics *g ){
	if( graphics!=g ) return;
	delete graphics;
	graphics=0;
	if( s_screen.gl ){
		SDL_GL_MakeCurrent( s_screen.win,s_screen.gl );
		if( s_screen.tex ) glDeleteTextures( 1,&s_screen.tex );
		if( s_screen.vao ) glDeleteVertexArrays( 1,&s_screen.vao );
		if( s_screen.prog ) glDeleteProgram( s_screen.prog );
		SDL_GL_DeleteContext( s_screen.gl );
	}
	if( s_screen.win ) SDL_DestroyWindow( s_screen.win );
	memset( &s_screen,0,sizeof(s_screen) );
}

bool gxRuntime::graphicsLost(){ return false; }

//---- events ----
static void postUser( int code ){
	SDL_Event e;
	SDL_zero( e );
	e.type=SDL_USEREVENT;
	e.user.code=code;
	SDL_PushEvent( &e );
}

void gxRuntime::asyncStop(){ postUser( EV_STOP ); }
void gxRuntime::asyncRun(){ postUser( EV_RUN ); }
void gxRuntime::asyncEnd(){ postUser( EV_END ); }

//Pump events, feeding input. False once the program should end (window closed
//or asyncEnd). While suspended (debugger stop) it blocks for the next event.
bool gxRuntime::idle(){
	for(;;){
		SDL_Event e;
		if( s_suspended && s_runFlag ){
			if( !SDL_WaitEvent( &e ) ) return s_runFlag;
		}else{
			if( !SDL_PollEvent( &e ) ){
				//a program drawing straight onto the front buffer never flips: show
				//what changed, at most every 16 ms
				if( graphics && !s_screen.headless ){
					gxCanvas *f=graphics->getFrontCanvas();
					if( (unsigned)f->getModify()!=s_screen.lastMod && SDL_GetTicks64()-s_screen.lastPresent>=16 ){
						present( graphics,false );
					}
				}
				return s_runFlag;
			}
		}
		switch( e.type ){
		case SDL_QUIT:
			s_runFlag=false;
			break;
		case SDL_USEREVENT:
			if( e.user.code==EV_STOP ) forceSuspend();
			else if( e.user.code==EV_RUN ) forceResume();
			else if( e.user.code==EV_END ) s_runFlag=false;
			break;
		case SDL_KEYDOWN:
			if( input && !e.key.repeat ){
				if( int n=gxSdlScancodeToBlitz( e.key.keysym.scancode ) ) input->wm_keydown( n );
			}
			break;
		case SDL_KEYUP:
			if( input ){
				if( int n=gxSdlScancodeToBlitz( e.key.keysym.scancode ) ) input->wm_keyup( n );
			}
			break;
		case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:
			if( input ){
				//SDL: left 1, middle 2, right 3. Blitz: left 1, right 2, middle 3.
				int b=e.button.button==SDL_BUTTON_LEFT ? 1 :
					e.button.button==SDL_BUTTON_RIGHT ? 2 :
					e.button.button==SDL_BUTTON_MIDDLE ? 3 : 0;
				if( b ){
					if( e.type==SDL_MOUSEBUTTONDOWN ) input->wm_mousedown( b );
					else input->wm_mouseup( b );
				}
			}
			break;
		case SDL_MOUSEMOTION:
			if( input ){
				int mx=e.motion.x,my=e.motion.y;
				mapMouse( &mx,&my );
				input->wm_mousemove( mx,my );
			}
			break;
		case SDL_MOUSEWHEEL:
			//Windows reports 120 per notch and MouseZ divides by 120. preciseY is 1.0 per
			//notch (fractional on trackpads); the integer y can lose fractions.
			if( input ) input->wm_mousewheel( (int)lroundf( e.wheel.preciseY*120 ) );
			break;
		}
	}
}

bool gxRuntime::delay( int ms ){
	Uint64 end=SDL_GetTicks64()+(ms>0 ? ms : 0);
	for(;;){
		if( !idle() ) return false;
		Sint64 d=(Sint64)end-(Sint64)SDL_GetTicks64();	//how long is left
		if( d<=0 ) return true;
		if( d>100 ) d=100;
		SDL_Delay( (Uint32)d );
	}
}

//---- debug hooks: no debugger yet ----
void gxRuntime::debugStmt( int pos,const char *file ){}
void gxRuntime::debugStop(){ forceSuspend(); }
void gxRuntime::debugEnter( void *frame,void *env,const char *func ){}
void gxRuntime::debugLeave(){}
void gxRuntime::debugError( const char *t ){}
void gxRuntime::debugInfo( const char *t ){}
void gxRuntime::debugLog( const char *t ){
	if( t ) fprintf( stderr,"%s\n",t );
}

std::string gxRuntime::commandLine(){ return s_cmdLine; }

//Launch a document or program with `open`, as ShellExecute does.
bool gxRuntime::execute( const std::string &cmd_line ){
	if( !cmd_line.size() ) return false;
	std::string cmd=cmd_line,params;
	while( cmd.size() && cmd[0]==' ' ) cmd=cmd.substr( 1 );
	if( cmd.find( '\"' )==0 ){
		size_t n=cmd.find( '\"',1 );
		if( n!=std::string::npos ){
			params=cmd.substr( n+1 );
			cmd=cmd.substr( 1,n-1 );
		}
	}else{
		size_t n=cmd.find( ' ' );
		if( n!=std::string::npos ){
			params=cmd.substr( n+1 );
			cmd=cmd.substr( 0,n );
		}
	}
	while( params.size() && params[0]==' ' ) params=params.substr( 1 );
	while( params.size() && params[params.size()-1]==' ' ) params=params.substr( 0,params.size()-1 );

	std::string q="'";
	for( size_t k=0;k<cmd.size();++k ){
		if( cmd[k]=='\'' ) q+="'\\''"; else q+=cmd[k];
	}
	q+="'";
	std::string line="open "+q;
	if( params.size() ) line+=" --args "+params;
	return system( line.c_str() )==0;
}

void gxRuntime::setTitle( const std::string &t,const std::string &e ){
	app_title=t;
	app_close=e;
	if( s_screen.win ) SDL_SetWindowTitle( s_screen.win,t.c_str() );
}

int gxRuntime::getMilliSecs(){
	return (int)SDL_GetTicks64();
}

void gxRuntime::setPointerVisible( bool vis ){
	if( pointer_visible==vis ) return;
	pointer_visible=vis;
	if( SDL_WasInit( SDL_INIT_VIDEO ) ) SDL_ShowCursor( vis ? SDL_ENABLE : SDL_DISABLE );
}

//---- display enumeration ----
void gxRuntime::enumGfx(){
	denumGfx();
	GfxDriver *d=new GfxDriver;
	d->name="Default";
	if( !SDL_WasInit( SDL_INIT_VIDEO ) && SDL_InitSubSystem( SDL_INIT_VIDEO )==0 ) s_videoOwned=true;
	if( SDL_WasInit( SDL_INIT_VIDEO ) ){
		int n=SDL_GetNumDisplayModes( 0 );
		for( int k=0;k<n;++k ){
			SDL_DisplayMode m;
			if( SDL_GetDisplayMode( 0,k,&m )!=0 ) continue;
			bool dup=false;
			for( size_t j=0;j<d->modes.size();++j ){
				if( d->modes[j]->w==m.w && d->modes[j]->h==m.h ) dup=true;
			}
			if( dup ) continue;
			GfxMode *g=new GfxMode;
			g->w=m.w;g->h=m.h;g->d=32;
			d->modes.push_back( g );
		}
	}
	drivers.push_back( d );
}

void gxRuntime::denumGfx(){
	for( size_t k=0;k<drivers.size();++k ){
		GfxDriver *d=drivers[k];
		for( size_t j=0;j<d->modes.size();++j ) delete d->modes[j];
		delete d;
	}
	drivers.clear();
}

int gxRuntime::numGraphicsDrivers(){
	if( !enum_all ){
		enum_all=true;
		enumGfx();
	}
	return (int)drivers.size();
}

void gxRuntime::graphicsDriverInfo( int driver,std::string *name,int *c ){
	*name=drivers[driver]->name;
	*c=GFXMODECAPS_3D;
}

int gxRuntime::numGraphicsModes( int driver ){
	return (int)drivers[driver]->modes.size();
}

void gxRuntime::graphicsModeInfo( int driver,int mode,int *w,int *h,int *d,int *c ){
	GfxMode *m=drivers[driver]->modes[mode];
	*w=m->w;*h=m->h;*d=m->d;
	*c=GFXMODECAPS_3D;
}

void gxRuntime::windowedModeInfo( int *c ){
	*c=GFXMODECAPS_3D;
}

//---- input, file system, timers ----
gxInput *gxRuntime::openInput( int flags ){
	if( input ) return 0;
	input=new gxInput( this );
	return input;
}

void gxRuntime::closeInput( gxInput *i ){
	if( input!=i ) return;
	delete input;
	input=0;
}

gxFileSystem *gxRuntime::openFileSystem( int flags ){
	if( fileSystem ) return 0;
	fileSystem=new gxFileSystem();
	return fileSystem;
}

void gxRuntime::closeFileSystem( gxFileSystem *f ){
	if( fileSystem!=f ) return;
	delete fileSystem;
	fileSystem=0;
}

gxTimer *gxRuntime::createTimer( int hertz ){
	gxTimer *t=new gxTimer( this,hertz );
	s_timers.insert( t );
	return t;
}

void gxRuntime::freeTimer( gxTimer *t ){
	if( !s_timers.count( t ) ) return;
	s_timers.erase( t );
	delete t;
}

void gxRuntime::enableDirectInput( bool enable ){
	use_di=enable;
}

//---- misc ----
static std::string toDir( std::string t ){
	if( t.size() && t[t.size()-1]!='/' ) t+='/';
	return t;
}

static std::string selfDir(){
	char buf[4096];
	uint32_t n=sizeof(buf);
	if( _NSGetExecutablePath( buf,&n )!=0 ) return "";
	std::string t=buf;
	size_t slash=t.find_last_of( '/' );
	return slash==std::string::npos ? "" : t.substr( 0,slash );
}

std::string gxRuntime::systemProperty( const std::string &p ){
	std::string t=p;
	for( size_t k=0;k<t.size();++k ) t[k]=(char)tolower( (unsigned char)t[k] );
	if( t=="cpu" ){
#if defined(__x86_64__)
		return "Intel";
#else
		return "Apple";
#endif
	}else if( t=="os" ){
		char ver[64];
		size_t sz=sizeof(ver);
		if( sysctlbyname( "kern.osproductversion",ver,&sz,0,0 )==0 ) return std::string( "macOS " )+ver;
		return "macOS";
	}else if( t=="appdir" ){
		return toDir( selfDir() );
	}else if( t=="tempdir" ){
		const char *tmp=getenv( "TMPDIR" );
		return toDir( tmp && *tmp ? tmp : "/tmp" );
	}
	return "";
}

//CallDLL: int func( const void *in,int in_sz,void *out,int out_sz )
typedef int (*LibFunc)( const void *,int,void *,int );
static std::map<std::string,void*> s_dlls;

int gxRuntime::callDll( const std::string &dll,const std::string &func,const void *in,int in_sz,void *out,int out_sz ){
	std::map<std::string,void*>::iterator it=s_dlls.find( dll );
	if( it==s_dlls.end() ){
		void *h=dlopen( dll.c_str(),RTLD_NOW|RTLD_LOCAL );
		if( !h ) return 0;
		it=s_dlls.insert( std::make_pair( dll,h ) ).first;
	}
	LibFunc f=(LibFunc)dlsym( it->second,func.c_str() );
	if( !f ) return 0;
	return f( in,in_sz,out,out_sz );
}
