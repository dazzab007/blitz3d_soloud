//gxMesh for the GL pipeline (macOS port): vertices and indices live in CPU arrays
//(the engine fills them through lock()/setVertex()/setTriangle()), and are copied
//to GL buffers lazily when the mesh is next drawn after a change.
#include "../gxmesh.h"
#include "../gxgraphics.h"
#include "gxgl_sdl.h"
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <cstdio>

//Attribute layout of dxVertex: coords(3f) normal(3f) argb(4 ubyte, BGRA) tex(4f)
static void setupAttribs(){
	const GLsizei stride=44;
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0,3,GL_FLOAT,GL_FALSE,stride,(void*)0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1,3,GL_FLOAT,GL_FALSE,stride,(void*)12 );
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2,GL_BGRA,GL_UNSIGNED_BYTE,GL_TRUE,stride,(void*)24 );
	glEnableVertexAttribArray( 3 );
	glVertexAttribPointer( 3,4,GL_FLOAT,GL_FALSE,stride,(void*)28 );
}

gxMesh::gxMesh( gxGraphics *g,void *vs,unsigned short *is,int max_vs,int max_ts ):
graphics(g),vertex_buff(vs),tri_indices(is),max_verts(max_vs),max_tris(max_ts),
mesh_dirty(false),locked_verts(0),gl_vao(0),gl_vbo(0),gl_ibo(0),gl_stale(true){
	static_assert( sizeof(dxVertex)==44,"dxVertex layout" );
	if( !vertex_buff ){
		dxVertex *v=new dxVertex[max_verts>0 ? max_verts : 1];
		memset( v,0,sizeof(dxVertex)*(max_verts>0 ? max_verts : 1) );
		vertex_buff=v;
	}
}

gxMesh::~gxMesh(){
	if( gl_vao && gxSdlMakeGLCurrent() ){
		glDeleteBuffers( 1,&gl_vbo );
		glDeleteBuffers( 1,&gl_ibo );
		glDeleteVertexArrays( 1,&gl_vao );
	}
	delete[] (dxVertex*)vertex_buff;
	delete[] tri_indices;
}

bool gxMesh::lock( bool all ){
	if( locked_verts ) return true;
	locked_verts=(dxVertex*)vertex_buff;
	mesh_dirty=false;
	gl_stale=true;		//the caller is about to change vertices and/or indices
	return true;
}

void gxMesh::unlock(){
	locked_verts=0;
}

void gxMesh::backup(){}
void gxMesh::restore(){}

//Draw triangles first_tri.. of the mesh; indices are relative to first_vert (as
//DrawIndexedPrimitiveVB treats them). The scene has set up the pipeline state.
void gxMesh::render( int first_vert,int vert_cnt,int first_tri,int tri_cnt ){
	if( tri_cnt<=0 ) return;
	if( !gl_vao ){
		glGenVertexArrays( 1,&gl_vao );
		glGenBuffers( 1,&gl_vbo );
		glGenBuffers( 1,&gl_ibo );
		gl_stale=true;
	}
	glBindVertexArray( gl_vao );
	glBindBuffer( GL_ARRAY_BUFFER,gl_vbo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER,gl_ibo );
	if( gl_stale ){
		glBufferData( GL_ARRAY_BUFFER,(GLsizeiptr)sizeof(dxVertex)*max_verts,vertex_buff,GL_DYNAMIC_DRAW );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER,(GLsizeiptr)sizeof(unsigned short)*3*max_tris,tri_indices,GL_DYNAMIC_DRAW );
		setupAttribs();
		gl_stale=false;
	}
	glDrawElementsBaseVertex( GL_TRIANGLES,tri_cnt*3,GL_UNSIGNED_SHORT,(void*)(size_t)(first_tri*3*2),first_vert );
}
