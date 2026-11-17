#include "debugenv.h"

#ifndef _WIN32

static Environ *g_debug_envs[1024];
static int g_debug_env_count = 0;

int debugEnvId( Environ *e ){
	for( int k=0;k<g_debug_env_count;++k )
		if( g_debug_envs[k]==e ) return k+1;
	if( g_debug_env_count>=1024 ) return 0; // table full: no id (debug only)
	g_debug_envs[g_debug_env_count++]=e;
	return g_debug_env_count;
}

Environ *debugEnvLookup( int id ){
	if( id<=0 || id>g_debug_env_count ) return 0;
	return g_debug_envs[id-1];
}

#endif // !_WIN32
