
#include "std.h"
#include "bbstream.h"
#include "bb_handle.h"
#include "bb_handle_types.h"

static set<bbStream*> stream_set;

void debugStream( bbStream *s ){
	if( stream_set.count(s) ) return;
	RTEX( "Stream does not exist" );
}

bbStream::bbStream(){
	stream_set.insert( this );
}

bbStream::~bbStream(){
#ifndef _WIN32
	g_handle_table.forget( this );
#endif
	stream_set.erase( this );
}

int bbEof( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	return s->eof();
}

int bbReadAvail( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	return s->avail();
}

int bbReadByte( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	int n=0;
	s->read( (char*)&n,1 );
	return n;
}

int bbReadShort( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	int n=0;
	s->read( (char*)&n,2 );
	return n;
}

int bbReadInt( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	int n=0;
	s->read( (char*)&n,4 );
	return n;
}

float bbReadFloat( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	float n=0;
	s->read( (char*)&n,4 );
	return n;
}

BBStr *bbReadString( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	int len;
	BBStr *str=d_new BBStr();
	if( s->read( (char*)&len,4 ) ){
		char *buff=d_new char[len];
		if( s->read( buff,len ) ){
			*str=string( buff,len );
		}
		delete[] buff;
	}
	return str;
}

BBStr *bbReadLine( BB_HANDLE(bbStream) s_h ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	unsigned char c;
	BBStr *str=d_new BBStr();
	for(;;){
		if( s->read( (char*)&c,1 )!=1 ) break;
		if( c=='\n' ) break;
		if( c!='\r' ) *str+=c;
	}
	return str;
}

void bbWriteByte( BB_HANDLE(bbStream) s_h,int n ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	s->write( (char*)&n,1 );
}

void bbWriteShort( BB_HANDLE(bbStream) s_h,int n ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	s->write( (char*)&n,2 );
}

void bbWriteInt( BB_HANDLE(bbStream) s_h,int n ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	s->write( (char*)&n,4 );
}

void bbWriteFloat( BB_HANDLE(bbStream) s_h,float n ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	s->write( (char*)&n,4 );
}

void bbWriteString( BB_HANDLE(bbStream) s_h,BBStr *t ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	int n=t->size();
	s->write( (char*)&n,4 );
	s->write( t->data(),t->size() );
	delete t;
}

void bbWriteLine( BB_HANDLE(bbStream) s_h,BBStr *t ){
	bbStream *s=s_h;
	if( debug ) debugStream( s );
	s->write( t->data(),t->size() );
	s->write( "\r\n",2 );
	delete t;
}

void bbCopyStream( BB_HANDLE(bbStream) s_h,BB_HANDLE(bbStream) d_h,int buff_size ){
	bbStream *s=s_h;
	bbStream *d=d_h;
	if( debug ){
		debugStream( s );debugStream( d ); 
		if( buff_size<1 || buff_size>1024*1024 ) RTEX( "Illegal buffer size" );
	}
	char *buff=d_new char[buff_size];
	while( s->eof()==0 && d->eof()==0 ){
		int n=s->read( buff,buff_size );
		d->write( buff,n );
		if( n<buff_size ) break;
	}
	delete buff;
}

bool stream_create(){
	return true;
}

bool stream_destroy(){
	return true;
}

void stream_link( void(*rtSym)(const char*,void*) ){
	rtSym( "%Eof%stream",RTSYM_CAST(bbEof) );
	rtSym( "%ReadAvail%stream",RTSYM_CAST(bbReadAvail) );
	rtSym( "%ReadByte%stream",RTSYM_CAST(bbReadByte) );
	rtSym( "%ReadShort%stream",RTSYM_CAST(bbReadShort) );
	rtSym( "%ReadInt%stream",RTSYM_CAST(bbReadInt) );
	rtSym( "#ReadFloat%stream",RTSYM_CAST(bbReadFloat) );
	rtSym( "$ReadString%stream",RTSYM_CAST(bbReadString) );
	rtSym( "$ReadLine%stream",RTSYM_CAST(bbReadLine) );
	rtSym( "WriteByte%stream%byte",RTSYM_CAST(bbWriteByte) );
	rtSym( "WriteShort%stream%short",RTSYM_CAST(bbWriteShort) );
	rtSym( "WriteInt%stream%int",RTSYM_CAST(bbWriteInt) );
	rtSym( "WriteFloat%stream#float",RTSYM_CAST(bbWriteFloat) );
	rtSym( "WriteString%stream$string",RTSYM_CAST(bbWriteString) );
	rtSym( "WriteLine%stream$string",RTSYM_CAST(bbWriteLine) );
	rtSym( "CopyStream%src_stream%dest_stream%buffer_size=16384",RTSYM_CAST(bbCopyStream) );
}



