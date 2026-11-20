//x64 front-end integration (commit 9, x64-plan.md section 5).
//Real BASIC source goes through the real parser, semantic pass and translate()
//into Codegen_x64, is linked against the stub runtime in rtstubs.h (one symbol
//table) and run in-process. Print appends to g_out; tests compare the text.

#include <sstream>
#include <fstream>
#include <vector>
#include "../../compiler/std.h"	//must come first: front-end headers use bare string/istream
#include "../../compiler/environ.h"
#include "../../compiler/parser.h"
#include "../../compiler/prognode.h"
#include "rtstubs.h"
#include "x64test.h"

static Type *bb_typeof( char c ){
	switch( c ){
	case '%':return Type::int_type;
	case '#':return Type::float_type;
	case '$':return Type::string_type;
	}
	return Type::void_type;
}

//Runtime declarations come from rtTable(): BASIC commands with their types,
//internals as void functions (what the compiler emits calls to).
static Environ *makeRuntimeEnviron(){
	Environ *e=d_new Environ( "",Type::int_type,0,0 );
	for( const RtEntry *s=rtTable();s->link;++s ){
		DeclSeq *params=d_new DeclSeq();
		string name;
		if( s->decl ){
			int k=0;
			for( const char *p=s->params;*p;++p,++k )
				params->insertDecl( "a"+itoa( k ),bb_typeof( *p ),DECL_PARAM );
			name=tolower( s->decl );
		}else{
			name=s->link;
		}
		e->funcDecls->insertDecl( name,d_new FuncType( bb_typeof( s->ret ),params,false,false ),DECL_FUNC );
	}
	return e;
}

static void resetRuntime(){
	g_out.clear();g_rtError.clear();g_strings=0;g_objects=0;
	s_handleMap.clear();s_objMap.clear();s_dataPtr=0;
}

//Compile src and run __MAIN. Returns false on a compile/link failure
//(message in g_compileError). Runtime errors end the run with g_rtError set.
static string g_compileError;
static const char *g_current="";
static bool runSource( const char *src,bool debug=false ){
	resetRuntime();
	g_compileError.clear();
	std::istringstream in( src );
	Environ *runtime=makeRuntimeEnviron();
	ProgNode *prog=0;
	Environ *environ=0;
	Prog p;
	delete p.cg;
	p.cg=new Codegen_x64( &p.holder,debug );
	bool ok=false;
	try{
		Toker toker( in );
		Parser parser( toker );
		prog=parser.parse( "test.bb" );
		environ=prog->semant( runtime );
		vector<UserFunc> userFuncs;
		prog->translate( p.cg,userFuncs );
		p.cg->flush();
		ok=true;
	}catch( Ex &x ){
		int row=((x.pos>>16)&65535)+1,col=(x.pos&65535)+1;
		char b[256];
		snprintf( b,sizeof(b),"%d:%d: %s",row,col,x.ex.c_str() );
		g_compileError=b;
	}
	delete prog;delete environ;delete runtime;
	if( !ok ) return false;
	if( !p.link( rtStubMap() ) ){ g_compileError="link failed";return false; }
	void (*main_)()=(void(*)())p.sym( "__MAIN" );
	if( !main_ ){ g_compileError="no __MAIN";return false; }
	jmp_buf jb;
	g_rtJmp=&jb;
	if( !setjmp( jb ) ) main_();
	g_rtJmp=0;
	return true;
}

//Run src, expect exactly `want` on Print.
static void expect( const char *name,const char *src,const char *want ){
	g_current=name;if( getenv( "X64_VERBOSE" ) ) printf( "[%s]\n",name );
	bool ok=runSource( src );
	++tests_run;
	if( !ok ){
		++tests_failed;
		printf( "FAIL %s: %s\n",name,g_compileError.c_str() );
		return;
	}
	if( g_out!=want || g_rtError.size() ){
		++tests_failed;
		printf( "FAIL %s:\n--- got ---\n%s%s\n--- want ---\n%s\n",name,g_out.c_str(),
			g_rtError.size() ? ("[runtime error: "+g_rtError+"]").c_str() : "",want );
	}
}

//Run src, expect the run to stop with a runtime error containing `err`.
//Bounds and Null checks are only emitted in debug builds, so these run with debug=true.
static void expectError( const char *name,const char *src,const char *out,const char *err,bool debug=true ){
	g_current=name;if( getenv( "X64_VERBOSE" ) ) printf( "[%s]\n",name );
	bool ok=runSource( src,debug );
	++tests_run;
	if( !ok || g_rtError.find( err )==string::npos || g_out!=out ){
		++tests_failed;
		printf( "FAIL %s: %s out='%s' rtError='%s'\n",name,ok ? "" : g_compileError.c_str(),
			g_out.c_str(),g_rtError.c_str() );
	}
}

////////////////
// The tests  //
////////////////

static void test_arith(){
	expect( "arith",
		"a=10\n"
		"Print 1+2*3\n"
		"Print a/3\n"
		"Print a Mod 3\n"
		"Print a Shl 2\n"
		"Print -a\n"
		"Print a*a-1\n"
		"Print (a+5) And 6\n"
		"Print a Or 5\n"
		"Print a Xor 3\n"
		"Print 100 Shr 2\n"
		"Print -100 Sar 2\n",
		"7\n3\n1\n40\n-10\n99\n6\n15\n9\n25\n-25\n" );
	expect( "compare",
		"a=3 : b=4\n"
		"Print a<b\nPrint a>b\nPrint a=3\nPrint a<>b\nPrint a<=3\nPrint b>=5\n",
		"1\n0\n1\n1\n1\n0\n" );
}

static void test_control(){
	expect( "if",
		"For i=1 To 4\n"
		" If i=1 Then Print \"one\" ElseIf i=2 Then Print \"two\" Else If i=3 Then Print \"three\" Else Print \"other\"\n"
		"Next\n",
		"one\ntwo\nthree\nother\n" );
	expect( "for_step",
		"For i=10 To 1 Step -3 : Print i : Next\n"
		"For i=1 To 2 : For j=1 To 2 : Print i*10+j : Next : Next\n",
		"10\n7\n4\n1\n11\n12\n21\n22\n" );
	expect( "while_repeat",
		"i=0\nWhile i<3 : Print i : i=i+1 : Wend\n"
		"Repeat : i=i-1 : Print i : Until i=0\n",
		"0\n1\n2\n2\n1\n0\n" );
	expect( "exit_select",
		"For i=1 To 100\n If i=3 Then Exit\n Print i\nNext\n"
		"For i=1 To 4\n Select i\n Case 1 : Print \"a\"\n Case 2,3 : Print \"bc\"\n Default : Print \"d\"\n End Select\nNext\n",
		"1\n2\na\nbc\nbc\nd\n" );
}

static void test_functions(){
	expect( "fact",
		"Function fact(n)\n If n<=1 Then Return 1\n Return n*fact(n-1)\nEnd Function\n"
		"Print fact(10)\nPrint fact(1)\n",
		"3628800\n1\n" );
	expect( "args",
		"Function f(a,b,c,d,e,f1,g,h,i,j)\n Return a+2*b+3*c+4*d+5*e+6*f1+7*g+8*h+9*i+10*j\nEnd Function\n"
		"Print f(1,2,3,4,5,6,7,8,9,10)\n",
		"385\n" );
	expect( "float_func",
		"Function avg#(a#,b#)\n Return (a+b)/2\nEnd Function\n"
		"x#=avg(3,4)\nPrint x\nPrint avg(1.5,2.5)\n",
		"3.5\n2\n" );
	expect( "string_func",
		"Function twice$(s$)\n Return s+s\nEnd Function\n"
		"Print twice(\"ab\")\nPrint twice(twice(\"x\"))\nPrint Len(twice(\"abc\"))\n",
		"abab\nxxxx\n6\n" );
	expect( "globals_locals",
		"Global g=5\n"
		"Function f()\n Local g=1\n Return g\nEnd Function\n"
		"Function h()\n g=g+1\n Return g\nEnd Function\n"
		"Print f()\nPrint h()\nPrint h()\nPrint g\n",
		"1\n6\n7\n7\n" );
	expect( "runtime_args",
		"Print Sum8(1,2,3,4,5,6,7,8)\n"
		"Print MixRt(1,2.5,3,0.5)\n",
		"204\n17\n" );
}

static void test_gosub(){
	expect( "gosub",
		"Gosub sub1\nPrint \"back\"\nGosub sub1\nGoto done\n"
		".sub1\nPrint \"in sub\"\nReturn\n.done\n",
		"in sub\nback\nin sub\n" );
	expect( "gosub_nested",
		"n=0\nGosub outer\nGosub outer\nPrint n\nGoto done\n"
		".outer\nn=n+1\nGosub inner\nReturn\n"
		".inner\nn=n*10\nReturn\n.done\n",
		"110\n" );
}

static void test_floats(){
	expect( "float_arith",
		"x#=1.5 : y#=2.25\n"
		"Print x+y\nPrint x*y\nPrint y-x\nPrint y/x\nPrint -x\nPrint x*2\n",
		"3.75\n3.375\n0.75\n1.5\n-1.5\n3\n" );
	expect( "float_compare",
		"x#=1.5 : y#=2.5\nPrint x<y\nPrint x>y\nPrint x=1.5\nPrint x<>y\nPrint y<=2.5\nPrint x>=y\n"
		"If x<y Then Print \"lt\"\nIf x>y Then Print \"gt\" Else Print \"le\"\n",
		"1\n0\n1\n1\n1\n0\nlt\nle\n" );
	expect( "float_int_cast",
		"x#=7\nPrint x\ni=2.5\nPrint i\ni=3.5\nPrint i\ni=-2.5\nPrint i\nPrint Int(1.49)\nPrint Float(3)/2\n",
		"7\n2\n4\n-2\n1\n1.5\n" );
	expect( "float_math",
		"Print Sqr(16)\nPrint Abs(-2.5)\nPrint Sgn(-3.5)\nx#=2 : Print x^10\nPrint 7.5 Mod 2\nPrint Sin(90)\n",
		"4\n2.5\n-1\n1024\n1.5\n1\n" );
}

static void test_strings(){
	expect( "concat",
		"a$=\"foo\" : b$=\"bar\"\nc$=a+b+\"!\"\nPrint c\nPrint Len(c)\nPrint Left(c,4)\n",
		"foobar!\n7\nfoob\n" );
	expect( "str_compare",
		"a$=\"abc\" : b$=\"abd\"\nPrint a<b\nPrint a=b\nPrint a<>b\nPrint a=\"abc\"\nPrint a>b\n",
		"1\n0\n1\n1\n0\n" );
	expect( "str_convert",
		"s$=12\nPrint s+\"x\"\ni=\"42\"\nPrint i+1\nf#=\"2.5\"\nPrint f*2\ns$=1.5\nPrint s\n",
		"12x\n43\n5\n1.5\n" );
	expect( "str_vars",
		"Global g$=\"glob\"\nFunction f$(x$)\n Local l$=x+\"-\"+g\n g=\"changed\"\n Return l\nEnd Function\n"
		"Print f(\"in\")\nPrint g\nPrint f(\"again\")\n",
		"in-glob\nchanged\nagain-changed\n" );
	expect( "str_loop",
		"s$=\"\"\nFor i=1 To 5 : s=s+i : Next\nPrint s\n",
		"12345\n" );
}

static void test_types(){
	const char *decl=
		"Type Thing\n Field id\n Field name$\n Field w#\nEnd Type\n";
	string pre=decl;
	expect( "type_new",
		(pre+
		"t.Thing=New Thing\nt\\id=7 : t\\name=\"seven\" : t\\w=0.5\n"
		"Print t\\id\nPrint t\\name\nPrint t\\w*4\n").c_str(),
		"7\nseven\n2\n" );
	expect( "type_list",
		(pre+
		"For i=1 To 3\n t.Thing=New Thing\n t\\id=i : t\\name=\"n\"+i\nNext\n"
		"t.Thing=First Thing : Print t\\id\nt=Last Thing : Print t\\id\n"
		"For t.Thing=Each Thing\n Print t\\id+\" \"+t\\name\nNext\n"
		"t=First Thing\nu.Thing=After t : Print u\\id\nt=Last Thing : u=Before t : Print u\\id\n").c_str(),
		"1\n3\n1 n1\n2 n2\n3 n3\n2\n2\n" );
	expect( "type_delete",
		(pre+
		"For i=1 To 4 : t.Thing=New Thing : t\\id=i : Next\n"
		"t=First Thing : Delete t\n"
		"For t.Thing=Each Thing : Print t\\id : Next\n"
		"Delete Each Thing\n"
		"Print (First Thing=Null)\n"
		"t=New Thing : t\\id=99\nt=First Thing : Print t\\id\n").c_str(),
		"2\n3\n4\n1\n99\n" );
	expect( "type_insert_handle",
		(pre+
		"a.Thing=New Thing : a\\id=1\nb.Thing=New Thing : b\\id=2\n"
		"Insert b Before a\nt.Thing=First Thing : Print t\\id\n"
		"h=Handle a\nc.Thing=Object.Thing(h)\nPrint c\\id\n"
		"Print (a=c)\nPrint (a=b)\n").c_str(),
		"2\n1\n1\n0\n" );
	expect( "type_func_return",
		(pre+
		"Function mk.Thing(n)\n t.Thing=New Thing : t\\id=n : Return t\nEnd Function\n"
		"x.Thing=mk(5)\nPrint x\\id\nx=mk(6)\nPrint x\\id\nPrint Len(x\\name)\n").c_str(),
		"5\n6\n0\n" );
	expect( "type_object_field",
		"Type Node\n Field nxt.Node\n Field v\nEnd Type\n"
		"a.Node=New Node : a\\v=1\nb.Node=New Node : b\\v=2\na\\nxt=b\n"
		"Print a\\nxt\\v\nPrint (b\\nxt=Null)\n",
		"2\n1\n" );
	//Null object access: only debug builds emit __bbNullObjEx checks; a release
	//build faults (signal mapping is Prompt 4).
	expectError( "type_null",
		(pre+"t.Thing=Null\nPrint \"before\"\nPrint t\\id\nPrint \"after\"\n").c_str(),
		"before\n","Object does not exist" );
}

static void test_arrays(){
	expect( "dim_int",
		"Dim a(10)\nFor i=0 To 10 : a(i)=i*i : Next\nPrint a(3)\nPrint a(10)\nPrint a(0)\n",
		"9\n100\n0\n" );
	expect( "dim_2d_float",
		"Dim m#(2,3)\nFor i=0 To 2 : For j=0 To 3 : m(i,j)=i*10+j+0.5 : Next : Next\n"
		"Print m(2,3)\nPrint m(1,0)\nPrint m(0,2)\n",
		"23.5\n10.5\n2.5\n" );
	expect( "dim_string",
		"Dim s$(3)\ns(1)=\"one\" : s(3)=\"three\"\nPrint s(1)+s(3)+s(2)\nPrint Len(s(2))\n",
		"onethree\n0\n" );
	expect( "dim_obj",
		"Type T\n Field v\nEnd Type\nDim o.T(2)\nFor i=0 To 2 : o(i)=New T : o(i)\\v=i+10 : Next\n"
		"Print o(0)\\v+o(1)\\v+o(2)\\v\n",
		"33\n" );
	expect( "dim_in_func",
		"Dim g(5)\nFunction fill(n)\n For i=0 To n : g(i)=i+1 : Next\nEnd Function\nfill(5)\n"
		"s=0 : For i=0 To 5 : s=s+g(i) : Next\nPrint s\n",
		"21\n" );
	expect( "local_array",
		"Function f()\n Local a[4]\n For i=0 To 4 : a[i]=i*2 : Next\n Return a[4]+a[3]\nEnd Function\nPrint f()\nPrint f()\n",
		"14\n14\n" );
	expect( "type_array_field",
		"Type V\n Field a[3]\n Field s$[2]\nEnd Type\n"
		"t.V=New V\nt\\a[2]=9 : t\\a[0]=1 : t\\s[1]=\"x\"\nPrint t\\a[2]+t\\a[0]\nPrint t\\s[1]\n",
		"10\nx\n" );
	expectError( "dim_bounds",
		"Dim a(3)\nPrint \"ok\"\na(4)=1\nPrint \"no\"\n","ok\n","out of bounds" );
	expectError( "dim_negative",
		"Dim a(3)\ni=-1\nPrint a(i)\n","","out of bounds" );
	expectError( "vec_bounds",
		"Local a[3]\ni=4\nPrint \"x\"\na[i]=1\n","x\n","out of bounds" );
}

static void test_data(){
	expect( "data",
		"Read a\nRead b#\nRead s$\nPrint a\nPrint b\nPrint s\n"
		"Read a : Print a\nRestore\nRead a : Print a\n"
		"Data 42,1.5,\"text\",-7\n",
		"42\n1.5\ntext\n-7\n42\n" );
	expectError( "data_out",
		"Read a\nPrint a\nRead a\nData 5\n","5\n","Out of data" );
}


//Larger programs: the kind of code real Blitz users write.
static void test_programs(){
	expect( "bubble_sort",
		"Const N=9\nDim a(N)\nFor i=0 To N : a(i)=(i*7+3) Mod 10 : Next\n"
		"For i=0 To N-1\n For j=0 To N-1-i\n  If a(j)>a(j+1) Then t=a(j) : a(j)=a(j+1) : a(j+1)=t\n Next\nNext\n"
		"s$=\"\" : For i=0 To N : s=s+a(i)+\" \" : Next : Print s\n",
		"0 1 2 3 4 5 6 7 8 9 \n" );
	expect( "sieve",
		"Dim p(50)\nFor i=2 To 50 : p(i)=1 : Next\n"
		"For i=2 To 7\n If p(i)\n  j=i*i\n  While j<=50 : p(j)=0 : j=j+i : Wend\n EndIf\nNext\n"
		"c=0 : For i=2 To 50 : c=c+p(i) : Next : Print c\n",
		"15\n" );
	expect( "fib_float",
		"Function fib#(n)\n If n<2 Then Return n\n Return fib(n-1)+fib(n-2)\nEnd Function\nPrint fib(20)\n",
		"6765\n" );
	expect( "linked_list",
		"Type Node\n Field nxt.Node\n Field v\nEnd Type\n"
		"head.Node=Null\nFor i=1 To 100\n n.Node=New Node : n\\v=i : n\\nxt=head : head=n\nNext\n"
		"s=0 : n=head : While n<>Null : s=s+n\\v : n=n\\nxt : Wend\nPrint s\n"
		"Delete Each Node\n",
		"5050\n" );
	expect( "string_reverse",
		"Function rev$(s$)\n If Len(s)<=1 Then Return s\n Return rev(Right(s,Len(s)-1))+Left(s,1)\nEnd Function\n"
		"Print rev(\"abcdef\")\n",
		"fedcba\n" );
	//the runtime counts live strings: locals, temporaries and params must all be
	//released, so a loop of calls leaves only the globals alive
	{
		bool ok=runSource(
			"Global keep$=\"k\"\n"
			"Function f$(a$,n)\n Local s$=a+n\n Local t$=s+s\n Return t+keep\nEnd Function\n"
			"For i=1 To 2000 : r$=f(\"x\",i) : Next\nPrint r\n" );
		++tests_run;
		//main-program locals (r) are released when __MAIN leaves; only the Global (keep) lives on
		if( !ok || g_out!="x2000x2000k\n" || g_strings!=1 ){
			++tests_failed;
			printf( "FAIL string_leak: %s out='%s' live strings=%d\n",g_compileError.c_str(),g_out.c_str(),g_strings );
		}
	}
	//objects: New/Delete in a loop keeps the live-object count at zero
	{
		bool ok=runSource(
			"Type T\n Field a$\n Field n\nEnd Type\n"
			"For i=1 To 500\n t.T=New T : t\\a=\"s\"+i : t\\n=i : Delete t\nNext\nPrint \"done\"\n" );
		++tests_run;
		if( !ok || g_out!="done\n" || g_objects!=0 || g_strings!=0 ){
			++tests_failed;
			printf( "FAIL object_leak: %s out='%s' objects=%d strings=%d\n",g_compileError.c_str(),g_out.c_str(),g_objects,g_strings );
		}
	}
}

int main( int argc,char **argv ){
	setvbuf( stdout,0,_IONBF,0 );
	//debugging aid: test_frontend_x64 file.bb runs one program and prints its output
	if( argc>1 ){
		std::ifstream f( argv[1] );
		std::stringstream ss;ss<<f.rdbuf();
		bool ok=runSource( ss.str().c_str() );
		printf( "%s%s%s\n",g_out.c_str(),ok ? "" : g_compileError.c_str(),g_rtError.c_str() );
		return ok ? 0 : 1;
	}
	test_arith();
	test_control();
	test_functions();
	test_gosub();
	test_floats();
	test_strings();
	test_types();
	test_arrays();
	test_data();
	test_programs();
	printf( "test_frontend_x64: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed?1:0;
}
