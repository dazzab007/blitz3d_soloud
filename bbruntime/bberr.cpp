//Runtime error delivery on x86-64 macOS (see RTEX in bbsys.h).
//Kept out of basic.cpp so unit tests that link only part of the runtime do
//not drag in the real string pool.
#ifndef _WIN32

#include "std.h"
#include "bbsys.h"

jmp_buf *bb_err_jmp=0;
const char *bb_err_msg=0;

void bbRaise( const char *e ){
	if( bb_err_jmp ){
		bb_err_msg=e;
		if( e && gx_runtime ) gx_runtime->debugError( e );
		longjmp( *bb_err_jmp,1 );
	}
	throw bbEx( e );
}

#endif
