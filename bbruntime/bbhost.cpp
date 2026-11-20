#ifndef _WIN32

#include "std.h"
#include "bbsys.h"
#include "bbhost.h"
#include "bb_handle.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>

bool basic_create();   bool basic_destroy();   void basic_link( void (*)( const char *,void * ) );
bool math_create();    bool math_destroy();    void math_link( void (*)( const char *,void * ) );
bool string_create();  bool string_destroy();  void string_link( void (*)( const char *,void * ) );
bool stream_create();  bool stream_destroy();  void stream_link( void (*)( const char *,void * ) );
bool bank_create();    bool bank_destroy();    void bank_link( void (*)( const char *,void * ) );

//The Windows build defines these in bbsys.cpp/bbruntime.cpp. The console host has
//no gxRuntime object (gx_runtime stays null: graphics, input and the file system
//are Prompt 5), and runtime errors are reported by bbhost_run's caller.
bool debug=false;
gxRuntime *gx_runtime=0;
void gxRuntime::debugError( const char *t ){ (void)t; }
void gxRuntime::debugLog( const char *t ){ if( t ) fprintf( stderr,"%s\n",t ); }
//CallDLL: needs dlopen/dlsym and the userlib calling convention (later step)
int gxRuntime::callDll( const std::string &dll,const std::string &func,const void *in,int in_sz,void *out,int out_sz ){ return 0; }

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

//---- internal hooks the compiler calls ----
//__bbLoadLibs(LIBS chunk): userlibs need dlopen (later step).
static void bbLoadLibs( void *libs ){
	if( libs && *(const char*)libs ){
		fprintf( stderr,"warning: userlibs are not supported yet\n" );
	}
}
//debug builds: the debugger is a later step, the calls are accepted and ignored
static void bbDebugEnter( void *frame,int env_id,const char *func ){}
static void bbDebugLeave(){}
static void bbDebugStmt( int pos,const char *file ){}

void bbhost_link( void (*rtSym)( const char *sym,void *pc ) ){
	basic_link( rtSym );
	math_link( rtSym );
	string_link( rtSym );
	stream_link( rtSym );
	bank_link( rtSym );
	rtSym( "Print$string=\"\"",RTSYM_CAST( bbConsolePrint ) );
	rtSym( "Write$string",RTSYM_CAST( bbConsoleWrite ) );
	rtSym( "$Input$prompt=\"\"",RTSYM_CAST( bbConsoleInput ) );
	rtSym( "_bbLoadLibs",RTSYM_CAST( bbLoadLibs ) );
	rtSym( "_bbDebugEnter",RTSYM_CAST( bbDebugEnter ) );
	rtSym( "_bbDebugLeave",RTSYM_CAST( bbDebugLeave ) );
	rtSym( "_bbDebugStmt",RTSYM_CAST( bbDebugStmt ) );
}

bool bbhost_create( bool dbg,const char **err ){
	debug=dbg;
	const char *e=0;
	if( !basic_create() ) e="basic_create failed";
	else if( !math_create() ) e="math_create failed";
	else if( !string_create() ) e="string_create failed";
	else if( !stream_create() ) e="stream_create failed";
	else if( !bank_create() ) e="bank_create failed";
	if( e ){
		if( err ) *err=e;
		return false;
	}
	return true;
}

void bbhost_destroy(){
	bank_destroy();
	stream_destroy();
	string_destroy();
	math_destroy();
	basic_destroy();
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
		result=bb_err_msg ? bb_err_msg : "Unknown runtime error";
	}
	bb_err_jmp=prev;

	for( int k=0;k<nFaultSigs;++k ) sigaction( faultSigs[k],&old[k],0 );
	sigaltstack( &oss,0 );
	fflush( stdout );
	return result;
}

#endif
