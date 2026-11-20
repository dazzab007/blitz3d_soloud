
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
#else
	//what the GL pipeline needs (mirrors D3DLIGHT7 as gxlight.cpp sets it up)
	struct GLLight{
		int type;				//LIGHT_DISTANT/POINT/SPOT
		float color[3];
		float position[3];		//world space
		float direction[3];		//world space, the way the light travels
		float atten1;			//1/range: attenuation = 1/(atten1*distance)
		float theta,phi;		//inner/outer cone (full angles, radians)
	}gl_light;
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
