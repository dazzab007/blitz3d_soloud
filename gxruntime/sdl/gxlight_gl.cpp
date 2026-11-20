//gxLight for the GL pipeline (macOS port): the same values gxlight.cpp stores in a
//D3DLIGHT7, held in a plain struct that gxScene turns into shader uniforms.
#include "../gxlight.h"
#include "../gxscene.h"
#include <cstring>

gxLight::gxLight( gxScene *s,int type ):scene( s ){
	memset( &gl_light,0,sizeof(gl_light) );
	switch( type ){
	case LIGHT_POINT:gl_light.type=LIGHT_POINT;break;
	case LIGHT_SPOT:gl_light.type=LIGHT_SPOT;break;
	default:gl_light.type=LIGHT_DISTANT;
	}
	gl_light.color[0]=gl_light.color[1]=gl_light.color[2]=1;
	gl_light.theta=0;
	gl_light.phi=1.57079632679f;	//HALFPI
	gl_light.direction[2]=1;
	setRange( 1000 );
}

gxLight::~gxLight(){
}

void gxLight::setRange( float r ){
	gl_light.atten1=1.0f/r;
}

void gxLight::setColor( const float rgb[3] ){
	memcpy( gl_light.color,rgb,12 );
}

void gxLight::getColor( float rgb[3] ){
	memcpy( rgb,gl_light.color,12 );
}

void gxLight::setPosition( const float pos[3] ){
	memcpy( gl_light.position,pos,12 );
}

void gxLight::setDirection( const float dir[3] ){
	memcpy( gl_light.direction,dir,12 );
}

void gxLight::setConeAngles( float inner,float outer ){
	gl_light.theta=inner;
	gl_light.phi=outer;
}
