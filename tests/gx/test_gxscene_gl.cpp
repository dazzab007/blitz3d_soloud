//SDL gxScene on OpenGL (Prompt 5 step 3): renders real geometry through the scene
//API and reads the result from the back canvas. Needs a GL context (a hidden
//window); skipped when there is none.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <SDL.h>
#include "../../gxruntime/gxruntime.h"
#include "../../gxruntime/gxgraphics.h"
#include "../../gxruntime/sdl/gxruntime_sdl.h"

static int tests_run=0,tests_failed=0;
#define CHECK(c) do{ ++tests_run; if( !(c) ){ ++tests_failed; printf( "FAIL %s:%d: %s\n",__FILE__,__LINE__,#c ); } }while(0)
#define CHECK_PIX(canvas,x,y,want,tol) do{ ++tests_run; if( !near( (canvas)->getPixel( x,y ),want,tol ) ){ ++tests_failed; printf( "FAIL %s:%d: pixel(%d,%d)=%08x want %08x\n",__FILE__,__LINE__,x,y,(canvas)->getPixel( x,y ),(unsigned)(want) ); } }while(0)

static bool near( unsigned a,unsigned b,int tol ){
	for( int s=0;s<24;s+=8 ){
		int d=(int)((a>>s)&0xff)-(int)((b>>s)&0xff);
		if( d<-tol || d>tol ) return false;
	}
	return true;
}

static const unsigned BLACK=0xff000000,RED=0xffff0000,GREEN=0xff00ff00,BLUE=0xff0000ff,WHITE=0xffffffff;
static const int W=64,H=48;

static gxGraphics *g;
static gxScene *scene;
static gxCanvas *back;

struct Vtx{ float p[3],n[3];unsigned argb;float uv[2][2]; };

static gxMesh *makeMesh( const std::vector<Vtx> &v,const std::vector<int> &idx ){
	gxMesh *m=g->createMesh( (int)v.size(),(int)idx.size()/3,0 );
	m->lock( true );
	for( size_t k=0;k<v.size();++k ) m->setVertex( (int)k,v[k].p,v[k].n,v[k].argb,v[k].uv );
	for( size_t k=0;k<idx.size()/3;++k ) m->setTriangle( (int)k,idx[k*3],idx[k*3+1],idx[k*3+2] );
	m->unlock();
	return m;
}

//A triangle (or quad) facing the camera at depth z. cw=true winds clockwise on screen.
static gxMesh *tri( float z,unsigned argb,bool cw=true,float s=8 ){
	Vtx a={{-s,-s,z},{0,0,-1},argb,{{0,1},{0,1}}};
	Vtx b={{-s, s,z},{0,0,-1},argb,{{0,0},{0,0}}};
	Vtx c={{ s,-s,z},{0,0,-1},argb,{{1,1},{1,1}}};
	std::vector<Vtx> v;v.push_back( a );v.push_back( b );v.push_back( c );
	std::vector<int> i;
	if( cw ){ i.push_back( 0 );i.push_back( 1 );i.push_back( 2 ); }
	else{ i.push_back( 0 );i.push_back( 2 );i.push_back( 1 ); }
	return makeMesh( v,i );
}

static gxMesh *quad( float z,unsigned argb,float s=8 ){
	Vtx v0={{-s,-s,z},{0,0,-1},argb,{{0,1},{0,1}}};
	Vtx v1={{-s, s,z},{0,0,-1},argb,{{0,0},{0,0}}};
	Vtx v2={{ s, s,z},{0,0,-1},argb,{{1,0},{1,0}}};
	Vtx v3={{ s,-s,z},{0,0,-1},argb,{{1,1},{1,1}}};
	std::vector<Vtx> v;v.push_back( v0 );v.push_back( v1 );v.push_back( v2 );v.push_back( v3 );
	int idx[]={0,1,2, 0,2,3};
	return makeMesh( v,std::vector<int>( idx,idx+6 ) );
}

static gxScene::RenderState state( float r,float gc,float b,float a,int blend,int fx ){
	gxScene::RenderState rs;memset( &rs,0,sizeof(rs) );
	rs.color[0]=r;rs.color[1]=gc;rs.color[2]=b;rs.alpha=a;rs.blend=blend;rs.fx=fx;
	return rs;
}

static void frameStart( float cr=0,float cg=0,float cb=0 ){
	scene->setViewport( 0,0,W,H );
	scene->setViewMatrix( 0 );scene->setWorldMatrix( 0 );
	scene->setPerspProj( 1,100,2,1.5f );
	float black[3]={cr,cg,cb};
	std::vector<gxLight*> none;
	scene->begin( none );
	scene->clear( black,1,1,true,true );
}

static void drawOne( gxMesh *m,const gxScene::RenderState &rs ){
	scene->setRenderState( rs );
	scene->render( m,0,m->maxVerts(),0,m->maxTris() );
}

//screen position of world x,y at depth 10 (view is the identity, fov as set in frameStart)
static int sx( float x ){ return (int)((x/10+1)*.5f*W); }
static int sy( float y ){ return (int)((1-y/7.5f)*.5f*H); }

static void test_unlit_and_culling(){
	const int FB=gxScene::FX_FULLBRIGHT;
	gxMesh *m=tri( 10,0xffff0000 );
	frameStart();
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),RED,2 );			//inside the triangle
	CHECK_PIX( back,sx( 5 ),sy( 5 ),BLACK,0 );			//the other half of the square
	CHECK_PIX( back,1,1,BLACK,0 );
	CHECK( scene->getTrianglesDrawn()>=1 );

	//a counter-clockwise triangle is culled ...
	gxMesh *ccw=tri( 10,0xffff0000,false );
	frameStart();
	drawOne( ccw,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),BLACK,0 );
	//... unless double sided
	frameStart();
	drawOne( ccw,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB|gxScene::FX_DOUBLESIDED ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),RED,2 );
	//flipped triangles reverse which side is culled
	scene->setFlippedTris( true );
	frameStart();
	drawOne( ccw,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	drawOne( m,state( 0,1,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	scene->setFlippedTris( false );
	CHECK_PIX( back,sx( -5 ),sy( -5 ),RED,2 );			//ccw shows, cw (green) is culled
	g->freeMesh( m );g->freeMesh( ccw );
}

//Geometry in front of the near plane or beyond the far plane is clipped (D3D clip z is
//[0,1], GL's [-1,1]: this fails if the projection is not converted)
static void test_clip_planes(){
	const int FB=gxScene::FX_FULLBRIGHT;
	gxMesh *tooNear=quad( 0.9f,0xffff0000,0.05f ),*tooFar=quad( 150,0xff00ff00,40 ),*ok=quad( 10,0xff0000ff,8 );
	frameStart();
	drawOne( tooNear,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	drawOne( tooFar,state( 0,1,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,BLACK,0 );						//near is 1, far is 100: both clipped away
	frameStart();
	drawOne( ok,state( 0,0,1,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,BLUE,2 );
	//the planes follow setPerspProj
	scene->setPerspProj( 0.5f,100,1,0.75f );
	float black[3]={0,0,0};
	std::vector<gxLight*> none;
	scene->begin( none );scene->clear( black,1,1,true,true );
	drawOne( tooNear,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );	//0.9 is now beyond the 0.5 near plane
	scene->end();
	CHECK_PIX( back,W/2,H/2,RED,2 );
	scene->setPerspProj( 1,100,2,1.5f );
	g->freeMesh( tooNear );g->freeMesh( tooFar );g->freeMesh( ok );
}

static void test_depth(){
	const int FB=gxScene::FX_FULLBRIGHT;
	gxMesh *nearM=tri( 10,0xffff0000 ),*farM=quad( 20,0xff00ff00,16 );		//far covers the whole view
	//far drawn after near: depth test keeps the near one
	frameStart();
	drawOne( nearM,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	drawOne( farM,state( 0,1,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),RED,2 );
	CHECK_PIX( back,sx( 5 ),sy( 5 ),GREEN,2 );			//upper right: only the far quad covers it
	//z disabled: the later draw wins
	frameStart();
	scene->setZMode( gxScene::ZMODE_DISABLE );
	drawOne( nearM,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );
	drawOne( farM,state( 0,1,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),GREEN,2 );
	//compare-only: tests against depth but does not write it
	frameStart();
	scene->setZMode( gxScene::ZMODE_NORMAL );
	drawOne( farM,state( 0,1,0,1,gxScene::BLEND_REPLACE,FB ) );
	scene->setZMode( gxScene::ZMODE_CMPONLY );
	drawOne( nearM,state( 1,0,0,1,gxScene::BLEND_REPLACE,FB ) );	//drawn (in front of far), not written
	scene->setZMode( gxScene::ZMODE_NORMAL );
	drawOne( farM,state( 0,0,1,1,gxScene::BLEND_REPLACE,FB ) );		//equal depth to the first far draw: passes (LEQUAL), overwrites
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),BLUE,2 );
	g->freeMesh( nearM );g->freeMesh( farM );
}

static void test_lighting(){
	gxMesh *m=quad( 10,0xffffffff );
	gxLight *sun=scene->createLight( gxLight::LIGHT_DISTANT );
	float white[3]={1,1,1},dirTo[3]={0,0,1},dirAway[3]={0,0,-1};
	sun->setColor( white );
	std::vector<gxLight*> lights;lights.push_back( sun );

	float ambient[3]={.25f,.25f,.25f};
	scene->setAmbient( ambient );
	float black[3]={0,0,0};

	//no lights: ambient * colour. normal faces -z (toward the camera)
	scene->setViewport( 0,0,W,H );scene->setViewMatrix( 0 );scene->setWorldMatrix( 0 );scene->setPerspProj( 1,100,2,1.5f );
	std::vector<gxLight*> none;
	scene->begin( none );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,.5f,0,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff402000,3 );				//(0.25, 0.125, 0)

	//distant light shining along +z hits the camera-facing side head on: diffuse 1 + ambient
	sun->setDirection( dirTo );
	scene->begin( lights );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,.5f,0,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xffff9f00,3 );				//ambient .25 + diffuse 1: R clamps to 1, G = .25*.5+.5

	//dim colour, same direction
	float half[3]={.5f,.5f,.5f};sun->setColor( half );
	scene->begin( lights );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xffbfbfbf,3 );				//.25 + .5

	//light from behind the surface: ambient only
	sun->setDirection( dirAway );
	scene->begin( lights );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff404040,3 );

	//fullbright ignores lights and ambient; condlight uses ambient2 and only point lights
	scene->begin( lights );scene->clear( black,1,1,true,true );
	drawOne( m,state( .5f,.5f,.5f,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff808080,3 );
	float amb2[3]={.5f,0,0};scene->setAmbient2( amb2 );
	sun->setDirection( dirTo );
	scene->begin( lights );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_CONDLIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff800000,3 );				//directional light is off, ambient2 only

	//point light in front of the surface: brightness falls with distance (range/d attenuation)
	//D3D lights per vertex, so use a tiny quad: its corners are all ~5 units from the light
	g->freeMesh( m );m=quad( 10,0xffffffff,0.5f );
	gxLight *pt=scene->createLight( gxLight::LIGHT_POINT );
	float ppos[3]={0,0,5};pt->setPosition( ppos );pt->setColor( white );pt->setRange( 100 );
	std::vector<gxLight*> pl;pl.push_back( pt );
	scene->setAmbient( black );
	scene->begin( pl );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	unsigned centre=back->getPixel( W/2,H/2 );
	CHECK( (centre&0xff)>200 );							//distance 5, range 100: attenuation 20 -> saturated
	pt->setRange( 2 );									//attenuation 2/5 = .4 at the centre
	scene->begin( pl );scene->clear( black,1,1,true,true );
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,0 ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff646464,10 );			//.4 * (N.L ~ .99)
	scene->freeLight( sun );scene->freeLight( pt );
	scene->setAmbient( ambient );
	g->freeMesh( m );
}

static void test_color_blend_fog(){
	//vertex colours replace the material colour
	Vtx a={{-8,-8,10},{0,0,-1},0xff00ff00,{{0,0},{0,0}}};
	Vtx b={{-8, 8,10},{0,0,-1},0xff00ff00,{{0,0},{0,0}}};
	Vtx c={{ 8,-8,10},{0,0,-1},0xff00ff00,{{0,0},{0,0}}};
	std::vector<Vtx> v;v.push_back( a );v.push_back( b );v.push_back( c );
	int idx[]={0,1,2};
	gxMesh *vc=makeMesh( v,std::vector<int>( idx,idx+3 ) );
	frameStart();
	drawOne( vc,state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT|gxScene::FX_VERTEXCOLOR ) );
	scene->end();
	CHECK_PIX( back,sx( -5 ),sy( -5 ),GREEN,2 );
	g->freeMesh( vc );

	//alpha blend over a background
	gxMesh *m=quad( 10,0xffffffff );
	frameStart( 0,0,1 );
	drawOne( m,state( 1,0,0,.5f,gxScene::BLEND_ALPHA,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xff800080,3 );				//half red over blue
	//additive: red + blue
	frameStart( 0,0,.5f );
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_ADD,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xffff0080,3 );
	//multiply: red * (0,1,1) background = black, * (1,1,1) = red
	frameStart( 1,1,1 );
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_MULTIPLY,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,RED,3 );

	//linear fog between 5 and 25: at depth 10 the factor is (25-10)/20 = .75 of the colour
	float fogc[3]={0,0,1};
	scene->setFogColor( fogc );scene->setFogRange( 5,25 );scene->setFogMode( gxScene::FOG_LINEAR );
	frameStart();
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,0xffbf0040,4 );
	frameStart();
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT|gxScene::FX_NOFOG ) );
	scene->end();
	CHECK_PIX( back,W/2,H/2,RED,2 );					//FX_NOFOG
	scene->setFogMode( gxScene::FOG_NONE );

	//alpha test (D3D: reference = 128*alpha, GREATER): a texture's alpha below half of the
	//material alpha is discarded
	gxCanvas *at=g->createCanvas( 2,2,gxCanvas::CANVAS_TEXTURE|gxCanvas::CANVAS_TEX_RGB|gxCanvas::CANVAS_TEX_ALPHA|gxCanvas::CANVAS_TEX_POINT );
	at->setPixel( 0,0,0x40ff0000 );at->setPixel( 1,0,0xc000ff00 );		//alpha .25 (< .5) and .75
	at->setPixel( 0,1,0x40ff0000 );at->setPixel( 1,1,0xc000ff00 );
	gxScene::RenderState ars=state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT|gxScene::FX_ALPHATEST );
	ars.tex_states[0].canvas=at;ars.tex_states[0].blend=gxScene::BLEND_MULTIPLY;
	frameStart( 0,0,1 );
	drawOne( m,ars );
	scene->end();
	CHECK_PIX( back,W/2-8,H/2,BLUE,2 );		//left half: alpha .25, discarded, background shows
	CHECK_PIX( back,W/2+8,H/2,GREEN,2 );				//right half: alpha .75, drawn
	g->freeCanvas( at );
	g->freeMesh( m );
}

static void test_textures(){
	//2x2 texture: red green / blue white
	gxCanvas *tex=g->createCanvas( 2,2,gxCanvas::CANVAS_TEXTURE|gxCanvas::CANVAS_TEX_RGB|gxCanvas::CANVAS_TEX_POINT );
	CHECK( tex!=0 );
	if( !tex ) return;
	CHECK( tex->getWidth()==2 && tex->getHeight()==2 );
	tex->setPixel( 0,0,RED );tex->setPixel( 1,0,GREEN );tex->setPixel( 0,1,BLUE );tex->setPixel( 1,1,WHITE );

	gxMesh *m=quad( 10,0xffffffff );
	gxScene::RenderState rs=state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT );
	rs.tex_states[0].canvas=tex;rs.tex_states[0].blend=gxScene::BLEND_MULTIPLY;
	frameStart();
	drawOne( m,rs );
	scene->end();
	//quad uv: (0,0) top-left .. (1,1) bottom-right; texel (0,0)=red is the top-left quarter
	CHECK_PIX( back,sx( -4 ),sy( 4 ),RED,2 );
	CHECK_PIX( back,sx( 4 ),sy( 4 ),GREEN,2 );
	CHECK_PIX( back,sx( -4 ),sy( -4 ),BLUE,2 );
	CHECK_PIX( back,sx( 4 ),sy( -4 ),WHITE,2 );

	//multiplied with the material colour
	rs=state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT );
	rs.tex_states[0].canvas=tex;rs.tex_states[0].blend=gxScene::BLEND_MULTIPLY;
	frameStart();
	drawOne( m,rs );
	scene->end();
	CHECK_PIX( back,sx( 4 ),sy( 4 ),BLACK,2 );			//green * red
	CHECK_PIX( back,sx( 4 ),sy( -4 ),RED,2 );				//white * red

	//texture matrix: shift the texture by half a texture to the right (u'=u+.5)
	gxScene::Matrix tm;memset( &tm,0,sizeof(tm) );
	tm.elements[0][0]=1;tm.elements[1][1]=1;tm.elements[2][0]=.5f;tm.elements[2][2]=1;
	rs=state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT );
	rs.tex_states[0].canvas=tex;rs.tex_states[0].blend=gxScene::BLEND_MULTIPLY;rs.tex_states[0].matrix=&tm;
	frameStart();
	drawOne( m,rs );
	scene->end();
	CHECK_PIX( back,sx( -4 ),sy( 4 ),GREEN,2 );			//u .25 -> .75: the green column
	CHECK_PIX( back,sx( 4 ),sy( 4 ),RED,2 );				//u .75 -> 1.25 wraps to red

	//two stages: the second multiplies the first (red * (0,1,1) mask)
	gxCanvas *tex2=g->createCanvas( 2,2,gxCanvas::CANVAS_TEXTURE|gxCanvas::CANVAS_TEX_RGB|gxCanvas::CANVAS_TEX_POINT );
	tex2->setColor( 0x00ffff );tex2->rect( 0,0,2,2,true );
	rs=state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT );
	rs.tex_states[0].canvas=tex;rs.tex_states[0].blend=gxScene::BLEND_MULTIPLY;
	rs.tex_states[1].canvas=tex2;rs.tex_states[1].blend=gxScene::BLEND_MULTIPLY;
	frameStart();
	drawOne( m,rs );
	scene->end();
	CHECK_PIX( back,sx( -4 ),sy( 4 ),BLACK,2 );			//red * cyan
	CHECK_PIX( back,sx( 4 ),sy( 4 ),GREEN,2 );			//green * cyan
	CHECK_PIX( back,sx( -4 ),sy( -4 ),BLUE,2 );
	g->freeCanvas( tex2 );

	//editing the canvas updates the texture
	tex->setPixel( 0,0,0xff808080 );
	rs=state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT );
	rs.tex_states[0].canvas=tex;rs.tex_states[0].blend=gxScene::BLEND_MULTIPLY;
	frameStart();
	drawOne( m,rs );
	scene->end();
	CHECK_PIX( back,sx( -4 ),sy( 4 ),0xff808080,2 );
	g->freeCanvas( tex );
	g->freeMesh( m );
}

static void test_viewport_and_2d(){
	gxMesh *m=quad( 10,0xffffffff );
	//2D content drawn before the scene survives where the scene does not draw
	back->setClsColor( 0x00ff00 );back->cls();
	back->setColor( 0xffff00 );back->rect( 0,0,8,8,true );
	scene->setViewport( 16,8,32,24 );scene->setViewMatrix( 0 );scene->setWorldMatrix( 0 );
	scene->setPerspProj( 1,100,2,1.5f );
	std::vector<gxLight*> none;
	scene->begin( none );
	float c[3]={0,0,1};
	scene->clear( c,1,1,true,true );							//clears only the viewport
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,32,20,RED,2 );							//inside the viewport
	CHECK_PIX( back,4,4,0xffffff00,0 );						//2D rect preserved
	CHECK_PIX( back,60,44,GREEN,0 );							//outside: the cls colour
	CHECK_PIX( back,10,20,GREEN,0 );
	//2D drawing after the scene is on top of it and the next scene frame sees it
	back->setColor( 0xffffff );back->rect( 30,18,4,4,true );
	scene->begin( none );
	scene->end();
	CHECK_PIX( back,31,19,WHITE,0 );
	g->freeMesh( m );
	scene->setViewport( 0,0,W,H );
}

static void test_wireframe_and_camera(){
	gxMesh *m=quad( 10,0xffffffff );
	scene->setWireframe( true );
	frameStart();
	drawOne( m,state( 1,1,1,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	scene->setWireframe( false );
	CHECK_PIX( back,W/2,H/2,BLACK,0 );						//centre of the quad is empty in wireframe
	int white=0;
	for( int y=0;y<H;++y ) for( int x=0;x<W;++x ) if( back->getPixel( x,y )==WHITE ) ++white;
	CHECK( white>20 );

	//moving the camera: view matrix with the camera translated by +x shifts the image left
	gxScene::Matrix v;memset( &v,0,sizeof(v) );
	v.elements[0][0]=1;v.elements[1][1]=1;v.elements[2][2]=1;
	v.elements[3][0]=-4;		//view = translate(-4,0,0): objects move 4 left
	frameStart();
	scene->setViewMatrix( &v );
	drawOne( m,state( 1,0,0,1,gxScene::BLEND_REPLACE,gxScene::FX_FULLBRIGHT ) );
	scene->end();
	CHECK_PIX( back,sx( -4-4 )+2,H/2,RED,2 );				//the quad's left edge is now at world x=-12
	CHECK_PIX( back,W-3,H/2,BLACK,0 );						//its right edge (x=8-4=4) leaves the right side bare
	g->freeMesh( m );
}

int main(){
	setenv( "BB_HIDDEN","1",1 );
	gxRuntime *rt=gxRuntime::openRuntime();
	g=rt ? rt->openGraphics( W,H,32,0,gxGraphics::GRAPHICS_WINDOWED|gxGraphics::GRAPHICS_3D ) : 0;
	if( !g ){ printf( "test_gxscene_gl: no window available, skipped\n" );return 0; }
	scene=g->createScene( 0 );
	if( !scene ){
		printf( "test_gxscene_gl: no OpenGL context, skipped\n" );
		rt->closeGraphics( g );gxRuntime::closeRuntime( rt );
		return 0;
	}
	back=g->getBackCanvas();
	CHECK( scene->hwTexUnits()>=2 );
	CHECK( g->createScene( 0 )==0 );							//one scene at a time
	test_unlit_and_culling();
	test_clip_planes();
	test_depth();
	test_lighting();
	test_color_blend_fog();
	test_textures();
	test_viewport_and_2d();
	test_wireframe_and_camera();
	g->freeScene( scene );
	CHECK( g->verifyScene( scene )==0 );
	rt->closeGraphics( g );
	gxRuntime::closeRuntime( rt );
	printf( "test_gxscene_gl: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed ? 1 : 0;
}
