//gxRuntime on SDL2 (macOS port, Prompt 5). Replaces gxruntime_b3d.cpp, which is
//the Win32 window/DirectX runtime. Step 1: events, time, timers, input,
//file system, display enumeration. Graphics (window + GL canvas) comes next;
//until then openGraphics fails.

#include "../gxruntime.h"
#include "gxruntime_sdl.h"
#include <SDL.h>
#include <set>
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

//---- window/graphics hooks (the SDL window arrives with the graphics step) ----
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

void gxRuntime::flip( bool vwait ){}
void gxRuntime::moveMouse( int x,int y ){}

gxGraphics *gxRuntime::openGraphics( int w,int h,int d,int driver,int flags ){
	return 0;
}
void gxRuntime::closeGraphics( gxGraphics *g ){}
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
			if( !SDL_PollEvent( &e ) ) return s_runFlag;
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
			if( input ) input->wm_mousemove( e.motion.x,e.motion.y );
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
