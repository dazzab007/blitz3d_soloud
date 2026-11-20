
#ifndef BBSYS_H
#define BBSYS_H

#include "basic.h"
#include "../gxruntime/gxruntime.h"

extern bool debug;
extern gxRuntime *gx_runtime;

struct bbEx{
	const char *err;
	bbEx( const char *e ):err(e){
		if( e ) gx_runtime->debugError( e );
	}
};

#ifdef _WIN32
#define RTEX( _X_ ) throw bbEx( _X_ );
#else
//x86-64 macOS: generated code has no unwind info, so a C++ throw cannot pass
//through it. When the loader has installed a jump buffer (bb_err_jmp) a
//runtime error longjmps back to the loader's entry point (nothing is
//unwound; the program is ending). With no buffer installed (unit tests of
//runtime modules) it still throws bbEx, as on Windows.
#include <csetjmp>
extern jmp_buf *bb_err_jmp;		//set by the loader around the call into compiled code
extern const char *bb_err_msg;	//the message of the error that longjmp'd
void bbRaise( const char *e );
#define RTEX( _X_ ) bbRaise( _X_ );
#endif

#endif