//x64 backend skeleton (commit 4, x64-plan.md section 5).
//Implements the Codegen interface using asmjit. The tile implementation
//(commit 5+) will fill in code(); for now it is a no-op so the harness
//can be exercised with hand-built TNode trees.

#ifndef CODEGEN_X64_H
#define CODEGEN_X64_H

#include "../codegen.h"
#include <asmjit/x86.h>

class Codegen_x64 : public Codegen{
public:
	Codegen_x64( asmjit::CodeHolder *holder,bool debug );

	virtual void enter( const string &l,int frameSize );
	virtual int argOffset( int k );
	virtual int localOffset( int k );
	virtual void code( TNode *code );
	virtual void leave( TNode *cleanup,int pop_sz );
	virtual void label( const string &l );
	virtual void i_data( int i,const string &l );
	virtual void s_data( const string &s,const string &l );
	virtual void p_data( const string &p,const string &l );
	virtual void align_data( int n );
	virtual void flush();

private:
	asmjit::CodeHolder *m_holder;
	asmjit::x86::Assembler m_asm;
};

#endif
