#include "debugenv.h"

#ifndef _WIN32

static vector<Environ*> g_debug_envs;

int debugEnvId( Environ *e ){
	for( int k=0;k<g_debug_envs.size();++k )
		if( g_debug_envs[k]==e ) return k+1;
	g_debug_envs.push_back( e );
	return g_debug_envs.size();
}

Environ *debugEnvLookup( int id ){
	if( id<=0 || id>g_debug_envs.size() ) return 0;
	return g_debug_envs[id-1];
}

#endif // !_WIN32
