
#ifndef GXINPUT_H
#define GXINPUT_H

#ifdef _WIN32
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#endif

#include "gxdevice.h"

class gxRuntime;

class gxInput{
public:
	gxRuntime *runtime;
#ifdef _WIN32
	IDirectInput8 *dirInput;
#endif

#ifdef _WIN32
	gxInput( gxRuntime *runtime,IDirectInput8 *di );
#else
	gxInput( gxRuntime *runtime );
#endif
	~gxInput();

	void reset();
	bool acquire();
	void unacquire();

	void wm_keydown( int key );
	void wm_keyup( int key );
	void wm_mousedown( int key );
	void wm_mouseup( int key );
	void wm_mousemove( int x,int y );
	void wm_mousewheel( int dz );

private:

	/***** GX INTERFACE *****/
public:
	enum{
		ASC_HOME=1,ASC_END=2,ASC_INSERT=3,ASC_DELETE=4,
		ASC_PAGEUP=5,ASC_PAGEDOWN=6,
		ASC_UP=28,ASC_DOWN=29,ASC_RIGHT=30,ASC_LEFT=31
	};

	void moveMouse( int x,int y );

	gxDevice *getMouse()const;
	gxDevice *getKeyboard()const;
	gxDevice *getJoystick( int port )const;
	int getJoystickType( int port )const;
	int numJoysticks()const;
	int toAscii( int key )const;
};

#endif