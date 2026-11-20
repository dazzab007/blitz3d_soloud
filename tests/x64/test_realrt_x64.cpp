//x64 compiled programs against the REAL runtime (Prompt 4, step 2).
//Same pipeline as test_frontend_x64 (real parser/semant/translate into
//Codegen_x64, harness loader), but the runtime is basic.cpp, bbmath.cpp and
//bbstring.cpp: their rtSym registrations are declared to the front end with
//rtlinkDeclare (the parsing blitz/libs.cpp does on Windows) and linked by
//address. Only Print and the __bbDebug* hooks are test code. Runtime errors
//arrive through RTEX -> bbRaise -> longjmp (bb_err_jmp).

#include <sstream>
#include <fstream>
#include "../../compiler/std.h"	//must come first: front-end headers use bare string/istream
#include "../../compiler/environ.h"
#include "../../compiler/parser.h"
#include "../../compiler/prognode.h"
#include "../../compiler/rtlink.h"
#include "../../compiler/rtlinkname.h"
#include "../../bbruntime/bbsys.h"
#include "x64test.h"

//what bbsys.h expects the host to provide (stubs_gxruntime.cpp has its own
//BBStr, which would clash with the real one, so it is not linked here)
bool debug=false;
gxRuntime *gx_runtime=0;
void gxRuntime::debugError( const char *t ){ (void)t; }
void gxRuntime::debugLog( const char *t ){ (void)t; }

void basic_link( void (*rtSym)( const char *sym,void *pc ) );
void math_link( void (*rtSym)( const char *sym,void *pc ) );
void string_link( void (*rtSym)( const char *sym,void *pc ) );
bool basic_create();
bool basic_destroy();

static std::string g_out;
static std::vector<std::pair<std::string,void*> > g_syms;
static void collect( const char *sym,void *pc ){ g_syms.push_back( std::make_pair( std::string( sym ),pc ) ); }

//test-only commands and debug hooks
static void t_Print( BBStr *s ){ g_out+=*s;g_out+="\n";delete s; }
static void t_LoadLibs( void * ){}
static int g_enter,g_leave,g_stmt;
static void t_DebugEnter( void *,int,const char * ){ ++g_enter; }
static void t_DebugLeave(){ ++g_leave; }
static void t_DebugStmt( int,const char * ){ ++g_stmt; }

//Declare the whole runtime; fills stubs with module symbol -> address.
static Environ *makeRuntimeEnviron( std::map<std::string,void*> &stubs ){
	Environ *e=d_new Environ( "",Type::int_type,0,0 );
	g_syms.clear();
	basic_link( collect );
	math_link( collect );
	string_link( collect );
	g_syms.push_back( std::make_pair( std::string( "$Print$text" ).substr( 1 ),(void*)&t_Print ) );	//void Print(text$)
	g_syms.push_back( std::make_pair( std::string( "_bbLoadLibs" ),(void*)&t_LoadLibs ) );
	g_syms.push_back( std::make_pair( std::string( "_bbDebugEnter" ),(void*)&t_DebugEnter ) );
	g_syms.push_back( std::make_pair( std::string( "_bbDebugLeave" ),(void*)&t_DebugLeave ) );
	g_syms.push_back( std::make_pair( std::string( "_bbDebugStmt" ),(void*)&t_DebugStmt ) );
	static bool nameChecked=false;
	for( size_t k=0;k<g_syms.size();++k ){
		string link=rtlinkDeclare( g_syms[k].first,e );
		//the launcher builds its table with rtlinkName: it must agree with rtlinkDeclare
		if( !nameChecked ) ++tests_run;
		if( !nameChecked && link!=rtlinkName( g_syms[k].first ) ){
			++tests_failed;
			printf( "FAIL rtlinkName(%s)=%s, rtlinkDeclare=%s\n",g_syms[k].first.c_str(),rtlinkName( g_syms[k].first ).c_str(),link.c_str() );
		}
		stubs[link]=g_syms[k].second;
	}
	nameChecked=true;
	return e;
}

static string g_compileError;
static string g_rtError;
static bool runSource( const char *src,bool dbg=false ){
	g_out.clear();g_rtError.clear();g_compileError.clear();
	g_enter=g_leave=g_stmt=0;
	basic_create();
	std::istringstream in( src );
	std::map<std::string,void*> stubs;
	Environ *runtime=makeRuntimeEnviron( stubs );
	ProgNode *prog=0;
	Environ *environ=0;
	Prog p;
	delete p.cg;
	p.cg=new Codegen_x64( &p.holder,dbg );
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
		char b[256];
		snprintf( b,sizeof(b),"%d:%d: %s",((x.pos>>16)&65535)+1,(x.pos&65535)+1,x.ex.c_str() );
		g_compileError=b;
	}
	delete prog;delete environ;delete runtime;
	if( !ok ) return false;
	if( !p.link( stubs ) ){ g_compileError="link failed";return false; }
	void (*main_)()=(void(*)())p.sym( "__MAIN" );
	if( !main_ ){ g_compileError="no __MAIN";return false; }
	jmp_buf jb;
	bb_err_jmp=&jb;
	bb_err_msg=0;
	if( !setjmp( jb ) ) main_();
	else g_rtError=bb_err_msg ? bb_err_msg : "(error)";
	bb_err_jmp=0;
	return true;
}

static void expectMode( const char *name,const char *src,const char *want,const char *err,bool dbg ){
	if( getenv( "X64_VERBOSE" ) ) printf( "[%s%s]\n",name,dbg ? " debug" : "" );
	bool ok=runSource( src,dbg );
	++tests_run;
	string tag=string( name )+(dbg ? " [debug]" : "");
	bool good=ok && g_out==want && (err ? g_rtError.find( err )!=string::npos : g_rtError.empty());
	if( good && dbg && !err ) good=g_enter>0 && g_enter==g_leave && g_stmt>0;
	if( !good ){
		++tests_failed;
		printf( "FAIL %s: %s\n--- got ---\n%s[rtError='%s' enter=%d leave=%d]\n--- want ---\n%s\n",tag.c_str(),
			g_compileError.c_str(),g_out.c_str(),g_rtError.c_str(),g_enter,g_leave,want );
	}
}
//release and debug builds must agree
static void expect( const char *name,const char *src,const char *want ){
	expectMode( name,src,want,0,false );
	expectMode( name,src,want,0,true );
}
//runtime error: debug build only (bounds and Null checks are debug-only)
static void expectError( const char *name,const char *src,const char *out,const char *err ){
	expectMode( name,src,out,err,true );
}

int main( int argc,char **argv ){
	setvbuf( stdout,0,_IONBF,0 );
	if( argc>1 ){
		std::ifstream f( argv[1] );
		std::stringstream ss;ss<<f.rdbuf();
		bool ok=runSource( ss.str().c_str(),argc>2 );
		printf( "%s%s%s\n",g_out.c_str(),ok ? "" : g_compileError.c_str(),g_rtError.c_str() );
		return ok ? 0 : 1;
	}

	expect( "arith","a=10\nPrint 1+2*3\nPrint a/3\nPrint a Mod 3\nPrint a Shl 2\nPrint -a\n","7\n3\n1\n40\n-10\n" );
	expect( "float_format","x#=1.5\nPrint x\nPrint x*2\nPrint x/4\nPrint 7.0\nPrint 1.0/3.0\n","1.5\n3.0\n0.375\n7.0\n0.333333\n" );
	expect( "string_lib",
		"s$=\"Hello, World\"\nPrint Len(s)\nPrint Left(s,5)\nPrint Right(s,5)\nPrint Mid(s,8,5)\n"
		"Print Upper(s)\nPrint Lower(s)\nPrint Instr(s,\"World\")\nPrint Chr(65)+Chr(66)\nPrint Asc(\"a\")\n"
		"Print Trim(\"  x  \")+\"|\"\nPrint Replace(s,\"l\",\"L\")\nPrint String(\"ab\",3)\n",
		"12\nHello\nWorld\nWorld\nHELLO, WORLD\nhello, world\n8\nAB\n97\nx|\nHeLLo, WorLd\nababab\n" );
	expect( "string_ops",
		"a$=\"abc\"\nb$=a+a\nPrint b\nPrint a<b\nPrint a=\"abc\"\ni=Int(\"12abc\")\nPrint i+1\nPrint Str(5)+\"!\"\n",
		"abcabc\n1\n1\n13\n5!\n" );
	expect( "math_lib",
		"Print Abs(-3)\nPrint Sgn(-5)\nPrint Sqr(16)\nPrint Floor(2.7)\nPrint Ceil(2.2)\nPrint Int(2.5)\nPrint Sin(90)\nPrint Cos(0)\nPrint 2^10\nPrint 10 Mod 4\n",
		"3\n-1\n4.0\n2.0\n3.0\n2\n1.0\n1.0\n1024.0\n2\n" );
	expect( "types",
		"Type T\n Field id\n Field name$\n Field w#\nEnd Type\n"
		"For i=1 To 5\n t.T=New T : t\\id=i : t\\name=\"n\"+i : t\\w=i*0.5\nNext\n"
		"For t.T=Each T\n Print t\\id+\" \"+t\\name+\" \"+t\\w\nNext\n"
		"t=First T : Delete t\nn=0\nFor t.T=Each T : n=n+1 : Next\nPrint n\nDelete Each T\nPrint (First T=Null)\n",
		"1 n1 0.5\n2 n2 1.0\n3 n3 1.5\n4 n4 2.0\n5 n5 2.5\n4\n1\n" );
	expect( "type_handles",
		"Type T\n Field id\nEnd Type\na.T=New T : a\\id=11\nh=Handle a\nb.T=Object.T(h)\nPrint b\\id\nPrint (a=b)\nPrint h>0\n",
		"11\n1\n1\n" );
	expect( "arrays",
		"Dim a(4)\nDim m#(2,2)\nDim s$(2)\nFor i=0 To 4 : a(i)=i*i : Next\n"
		"For i=0 To 2 : For j=0 To 2 : m(i,j)=i+j/2.0 : Next : Next\ns(1)=\"x\" : s(2)=\"y\"\n"
		"Print a(4)\nPrint m(2,2)\nPrint s(1)+s(2)+s(0)\nLocal v[3]\nv[2]=8\nPrint v[2]\n",
		"16\n3.0\nxy\n8\n" );
	expect( "type_with_vectors",
		"Type V\n Field a[3]\n Field s$[2]\nEnd Type\nt.V=New V\nt\\a[2]=9\nt\\s[1]=\"q\"\nPrint t\\a[2]\nPrint t\\s[1]\nDelete t\n",
		"9\nq\n" );
	expect( "data",
		"Read a\nRead b#\nRead s$\nPrint a\nPrint b\nPrint s\nRestore\nRead a\nPrint a\nData 42,1.5,\"text\"\n",
		"42\n1.5\ntext\n42\n" );
	expect( "functions_gosub",
		"Function fact(n)\n If n<=1 Then Return 1\n Return n*fact(n-1)\nEnd Function\n"
		"Function cat$(a$,b$)\n Return a+\"-\"+b\nEnd Function\n"
		"Print fact(10)\nPrint cat(\"x\",\"y\")\nGosub s\nGosub s\nGoto done\n.s\nPrint \"sub\"\nReturn\n.done\n",
		"3628800\nx-y\nsub\nsub\n" );
	//a loop that creates and drops lots of strings and objects stays fine
	expect( "churn",
		"Type N\n Field s$\nEnd Type\ntotal=0\nFor i=1 To 3000\n n.N=New N : n\\s=\"item\"+i\n total=total+Len(n\\s)\n Delete n\nNext\nPrint total\n",
		"22893\n" );
	expectError( "err_array","Dim a(3)\nPrint \"ok\"\na(4)=1\nPrint \"no\"\n","ok\n","out of bounds" );
	expectError( "err_null","Type T\n Field v\nEnd Type\nt.T=Null\nPrint \"x\"\nPrint t\\v\nPrint \"no\"\n","x\n","Object does not exist" );
	expectError( "err_data","Read a\nPrint a\nRead b\nData 5\n","5\n","Out of data" );

	printf( "test_realrt_x64: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed ? 1 : 0;
}
