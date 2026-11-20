//blitzrun: the launcher copied in front of a compiled program by `blitzcc -o`.
//It reads the program appended to its own executable (linker/x64exe.cpp), links
//it against the statically linked runtime and runs it under the console host.

#include "../linker/x64module.h"
#include "../linker/x64exe.h"
#include "../compiler/rtlinkname.h"
#include "../bbruntime/bbhost.h"
#include <cstdio>

static std::map<std::string,void*> g_syms;
static void collect( const char *sym,void *pc ){ g_syms[rtlinkName( sym )]=pc; }

int main( int argc,char *argv[] ){
	std::string err;
	X64Module module;
	unsigned flags=0;
	if( !x64ReadExe( x64SelfPath(),module,flags,err ) ){
		fprintf( stderr,"%s\n",err.c_str() );
		return 2;
	}

	bool console=(flags & X64EXE_CONSOLE)!=0;
	bbhost_link( collect,console );
	{
		//userlib dylibs: $blitzpath/userlibs, then <directory of this executable>/userlibs
		std::string self=x64SelfPath();
		size_t slash=self.rfind( '/' );
		static std::string dir;
		dir=(slash==std::string::npos ? std::string( "." ) : self.substr( 0,slash ))+"/userlibs";
		bbhost_set_userlib_dir( dir.c_str() );
	}

	X64Loader loader;
	if( !loader.load( module,g_syms ) ){
		fprintf( stderr,"Unable to link the program against the runtime\n" );
		return 2;
	}
	void (*entry)()=(void(*)())loader.symbol( module,"__MAIN" );
	if( !entry ){
		fprintf( stderr,"The program has no entry point\n" );
		return 2;
	}

	const char *e=0;
	if( !bbhost_create( (flags&X64EXE_DEBUG)!=0,console,&e ) ){
		fprintf( stderr,"Startup Error: %s\n",e );
		return 2;
	}
	const char *rtErr=bbhost_run( entry );
	bbhost_destroy();
	if( rtErr ){
		fprintf( stderr,"Runtime Error: %s\n",rtErr );
		return 1;
	}
	return 0;
}
