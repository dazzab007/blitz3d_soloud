//In-memory Module + mini loader for x64 backend tests (commit 4, x64-plan.md).
//The Module collects machine code bytes, symbols and relocations. The loader
//mmaps RW memory, copies the code, applies REL32/ABS64 relocs against a table
//of stub function addresses, mprotects RX, and returns a callable pointer.

#ifndef X64_HARNESS_H
#define X64_HARNESS_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

//Relocation kinds (x64-plan.md D7).
enum RelocKind{
	REL32,		//PC-relative 32-bit (existing pcrel=true)
	ABS32,		//Absolute 32-bit (existing pcrel=false)
	ABS64		//Absolute 64-bit (new, for x86-64 runtime calls)
};

struct Reloc{
	int offset;		//byte offset in the code buffer where the reloc applies
	std::string sym;	//symbol name to resolve (owned copy)
	RelocKind kind;
};

//In-memory Module: collects machine code, symbols and relocations.
class X64Module{
public:
	X64Module();
	~X64Module();

	//Emit raw bytes at the current position.
	void emit( uint8_t byte );
	void emit16( uint16_t w );
	void emit32( uint32_t d );
	void emit64( uint64_t q );
	void emitBytes( const void *data,int sz );

	//Current position (byte offset from start).
	int getPC()const{ return (int)m_code.size(); }

	//Define a symbol at the current position.
	bool addSymbol( const char *sym );

	//Add a relocation: patch m_code[offset] with the address of sym.
	//For REL32: writes (sym_addr - (base + offset + 4)) as int32.
	//For ABS64: writes sym_addr as uint64.
	bool addReloc( const char *sym,int offset,RelocKind kind );

	//Look up a symbol's offset in the code buffer. Returns false if not found.
	bool findSymbol( const char *sym,int *offset )const;

	//Relocations recorded so far (applied by X64Loader).
	const std::vector<Reloc> &relocs()const{ return m_relocs; }

	//Access the raw code buffer.
	const uint8_t *code()const{ return m_code.data(); }
	int size()const{ return (int)m_code.size(); }

private:
	std::vector<uint8_t> m_code;
	std::map<std::string,int> m_symbols;	//name -> offset in m_code
	std::vector<Reloc> m_relocs;
};

//Mini loader: maps the module into executable memory and applies relocs.
//The stubTable maps symbol names to their addresses (function pointers).
class X64Loader{
public:
	X64Loader();
	~X64Loader();

	//Load the module: mmap RW, copy code, apply relocs (symbols defined in
	//the module first, then stubTable), mprotect RX. Returns a callable
	//pointer to the entry point (offset 0), or nullptr on failure (unknown
	//symbol, REL32 out of range, ABS32 on x86-64). Call free() when done.
	void *load( const X64Module &mod,const std::map<std::string,void*> &stubTable );

	//Free the mapped memory.
	void free();

	//Address of a module symbol after load(), or nullptr.
	void *symbol( const X64Module &mod,const char *sym )const;

private:
	void *m_mem;		//mapped memory (RW then RX)
	int m_size;			//size in bytes
};

#endif
