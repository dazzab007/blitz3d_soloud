//Stub runtime for the x64 front-end integration tests (commit 9).
//
//The real runtime sources (basic.cpp, bbstring.cpp, ...) do not compile on
//macOS yet (rtSym takes function pointers as void*, and the helpers still
//assume 4-byte slots: _bbVecAlloc/_bbDimArray use size*4, _bbObjNew uses
//fieldCnt*4 while BBField is 8 bytes). These stubs follow basic.cpp's
//semantics with 8-byte slots, using the real packed BBObj/BBObjType/BBArray/
//BBData structs from basic.h. Strings are std::string* (the compiled code
//never looks inside a string).
//
//THE runtime symbol table lives here, once: rtTable() gives, per entry, the
//link name the compiler emits, the BASIC declaration the front end is given
//(name, return and param types) and the function address. makeRuntimeEnviron()
//and rtStubMap() are both built from it.

#ifndef X64_RTSTUBS_H
#define X64_RTSTUBS_H

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include "../../bbruntime/basic.h"

//Output of Print, runtime error state, and the longjmp target for runtime
//errors (a runtime error never returns to the compiled code).
static std::string g_out;
static std::string g_rtError;
static jmp_buf *g_rtJmp=0;
static int g_strings;	//live strings (leak check)
static int g_objects;	//live (undeleted) objects

//Debug-build calls (commit 10): what the compiled code passes to
//__bbDebugEnter/__bbDebugLeave/__bbDebugStmt.
struct DebugEnterRec{ void *frame;int env;std::string func; };
static std::vector<DebugEnterRec> g_dbgEnter;
static int g_dbgLeave;
static std::vector<int> g_dbgPos;			//statement positions, in order
static std::string g_dbgFile;				//file name seen by DebugStmt
static void (*g_dbgStmtHook)( int pos )=0;	//lets a test inspect state at a statement
static void (*g_dbgEnterHook)( void *frame )=0;
static int g_dbgMisaligned;					//debug calls entered with a misaligned stack
__attribute__((noinline)) static void dbgNoteAlignment(){
	//after `call` + the stub's own push rbp, a correctly aligned caller gives 16-aligned rbp
	if( ((uintptr_t)__builtin_frame_address( 0 )&15)!=0 ) ++g_dbgMisaligned;
}

static void rtError( const char *msg ){
	g_rtError=msg;
	if( g_rtJmp ) longjmp( *g_rtJmp,1 );
	fprintf( stderr,"runtime error with no handler: %s\n",msg );
	abort();
}

typedef std::string *S;

static std::string rtFtoa( float f ){
	char b[64];
	snprintf( b,sizeof(b),"%g",(double)f );
	return b;
}
static S newStr( const std::string &s ){ ++g_strings;return new std::string( s ); }

//---- strings ----
static S rt_StrLoad( S *var ){ return newStr( *var ? **var : std::string() ); }
static void rt_StrRelease( S s ){ if( s ){ --g_strings;delete s; } }
static void rt_StrStore( S *var,S s ){ rt_StrRelease( *var );*var=s; }
static int rt_StrCompare( S a,S b ){ int n=a->compare( *b );rt_StrRelease( a );rt_StrRelease( b );return n; }
static S rt_StrConcat( S a,S b ){ *a+=*b;rt_StrRelease( b );return a; }
static int rt_StrToInt( S s ){ int n=atoi( s->c_str() );rt_StrRelease( s );return n; }
static S rt_StrFromInt( int n ){ return newStr( std::to_string( n ) ); }
static float rt_StrToFloat( S s ){ float f=(float)atof( s->c_str() );rt_StrRelease( s );return f; }
static S rt_StrFromFloat( float f ){ return newStr( rtFtoa( f ) ); }
static S rt_StrConst( const char *s ){ return newStr( s ); }

//---- arrays and vectors (8-byte slots) ----
static void rt_ArrayBoundsEx(){ rtError( "Array index out of bounds" ); }
static void rt_VecBoundsEx(){ rtError( "Blitz array index out of bounds" ); }
static void rt_ObjRelease( BBObj *obj );

static void rt_DimArray( BBArray *a ){
	int k;
	for( k=0;k<a->dims;++k ) ++a->scales[k];
	for( k=1;k<a->dims;++k ) a->scales[k]*=a->scales[k-1];
	int size=a->scales[a->dims-1];
	a->data=calloc( size,sizeof(BBField) );
}
static void rt_UndimArray( BBArray *a ){
	if( !a->data ) return;
	int size=a->scales[a->dims-1];
	if( a->elementType==BBTYPE_STR ){
		S *p=(S*)a->data;
		for( int k=0;k<size;++k ) rt_StrRelease( p[k] );
	}else if( a->elementType==BBTYPE_OBJ ){
		BBField *p=(BBField*)a->data;
		for( int k=0;k<size;++k ) rt_ObjRelease( p[k].OBJ );
	}
	free( a->data );
	a->data=0;
}
static void *rt_VecAlloc( BBVecType *t ){ return calloc( t->size,sizeof(BBField) ); }
static void rt_VecFree( void *vec,BBVecType *t ){
	BBField *p=(BBField*)vec;
	if( t->elementType->type==BBTYPE_STR ){
		for( int k=0;k<t->size;++k ) rt_StrRelease( (S)p[k].VEC );
	}else if( t->elementType->type==BBTYPE_OBJ ){
		for( int k=0;k<t->size;++k ) rt_ObjRelease( p[k].OBJ );
	}
	free( vec );
}

//---- objects ----
static void unlinkObj( BBObj *o ){ o->next->prev=o->prev;o->prev->next=o->next; }
static void insertObj( BBObj *o,BBObj *next ){
	o->next=next;o->prev=next->prev;next->prev->next=o;next->prev=o;
}
static BBObj *rt_ObjNew( BBObjType *t ){
	if( t->free.next==&t->free ){
		int sz=sizeof(BBObj)+t->fieldCnt*sizeof(BBField);
		char *o=(char*)calloc( 1,sz );
		insertObj( (BBObj*)o,&t->free );
	}
	BBObj *o=t->free.next;
	unlinkObj( o );
	o->type=t;
	o->ref_cnt=1;
	o->fields=(BBField*)(o+1);
	for( int k=0;k<t->fieldCnt;++k ){
		if( t->fieldTypes[k]->type==BBTYPE_VEC ) o->fields[k].VEC=rt_VecAlloc( (BBVecType*)t->fieldTypes[k] );
		else memset( &o->fields[k],0,sizeof(BBField) );
	}
	insertObj( o,&t->used );
	++g_objects;
	return o;
}
static void rt_ObjRelease( BBObj *o ){
	if( !o || --o->ref_cnt ) return;
	unlinkObj( o );
	insertObj( o,&o->type->free );
}
static void rt_ObjDelete( BBObj *o ){
	if( !o || !o->fields ) return;
	BBObjType *t=o->type;
	for( int k=0;k<t->fieldCnt;++k ){
		switch( t->fieldTypes[k]->type ){
		case BBTYPE_STR:rt_StrRelease( o->fields[k].STR );break;
		case BBTYPE_OBJ:rt_ObjRelease( o->fields[k].OBJ );break;
		case BBTYPE_VEC:rt_VecFree( o->fields[k].VEC,(BBVecType*)t->fieldTypes[k] );break;
		}
	}
	o->fields=0;
	rt_ObjRelease( o );
	--g_objects;
}
static void rt_ObjDeleteEach( BBObjType *t ){
	BBObj *o=t->used.next;
	while( o->type ){
		BBObj *n=o->next;
		if( o->fields ) rt_ObjDelete( o );
		o=n;
	}
}
static void rt_ObjStore( BBObj **var,BBObj *o ){
	if( o ) ++o->ref_cnt;
	rt_ObjRelease( *var );
	*var=o;
}
static int rt_ObjCompare( BBObj *a,BBObj *b ){ return (a ? a->fields : 0)!=(b ? b->fields : 0); }
static BBObj *rt_ObjNext( BBObj *o ){
	do{ o=o->next;if( !o->type ) return 0; }while( !o->fields );
	return o;
}
static BBObj *rt_ObjPrev( BBObj *o ){
	do{ o=o->prev;if( !o->type ) return 0; }while( !o->fields );
	return o;
}
static BBObj *rt_ObjFirst( BBObjType *t ){ return rt_ObjNext( &t->used ); }
static BBObj *rt_ObjLast( BBObjType *t ){ return rt_ObjPrev( &t->used ); }
static void rt_ObjInsBefore( BBObj *a,BBObj *b ){ if( a==b ) return;unlinkObj( a );insertObj( a,b ); }
static void rt_ObjInsAfter( BBObj *a,BBObj *b ){ if( a==b ) return;unlinkObj( a );insertObj( a,b->next ); }
static int rt_ObjEachFirst( BBObj **var,BBObjType *t ){ rt_ObjStore( var,rt_ObjFirst( t ) );return *var!=0; }
static int rt_ObjEachNext( BBObj **var ){ rt_ObjStore( var,rt_ObjNext( *var ) );return *var!=0; }
static void rt_NullObjEx(){ rtError( "Object does not exist" ); }

static std::map<int,BBObj*> s_handleMap;
static std::map<BBObj*,int> s_objMap;
static int rt_ObjToHandle( BBObj *o ){
	if( !o || !o->fields ) return 0;
	std::map<BBObj*,int>::iterator it=s_objMap.find( o );
	if( it!=s_objMap.end() ) return it->second;
	int h=(int)s_handleMap.size()+1;
	s_objMap[o]=h;s_handleMap[h]=o;
	return h;
}
static BBObj *rt_ObjFromHandle( int h,BBObjType *t ){
	std::map<int,BBObj*>::iterator it=s_handleMap.find( h );
	if( it==s_handleMap.end() ) return 0;
	return it->second->type==t ? it->second : 0;
}
static S rt_ObjToStr( BBObj *o ){
	if( !o || !o->fields ) return newStr( "[NULL]" );
	std::string s="[";
	for( int k=0;k<o->type->fieldCnt;++k ){
		if( k ) s+=",";
		switch( o->type->fieldTypes[k]->type ){
		case BBTYPE_INT:s+=std::to_string( o->fields[k].INT );break;
		case BBTYPE_FLT:s+=rtFtoa( o->fields[k].FLT );break;
		case BBTYPE_STR:s+="\""+( o->fields[k].STR ? *(S)o->fields[k].STR : std::string() )+"\"";break;
		default:s+="???";
		}
	}
	return newStr( s+"]" );
}

//---- DATA ----
static BBData *s_dataPtr;
static void rt_Restore( BBData *d ){ s_dataPtr=d; }
static int rt_ReadInt(){
	switch( s_dataPtr->fieldType ){
	case BBTYPE_INT:return s_dataPtr++->field.INT;
	case BBTYPE_FLT:return (int)s_dataPtr++->field.FLT;
	case BBTYPE_CSTR:return atoi( s_dataPtr++->field.CSTR );
	}
	rtError( "Out of data" );return 0;
}
static float rt_ReadFloat(){
	switch( s_dataPtr->fieldType ){
	case BBTYPE_INT:return (float)s_dataPtr++->field.INT;
	case BBTYPE_FLT:return s_dataPtr++->field.FLT;
	case BBTYPE_CSTR:return (float)atof( s_dataPtr++->field.CSTR );
	}
	rtError( "Out of data" );return 0;
}
static S rt_ReadStr(){
	switch( s_dataPtr->fieldType ){
	case BBTYPE_INT:return newStr( std::to_string( s_dataPtr++->field.INT ) );
	case BBTYPE_FLT:return newStr( rtFtoa( s_dataPtr++->field.FLT ) );
	case BBTYPE_CSTR:return newStr( s_dataPtr++->field.CSTR );
	}
	rtError( "Out of data" );return 0;
}

//---- math ----
static int rt_Abs( int n ){ return n>=0 ? n : -n; }
static int rt_Sgn( int n ){ return n>0 ? 1 : (n<0 ? -1 : 0); }
static int rt_Mod( int x,int y ){ return x%y; }
static float rt_FAbs( float n ){ return n>=0 ? n : -n; }
static float rt_FSgn( float n ){ return n>0 ? 1 : (n<0 ? -1 : 0); }
static float rt_FMod( float x,float y ){ return (float)fmod( x,y ); }
static float rt_FPow( float x,float y ){ return (float)pow( x,y ); }

//---- misc internals ----
static void rt_LoadLibs( void * ){}
static void rt_DebugEnter( void *frame,int env,const char *func ){
	dbgNoteAlignment();
	DebugEnterRec r;r.frame=frame;r.env=env;r.func=func ? func : "(null)";
	g_dbgEnter.push_back( r );
	if( g_dbgEnterHook ) g_dbgEnterHook( frame );
}
static void rt_DebugLeave(){
	dbgNoteAlignment();++g_dbgLeave;
	//A real runtime function may use any caller-saved register. The compiled
	//function's return value (rax / xmm0) must survive this call, which runs
	//in the leave() cleanup: make the stub trash them.
	__asm__ volatile( "movl $0x7badf00d,%%eax\n\txorps %%xmm0,%%xmm0\n\txorps %%xmm1,%%xmm1"
		::: "rax","xmm0","xmm1","memory" );
}
static void rt_DebugStmt( int pos,const char *file ){
	dbgNoteAlignment();
	g_dbgPos.push_back( pos );
	g_dbgFile=file ? file : "(null)";
	if( g_dbgStmtHook ) g_dbgStmtHook( pos );
}

//---- BASIC commands used by the tests ----
static void cmd_Print( S s ){ g_out+=*s;g_out+="\n";rt_StrRelease( s ); }
static int cmd_Len( S s ){ int n=(int)s->size();rt_StrRelease( s );return n; }
static S cmd_Left( S s,int n ){
	if( n<0 ) n=0;
	S r=newStr( s->substr( 0,n ) );rt_StrRelease( s );return r;
}
static S cmd_Right( S s,int n ){
	if( n<0 ) n=0;
	if( n>(int)s->size() ) n=(int)s->size();
	S r=newStr( s->substr( s->size()-n ) );rt_StrRelease( s );return r;
}
static float cmd_Sin( float x ){ return (float)sin( x*3.14159265358979/180 ); }
static float cmd_Sqr( float x ){ return (float)sqrt( x ); }
//A call with 8 int args and a call with mixed int/float args, to exercise
//the System V overflow path and the XMM classes from real BASIC.
static int cmd_Sum8( int a,int b,int c,int d,int e,int f,int g,int h ){ return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h; }
static float cmd_Mix( int a,float b,int c,float d ){ return a+b*2+c*3+d*4; }

struct RtEntry{
	const char *link;	//symbol the compiler emits
	const char *decl;	//BASIC command name (0 for internals)
	char ret;			//'%' int, '#' float, '$' string, 0 void
	const char *params;	//'%' '#' '$'
	void *fn;
};

#define RTI(name,fn) { "__bb" name,0,0,0,(void*)&fn }
static const RtEntry *rtTable(){
	static const RtEntry t[]={
		//BASIC commands (declared to the front end; linked as "_f"+lowercase)
		{ "_fprint","Print",0,"$",(void*)&cmd_Print },
		{ "_flen","Len",'%',"$",(void*)&cmd_Len },
		{ "_fleft","Left",'$',"$%",(void*)&cmd_Left },
		{ "_fright","Right",'$',"$%",(void*)&cmd_Right },
		{ "_fsin","Sin",'#',"#",(void*)&cmd_Sin },
		{ "_fsqr","Sqr",'#',"#",(void*)&cmd_Sqr },
		{ "_fsum8","Sum8",'%',"%%%%%%%%",(void*)&cmd_Sum8 },
		{ "_fmixrt","MixRt",'#',"%#%#",(void*)&cmd_Mix },
		//internals the front end emits calls to
		RTI("StrLoad",rt_StrLoad),RTI("StrRelease",rt_StrRelease),RTI("StrStore",rt_StrStore),
		RTI("StrCompare",rt_StrCompare),RTI("StrConcat",rt_StrConcat),RTI("StrToInt",rt_StrToInt),
		RTI("StrFromInt",rt_StrFromInt),RTI("StrToFloat",rt_StrToFloat),RTI("StrFromFloat",rt_StrFromFloat),
		RTI("StrConst",rt_StrConst),
		RTI("DimArray",rt_DimArray),RTI("UndimArray",rt_UndimArray),RTI("ArrayBoundsEx",rt_ArrayBoundsEx),
		RTI("VecAlloc",rt_VecAlloc),RTI("VecFree",rt_VecFree),RTI("VecBoundsEx",rt_VecBoundsEx),
		RTI("ObjNew",rt_ObjNew),RTI("ObjDelete",rt_ObjDelete),RTI("ObjDeleteEach",rt_ObjDeleteEach),
		RTI("ObjRelease",rt_ObjRelease),RTI("ObjStore",rt_ObjStore),RTI("ObjCompare",rt_ObjCompare),
		RTI("ObjNext",rt_ObjNext),RTI("ObjPrev",rt_ObjPrev),RTI("ObjFirst",rt_ObjFirst),RTI("ObjLast",rt_ObjLast),
		RTI("ObjInsBefore",rt_ObjInsBefore),RTI("ObjInsAfter",rt_ObjInsAfter),
		RTI("ObjEachFirst",rt_ObjEachFirst),RTI("ObjEachNext",rt_ObjEachNext),
		RTI("ObjToStr",rt_ObjToStr),RTI("ObjToHandle",rt_ObjToHandle),RTI("ObjFromHandle",rt_ObjFromHandle),
		RTI("NullObjEx",rt_NullObjEx),
		RTI("Restore",rt_Restore),RTI("ReadInt",rt_ReadInt),RTI("ReadFloat",rt_ReadFloat),RTI("ReadStr",rt_ReadStr),
		RTI("Abs",rt_Abs),RTI("Sgn",rt_Sgn),RTI("Mod",rt_Mod),
		RTI("FAbs",rt_FAbs),RTI("FSgn",rt_FSgn),RTI("FMod",rt_FMod),RTI("FPow",rt_FPow),
		RTI("LoadLibs",rt_LoadLibs),
		RTI("DebugEnter",rt_DebugEnter),RTI("DebugLeave",rt_DebugLeave),RTI("DebugStmt",rt_DebugStmt),
		{ 0,0,0,0,0 }
	};
	return t;
}

//Type descriptors the compiled data refers to (__bbIntType etc.): only the
//BBType.type tag is ever read.
static BBType s_intT( BBTYPE_INT ),s_fltT( BBTYPE_FLT ),s_strT( BBTYPE_STR ),s_cstrT( BBTYPE_CSTR );

static std::map<std::string,void*> rtStubMap(){
	std::map<std::string,void*> m;
	for( const RtEntry *e=rtTable();e->link;++e ) m[e->link]=e->fn;
	m["__bbIntType"]=&s_intT;
	m["__bbFltType"]=&s_fltT;
	m["__bbStrType"]=&s_strT;
	m["__bbCStrType"]=&s_cstrT;
	return m;
}

#endif
