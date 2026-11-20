//Core BASIC commands for the macOS host (Prompt 4/5): the portable part of
//bbruntime.cpp, which on Windows also carries the runtime start-up and the
//commands of modules that are not ported yet (sockets, ...). They only use
//gx_runtime, so they work with any gxRuntime backend.
#ifndef _WIN32

#include "std.h"
#include "bbsys.h"
#include "bb_handle.h"
#include "bbhost.h"

// Timer functions (defined in bbtimer.cpp).
BB_HANDLE(gxTimer) bbCreateTimer( int hertz );
int bbWaitTimer( BB_HANDLE(gxTimer) t_h );
void bbFreeTimer( BB_HANDLE(gxTimer) t_h );

//End / program end: a null message is a normal exit (see bbhost_run)
static void bbEnd(){
	RTEX( 0 );
}

static void bbStop(){
	gx_runtime->debugStop();
	if( !gx_runtime->idle() ) RTEX( 0 );
}

static void bbAppTitle( BBStr *ti,BBStr *cp ){
	gx_runtime->setTitle( *ti,*cp );
	delete ti;
	delete cp;
}

static void bbRuntimeError( BBStr *str ){
	string t=*str;
	delete str;
	if( t.size()>255 ) t.resize( 255 );
	static char err[256];
	strcpy( err,t.c_str() );
	RTEX( err );
}

static int bbExecFile( BBStr *f ){
	string t=*f;
	delete f;
	int n=gx_runtime->execute( t );
	if( !gx_runtime->idle() ) RTEX( 0 );
	return n;
}

static void bbDelay( int ms ){
	if( !gx_runtime->delay( ms ) ) RTEX( 0 );
}

static int bbMilliSecs(){
	return gx_runtime->getMilliSecs();
}

static BBStr *bbCommandLine(){
	return d_new BBStr( gx_runtime->commandLine() );
}

static BBStr *bbSystemProperty( BBStr *p ){
	string t=gx_runtime->systemProperty( *p );
	delete p;
	return d_new BBStr( t );
}

static BBStr *bbGetEnv( BBStr *env_var ){
	char *p=getenv( env_var->c_str() );
	BBStr *val=d_new BBStr( p ? p : "" );
	delete env_var;
	return val;
}

static void bbSetEnv( BBStr *env_var,BBStr *val ){
	setenv( env_var->c_str(),val->c_str(),1 );
	delete env_var;
	delete val;
}

static void bbDebugLog( BBStr *t ){
	gx_runtime->debugLog( t->c_str() );
	delete t;
}

void bbcmds_link( void (*rtSym)( const char *sym,void *pc ) ){
	rtSym( "End",RTSYM_CAST( bbEnd ) );
	rtSym( "Stop",RTSYM_CAST( bbStop ) );
	rtSym( "AppTitle$title$close_prompt=\"\"",RTSYM_CAST( bbAppTitle ) );
	rtSym( "RuntimeError$message",RTSYM_CAST( bbRuntimeError ) );
	rtSym( "ExecFile$command",RTSYM_CAST( bbExecFile ) );
	rtSym( "Delay%millisecs",RTSYM_CAST( bbDelay ) );
	rtSym( "%MilliSecs",RTSYM_CAST( bbMilliSecs ) );
	rtSym( "$CommandLine",RTSYM_CAST( bbCommandLine ) );
	rtSym( "$SystemProperty$property",RTSYM_CAST( bbSystemProperty ) );
	rtSym( "$GetEnv$env_var",RTSYM_CAST( bbGetEnv ) );
	rtSym( "SetEnv$env_var$value",RTSYM_CAST( bbSetEnv ) );
	rtSym( "%CreateTimer%hertz",RTSYM_CAST( bbCreateTimer ) );
	rtSym( "%WaitTimer%timer",RTSYM_CAST( bbWaitTimer ) );
	rtSym( "FreeTimer%timer",RTSYM_CAST( bbFreeTimer ) );
	rtSym( "DebugLog$text",RTSYM_CAST( bbDebugLog ) );
}

#endif
