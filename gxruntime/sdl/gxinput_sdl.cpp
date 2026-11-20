//gxInput on SDL2 (macOS port, Prompt 5). Replaces the DirectInput version in
//gxinput.cpp. Key numbers stay DirectInput scan codes (what Blitz programs see);
//gxRuntime::idle() translates SDL events and feeds wm_keydown/wm_mousedown/...

#include "../gxinput.h"
#include "../gxruntime.h"
#include <SDL.h>
#include <vector>
#include <cmath>

//SDL scancode -> DirectInput scan code (DIK_*), 0 = no equivalent
static int s_sdlToDik[SDL_NUM_SCANCODES];
static int s_dikToSdl[256];

static void buildTables(){
	static bool done=false;
	if( done ) return;
	done=true;
	struct E{ int sdl,dik; };
	static const E map[]={
		{SDL_SCANCODE_ESCAPE,1},
		{SDL_SCANCODE_1,2},{SDL_SCANCODE_2,3},{SDL_SCANCODE_3,4},{SDL_SCANCODE_4,5},{SDL_SCANCODE_5,6},
		{SDL_SCANCODE_6,7},{SDL_SCANCODE_7,8},{SDL_SCANCODE_8,9},{SDL_SCANCODE_9,10},{SDL_SCANCODE_0,11},
		{SDL_SCANCODE_MINUS,12},{SDL_SCANCODE_EQUALS,13},{SDL_SCANCODE_BACKSPACE,14},{SDL_SCANCODE_TAB,15},
		{SDL_SCANCODE_Q,16},{SDL_SCANCODE_W,17},{SDL_SCANCODE_E,18},{SDL_SCANCODE_R,19},{SDL_SCANCODE_T,20},
		{SDL_SCANCODE_Y,21},{SDL_SCANCODE_U,22},{SDL_SCANCODE_I,23},{SDL_SCANCODE_O,24},{SDL_SCANCODE_P,25},
		{SDL_SCANCODE_LEFTBRACKET,26},{SDL_SCANCODE_RIGHTBRACKET,27},{SDL_SCANCODE_RETURN,28},
		{SDL_SCANCODE_LCTRL,29},
		{SDL_SCANCODE_A,30},{SDL_SCANCODE_S,31},{SDL_SCANCODE_D,32},{SDL_SCANCODE_F,33},{SDL_SCANCODE_G,34},
		{SDL_SCANCODE_H,35},{SDL_SCANCODE_J,36},{SDL_SCANCODE_K,37},{SDL_SCANCODE_L,38},
		{SDL_SCANCODE_SEMICOLON,39},{SDL_SCANCODE_APOSTROPHE,40},{SDL_SCANCODE_GRAVE,41},
		{SDL_SCANCODE_LSHIFT,42},{SDL_SCANCODE_BACKSLASH,43},
		{SDL_SCANCODE_Z,44},{SDL_SCANCODE_X,45},{SDL_SCANCODE_C,46},{SDL_SCANCODE_V,47},{SDL_SCANCODE_B,48},
		{SDL_SCANCODE_N,49},{SDL_SCANCODE_M,50},
		{SDL_SCANCODE_COMMA,51},{SDL_SCANCODE_PERIOD,52},{SDL_SCANCODE_SLASH,53},{SDL_SCANCODE_RSHIFT,54},
		{SDL_SCANCODE_KP_MULTIPLY,55},{SDL_SCANCODE_LALT,56},{SDL_SCANCODE_SPACE,57},{SDL_SCANCODE_CAPSLOCK,58},
		{SDL_SCANCODE_F1,59},{SDL_SCANCODE_F2,60},{SDL_SCANCODE_F3,61},{SDL_SCANCODE_F4,62},{SDL_SCANCODE_F5,63},
		{SDL_SCANCODE_F6,64},{SDL_SCANCODE_F7,65},{SDL_SCANCODE_F8,66},{SDL_SCANCODE_F9,67},{SDL_SCANCODE_F10,68},
		{SDL_SCANCODE_NUMLOCKCLEAR,69},{SDL_SCANCODE_SCROLLLOCK,70},
		{SDL_SCANCODE_KP_7,71},{SDL_SCANCODE_KP_8,72},{SDL_SCANCODE_KP_9,73},{SDL_SCANCODE_KP_MINUS,74},
		{SDL_SCANCODE_KP_4,75},{SDL_SCANCODE_KP_5,76},{SDL_SCANCODE_KP_6,77},{SDL_SCANCODE_KP_PLUS,78},
		{SDL_SCANCODE_KP_1,79},{SDL_SCANCODE_KP_2,80},{SDL_SCANCODE_KP_3,81},{SDL_SCANCODE_KP_0,82},
		{SDL_SCANCODE_KP_PERIOD,83},{SDL_SCANCODE_NONUSBACKSLASH,86},{SDL_SCANCODE_F11,87},{SDL_SCANCODE_F12,88},
		{SDL_SCANCODE_KP_ENTER,156},{SDL_SCANCODE_RCTRL,157},{SDL_SCANCODE_KP_DIVIDE,181},
		{SDL_SCANCODE_PRINTSCREEN,183},{SDL_SCANCODE_RALT,184},{SDL_SCANCODE_PAUSE,197},
		{SDL_SCANCODE_HOME,199},{SDL_SCANCODE_UP,200},{SDL_SCANCODE_PAGEUP,201},{SDL_SCANCODE_LEFT,203},
		{SDL_SCANCODE_RIGHT,205},{SDL_SCANCODE_END,207},{SDL_SCANCODE_DOWN,208},{SDL_SCANCODE_PAGEDOWN,209},
		{SDL_SCANCODE_INSERT,210},{SDL_SCANCODE_DELETE,211},
		{SDL_SCANCODE_LGUI,219},{SDL_SCANCODE_RGUI,220},{SDL_SCANCODE_APPLICATION,221},
	};
	for( size_t k=0;k<sizeof(map)/sizeof(map[0]);++k ){
		s_sdlToDik[map[k].sdl]=map[k].dik;
		s_dikToSdl[map[k].dik]=map[k].sdl;
	}
}

int gxSdlScancodeToBlitz( int sdlScancode ){
	buildTables();
	if( sdlScancode<0 || sdlScancode>=SDL_NUM_SCANCODES ) return 0;
	return s_sdlToDik[sdlScancode];
}

class Keyboard : public gxDevice{};
class Mouse : public gxDevice{};

class Joystick : public gxDevice{
public:
	SDL_Joystick *joy;
	int type;
	Joystick( SDL_Joystick *j,int t ):joy(j),type(t){}
	~Joystick(){ if( joy ) SDL_JoystickClose( joy ); }
	void update(){
		SDL_JoystickUpdate();
		int n=SDL_JoystickNumAxes( joy );
		//axes in SDL order fill x,y,z,slider0,slider1,rx,ry,rz (-1..1)
		for( int k=0;k<8;++k ){
			axis_states[k]=k<n ? SDL_JoystickGetAxis( joy,k )/32767.0f : 0;
			if( axis_states[k]<-1 ) axis_states[k]=-1;
		}
		//hat -> degrees (0 = up, clockwise), -1 when centred, as the DirectInput POV
		float pov=-1;
		if( SDL_JoystickNumHats( joy )>0 ){
			Uint8 h=SDL_JoystickGetHat( joy,0 );
			switch( h ){
			case SDL_HAT_UP:pov=0;break;
			case SDL_HAT_RIGHTUP:pov=45;break;
			case SDL_HAT_RIGHT:pov=90;break;
			case SDL_HAT_RIGHTDOWN:pov=135;break;
			case SDL_HAT_DOWN:pov=180;break;
			case SDL_HAT_LEFTDOWN:pov=225;break;
			case SDL_HAT_LEFT:pov=270;break;
			case SDL_HAT_LEFTUP:pov=315;break;
			}
		}
		axis_states[8]=pov;
		int nb=SDL_JoystickNumButtons( joy );
		for( int k=0;k<31;++k ) setDownState( k+1,k<nb && SDL_JoystickGetButton( joy,k ) );
	}
};

static Keyboard *keyboard;
static Mouse *mouse;
static std::vector<Joystick*> joysticks;

gxInput::gxInput( gxRuntime *rt ):runtime(rt){
	buildTables();
	keyboard=new Keyboard;
	mouse=new Mouse;
	joysticks.clear();
	if( SDL_WasInit( SDL_INIT_JOYSTICK ) ){
		for( int k=0;k<SDL_NumJoysticks();++k ){
			if( SDL_Joystick *j=SDL_JoystickOpen( k ) ){
				joysticks.push_back( new Joystick( j,SDL_IsGameController( k ) ? 1 : 2 ) );
			}
		}
	}
}

gxInput::~gxInput(){
	for( size_t k=0;k<joysticks.size();++k ) delete joysticks[k];
	joysticks.clear();
	delete mouse;mouse=0;
	delete keyboard;keyboard=0;
}

void gxInput::wm_keydown( int key ){ if( keyboard ) keyboard->downEvent( key ); }
void gxInput::wm_keyup( int key ){ if( keyboard ) keyboard->upEvent( key ); }
void gxInput::wm_mousedown( int key ){ if( mouse ) mouse->downEvent( key ); }
void gxInput::wm_mouseup( int key ){ if( mouse ) mouse->upEvent( key ); }

void gxInput::wm_mousemove( int x,int y ){
	if( mouse ){
		mouse->axis_states[0]=(float)x;
		mouse->axis_states[1]=(float)y;
	}
}

void gxInput::wm_mousewheel( int dz ){
	if( mouse ) mouse->axis_states[2]+=dz;
}

void gxInput::reset(){
	if( mouse ) mouse->reset();
	if( keyboard ) keyboard->reset();
	for( size_t k=0;k<joysticks.size();++k ) joysticks[k]->reset();
}

//SDL delivers events whenever the window has focus: nothing to acquire
bool gxInput::acquire(){ return true; }
void gxInput::unacquire(){}

void gxInput::moveMouse( int x,int y ){
	if( !mouse ) return;
	mouse->axis_states[0]=(float)x;
	mouse->axis_states[1]=(float)y;
	runtime->moveMouse( x,y );
}

gxDevice *gxInput::getMouse()const{ return mouse; }
gxDevice *gxInput::getKeyboard()const{ return keyboard; }
gxDevice *gxInput::getJoystick( int n )const{
	return n>=0 && n<(int)joysticks.size() ? joysticks[n] : 0;
}
int gxInput::getJoystickType( int n )const{
	return n>=0 && n<(int)joysticks.size() ? joysticks[n]->type : 0;
}
int gxInput::numJoysticks()const{ return (int)joysticks.size(); }

//Blitz scan code -> ASCII for GetKey, using the active layout's unshifted key
//and a US shift table for symbols (the Windows version asks ToAscii).
int gxInput::toAscii( int scan )const{
	switch( scan ){
	case 210:return ASC_INSERT;
	case 211:return ASC_DELETE;
	case 199:return ASC_HOME;
	case 207:return ASC_END;
	case 201:return ASC_PAGEUP;
	case 209:return ASC_PAGEDOWN;
	case 200:return ASC_UP;
	case 208:return ASC_DOWN;
	case 203:return ASC_LEFT;
	case 205:return ASC_RIGHT;
	}
	buildTables();
	if( scan<0 || scan>255 || !s_dikToSdl[scan] ) return 0;
	SDL_Keycode kc=SDL_GetKeyFromScancode( (SDL_Scancode)s_dikToSdl[scan] );
	int ch=0;
	switch( kc ){
	case SDLK_KP_0:case SDLK_KP_1:case SDLK_KP_2:case SDLK_KP_3:case SDLK_KP_4:
	case SDLK_KP_5:case SDLK_KP_6:case SDLK_KP_7:case SDLK_KP_8:case SDLK_KP_9:
		return kc==SDLK_KP_0 ? '0' : '1'+(kc-SDLK_KP_1);
	case SDLK_KP_DIVIDE:return '/';
	case SDLK_KP_MULTIPLY:return '*';
	case SDLK_KP_MINUS:return '-';
	case SDLK_KP_PLUS:return '+';
	case SDLK_KP_PERIOD:return '.';
	case SDLK_KP_ENTER:return 13;
	}
	if( kc<=0 || kc>=127 ) return 0;
	ch=(int)kc;
	bool shift=keyboard && (keyboard->keyDown( 42 ) || keyboard->keyDown( 54 ));
	if( shift ){
		if( ch>='a' && ch<='z' ) return ch-32;
		static const char from[]="`1234567890-=[]\;',./";
		static const char to[]=  "~!@#$%^&*()_+{}|:\"<>?";
		for( int k=0;from[k];++k ) if( from[k]==ch ) return to[k];
	}
	return ch;
}
