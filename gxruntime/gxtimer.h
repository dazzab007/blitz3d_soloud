
#ifndef GXTIMER_H
#define GXTIMER_H

#ifdef _WIN32
#include <windows.h>
#endif

class gxRuntime;

class gxTimer{
public:
	gxTimer( gxRuntime *rt,int hertz );
	~gxTimer();

#ifdef _WIN32
	static void CALLBACK timerCallback( UINT id,UINT msg,DWORD user,DWORD dw1,DWORD dw2 );
#endif

private:
	gxRuntime *runtime;
#ifdef _WIN32
	HANDLE event;
	MMRESULT timerID;
#endif
	int ticks_put,ticks_get;

	/***** GX INTERFACE *****/
public:
	int wait();
};

#endif
