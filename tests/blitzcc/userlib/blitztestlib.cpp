//Test userlib for tests/blitzcc: plain C functions called from BASIC through
//userlibs/*.decls. Types follow the userlib ABI: ints and floats by value, strings
//as char*, '*' parameters as void* (a bank's data, or an object's field array).
#include <cstring>
#include <cstdio>

extern "C" {

int tl_add( int a,int b ){ return a+b; }
float tl_scale( float x,float k ){ return x*k; }
float tl_mix( int a,float b,int c,float d ){ return a+b*2+c*3+d*4; }
int tl_sum9( int a,int b,int c,int d,int e,int f,int g,int h,int i ){ return a+b+c+d+e+f+g+h+i; }

const char *tl_greet( const char *name ){
	static char buf[256];
	snprintf( buf,sizeof(buf),"Hello, %s!",name );
	return buf;
}

//bank data pointer
int tl_sumbytes( void *data,int n ){
	int s=0;
	for( int k=0;k<n;++k ) s+=((unsigned char*)data)[k];
	return s;
}
void tl_fill( void *data,int n,int v ){ memset( data,v,n ); }

//an object passed to a '*' parameter arrives as its field array: one 8-byte slot
//per field on x86-64 (BBField), the int in the low 4 bytes
int tl_objsum( void *fields ){
	long long *f=(long long*)fields;
	return (int)f[0]+(int)f[1];
}

}
