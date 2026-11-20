//In-memory Module + mini loader implementation (commit 4, x64-plan.md).

#include "x64module.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstdint>

X64Module::X64Module():m_textSize(-1){}
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

void X64Module::beginData(){
	long pageSize=sysconf(_SC_PAGESIZE);
	while( m_code.size()%pageSize ) m_code.push_back( 0 );
	m_textSize=(int)m_code.size();
}

bool X64Module::addSymbol( const char *sym ){
	int offset=getPC();
	m_symbols[sym]=offset;
	return true;
}

bool X64Module::addDataSymbol( const char *sym,int offset ){
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
		fprintf( stderr,"loader: mmap failed\n" );
		return nullptr;
	}

	//Copy code.
	std::memcpy( mem,mod.code(),sz );

	//Apply relocations: module symbols first, then the stub table.
	uint8_t *base=(uint8_t*)mem;
	for( size_t i=0;i<mod.relocs().size();++i ){
		const Reloc &r=mod.relocs()[i];
		uint64_t target;
		int off;
		if( mod.findSymbol( r.sym.c_str(),&off ) ){
			target=(uint64_t)(base+off);
		}else{
			std::map<std::string,void*>::const_iterator it=stubTable.find( r.sym );
			if( it==stubTable.end() ){
				fprintf( stderr,"loader: unresolved symbol %s\n",r.sym.c_str() );
				munmap( mem,mapSize );
				return nullptr;
			}
			target=(uint64_t)it->second;
		}
		if( r.kind==ABS64 ){
			std::memcpy( base+r.offset,&target,8 );
		}else if( r.kind==REL32 ){
			int64_t d=(int64_t)target-(int64_t)(uint64_t)(base+r.offset+4);
			if( d<INT32_MIN || d>INT32_MAX ){
				fprintf( stderr,"loader: REL32 to %s out of range\n",r.sym.c_str() );
				munmap( mem,mapSize );
				return nullptr;
			}
			int32_t d32=(int32_t)d;
			std::memcpy( base+r.offset,&d32,4 );
		}else{
			fprintf( stderr,"loader: ABS32 reloc to %s not valid on x86-64\n",r.sym.c_str() );
			munmap( mem,mapSize );
			return nullptr;
		}
	}

	//mprotect RX.
	//Text pages RX; data pages (from textSize()) stay RW.
	int textBytes=mod.textSize();
	textBytes=((textBytes+pageSize-1)/pageSize)*pageSize;
	if( textBytes>0 && mprotect( mem,textBytes,PROT_READ|PROT_EXEC )!=0 ){
		fprintf( stderr,"loader: mprotect failed\n" );
		munmap( mem,mapSize );
		return nullptr;
	}

	m_mem=mem;
	m_size=mapSize;
	return mem;
}

void *X64Loader::symbol( const X64Module &mod,const char *sym )const{
	int off;
	if( !m_mem || !mod.findSymbol( sym,&off ) ) return nullptr;
	return (uint8_t*)m_mem+off;
}

void X64Loader::free(){
	if( m_mem ){
		munmap( m_mem,m_size );
		m_mem=nullptr;
		m_size=0;
	}
}

//---- serialization (stand-alone executables) ----
//All integers little-endian 32-bit. Layout:
//  u32 textSize (-1 = none), u32 codeSize, code bytes,
//  u32 nsyms,   { u32 nameLen, name, i32 offset }...
//  u32 nrelocs, { i32 offset, u32 kind, u32 nameLen, name }...
static void put32( std::string &o,uint32_t v ){ o.append( (const char*)&v,4 ); }
static void putStr( std::string &o,const std::string &s ){ put32( o,(uint32_t)s.size() );o+=s; }

std::string X64Module::serialize()const{
	std::string o;
	put32( o,(uint32_t)m_textSize );
	put32( o,(uint32_t)m_code.size() );
	if( m_code.size() ) o.append( (const char*)&m_code[0],m_code.size() );
	put32( o,(uint32_t)m_symbols.size() );
	for( std::map<std::string,int>::const_iterator it=m_symbols.begin();it!=m_symbols.end();++it ){
		putStr( o,it->first );
		put32( o,(uint32_t)it->second );
	}
	put32( o,(uint32_t)m_relocs.size() );
	for( size_t k=0;k<m_relocs.size();++k ){
		put32( o,(uint32_t)m_relocs[k].offset );
		put32( o,(uint32_t)m_relocs[k].kind );
		putStr( o,m_relocs[k].sym );
	}
	return o;
}

namespace{
struct Reader{
	const uint8_t *p;size_t left;bool ok;
	Reader( const void *d,size_t n ):p((const uint8_t*)d),left(n),ok(true){}
	uint32_t u32(){
		uint32_t v=0;
		if( left<4 ){ ok=false;return 0; }
		memcpy( &v,p,4 );p+=4;left-=4;return v;
	}
	bool bytes( std::string &s,uint32_t n ){
		if( left<n ){ ok=false;return false; }
		s.assign( (const char*)p,n );p+=n;left-=n;return true;
	}
	std::string str(){
		std::string s;
		uint32_t n=u32();
		if( ok && n<=left ) bytes( s,n ); else ok=false;
		return s;
	}
};
}

bool X64Module::deserialize( const void *data,size_t size,X64Module &mod ){
	Reader r( data,size );
	mod.m_code.clear();mod.m_symbols.clear();mod.m_relocs.clear();mod.m_textSize=-1;
	int textSize=(int)r.u32();
	uint32_t codeSize=r.u32();
	std::string code;
	if( !r.ok || !r.bytes( code,codeSize ) ){ return false; }
	uint32_t nsyms=r.u32();
	std::map<std::string,int> syms;
	for( uint32_t k=0;r.ok && k<nsyms;++k ){
		std::string name=r.str();
		int off=(int)r.u32();
		if( r.ok ) syms[name]=off;
	}
	uint32_t nrel=r.u32();
	std::vector<Reloc> rels;
	for( uint32_t k=0;r.ok && k<nrel;++k ){
		Reloc rl;
		rl.offset=(int)r.u32();
		uint32_t kind=r.u32();
		rl.sym=r.str();
		if( !r.ok || kind>ABS64 || rl.offset<0 || (uint32_t)rl.offset+4>codeSize ) return false;
		rl.kind=(RelocKind)kind;
		rels.push_back( rl );
	}
	if( !r.ok || r.left!=0 ) return false;
	mod.m_code.assign( code.begin(),code.end() );
	mod.m_textSize=textSize;
	mod.m_symbols=syms;
	mod.m_relocs=rels;
	return true;
}
