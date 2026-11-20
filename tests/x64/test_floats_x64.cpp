//x64 float tile tests (commit 7, x64-plan.md section 5).
//SSE single precision: FADD..FDIV, FNEG, FSETxx, CAST/FCAST, FCALL/FRETURN.
//Each test builds TNode trees by hand, runs them through Codegen_x64, links
//the result and calls it.

#include "x64test.h"

using namespace asmjit;

/////////////////////////
// C functions to call //
/////////////////////////

static int g_misaligned;

__attribute__((noinline)) static int c_aligned(){
	return ((uintptr_t)__builtin_frame_address( 0 )&15)==0;
}
__attribute__((noinline)) static void note_alignment(){
	if( !c_aligned() ) ++g_misaligned;
}

static float c_fmul( float a,float b ){ note_alignment();return a*b; }
static int c_mix( int a,float b,int c,float d ){
	note_alignment();
	return a+(int)(b*10)+100*c+(int)(d*1000);
}
static float c_clobber(){ note_alignment();return 99.0f; }
static float c_note(){ note_alignment();return 1.0f; }

static std::map<std::string,void*> stubs(){
	std::map<std::string,void*> s;
	s["c_fmul"]=(void*)&c_fmul;
	s["c_mix"]=(void*)&c_mix;
	s["c_clobber"]=(void*)&c_clobber;
	s["c_note"]=(void*)&c_note;
	return s;
}

typedef float (*FnF)();
typedef int (*FnI)();

//_f = FRETURN e (extra functions added by `more`), run it.
static float runF( TNode *e,void (*more)( Prog & )=0,TNode *cleanup=0 ){
	Prog p;
	p.begin( "_f" );
	p.code( fret( e ) );
	p.end( cleanup );
	if( more ) more( p );
	bool ok=p.link( stubs() );
	CHECK( ok );
	if( !ok ) return -12345678.0f;
	return ((FnF)p.sym( "_f" ))();
}

//_f = RETURN e (int), run it.
static int runI( TNode *e ){
	Prog p;
	p.begin( "_f" );
	p.code( ret( e ) );
	p.end();
	bool ok=p.link( stubs() );
	CHECK( ok );
	if( !ok ) return -12345678;
	return ((FnI)p.sym( "_f" ))();
}

/////////////////
// Arithmetic  //
/////////////////

static void test_arith(){
	CHECK_EQ( runF( op( IR_FADD,KF( 1.5f ),KF( 2.25f ) ) ),3.75f );
	CHECK_EQ( runF( op( IR_FSUB,KF( 1.5f ),KF( 2.5f ) ) ),-1.0f );
	CHECK_EQ( runF( op( IR_FMUL,KF( 1.5f ),KF( 2.0f ) ) ),3.0f );
	CHECK_EQ( runF( op( IR_FDIV,KF( 7.0f ),KF( 2.0f ) ) ),3.5f );
	CHECK_EQ( runF( op( IR_FNEG,KF( 3.5f ) ) ),-3.5f );
	CHECK_EQ( runF( op( IR_FNEG,KF( -3.5f ) ) ),3.5f );
	CHECK_EQ( runF( op( IR_FNEG,op( IR_FNEG,KF( 0.25f ) ) ) ),0.25f );

	//locals as operands: a=9, b=4
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( KF( 9.0f ),lv( 0 ) ) );
		p.code( move( KF( 4.0f ),lv( 1 ) ) );
		p.code( fret( op( IR_FSUB,lv( 0 ),lv( 1 ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnF)p.sym( "_f" ))(),5.0f );
	}
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( KF( 9.0f ),lv( 0 ) ) );
		p.code( fret( op( IR_FDIV,lv( 0 ),KF( 4.0f ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnF)p.sym( "_f" ))(),2.25f );
	}
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( KF( 9.0f ),lv( 0 ) ) );
		p.code( fret( op( IR_FNEG,lv( 0 ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnF)p.sym( "_f" ))(),-9.0f );
	}
	//nested: (a+b)*(a-b)/2 with a=9, b=4 -> 13*5/2 = 32.5
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( KF( 9.0f ),lv( 0 ) ) );
		p.code( move( KF( 4.0f ),lv( 1 ) ) );
		p.code( fret( op( IR_FDIV,
			op( IR_FMUL,op( IR_FADD,lv( 0 ),lv( 1 ) ),op( IR_FSUB,lv( 0 ),lv( 1 ) ) ),
			KF( 2.0f ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnF)p.sym( "_f" ))(),32.5f );
	}
	//float stored to a local and read back (store goes through the int path)
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( op( IR_FMUL,KF( 1.5f ),KF( 3.0f ) ),lv( 2 ) ) );
		p.code( fret( lv( 2 ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnF)p.sym( "_f" ))(),4.5f );
	}
}

////////////
// Casts  //
////////////

static void test_casts(){
	CHECK_EQ( runF( op( IR_FCAST,K( 7 ) ) ),7.0f );
	CHECK_EQ( runF( op( IR_FCAST,K( -7 ) ) ),-7.0f );
	CHECK_EQ( runF( op( IR_FCAST,K( 0 ) ) ),0.0f );
	//float->int rounds to nearest even
	CHECK_EQ( runI( op( IR_CAST,KF( 2.5f ) ) ),2 );
	CHECK_EQ( runI( op( IR_CAST,KF( 3.5f ) ) ),4 );
	CHECK_EQ( runI( op( IR_CAST,KF( -2.5f ) ) ),-2 );
	CHECK_EQ( runI( op( IR_CAST,KF( 1.49f ) ) ),1 );
	CHECK_EQ( runI( op( IR_CAST,KF( -7.9f ) ) ),-8 );
}

/////////////////
// Comparisons //
/////////////////

static void test_relops(){
	struct{ int op;float a,b;int want; } c[]={
		{IR_FSETEQ,3.0f,3.0f,1},{IR_FSETEQ,3.0f,4.0f,0},{IR_FSETEQ,-2.0f,-2.0f,1},
		{IR_FSETNE,3.0f,3.0f,0},{IR_FSETNE,3.0f,4.0f,1},{IR_FSETNE,-2.0f,2.0f,1},
		{IR_FSETLT,-1.0f,1.0f,1},{IR_FSETLT,1.0f,-1.0f,0},{IR_FSETLT,2.0f,2.0f,0},{IR_FSETLT,-3.0f,-2.0f,1},
		{IR_FSETGT,1.0f,-1.0f,1},{IR_FSETGT,-1.0f,1.0f,0},{IR_FSETGT,2.0f,2.0f,0},{IR_FSETGT,-2.0f,-3.0f,1},
		{IR_FSETLE,2.0f,2.0f,1},{IR_FSETLE,3.0f,2.0f,0},{IR_FSETLE,-3.0f,-2.0f,1},{IR_FSETLE,-2.0f,-3.0f,0},
		{IR_FSETGE,2.0f,2.0f,1},{IR_FSETGE,-3.0f,2.0f,0},{IR_FSETGE,-2.0f,-3.0f,1},{IR_FSETGE,1.0f,2.0f,0},
	};
	for( size_t i=0;i<sizeof(c)/sizeof(c[0]);++i ){
		CHECK_EQ( runI( op( c[i].op,KF( c[i].a ),KF( c[i].b ) ) ),c[i].want );
	}
	//with local operands
	{
		Prog p;
		p.begin( "_f" );
		p.code( move( KF( -1.5f ),lv( 0 ) ) );
		p.code( move( KF( 2.5f ),lv( 1 ) ) );
		p.code( ret( op( IR_FSETLT,lv( 0 ),lv( 1 ) ) ) );
		p.end();
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnI)p.sym( "_f" ))(),1 );
	}
	//as JUMPT / JUMPF conditions: x=2.5; if x<3 -> 1 else 0
	for( int jt=0;jt<2;++jt ){
		for( int v=0;v<2;++v ){
			float x=v ? 3.5f : 2.5f;
			Prog p;
			p.begin( "_f" );
			p.code( move( KF( x ),lv( 0 ) ) );
			p.code( jump( jt ? IR_JUMPT : IR_JUMPF,op( IR_FSETLT,lv( 0 ),KF( 3.0f ) ),"_L" ) );
			p.code( ret( K( 10 ) ) );
			p.label( "_L" );
			p.code( ret( K( 20 ) ) );
			p.end();
			CHECK( p.link( stubs() ) );
			bool lt=x<3.0f;
			//JUMPT jumps when lt, JUMPF jumps when !lt
			int want=(jt ? lt : !lt) ? 20 : 10;
			CHECK_EQ( ((FnI)p.sym( "_f" ))(),want );
		}
	}
}

///////////
// Calls //
///////////

//_half(v#) = v / 2
static void add_half( Prog &p ){
	p.begin( "_half" );
	p.code( fret( op( IR_FDIV,pv( 0 ),KF( 2.0f ) ) ) );
	p.end();
}

static void test_calls(){
	g_misaligned=0;
	{
		TNode *a[]={ KF( 4.0f ),KF( 2.5f ) };
		CHECK_EQ( runF( fcallN( global( "c_fmul" ),2,a,"Cff" ) ),10.0f );
	}
	{
		//a + (int)(b*10) + 100*c + (int)(d*1000) = 1+25+300+500
		TNode *a[]={ K( 1 ),KF( 2.5f ),K( 3 ),KF( 0.5f ) };
		CHECK_EQ( runI( callN( global( "c_mix" ),4,a,"Cifif" ) ),1+25+300+500 );
	}
	{
		TNode *a[]={ KF( 9.0f ) };
		CHECK_EQ( runF( fcallN( global( "_half" ),1,a,"" ),add_half ),4.5f );
	}
	{
		//Blitz float result used in an expression
		TNode *a[]={ KF( 9.0f ) };
		CHECK_EQ( runF( op( IR_FADD,fcallN( global( "_half" ),1,a,"" ),KF( 1.0f ) ),add_half ),5.5f );
	}
	CHECK_EQ( g_misaligned,0 );
}

//leave(): cleanup calls C (clobbers rax and xmm0) while a float is returned.
static void test_leave(){
	g_misaligned=0;
	CHECK_EQ( runF( KF( 5.5f ),0,fcallN( global( "c_clobber" ),0,0,"C" ) ),5.5f );
	//int result survives too
	{
		Prog p;
		p.begin( "_f" );
		p.code( ret( K( 1234 ) ) );
		p.end( fcallN( global( "c_clobber" ),0,0,"C" ) );
		CHECK( p.link( stubs() ) );
		CHECK_EQ( ((FnI)p.sym( "_f" ))(),1234 );
	}
	CHECK_EQ( g_misaligned,0 );
}

//A C call in the left operand with the right operand already pushed.
static void test_alignment(){
	g_misaligned=0;
	CHECK_EQ( runF( op( IR_FADD,fcallN( global( "c_note" ),0,0,"C" ),KF( 2.0f ) ) ),3.0f );
	CHECK_EQ( runF( op( IR_FADD,
		op( IR_FSUB,fcallN( global( "c_note" ),0,0,"C" ),KF( 0.5f ) ),
		op( IR_FMUL,KF( 2.0f ),fcallN( global( "c_note" ),0,0,"C" ) ) ) ),2.5f );
	CHECK_EQ( g_misaligned,0 );
}

////////////
// Errors //
////////////

static void test_errors(){
	//an integer op in float context is unsupported
	Prog p;
	p.begin( "_f" );
	p.code( fret( op( IR_ADD,K( 1 ),K( 2 ) ) ) );
	p.end();
	CHECK( !p.link( stubs() ) );
}

int main(){
	test_arith();
	test_casts();
	test_relops();
	test_calls();
	test_leave();
	test_alignment();
	test_errors();
	printf( "test_floats_x64: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed?1:0;
}
