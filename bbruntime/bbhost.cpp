#ifndef _WIN32

#include "std.h"
#include "bbsys.h"
#include "bbhost.h"
#include "bb_handle.h"
#include <csignal>
#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

bool basic_create();   bool basic_destroy();   void basic_link( void (*)( const char *,void * ) );
bool math_create();    bool math_destroy();    void math_link( void (*)( const char *,void * ) );
bool string_create();  bool string_destroy();  void string_link( void (*)( const char *,void * ) );
bool stream_create();  bool stream_destroy();  void stream_link( void (*)( const char *,void * ) );
bool filesystem_create();  bool filesystem_destroy();  void filesystem_link( void (*)( const char *,void * ) );
bool input_create();   bool input_destroy();   void input_link( void (*)( const char *,void * ) );
bool graphics_create();  bool graphics_destroy();  void graphics_link( void (*)( const char *,void * ) );
bool blitz3d_create();   bool blitz3d_destroy();   void blitz3d_link( void (*)( const char *,void * ) );
void bbcmds_link( void (*)( const char *,void * ) );
bool bank_create();    bool bank_destroy();    void bank_link( void (*)( const char *,void * ) );

//The Windows build defines these in bbsys.cpp/bbruntime.cpp. gx_runtime is the SDL
//gxRuntime (gxruntime/sdl), created by bbhost_create; runtime errors are
//reported by bbhost_run's caller.
bool debug=false;
gxRuntime *gx_runtime=0;

//---- console commands (stand in for the graphics Print until Prompt 5) ----
static void bbConsolePrint( BBStr *s ){
	fwrite( s->data(),1,s->size(),stdout );
	fputc( '\n',stdout );
	delete s;
}
static void bbConsoleWrite( BBStr *s ){
	fwrite( s->data(),1,s->size(),stdout );
	fflush( stdout );
	delete s;
}
static BBStr *bbConsoleInput( BBStr *prompt ){
	fwrite( prompt->data(),1,prompt->size(),stdout );
	fflush( stdout );
	delete prompt;
	string line;
	int c;
	while( (c=fgetc( stdin ))!=EOF && c!='\n' ) line+=(char)c;
	if( line.size() && line[line.size()-1]=='\r' ) line.resize( line.size()-1 );
	return d_new BBStr( line );
}

//---- userlibs (.decls + dylib) ----
//The LIBS chunk the compiler emits (prognode.cpp) is, per library, its name then
//{ proc name, 8-byte slot address }... then an empty name, ending with one more
//empty name. Each slot is the "_f<ident>" data word the compiled code calls through.
static std::vector<void*> s_libs;
static std::string s_userlibDir;

void bbhost_set_userlib_dir( const char *dir ){ s_userlibDir=dir ? dir : ""; }

static void libNotFound(){ RTEX( "User lib not found" ); }
static void procNotFound(){ RTEX( "User lib function not found" ); }

static void *openLib( const char *name ){
	void *mod=dlopen( name,RTLD_NOW|RTLD_LOCAL );
	if( mod ) return mod;
	if( const char *home=getenv( "blitzpath" ) ){
		mod=dlopen( (std::string( home )+"/userlibs/"+name).c_str(),RTLD_NOW|RTLD_LOCAL );
		if( mod ) return mod;
	}
	if( s_userlibDir.size() ){
		mod=dlopen( (s_userlibDir+"/"+name).c_str(),RTLD_NOW|RTLD_LOCAL );
	}
	return mod;
}

static void bbLoadLibs( const char *p ){
	if( !p ) return;
	while( *p ){
		void *mod=openLib( p );
		p+=strlen( p )+1;
		if( mod ) s_libs.push_back( mod );
		while( *p ){
			void *proc=mod ? dlsym( mod,p ) : 0;
			p+=strlen( p )+1;
			void *slot;
			memcpy( &slot,p,sizeof(slot) );
			p+=sizeof(slot);
			*(void**)slot=proc ? proc : (mod ? (void*)procNotFound : (void*)libNotFound);
		}
		++p;
	}
}

//BASIC string -> C string for userlib calls: a ring of 256 buffers, as on Windows
//(the C function must not keep the pointer).
static const char *bbStrToCStr( BBStr *str ){
	static struct{ char *p;int size; } strs[256];
	static int next=0;
	auto &t=strs[next++ & 255];
	int size=(int)str->size();
	if( !t.p || t.size<size ){
		delete[] t.p;
		t.p=new char[size+1];
		t.size=size;
	}
	memcpy( t.p,str->data(),size );
	t.p[size]=0;
	delete str;
	return t.p;
}
static BBStr *bbCStrToStr( const char *s ){ return d_new BBStr( s ? s : "" ); }

//---- internal hooks the compiler calls ----
//debug builds: the debugger is a later step, the calls are accepted and ignored
static void bbDebugEnter( void *frame,int env_id,const char *func ){}
static void bbDebugLeave(){}
static void bbDebugStmt( int pos,const char *file ){}

void bbhost_link( void (*rtSym)( const char *sym,void *pc ),bool console ){
	basic_link( rtSym );
	math_link( rtSym );
	string_link( rtSym );
	stream_link( rtSym );
	bank_link( rtSym );
	filesystem_link( rtSym );
	input_link( rtSym );
	bbcmds_link( rtSym );
	if( console ){
		rtSym( "Print$string=\"\"",RTSYM_CAST( bbConsolePrint ) );
		rtSym( "Write$string",RTSYM_CAST( bbConsoleWrite ) );
		rtSym( "$Input$prompt=\"\"",RTSYM_CAST( bbConsoleInput ) );
	}else{
		graphics_link( rtSym );
		blitz3d_link( rtSym );
	}
	rtSym( "_bbLoadLibs",RTSYM_CAST( bbLoadLibs ) );
	rtSym( "_bbStrToCStr",RTSYM_CAST( bbStrToCStr ) );
	rtSym( "_bbCStrToStr",RTSYM_CAST( bbCStrToStr ) );
	rtSym( "_bbDebugEnter",RTSYM_CAST( bbDebugEnter ) );
	rtSym( "_bbDebugLeave",RTSYM_CAST( bbDebugLeave ) );
	rtSym( "_bbDebugStmt",RTSYM_CAST( bbDebugStmt ) );
}

static bool s_console=true;

bool bbhost_create( bool dbg,bool console,const char **err ){
	s_console=console;
	debug=dbg;
	const char *e=0;
	gx_runtime=gxRuntime::openRuntime();
	if( !gx_runtime ) e="Unable to start SDL";
	else if( !basic_create() ) e="basic_create failed";
	else if( !math_create() ) e="math_create failed";
	else if( !string_create() ) e="string_create failed";
	else if( !stream_create() ) e="stream_create failed";
	else if( !bank_create() ) e="bank_create failed";
	else if( !filesystem_create() ) e="filesystem_create failed";
	else if( !console && !graphics_create() ) e="Unable to open the graphics window";
	else if( !input_create() ) e="input_create failed";
	else if( !console && !blitz3d_create() ) e="blitz3d_create failed";
	if( e ){
		if( err ) *err=e;
		return false;
	}
	return true;
}

void bbhost_destroy(){
	if( !s_console ) blitz3d_destroy();
	input_destroy();
	if( !s_console ) graphics_destroy();
	filesystem_destroy();
	for( size_t k=0;k<s_libs.size();++k ) dlclose( s_libs[k] );
	s_libs.clear();
	bank_destroy();
	stream_destroy();
	string_destroy();
	math_destroy();
	basic_destroy();
	gxRuntime::closeRuntime( gx_runtime );
	gx_runtime=0;
}

//---- faults become runtime errors ----
static const char *faultMessage( int sig ){
	switch( sig ){
	case SIGSEGV:case SIGBUS:return "Memory access violation";
	case SIGFPE:return "Integer divide by zero";
	case SIGILL:return "Illegal instruction";
	}
	return "Unknown fault";
}

static void faultHandler( int sig,siginfo_t *,void * ){
	if( bb_err_jmp ){
		bb_err_msg=faultMessage( sig );
		longjmp( *bb_err_jmp,1 );
	}
	//no program running: fall back to the default action and fault again
	signal( sig,SIG_DFL );
}

static const int faultSigs[]={ SIGSEGV,SIGBUS,SIGFPE,SIGILL };
static const int nFaultSigs=4;

const char *bbhost_run( void (*entry)() ){
	//alternate stack so a stack overflow (deep recursion) can still be handled
	static char *altStack=0;
	const size_t altSize=64*1024;
	if( !altStack ) altStack=(char*)malloc( altSize );
	stack_t ss,oss;
	ss.ss_sp=altStack;ss.ss_size=altSize;ss.ss_flags=0;
	sigaltstack( &ss,&oss );

	struct sigaction sa,old[nFaultSigs];
	memset( &sa,0,sizeof(sa) );
	sa.sa_sigaction=faultHandler;
	//NODEFER: we leave the handler by longjmp, so the signal must not stay blocked
	sa.sa_flags=SA_SIGINFO|SA_ONSTACK|SA_NODEFER;
	sigemptyset( &sa.sa_mask );
	for( int k=0;k<nFaultSigs;++k ) sigaction( faultSigs[k],&sa,&old[k] );

	static jmp_buf jb;
	jmp_buf *prev=bb_err_jmp;
	bb_err_jmp=&jb;
	bb_err_msg=0;
	const char *result=0;
	if( !setjmp( jb ) ){
		entry();
	}else{
		result=bb_err_msg;	//null (End, window closed) is a normal end of the program
	}
	bb_err_jmp=prev;

	for( int k=0;k<nFaultSigs;++k ) sigaction( faultSigs[k],&old[k],0 );
	sigaltstack( &oss,0 );
	fflush( stdout );
	return result;
}

#endif
