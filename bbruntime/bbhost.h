//Console host for the macOS runtime (Prompt 4 step 3).
//
//Runs compiled code in-process: creates the portable runtime modules, installs
//the error jump buffer (RTEX -> longjmp, see bbsys.h) and signal handlers
//(SIGSEGV/SIGBUS/SIGFPE/SIGILL become Blitz runtime errors through the same
//buffer), calls the entry point and tears the runtime down. Provides a console
//Print/Write/Input until the graphics runtime exists (Prompt 5), and no-op
//debug hooks until there is a debugger.

#ifndef BBHOST_H
#define BBHOST_H

//Register every symbol the host provides: the portable runtime modules
//(basic, math, string, stream, bank) plus the console commands and the
//internal hooks (__bbLoadLibs, __bbDebug*). Same rtSym contract as *_link.
void bbhost_link( void (*rtSym)( const char *sym,void *pc ) );

//Create the runtime modules. False (with a message in *err) on failure.
bool bbhost_create( bool debug,const char **err );

//Run entry() under the error handlers. Returns 0 when the program ended
//normally, else the runtime error message (valid until the next call).
const char *bbhost_run( void (*entry)() );

void bbhost_destroy();

#endif
