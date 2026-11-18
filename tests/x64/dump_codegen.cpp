
#include "dump_codegen.h"
#include <vector>
#include <cstdio>

//opcode names, indexed by the IR_* enum in compiler/codegen.h
static const char *opName( int op ){
	static const char *names[] = {
		"JUMP","JUMPT","JUMPF","JUMPGE",
		"SEQ","MOVE","MEM","LOCAL","GLOBAL","ARG","CONST",
		"JSR","RET","AND","OR","XOR","SHL","SHR","SAR",
		"CALL","RETURN","CAST",
		"NEG","ADD","SUB","MUL","DIV",
		"SETEQ","SETNE","SETLT","SETGT","SETLE","SETGE",
		"FCALL","FRETURN","FCAST",
		"FNEG","FADD","FSUB","FMUL","FDIV",
		"FSETEQ","FSETNE","FSETLT","FSETGT","FSETLE","FSETGE",
	};
	if( op<0 || op>=int( sizeof(names)/sizeof(names[0]) ) ) return "?";
	return names[op];
}

DumpCodegen::DumpCodegen( ostream &out,bool debug ):Codegen( out,debug ){
}

void DumpCodegen::enter( const string &l,int frameSize ){
	out<<"func "<<l<<" frame="<<frameSize<<'\n';
}

//x86-32 values for now (commit 3a); the x64 backend will use its own layout.
int DumpCodegen::argOffset( int k ){
	return 20+k*4;
}

int DumpCodegen::localOffset( int k ){
	return -4*(k+1);
}

void DumpCodegen::code( TNode *stmt ){
	printNode( stmt,0 );
	delete stmt;
}

void DumpCodegen::printNode( TNode *t,int depth ){
	string pad( depth*2,' ' );
	out<<pad<<opName( t->op );
	if( t->iconst ) out<<" "<<t->iconst;
	if( t->sconst.size() ) out<<" \""<<t->sconst<<"\"";
	//commit 3: append " wide" here once TNode carries the flag
	out<<'\n';
	if( t->l ) printNode( t->l,depth+1 );
	if( t->r ) printNode( t->r,depth+1 );
}

void DumpCodegen::leave( TNode *cleanup,int pop_sz ){
	if( cleanup ){
		out<<"cleanup:\n";
		printNode( cleanup,1 );
		delete cleanup;
	}
	out<<"leave pop="<<pop_sz<<'\n';
}

void DumpCodegen::label( const string &l ){
	out<<"label "<<l<<'\n';
}

void DumpCodegen::i_data( int i,const string &l ){
	char buf[64];
	snprintf( buf,sizeof(buf),"%d",i );
	s_data( buf,l );
}

void DumpCodegen::s_data( const string &s,const string &l ){
	out<<"data \""<<s<<"\"";
	if( l.size() ) out<<" label="<<l;
	out<<'\n';
}

void DumpCodegen::p_data( const string &p,const string &l ){
	out<<"ptr "<<p;
	if( l.size() ) out<<" label="<<l;
	out<<'\n';
}

void DumpCodegen::align_data( int n ){
	out<<"align "<<n<<'\n';
}

void DumpCodegen::flush(){
	//no deferred data, nothing to do
}
