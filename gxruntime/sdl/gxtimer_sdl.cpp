//gxTimer on SDL (macOS port, Prompt 5): a periodic SDL timer counts ticks,
//wait() blocks until at least one has elapsed and returns how many.

#include "../gxtimer.h"
#include "../gxruntime.h"
#include <SDL.h>

static Uint32 SDLCALL timerCallback( Uint32 interval,void *param ){
	((gxTimer*)param)->tick();
	return interval;
}

gxTimer::gxTimer( gxRuntime *rt,int hertz ):
runtime(rt),ticks_put(0),ticks_get(0){
	if( hertz<1 ) hertz=1;
	int ms=1000/hertz;
	if( ms<1 ) ms=1;
	timerID=SDL_AddTimer( ms,timerCallback,this );
}

gxTimer::~gxTimer(){
	if( timerID ) SDL_RemoveTimer( timerID );
}

int gxTimer::wait(){
	for(;;){
		int put=__sync_fetch_and_add( &ticks_put,0 );
		if( put!=ticks_get ){
			int n=put-ticks_get;
			ticks_get+=n;
			return n;
		}
		SDL_Delay( 1 );
	}
}
