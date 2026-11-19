//x64 backend (x64-plan.md section 5).
//commit 5: integer tiles. A simple stack evaluator: every expression leaves
//its result in rax (eax when 32-bit); binary ops evaluate the right operand,
//push it, evaluate the left operand into rax and pop the right into rcx.
//Only rax/rcx/rdx are used, all caller-saved, so no registers need saving.
//
//Widths (x64-plan.md section 7): MEM/CALL/MOVE carry the front end's wide
//tag; LOCAL/GLOBAL/ARG are addresses (wide); ADD/SUB/logic/MUL/DIV are wide
//if either operand is; the narrow side is sign-extended. A store's width is
//the destination MEM's wide flag.
//
//Frame: push rbp; mov rbp,rsp; sub rsp,align16(frameSize). Locals at
//[rbp-8*(k+1)]. Params at [rbp+16+8*k] for now; commit 6 decides the
//Blitz->Blitz call convention and parameter homing.

#include "codegen_x64.h"
#include "../bb_abi.h"
#include <sstream>
#include <cstdio>

using namespace asmjit;
using namespace asmjit::x86;

//The base Codegen class requires an ostream& (used by the x86 backend to
//emit text). The x64 backend emits machine code via asmjit and does not use
//it, so we pass a static null stream.
static std::ostringstream s_nullStream;

namespace{
struct CountingErrorHandler : public ErrorHandler{
	int *count;
	explicit CountingErrorHandler( int *c ):count(c){}
	void handle_error( Error err,const char *message,BaseEmitter *origin ) override{
		fprintf( stderr,"codegen_x64: asmjit error: %s\n",message );
		++*count;
	}
};
}

Codegen_x64::Codegen_x64( CodeHolder *holder,bool debug )
	:Codegen( s_nullStream,debug ),m_holder(holder),m_asm(holder),
	m_inCode(false),m_depth(0),m_errors(0){
	m_errorHandler=new CountingErrorHandler( &m_errors );
	m_asm.set_error_handler( m_errorHandler );
}

Codegen_x64::~Codegen_x64(){
	m_asm.reset_error_handler();
	delete m_errorHandler;
}

void Codegen_x64::fail(){
	++m_errors;
}

/////////////
// Helpers //
/////////////

Label Codegen_x64::getLabel( const string &l ){
	std::map<string,Label>::iterator it=m_labels.find( l );
	if( it!=m_labels.end() ) return it->second;
	Label lab=m_asm.new_label();
	m_labels[l]=lab;
	return lab;
}

void Codegen_x64::bindLabel( const string &l ){
	Label lab=getLabel( l );
	if( m_holder->is_label_bound( lab ) ){
		fprintf( stderr,"codegen_x64: label bound twice: %s\n",l.c_str() );
		fail();return;
	}
	m_asm.bind( lab );
}

//mov rax,imm64 with an ABS64 fixup. The placeholder is large so asmjit always
//picks the 10-byte movabs form (imm64 = last 8 bytes).
void Codegen_x64::movAbs64( const string &sym ){
	m_asm.mov( rax,Imm( (int64_t)0x1122334455667788LL ) );
	Fixup f;
	f.offset=(int)m_asm.offset()-8;
	f.sym=sym;
	f.kind=FIX_ABS64;
	m_fixups.push_back( f );
}

void Codegen_x64::push(){
	m_asm.push( rax );
	m_depth+=8;
}

void Codegen_x64::pop( const Gp &r ){
	m_asm.pop( r );
	m_depth-=8;
}

static bool isRelop( int op ){
	return op>=IR_SETEQ && op<=IR_SETGE;
}

static CondCode relopCond( int op ){
	switch( op ){
	case IR_SETEQ:return CondCode::kE;
	case IR_SETNE:return CondCode::kNE;
	case IR_SETLT:return CondCode::kL;
	case IR_SETGT:return CondCode::kG;
	case IR_SETLE:return CondCode::kLE;
	default:return CondCode::kGE;
	}
}

//Power-of-two test, same as codegen_x86 getShift (1..2^31).
static bool getShift( int n,int &shift ){
	for( shift=0;shift<32;++shift ){
		if( (int)(1u<<shift)==n ) return true;
	}
	return false;
}

///////////////////////
// Width inference   //
///////////////////////

bool Codegen_x64::isWide( TNode *t ){
	if( !t ) return false;
	switch( t->op ){
	case IR_MEM:case IR_CALL:case IR_MOVE:
		return t->wide;
	case IR_LOCAL:case IR_GLOBAL:case IR_ARG:
		return true;
	case IR_ADD:case IR_SUB:case IR_MUL:case IR_DIV:
	case IR_AND:case IR_OR:case IR_XOR:
		return isWide( t->l ) || isWide( t->r );
	case IR_NEG:case IR_SHL:case IR_SHR:case IR_SAR:
		return isWide( t->l );
	case IR_SEQ:
		return isWide( t->r );
	default:
		return false;
	}
}

////////////////
// Evaluation //
////////////////

void Codegen_x64::evalAs( TNode *t,bool wide ){
	if( wide && t->op==IR_CONST ){
		m_asm.mov( rax,Imm( (int64_t)t->iconst ) );
		return;
	}
	eval( t );
	if( wide && !isWide( t ) ) m_asm.movsxd( rax,eax );
}

//rax=l, rcx=r, both at the given width.
void Codegen_x64::evalBinary( TNode *t,bool wide ){
	evalAs( t->r,wide );
	push();
	evalAs( t->l,wide );
	pop( rcx );
}

//cmp l,r at the wider of the two operand widths.
void Codegen_x64::evalCompare( TNode *t ){
	bool w=isWide( t->l ) || isWide( t->r );
	if( t->r->op==IR_CONST ){
		evalAs( t->l,w );
		if( w ) m_asm.cmp( rax,Imm( t->r->iconst ) );
		else m_asm.cmp( eax,Imm( t->r->iconst ) );
		return;
	}
	evalBinary( t,w );
	if( w ) m_asm.cmp( rax,rcx );
	else m_asm.cmp( eax,ecx );
}

//JUMPT (negate=false) / JUMPF (negate=true).
void Codegen_x64::condJump( TNode *t,bool negate ){
	Label target=getLabel( t->sconst );
	TNode *p=t->l;
	if( isRelop( p->op ) ){
		evalCompare( p );
		CondCode cc=relopCond( p->op );
		m_asm.j( negate ? negate_cond( cc ) : cc,target );
		return;
	}
	eval( p );
	if( isWide( p ) ) m_asm.test( rax,rax );
	else m_asm.test( eax,eax );
	m_asm.j( negate ? CondCode::kZ : CondCode::kNZ,target );
}

void Codegen_x64::stmt( TNode *t ){
	switch( t->op ){
	case IR_SEQ:
		stmt( t->l );
		stmt( t->r );
		return;
	case IR_JUMP:
		m_asm.jmp( getLabel( t->sconst ) );
		return;
	case IR_JUMPT:
		condJump( t,false );
		return;
	case IR_JUMPF:
		condJump( t,true );
		return;
	case IR_JUMPGE:{
		//Unsigned l>=r jumps to a runtime error routine (__bbArrayBoundsEx
		//etc.), which may be anywhere in the address space: invert the test
		//around an absolute jump.
		bool w=isWide( t->l ) || isWide( t->r );
		evalBinary( t,w );
		if( w ) m_asm.cmp( rax,rcx );
		else m_asm.cmp( eax,ecx );
		Label skip=m_asm.new_label();
		m_asm.j( CondCode::kB,skip );
		movAbs64( t->sconst );
		m_asm.jmp( rax );
		m_asm.bind( skip );
		return;
	}
	case IR_RETURN:
		if( t->l ) eval( t->l );
		m_asm.jmp( getLabel( t->sconst ) );
		return;
	default:
		eval( t );
		return;
	}
}

void Codegen_x64::eval( TNode *t ){
	switch( t->op ){

	case IR_CONST:
		m_asm.mov( eax,Imm( t->iconst ) );
		return;

	case IR_LOCAL:
		m_asm.lea( rax,ptr( rbp,t->iconst ) );
		return;

	case IR_GLOBAL:
		movAbs64( t->sconst );
		return;

	case IR_MEM:{
		bool w=t->wide;
		if( t->l->op==IR_LOCAL ){
			if( w ) m_asm.mov( rax,qword_ptr( rbp,t->l->iconst ) );
			else m_asm.mov( eax,dword_ptr( rbp,t->l->iconst ) );
			return;
		}
		evalAs( t->l,true );
		if( w ) m_asm.mov( rax,qword_ptr( rax ) );
		else m_asm.mov( eax,dword_ptr( rax ) );
		return;
	}

	case IR_MOVE:{
		//MUST BE MOVE TO MEM. Yields the stored value (chained MOVEs).
		TNode *dst=t->r;
		if( !dst || dst->op!=IR_MEM ){ fail();return; }
		bool w=dst->wide;
		evalAs( t->l,w );
		if( dst->l->op==IR_LOCAL ){
			if( w ) m_asm.mov( qword_ptr( rbp,dst->l->iconst ),rax );
			else m_asm.mov( dword_ptr( rbp,dst->l->iconst ),eax );
			return;
		}
		push();
		evalAs( dst->l,true );
		m_asm.mov( rcx,rax );
		pop( rax );
		if( w ) m_asm.mov( qword_ptr( rcx ),rax );
		else m_asm.mov( dword_ptr( rcx ),eax );
		return;
	}

	case IR_SEQ:
		stmt( t->l );
		eval( t->r );
		return;

	case IR_NEG:
		eval( t->l );
		if( isWide( t->l ) ) m_asm.neg( rax );
		else m_asm.neg( eax );
		return;

	case IR_ADD:case IR_SUB:case IR_AND:case IR_OR:case IR_XOR:case IR_MUL:{
		bool w=isWide( t );
		Gp a=w ? rax : eax;
		Gp c=w ? rcx : ecx;
		int shift;
		if( t->op==IR_MUL && t->r->op==IR_CONST && getShift( t->r->iconst,shift ) ){
			evalAs( t->l,w );
			m_asm.shl( a,Imm( shift ) );
			return;
		}
		if( t->op==IR_MUL && t->l->op==IR_CONST && getShift( t->l->iconst,shift ) ){
			evalAs( t->r,w );
			m_asm.shl( a,Imm( shift ) );
			return;
		}
		if( t->r->op==IR_CONST ){
			evalAs( t->l,w );
			Imm k( t->r->iconst );
			switch( t->op ){
			case IR_ADD:m_asm.add( a,k );break;
			case IR_SUB:m_asm.sub( a,k );break;
			case IR_AND:m_asm.and_( a,k );break;
			case IR_OR:m_asm.or_( a,k );break;
			case IR_XOR:m_asm.xor_( a,k );break;
			case IR_MUL:m_asm.imul( a,a,k );break;
			}
			return;
		}
		evalBinary( t,w );
		switch( t->op ){
		case IR_ADD:m_asm.add( a,c );break;
		case IR_SUB:m_asm.sub( a,c );break;
		case IR_AND:m_asm.and_( a,c );break;
		case IR_OR:m_asm.or_( a,c );break;
		case IR_XOR:m_asm.xor_( a,c );break;
		case IR_MUL:m_asm.imul( a,c );break;
		}
		return;
	}

	case IR_DIV:{
		bool w=isWide( t );
		Gp a=w ? rax : eax;
		int shift;
		//Divide by a power-of-two constant is an arithmetic shift, as in
		//codegen_x86 (Blitz semantics: rounds towards -infinity).
		if( t->r->op==IR_CONST && getShift( t->r->iconst,shift ) ){
			evalAs( t->l,w );
			m_asm.sar( a,Imm( shift ) );
			return;
		}
		evalBinary( t,w );
		if( w ){
			m_asm.cqo( rdx,rax );
			m_asm.idiv( rdx,rax,rcx );
		}else{
			m_asm.cdq( edx,eax );
			m_asm.idiv( edx,eax,ecx );
		}
		return;
	}

	case IR_SHL:case IR_SHR:case IR_SAR:{
		bool w=isWide( t->l );
		Gp a=w ? rax : eax;
		if( t->r->op==IR_CONST ){
			eval( t->l );
			Imm k( t->r->iconst & (w ? 63 : 31) );
			switch( t->op ){
			case IR_SHL:m_asm.shl( a,k );break;
			case IR_SHR:m_asm.shr( a,k );break;
			case IR_SAR:m_asm.sar( a,k );break;
			}
			return;
		}
		eval( t->r );
		push();
		eval( t->l );
		pop( rcx );
		switch( t->op ){
		case IR_SHL:m_asm.shl( a,cl );break;
		case IR_SHR:m_asm.shr( a,cl );break;
		case IR_SAR:m_asm.sar( a,cl );break;
		}
		return;
	}

	case IR_SETEQ:case IR_SETNE:case IR_SETLT:case IR_SETGT:case IR_SETLE:case IR_SETGE:
		evalCompare( t );
		m_asm.set( relopCond( t->op ),al );
		m_asm.movzx( eax,al );
		return;

	case IR_JUMP:case IR_JUMPT:case IR_JUMPF:case IR_JUMPGE:case IR_RETURN:
		stmt( t );
		return;

	default:
		fprintf( stderr,"codegen_x64: unsupported IR op %d\n",t->op );
		fail();
		return;
	}
}

///////////////////////
// Codegen interface //
///////////////////////

void Codegen_x64::enter( const string &l,int frameSize ){
	m_inCode=true;
	m_depth=0;
	if( l.size() ) bindLabel( l );
	m_asm.push( rbp );
	m_asm.mov( rbp,rsp );
	int sz=(frameSize+15)&~15;
	if( sz ) m_asm.sub( rsp,Imm( sz ) );
}

int Codegen_x64::argOffset( int k ){
	return 16+k*BB_SLOT;	//saved rbp + return address; commit 6 may change this
}

int Codegen_x64::localOffset( int k ){
	return -BB_SLOT*(k+1);
}

void Codegen_x64::code( TNode *t ){
	stmt( t );
	if( m_depth!=0 ){
		fprintf( stderr,"codegen_x64: unbalanced temporaries (%d)\n",m_depth );
		fail();
		m_depth=0;
	}
	delete t;
}

void Codegen_x64::leave( TNode *cleanup,int pop_sz ){
	if( cleanup ){
		//rax holds the return value; keep it (and 16-byte alignment).
		m_asm.push( rax );
		m_asm.push( rax );
		m_depth+=16;
		stmt( cleanup );
		m_asm.pop( rax );
		m_asm.pop( rax );
		m_depth-=16;
		delete cleanup;
	}
	m_asm.mov( rsp,rbp );
	m_asm.pop( rbp );
	m_asm.ret();
	m_inCode=false;
}

void Codegen_x64::label( const string &l ){
	if( m_inCode ) bindLabel( l );
	//data labels: commit 8
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
}

std::vector<std::pair<string,int> > Codegen_x64::symbols()const{
	std::vector<std::pair<string,int> > out;
	std::map<string,Label>::const_iterator it;
	for( it=m_labels.begin();it!=m_labels.end();++it ){
		if( m_holder->is_label_bound( it->second ) ){
			out.push_back( std::make_pair( it->first,(int)m_holder->label_offset( it->second ) ) );
		}
	}
	return out;
}

bool Codegen_x64::ok()const{
	if( m_errors ) return false;
	std::map<string,Label>::const_iterator it;
	for( it=m_labels.begin();it!=m_labels.end();++it ){
		if( !m_holder->is_label_bound( it->second ) ){
			fprintf( stderr,"codegen_x64: label never bound: %s\n",it->first.c_str() );
			return false;
		}
	}
	return true;
}
