//x64 backend skeleton (commit 4, x64-plan.md section 5).
//The Codegen interface is implemented; code() and the data methods are
//no-ops until commits 5+ add the tile implementation.

#include "codegen_x64.h"
#include <sstream>

//The base Codegen class requires an ostream& (used by the x86 backend to
//emit text). The x64 backend emits machine code via asmjit and does not use
//it, so we pass a static null stream.
static std::ostringstream s_nullStream;

Codegen_x64::Codegen_x64( asmjit::CodeHolder *holder,bool debug )
	:Codegen( s_nullStream,debug ),m_holder(holder),m_asm(holder){}

void Codegen_x64::enter( const string &l,int frameSize ){
	//label + prologue: commit 5+
}

int Codegen_x64::argOffset( int k ){
	//System V x86-64: first 6 args in registers; stack args at [rsp+16+k*BB_SLOT].
	//The real layout is set up in enter() (commit 5+). For now return 0.
	return 0;
}

int Codegen_x64::localOffset( int k ){
	//Locals are relative to rbp (frame base register) on x86-64.
	//The real layout is set up in enter() (commit 5+). For now return 0.
	return 0;
}

void Codegen_x64::code( TNode *code ){
	//commit 5+: tile implementation
}

void Codegen_x64::leave( TNode *cleanup,int pop_sz ){
	//commit 5+
}

void Codegen_x64::label( const string &l ){
	//commit 5+
}

void Codegen_x64::i_data( int i,const string &l ){
	//commit 8
}

void Codegen_x64::s_data( const string &s,const string &l ){
	//commit 8
}

void Codegen_x64::p_data( const string &p,const string &l ){
	//commit 8
}

void Codegen_x64::align_data( int n ){
	//commit 8
}

void Codegen_x64::flush(){
	//commit 5+
}
