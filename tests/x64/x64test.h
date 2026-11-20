//Shared scaffolding for the x64 backend tests (tests/x64/test_*_x64.cpp).
//Prog builds one or more functions with Codegen_x64, links the output into
//an X64Module (labels -> symbols, fixups -> relocs) and loads it.

#ifndef X64TEST_H
#define X64TEST_H

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "harness.h"
#include "../../compiler/codegen_x64/codegen_x64.h"

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

static string g_retLabel="_ret";	//return label of the function being built

static TNode *K( int n ){ return new TNode( IR_CONST,0,0,n ); }
//Float constant: CONST with a float bit pattern (FCONST, as x86-32 does).
static TNode *KF( float f ){ int n;memcpy( &n,&f,4 );return K( n ); }
static TNode *op( int o,TNode *l,TNode *r=0 ){ return new TNode( o,l,r ); }
static TNode *global( const char *s ){ return new TNode( IR_GLOBAL,0,0,string( s ) ); }
static TNode *mem( TNode *a,bool wide=false ){ TNode *t=new TNode( IR_MEM,a ); t->wide=wide;return t; }
static TNode *move( TNode *v,TNode *m ){ TNode *t=new TNode( IR_MOVE,v,m ); t->wide=m->wide;return t; }
static TNode *seq( TNode *a,TNode *b ){ return new TNode( IR_SEQ,a,b ); }
static TNode *jump( int o,TNode *l,const char *s,TNode *r=0 ){ return new TNode( o,l,r,string( s ) ); }
static TNode *ret( TNode *v ){ return new TNode( IR_RETURN,v,0,g_retLabel ); }
static TNode *fret( TNode *v ){ return new TNode( IR_FRETURN,v,0,g_retLabel ); }

//Locals and params of the function being built. Floats are plain (narrow)
//slots: a float is the low 32 bits of an 8-byte slot.
static TNode *local( int k ){ return new TNode( IR_LOCAL,0,0,-8*(k+1) ); }
static TNode *lv( int k,bool wide=false ){ return mem( local( k ),wide ); }
static TNode *pv( int k,bool wide=false ){ return mem( new TNode( IR_LOCAL,0,0,16+8*k ),wide ); }

//Argument list shaped like ExprSeqNode::translate: SEQ(MOVE(a0,MEM(ARG 0)),SEQ(...,0)).
//A trailing 'W' in wides marks pointer-width args.
static TNode *args( int n,TNode **a,const char *wides="" ){
	TNode *t=0,*l=0;
	for( int k=0;k<n;++k ){
		bool w=k<(int)strlen( wides ) && wides[k]=='W';
		TNode *p=move( a[k],mem( new TNode( IR_ARG,0,0,k*8 ),w ) );
		p=seq( p,0 );
		if( l ) l->r=p; else t=p;
		l=p;
	}
	return t;
}

//CALL node. cls: 0 = Blitz convention, else "C"+classes.
static TNode *callN( TNode *target,int n,TNode **a,const char *cls,bool wideRet=false,const char *wides="" ){
	TNode *t=new TNode( IR_CALL,target,args( n,a,wides ),n*8 );
	if( cls ) t->sconst=cls;
	t->wide=wideRet;
	return t;
}

//FCALL node (float result in xmm0).
static TNode *fcallN( TNode *target,int n,TNode **a,const char *cls,const char *wides="" ){
	TNode *t=new TNode( IR_FCALL,target,args( n,a,wides ),n*8 );
	if( cls ) t->sconst=cls;
	return t;
}

/////////////////////
// Build and load  //
/////////////////////

struct Prog{
	asmjit::CodeHolder holder;
	Codegen_x64 *cg;
	X64Module mod;
	X64Loader loader;

	Prog():cg(0){
		holder.init( asmjit::Environment::host() );
		cg=new Codegen_x64( &holder,false );
	}
	~Prog(){ loader.free();delete cg; }

	void begin( const string &name,int frame=64 ){
		g_retLabel=name+"_ret";
		cg->enter( name,frame );
	}
	void code( TNode *t ){ cg->code( t ); }
	void label( const string &l ){ cg->label( l ); }
	void end( TNode *cleanup=0 ){
		cg->label( g_retLabel );
		cg->leave( cleanup,0 );
	}

	//Copy the text section with each bound label as a module symbol, turn
	//fixups into relocs and load.
	bool link( const std::map<std::string,void*> &stubs ){
		if( !cg->ok() ) return false;
		asmjit::CodeBuffer &buf=holder.text_section()->buffer();
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

		//Data section (commit 8): its own pages so it stays writable.
		const std::vector<unsigned char> &d=cg->data();
		int dataBase=0;
		if( !d.empty() ){
			mod.beginData();
			dataBase=mod.getPC();
			mod.emitBytes( &d[0],(int)d.size() );
			const std::vector<std::pair<string,int> > &ds=cg->dataSymbols();
			for( size_t i=0;i<ds.size();++i ){
				mod.addDataSymbol( ds[i].first.c_str(),dataBase+ds[i].second );
			}
			const std::vector<Codegen_x64::Fixup> &df=cg->dataFixups();
			for( size_t i=0;i<df.size();++i ){
				mod.addReloc( df[i].sym.c_str(),dataBase+df[i].offset,ABS64 );
			}
		}

		const std::vector<Codegen_x64::Fixup> &fx=cg->fixups();
		for( size_t i=0;i<fx.size();++i ){
			RelocKind k=fx[i].kind==Codegen_x64::FIX_ABS64 ? ABS64 :
				(fx[i].kind==Codegen_x64::FIX_REL32 ? REL32 : ABS32);
			mod.addReloc( fx[i].sym.c_str(),fx[i].offset,k );
		}
		return loader.load( mod,stubs )!=0;
	}

	void *sym( const char *s ){ return loader.symbol( mod,s ); }
};

#endif
