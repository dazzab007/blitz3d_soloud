
#ifndef GXRUNTIME_B3D_H
#define GXRUNTIME_B3D_H

#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <vector>

#ifdef _WIN32
#if BB_FMOD_ENABLED
#include "gxaudio.h"
#endif
#include "gxinput.h"
#include "gxgraphics.h"
#include "gxfilesystem.h"
#include "gxtimer.h"

#include "../debugger/debugger.h"
#else
// Forward declarations for non-Windows builds (no Windows headers needed).
class gxAudio;
class gxInput;
class gxGraphics;
class gxFileSystem;
class gxTimer;
class Debugger;
#endif

class gxRuntime{
	/***** INTERNAL INTERFACE *****/
public:

#ifdef _WIN32
	HWND hwnd;
	HINSTANCE hinst;
#endif

#if BB_FMOD_ENABLED
	gxAudio *audio;
#endif
	gxInput *input;
	gxGraphics *graphics;
	gxFileSystem *fileSystem;

	void flip( bool vwait );
	void moveMouse( int x,int y );

#ifdef _WIN32
	LRESULT windowProc( HWND hwnd,UINT msg,WPARAM w,LPARAM l );
#endif

	struct GfxMode;
	struct GfxDriver;

private:
#ifdef _WIN32
	gxRuntime( HINSTANCE hinst,const std::string &cmd_line,HWND hwnd );
#else
	gxRuntime();
#endif
	~gxRuntime();

	void paint();
	void suspend();
	void forceSuspend();
	void resume();
	void forceResume();
	void backupWindowState();
	void restoreWindowState();

#ifdef _WIN32
	RECT t_rect;
	int t_style;
#endif
	std::string cmd_line;
	bool pointer_visible;
	std::string app_title;
	std::string app_close;

#ifdef _WIN32
	bool setDisplayMode( int w,int h,int d,bool d3d,IDirectDraw7 *dd );
	gxGraphics *openWindowedGraphics( int w,int h,int d,bool d3d );
	gxGraphics *openExclusiveGraphics( int w,int h,int d,bool d3d );
#endif

	bool enum_all;
	std::vector<GfxDriver*> drivers;
	GfxDriver *curr_driver;
	int use_di;

	void enumGfx();
	void denumGfx();

	void resetInput();
	void pauseAudio();
	void resumeAudio();
	void backupGraphics();
	void restoreGraphics();
	void acquireInput();
	void unacquireInput();

	/***** APP INTERFACE *****/
public:
#ifdef _WIN32
	static gxRuntime *openRuntime( HINSTANCE hinst,const std::string &cmd_line,Debugger *debugger );
#else
	static gxRuntime *openRuntime();
#endif
	static void closeRuntime( gxRuntime *runtime );

	void asyncStop();
	void asyncRun();
	void asyncEnd();

	/***** GX INTERFACE *****/
public:
	enum{
		GFXMODECAPS_3D=1
	};

	//return true if program should continue, or false for quit.
	bool idle();
	bool delay( int ms );

	bool execute( const std::string &cmd );
	void setTitle( const std::string &title,const std::string &close );
	int  getMilliSecs();
	void setPointerVisible( bool vis );

	std::string commandLine();

	std::string systemProperty( const std::string &t );

	void debugStop();
	void debugProfile( int per );
	void debugStmt( int pos,const char *file );
	void debugEnter( void *frame,void *env,const char *func );
	void debugLeave();
	void debugInfo( const char *t );
	void debugError( const char *t );
	void debugLog( const char *t );

	int numGraphicsDrivers();
	void graphicsDriverInfo( int driver,std::string *name,int *caps );

	int numGraphicsModes( int driver );
	void graphicsModeInfo( int driver,int mode,int *w,int *h,int *d,int *caps );

	void windowedModeInfo( int *caps );

#if BB_FMOD_ENABLED
	gxAudio *openAudio( int flags );
	void closeAudio( gxAudio *audio );
#endif

	gxInput *openInput( int flags );
	void closeInput( gxInput *input );

	gxGraphics *openGraphics( int w,int h,int d,int driver,int flags );
	void closeGraphics( gxGraphics *graphics );
	bool graphicsLost();

	gxFileSystem *openFileSystem( int flags );
	void closeFileSystem( gxFileSystem *filesys );

	gxTimer *createTimer( int hertz );
	void freeTimer( gxTimer *timer );

	void enableDirectInput( bool use );
	int  directInputEnabled(){ return use_di; }

	int callDll( const std::string &dll,const std::string &func,const void *in,int in_sz,void *out,int out_sz );

#ifdef _WIN32
	OSVERSIONINFO osinfo;
#endif
};

#endif
