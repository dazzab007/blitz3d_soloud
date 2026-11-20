//gxScene on OpenGL 3.3 core (macOS port, Prompt 5 step 3).
//
//Gxscene.cpp programs a Direct3D 7 device. Here the same state is held in
//GLState and applied through one "fixed function" uber shader:
//  - D3D per-vertex lighting (8 lights: distant/point/spot, D3D attenuation and
//    spot falloff, ambient, specular, vertex colour, emissive, flat shading)
//  - up to 8 texture stages, combined like D3DTOP_* (the blend modes of Blitz)
//  - linear fog, alpha test, framebuffer blend modes, z modes, culling
//Rendering goes to an FBO the size of the target canvas: the canvas pixels are
//uploaded at begin() when 2D code changed them, and the viewport is read back
//into the canvas at end(), so 2D drawing, ReadPixel and Flip keep working as
//they do on the CPU canvas.
//
//Coordinate conventions: Blitz hands us D3D matrices (left-handed, row vectors,
//clip z in [0,1]). They are uploaded as flat row-major arrays and multiplied as
//`M*v` in GLSL, which is exactly the row-vector product v*M. The projection's z
//column is rewritten to GL's [-1,1] range, and the winding keeps its meaning
//(clockwise = front), so culling needs no flip.

#include "../gxscene.h"
#include "../gxgraphics.h"
#include "../gxmesh.h"
#include "../gxlight.h"
#include "../gxcanvas.h"
#include "gxgl_sdl.h"
#include "gxsurface_sdl.h"
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

extern gxRuntime *gx_runtime;

namespace{

const int NSTAGES=8;
const int NLIGHTS=8;

std::string vertexSource(){
	return std::string(
	"#version 330 core\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec3 aNormal;\n"
	"layout(location=2) in vec4 aColor;\n"
	"layout(location=3) in vec4 aTex;\n"
	"uniform mat4 uWorld,uView,uProj;\n"
	"uniform mat3 uNormalMat;\n"
	"uniform int uLightType[8];\n"			//0 off, 1 distant, 2 point, 3 spot
	"uniform vec3 uLightPos[8],uLightDir[8],uLightColor[8];\n"
	"uniform float uLightAtten[8],uLightCosIn[8],uLightCosOut[8];\n"
	"uniform vec3 uAmbient;\n"
	"uniform vec4 uMatDiffuse;\n"
	"uniform float uSpecPower,uSpecAmount;\n"
	"uniform int uVertexColor,uEmissive,uVertexAlpha;\n"
	"uniform int uTexCoordSet[8],uTexGen[8],uTexMatOn[8];\n"
	"uniform mat3 uTexMat[8];\n"
	"out vec4 vColor;\n"
	"flat out vec4 vColorFlat;\n"
	"out vec3 vSpec;\n"
	"flat out vec3 vSpecFlat;\n"
	"out vec2 vUV[8];\n"
	"out float vDepth;\n"
	"void main(){\n"
	"	vec4 pv=uView*(uWorld*vec4(aPos,1.0));\n"
	"	gl_Position=uProj*pv;\n"
	"	vDepth=pv.z;\n"
	"	vec3 n=normalize(uNormalMat*aNormal);\n"
	"	vec3 view=normalize(-pv.xyz);\n"
	//material colours: vertex colour replaces the material when COLORVERTEX is on
	"	vec3 matD=uMatDiffuse.rgb,matA=uMatDiffuse.rgb,emis=vec3(0.0);\n"
	"	float alpha=uMatDiffuse.a;\n"
	"	if( uVertexColor!=0 ){\n"
	"		if( uEmissive!=0 ) emis=aColor.rgb; else { matD=aColor.rgb;matA=aColor.rgb; }\n"
	"		if( uVertexAlpha!=0 ) alpha=aColor.a;\n"
	"	}\n"
	"	vec3 diff=vec3(0.0),spec=vec3(0.0);\n"
	"	for( int i=0;i<8;++i ){\n"
	"		int t=uLightType[i];\n"
	"		if( t==0 ) continue;\n"
	"		vec3 L;float att=1.0;\n"
	"		if( t==1 ){ L=-normalize(uLightDir[i]); }\n"
	"		else{\n"
	"			vec3 d=uLightPos[i]-pv.xyz;\n"
	"			float dist=length(d);\n"
	"			L=d/max(dist,1e-6);\n"
	"			att=1.0/max(uLightAtten[i]*dist,1e-6);\n"
	"			if( t==3 ){\n"
	"				float rho=dot(-L,normalize(uLightDir[i]));\n"
	"				if( rho<=uLightCosOut[i] ) att=0.0;\n"
	"				else if( rho<uLightCosIn[i] ) att*=(rho-uLightCosOut[i])/(uLightCosIn[i]-uLightCosOut[i]);\n"
	"			}\n"
	"		}\n"
	"		float nl=dot(n,L);\n"
	"		if( nl>0.0 ){\n"
	"			diff+=att*nl*uLightColor[i];\n"
	"			if( uSpecAmount>0.0 ){\n"
	"				vec3 h=normalize(L+view);\n"
	"				float nh=dot(n,h);\n"
	"				if( nh>0.0 ) spec+=att*pow(nh,uSpecPower)*uLightColor[i];\n"
	"			}\n"
	"		}\n"
	"	}\n"
	"	vec3 col=clamp(emis+uAmbient*matA+diff*matD,0.0,1.0);\n"
	"	vec3 sp=clamp(spec*uSpecAmount,0.0,1.0);\n"
	"	vColor=vec4(col,alpha);vColorFlat=vColor;\n"
	"	vSpec=sp;vSpecFlat=sp;\n"
	//texture coordinates per stage
	"	for( int i=0;i<8;++i ){\n"
	"		vec2 uv=uTexCoordSet[i]==1 ? aTex.zw : aTex.xy;\n"
	"		if( uTexGen[i]==1 ) uv=vec2( n.x*0.5+0.5,-n.y*0.5+0.5 );\n"
	"		else if( uTexMatOn[i]!=0 ) uv=(uTexMat[i]*vec3(uv,1.0)).xy;\n"
	"		vUV[i]=uv;\n"
	"	}\n"
	"}\n");
}

std::string fragmentSource(){
	std::string s=
	"#version 330 core\n"
	"in vec4 vColor;\n"
	"flat in vec4 vColorFlat;\n"
	"in vec3 vSpec;\n"
	"flat in vec3 vSpecFlat;\n"
	"in vec2 vUV[8];\n"
	"in float vDepth;\n"
	"out vec4 fragColor;\n"
	"uniform int uFlat,uNTex,uAlphaTest,uFogOn;\n"
	"uniform float uAlphaRef;\n"
	"uniform vec3 uFogColor;\n"
	"uniform vec2 uFogRange;\n"
	"uniform int uTexBlend[8],uTexAlpha[8];\n"
	"uniform sampler2D uTex0,uTex1,uTex2,uTex3,uTex4,uTex5,uTex6,uTex7;\n"
	"vec4 stage( vec4 cur,vec4 t,int blend,int hasAlpha ){\n"
	"	vec3 c=cur.rgb;\n"
	"	if( blend==1 ) c=t.rgb*t.a+cur.rgb*(1.0-t.a);\n"		//D3DTOP_BLENDTEXTUREALPHA
	"	else if( blend==2 ) c=t.rgb*cur.rgb;\n"				//MODULATE
	"	else if( blend==3 ) c=t.rgb+cur.rgb;\n"				//ADD
	"	else if( blend==4 ){ float d=4.0*dot(t.rgb-0.5,cur.rgb-0.5);c=vec3(d); }\n"	//DOTPRODUCT3
	"	else if( blend==5 ) c=t.rgb*cur.rgb*2.0;\n"			//MODULATE2X
	"	float a=hasAlpha!=0 ? t.a*cur.a : cur.a;\n"
	"	return vec4( clamp(c,0.0,1.0),a );\n"
	"}\n"
	"void main(){\n"
	"	vec4 cur=uFlat!=0 ? vColorFlat : vColor;\n"
	"	vec3 spec=uFlat!=0 ? vSpecFlat : vSpec;\n";
	for( int i=0;i<NSTAGES;++i ){
		char b[256];
		snprintf( b,sizeof(b),"	if( uNTex>%d ) cur=stage( cur,texture( uTex%d,vUV[%d] ),uTexBlend[%d],uTexAlpha[%d] );\n",i,i,i,i,i );
		s+=b;
	}
	s+=
	"	cur.rgb=clamp( cur.rgb+spec,0.0,1.0 );\n"
	"	if( uFogOn!=0 ){\n"
	"		float f=clamp( (uFogRange.y-vDepth)/(uFogRange.y-uFogRange.x),0.0,1.0 );\n"
	"		cur.rgb=mix( uFogColor,cur.rgb,f );\n"
	"	}\n"
	"	if( uAlphaTest!=0 && cur.a<=uAlphaRef ) discard;\n"
	"	fragColor=cur;\n"
	"}\n";
	return s;
}

//---- small matrix helpers (flat row-major, the D3D layout) ----
void identity4( float *m ){
	memset( m,0,64 );
	m[0]=m[5]=m[10]=m[15]=1;
}

void fromMatrix( float *m,const gxScene::Matrix *src ){
	if( !src ){ identity4( m );return; }
	for( int r=0;r<4;++r ){
		m[r*4]=src->elements[r][0];
		m[r*4+1]=src->elements[r][1];
		m[r*4+2]=src->elements[r][2];
		m[r*4+3]=r==3 ? 1.0f : 0.0f;
	}
}

//3x3 inverse transpose of a 3x3 row-major matrix (identity if singular)
void inverseTranspose3( const float *a,float *out ){
	float c00=a[4]*a[8]-a[5]*a[7],c01=a[5]*a[6]-a[3]*a[8],c02=a[3]*a[7]-a[4]*a[6];
	float det=a[0]*c00+a[1]*c01+a[2]*c02;
	if( fabsf( det )<1e-12f ){
		memset( out,0,36 );out[0]=out[4]=out[8]=1;
		return;
	}
	float c10=a[2]*a[7]-a[1]*a[8],c11=a[0]*a[8]-a[2]*a[6],c12=a[1]*a[6]-a[0]*a[7];
	float c20=a[1]*a[5]-a[2]*a[4],c21=a[2]*a[3]-a[0]*a[5],c22=a[0]*a[4]-a[1]*a[3];
	float id=1.0f/det;
	//inverse = adjugate/det = cofactor^T/det, so inverse-transpose = cofactor/det
	out[0]=c00*id;out[1]=c01*id;out[2]=c02*id;
	out[3]=c10*id;out[4]=c11*id;out[5]=c12*id;
	out[6]=c20*id;out[7]=c21*id;out[8]=c22*id;
}

}	//namespace

struct gxScene::GLState{
	GLuint prog,fbo,colorTex,depthRb;
	int fw,fh;
	unsigned lastMod;
	bool fboValid;
	std::map<std::string,GLint> uloc;

	//camera/world
	float world[16],view[16],proj[16],normalMat[9];
	int vx,vy,vw,vh;
	float ambient[3],ambient2[3],fogColor[3],fogNr,fogFr;
	int fogMode,zmode;
	bool wire,flipped;

	//render state
	float color[3],alpha,shininess;
	int blend,fx;
	struct Stage{
		gxCanvas *canvas;
		int blend,flags;
		float mat[9];
		bool matValid;
	}stages[NSTAGES];
	int nTex;

	//lights as set at begin() (view space)
	struct L{
		int type;
		float pos[3],dir[3],color[3],atten,cosIn,cosOut;
	}lights[NLIGHTS];
	int nLights;

	struct Tex{ GLuint id;int mod,w,h,flags; };
	std::map<gxCanvas*,Tex> textures;

	GLState():prog(0),fbo(0),colorTex(0),depthRb(0),fw(0),fh(0),lastMod(~0u),fboValid(false){
		identity4( world );identity4( view );identity4( proj );
		float id3[9]={1,0,0,0,1,0,0,0,1};memcpy( normalMat,id3,36 );
		vx=vy=vw=vh=0;
		ambient[0]=ambient[1]=ambient[2]=.5f;
		ambient2[0]=ambient2[1]=ambient2[2]=0;
		fogColor[0]=fogColor[1]=fogColor[2]=0;
		fogNr=1;fogFr=1000;fogMode=FOG_NONE;zmode=ZMODE_NORMAL;
		wire=flipped=false;
		color[0]=color[1]=color[2]=1;alpha=1;shininess=0;blend=BLEND_REPLACE;fx=0;
		memset( stages,0,sizeof(stages) );
		nTex=0;nLights=0;
		memset( lights,0,sizeof(lights) );
	}

	GLint U( const char *name ){
		std::map<std::string,GLint>::iterator it=uloc.find( name );
		if( it!=uloc.end() ) return it->second;
		GLint l=glGetUniformLocation( prog,name );
		uloc[name]=l;
		return l;
	}
};

static void (*s_prevHook)( gxCanvas * )=0;
static gxScene::GLState *s_hookState=0;

static GLuint compileShader( GLenum type,const std::string &src ){
	GLuint s=glCreateShader( type );
	const char *p=src.c_str();
	glShaderSource( s,1,&p,0 );
	glCompileShader( s );
	GLint ok=0;
	glGetShaderiv( s,GL_COMPILE_STATUS,&ok );
	if( !ok ){
		char log[2048];glGetShaderInfoLog( s,sizeof(log),0,log );
		fprintf( stderr,"gxScene: shader compile error: %s\n",log );
		glDeleteShader( s );
		return 0;
	}
	return s;
}

static void canvasFreed( gxCanvas *c );

gxScene::gxScene( gxGraphics *g,gxCanvas *t ):
graphics(g),target(t),n_texs(0),tris_drawn(0),gl(new GLState){

	wbuffer=dither=antialias=wireframe=flipped=false;
	ambient=ambient2=fogcolor=0;
	caps_level=100;
	fogmode=FOG_NONE;zmode=ZMODE_NORMAL;
	fogrange_nr=1;fogrange_fr=1000;
	ortho_proj=false;
	frustum_nr=frustum_fr=frustum_w=frustum_h=0;
	shininess=0;blend=BLEND_REPLACE;fx=0;
	memset( texstate,0,sizeof(texstate) );

	gxSdlMakeGLCurrent();

	GLuint vs=compileShader( GL_VERTEX_SHADER,vertexSource() );
	GLuint fs=compileShader( GL_FRAGMENT_SHADER,fragmentSource() );
	if( vs && fs ){
		gl->prog=glCreateProgram();
		glAttachShader( gl->prog,vs );
		glAttachShader( gl->prog,fs );
		glLinkProgram( gl->prog );
		GLint ok=0;
		glGetProgramiv( gl->prog,GL_LINK_STATUS,&ok );
		if( !ok ){
			char log[2048];glGetProgramInfoLog( gl->prog,sizeof(log),0,log );
			fprintf( stderr,"gxScene: shader link error: %s\n",log );
			glDeleteProgram( gl->prog );gl->prog=0;
		}
	}
	if( vs ) glDeleteShader( vs );
	if( fs ) glDeleteShader( fs );

	gl->fw=target->getWidth();
	gl->fh=target->getHeight();
	glGenTextures( 1,&gl->colorTex );
	glBindTexture( GL_TEXTURE_2D,gl->colorTex );
	glTexImage2D( GL_TEXTURE_2D,0,GL_RGBA8,gl->fw,gl->fh,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,0 );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST );
	glGenRenderbuffers( 1,&gl->depthRb );
	glBindRenderbuffer( GL_RENDERBUFFER,gl->depthRb );
	glRenderbufferStorage( GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,gl->fw,gl->fh );
	glGenFramebuffers( 1,&gl->fbo );
	glBindFramebuffer( GL_FRAMEBUFFER,gl->fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gl->colorTex,0 );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,gl->depthRb );
	gl->fboValid=glCheckFramebufferStatus( GL_FRAMEBUFFER )==GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer( GL_FRAMEBUFFER,0 );
	if( !gl->fboValid ) fprintf( stderr,"gxScene: framebuffer incomplete\n" );

	glProvokingVertex( GL_FIRST_VERTEX_CONVENTION );	//D3D flat shading uses the first vertex

	s_prevHook=gxCanvasFreedHook;
	s_hookState=gl;
	gxCanvasFreedHook=canvasFreed;

	//defaults, as the Windows constructor sets them
	float gray[3]={.5f,.5f,.5f},black[3]={0,0,0};
	setAmbient( gray );setAmbient2( black );setFogColor( black );
	setFogRange( 1,1000 );setFogMode( FOG_NONE );setZMode( ZMODE_NORMAL );
	setPerspProj( 1,1000,1,1 );
	setViewport( 0,0,target->getWidth(),target->getHeight() );
	setViewMatrix( 0 );setWorldMatrix( 0 );
}

gxScene::~gxScene(){
	while( _allLights.size() ) freeLight( *_allLights.begin() );
	gxCanvasFreedHook=s_prevHook;
	s_hookState=0;
	if( gxSdlMakeGLCurrent() ){
		for( std::map<gxCanvas*,GLState::Tex>::iterator it=gl->textures.begin();it!=gl->textures.end();++it ){
			glDeleteTextures( 1,&it->second.id );
		}
		if( gl->fbo ) glDeleteFramebuffers( 1,&gl->fbo );
		if( gl->colorTex ) glDeleteTextures( 1,&gl->colorTex );
		if( gl->depthRb ) glDeleteRenderbuffers( 1,&gl->depthRb );
		if( gl->prog ) glDeleteProgram( gl->prog );
	}
	delete gl;
}

static void canvasFreed( gxCanvas *c ){
	if( s_hookState ){
		std::map<gxCanvas*,gxScene::GLState::Tex>::iterator it=s_hookState->textures.find( c );
		if( it!=s_hookState->textures.end() ){
			if( gxSdlMakeGLCurrent() ) glDeleteTextures( 1,&it->second.id );
			s_hookState->textures.erase( it );
		}
	}
	if( s_prevHook ) s_prevHook( c );
}

/////////////
// State   //
/////////////
int gxScene::hwTexUnits(){ return NSTAGES; }
int gxScene::gfxDriverCaps3D(){ return caps_level; }

void gxScene::setWBuffer( bool n ){ wbuffer=n; }
void gxScene::setHWMultiTex( bool e ){}
void gxScene::setDither( bool n ){ dither=n; }
void gxScene::setAntialias( bool n ){ antialias=n; }
void gxScene::setWireframe( bool n ){ gl->wire=n; }
void gxScene::setFlippedTris( bool n ){ gl->flipped=n; }

void gxScene::setAmbient( const float rgb[3] ){ memcpy( gl->ambient,rgb,12 ); }
void gxScene::setAmbient2( const float rgb[3] ){ memcpy( gl->ambient2,rgb,12 ); }
void gxScene::setFogColor( const float rgb[3] ){ memcpy( gl->fogColor,rgb,12 ); }
void gxScene::setFogRange( float nr,float fr ){ gl->fogNr=nr;gl->fogFr=fr; }
void gxScene::setFogMode( int mode ){ gl->fogMode=mode; }
void gxScene::setZMode( int mode ){ gl->zmode=mode; }
void gxScene::setViewport( int x,int y,int w,int h ){ gl->vx=x;gl->vy=y;gl->vw=w;gl->vh=h; }

//D3D projection (row vectors, z in [0,1]) with the z column rewritten for GL's [-1,1]
static void buildProj( float *m,bool persp,float nr,float fr,float w,float h ){
	memset( m,0,64 );
	float Q=persp ? fr/(fr-nr) : 1/(fr-nr);
	m[0]=persp ? 2*nr/w : 2/w;
	m[5]=persp ? 2*nr/h : 2/h;
	m[10]=Q;
	m[11]=persp ? 1.0f : 0.0f;
	m[14]=-Q*nr;
	m[15]=persp ? 0.0f : 1.0f;
	for( int r=0;r<4;++r ) m[r*4+2]=2*m[r*4+2]-m[r*4+3];
}

void gxScene::setOrthoProj( float nr,float fr,float w,float h ){
	frustum_nr=nr;frustum_fr=fr;frustum_w=w;frustum_h=h;ortho_proj=true;
	buildProj( gl->proj,false,nr,fr,w,h );
}

void gxScene::setPerspProj( float nr,float fr,float w,float h ){
	frustum_nr=nr;frustum_fr=fr;frustum_w=w;frustum_h=h;ortho_proj=false;
	buildProj( gl->proj,true,nr,fr,w,h );
}

static void updateNormalMat( gxScene::GLState *gl ){
	//normals use the inverse transpose of world*view (row-vector product)
	float m[9];
	for( int r=0;r<3;++r ){
		for( int c=0;c<3;++c ){
			float s=0;
			for( int k=0;k<3;++k ) s+=gl->world[r*4+k]*gl->view[k*4+c];
			m[r*3+c]=s;
		}
	}
	inverseTranspose3( m,gl->normalMat );
}

void gxScene::setViewMatrix( const Matrix *m ){
	fromMatrix( gl->view,m );
	updateNormalMat( gl );
}

void gxScene::setWorldMatrix( const Matrix *m ){
	fromMatrix( gl->world,m );
	updateNormalMat( gl );
}

void gxScene::setRenderState( const RenderState &rs ){
	memcpy( gl->color,rs.color,12 );
	gl->alpha=rs.alpha;
	gl->shininess=rs.shininess;
	gl->blend=rs.blend;
	gl->fx=rs.fx;
	//stages with a texture and a blend mode are packed from stage 0, as in the D3D version
	gl->nTex=0;
	for( int k=0;k<MAX_TEXTURES;++k ){
		const RenderState::TexState &ts=rs.tex_states[k];
		if( !ts.canvas || !ts.blend ) continue;
		GLState::Stage &st=gl->stages[gl->nTex++];
		st.canvas=ts.canvas;
		st.blend=ts.blend;
		st.flags=ts.flags;
		st.matValid=ts.matrix!=0;
		if( ts.matrix ){
			for( int r=0;r<3;++r ) for( int c=0;c<3;++c ) st.mat[r*3+c]=ts.matrix->elements[r][c];
		}
	}
	n_texs=gl->nTex;
}

void gxScene::setEnabled( gxLight *light,bool enabled ){}

gxLight *gxScene::createLight( int flags ){
	gxLight *l=new gxLight( this,flags );
	_allLights.insert( l );
	return l;
}

void gxScene::freeLight( gxLight *l ){
	if( _allLights.erase( l ) ) delete l;
}

int gxScene::getTrianglesDrawn()const{ return tris_drawn; }

/////////////////
// Rendering   //
/////////////////
static void uploadTarget( gxScene::GLState *gl,gxCanvas *target ){
	GxNativeSurface *s=target->getSurface();
	glBindTexture( GL_TEXTURE_2D,gl->colorTex );
	glPixelStorei( GL_UNPACK_ALIGNMENT,4 );
	//canvas rows run top to bottom, the FBO bottom to top: upload row by row, flipped
	std::vector<uint32_t> flip( (size_t)s->width*s->height );
	for( int y=0;y<s->height;++y ){
		memcpy( &flip[(size_t)(s->height-1-y)*s->width],&s->pixels[(size_t)y*s->width],(size_t)s->width*4 );
	}
	glTexSubImage2D( GL_TEXTURE_2D,0,0,0,s->width,s->height,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,flip.data() );
	gl->lastMod=(unsigned)target->getModify();
}

bool gxScene::begin( const std::vector<gxLight*> &lights ){
	if( !gl->prog || !gl->fboValid || !gxSdlMakeGLCurrent() ) return false;
	glBindFramebuffer( GL_FRAMEBUFFER,gl->fbo );
	if( (unsigned)target->getModify()!=gl->lastMod ) uploadTarget( gl,target );

	//lights are snapshotted into view space now, like D3D's SetLight
	_curLights.clear();
	gl->nLights=0;
	for( size_t n=0;n<lights.size() && n<(size_t)NLIGHTS;++n ){
		gxLight *light=lights[n];
		_curLights.push_back( light );
		const gxLight::GLLight &d=light->gl_light;
		GLState::L &l=gl->lights[gl->nLights++];
		l.type=d.type;
		const float *V=gl->view;
		for( int c=0;c<3;++c ){
			l.pos[c]=d.position[0]*V[c]+d.position[1]*V[4+c]+d.position[2]*V[8+c]+V[12+c];
			l.dir[c]=d.direction[0]*V[c]+d.direction[1]*V[4+c]+d.direction[2]*V[8+c];
			l.color[c]=d.color[c];
		}
		l.atten=d.atten1;
		l.cosIn=cosf( d.theta*.5f );
		l.cosOut=cosf( d.phi*.5f );
	}
	return true;
}

void gxScene::clear( const float rgb[3],float alpha,float z,bool clear_argb,bool clear_z ){
	if( !clear_argb && !clear_z ) return;
	glBindFramebuffer( GL_FRAMEBUFFER,gl->fbo );
	glEnable( GL_SCISSOR_TEST );
	glScissor( gl->vx,gl->fh-gl->vy-gl->vh,gl->vw,gl->vh );
	GLbitfield bits=0;
	if( clear_argb ){
		glColorMask( GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE );
		glClearColor( rgb[0],rgb[1],rgb[2],alpha );
		bits|=GL_COLOR_BUFFER_BIT;
	}
	if( clear_z ){
		glDepthMask( GL_TRUE );
		glClearDepth( z );
		bits|=GL_DEPTH_BUFFER_BIT;
	}
	glClear( bits );
	glDisable( GL_SCISSOR_TEST );
}

//GL texture for a canvas, (re)uploaded when the canvas changed
static GLuint textureFor( gxScene::GLState *gl,gxCanvas *c ){
	GxNativeSurface *s=c->getSurface();
	int flags=c->getFlags();
	std::map<gxCanvas*,gxScene::GLState::Tex>::iterator it=gl->textures.find( c );
	gxScene::GLState::Tex *t;
	if( it==gl->textures.end() ){
		gxScene::GLState::Tex n={0,-1,0,0,0};
		glGenTextures( 1,&n.id );
		t=&(gl->textures[c]=n);
	}else t=&it->second;
	if( t->mod!=c->getModify() || t->w!=s->width || t->h!=s->height || t->flags!=flags ){
		glBindTexture( GL_TEXTURE_2D,t->id );
		glPixelStorei( GL_UNPACK_ALIGNMENT,4 );
		glTexImage2D( GL_TEXTURE_2D,0,GL_RGBA8,s->width,s->height,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,s->pixels.data() );
		bool mip=(flags & gxCanvas::CANVAS_TEX_MIPMAP)!=0,point=(flags & gxCanvas::CANVAS_TEX_POINT)!=0;
		if( mip ) glGenerateMipmap( GL_TEXTURE_2D );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip ? (point ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR) : (point ? GL_NEAREST : GL_LINEAR) );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,point ? GL_NEAREST : GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,(flags & gxCanvas::CANVAS_TEX_CLAMPU) ? GL_CLAMP_TO_EDGE : GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,(flags & gxCanvas::CANVAS_TEX_CLAMPV) ? GL_CLAMP_TO_EDGE : GL_REPEAT );
		t->mod=c->getModify();t->w=s->width;t->h=s->height;t->flags=flags;
	}
	return t->id;
}

//Push all the state to the pipeline before a draw
static void applyState( gxScene::GLState *gl ){
	glUseProgram( gl->prog );
	const int fx=gl->fx;

	//viewport (D3D origin is the top left, GL's the bottom left)
	glViewport( gl->vx,gl->fh-gl->vy-gl->vh,gl->vw,gl->vh );
	glEnable( GL_SCISSOR_TEST );
	glScissor( gl->vx,gl->fh-gl->vy-gl->vh,gl->vw,gl->vh );

	//z mode
	switch( gl->zmode ){
	case gxScene::ZMODE_NORMAL:glEnable( GL_DEPTH_TEST );glDepthMask( GL_TRUE );break;
	case gxScene::ZMODE_DISABLE:glDisable( GL_DEPTH_TEST );glDepthMask( GL_FALSE );break;
	case gxScene::ZMODE_CMPONLY:glEnable( GL_DEPTH_TEST );glDepthMask( GL_FALSE );break;
	}
	glDepthFunc( GL_LEQUAL );
	glColorMask( GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE );

	//culling: clockwise is front, as in D3D with a left-handed projection
	glFrontFace( GL_CW );
	if( fx & gxScene::FX_DOUBLESIDED ){
		glDisable( GL_CULL_FACE );
	}else{
		glEnable( GL_CULL_FACE );
		glCullFace( gl->flipped ? GL_FRONT : GL_BACK );	//D3DCULL_CCW normally, CW when flipped
	}
	glPolygonMode( GL_FRONT_AND_BACK,gl->wire ? GL_LINE : GL_FILL );

	//framebuffer blending
	switch( gl->blend ){
	case gxScene::BLEND_REPLACE:glDisable( GL_BLEND );break;
	case gxScene::BLEND_ALPHA:glEnable( GL_BLEND );glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA );break;
	case gxScene::BLEND_MULTIPLY:glEnable( GL_BLEND );glBlendFunc( GL_DST_COLOR,GL_ZERO );break;
	case gxScene::BLEND_ADD:glEnable( GL_BLEND );glBlendFunc( GL_SRC_ALPHA,GL_ONE );break;
	default:glDisable( GL_BLEND );
	}

	//transforms
	glUniformMatrix4fv( gl->U( "uWorld" ),1,GL_FALSE,gl->world );
	glUniformMatrix4fv( gl->U( "uView" ),1,GL_FALSE,gl->view );
	glUniformMatrix4fv( gl->U( "uProj" ),1,GL_FALSE,gl->proj );
	glUniformMatrix3fv( gl->U( "uNormalMat" ),1,GL_FALSE,gl->normalMat );

	//lights: fullbright = none (ambient white), condlight = non-directional only (ambient2)
	int ltype[8]={0};
	float lpos[24]={0},ldir[24]={0},lcol[24]={0},latt[8]={0},lci[8]={0},lco[8]={0};
	for( int i=0;i<gl->nLights;++i ){
		const gxScene::GLState::L &l=gl->lights[i];
		bool on=!(fx & gxScene::FX_FULLBRIGHT);
		if( on && (fx & gxScene::FX_CONDLIGHT) && l.type==gxLight::LIGHT_DISTANT ) on=false;
		ltype[i]=on ? l.type : 0;
		memcpy( lpos+i*3,l.pos,12 );memcpy( ldir+i*3,l.dir,12 );memcpy( lcol+i*3,l.color,12 );
		latt[i]=l.atten;lci[i]=l.cosIn;lco[i]=l.cosOut;
	}
	glUniform1iv( gl->U( "uLightType" ),8,ltype );
	glUniform3fv( gl->U( "uLightPos" ),8,lpos );
	glUniform3fv( gl->U( "uLightDir" ),8,ldir );
	glUniform3fv( gl->U( "uLightColor" ),8,lcol );
	glUniform1fv( gl->U( "uLightAtten" ),8,latt );
	glUniform1fv( gl->U( "uLightCosIn" ),8,lci );
	glUniform1fv( gl->U( "uLightCosOut" ),8,lco );
	const float *amb=(fx & gxScene::FX_FULLBRIGHT) ? (const float*)0 : ((fx & gxScene::FX_CONDLIGHT) ? gl->ambient2 : gl->ambient);
	float white[3]={1,1,1};
	glUniform3fv( gl->U( "uAmbient" ),1,amb ? amb : white );

	//material
	glUniform4f( gl->U( "uMatDiffuse" ),gl->color[0],gl->color[1],gl->color[2],gl->alpha );
	float shin=gl->shininess;
	glUniform1f( gl->U( "uSpecAmount" ),shin>0 ? (shin<1 ? shin : 1.0f) : 0.0f );
	glUniform1f( gl->U( "uSpecPower" ),shin*128.0f );
	glUniform1i( gl->U( "uVertexColor" ),(fx & (gxScene::FX_VERTEXCOLOR|gxScene::FX_EMISSIVE)) ? 1 : 0 );
	glUniform1i( gl->U( "uEmissive" ),(fx & gxScene::FX_EMISSIVE) ? 1 : 0 );
	glUniform1i( gl->U( "uVertexAlpha" ),(fx & gxScene::FX_VERTEXALPHA) ? 1 : 0 );
	glUniform1i( gl->U( "uFlat" ),(fx & gxScene::FX_FLATSHADED) ? 1 : 0 );

	//fog and alpha test
	bool fog=gl->fogMode==gxScene::FOG_LINEAR && !(fx & gxScene::FX_NOFOG);
	glUniform1i( gl->U( "uFogOn" ),fog ? 1 : 0 );
	glUniform3fv( gl->U( "uFogColor" ),1,gl->fogColor );
	glUniform2f( gl->U( "uFogRange" ),gl->fogNr,gl->fogFr );
	glUniform1i( gl->U( "uAlphaTest" ),(fx & gxScene::FX_ALPHATEST) ? 1 : 0 );
	float ref=(fx & gxScene::FX_VERTEXALPHA) ? 0.0f : (int)(128*gl->alpha)/255.0f;
	glUniform1f( gl->U( "uAlphaRef" ),ref );

	//texture stages
	int tblend[8]={0},talpha[8]={0},tset[8]={0},tgen[8]={0},tmat[8]={0};
	float tm[72]={0};
	for( int i=0;i<gl->nTex && i<NSTAGES;++i ){
		const gxScene::GLState::Stage &st=gl->stages[i];
		int cflags=st.canvas->getFlags();
		glActiveTexture( GL_TEXTURE0+i );
		glBindTexture( GL_TEXTURE_2D,textureFor( gl,st.canvas ) );
		tblend[i]=st.blend;
		talpha[i]=(cflags & gxCanvas::CANVAS_TEX_ALPHA) ? 1 : 0;
		tset[i]=(st.flags & gxScene::TEX_COORDS2) ? 1 : 0;
		tgen[i]=(cflags & gxCanvas::CANVAS_TEX_SPHERE) ? 1 : 0;	//(cube maps are not supported yet)
		tmat[i]=st.matValid ? 1 : 0;
		memcpy( tm+i*9,st.mat,36 );
	}
	glActiveTexture( GL_TEXTURE0 );
	glUniform1i( gl->U( "uNTex" ),gl->nTex<NSTAGES ? gl->nTex : NSTAGES );
	glUniform1iv( gl->U( "uTexBlend" ),8,tblend );
	glUniform1iv( gl->U( "uTexAlpha" ),8,talpha );
	glUniform1iv( gl->U( "uTexCoordSet" ),8,tset );
	glUniform1iv( gl->U( "uTexGen" ),8,tgen );
	glUniform1iv( gl->U( "uTexMatOn" ),8,tmat );
	glUniformMatrix3fv( gl->U( "uTexMat" ),8,GL_FALSE,tm );
	for( int i=0;i<NSTAGES;++i ){
		char name[16];snprintf( name,sizeof(name),"uTex%d",i );
		glUniform1i( gl->U( name ),i );
	}
}

void gxScene::render( gxMesh *mesh,int first_vert,int vert_cnt,int first_tri,int tri_cnt ){
	if( !gl->prog ) return;
	applyState( gl );
	mesh->render( first_vert,vert_cnt,first_tri,tri_cnt );
	tris_drawn+=tri_cnt;
}

void gxScene::end(){
	if( !gl->prog || !gl->fboValid ) return;
	glDisable( GL_SCISSOR_TEST );
	int x=gl->vx,y=gl->vy,w=gl->vw,h=gl->vh;
	if( x<0 ){ w+=x;x=0; }
	if( y<0 ){ h+=y;y=0; }
	if( x+w>gl->fw ) w=gl->fw-x;
	if( y+h>gl->fh ) h=gl->fh-y;
	GxNativeSurface *s=target->getSurface();
	if( w>0 && h>0 ){
		std::vector<uint32_t> px( (size_t)w*h );
		glPixelStorei( GL_PACK_ALIGNMENT,4 );
		glReadPixels( x,gl->fh-y-h,w,h,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,px.data() );
		for( int j=0;j<h;++j ){
			const uint32_t *src=&px[(size_t)(h-1-j)*w];
			uint32_t *dst=&s->pixels[(size_t)(y+j)*s->width+x];
			for( int i=0;i<w;++i ) dst[i]=src[i]|0xff000000u;
		}
	}
	glBindFramebuffer( GL_FRAMEBUFFER,0 );
	RECT r={ x,y,x+(w>0 ? w : 0),y+(h>0 ? h : 0) };
	target->damage( r );
	gl->lastMod=(unsigned)target->getModify();
}
