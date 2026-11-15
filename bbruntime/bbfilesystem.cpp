
#include "std.h"
#include "bbfilesystem.h"
#include "bbstream.h"
#include "bb_handle.h"
#include "bb_handle_types.h"
#include <fstream>

gxFileSystem *gx_filesys;

struct bbFile : public bbStream{
	filebuf *buf;
	bbFile( filebuf *f ):buf(f){
	}
	~bbFile(){
#ifndef _WIN32
		g_handle_table.forget( this );
#endif
		delete buf;
	}
	int read( char *buff,int size ){
		return buf->sgetn( (char*)buff,size );
	}
	int write( const char *buff,int size ){
		return buf->sputn( (char*)buff,size );
	}
	int avail(){
		return buf->in_avail();
	}
	int eof(){
		return buf->sgetc()==EOF;
	}
};

static set<bbFile*> file_set;

static inline void debugFile( bbFile *f ){
	if( debug ){
		if( !file_set.count( f ) ) RTEX( "File does not exist" );
	}
}

static inline void debugDir( gxDir *d ){
	if( debug ){
		if( !gx_filesys->verifyDir( d ) ) RTEX( "Directory does not exist" );
	}
}

static bbFile *open( BBStr *f,int n ){
	string t=*f;
	filebuf *buf=d_new filebuf();
	if( buf->open( t.c_str(),n|ios_base::binary ) ){
		bbFile *f=d_new bbFile( buf );
		file_set.insert( f );
		return f;
	}
	delete buf;
	return 0;
}

BB_HANDLE(bbFile) bbReadFile( BBStr *f ){
	bbFile *r=open( f,ios_base::in );
	return r;
}

BB_HANDLE(bbFile) bbWriteFile( BBStr *f ){
	bbFile *r=open( f,ios_base::out|ios_base::trunc );
	return r;
}

BB_HANDLE(bbFile) bbOpenFile( BBStr *f ){
	bbFile *r=open( f,ios_base::in|ios_base::out );
	return r;
}

void bbCloseFile( BB_HANDLE(bbStream) s_h ){
	// A file handle is a stream-family handle (bbFile upcasts to bbStream).
	bbStream *s=s_h;
#ifndef _WIN32
	if( !s ) return;
#endif
	bbFile *f=(bbFile*)s;
	debugFile( f );
	file_set.erase( f );
	delete f;
}

int bbFilePos( BB_HANDLE(bbFile) f_h ){
	bbFile *f=f_h;
	return f->buf->pubseekoff( 0,ios_base::cur );
}

int bbSeekFile( BB_HANDLE(bbFile) f_h,int pos ){
	bbFile *f=f_h;
	return f->buf->pubseekoff( pos,ios_base::beg );
}

BB_HANDLE(gxDir) bbReadDir( BBStr *d ){
	string t=*d;delete d;
	return gx_filesys->openDir( t,0 );
}

void bbCloseDir( BB_HANDLE(gxDir) d_h ){
	gxDir *d=d_h;
#ifndef _WIN32
	if( !d ) return;
	g_handle_table.forget( d );
#endif
	gx_filesys->closeDir( d );
}

BBStr *bbNextFile( BB_HANDLE(gxDir) d_h ){
	gxDir *d=d_h;
	debugDir( d );
	return d_new BBStr( d->getNextFile() );
}

BBStr *bbCurrentDir(){
	return d_new BBStr( gx_filesys->getCurrentDir() );
}

void bbChangeDir( BBStr *d ){
	gx_filesys->setCurrentDir( *d );
	delete d;
}

void bbCreateDir( BBStr *d ){
	gx_filesys->createDir( *d );
	delete d;
}

void bbDeleteDir( BBStr *d ){
	gx_filesys->deleteDir( *d );
	delete d;
}

int bbFileType( BBStr *f ){
	string t=*f;delete f;
	int n=gx_filesys->getFileType( t );
	return n==gxFileSystem::FILE_TYPE_FILE ? 1 : (n==gxFileSystem::FILE_TYPE_DIR ? 2 : 0);
}

int	bbFileSize( BBStr *f ){
	string t=*f;delete f;
	return gx_filesys->getFileSize( t );
}

void bbCopyFile( BBStr *f,BBStr *to ){
	string src=*f,dest=*to;
	delete f;delete to;
	gx_filesys->copyFile( src,dest );
}

void bbDeleteFile( BBStr *f ){
	gx_filesys->deleteFile( *f );
	delete f;
}

bool filesystem_create(){
	if( gx_filesys=gx_runtime->openFileSystem( 0 ) ){
		return true;
	}
	return false;
}

bool filesystem_destroy(){
	while( file_set.size() ) bbCloseFile( *file_set.begin() );
	gx_runtime->closeFileSystem( gx_filesys );
	return true;
}

void filesystem_link( void(*rtSym)(const char*,void*) ){
	rtSym( "%OpenFile$filename",RTSYM_CAST(bbOpenFile) );
	rtSym( "%ReadFile$filename",RTSYM_CAST(bbReadFile) );
	rtSym( "%WriteFile$filename",RTSYM_CAST(bbWriteFile) );
	rtSym( "CloseFile%file_stream",RTSYM_CAST(bbCloseFile) );
	rtSym( "%FilePos%file_stream",RTSYM_CAST(bbFilePos) );
	rtSym( "%SeekFile%file_stream%pos",RTSYM_CAST(bbSeekFile) );

	rtSym( "%ReadDir$dirname",RTSYM_CAST(bbReadDir) );
	rtSym( "CloseDir%dir",RTSYM_CAST(bbCloseDir) );
	rtSym( "$NextFile%dir",RTSYM_CAST(bbNextFile) );
	rtSym( "$CurrentDir",RTSYM_CAST(bbCurrentDir) );
	rtSym( "ChangeDir$dir",RTSYM_CAST(bbChangeDir) );
	rtSym( "CreateDir$dir",RTSYM_CAST(bbCreateDir) );
	rtSym( "DeleteDir$dir",RTSYM_CAST(bbDeleteDir) );

	rtSym( "%FileSize$file",RTSYM_CAST(bbFileSize) );
	rtSym( "%FileType$file",RTSYM_CAST(bbFileType) );
	rtSym( "CopyFile$file$to",RTSYM_CAST(bbCopyFile) );
	rtSym( "DeleteFile$file",RTSYM_CAST(bbDeleteFile) );
}
