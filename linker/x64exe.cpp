#include "x64exe.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static const char MAGIC[8]={ 'B','B','X','6','4','E','X','E' };

//FNV-1a, 64 bit: catches truncation and accidental corruption of the attached program
static uint64_t fnv64( const std::string &s ){
	uint64_t h=1469598103934665603ULL;
	for( size_t k=0;k<s.size();++k ){ h^=(unsigned char)s[k];h*=1099511628211ULL; }
	return h;
}

static bool readFile( const std::string &path,std::string &out ){
	FILE *f=fopen( path.c_str(),"rb" );
	if( !f ) return false;
	char buf[65536];
	size_t n;
	out.clear();
	while( (n=fread( buf,1,sizeof(buf),f ))>0 ) out.append( buf,n );
	fclose( f );
	return true;
}

bool x64WriteExe( const std::string &launcher,const std::string &outFile,
	const X64Module &mod,unsigned flags,std::string &err ){
	std::string image;
	if( !readFile( launcher,image ) ){ err="Unable to read launcher '"+launcher+"'";return false; }
	std::string blob=mod.serialize();
	image+=blob;
	uint32_t f=flags;
	image.append( (const char*)&f,4 );
	uint64_t sz=blob.size();
	image.append( (const char*)&sz,8 );
	uint64_t hash=fnv64( blob );
	image.append( (const char*)&hash,8 );
	image.append( MAGIC,8 );

	FILE *o=fopen( outFile.c_str(),"wb" );
	if( !o ){ err="Unable to create '"+outFile+"'";return false; }
	bool ok=fwrite( image.data(),1,image.size(),o )==image.size();
	ok=fclose( o )==0 && ok;
	if( !ok ){ err="Error writing '"+outFile+"'";return false; }
	chmod( outFile.c_str(),0755 );
	return true;
}

bool x64ReadExe( const std::string &exeFile,X64Module &mod,unsigned &flags,std::string &err ){
	FILE *f=fopen( exeFile.c_str(),"rb" );
	if( !f ){ err="Unable to open '"+exeFile+"'";return false; }
	const int trailer=4+8+8+8;
	std::string blob;
	bool ok=false;
	if( fseek( f,-trailer,SEEK_END )==0 ){
		char t[trailer];
		if( fread( t,1,trailer,f )==(size_t)trailer && memcmp( t+20,MAGIC,8 )==0 ){
			uint32_t fl;uint64_t sz,hash;
			memcpy( &fl,t,4 );memcpy( &sz,t+4,8 );memcpy( &hash,t+12,8 );
			long total=ftell( f );
			if( sz<=(uint64_t)(total-trailer) && fseek( f,(long)(total-trailer-(long)sz),SEEK_SET )==0 ){
				blob.resize( (size_t)sz );
				if( sz==0 || fread( &blob[0],1,(size_t)sz,f )==(size_t)sz ){
					if( fnv64( blob )!=hash ){
						fclose( f );
						err="The attached program is damaged";
						return false;
					}
					flags=fl;
					ok=true;
				}
			}
		}
	}
	fclose( f );
	if( !ok ){ err="No program is attached to this executable";return false; }
	if( !X64Module::deserialize( blob.data(),blob.size(),mod ) ){
		err="The attached program is damaged";
		return false;
	}
	return true;
}

std::string x64SelfPath(){
#ifdef __APPLE__
	char buf[PATH_MAX];
	uint32_t n=sizeof(buf);
	if( _NSGetExecutablePath( buf,&n )!=0 ) return "";
	char real[PATH_MAX];
	if( realpath( buf,real ) ) return real;
	return buf;
#else
	char real[PATH_MAX];
	ssize_t n=readlink( "/proc/self/exe",real,sizeof(real)-1 );
	if( n<=0 ) return "";
	real[n]=0;
	return real;
#endif
}
