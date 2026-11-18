
//bbdump: parse + semant + translate a .bb file through the real compiler
//front end into DumpCodegen, printing the TNode trees (x64-plan.md commit 3a).
//
//The runtime function declarations mirror what blitz/libs.cpp feeds the
//compiler from runtime.dll's symbol table; the list below is the single place
//they are declared for this tool.

#include <cstdio>
#include "../../compiler/std.h"	//must come first: front-end headers use bare string/istream

#include "../../compiler/environ.h"
#include "../../compiler/parser.h"
#include "../../compiler/prognode.h"
#include "dump_codegen.h"

//runtime symbols: name, return type ('%' int / '#' float / '$' string / 0 void),
//param types (same encoding, 0-terminated). 0 = end of table.
struct RtSym{
	const char *name;
	char ret;
	const char *params;
};

static const RtSym rtSyms[] = {
	//runtime functions the sample programs call
	{ "Print", 0, "%$#" },
	{ "Len",   '%', "$"  },
	{ 0, 0, 0 }
};

//__bb* internals the front end emits calls to (compiler/*.cpp).
static const char *rtInternals[] = {
	"__bbAbs","__bbArrayBoundsEx","__bbCStrToStr","__bbDebugEnter",
	"__bbDebugLeave","__bbDebugStmt","__bbDimArray","__bbFAbs",
	"__bbFltType","__bbFMod","__bbFPow","__bbFSgn","__bbIntType",
	"__bbLoadLibs","__bbMod","__bbNullObjEx","__bbObjCompare",
	"__bbObjDelete","__bbObjDeleteEach","__bbObjEachFirst","__bbObjEachNext",
	"__bbObjFirst","__bbObjFromHandle","__bbObjInsAfter","__bbObjInsBefore",
	"__bbObjLast","__bbObjNew","__bbObjNext","__bbObjPrev","__bbObjRelease",
	"__bbObjStore","__bbObjToHandle","__bbObjToStr","__bbReadFloat",
	"__bbReadInt","__bbReadStr","__bbRestore","__bbSgn","__bbStrCompare",
	"__bbStrConcat","__bbStrConst","__bbStrFromFloat","__bbStrFromInt",
	"__bbStrLoad","__bbStrRelease","__bbStrStore","__bbStrToCStr",
	"__bbStrToFloat","__bbStrToInt","__bbStrType","__bbUndimArray",
	"__bbVecAlloc","__bbVecBoundsEx","__bbVecFree",
	0
};

static Type *bb_typeof( char c ){
	switch( c ){
	case '%':return Type::int_type;
	case '#':return Type::float_type;
	case '$':return Type::string_type;
	}
	return Type::void_type;
}

static Environ *makeRuntimeEnviron(){
	Environ *e=d_new Environ( "",Type::int_type,0,0 );
	for( const RtSym *s=rtSyms;s->name;++s ){
		DeclSeq *params=d_new DeclSeq();
		for( const char *p=s->params;*p;++p )
			params->insertDecl( "a",bb_typeof( *p ),DECL_PARAM );
		e->funcDecls->insertDecl( tolower( s->name ),d_new FuncType( bb_typeof( s->ret ),params,false,false ),DECL_FUNC );
	}
	for( const char **n=rtInternals;*n;++n )
		e->funcDecls->insertDecl( *n,d_new FuncType( Type::void_type,d_new DeclSeq(),false,false ),DECL_FUNC );
	return e;
}

int main( int argc,char *argv[] ){
	if( argc!=2 ){
		fprintf( stderr,"usage: bbdump <file.bb>\n" );
		return 1;
	}
	ifstream in( argv[1] );
	if( !in ){
		fprintf( stderr,"unable to open %s\n",argv[1] );
		return 1;
	}

	Environ *runtime=makeRuntimeEnviron();
	ProgNode *prog=0;
	Environ *environ=0;

	try{
		Toker toker( in );
		Parser parser( toker );
		prog=parser.parse( argv[1] );
		environ=prog->semant( runtime );

		DumpCodegen codegen( cout,false );
		vector<UserFunc> userFuncs;
		prog->translate( &codegen,userFuncs );
		codegen.flush();
	}catch( Ex &x ){
		int row=((x.pos>>16)&65535)+1,col=(x.pos&65535)+1;
		fprintf( stderr,"%s:%d:%d: %s\n",x.file.c_str(),row,col,x.ex.c_str() );
		delete prog;
		delete environ;
		delete runtime;
		return 2;
	}

	delete prog;
	delete environ;
	delete runtime;
	return 0;
}
