//x64 integer tile tests (commit 5, x64-plan.md section 5).
//Each test builds TNode trees by hand, runs them through Codegen_x64
//(enter / code / label / leave), links the result into an X64Module,
//loads it with X64Loader and calls it.

#include <cstdio>
#include <cstdint>
#include <csetjmp>
#include <algorithm>
#include "harness.h"
#include "../../compiler/codegen_x64/codegen_x64.h"

using namespace asmjit;

static int tests_run=0;
static int tests_failed=0;

#define CHECK(cond) do{ \
	++tests_run; \
	if( !(cond) ){ \
		++tests_failed; \
		printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond); \
	} \
}while(0)

#define CHECK_EQ(a,b) do{ \
	++tests_run; \
	long long _a=(long long)(a),_b=(long long)(b); \
	if( _a!=_b ){ \
		++tests_failed; \
		printf("FAIL %s:%d: %s == %lld, expected %lld\n",__FILE__,__LINE__,#a,_a,_b); \
	} \
}while(0)

///////////////////
// TNode helpers //
///////////////////

static TNode *K( int n ){ return new TNode( IR_CONST,0,0,n ); }
static TNode *op( int o,TNode *l,TNode *r=0 ){ return new TNode( o,l,r ); }
static TNode *global( const char *s ){ return new TNode( IR_GLOBAL,0,0,string( s ) ); }
static TNode *mem( TNode *a,bool wide=false ){ TNode *t=new TNode( IR_MEM,a ); t->wide=wide;return t; }
static TNode *move( TNode *v,TNode *m ){ TNode *t=new TNode( IR_MOVE,v,m ); t->wide=m->wide;return t; }
static TNode *seq( TNode *a,TNode *b ){ return new TNode( IR_SEQ,a,b ); }
static TNode *jump( int o,TNode *l,const char *s,TNode *r=0 ){ return new TNode( o,l,r,string( s ) ); }
static TNode *ret( TNode *v ){ return new TNode( IR_RETURN,v,0,string( "_ret" ) ); }

//Locals: slot k of the function under test.
static int g_slot[8];
static TNode *local( int k ){ return new TNode( IR_LOCAL,0,0,g_slot[k] ); }
static TNode *lv( int k,bool wide=false ){ return mem( local( k ),wide ); }

/////////////////////
// Build and load  //
/////////////////////

//Stub symbols (runtime functions / data) visible to generated code.
static int g_int;
static int64_t g_wide;
static int g_array[8]={10,11,12,13,14,15,16,17};
static int *g_arrp;
static jmp_buf g_jb;
static int g_boundsHits;
static void stub_bounds(){ ++g_boundsHits;longjmp( g_jb,1 ); }

static std::map<std::string,void*> stubs(){
	std::map<std::string,void*> s;
	s["g_int"]=(void*)&g_int;
	s["g_wide"]=(void*)&g_wide;
	s["g_arrp"]=(void*)&g_arrp;
	s["__bbArrayBoundsEx"]=(void*)&stub_bounds;
	return s;
}

struct Fn{
	CodeHolder holder;
	Codegen_x64 *cg;
	X64Module mod;
	X64Loader loader;
	void *entry;
	bool built;

	Fn():cg(0),entry(0),built(false){
		holder.init( Environment::host() );
		cg=new Codegen_x64( &holder,false );
		for( int k=0;k<8;++k ) g_slot[k]=cg->localOffset( k );
		cg->enter( "_f",8*8 );
	}
	~Fn(){ loader.free();delete cg; }

	void code( TNode *t ){ cg->code( t ); }

	//Finish the function (return label + epilogue), link and load.
	bool finish( TNode *cleanup=0 ){
		cg->label( "_ret" );
		cg->leave( cleanup,0 );
		built=cg->ok();
		if( !built ) return false;

		//Copy the text section, defining each bound label at its offset.
		CodeBuffer &buf=holder.text_section()->buffer();
		std::vector<std::pair<string,int> > syms=cg->symbols();
		std::vector<std::pair<int,string> > byOff;
		for( size_t i=0;i<syms.size();++i ) byOff.push_back( std::make_pair( syms[i].second,syms[i].first ) );
		std::sort( byOff.begin(),byOff.end() );
		int at=0;
		for( size_t i=0;i<byOff.size();++i ){
			mod.emitBytes( buf.data()+at,byOff[i].first-at );
			at=byOff[i].first;
			mod.addSymbol( byOff[i].second.c_str() );
		}
		mod.emitBytes( buf.data()+at,(int)buf.size()-at );

		const std::vector<Codegen_x64::Fixup> &fx=cg->fixups();
		for( size_t i=0;i<fx.size();++i ){
			RelocKind k=fx[i].kind==Codegen_x64::FIX_ABS64 ? ABS64 :
				(fx[i].kind==Codegen_x64::FIX_REL32 ? REL32 : ABS32);
			mod.addReloc( fx[i].sym.c_str(),fx[i].offset,k );
		}
		loader.load( mod,stubs() );
		entry=loader.symbol( mod,"_f" );
		return entry!=0;
	}

	int call32(){ return entry ? ((int(*)())entry)() : -12345678; }
	int64_t call64(){ return entry ? ((int64_t(*)())entry)() : -12345678; }
};

//Single expression: return it.
static int eval32( TNode *e ){
	Fn f;
	f.code( ret( e ) );
	CHECK( f.finish() );
	return f.call32();
}

//Two locals a,b initialised, then return expr.
static int evalAB( int a,int b,TNode *e ){
	Fn f;
	f.code( move( K( a ),lv( 0 ) ) );
	f.code( move( K( b ),lv( 1 ) ) );
	f.code( ret( e ) );
	CHECK( f.finish() );
	return f.call32();
}

///////////
// Tests //
///////////

static void test_const(){
	CHECK_EQ( eval32( K( 42 ) ),42 );
	CHECK_EQ( eval32( K( -5 ) ),-5 );
	CHECK_EQ( eval32( K( 0x7fffffff ) ),0x7fffffff );
}

static void test_arith(){
	//register/register
	CHECK_EQ( evalAB( 7,5,op( IR_ADD,lv( 0 ),lv( 1 ) ) ),12 );
	CHECK_EQ( evalAB( 7,5,op( IR_SUB,lv( 0 ),lv( 1 ) ) ),2 );
	CHECK_EQ( evalAB( 7,-5,op( IR_MUL,lv( 0 ),lv( 1 ) ) ),-35 );
	CHECK_EQ( evalAB( 17,5,op( IR_DIV,lv( 0 ),lv( 1 ) ) ),3 );
	CHECK_EQ( evalAB( -17,5,op( IR_DIV,lv( 0 ),lv( 1 ) ) ),-3 );
	//right constant
	CHECK_EQ( evalAB( 7,0,op( IR_ADD,lv( 0 ),K( 100 ) ) ),107 );
	CHECK_EQ( evalAB( 7,0,op( IR_SUB,lv( 0 ),K( 100 ) ) ),-93 );
	CHECK_EQ( evalAB( 7,0,op( IR_MUL,lv( 0 ),K( 3 ) ) ),21 );
	CHECK_EQ( evalAB( 7,0,op( IR_DIV,lv( 0 ),K( 3 ) ) ),2 );
	//power-of-two: MUL -> shl, DIV -> sar (rounds towards -inf, as x86 backend)
	CHECK_EQ( evalAB( 7,0,op( IR_MUL,lv( 0 ),K( 8 ) ) ),56 );
	CHECK_EQ( evalAB( 7,0,op( IR_MUL,K( 4 ),lv( 0 ) ) ),28 );
	CHECK_EQ( evalAB( 17,0,op( IR_DIV,lv( 0 ),K( 4 ) ) ),4 );
	CHECK_EQ( evalAB( -7,0,op( IR_DIV,lv( 0 ),K( 4 ) ) ),-2 );
	//32-bit wraparound
	CHECK_EQ( evalAB( 0x7fffffff,1,op( IR_ADD,lv( 0 ),lv( 1 ) ) ),(int)0x80000000 );
	//nested: (a+b)*(a-b)
	CHECK_EQ( evalAB( 9,4,op( IR_MUL,op( IR_ADD,lv( 0 ),lv( 1 ) ),op( IR_SUB,lv( 0 ),lv( 1 ) ) ) ),65 );
	CHECK_EQ( evalAB( 9,0,op( IR_NEG,lv( 0 ) ) ),-9 );
}

static void test_logic(){
	CHECK_EQ( evalAB( 0xf0f0,0x0ff0,op( IR_AND,lv( 0 ),lv( 1 ) ) ),0x00f0 );
	CHECK_EQ( evalAB( 0xf0f0,0x0ff0,op( IR_OR,lv( 0 ),lv( 1 ) ) ),0xfff0 );
	CHECK_EQ( evalAB( 0xf0f0,0x0ff0,op( IR_XOR,lv( 0 ),lv( 1 ) ) ),0xff00 );
	CHECK_EQ( evalAB( 0xf0f0,0,op( IR_AND,lv( 0 ),K( 0xff ) ) ),0xf0 );
	CHECK_EQ( evalAB( 3,4,op( IR_SHL,lv( 0 ),lv( 1 ) ) ),48 );
	CHECK_EQ( evalAB( 3,0,op( IR_SHL,lv( 0 ),K( 4 ) ) ),48 );
	CHECK_EQ( evalAB( -16,2,op( IR_SHR,lv( 0 ),lv( 1 ) ) ),(int)(0xfffffff0u>>2) );
	CHECK_EQ( evalAB( -16,2,op( IR_SAR,lv( 0 ),lv( 1 ) ) ),-4 );
	CHECK_EQ( evalAB( -16,0,op( IR_SAR,lv( 0 ),K( 2 ) ) ),-4 );
}

static void test_relops(){
	struct{ int op,a,b,want; } c[]={
		{IR_SETEQ,3,3,1},{IR_SETEQ,3,4,0},
		{IR_SETNE,3,3,0},{IR_SETNE,3,4,1},
		{IR_SETLT,-1,1,1},{IR_SETLT,1,-1,0},
		{IR_SETGT,1,-1,1},{IR_SETGT,-1,1,0},
		{IR_SETLE,2,2,1},{IR_SETLE,3,2,0},
		{IR_SETGE,2,2,1},{IR_SETGE,-3,2,0},
	};
	for( size_t i=0;i<sizeof(c)/sizeof(c[0]);++i ){
		CHECK_EQ( evalAB( c[i].a,c[i].b,op( c[i].op,lv( 0 ),lv( 1 ) ) ),c[i].want );
		CHECK_EQ( evalAB( c[i].a,0,op( c[i].op,lv( 0 ),K( c[i].b ) ) ),c[i].want );
	}
}

//if( cond ) return 1 else return 0, via JUMPT / JUMPF.
static int branch( int o,int a,int b,TNode *cond ){
	Fn f;
	f.code( move( K( a ),lv( 0 ) ) );
	f.code( move( K( b ),lv( 1 ) ) );
	f.code( jump( o,cond,"_yes" ) );
	f.code( ret( K( 0 ) ) );
	f.cg->label( "_yes" );
	f.code( ret( K( 1 ) ) );
	CHECK( f.finish() );
	return f.call32();
}

static void test_jumps(){
	CHECK_EQ( branch( IR_JUMPT,1,2,op( IR_SETLT,lv( 0 ),lv( 1 ) ) ),1 );
	CHECK_EQ( branch( IR_JUMPT,3,2,op( IR_SETLT,lv( 0 ),lv( 1 ) ) ),0 );
	CHECK_EQ( branch( IR_JUMPF,1,2,op( IR_SETLT,lv( 0 ),lv( 1 ) ) ),0 );
	CHECK_EQ( branch( IR_JUMPF,3,2,op( IR_SETLT,lv( 0 ),lv( 1 ) ) ),1 );
	//non-relop condition: test value
	CHECK_EQ( branch( IR_JUMPT,5,0,lv( 0 ) ),1 );
	CHECK_EQ( branch( IR_JUMPT,0,0,lv( 0 ) ),0 );
	CHECK_EQ( branch( IR_JUMPF,0,0,lv( 0 ) ),1 );
	CHECK_EQ( branch( IR_JUMPF,5,0,lv( 0 ) ),0 );

	//loop: s=0; i=1; top: s=s+i; i=i+1; if i<=10 goto top; return s
	Fn f;
	f.code( move( K( 0 ),lv( 0 ) ) );
	f.code( move( K( 1 ),lv( 1 ) ) );
	f.cg->label( "_top" );
	f.code( move( op( IR_ADD,lv( 0 ),lv( 1 ) ),lv( 0 ) ) );
	f.code( move( op( IR_ADD,lv( 1 ),K( 1 ) ),lv( 1 ) ) );
	f.code( jump( IR_JUMPT,op( IR_SETLE,lv( 1 ),K( 10 ) ),"_top" ) );
	f.code( jump( IR_JUMP,0,"_done" ) );
	f.code( ret( K( -1 ) ) );
	f.cg->label( "_done" );
	f.code( ret( lv( 0 ) ) );
	CHECK( f.finish() );
	CHECK_EQ( f.call32(),55 );
}

static void test_memory(){
	//chained MOVE: a=b=7, then a+b
	{
		Fn f;
		f.code( move( move( K( 7 ),lv( 1 ) ),lv( 0 ) ) );
		f.code( ret( op( IR_ADD,lv( 0 ),lv( 1 ) ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call32(),14 );
	}
	//64-bit local: store a wide address, read back through it
	{
		Fn f;
		f.code( move( K( 1234 ),lv( 0 ) ) );
		f.code( move( local( 0 ),lv( 1,true ) ) );		//p=&a (wide)
		f.code( move( K( 99 ),mem( lv( 1,true ) ) ) );	//*p=99
		f.code( ret( lv( 0 ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call32(),99 );
	}
	//narrow store must not clobber the upper half of a slot
	{
		Fn f;
		f.code( move( K( -1 ),lv( 0,true ) ) );			//slot = 0xffffffffffffffff
		f.code( move( K( 0 ),lv( 0 ) ) );				//low 32 = 0
		f.code( ret( lv( 0,true ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call64(),(int64_t)0xffffffff00000000LL );
	}
	//narrow value into wide slot is sign-extended
	{
		Fn f;
		f.code( move( K( -3 ),lv( 0 ) ) );
		f.code( move( lv( 0 ),lv( 1,true ) ) );
		f.code( ret( lv( 1,true ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call64(),-3 );
	}
	//SEQ as an expression
	CHECK_EQ( eval32( seq( move( K( 5 ),lv( 0 ) ),op( IR_ADD,lv( 0 ),K( 1 ) ) ) ),6 );
}

static void test_globals(){
	g_int=0;
	{
		Fn f;
		f.code( move( K( 77 ),mem( global( "g_int" ) ) ) );
		f.code( ret( op( IR_ADD,mem( global( "g_int" ) ),K( 1 ) ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call32(),78 );
		CHECK_EQ( g_int,77 );
	}
	//wide global: 64-bit store and load
	g_wide=0;
	{
		Fn f;
		f.code( move( global( "g_int" ),mem( global( "g_wide" ),true ) ) );
		f.code( ret( mem( global( "g_wide" ),true ) ) );
		CHECK( f.finish() );
		CHECK( f.call64()==(int64_t)(intptr_t)&g_int );
		CHECK( g_wide==(int64_t)(intptr_t)&g_int );
	}
}

//The x64 point of all this: a 64-bit pointer plus a 32-bit int index.
static void test_wide_index(){
	g_arrp=&g_array[4];
	struct{ int idx,want; } c[]={ {0,14},{3,17},{-1,13},{-4,10} };
	for( size_t i=0;i<sizeof(c)/sizeof(c[0]);++i ){
		Fn f;
		f.code( move( K( c[i].idx ),lv( 0 ) ) );
		TNode *addr=op( IR_ADD,mem( global( "g_arrp" ),true ),op( IR_MUL,lv( 0 ),K( 4 ) ) );
		f.code( ret( mem( addr ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call32(),c[i].want );
	}
	//store through pointer+index, int on the left
	{
		Fn f;
		f.code( move( K( 2 ),lv( 0 ) ) );
		TNode *addr=op( IR_ADD,op( IR_MUL,lv( 0 ),K( 4 ) ),mem( global( "g_arrp" ),true ) );
		f.code( move( K( 555 ),mem( addr ) ) );
		f.code( ret( K( 0 ) ) );
		CHECK( f.finish() );
		f.call32();
		CHECK_EQ( g_array[6],555 );
		g_array[6]=16;
	}
	//wide compare: pointer equality
	{
		Fn f;
		f.code( ret( op( IR_SETEQ,mem( global( "g_arrp" ),true ),mem( global( "g_arrp" ),true ) ) ) );
		CHECK( f.finish() );
		CHECK_EQ( f.call32(),1 );
	}
	//return a wide value
	{
		Fn f;
		f.code( ret( mem( global( "g_arrp" ),true ) ) );
		CHECK( f.finish() );
		CHECK( f.call64()==(int64_t)(intptr_t)g_arrp );
	}
}

//JUMPGE: unsigned bounds check jumping to an external runtime routine.
static int bounded( int idx ){
	Fn f;
	f.code( move( K( idx ),lv( 0 ) ) );
	f.code( jump( IR_JUMPGE,lv( 0 ),"__bbArrayBoundsEx",K( 4 ) ) );
	f.code( ret( op( IR_ADD,lv( 0 ),K( 100 ) ) ) );
	CHECK( f.finish() );
	if( setjmp( g_jb ) ) return -1;
	return f.call32();
}

static void test_jumpge(){
	g_boundsHits=0;
	CHECK_EQ( bounded( 0 ),100 );
	CHECK_EQ( bounded( 3 ),103 );
	CHECK_EQ( g_boundsHits,0 );
	CHECK_EQ( bounded( 4 ),-1 );
	CHECK_EQ( bounded( -1 ),-1 );		//unsigned compare
	CHECK_EQ( g_boundsHits,2 );
}

static void test_cleanup(){
	//leave() cleanup runs after the return label and must preserve rax
	g_int=0;
	Fn f;
	f.code( ret( K( 5 ) ) );
	CHECK( f.finish( move( K( 99 ),mem( global( "g_int" ) ) ) ) );
	CHECK_EQ( f.call32(),5 );
	CHECK_EQ( g_int,99 );
}

static void test_errors(){
	//jump to a label that is never bound
	{
		Fn f;
		f.code( jump( IR_JUMP,0,"_nowhere" ) );
		CHECK( !f.finish() );
	}
	//unsupported op (floats are commit 7)
	{
		Fn f;
		f.code( op( IR_FADD,K( 1 ),K( 2 ) ) );
		CHECK( !f.finish() );
	}
}

int main(){
	test_const();
	test_arith();
	test_logic();
	test_relops();
	test_jumps();
	test_memory();
	test_globals();
	test_wide_index();
	test_jumpge();
	test_cleanup();
	test_errors();
	printf("test_tiles_x64: %d checks, %d failed\n",tests_run,tests_failed);
	return tests_failed?1:0;
}
