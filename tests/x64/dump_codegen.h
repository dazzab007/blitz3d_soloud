
//IR dump backend: implements the Codegen interface and prints TNode trees
//as indented text instead of x86-32 assembly. Used by bbdump to compare the
//front end's output against the x86 backend (commit 3a, x64-plan.md).
//The tree structure is backend-independent: ops, order, iconst/sconst and
//labels/data are exactly what Codegen_x86 would munch.

#ifndef DUMP_CODEGEN_H
#define DUMP_CODEGEN_H

#include "../../compiler/codegen.h"

class DumpCodegen : public Codegen{
public:
	DumpCodegen( ostream &out,bool debug );

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
	void printNode( TNode *t,int depth );
};

#endif
