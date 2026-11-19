//x64 call/frame tests (commit 6, x64-plan.md section 5).
//Blitz->Blitz calls (args in the outgoing area), Blitz->C calls (System V
//registers + stack overflow), stack alignment, nested calls, recursion,
//indirect (userlib-style) calls, int vs wide returns, Gosub.

#include <csetjmp>
#include "x64test.h"

using namespace asmjit;

/////////////////////////
// C functions to call //
/////////////////////////

static int g_int;
static int g_array[8]={10,11,12,13,14,15,16,17};
static int *g_arrp=&g_array[4];
static int g_misaligned;
static jmp_buf g_jb;

//1 if this function was entered with a correctly aligned stack
//(rsp%16==8 at entry, so the frame pointer is 16-aligned).
__attribute__((noinline)) static int c_aligned(){
	return ((uintptr_t)__builtin_frame_address( 0 )&15)==0;
}
__attribute__((noinline)) static void note_alignment(){
	if( !c_aligned() ) ++g_misaligned;
}

static int c_add1( int a ){ note_alignment();return a+1; }
static int c_sum6( int a,int b,int c,int d,int e,int f ){
	note_alignment();
	return a+2*b+3*c+4*d+5*e+6*f;
}
//int params take the low 32 bits of a GP register; h is pointer-width.
static int64_t c_sum8( int a,int b,int c,int d,int e,int f,int g,int64_t h ){
	note_alignment();
	return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h;
}
//7 ints (i6 on the stack) and 9 floats (f8 on the stack), interleaved.
static int c_mixed( int i0,float f0,int i1,float f1,int i2,float f2,int i3,float f3,
	int i4,float f4,int i5,float f5,int i6,float f6,float f7,float f8 ){
	note_alignment();
	int s=i0+2*i1+3*i2+4*i3+5*i4+6*i5+7*i6;
	float f=f0+2*f1+3*f2+4*f3+5*f4+6*f5+7*f6+8*f7+9*f8;
	return s*1000+(int)(f*2);
}
static int c_deref( int *p ){ return *p; }
static int *c_ptr(){ return &g_int; }
static int c_neg1(){ return -1; }
static int c_twice( int a ){ return 2*a; }
static int (*g_fnptr)( int )=c_twice;
static void c_bounds(){ note_alignment();longjmp( g_jb,1 ); }

static std::map<std::string,void*> stubs(){
	std::map<std::string,void*> s;
	s["c_aligned"]=(void*)&c_aligned;
	s["c_add1"]=(void*)&c_add1;
	s["c_sum6"]=(void*)&c_sum6;
	s["c_sum8"]=(void*)&c_sum8;
	s["c_mixed"]=(void*)&c_mixed;
	s["c_deref"]=(void*)&c_deref;
	s["c_ptr"]=(void*)&c_ptr;
	s["c_neg1"]=(void*)&c_neg1;
	s["g_int"]=(void*)&g_int;
	s["g_arrp"]=(void*)&g_arrp;
	s["g_fnptr"]=(void*)&g_fnptr;
	s["__bbArrayBoundsEx"]=(void*)&c_bounds;
	return s;
}

typedef int (*Fn32)();
typedef int64_t (*Fn64)();

//Build _f = return expr (plus any extra functions added by `more`), run it.
static int run32( TNode *e,void (*more)( Prog & )=0 ){
	Prog p;
	p.begin( "_f" );
	p.code( ret( e ) );
	p.end();
	if( more ) more( p );
	bool ok=p.link( stubs() );
	CHECK( ok );
	if( !ok ) return -12345678;
	return ((Fn32)p.sym( "_f" ))();
}

//////////////
// C calls  //
//////////////

static void test_c_calls(){
	g_misaligned=0;
	{
		TNode *a[]={ K( 41 ) };
		CHECK_EQ( run32( callN( global( "c_add1" ),1,a,"Ci" ) ),42 );
	}
	{
		TNode *a[]={ K( 1 ),K( 2 ),K( 3 ),K( 4 ),K( 5 ),K( 6 ) };
		CHECK_EQ( run32( callN( global( "c_sum6" ),6,a,"Ciiiiii" ) ),1+4+9+16+25+36 );
	}
	{
		//8 args: 6 in registers, 2 on the stack; 64-bit values
		Prog p;
		p.begin( "_f" );
		TNode *a[8];
		for( int k=0;k<8;++k ) a[k]=K( k+1 );
		a[7]=mem( global( "g_arrp" ),true );		//a wide arg on the stack
		p.code( ret( callN( global( "c_sum8" ),8,a,"Ciiiiiiii",true,"       W" ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		int64_t want=1+4+9+16+25+36+49+8*(int64_t)(intptr_t)g_arrp;
		CHECK( ((Fn64)p.sym( "_f" ))()==want );
	}
	{
		//interleaved ints and floats with overflow in both classes
		TNode *a[16];
		const char *cls="Cifififififififff";
		int ni=0,nf=0;
		for( int k=0;k<16;++k ){
			if( cls[k+1]=='i' ) a[k]=K( ++ni );
			else a[k]=KF( 0.5f*(++nf) );
		}
		//ints 1..7, floats 0.5..4.5
		int si=1+4+9+16+25+36+49;
		float sf=0;
		for( int k=0;k<9;++k ) sf+=(k+1)*0.5f*(k+1);
		CHECK_EQ( run32( callN( global( "c_mixed" ),16,a,cls ) ),si*1000+(int)(sf*2) );
	}
	{
		//wide return, then a wide arg
		TNode *a[]={ callN( global( "c_ptr" ),0,0,"C",true ) };
		g_int=1234;
		CHECK_EQ( run32( callN( global( "c_deref" ),1,a,"Ci",false,"W" ) ),1234 );
	}
	{
		//narrow int return used as a pointer index: must be sign-extended
		TNode *idx=op( IR_MUL,callN( global( "c_neg1" ),0,0,"C" ),K( 4 ) );
		CHECK_EQ( run32( mem( op( IR_ADD,mem( global( "g_arrp" ),true ),idx ) ) ),13 );
	}
	{
		//indirect call through a wide function pointer (userlib style)
		TNode *a[]={ K( 21 ) };
		CHECK_EQ( run32( callN( mem( global( "g_fnptr" ),true ),1,a,"Ci" ) ),42 );
	}
	CHECK_EQ( g_misaligned,0 );
}

//////////////////
// Alignment    //
//////////////////

static void test_alignment(){
	//direct
	CHECK_EQ( run32( callN( global( "c_aligned" ),0,0,"C" ) ),1 );
	//call evaluated while a temporary is pushed (right operand of ADD)
	CHECK_EQ( run32( op( IR_ADD,callN( global( "c_aligned" ),0,0,"C" ),K( 10 ) ) ),11 );
	CHECK_EQ( run32( op( IR_ADD,K( 10 ),op( IR_ADD,callN( global( "c_aligned" ),0,0,"C" ),
		op( IR_SUB,K( 0 ),op( IR_ADD,callN( global( "c_aligned" ),0,0,"C" ),K( 0 ) ) ) ) ) ),10 );
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( K( 5 ),lv( 0 ) ) );
		TNode *s=op( IR_SUB,lv( 0 ),callN( global( "c_aligned" ),0,0,"C" ) );
		p.code( ret( op( IR_MUL,s,op( IR_ADD,lv( 0 ),callN( global( "c_aligned" ),0,0,"C" ) ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((Fn32)p.sym( "_f" ))(),(5-1)*(5+1) );
	}
	//inside a nested call's argument evaluation (odd area sizes)
	{
		TNode *inner[]={ callN( global( "c_aligned" ),0,0,"C" ) };
		TNode *a[]={ K( 100 ),callN( global( "c_add1" ),1,inner,"Ci" ),K( 0 ),K( 0 ),K( 0 ),K( 0 ) };
		CHECK_EQ( run32( callN( global( "c_sum6" ),6,a,"Ciiiiii" ) ),100+2*2 );
	}
}

/////////////////////
// Blitz functions //
/////////////////////

//_g(a,b) = a - 2*b
static void add_g( Prog &p ){
	p.begin( "_g" );
	p.code( ret( op( IR_SUB,pv( 0 ),op( IR_MUL,pv( 1 ),K( 2 ) ) ) ) );
	p.end();
}

//_h(a..h) = a+2b+...+8h, calls C with its own args to check alignment too
static void add_h( Prog &p ){
	p.begin( "_h" );
	TNode *s=pv( 0 );
	for( int k=1;k<8;++k ) s=op( IR_ADD,s,op( IR_MUL,pv( k ),K( k+1 ) ) );
	p.code( move( s,lv( 0 ) ) );
	p.code( ret( op( IR_ADD,lv( 0 ),op( IR_SUB,callN( global( "c_aligned" ),0,0,"C" ),K( 1 ) ) ) ) );
	p.end();
}

//_fact(n) = n<=1 ? 1 : n*_fact(n-1)
static void add_fact( Prog &p ){
	p.begin( "_fact" );
	p.code( jump( IR_JUMPT,op( IR_SETLE,pv( 0 ),K( 1 ) ),"_fact_one" ) );
	TNode *a[]={ op( IR_SUB,pv( 0 ),K( 1 ) ) };
	p.code( ret( op( IR_MUL,pv( 0 ),callN( global( "_fact" ),1,a,0 ) ) ) );
	p.label( "_fact_one" );
	p.code( ret( K( 1 ) ) );
	p.end();
}

//_wid(p) returns its wide param unchanged
static void add_wid( Prog &p ){
	p.begin( "_wid" );
	p.code( ret( pv( 0,true ) ) );
	p.end();
}

static void add_all( Prog &p ){ add_g( p );add_h( p );add_fact( p );add_wid( p ); }

static void test_blitz_calls(){
	g_misaligned=0;
	{
		TNode *a[]={ K( 10 ),K( 3 ) };
		CHECK_EQ( run32( callN( global( "_g" ),2,a,0 ),add_all ),4 );
	}
	{
		//nested: _g( _g(20,1), _g(5,1) ) = 18 - 2*3 = 12
		TNode *a1[]={ K( 20 ),K( 1 ) };
		TNode *a2[]={ K( 5 ),K( 1 ) };
		TNode *a[]={ callN( global( "_g" ),2,a1,0 ),callN( global( "_g" ),2,a2,0 ) };
		CHECK_EQ( run32( callN( global( "_g" ),2,a,0 ),add_all ),12 );
	}
	{
		TNode *a[8];
		for( int k=0;k<8;++k ) a[k]=K( k+1 );
		CHECK_EQ( run32( callN( global( "_h" ),8,a,0 ),add_all ),1+4+9+16+25+36+49+64 );
	}
	{
		TNode *a[]={ K( 10 ) };
		CHECK_EQ( run32( callN( global( "_fact" ),1,a,0 ),add_all ),3628800 );
	}
	{
		//wide param and wide return through a Blitz function
		Prog p;
		p.begin( "_f" );
		TNode *a[]={ mem( global( "g_arrp" ),true ) };
		p.code( ret( mem( callN( global( "_wid" ),1,a,0,true,"W" ) ) ) );
		p.end();
		add_all( p );
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((Fn32)p.sym( "_f" ))(),14 );
	}
	{
		//Blitz call with a C call as an argument, inside a C call
		TNode *ga[]={ callN( global( "c_neg1" ),0,0,"C" ),K( -2 ) };
		TNode *a[]={ callN( global( "_g" ),2,ga,0 ) };
		CHECK_EQ( run32( callN( global( "c_add1" ),1,a,"Ci" ),add_all ),4 );
	}
	CHECK_EQ( g_misaligned,0 );
}

///////////
// Gosub //
///////////

static void test_gosub(){
	Prog p;
	p.begin( "_f" );
	p.code( move( K( 0 ),lv( 0 ) ) );
	p.code( new TNode( IR_JSR,0,0,string( "_sub" ) ) );
	p.code( new TNode( IR_JSR,0,0,string( "_sub" ) ) );
	p.code( ret( lv( 0 ) ) );
	p.label( "_sub" );
	//lv0 += 10*c_aligned()
	p.code( move( op( IR_ADD,lv( 0 ),op( IR_MUL,callN( global( "c_aligned" ),0,0,"C" ),K( 10 ) ) ),lv( 0 ) ) );
	p.code( new TNode( IR_RET ) );
	p.end();
	CHECK( p.link( stubs() ) );
	CHECK_EQ( ((Fn32)p.sym( "_f" ))(),20 );
}

////////////////////////////
// Bounds check alignment //
////////////////////////////

static int bounded( int idx,bool pushed ){
	Prog p;
	p.begin( "_f" );
	p.code( move( K( idx ),lv( 0 ) ) );
	TNode *chk=seq( jump( IR_JUMPGE,lv( 0 ),"__bbArrayBoundsEx",K( 4 ) ),lv( 0 ) );
	//pushed: the check runs while a temporary is on the stack
	p.code( ret( pushed ? op( IR_ADD,chk,lv( 0 ) ) : op( IR_ADD,chk,K( 0 ) ) ) );
	p.end();
	CHECK( p.link( stubs() ) );
	if( setjmp( g_jb ) ) return -1;
	return ((Fn32)p.sym( "_f" ))();
}

static void test_bounds(){
	g_misaligned=0;
	CHECK_EQ( bounded( 2,false ),2 );
	CHECK_EQ( bounded( 2,true ),4 );
	CHECK_EQ( bounded( 9,false ),-1 );
	CHECK_EQ( bounded( 9,true ),-1 );
	CHECK_EQ( g_misaligned,0 );
}

////////////
// Errors //
////////////

static void test_errors(){
	//class string length must match the arg count
	TNode *a[]={ K( 1 ) };
	Prog p;
	p.begin( "_f" );
	p.code( ret( callN( global( "c_add1" ),1,a,"Cii" ) ) );
	p.end();
	CHECK( !p.link( stubs() ) );
}

int main(){
	test_c_calls();
	test_alignment();
	test_blitz_calls();
	test_gosub();
	test_bounds();
	test_errors();
	printf("test_calls_x64: %d checks, %d failed\n",tests_run,tests_failed);
	return tests_failed?1:0;
}
