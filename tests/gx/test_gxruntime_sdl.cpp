//SDL gxRuntime step 1 (Prompt 5): events and input (injected SDL events), time,
//timers, file system, display enumeration (SDL dummy video driver), misc
//system properties. No window is needed.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <SDL.h>
#include "../../gxruntime/gxruntime.h"
#include "../../gxruntime/sdl/gxruntime_sdl.h"

static int tests_run=0,tests_failed=0;
#define CHECK(c) do{ ++tests_run; if( !(c) ){ ++tests_failed; printf( "FAIL %s:%d: %s\n",__FILE__,__LINE__,#c ); } }while(0)

static void key( bool down,SDL_Scancode sc,bool repeat=false ){
	SDL_Event e;SDL_zero( e );
	e.type=down ? SDL_KEYDOWN : SDL_KEYUP;
	e.key.keysym.scancode=sc;
	e.key.repeat=repeat;
	SDL_PushEvent( &e );
}
static void button( bool down,int b ){
	SDL_Event e;SDL_zero( e );
	e.type=down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	e.button.button=b;
	SDL_PushEvent( &e );
}

static void test_scancodes(){
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_ESCAPE )==1 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_A )==30 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_1 )==2 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_RETURN )==28 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_SPACE )==57 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_UP )==200 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_F12 )==88 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_RCTRL )==157 );
	CHECK( gxSdlScancodeToBlitz( SDL_SCANCODE_VOLUMEUP )==0 );
}

static void test_input( gxRuntime *rt ){
	gxInput *in=rt->openInput( 0 );
	CHECK( in!=0 );
	CHECK( rt->openInput( 0 )==0 );		//only one at a time
	gxDevice *kb=in->getKeyboard(),*mouse=in->getMouse();
	CHECK( kb && mouse );

	key( true,SDL_SCANCODE_A );
	CHECK( !kb->keyDown( 30 ) );			//not delivered until idle() pumps
	CHECK( rt->idle() );
	CHECK( kb->keyDown( 30 ) );
	CHECK( kb->keyHit( 30 )==1 );
	CHECK( kb->keyHit( 30 )==0 );			//hit counts are consumed
	CHECK( kb->getKey()==30 );
	key( true,SDL_SCANCODE_A,true );		//auto-repeat is ignored
	rt->idle();
	CHECK( kb->keyHit( 30 )==0 );
	key( false,SDL_SCANCODE_A );
	rt->idle();
	CHECK( !kb->keyDown( 30 ) );

	//ASCII for GetKey: unshifted, shifted letter, shifted symbol, specials
	CHECK( in->toAscii( 30 )=='a' || in->toAscii( 30 )=='A' );	//(layout dependent only for non-US)
	key( true,SDL_SCANCODE_LSHIFT );rt->idle();
	CHECK( in->toAscii( 30 )=='A' );
	CHECK( in->toAscii( 2 )=='!' );
	key( false,SDL_SCANCODE_LSHIFT );rt->idle();
	CHECK( in->toAscii( 2 )=='1' );
	CHECK( in->toAscii( 28 )==13 );
	CHECK( in->toAscii( 57 )==' ' );
	CHECK( in->toAscii( 200 )==gxInput::ASC_UP );
	CHECK( in->toAscii( 211 )==gxInput::ASC_DELETE );
	CHECK( in->toAscii( 0 )==0 );

	//mouse: position, buttons (SDL right=3 -> Blitz 2, SDL middle=2 -> Blitz 3), wheel
	SDL_Event m;SDL_zero( m );
	m.type=SDL_MOUSEMOTION;m.motion.x=123;m.motion.y=45;
	SDL_PushEvent( &m );
	button( true,SDL_BUTTON_LEFT );button( true,SDL_BUTTON_RIGHT );button( true,SDL_BUTTON_MIDDLE );
	SDL_Event w;SDL_zero( w );
	w.type=SDL_MOUSEWHEEL;w.wheel.y=2;w.wheel.preciseY=2.0f;	//SDL derives y from preciseY
	SDL_PushEvent( &w );
	rt->idle();
	CHECK( mouse->getAxisState( 0 )==123 );
	CHECK( mouse->getAxisState( 1 )==45 );
	CHECK( mouse->getAxisState( 2 )==240 );		//120 per notch, MouseZ divides by 120
	CHECK( mouse->keyDown( 1 ) && mouse->keyDown( 2 ) && mouse->keyDown( 3 ) );
	button( false,SDL_BUTTON_RIGHT );rt->idle();
	CHECK( mouse->keyDown( 1 ) && !mouse->keyDown( 2 ) && mouse->keyDown( 3 ) );
	in->reset();
	CHECK( !mouse->keyDown( 1 ) );
	CHECK( in->numJoysticks()>=0 );
	CHECK( in->getJoystick( 99 )==0 && in->getJoystickType( 99 )==0 );

	rt->closeInput( in );
	CHECK( rt->openInput( 0 )!=0 );			//can be reopened
	rt->closeInput( rt->input );
}

static void test_time( gxRuntime *rt ){
	int t0=rt->getMilliSecs();
	CHECK( rt->delay( 60 ) );
	int dt=rt->getMilliSecs()-t0;
	CHECK( dt>=58 && dt<400 );
	CHECK( rt->delay( 0 ) );
	CHECK( rt->delay( -5 ) );
}

static void test_timers( gxRuntime *rt ){
	gxTimer *t=rt->createTimer( 50 );		//20 ms
	CHECK( t!=0 );
	int t0=rt->getMilliSecs();
	int n=t->wait();
	CHECK( n>=1 );
	CHECK( rt->getMilliSecs()-t0<200 );
	SDL_Delay( 130 );
	n=t->wait();							//several ticks piled up meanwhile
	CHECK( n>=3 );
	rt->freeTimer( t );
	rt->freeTimer( t );						//freeing twice is harmless
}

static void test_filesystem( gxRuntime *rt ){
	gxFileSystem *fs=rt->openFileSystem( 0 );
	CHECK( fs!=0 );
	CHECK( rt->openFileSystem( 0 )==0 );
	char tmpl[]="/tmp/gxfsXXXXXX";
	std::string base=mkdtemp( tmpl );
	std::string dir=base+"/sub";
	CHECK( fs->getFileType( dir )==gxFileSystem::FILE_TYPE_NONE );
	CHECK( fs->createDir( dir ) );
	CHECK( !fs->createDir( dir ) );
	CHECK( fs->getFileType( dir )==gxFileSystem::FILE_TYPE_DIR );
	CHECK( fs->getFileSize( dir )==0 );
	CHECK( !fs->createFile( dir+"/x" ) );	//as on Windows, always false

	FILE *f=fopen( (dir+"/a.txt").c_str(),"wb" );
	fwrite( "hello world",1,11,f );fclose( f );
	CHECK( fs->getFileType( dir+"/a.txt" )==gxFileSystem::FILE_TYPE_FILE );
	CHECK( fs->getFileSize( dir+"/a.txt" )==11 );
	CHECK( fs->copyFile( dir+"/a.txt",dir+"/b.txt" ) );
	CHECK( fs->getFileSize( dir+"/b.txt" )==11 );
	CHECK( fs->copyFile( dir+"/a.txt",dir+"/b.txt" ) );		//overwrite is allowed
	CHECK( !fs->copyFile( dir+"/missing",dir+"/c.txt" ) );
	CHECK( fs->renameFile( dir+"/b.txt",dir+"/c.txt" ) );
	CHECK( fs->getFileType( dir+"/b.txt" )==gxFileSystem::FILE_TYPE_NONE );

	gxDir *d=fs->openDir( dir,0 );
	CHECK( d!=0 );
	std::vector<std::string> names;
	for(;;){
		std::string n=d->getNextFile();
		if( !n.size() ) break;
		names.push_back( n );
	}
	CHECK( names.size()==4 );		//. .. a.txt c.txt, sorted
	CHECK( names.size()==4 && names[0]=="." && names[1]==".." && names[2]=="a.txt" && names[3]=="c.txt" );
	CHECK( d->getNextFile()=="" );
	CHECK( fs->verifyDir( d )==d );
	fs->closeDir( d );
	CHECK( fs->verifyDir( d )==0 );
	CHECK( fs->openDir( base+"/nope",0 )==0 );

	CHECK( !fs->deleteDir( dir ) );			//not empty
	CHECK( fs->deleteFile( dir+"/a.txt" ) );
	CHECK( fs->deleteFile( dir+"/c.txt" ) );
	CHECK( !fs->deleteFile( dir+"/c.txt" ) );
	CHECK( fs->deleteDir( dir ) );

	std::string old=fs->getCurrentDir();
	CHECK( old.size() && old[old.size()-1]=='/' );
	CHECK( fs->setCurrentDir( base ) );
	char real[4096];
	std::string want=realpath( base.c_str(),real ) ? std::string( real )+"/" : base+"/";
	CHECK( fs->getCurrentDir()==want );
	CHECK( !fs->setCurrentDir( base+"/nope" ) );
	CHECK( fs->setCurrentDir( old ) );
	rmdir( base.c_str() );
	rt->closeFileSystem( fs );
	CHECK( rt->openFileSystem( 0 )!=0 );
	rt->closeFileSystem( rt->fileSystem );
}

static void test_display( gxRuntime *rt ){
	int n=rt->numGraphicsDrivers();
	CHECK( n==1 );
	std::string name;int caps=0;
	rt->graphicsDriverInfo( 0,&name,&caps );
	CHECK( name=="Default" );
	CHECK( caps&gxRuntime::GFXMODECAPS_3D );
	int modes=rt->numGraphicsModes( 0 );
	CHECK( modes>0 );
	for( int k=0;k<modes;++k ){
		int w,h,d,c;
		rt->graphicsModeInfo( 0,k,&w,&h,&d,&c );
		CHECK( w>0 && h>0 && d==32 );
	}
	rt->windowedModeInfo( &caps );
	CHECK( caps&gxRuntime::GFXMODECAPS_3D );
	CHECK( rt->openGraphics( 0,480,32,0,0 )==0 );		//a zero-sized screen is refused
}

static void test_misc( gxRuntime *rt ){
	CHECK( rt->systemProperty( "CPU" )=="Intel" );
	CHECK( rt->systemProperty( "OS" ).substr( 0,5 )=="macOS" );
	std::string app=rt->systemProperty( "AppDir" );
	CHECK( app.size() && app[app.size()-1]=='/' );
	std::string tmp=rt->systemProperty( "tempdir" );
	CHECK( tmp.size() && tmp[tmp.size()-1]=='/' );
	CHECK( rt->systemProperty( "nonsense" )=="" );
	gxSdlSetCommandLine( "a b" );
	CHECK( rt->commandLine()=="a b" );
	rt->setTitle( "t","c" );
	CHECK( rt->callDll( "/nonexistent.dylib","f",0,0,0,0 )==0 );
	CHECK( !rt->execute( "" ) );
}

static void test_quit(){
	gxRuntime *rt=gxRuntime::openRuntime();
	CHECK( rt->idle() );
	rt->asyncEnd();
	CHECK( !rt->idle() );					//asyncEnd ends the program
	gxRuntime::closeRuntime( rt );
	rt=gxRuntime::openRuntime();			//a new runtime starts running again
	CHECK( rt->idle() );
	SDL_Event q;SDL_zero( q );q.type=SDL_QUIT;SDL_PushEvent( &q );
	CHECK( !rt->idle() );					//closing the window ends it too
	gxRuntime::closeRuntime( rt );
}

int main(){
	setenv( "SDL_VIDEODRIVER","dummy",1 );
	gxRuntime *rt=gxRuntime::openRuntime();
	CHECK( rt!=0 );
	if( !rt ){ printf( "test_gxruntime_sdl: SDL_Init failed: %s\n",SDL_GetError() );return 1; }
	test_scancodes();
	test_input( rt );
	test_time( rt );
	test_timers( rt );
	test_filesystem( rt );
	test_display( rt );
	test_misc( rt );
	gxRuntime::closeRuntime( rt );
	test_quit();
	printf( "test_gxruntime_sdl: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed ? 1 : 0;
}
