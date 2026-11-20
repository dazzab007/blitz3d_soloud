//POSIX gxFileSystem and gxDir (macOS port, Prompt 5). The Windows versions are
//gxfilesystem.cpp / gxdir.cpp. Paths use '/'; getCurrentDir ends with a '/'.

#include "../gxfilesystem.h"
#include "../gxdir.h"
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

static std::set<gxDir*> dir_set;

gxFileSystem::gxFileSystem(){
	dir_set.clear();
}

gxFileSystem::~gxFileSystem(){
	while( dir_set.size() ) closeDir( *dir_set.begin() );
}

bool gxFileSystem::createDir( const std::string &dir ){
	return mkdir( dir.c_str(),0777 )==0;
}

bool gxFileSystem::deleteDir( const std::string &dir ){
	return rmdir( dir.c_str() )==0;
}

//the Windows version always fails; keep that (Blitz has no CreateFile command)
bool gxFileSystem::createFile( const std::string &file ){
	return false;
}

bool gxFileSystem::deleteFile( const std::string &file ){
	return unlink( file.c_str() )==0;
}

bool gxFileSystem::copyFile( const std::string &src,const std::string &dest ){
	FILE *in=fopen( src.c_str(),"rb" );
	if( !in ) return false;
	FILE *out=fopen( dest.c_str(),"wb" );	//overwrites, like CopyFile(..., false)
	if( !out ){ fclose( in );return false; }
	char buf[65536];
	size_t n;
	bool ok=true;
	while( (n=fread( buf,1,sizeof(buf),in ))>0 ){
		if( fwrite( buf,1,n,out )!=n ){ ok=false;break; }
	}
	fclose( in );
	if( fclose( out )!=0 ) ok=false;
	return ok;
}

bool gxFileSystem::renameFile( const std::string &src,const std::string &dest ){
	return rename( src.c_str(),dest.c_str() )==0;
}

bool gxFileSystem::setCurrentDir( const std::string &dir ){
	return chdir( dir.c_str() )==0;
}

std::string gxFileSystem::getCurrentDir()const{
	char buff[PATH_MAX];
	if( !getcwd( buff,sizeof(buff) ) ) return "";
	std::string t=buff;
	if( t.size() && t[t.size()-1]!='/' ) t+='/';
	return t;
}

int gxFileSystem::getFileSize( const std::string &name )const{
	struct stat st;
	if( stat( name.c_str(),&st )!=0 ) return 0;
	return S_ISDIR( st.st_mode ) ? 0 : (int)st.st_size;
}

int gxFileSystem::getFileType( const std::string &name )const{
	struct stat st;
	if( stat( name.c_str(),&st )!=0 ) return FILE_TYPE_NONE;
	return S_ISDIR( st.st_mode ) ? FILE_TYPE_DIR : FILE_TYPE_FILE;
}

//A snapshot of the directory, including "." and ".." like FindFirstFile does.
gxDir *gxFileSystem::openDir( const std::string &name,int flags ){
	DIR *d=opendir( name.c_str() );
	if( !d ) return 0;
	std::vector<std::string> names;
	while( struct dirent *e=readdir( d ) ) names.push_back( e->d_name );
	closedir( d );
	std::sort( names.begin(),names.end() );	//deterministic order
	gxDir *r=new gxDir( names );
	dir_set.insert( r );
	return r;
}

gxDir *gxFileSystem::verifyDir( gxDir *d ){
	return dir_set.count( d ) ? d : 0;
}

void gxFileSystem::closeDir( gxDir *d ){
	if( dir_set.erase( d ) ) delete d;
}

gxDir::gxDir( const std::vector<std::string> &entries ):names( entries ),pos( 0 ){
}

gxDir::~gxDir(){
}

//"" when the directory is exhausted (as on Windows)
std::string gxDir::getNextFile(){
	if( pos>=names.size() ) return "";
	return names[pos++];
}
