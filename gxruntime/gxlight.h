
#ifndef GXLIGHT_H
#define GXLIGHT_H

#include <cstring>

#ifdef _WIN32
#include <d3d.h>
#endif

class gxScene;

class gxLight{
public:
	gxLight( gxScene *scene,int type );
	~gxLight();

#ifdef _WIN32
	D3DLIGHT7 d3d_light;
#endif

private:
	gxScene *scene;

	/***** GX INTERFACE *****/
public:
	enum{
		LIGHT_DISTANT=1,LIGHT_POINT=2,LIGHT_SPOT=3
	};
	void setRange( float range );
#ifdef _WIN32
	void setColor( const float rgb[3] ){ memcpy( &d3d_light.dcvDiffuse,rgb,12 ); }
#else
	void setColor( const float rgb[3] );
#endif
	void setPosition( const float pos[3] );
	void setDirection( const float dir[3] );
	void setConeAngles( float inner,float outer );

#ifdef _WIN32
	void getColor( float rgb[3] ){ memcpy( rgb,&d3d_light.dcvDiffuse,12 ); }
#else
	void getColor( float rgb[3] );
#endif
};

#endif
