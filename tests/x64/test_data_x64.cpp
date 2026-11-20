//x64 data section tests (commit 8, x64-plan.md section 5 and 7).
//i_data / s_data / p_data / null_data / field_data / align_data, data labels
//as module symbols, and the runtime descriptor layouts (BBObjType, BBObj,
//BBVecType, BBArray, BBData). basic.h is #pragma pack(1): ints are 4 bytes,
//pointers 8, no padding anywhere.

#include <cstddef>
#include "x64test.h"
#include "../../bbruntime/basic.h"

using namespace asmjit;

//The layouts the code generator emits, checked against the real runtime
//structs so the compiler and runtime can't drift apart.
static_assert( sizeof(BBObj)==36,"BBObj size" );
static_assert( offsetof(BBObj,fields)==0,"BBObj.fields" );
static_assert( offsetof(BBObj,next)==8,"BBObj.next" );
static_assert( offsetof(BBObj,prev)==16,"BBObj.prev" );
static_assert( offsetof(BBObj,type)==24,"BBObj.type" );
static_assert( offsetof(BBObj,ref_cnt)==32,"BBObj.ref_cnt" );
static_assert( offsetof(BBObjType,type)==0,"BBObjType.type" );
static_assert( offsetof(BBObjType,used)==4,"BBObjType.used" );
static_assert( offsetof(BBObjType,free)==40,"BBObjType.free" );
static_assert( offsetof(BBObjType,fieldCnt)==76,"BBObjType.fieldCnt" );
static_assert( offsetof(BBObjType,fieldTypes)==80,"BBObjType.fieldTypes" );
static_assert( offsetof(BBVecType,size)==4,"BBVecType.size" );
static_assert( offsetof(BBVecType,elementType)==8,"BBVecType.elementType" );
static_assert( sizeof(BBData)==12,"BBData size" );
static_assert( offsetof(BBData,field)==4,"BBData.field" );
static_assert( offsetof(BBArray,elementType)==8,"BBArray.elementType" );
static_assert( offsetof(BBArray,dims)==12,"BBArray.dims" );
static_assert( offsetof(BBArray,scales)==16,"BBArray.scales" );

static char s_intType,s_fltType,s_strType;	//stand-ins for __bbIntType etc.
static int g_called;
static void c_note(){ ++g_called; }

static std::map<std::string,void*> stubs(){
	std::map<std::string,void*> s;
	s["__bbIntType"]=&s_intType;
	s["__bbFltType"]=&s_fltType;
	s["__bbStrType"]=&s_strType;
	s["c_note"]=(void*)&c_note;
	return s;
}

//Emit a type descriptor exactly the way StructDeclNode::translate does.
static void structDecl( Codegen_x64 &g,const char *name,const char **fieldTypes,int n ){
	g.align_data( 4 );
	g.i_data( 5,string( "_t" )+name );
	for( int k=0;k<2;++k ){
		string lab=string( "_l" )+name+char( '0'+k );
		g.null_data( lab );		//fields
		g.p_data( lab );		//next
		g.p_data( lab );		//prev
		g.null_data();			//type
		g.i_data( -1 );			//ref_cnt
	}
	g.i_data( n );
	for( int k=0;k<n;++k ) g.p_data( fieldTypes[k] );
}

//A function that does nothing, so the module has some text.
static void nop( Prog &p ){
	p.begin( "_f" );
	p.code( ret( K( 0 ) ) );
	p.end();
}

static void test_struct(){
	Prog p;
	nop( p );
	const char *ft[]={ "__bbIntType","__bbStrType","_tother" };
	structDecl( *p.cg,"thing",ft,3 );
	structDecl( *p.cg,"other",ft,0 );
	CHECK( p.link( stubs() ) );
	BBObjType *t=(BBObjType*)p.sym( "_tthing" );
	CHECK( t!=0 );
	if( !t ) return;
	CHECK_EQ( t->type,BBTYPE_OBJ );
	CHECK_EQ( t->fieldCnt,3 );
	CHECK( t->used.fields==0 );
	CHECK( t->used.next==&t->used );
	CHECK( t->used.prev==&t->used );
	CHECK( t->used.type==0 );
	CHECK_EQ( t->used.ref_cnt,-1 );
	CHECK( t->free.fields==0 );
	CHECK( t->free.next==&t->free );
	CHECK( t->free.prev==&t->free );
	CHECK( t->free.type==0 );
	CHECK_EQ( t->free.ref_cnt,-1 );
	CHECK( (void*)t->fieldTypes[0]==&s_intType );
	CHECK( (void*)t->fieldTypes[1]==&s_strType );
	//a pointer to another descriptor in the same module
	CHECK( (void*)t->fieldTypes[2]==p.sym( "_tother" ) );
	CHECK( ((uintptr_t)t&7)==0 );
	BBObjType *o=(BBObjType*)p.sym( "_tother" );
	CHECK( o && o->fieldCnt==0 && o->used.next==&o->used );
	//the descriptor is writable (the runtime links objects into the lists)
	t->used.ref_cnt=7;
	CHECK_EQ( t->used.ref_cnt,7 );
}

static void test_vector_and_array(){
	Prog p;
	nop( p );
	Codegen_x64 &g=*p.cg;
	//vector descriptor: {int type; int size; BBType *elementType}
	g.align_data( 4 );
	g.i_data( BBTYPE_VEC,"_vec0" );
	g.i_data( 12 );
	g.p_data( "__bbFltType" );
	g.null_data( "_vglobal" );
	//Dim: {void *data; int elementType,dims,scales[]}
	g.align_data( 4 );
	g.null_data( "_ahdr" );
	g.i_data( 3 );
	g.i_data( 2 );
	g.null_data();		//Dim a(x,y): the frontend emits one slot per dim (8 here)
	CHECK( p.link( stubs() ) );
	BBVecType *v=(BBVecType*)p.sym( "_vec0" );
	CHECK( v!=0 );
	if( v ){
		CHECK_EQ( v->type,BBTYPE_VEC );
		CHECK_EQ( v->size,12 );
		CHECK( (void*)v->elementType==&s_fltType );
	}
	long long *gv=(long long*)p.sym( "_vglobal" );
	CHECK( gv && *gv==0 && ((uintptr_t)gv&7)==0 );
	BBArray *a=(BBArray*)p.sym( "_ahdr" );
	CHECK( a!=0 );
	if( a ){
		CHECK( a->data==0 );
		CHECK_EQ( a->elementType,3 );
		CHECK_EQ( a->dims,2 );
	}
}

static void test_strings_and_ints(){
	Prog p;
	nop( p );
	Codegen_x64 &g=*p.cg;
	g.s_data( "hello","_s0" );
	g.s_data( "","_s1" );
	g.s_data( "wor\"ld","_s2" );
	g.align_data( 4 );
	g.i_data( 0x01020304,"_i0" );
	g.i_data( -5,"_i1" );
	CHECK( p.link( stubs() ) );
	const char *s0=(const char*)p.sym( "_s0" );
	CHECK( s0 && !strcmp( s0,"hello" ) );
	const char *s1=(const char*)p.sym( "_s1" );
	CHECK( s1 && s1[0]==0 );
	CHECK( s1==s0+6 );		//strings are packed, NUL terminated
	const char *s2=(const char*)p.sym( "_s2" );
	CHECK( s2 && !strcmp( s2,"wor\"ld" ) );
	int *i0=(int*)p.sym( "_i0" );
	int *i1=(int*)p.sym( "_i1" );
	CHECK( i0 && *i0==0x01020304 );
	CHECK( i1 && *i1==-5 );
	CHECK( (char*)i1==(char*)i0+4 );		//i_data is exactly 4 bytes
	CHECK( ((uintptr_t)i0&7)==0 );		//align_data(4) gives 8
}

//DATA chunk: {int fieldType; BBField field} entries, 12 bytes each, a 0 type ends.
static void test_data_chunk(){
	Prog p;
	nop( p );
	Codegen_x64 &g=*p.cg;
	g.s_data( "text","_dstr" );
	g.align_data( 4 );
	g.label( "__DATA" );
	g.i_data( BBTYPE_INT );g.field_data( 42 );
	float f=1.5f;int fb;memcpy( &fb,&f,4 );
	g.i_data( BBTYPE_FLT );g.field_data( fb );
	g.i_data( BBTYPE_CSTR );g.p_data( "_dstr" );
	g.i_data( -7 );g.field_data( -1 );
	g.i_data( 0 );
	CHECK( p.link( stubs() ) );
	BBData *d=(BBData*)p.sym( "__DATA" );
	CHECK( d!=0 );
	if( !d ) return;
	CHECK_EQ( d[0].fieldType,BBTYPE_INT );
	CHECK_EQ( d[0].field.INT,42 );
	CHECK_EQ( d[1].fieldType,BBTYPE_FLT );
	CHECK( d[1].field.FLT==1.5f );
	CHECK_EQ( d[2].fieldType,BBTYPE_CSTR );
	CHECK( d[2].field.CSTR && !strcmp( d[2].field.CSTR,"text" ) );
	CHECK( d[2].field.CSTR==(char*)p.sym( "_dstr" ) );
	//negative ints: low 4 bytes hold the value, the rest is zero
	CHECK_EQ( d[3].field.INT,-1 );
	CHECK_EQ( d[4].fieldType,0 );
}

//LIBS chunk: names then a pointer slot per function, plus the "_f" slot.
static void test_pointer_to_code(){
	Prog p;
	nop( p );
	Codegen_x64 &g=*p.cg;
	g.align_data( 4 );
	g.null_data( "_fslot" );
	g.s_data( "lib.dll","__LIBS" );
	g.s_data( "proc" );
	g.p_data( "_fslot" );
	g.s_data( "" );
	g.p_data( "_f" );	//pointer into the text section
	CHECK( p.link( stubs() ) );
	const char *libs=(const char*)p.sym( "__LIBS" );
	CHECK( libs && !strcmp( libs,"lib.dll" ) );
	const char *proc=libs+8;
	CHECK( !strcmp( proc,"proc" ) );
	void *ptr;memcpy( &ptr,proc+5,8 );
	CHECK( ptr==p.sym( "_fslot" ) );
	void *fp;memcpy( &fp,proc+5+8+1,8 );
	CHECK( fp==p.sym( "_f" ) );
}

//Code reads and writes data labels through IR_GLOBAL; text stays executable.
static void test_code_uses_data(){
	Prog p;
	p.begin( "_f" );
	//_vx = 40 ; _vy (8-byte slot) = _vx + 2 ; return _vy
	p.code( move( K( 40 ),mem( global( "_vx" ) ) ) );
	p.code( move( op( IR_ADD,mem( global( "_vx" ) ),K( 2 ) ),mem( global( "_vy" ) ) ) );
	p.code( ret( mem( global( "_vy" ) ) ) );
	p.end();
	p.cg->align_data( 4 );
	p.cg->null_data( "_vx" );
	p.cg->null_data( "_vy" );
	p.cg->i_data( 77,"_vconst" );
	//an initialised int read by code
	CHECK( p.link( stubs() ) );
	CHECK_EQ( ((int(*)())p.sym( "_f" ))(),42 );
	long long *vx=(long long*)p.sym( "_vx" );
	CHECK( vx && (int)*vx==40 );
	//wide store of a pointer into a data slot
	Prog q;
	q.begin( "_f" );
	q.code( move( global( "_target" ),mem( global( "_slot" ),true ) ) );
	q.code( ret( K( 0 ) ) );
	q.end();
	q.cg->align_data( 4 );
	q.cg->null_data( "_slot" );
	q.cg->i_data( 1,"_target" );
	CHECK( q.link( stubs() ) );
	((int(*)())q.sym( "_f" ))();
	void *got;memcpy( &got,q.sym( "_slot" ),8 );
	CHECK( got==q.sym( "_target" ) );
}

static void test_errors(){
	{
		Prog p;
		nop( p );
		p.cg->i_data( 1,"_dup" );
		p.cg->i_data( 2,"_dup" );
		CHECK( !p.link( stubs() ) );
	}
	{
		//pointer to an undefined symbol: the loader refuses
		Prog p;
		nop( p );
		p.cg->p_data( "_nowhere" );
		CHECK( !p.link( stubs() ) );
	}
}

int main(){
	test_struct();
	test_vector_and_array();
	test_strings_and_ints();
	test_data_chunk();
	test_pointer_to_code();
	test_code_uses_data();
	test_errors();
	printf( "test_data_x64: %d checks, %d failed\n",tests_run,tests_failed );
	return tests_failed?1:0;
}
