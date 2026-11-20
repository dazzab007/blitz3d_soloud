
#ifndef CODEGEN_H
#define CODEGEN_H

#include "std.h"

enum{
	IR_JUMP,IR_JUMPT,IR_JUMPF,IR_JUMPGE,

	IR_SEQ,IR_MOVE,IR_MEM,IR_LOCAL,IR_GLOBAL,IR_ARG,IR_CONST,

	IR_JSR,IR_RET,IR_AND,IR_OR,IR_XOR,IR_SHL,IR_SHR,IR_SAR,

	IR_CALL,IR_RETURN,IR_CAST,
	IR_NEG,IR_ADD,IR_SUB,IR_MUL,IR_DIV,
	IR_SETEQ,IR_SETNE,IR_SETLT,IR_SETGT,IR_SETLE,IR_SETGE,

	IR_FCALL,IR_FRETURN,IR_FCAST,
	IR_FNEG,IR_FADD,IR_FSUB,IR_FMUL,IR_FDIV,
	IR_FSETEQ,IR_FSETNE,IR_FSETLT,IR_FSETGT,IR_FSETLE,IR_FSETGE,
};

struct TNode{

	int op;				//opcode
	TNode *l,*r;		//args
	int iconst;			//for CONST type_int
	string sconst;		//for CONST type_string
	bool wide;			//pointer-width value (x64 only, D4 in x64-plan.md)

	TNode( int op,TNode *l=0,TNode *r=0 ):op(op),l(l),r(r),iconst(0),wide(false){}
	TNode( int op,TNode *l,TNode *r,int i ):op(op),l(l),r(r),iconst(i),wide(false){}
	TNode( int op,TNode *l,TNode *r,const string &s ):op(op),l(l),r(r),iconst(0),sconst(s),wide(false){}
	~TNode(){ delete l;delete r; }

	void log();
};

class Codegen{
public:
	ostream &out;
	bool debug;
	Codegen( ostream &out,bool debug ):out( out ),debug( debug ){}

	virtual void enter( const string &l,int frameSize )=0;
	//frame layout: byte offset of parameter k (stack slot, read via [esp+off] on x86-32)
	//and local k (relative to the frame base register). Backends must keep these
	//consistent with the addressing they emit for IR_ARG / IR_LOCAL.
	virtual int argOffset( int k )=0;
	virtual int localOffset( int k )=0;
	virtual void code( TNode *code )=0;
	virtual void leave( TNode *cleanup,int pop_sz )=0;
	virtual void label( const string &l )=0;
	virtual void i_data( int i,const string &l="" )=0;
	virtual void s_data( const string &s,const string &l="" )=0;
	virtual void p_data( const string &p,const string &l="" )=0;
	virtual void align_data( int n )=0;
	//Pointer-width data hooks (x64). The BB runtime structs (BBObj, BBObjType,
	//BBData, BBArray) are packed, with 8-byte pointers on x86-64. The defaults
	//forward to i_data so the x86-32 output is unchanged.
	//null_data: a null pointer field (4 bytes on x86-32, 8 on x86-64).
	virtual void null_data( const string &l="" ){ i_data( 0,l ); }
	//field_data: an int stored in a BBField union (4 bytes on x86-32, 8 on x86-64).
	virtual void field_data( int i,const string &l="" ){ i_data( i,l ); }
	virtual void flush()=0;
};

#endif