//x64 backend (x64-plan.md section 5).
//Implements the Codegen interface using asmjit.
//commit 5: integer tiles - CONST, LOCAL/GLOBAL, MEM/MOVE (32/64-bit),
//ADD/SUB/MUL/DIV, AND/OR/XOR/shifts, NEG, SETxx, JUMP/JUMPT/JUMPF/JUMPGE,
//RETURN, SEQ; rbp frame. Calls, ARG, JSR/RET: commit 6. Floats: commit 7.
//Data: commit 8.

#ifndef CODEGEN_X64_H
#define CODEGEN_X64_H

#include "../codegen.h"
#include <asmjit/x86.h>
#include <map>
#include <vector>

class Codegen_x64 : public Codegen{
public:
	//Relocation the loader must apply (kinds match x64-plan.md D7).
	enum{ FIX_REL32=0,FIX_ABS32=1,FIX_ABS64=2 };
	struct Fixup{
		int offset;			//byte offset of the field in the text section
		string sym;			//symbol (module label or runtime symbol)
		int kind;
	};

	Codegen_x64( asmjit::CodeHolder *holder,bool debug );
	~Codegen_x64();

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

	//Direct access to the assembler (tests).
	asmjit::x86::Assembler &as(){ return m_asm; }

	//Fixups recorded so far (ABS64 for GLOBAL and JUMPGE targets).
	const std::vector<Fixup> &fixups()const{ return m_fixups; }

	//Labels bound in the text section, with their offsets.
	std::vector<std::pair<string,int> > symbols()const;

	//False if any tile failed, a TNode was not supported, or a jump
	//target label was never bound.
	bool ok()const;

	//Pointer-width inference for a TNode tree (D4, x64-plan.md section 7).
	static bool isWide( TNode *t );

private:
	asmjit::CodeHolder *m_holder;
	asmjit::x86::Assembler m_asm;
	std::map<string,asmjit::Label> m_labels;
	std::vector<Fixup> m_fixups;
	bool m_inCode;
	int m_depth;		//bytes pushed for temporaries (commit 6 stack alignment)
	int m_errors;
	asmjit::ErrorHandler *m_errorHandler;

	asmjit::Label getLabel( const string &l );
	void bindLabel( const string &l );
	void movAbs64( const string &sym );
	void push();
	void pop( const asmjit::x86::Gp &r );
	void fail();

	void eval( TNode *t );				//result in rax/eax, width isWide(t)
	void evalAs( TNode *t,bool wide );	//sign-extends to 64 when needed
	void stmt( TNode *t );				//statement context (no result needed)
	void evalBinary( TNode *t,bool wide );	//rax=l, rcx=r
	void evalCompare( TNode *t );		//cmp l,r - flags only
	void condJump( TNode *t,bool negate );
};

#endif
