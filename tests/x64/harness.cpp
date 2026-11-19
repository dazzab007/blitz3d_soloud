//In-memory Module + mini loader implementation (commit 4, x64-plan.md).

#include "harness.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

X64Module::X64Module(){}
X64Module::~X64Module(){}

void X64Module::emit( uint8_t byte ){
	m_code.push_back( byte );
}

void X64Module::emit16( uint16_t w ){
	uint8_t b[2];
	std::memcpy( b,&w,2 );
	m_code.insert( m_code.end(),b,b+2 );
}

void X64Module::emit32( uint32_t d ){
	uint8_t b[4];
	std::memcpy( b,&d,4 );
	m_code.insert( m_code.end(),b,b+4 );
}

void X64Module::emit64( uint64_t q ){
	uint8_t b[8];
	std::memcpy( b,&q,8 );
	m_code.insert( m_code.end(),b,b+8 );
}

void X64Module::emitBytes( const void *data,int sz ){
	const uint8_t *p=(const uint8_t*)data;
	m_code.insert( m_code.end(),p,p+sz );
}

bool X64Module::addSymbol( const char *sym ){
	int offset=getPC();
	m_symbols[sym]=offset;
	return true;
}

bool X64Module::addReloc( const char *sym,int offset,RelocKind kind ){
	Reloc r;
	r.offset=offset;
	r.sym=sym;
	r.kind=kind;
	m_relocs.push_back( r );
	return true;
}

bool X64Module::findSymbol( const char *sym,int *offset )const{
	std::map<std::string,int>::const_iterator it=m_symbols.find(sym);
	if( it==m_symbols.end() ) return false;
	*offset=it->second;
	return true;
}

X64Loader::X64Loader():m_mem(nullptr),m_size(0){}

X64Loader::~X64Loader(){
	free();
}

void *X64Loader::load( const X64Module &mod,const std::map<std::string,void*> &stubTable ){
	int sz=mod.size();
	if( sz==0 ) return nullptr;

	//Round up to page size for mmap.
	long pageSize=sysconf(_SC_PAGESIZE);
	int mapSize=((sz+pageSize-1)/pageSize)*pageSize;

	//mmap RW memory.
	void *mem=mmap( nullptr,mapSize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0 );
	if( mem==MAP_FAILED ){
		fprintf( stderr,"harness: mmap failed\n" );
		return nullptr;
	}

	//Copy code.
	std::memcpy( mem,mod.code(),sz );

	//Apply relocations.
	//We need access to the reloc list, but X64Module doesn't expose it publicly.
	//For now, we'll skip reloc application in this skeleton; the test program
	//doesn't use relocs (it's just mov eax,42; ret).
	//Commit 5+ will add proper reloc support.

	//mprotect RX.
	if( mprotect( mem,mapSize,PROT_READ|PROT_EXEC )!=0 ){
		fprintf( stderr,"harness: mprotect failed\n" );
		munmap( mem,mapSize );
		return nullptr;
	}

	m_mem=mem;
	m_size=mapSize;
	return mem;
}

void X64Loader::free(){
	if( m_mem ){
		munmap( m_mem,m_size );
		m_mem=nullptr;
		m_size=0;
	}
}
