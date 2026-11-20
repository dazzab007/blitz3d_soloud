
#ifndef GXMESH_H
#define GXMESH_H

#include <cstring>

#ifdef _WIN32
#include <d3d.h>
#endif

class gxGraphics;

class gxMesh{
public:

#ifdef _WIN32
	gxMesh( gxGraphics *graphics,IDirect3DVertexBuffer7 *verts,WORD *indicies,int max_verts,int max_tris );
#else
	gxMesh( gxGraphics *graphics,void *verts,unsigned short *indicies,int max_verts,int max_tris );
#endif
	~gxMesh();

	int maxVerts()const{ return max_verts; }
	int maxTris()const{ return max_tris; }

	bool dirty()const{ return mesh_dirty; }

	void render( int first_vert,int vert_cnt,int first_tri,int tri_cnt );

	void backup();
	void restore();

private:
	struct dxVertex{
		float coords[3];
		float normal[3];
		unsigned argb;
		float tex_coords[4];
	};

	gxGraphics *graphics;
#ifdef _WIN32
	IDirect3DVertexBuffer7 *vertex_buff;
	WORD *tri_indices;
#else
	void *vertex_buff;
	unsigned short *tri_indices;
#endif

	int max_verts,max_tris;
	bool mesh_dirty;
	dxVertex *locked_verts;
#ifndef _WIN32
	unsigned gl_vao,gl_vbo,gl_ibo;	//OpenGL buffers, created on first render
	bool gl_stale;					//CPU copy changed since the last upload
	friend class gxScene;
#endif

	/***** GX INTERFACE *****/
public:
	bool lock( bool all );
	void unlock();

	//VERY NAUGHTY!!!!!
	void setVertex( int n,const void *v ){
		memcpy( locked_verts+n,v,sizeof(dxVertex) );
	}
	void setVertex( int n,const float coords[3],const float normal[3],const float tex_coords[2][2] ){
		dxVertex *t=locked_verts+n;
		memcpy( t->coords,coords,12 );
		memcpy( t->normal,normal,12 );
		t->argb=0xffffffff;
		memcpy( t->tex_coords,tex_coords,16 );
	}
	void setVertex( int n,const float coords[3],const float normal[3],unsigned argb,const float tex_coords[2][2] ){
		dxVertex *t=locked_verts+n;
		memcpy( t->coords,coords,12 );
		memcpy( t->normal,normal,12 );
		t->argb=argb;
		memcpy( t->tex_coords,tex_coords,16 );
	}
	void setTriangle( int n,int v0,int v1,int v2 ){
		tri_indices[n*3]=v0;
		tri_indices[n*3+1]=v1;
		tri_indices[n*3+2]=v2;
	}
};

#endif