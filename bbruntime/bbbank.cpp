
#include "std.h"
#include "bbbank.h"
#include "bbstream.h"
#include "bb_handle.h"

struct bbBank{
	char *data;
	int size,capacity;

	bbBank( int sz ):size(sz){
		capacity=(size+15)&~15;
		data=d_new char[capacity];
		memset( data,0,size );
	}
	virtual ~bbBank(){
#ifndef _WIN32
		g_handle_table.forget(this);
#endif
		delete[] data;
	}
	void resize( int n ){
		if( n>size ){
			if( n>capacity ){
				capacity=capacity*3/2;
				if( n>capacity ) capacity=n;
				capacity=(capacity+15)&~15;
				char *p=d_new char[capacity];
				memcpy( p,data,size );
				delete[] data;
				data=p;
			}else memset( data+size,0,n-size );
		}
		size=n;
	}
};

#include "bb_handle_types.h"

static set<bbBank*> bank_set;

static inline void debugBank( bbBank *b ){
	if( debug ){
		if( !bank_set.count( b ) ) RTEX( "bbBank does not exist" );
	}
}

static inline void debugBank( bbBank *b,int offset ){
	if( debug ){
		debugBank( b );
		if( offset>=b->size ) RTEX( "Offset out of range" );
	}
}

BB_HANDLE(bbBank) bbCreateBank( int size ){
	bbBank *b=d_new bbBank( size );
	bank_set.insert( b );
	return b;
}

void bbFreeBank( BB_HANDLE(bbBank) h ){
	bbBank *b=h;
	if( bank_set.erase( b ) ) delete b;
}

int bbBankSize( BB_HANDLE(bbBank) h ){
	bbBank *b=h;
	debugBank( b );
	return b->size;
}

void  bbResizeBank( BB_HANDLE(bbBank) h,int size ){
	bbBank *b=h;
	debugBank( b );
	b->resize( size );
}

void  bbCopyBank( BB_HANDLE(bbBank) src_h,int src_p,BB_HANDLE(bbBank) dest_h,int dest_p,int count ){
	bbBank *src=src_h;
	bbBank *dest=dest_h;
	if( debug ){ debugBank( src,src_p+count-1 );debugBank( dest,dest_p+count-1 ); }
	memmove( dest->data+dest_p,src->data+src_p,count );
}

int  bbPeekByte( BB_HANDLE(bbBank) h,int offset ){
	bbBank *b=h;
	debugBank( b,offset );
	return *(unsigned char*)(b->data+offset);
}

int  bbPeekShort( BB_HANDLE(bbBank) h,int offset ){
	bbBank *b=h;
	debugBank( b,offset+1 );
	return *(unsigned short*)(b->data+offset);
}

int  bbPeekInt( BB_HANDLE(bbBank) h,int offset ){
	bbBank *b=h;
	debugBank( b,offset+3 );
	return *(int*)(b->data+offset);
}

float  bbPeekFloat( BB_HANDLE(bbBank) h,int offset ){
	bbBank *b=h;
	debugBank( b,offset+3 );
	return *(float*)(b->data+offset);
}

void  bbPokeByte( BB_HANDLE(bbBank) h,int offset,int value ){
	bbBank *b=h;
	debugBank( b,offset );
	*(char*)(b->data+offset)=value;
}

void  bbPokeShort( BB_HANDLE(bbBank) h,int offset,int value ){
	bbBank *b=h;
	debugBank( b,offset );
	*(unsigned short*)(b->data+offset)=value;
}

void  bbPokeInt( BB_HANDLE(bbBank) h,int offset,int value ){
	bbBank *b=h;
	debugBank( b,offset );
	*(int*)(b->data+offset)=value;
}

void  bbPokeFloat( BB_HANDLE(bbBank) h,int offset,float value ){
	bbBank *b=h;
	debugBank( b,offset );
	*(float*)(b->data+offset)=value;
}

int   bbReadBytes( BB_HANDLE(bbBank) h,BB_HANDLE(bbStream) s_h,int offset,int count ){
	bbBank *b=h;
	bbStream *s=s_h;
	if( debug ){
		debugBank( b,offset+count-1 );
		debugStream( s );
	}
	return s->read( b->data+offset,count );
}

int   bbWriteBytes( BB_HANDLE(bbBank) h,BB_HANDLE(bbStream) s_h,int offset,int count ){
	bbBank *b=h;
	bbStream *s=s_h;
	if( debug ){
		debugBank( b,offset+count-1 );
		debugStream( s );
	}
	return s->write( b->data+offset,count );
}

int  bbCallDLL( BBStr *dll,BBStr *fun,BB_HANDLE(bbBank) in_h,BB_HANDLE(bbBank) out_h ){
	bbBank *in=in_h;
	bbBank *out=out_h;
	if( debug ){
		if( in ) debugBank( in );
		if( out ) debugBank( out );
	}
	int t=gx_runtime->callDll( *dll,*fun,
		in ? in->data : 0,in ? in->size : 0,
		out ? out->data : 0,out ? out->size : 0 );
	delete dll;delete fun;
	return t;
}

bool bank_create(){
	return true;
}

bool bank_destroy(){
	while( bank_set.size() ){
		bbFreeBank( *bank_set.begin() );
	}
	return true;
}

// On macOS, BB_HANDLE(T) is bbH<T> (a struct), so function pointers with
// BB_HANDLE params can't implicitly convert to void*. Cast through reinterpret_cast.
#ifdef _WIN32
#define RTSYM_CAST(f) f
#else
#define RTSYM_CAST(f) (void*)(f)
#endif

void bank_link( void(*rtSym)(const char*,void*) ){
	rtSym( "%CreateBank%size=0",RTSYM_CAST(bbCreateBank) );
	rtSym( "FreeBank%bank",RTSYM_CAST(bbFreeBank) );
	rtSym( "%BankSize%bank",RTSYM_CAST(bbBankSize) );
	rtSym( "ResizeBank%bank%size",RTSYM_CAST(bbResizeBank) );
	rtSym( "CopyBank%src_bank%src_offset%dest_bank%dest_offset%count",RTSYM_CAST(bbCopyBank) );
	rtSym( "%PeekByte%bank%offset",RTSYM_CAST(bbPeekByte) );
	rtSym( "%PeekShort%bank%offset",RTSYM_CAST(bbPeekShort) );
	rtSym( "%PeekInt%bank%offset",RTSYM_CAST(bbPeekInt) );
	rtSym( "#PeekFloat%bank%offset",RTSYM_CAST(bbPeekFloat) );
	rtSym( "PokeByte%bank%offset%value",RTSYM_CAST(bbPokeByte) );
	rtSym( "PokeShort%bank%offset%value",RTSYM_CAST(bbPokeShort) );
	rtSym( "PokeInt%bank%offset%value",RTSYM_CAST(bbPokeInt) );
	rtSym( "PokeFloat%bank%offset#value",RTSYM_CAST(bbPokeFloat) );
	rtSym( "%ReadBytes%bank%file%offset%count",RTSYM_CAST(bbReadBytes) );
	rtSym( "%WriteBytes%bank%file%offset%count",RTSYM_CAST(bbWriteBytes) );
	rtSym( "%CallDLL$dll_name$func_name%in_bank=0%out_bank=0",RTSYM_CAST(bbCallDLL) );
}


